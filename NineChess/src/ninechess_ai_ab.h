/****************************************************************************
** NineChess - Alpha-Beta AI
** 基于 Alpha-Beta 剪枝、置换表和对称局面归一化的 AI 搜索器
****************************************************************************/

#pragma once

#include "ninechess.h"
#include "ninechess_symmetry.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

class NineChess_AI_AB
{
public:
    // ==================== 公共配置 ====================

    // 置换表哈希模式（对称规范化实验开关）。
    // 实测结论：对称规范化收益集中在开局（开局 canonical 比普通哈希快 2.3 倍），
    // 中局对称破缺后无复用收益，而每节点 16 视角 canonical 哈希会让 NPS 下降约 4 倍。
    // 因此默认只在开局节点规范化，中局回退普通哈希。
    enum class HashMode : uint32_t {
        Plain = 0,             // 普通哈希：无对称规范化，中局 NPS 最优
        OpeningCanonical = 1,  // 仅开局节点用 canonical，根节点按对称类分组（默认）
        FullCanonical = 2      // 全部节点用 canonical（旧行为，16 视角 × 每节点 2 次）
    };

    // 搜索配置。setOptions() 修改后，对后续每次搜索生效。
    struct SearchOptions {
        int64_t timeLimitMs = 0;    // 每次搜索的时间预算；0 = 不限时（纯深度）
        uint32_t threads = 1;       // 并行线程数（>=1；Lazy SMP）
        uint32_t randomness = 0;    // 0 = 纯最优；>0 启用根节点随机（分差阈值 + 加权）
        int32_t randomGap = 60;     // 随机候选集与最优的分差阈值（估值单位）
        HashMode hashMode = HashMode::OpeningCanonical;
        size_t ttMaxEntries = 256u * 1024u;
        uint64_t seed = 0;          // 随机种子；0 = 每次随机
        int32_t pointValueWeight = 16;  // 点位结构价值权重；0 = 关闭（A/B 用）
        bool aspirationWindows = true;  // 根窗口随上一迭代估值收窄，失败时渐进放宽重搜
        int32_t repetitionPenalty = 40; // 搜索内 2~8 层重复局面的惩罚分；0 = 关闭
        bool quiescenceSearch = true;   // 深度 0 时展开“形成三连/提子”的静默搜索
    };

    // 搜索统计（原子计数，多线程搜索结束时汇总）。
    struct SearchStats {
        std::atomic<uint64_t> nodes{0};
        std::atomic<uint64_t> ttProbes{0};
        std::atomic<uint64_t> ttHits{0};
        std::atomic<uint64_t> ttStores{0};
        std::atomic<uint64_t> betaCutoffs{0};
        std::atomic<uint64_t> repetitionHits{0};
    };

    // 构造一个空的 AI；真正搜索前需要先调用 setChess() 注入局面。
    NineChess_AI_AB();

    // 默认析构即可，AI 不拥有需要手工释放的外部资源。
    ~NineChess_AI_AB() = default;

    // 设置待搜索的根局面，并重置本轮搜索的内部状态。
    void setChess(const NineChess& chess);

    // 设置搜索配置。
    void setOptions(const SearchOptions& options) { m_options = options; }
    const SearchOptions& getOptions() const { return m_options; }

    // 请求搜索尽快中止；供外部线程或控制层发出停止信号。
    // 多线程模式下所有工作线程都会检查该标志。
    void quit() { m_requiredQuit.store(true); }

    // 以给定深度执行迭代加深 Alpha-Beta 搜索，返回最终估值。
    // 若配置了 timeLimitMs，会在预算内尽可能完成更深的迭代，
    // 中断时保留最后一次完整算完的迭代结果作为 bestMove()。
    // 若配置了 threads > 1，采用 Lazy SMP 并行搜索。
    int alphaBetaPruning(int depth);

    // 返回当前搜索得到的最佳着法文本。
    const char* bestMove();

    // 把最近一次搜索的统计写入 out（SearchStats 含原子成员，不可按值拷贝）。
    void snapshotStats(SearchStats& out) const;

    // 最近一次搜索的完成深度 / 估值 / 耗时。
    int getLastCompletedDepth() const { return m_lastCompletedDepth; }
    int getLastCompletedValue() const { return m_lastCompletedValue; }
    int64_t getLastSearchTimeMs() const { return m_lastSearchTimeMs; }

    // 清空当前规则置换表（自对弈 / A/B 实验隔离用）。
    void clearTranspositionTable();

    // 最近一次完成迭代的根走法分数文本（每行 "命令 = 分数"）。
    // 未启用随机时，非最优走法的分数可能是边界值而非精确值，仅供诊断；
    // 启用随机后，列出的是精确打分阶段的候选集及其精确分数。
    std::string rootScoresText() const;

private:
    // AI 内部统一使用的走法类别。
    enum MoveType : uint8_t {
        MOVE_NONE = 0,    // 空走法，占位用。
        MOVE_PLACE = 1,   // 落子，包含开局摆子和某些“单点命令”。
        MOVE_SHIFT = 2,   // 走子，从 from 移到 to。
        MOVE_CAPTURE = 3  // 提子，只使用 to 记录目标点位。
    };

    struct Move {
        // 当前走法的类别。
        MoveType type = MOVE_NONE;

        // MOVE_SHIFT 的起点；其它类型通常为 -1。
        int8_t from = -1;

        // 落点或提子目标点位。
        int8_t to = -1;

        // 用于排序的启发式分值，分值越高越优先展开。
        int16_t order = 0;
    };

    struct MoveList {
        // 单个节点允许生成的最大走法数；超过后直接截断。
        // 飞子规则下最大为 12 子 × 21 空位，256 留足余量。
        static constexpr size_t MAX_COUNT = 256;

        // 走法缓存数组。
        std::array<Move, MAX_COUNT> moves = {};

        // 当前实际存放的走法数量。
        size_t count = 0;
    };

    struct Snapshot {
        // 搜索中 applyMove() 会直接改写工作局面，
        // 因此回溯时只需恢复这份最小必要状态快照。

        // 完整局面数据。
        NineChess::ChessData data;

        // 走法执行前的胜者缓存。
        NineChess::Players winner = NOBODY;

        // 走法执行前的当前选中点位。
        int32_t selectedPos = -1;
    };

    struct TTEntry {
        // 该局面的缓存估值。
        int16_t value = 0;

        // 该估值对应的搜索深度；越大代表信息越可靠。
        int16_t depth = 0;

        // 估值类型：精确值、下界或上界。
        uint8_t flag = 0;

        // 该节点已知的最佳走法（用于下次搜索时优先展开）。
        uint8_t moveType = MOVE_NONE;
        int8_t moveFrom = -1;
        int8_t moveTo = -1;

        // 最近一次被当前真实局面搜索访问到的代号，
        // 用于置换表老化清理时判断“新旧程度”。
        uint32_t generation = 0;
    };

    // 置换表分片：哈希低位路由到分片，各分片独立加锁，
    // 避免整表互斥锁成为多线程搜索的争用瓶颈。
    struct TTStore {
        static constexpr size_t SHARD_COUNT = 256;

        struct Shard {
            std::mutex mutex;
            std::unordered_map<uint64_t, TTEntry> table;
        };

        std::array<Shard, SHARD_COUNT> shards;

        // 世代号与整表清理用的小锁（与分片锁不存在嵌套顺序）。
        std::mutex metaMutex;

        // 当前规则置换表所在的“世代号”。
        uint32_t generation = 0;
    };

    // 镜像、内外翻转和离散旋转组合后共有 16 种等价视角
    // （SymmetryVariant 与变换表见 ninechess_symmetry.h，AI 与开局库共用）。

    // 置换表条目标记：精确值。
    static constexpr uint8_t TT_EXACT = 0;

    // 置换表条目标记：下界。
    static constexpr uint8_t TT_LOWER = 1;

    // 置换表条目标记：上界。
    static constexpr uint8_t TT_UPPER = 2;

    // 必胜/必败分值的基准值。
    static constexpr int WIN_SCORE = 30000;

    // Alpha-Beta 使用的正负无穷边界。
    static constexpr int INF_SCORE = 32000;

    // 搜索最大深度上限，同时约束杀手走法表大小。
    static constexpr int kMaxPly = 128;

    // 重复检测只在浅层进行：深层节点数量巨大，逐个计算普通哈希
    // （编号规则下是 hard hash）会显著拖慢搜索，而重复主要影响浅层计划。
    static constexpr int kRepetitionMaxPly = 10;

    // 静默搜索的最大链深（提子链有界：盘面子数单调递减）。
    static constexpr int kMaxQuiescenceDepth = 12;

    // 单线程搜索上下文；Lazy SMP 下每个工作线程一份，
    // 避免各线程在类成员上互相踩踏。
    struct SearchContext {
        // 递归搜索过程中实际被 applyMove()/undoMove() 改写的工作局面。
        NineChess board;

        // 本线程统计（非原子；搜索结束时汇总到类级 m_stats）。
        SearchStats stats;

        // 杀手走法：每层最多 2 个，仅记录非提子走法。
        std::array<std::array<Move, 2>, kMaxPly> killers = {};

        // 历史启发表：[来源点+1][目标点]，来源点 -1（落子）映射到 0。
        std::array<std::array<int32_t, BOARD_SIZE>, BOARD_SIZE + 1> history = {};

        // 时间检查节流计数器（每 1024 个节点检查一次）。
        uint32_t nodeCounterForTime = 0;

        // 当前这一层迭代是否被中途打断。
        bool iterationAborted = false;

        // 最近一次完整算完的迭代深度。
        int lastCompletedDepth = 0;

        // 最近一次完整算完的迭代估值。
        int lastCompletedValue = 0;

        // 迭代结束后对外公布的最佳走法。
        Move bestMove = {};

        // 当前迭代临时得到的最佳走法。
        Move iterationBestMove = {};

        // 最佳走法对应的命令行文本。
        std::string bestMoveText;

        // 最近一次完整迭代的根走法分数。
        std::vector<std::pair<Move, int>> rootScores;

        // 当前迭代临时收集的根走法分数。
        std::vector<std::pair<Move, int>> iterationRootScores;

        // 本搜索线各层的局面普通哈希（重复检测用，仅浅层写入）。
        std::array<uint64_t, kMaxPly> positionHistory = {};
    };

private:
    // ==================== 搜索调度 ====================

    // 在给定上下文上执行完整迭代加深流程。
    // isMaster 为 false 时（工作线程）还会检查主线程完成标志。
    void runIterativeDeepening(SearchContext& ctx, int depth, bool isMaster);

    // 工作线程入口。
    void runWorkerSearch(SearchContext* ctx, int depth);

    // 把某个上下文的结果发布到类级对外接口。
    void publishResults(SearchContext& ctx, int64_t startTime);

    // 把全部上下文的统计汇总到类级 m_stats。
    void aggregateStats(const SearchContext* contexts, size_t count);

    // 根节点随机选择：
    // 对除已知最优外的每个根走法，用窗口 [best-gap, +INF] 重新精确打分，
    // 失败低（真实值 <= best-gap）的走法被排除；候选集内按 softmax 加权随机。
    // 该窗口下不可能失败高（beta=INF），因此候选走法的分数全部是精确值。
    void applyExactRootScoring(SearchContext& ctx);

    // 根节点搜索：除了求值，还负责记录“本层迭代的最佳着法”。
    // alpha/beta 由调用方传入（迭代加深的 aspiration 窗口或全窗口）。
    int searchRoot(SearchContext& ctx, int depth, int alpha, int beta);

    // 常规 Alpha-Beta 递归搜索。
    int search(SearchContext& ctx, int depth, int alpha, int beta, int ply);

    // 静默搜索：深度 0 时只展开“能形成新三连”的走法与强制提子，
    // 减轻地平线效应；qdepth 用于限制链长。
    int quiescence(SearchContext& ctx, int alpha, int beta, int ply, int qdepth);

    // 对非终局局面进行静态评估。
    int evaluate(SearchContext& ctx, int ply) const;

    // 对终局局面进行评估，通常直接给出胜负分。
    int evaluateTerminal(SearchContext& ctx, int ply) const;

    // ==================== 走法生成 ====================

    // 按当前局面阶段统一生成合法走法列表。
    void generateMoves(SearchContext& ctx, MoveList& list) const;

    // 生成开局摆子阶段的落子走法。
    void generateOpeningMoves(SearchContext& ctx, MoveList& list) const;

    // 生成中局“待选子”状态下的所有可走子。
    void generateMidMoves(SearchContext& ctx, MoveList& list) const;

    // 从指定起点生成后续可落到的目标点位。
    void generateMovesFromSelected(SearchContext& ctx, MoveList& list, int32_t fromPos) const;

    // 生成当前提子阶段允许的全部提子走法。
    void generateCaptureMoves(SearchContext& ctx, MoveList& list) const;

    // ==================== 走法排序 ====================

    // 按动态排序键对走法排序；根节点额外优先沿用上一层最优着法，
    // 并（在 canonical 模式下）按对称等价类分组，使同类走法相邻。
    void orderMoves(SearchContext& ctx, MoveList& list, bool isRoot,
        const Move& ttMove, int ply) const;

    // 计算单个走法的动态排序键（静态启发 + TT 走法/杀手/历史加成）。
    int64_t orderKey(SearchContext& ctx, const Move& move, bool isRoot,
        const Move& ttMove, int ply) const;

    // 历史启发表加成（按来源点+目标点累计的剪枝贡献）。
    int moveHistoryBonus(SearchContext& ctx, const Move& move) const;

    // 把走法映射到对称等价类（对 16 个视角的 (type, from, to) 取最小）。
    // 仅用于根节点分组排序，开销是 16 次查表，与局面哈希无关。
    uint64_t canonicalMoveKey(const Move& move) const;

    // 为落子/走子计算排序分。
    int scorePlaceOrShiftMove(SearchContext& ctx, int32_t fromPos, int32_t toPos) const;

    // 为提子计算排序分。
    int scoreCaptureMove(SearchContext& ctx, int32_t pos) const;

    // ==================== 搜索执行 ====================

    // 在 ctx.board 上执行一个走法，并保存回退所需快照。
    void applyMove(SearchContext& ctx, const Move& move, Snapshot& snapshot);

    // 把 ctx.board 回退到快照记录的旧状态。
    void undoMove(SearchContext& ctx, const Snapshot& snapshot);

    // ==================== 置换表 ====================

    // 查询置换表；若命中精确值或命中后足以剪枝，则返回 true。
    // 命中时通过 ttMove 返回该节点已知的最佳走法（用于排序）。
    bool probeTransposition(SearchContext& ctx, int depth, int& alpha, int& beta,
        int& value, Move& ttMove) const;

    // 把当前节点结果写入置换表，同时记录该节点最佳走法。
    void storeTransposition(SearchContext& ctx, int depth, int value, int alpha, int beta,
        const Move& bestMove) const;

    // 当新的真实局面开始搜索时，切换到置换表的新 generation。
    void beginTranspositionGeneration();

    // 某个分片满时执行老化清理，优先删除更老、更浅、非精确值条目。
    void pruneTranspositionStore(TTStore::Shard& shard, size_t shardMax) const;

    // ==================== 哈希 ====================

    // 按当前哈希模式返回本节点的置换表键。
    uint64_t makeSearchHash(SearchContext& ctx) const;

    // 普通（非规范化）局面哈希：编号规则 hard，普通规则 lite，
    // 并按需混入 selectedPos，与 canonical 视角的语义保持一致。
    uint64_t makePlainHash(SearchContext& ctx) const;

    // 对所有对称变换生成哈希，取最小值作为规范化 key。
    uint64_t makeCanonicalHash(SearchContext& ctx) const;

    // 在某一个具体对称视角下生成局面哈希。
    uint64_t makeSymmetryHash(SearchContext& ctx, const SymmetryVariant& symmetry) const;

    // 将 selectedPos 额外混入哈希，避免 ACTION_PLACE 状态丢失关键信息。
    uint64_t mixSelectedPos(uint64_t hash, int32_t selectedPos) const;

    // ==================== 评估 ====================

    // 统计某一方当前形成的“可提三连”总数。
    // 编号规则下，历史中已登记过的同编号三连不再计入。
    int countClaimableMills(SearchContext& ctx, NineChess::Players player) const;

    // 判断某一方在指定三连线上形成的三连，是否已登记在历史中。
    bool isMillKeyInHistory(SearchContext& ctx, NineChess::Players player, uint32_t lineId) const;

    // 判断假设性落子后形成的新三连，其 MillKey 是否已在历史中。
    // 无法确定编号时保守返回 false（视为可提）。
    bool isHypotheticalMillInHistory(SearchContext& ctx, NineChess::Players player,
        uint32_t lineId, int32_t fromPos, int32_t toPos) const;

    // 开局落子时下一颗新棋子的编号。
    int32_t nextPlacementNumber(SearchContext& ctx, NineChess::Players player) const;

    // 统计某一方“二子成线且第三点为空”的活三潜力。
    int countOpenMills(SearchContext& ctx, NineChess::Players player) const;

    // 统计某一方当前局面的机动性。
    int countMobility(SearchContext& ctx, NineChess::Players player) const;

    // 统计某点位能阻断对手多少条潜在威胁线。
    int countBlockedThreats(SearchContext& ctx, NineChess::Players player, int32_t pos) const;

    // 统计某点位穿过的己方线资源，用于估计此点的重要性。
    int countLinesThroughPos(SearchContext& ctx, NineChess::Players player, int32_t pos) const;

    // 估计把棋子占到 toPos 后可立即形成多少个可提三连。
    // 编号规则下，已在历史中登记过的同编号三连不计入。
    int countClaimableMillsAfterOccupy(SearchContext& ctx, NineChess::Players player,
        int32_t fromPos, int32_t toPos) const;

    // 估计把棋子占到 toPos 后可形成多少个活三。
    int countOpenMillsAfterOccupy(SearchContext& ctx, NineChess::Players player,
        int32_t fromPos, int32_t toPos) const;

    // 统计某一方棋子所在点位的结构价值之和（点位数 = 该点穿过的三连线数）。
    int countPointValue(SearchContext& ctx, NineChess::Players player) const;

    // ==================== 工具 ====================

    // 判断两个走法在搜索语义上是否相同。
    bool isSameMove(const Move& lhs, const Move& rhs) const;

    // 把内部走法结构格式化成外部命令文本。
    std::string formatMove(const Move& move) const;

    // 每次搜索开始时按配置种子初始化随机源。
    void seedRng();

    // 重置类级统计计数。
    void resetStats();

    // 当前稳态时钟毫秒。
    static int64_t currentTimeMS();

    // 时间预算是否已耗尽（未设预算时恒为 false）。
    bool isTimeUp() const;

private:
    // 搜索开始时的根局面，不在递归中直接改动（多线程只读共享）。
    NineChess m_root;

    // 预计算的全部对称变换表（多线程只读共享；AI 与开局库共用同一工具）。
    NineChessSymmetry m_symmetry;

    // 每个点位穿过的三连线数（= 结构价值），setChess 时按规则重建。
    std::array<int8_t, BOARD_SIZE> m_pointValue = {};

    // 外部请求停止搜索的原子标志（多线程共享）。
    std::atomic<bool> m_requiredQuit;

    // 主线程完成标志：工作线程在迭代边界检查，主线程结束后尽快退出。
    std::atomic<bool> m_masterDone;

    // 最近一次完整算完的迭代深度（对外发布值）。
    int m_lastCompletedDepth = 0;

    // 最近一次完整算完的迭代估值（对外发布值）。
    int m_lastCompletedValue = 0;

    // 全部迭代结束后对外公布的最佳走法。
    Move m_bestMove = {};

    // 最佳走法对应的命令行文本。
    std::string m_bestMoveText;

    // 最近一次搜索的耗时（毫秒）。
    int64_t m_lastSearchTimeMs = 0;

    // 当前 AI 实例正在使用的置换表 generation。
    uint32_t m_generation = 0;

    // 搜索配置。
    SearchOptions m_options;

    // 搜索统计（多线程汇总后的总量）。
    mutable SearchStats m_stats;

    // 时间预算截止时刻（稳态毫秒）；0 表示不限时。
    int64_t m_timeDeadline = 0;

    // 根节点随机选择的随机源。
    std::mt19937_64 m_rng;

    // 对外发布的根走法分数（命令文本由 rootScoresText 生成）。
    std::vector<std::pair<Move, int>> m_rootScores;

    // 按规则分开的全局置换表：
    // 同规则不同 AI 实例共享缓存，不同规则之间彼此隔离。
    static std::array<TTStore, RULE_COUNT> s_ttStores;
};

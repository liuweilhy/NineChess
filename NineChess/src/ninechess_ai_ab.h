/****************************************************************************
** NineChess - Alpha-Beta AI
** 基于 Alpha-Beta 剪枝、置换表和对称局面归一化的 AI 搜索器
****************************************************************************/

#pragma once

#include "ninechess.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

class NineChess_AI_AB
{
public:
    // 构造一个空的 AI；真正搜索前需要先调用 setChess() 注入局面。
    NineChess_AI_AB();

    // 默认析构即可，AI 不拥有需要手工释放的外部资源。
    ~NineChess_AI_AB() = default;

    // 设置待搜索的根局面，并重置本轮搜索的内部状态。
    void setChess(const NineChess& chess);

    // 请求搜索尽快中止；供外部线程或控制层发出停止信号。
    void quit() { m_requiredQuit.store(true); }

    // 以给定深度执行迭代加深 Alpha-Beta 搜索，返回最终估值。
    int alphaBetaPruning(int depth);

    // 返回当前搜索得到的最佳着法文本。
    const char* bestMove();

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
        static constexpr size_t MAX_COUNT = 128;

        // 走法缓存数组。
        std::array<Move, MAX_COUNT> moves = {};

        // 当前实际存放的走法数量。
        size_t count = 0;
    };

    struct Snapshot {
        // 搜索中 applyMove() 会直接改写 m_search，
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

        // 最近一次被当前真实局面搜索访问到的代号，
        // 用于置换表老化清理时判断“新旧程度”。
        uint32_t generation = 0;
    };

    struct TTStore {
        // 同规则的置换表可能被多个 AI 实例共享，因此需要互斥保护。
        std::mutex mutex;

        // 从 canonical hash 到置换表条目的映射。
        std::unordered_map<uint64_t, TTEntry> table;

        // 当前规则置换表所在的“世代号”。
        uint32_t generation = 0;
    };

    struct SymmetryVariant {
        // 某一种等价变换下的完整映射关系。

        // 原点位 -> 变换后点位。
        std::array<int8_t, BOARD_SIZE> posMap = {};

        // 原三连线编号 -> 变换后线编号。
        std::array<int8_t, 20> lineMap = {};

        // 对于某条原线，变换后目标线的第 k 个位置对应原线的哪个槽位。
        // 九连棋映射 millHistory 时需要用它来重排 3 个编号槽。
        std::array<std::array<uint8_t, MILL>, 20> targetSource = {};
    };

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

    // 单规则置换表允许保存的最大条目数。
    static constexpr size_t MAX_TT_ENTRIES = 256u * 1024u;

    // 镜像、内外翻转和离散旋转组合后共有 16 种等价视角。
    static constexpr size_t SYMMETRY_COUNT = 16;

private:
    // 根节点搜索：除了求值，还负责记录“本层迭代的最佳着法”。
    int searchRoot(int depth);

    // 常规 Alpha-Beta 递归搜索。
    int search(int depth, int alpha, int beta, int ply);

    // 对非终局局面进行静态评估。
    int evaluate(int ply) const;

    // 对终局局面进行评估，通常直接给出胜负分。
    int evaluateTerminal(int ply) const;

    // 按当前局面阶段统一生成合法走法列表。
    void generateMoves(MoveList& list) const;

    // 生成开局摆子阶段的落子走法。
    void generateOpeningMoves(MoveList& list) const;

    // 生成中局“待选子”状态下的所有可走子。
    void generateMidMoves(MoveList& list) const;

    // 从指定起点生成后续可落到的目标点位。
    void generateMovesFromSelected(MoveList& list, int32_t fromPos) const;

    // 生成当前提子阶段允许的全部提子走法。
    void generateCaptureMoves(MoveList& list) const;

    // 按启发式分值对走法排序；根节点会额外优先沿用上一层最优着法。
    void orderMoves(MoveList& list, bool isRoot) const;

    // 为落子/走子计算排序分。
    int scorePlaceOrShiftMove(int32_t fromPos, int32_t toPos) const;

    // 为提子计算排序分。
    int scoreCaptureMove(int32_t pos) const;

    // 在 m_search 上执行一个走法，并保存回退所需快照。
    void applyMove(const Move& move, Snapshot& snapshot);

    // 把 m_search 回退到快照记录的旧状态。
    void undoMove(const Snapshot& snapshot);

    // 查询置换表；若命中精确值或命中后足以剪枝，则返回 true。
    bool probeTransposition(int depth, int& alpha, int& beta, int& value) const;

    // 把当前节点结果写入置换表。
    void storeTransposition(int depth, int value, int alpha, int beta) const;

    // 当新的真实局面开始搜索时，切换到置换表的新 generation。
    void beginTranspositionGeneration();

    // 置换表满时执行老化清理，优先删除更老、更浅、非精确值条目。
    void pruneTranspositionStore(TTStore& store) const;

    // 对所有对称变换生成哈希，取最小值作为规范化 key。
    uint64_t makeCanonicalHash() const;

    // 在某一个具体对称视角下生成局面哈希。
    uint64_t makeSymmetryHash(const SymmetryVariant& symmetry) const;

    // 将 selectedPos 额外混入哈希，避免 ACTION_PLACE 状态丢失关键信息。
    uint64_t mixSelectedPos(uint64_t hash, int32_t selectedPos) const;

    // 预计算本规则下全部 16 个等价变换。
    void buildSymmetryVariants();

    // 把一个位棋盘按给定对称变换映射到新视角。
    uint32_t mapBoard(uint32_t board, const SymmetryVariant& symmetry) const;

    // 把九连棋的历史三连 key 映射到给定对称视角。
    NineChess::MillKey mapMillKey(NineChess::MillKey key, const SymmetryVariant& symmetry) const;

    // 统计某一方当前形成的三连总数。
    int countAllMills(NineChess::Players player) const;

    // 统计某一方当前“二子成线且第三点为空”的活三潜力。
    int countOpenMills(NineChess::Players player) const;

    // 统计某一方当前局面的机动性。
    int countMobility(NineChess::Players player) const;

    // 统计某点位能阻断对手多少条潜在威胁线。
    int countBlockedThreats(NineChess::Players player, int32_t pos) const;

    // 统计某点位穿过的己方线资源，用于估计此点的重要性。
    int countLinesThroughPos(NineChess::Players player, int32_t pos) const;

    // 估计把棋子占到 toPos 后可立即形成多少个三连。
    int countMillsAfterOccupy(NineChess::Players player, int32_t fromPos, int32_t toPos) const;

    // 估计把棋子占到 toPos 后可形成多少个活三。
    int countOpenMillsAfterOccupy(NineChess::Players player, int32_t fromPos, int32_t toPos) const;

    // 判断两个走法在搜索语义上是否相同。
    bool isSameMove(const Move& lhs, const Move& rhs) const;

    // 把内部走法结构格式化成外部命令文本。
    std::string formatMove(const Move& move) const;

private:
    // 搜索开始时的根局面，不在递归中直接改动。
    NineChess m_root;

    // 递归搜索过程中实际被 applyMove()/undoMove() 改写的工作局面。
    mutable NineChess m_search;

    // 预计算的全部对称变换表。
    std::array<SymmetryVariant, SYMMETRY_COUNT> m_symmetries = {};

    // 当前规则实际可用的对称变换数量。
    size_t m_symmetryCount = 0;

    // 外部请求停止搜索的原子标志。
    std::atomic<bool> m_requiredQuit;

    // 当前这一层迭代是否被中途打断。
    bool m_iterationAborted = false;

    // 最近一次完整算完的迭代深度。
    int m_lastCompletedDepth = 0;

    // 最近一次完整算完的迭代估值。
    int m_lastCompletedValue = 0;

    // 全部迭代结束后对外公布的最佳走法。
    Move m_bestMove = {};

    // 当前迭代临时得到的最佳走法。
    Move m_iterationBestMove = {};

    // m_bestMove 对应的命令行文本。
    std::string m_bestMoveText;

    // 当前 AI 实例正在使用的置换表 generation。
    uint32_t m_generation = 0;

    // 按规则分开的全局置换表：
    // 同规则不同 AI 实例共享缓存，不同规则之间彼此隔离。
    static std::array<TTStore, RULE_COUNT> s_ttStores;
};

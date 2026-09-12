/****************************************************************************
** NineChess - Alpha-Beta AI
** 以Alpha-Beta算法设计的AI下棋程序，实现等价局面和置换表功能 ***************/

#include "ninechess_ai_ab.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

std::array<NineChess_AI_AB::TTStore, RULE_COUNT> NineChess_AI_AB::s_ttStores = {};

// 按规则的评估权重表。四行初始值相同（等于历史魔数），后续调参按行独立修改；
// 各列含义见 EvalWeights 字段注释。
//        开局: 子 手牌 三连 活二 | 中局: 子 三连 活二 机动 | 提子: 开局 中局 | 双三 闷杀 安全
const NineChess_AI_AB::EvalWeights NineChess_AI_AB::s_evalWeightsPerRule[RULE_COUNT] = {
    /* 0 成三棋 */ { 120,  48,  96,  24,      180, 112,  32,  10,       160,  220,    0,   0,   0 },
    /* 1 打三棋 */ { 120,  48,  96,  24,      180, 112,  32,  10,       160,  220,    0,   0,   0 },
    /* 2 九连棋 */ { 120,  48,  96,  24,      180, 112,  32,  10,       160,  220,    0,   0,   0 },
    /* 3 莫里斯 */ { 120,  48,  96,  24,      180, 112,  32,  10,       160,  220,    0,   0,   0 },
};

namespace {

// ===== 临时测量用（测完回退）：评估耗时占比 =====
inline uint64_t mix64(uint64_t value)
{
    value ^= value >> 30;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31;
    return value;
}

inline int clampScore(int value, int lower, int upper)
{
    return value < lower ? lower : (value > upper ? upper : value);
}

} // namespace

NineChess_AI_AB::NineChess_AI_AB()
    : m_requiredQuit(false),
      m_masterDone(false)
{
    m_bestMoveText = "error!";
    m_pointValue.fill(0);
}

uint32_t NineChess_AI_AB::defaultThreadCount()
{
    // 依据本机逻辑核心数给一个“开箱即用”的默认线程数：
    // 实测 Lazy SMP 扩展性为 4 线程约 3.7 倍 NPS、8 线程约 6.3 倍、16 线程约 8.8 倍，
    // 16 线程以上收益已明显递减，且桌面对局需要给界面/系统保留核心，故封顶 16。
    const unsigned hardware = std::thread::hardware_concurrency();
    if (hardware == 0u) {
        // 查询失败（部分平台/沙箱环境返回 0）时的保守猜测。
        return 4u;
    }
    if (hardware <= 2u) {
        return 1u;
    }
    if (hardware <= 4u) {
        return hardware - 1u;
    }
    return static_cast<uint32_t>(std::min<unsigned>(hardware - 2u, 16u));
}

void NineChess_AI_AB::refreshWeights()
{
    if (m_options.useCustomWeights) {
        // 调参/实验模式：整行覆盖（winPressure 仍是选项，不在表内）。
        m_weights = m_options.weights;
        return;
    }
    m_weights = s_evalWeightsPerRule[m_root.getRuleIndex()];
    if (m_options.stalematePressureWeight >= 0) {
        m_weights.stalematePressure = m_options.stalematePressureWeight;
    }
    if (m_options.forkThreatWeight >= 0) {
        m_weights.forkThreat = m_options.forkThreatWeight;
    }
    if (m_options.millSafetyWeight >= 0) {
        m_weights.millSafety = m_options.millSafetyWeight;
    }
}

void NineChess_AI_AB::setChess(const NineChess& chess)
{
    m_root = chess;
    // 根随机 randomPlies 需要"当前已走手数"：命令历史即将从副本剥离（见下），
    // 先在这里记录口径（摆/走/提各计 1，与限步判和同口径）。
    m_rootPlyCount = static_cast<int32_t>(chess.getCmdList()->size());
    refreshWeights();
    m_requiredQuit.store(false);
    m_masterDone.store(false);
    m_lastCompletedDepth = 0;
    m_lastCompletedValue = 0;
    m_bestMove = Move();
    m_bestMoveText = "error!";
    beginTranspositionGeneration();
    m_symmetry.build(chess);
    for (int32_t pos = 0; pos < BOARD_SIZE; ++pos) {
        m_pointValue[pos] = static_cast<int8_t>(chess.m_posLineCount[pos]);
    }
    // 回放真实对局历史，构建重复检测用的局面哈希轨迹。
    buildRealHistoryTrail(chess);

    // 搜索工作副本瘦身：命令历史与搜索无关，但每次迭代 ctx.board = m_root
    // 都会深拷贝这份 vector<string>（对局越长越贵，300 步的棋每迭代
    // 每线程拷贝上百个 string）。轨迹已在上面从原始局面构建完毕，
    // 这里把副本内的历史清掉。调用方传入的原局面不受影响。
    m_root.m_cmdHistory.clear();
    m_root.m_cmdHistory.shrink_to_fit();
}

void NineChess_AI_AB::buildRealHistoryTrail(const NineChess& chess)
{
    m_realHistoryHashes.clear();
    const std::vector<std::string>& history = chess.getCmdHistory();
    if (history.empty()) {
        return;
    }

    // 在一副临时棋上回放命令历史，逐命令记录局面普通哈希。
    // 命令历史只含走子/提子/终局命令（与保存的棋谱同一格式），
    // 终局命令（认输/判和）之后不会再有 AI 搜索，遇到执行失败即停止回放。
    // 变换命令会改写历史文本，因此回放结果与变换后的根局面保持一致。
    NineChess replay;
    replay.setRule(chess.getRuleIndex());
    replay.start();
    const size_t keep = static_cast<size_t>(kRepetitionMaxPly) + 2;
    for (const std::string& cmd : history) {
        if (!replay.command(cmd.c_str())) {
            break;
        }
        m_realHistoryHashes.push_back(plainHashOfBoard(replay));
    }
    if (m_realHistoryHashes.size() > keep) {
        m_realHistoryHashes.erase(m_realHistoryHashes.begin(),
            m_realHistoryHashes.end() - static_cast<std::ptrdiff_t>(keep));
    }
}

uint64_t NineChess_AI_AB::plainHashOfBoard(const NineChess& board) const
{
    // 与 makePlainHash() 逐位一致的全量重算版本：
    // 编号规则用 getHashHard()（内部与 hardHashCombine(lite, 全量层累加, 全量历史累加) 等价），
    // 普通规则用 getHashLite()，再按同样的规则混入 selectedPos。
    uint64_t hash = board.m_rule.allowRepeatedMills
        ? board.m_data.getHashLite()
        : board.m_data.getHashHard();

    int32_t selectedPos = -1;
    if (board.getPhase() == GAME_MID
        && board.getAction() == ACTION_PLACE
        && board.isValidPos(board.m_selectedPos)) {
        selectedPos = board.m_selectedPos;
    }
    return mixSelectedPos(hash, selectedPos);
}

void NineChess_AI_AB::clearTranspositionTable()
{
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    {
        std::lock_guard<std::mutex> lock(store.metaMutex);
        store.generation = 0u;
    }
    for (TTStore::Shard& shard : store.shards) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        for (TTStore::TTBucket& bucket : shard.buckets) {
            for (TTStore::TTSlot& slot : bucket.slotArray) {
                slot = TTStore::TTSlot();
            }
        }
    }
}

void NineChess_AI_AB::snapshotStats(SearchStats& out) const
{
    out.nodes.store(m_stats.nodes.load());
    out.ttProbes.store(m_stats.ttProbes.load());
    out.ttHits.store(m_stats.ttHits.load());
    out.ttStores.store(m_stats.ttStores.load());
    out.betaCutoffs.store(m_stats.betaCutoffs.load());
    out.repetitionHits.store(m_stats.repetitionHits.load());
}

std::string NineChess_AI_AB::rootScoresText() const
{
    std::string out;
    for (const auto& item : m_rootScores) {
        out += formatMove(item.first);
        out += " = ";
        out += std::to_string(item.second);
        out += "\n";
    }
    return out;
}

int64_t NineChess_AI_AB::currentTimeMS()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

bool NineChess_AI_AB::isTimeUp() const
{
    return m_timeDeadline > 0 && currentTimeMS() >= m_timeDeadline;
}

void NineChess_AI_AB::resetStats()
{
    m_stats.nodes.store(0u);
    m_stats.ttProbes.store(0u);
    m_stats.ttHits.store(0u);
    m_stats.ttStores.store(0u);
    m_stats.betaCutoffs.store(0u);
    m_stats.repetitionHits.store(0u);
}

void NineChess_AI_AB::seedRng()
{
    const uint64_t seed = m_options.seed != 0u
        ? m_options.seed
        : static_cast<uint64_t>(std::random_device{}());
    m_rng.seed(seed);
}

int NineChess_AI_AB::alphaBetaPruning(int depth)
{
    // 采用迭代加深：
    // 1. 浅层结果可以为深层排序；
    // 2. 如果外部要求中断或时间预算耗尽，仍然能保留“最后一次完整算完”的 best move。
    const int64_t startTime = currentTimeMS();
    m_lastCompletedDepth = 0;
    m_lastCompletedValue = 0;
    resetStats();
    m_hashVerifyFailed = false;
    refreshWeights();
    m_rootScores.clear();
    m_timeDeadline = m_options.timeLimitMs > 0
        ? startTime + m_options.timeLimitMs : 0;
    if (depth > kMaxPly - 1) {
        depth = kMaxPly - 1;
    }
    if (depth < 0) {
        depth = 0;
    }
    // 动态深度：开局（摆子阶段）分支数约为中局的三倍，等深度下节点数差百倍量级，
    // 同样的深度上限会让中局搜索“瞬间结束”而开局“耗尽预算”。
    // 中局根局面追加 2 层，把固定深度模式下的时间预算向中局倾斜；
    // 配合时间预算使用时该值只是抬高深度天花板，实际深度仍由预算决定。
    if (m_options.dynamicDepth && m_root.getPhase() == GAME_MID) {
        depth = std::min(depth + 2, kMaxPly - 1);
    }
    m_masterDone.store(false);
    seedRng();

    const uint32_t threadCount = std::max(1u, m_options.threads);

    if (threadCount == 1) {
        SearchContext ctx;
        ctx.board = m_root;
        runIterativeDeepening(ctx, depth, true);
        if (m_options.randomness > 0 && ctx.lastCompletedDepth >= 1) {
            applyExactRootScoring(ctx);
        }
        publishResults(ctx, startTime);
        aggregateStats(&ctx, 1);
        return m_lastCompletedValue;
    }

    // Lazy SMP：N 个线程各自独立跑同一套迭代加深，共享分片置换表；
    // 工作线程的搜索结果通过置换表反馈给主线程的后续迭代。
    std::vector<SearchContext> contexts(threadCount);
    std::vector<std::thread> workers;
    workers.reserve(threadCount - 1);
    for (uint32_t i = 1; i < threadCount; ++i) {
        contexts[i].board = m_root;
        workers.emplace_back(&NineChess_AI_AB::runWorkerSearch, this, &contexts[i], depth);
    }
    contexts[0].board = m_root;
    runIterativeDeepening(contexts[0], depth, true);
    m_masterDone.store(true);
    for (std::thread& worker : workers) {
        worker.join();
    }

    // 采用完成深度最大的上下文（平局取主线程）。
    size_t bestIndex = 0;
    for (size_t i = 1; i < contexts.size(); ++i) {
        if (contexts[i].lastCompletedDepth > contexts[bestIndex].lastCompletedDepth) {
            bestIndex = i;
        }
    }
    SearchContext& chosen = contexts[bestIndex];
    if (m_options.randomness > 0 && chosen.lastCompletedDepth >= 1) {
        applyExactRootScoring(chosen);
    }
    publishResults(chosen, startTime);
    aggregateStats(contexts.data(), contexts.size());
    return m_lastCompletedValue;
}

void NineChess_AI_AB::runIterativeDeepening(SearchContext& ctx, int depth, bool isMaster)
{
    ctx.lastCompletedDepth = 0;
    // 局面刚被整体设置为根局面，增量哈希累加器需全量重建。
    resetHardHashAccum(ctx);
    ctx.lastCompletedValue = evaluate(ctx, 0);
    ctx.bestMove = Move();
    ctx.bestMoveText = "error!";

    MoveList rootMoves;
    generateMoves(ctx, rootMoves);
    if (rootMoves.count == 0) {
        return;
    }

    orderMoves(ctx, rootMoves, true, Move(), 0);
    ctx.bestMove = rootMoves.moves[0];
    ctx.bestMoveText = formatMove(ctx.bestMove);

    if (depth <= 0) {
        return;
    }

    for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        if (m_requiredQuit.load() || isTimeUp()) {
            break;
        }
        if (!isMaster && m_masterDone.load()) {
            break;
        }

        ctx.board = m_root;
        resetHardHashAccum(ctx);
        ctx.iterationAborted = false;

        // 重复检测播种：positionHistory[0] 记为根局面哈希，
        // 这样搜索走法循环转回根局面时，常规的 back=2/4/6/8 比较就能捕获；
        // 同时携带根局面之前的真实对局历史轨迹。
        ctx.positionHistory[0] = makePlainHash(ctx);
        ctx.realTrailCount = static_cast<int>(m_realHistoryHashes.size());
        for (size_t i = 0; i < m_realHistoryHashes.size()
            && i < ctx.realTrail.size(); ++i) {
            ctx.realTrail[i] = m_realHistoryHashes[i];
        }

        int value = 0;
        if (m_options.aspirationWindows && currentDepth >= 3
            && ctx.lastCompletedValue > -WIN_SCORE + 100
            && ctx.lastCompletedValue < WIN_SCORE - 100) {
            // Aspiration：以上一迭代估值为中心收窄根窗口，减少不必要的子树展开。
            // 本游戏估值跨层跳变大（一个三连 112、一次提子 220），
            // 因此采用渐进放宽：先窄窗口，失败后窗口 ×3，仍失败才全窗口重搜。
            const int baseDelta = 300;
            int delta = baseDelta;
            int narrowAlpha = ctx.lastCompletedValue - delta;
            int narrowBeta = ctx.lastCompletedValue + delta;
            value = searchRoot(ctx, currentDepth, narrowAlpha, narrowBeta);
            if (ctx.iterationAborted) {
                break;
            }
            if (value <= narrowAlpha || value >= narrowBeta) {
                ctx.board = m_root;
                resetHardHashAccum(ctx);
                ctx.iterationAborted = false;
                delta *= 3;
                narrowAlpha = ctx.lastCompletedValue - delta;
                narrowBeta = ctx.lastCompletedValue + delta;
                value = searchRoot(ctx, currentDepth, narrowAlpha, narrowBeta);
                if (ctx.iterationAborted) {
                    break;
                }
                if (value <= narrowAlpha || value >= narrowBeta) {
                    ctx.board = m_root;
                    resetHardHashAccum(ctx);
                    ctx.iterationAborted = false;
                    value = searchRoot(ctx, currentDepth, -INF_SCORE, INF_SCORE);
                    if (ctx.iterationAborted) {
                        break;
                    }
                }
            }
        }
        else {
            value = searchRoot(ctx, currentDepth, -INF_SCORE, INF_SCORE);
            if (ctx.iterationAborted) {
                break;
            }
        }

        ctx.lastCompletedDepth = currentDepth;
        ctx.lastCompletedValue = value;
        ctx.bestMove = ctx.iterationBestMove;
        ctx.bestMoveText = formatMove(ctx.bestMove);
        ctx.rootScores = ctx.iterationRootScores;
    }
}

void NineChess_AI_AB::runWorkerSearch(SearchContext* ctx, int depth)
{
    runIterativeDeepening(*ctx, depth, false);
}

void NineChess_AI_AB::publishResults(SearchContext& ctx, int64_t startTime)
{
    m_lastCompletedDepth = ctx.lastCompletedDepth;
    m_lastCompletedValue = ctx.lastCompletedValue;
    m_bestMove = ctx.bestMove;
    m_bestMoveText = ctx.bestMoveText;
    m_rootScores = ctx.rootScores;
    m_lastSearchTimeMs = currentTimeMS() - startTime;
}

void NineChess_AI_AB::aggregateStats(const SearchContext* contexts, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        m_stats.nodes.fetch_add(contexts[i].stats.nodes.load());
        m_stats.ttProbes.fetch_add(contexts[i].stats.ttProbes.load());
        m_stats.ttHits.fetch_add(contexts[i].stats.ttHits.load());
        m_stats.ttStores.fetch_add(contexts[i].stats.ttStores.load());
        m_stats.betaCutoffs.fetch_add(contexts[i].stats.betaCutoffs.load());
        m_stats.repetitionHits.fetch_add(contexts[i].stats.repetitionHits.load());
    }
}

void NineChess_AI_AB::applyExactRootScoring(SearchContext& ctx)
{
    // randomPlies 限定：仅开局前 N 条命令内启用根随机，之后自动恢复纯最优。
    // 开局对称等价着法多、随机损失小；中残局每一手都关键，且跳过候选
    // 重打分还能省去整个精确打分阶段的时间开销。
    if (m_options.randomPlies > 0 && m_rootPlyCount >= m_options.randomPlies) {
        return;
    }
    // 根节点随机选择的“精确打分”阶段：
    // 1. 迭代加深已保证 ctx.lastCompletedValue 是精确值（首个根走法以全窗口搜索）；
    // 2. 对其它根走法用窗口 [best-gap, +INF] 搜索：失败低（真实值 <= best-gap）直接排除；
    //    beta=INF 意味着不可能失败高，因此候选走法的分数全部是精确值；
    // 3. 候选集内按 softmax 权重随机选一个，杜绝“第二名很臭”的走法被选中。
    const int depth = ctx.lastCompletedDepth;
    if (depth < 1) {
        return;
    }
    const int gap = std::max(1, m_options.randomGap);

    ctx.board = m_root;
    resetHardHashAccum(ctx);
    MoveList moves;
    generateMoves(ctx, moves);
    if (moves.count <= 1) {
        return;
    }

    orderMoves(ctx, moves, true, Move(), 0);

    // 重新推导最优走法的精确值：Aspiration 窗口的边界缓存可能使
    // lastCompletedValue 成为上下界而非精确值，这里用全窗口重搜一次作为基准，
    // 保证候选集的分差比较自洽。
    int bestValue = 0;
    {
        Snapshot snapshot;
        ctx.board = m_root;
        resetHardHashAccum(ctx);
        applyMove(ctx, ctx.bestMove, snapshot);
        bestValue = search(ctx, depth - 1, -INF_SCORE, INF_SCORE, 1);
        undoMove(ctx, snapshot);
        if (ctx.iterationAborted) {
            return;
        }
    }
    const int alpha = std::max(-INF_SCORE, bestValue - gap);

    std::vector<std::pair<Move, int>> exact;
    exact.reserve(moves.count);
    // 迭代的最优走法已带重新推导的精确值。
    exact.push_back(std::make_pair(ctx.bestMove, bestValue));

    for (size_t i = 0; i < moves.count; ++i) {
        if (isSameMove(moves.moves[i], ctx.bestMove)) {
            continue;
        }
        if (m_requiredQuit.load() || isTimeUp()) {
            break;
        }

        Snapshot snapshot;
        ctx.board = m_root;
        resetHardHashAccum(ctx);
        applyMove(ctx, moves.moves[i], snapshot);
        const int value = search(ctx, depth - 1, alpha, INF_SCORE, 1);
        undoMove(ctx, snapshot);

        if (ctx.iterationAborted) {
            break;
        }
        if (value > alpha) {
            exact.push_back(std::make_pair(moves.moves[i], value));
        }
    }

    std::sort(exact.begin(), exact.end(),
        [](const std::pair<Move, int>& lhs, const std::pair<Move, int>& rhs) {
            return lhs.second > rhs.second;
        });
    ctx.rootScores = exact;

    if (m_options.randomness > 0 && exact.size() > 1) {
        const double temperature = std::max(1.0, static_cast<double>(gap) / 3.0);
        const double bestScore = static_cast<double>(exact.front().second);
        std::vector<double> weights;
        weights.reserve(exact.size());
        double total = 0.0;
        for (const auto& item : exact) {
            const double w = std::exp((static_cast<double>(item.second) - bestScore) / temperature);
            weights.push_back(w);
            total += w;
        }

        std::uniform_real_distribution<double> dist(0.0, total);
        const double draw = dist(m_rng);
        double acc = 0.0;
        for (size_t i = 0; i < exact.size(); ++i) {
            acc += weights[i];
            if (draw <= acc) {
                ctx.bestMove = exact[i].first;
                ctx.bestMoveText = formatMove(ctx.bestMove);
                break;
            }
        }
    }
}

const char* NineChess_AI_AB::bestMove()
{
    return m_bestMoveText.empty() ? "error!" : m_bestMoveText.c_str();
}

int NineChess_AI_AB::searchRoot(SearchContext& ctx, int depth, int alpha, int beta)
{
    // 根节点与普通节点的区别在于：
    // 普通节点只关心分值，根节点还需要把“哪一步走到这个分值”记下来。
    MoveList moves;
    generateMoves(ctx, moves);
    if (moves.count == 0) {
        return evaluate(ctx, 0);
    }

    ctx.iterationRootScores.clear();
    orderMoves(ctx, moves, true, Move(), 0);

    const bool maximizing = ctx.board.getTurn() == PLAYER1;
    int bestValue = maximizing ? -INF_SCORE : INF_SCORE;
    ctx.iterationBestMove = moves.moves[0];

    for (size_t i = 0; i < moves.count; ++i) {
        if (m_requiredQuit.load() || isTimeUp()) {
            ctx.iterationAborted = true;
            break;
        }
        ++ctx.stats.nodes;

        Snapshot snapshot;
        applyMove(ctx, moves.moves[i], snapshot);
        const int value = search(ctx, depth - 1, alpha, beta, 1);
        undoMove(ctx, snapshot);

        if (ctx.iterationAborted) {
            break;
        }

        ctx.iterationRootScores.push_back(std::make_pair(moves.moves[i], value));

        if (maximizing) {
            if (value > bestValue || (value == bestValue && i == 0u)) {
                bestValue = value;
                ctx.iterationBestMove = moves.moves[i];
            }
            if (bestValue > alpha) {
                alpha = bestValue;
            }
        }
        else {
            if (value < bestValue || (value == bestValue && i == 0u)) {
                bestValue = value;
                ctx.iterationBestMove = moves.moves[i];
            }
            if (bestValue < beta) {
                beta = bestValue;
            }
        }

        if (alpha >= beta) {
            break;
        }
    }

    return bestValue;
}

int NineChess_AI_AB::search(SearchContext& ctx, int depth, int alpha, int beta, int ply)
{
    if (m_requiredQuit.load()) {
        ctx.iterationAborted = true;
        return evaluate(ctx, ply);
    }

    // 时间预算：每 1024 个节点检查一次，避免高频时钟调用拖慢搜索。
    if ((++ctx.nodeCounterForTime & 1023u) == 0u && isTimeUp()) {
        ctx.iterationAborted = true;
        return evaluate(ctx, ply);
    }
    ++ctx.stats.nodes;

    if (ctx.board.getPhase() == GAME_OVER) {
        return evaluateTerminal(ctx, ply);
    }

    // 重复检测：2/4/6/8 层内回到完全相同局面时，给当前轮次一个小惩罚，
    // 让引擎主动避免无意义的互磨循环。只在前 kRepetitionMaxPly 层进行
    // （深层节点数量巨大，逐个计算普通哈希会拖慢搜索，而重复主要影响浅层计划）。
    if (m_options.repetitionPenalty > 0 && ply <= kRepetitionMaxPly) {
        ctx.positionHistory[ply] = makePlainHash(ctx);
        bool repeated = false;
        for (int back = 2; back <= 8 && back <= ply; back += 2) {
            if (ctx.positionHistory[ply - back] == ctx.positionHistory[ply]) {
                repeated = true;
                break;
            }
        }
        if (!repeated) {
            // 再与根局面之前的真实对局历史比较（末位为根局面）。
            // 哈希包含轮次/动作等状态位，不同轮次的轨迹条目不可能误匹配。
            for (int j = ctx.realTrailCount - 1;
                j >= 0 && (ctx.realTrailCount - 1 - j) < 8; --j) {
                if (ctx.realTrail[j] == ctx.positionHistory[ply]) {
                    repeated = true;
                    break;
                }
            }
        }
        if (repeated) {
            const int penalty = m_options.repetitionPenalty;
            ++ctx.stats.repetitionHits;
            return ctx.board.getTurn() == PLAYER1 ? -penalty : penalty;
        }
    }

    if (depth <= 0) {
        if (m_options.quiescenceSearch) {
            return quiescence(ctx, alpha, beta, ply, 0);
        }
        return evaluate(ctx, ply);
    }

    // 增量哈希校验：常态每 2048 个节点抽验一次，selfcheck 模式逐节点校验。
    // 累加器与全量重算不一致说明差量维护有漏洞，立即中止搜索并置失败标志，
    // 避免错误键值静默污染置换表。
    if (!ctx.board.m_rule.allowRepeatedMills) {
        const bool verifyNow = m_verifyHashAccum
            || ((++ctx.hashVerifyCounter & 2047u) == 0u);
        if (verifyNow && !verifyHardHashAccum(ctx)) {
            ctx.hashVerifyFailed = true;
            m_hashVerifyFailed = true;
            ctx.iterationAborted = true;
            return evaluate(ctx, ply);
        }
    }

    const int originalAlpha = alpha;
    const int originalBeta = beta;

    // 本节点的置换表键只计算一次，probe 与 store 共用。
    // canonical 模式下一次计算要跑 16 个视角哈希，原来 probe/store 各算一遍是纯浪费。
    const uint64_t hash = makeSearchHash(ctx);

    int ttValue = 0;
    Move ttMove;
    // 先查置换表：
    // - 精确命中时可以直接复用；
    // - 边界命中时可以先收紧窗口，再决定是否已经足够剪枝；
    // - 命中的最佳走法用于后续走法排序。
    if (probeTransposition(ctx, hash, depth, alpha, beta, ttValue, ttMove)) {
        return ttValue;
    }

    MoveList moves;
    generateMoves(ctx, moves);
    if (moves.count == 0) {
        return evaluate(ctx, ply);
    }

    orderMoves(ctx, moves, false, ttMove, ply);

    const bool maximizing = ctx.board.getTurn() == PLAYER1;
    int bestValue = maximizing ? -INF_SCORE : INF_SCORE;
    Move bestMoveAtNode;
    bool hasBestMove = false;

    for (size_t i = 0; i < moves.count; ++i) {
        // PVS：首走法用当前窗口搜索；其余走法先以零宽窗口试探，
        // 只有试探值落回 (alpha, beta) 内（可能是更优走法但值不精确）才全窗口重搜。
        // 排序质量越高试探失败率越高，等效子树越小。
        int childAlpha = alpha;
        int childBeta = beta;
        if (m_options.pvSearch && i > 0u && alpha > -INF_SCORE && beta < INF_SCORE) {
            if (maximizing) {
                childBeta = alpha + 1;
            }
            else {
                childAlpha = beta - 1;
            }
        }

        Snapshot snapshot;
        applyMove(ctx, moves.moves[i], snapshot);
        // 强制提子延伸：走子成三后，走子方进入被迫提子状态，
        // 该强制节点深度不减 1，让“成三→提子→后续”的战术链在主搜索内算清。
        // 只在浅层（depth <= 2，即静默边界前）延伸：
        // 深层延伸会让节点数膨胀数倍，而 depth 0 的地平线已由静默搜索兜底；
        // 有界性：每次延伸后必然伴随提子，盘面子数单调递减。
        const int extension = (m_options.forcedExtension && depth <= 2
            && ctx.board.getAction() == ACTION_CAPTURE) ? 1 : 0;

        // LMR：排序靠后的静默走法先减层试探，失败高再恢复全深。
        // 排序键（order）含静态启发 + TT/杀手/历史加成：
        // 键 < 2400 意味着该走法不成三且没有显著启发加成，才允许减搜。
        // 减搜量随深度与走法序号递增；保证子节点至少剩 1 层（reduction <= depth - 2）。
        int reduction = 0;
        if (m_options.lateMoveReductions && depth >= 3 && i >= 4u
            && moves.moves[i].type != MOVE_CAPTURE && moves.moves[i].order < 2400) {
            reduction = 1 + (depth >= 6 ? 1 : 0) + (i >= 12 ? 1 : 0);
            if (reduction > depth - 2) {
                reduction = depth - 2;
            }
        }

        int value = search(ctx, depth - 1 + extension - reduction,
            childAlpha, childBeta, ply + 1);
        undoMove(ctx, snapshot);

        if (ctx.iterationAborted) {
            return value;
        }

        if (reduction > 0 && value > alpha) {
            // 减搜失败高：该走法可能被低估，恢复全深重搜（零宽窗口）。
            applyMove(ctx, moves.moves[i], snapshot);
            value = search(ctx, depth - 1 + extension, childAlpha, childBeta, ply + 1);
            undoMove(ctx, snapshot);
            if (ctx.iterationAborted) {
                return value;
            }
        }

        if (m_options.pvSearch && i > 0u && value > alpha && value < beta) {
            // 试探发现可能的更优走法：全窗口重搜取得精确值。
            applyMove(ctx, moves.moves[i], snapshot);
            value = search(ctx, depth - 1 + extension, alpha, beta, ply + 1);
            undoMove(ctx, snapshot);
            if (ctx.iterationAborted) {
                return value;
            }
        }

        if (maximizing) {
            if (value > bestValue) {
                bestValue = value;
                bestMoveAtNode = moves.moves[i];
                hasBestMove = true;
            }
            if (bestValue > alpha) {
                alpha = bestValue;
            }
        }
        else {
            if (value < bestValue) {
                bestValue = value;
                bestMoveAtNode = moves.moves[i];
                hasBestMove = true;
            }
            if (bestValue < beta) {
                beta = bestValue;
            }
        }

        // Alpha-Beta 的核心：
        // 当前节点已经找到一个“至少不比 alpha 差 / 至多不比 beta 好”的选择时，
        // 后续兄弟节点不可能再影响祖先决策，直接停止展开。
        if (alpha >= beta) {
            ++ctx.stats.betaCutoffs;
            // 杀手走法与历史启发：只记录非提子走法，
            // 让相同深度的兄弟节点优先尝试“刚刚剪枝成功”的走法。
            if (moves.moves[i].type != MOVE_CAPTURE && !isSameMove(moves.moves[i], ttMove)) {
                if (!isSameMove(moves.moves[i], ctx.killers[ply][0])) {
                    ctx.killers[ply][1] = ctx.killers[ply][0];
                    ctx.killers[ply][0] = moves.moves[i];
                }
                const int32_t fromIndex = moves.moves[i].from + 1;
                if (fromIndex >= 0 && fromIndex <= BOARD_SIZE) {
                    ctx.history[fromIndex][moves.moves[i].to] += depth * depth;
                }
            }
            break;
        }
    }

    // 用进入节点时的原始窗口来决定 bestValue 是精确值、上界还是下界。
    storeTransposition(ctx, hash, depth, bestValue, originalAlpha, originalBeta,
        hasBestMove ? bestMoveAtNode : Move());
    return bestValue;
}

int NineChess_AI_AB::quiescence(SearchContext& ctx, int alpha, int beta, int ply, int qdepth)
{
    // 静默搜索：深度耗尽后，只展开“有战术价值”的走法，减轻地平线效应：
    // - 提子阶段：全部提子走法（强制）；
    // - 其它阶段：能形成新三连的落子/移子（即将到来的提子收益）。
    // 链长受 kMaxQuiescenceDepth 限制；提子本身使盘面子数单调递减，因此有界。
    if (qdepth >= kMaxQuiescenceDepth || ctx.board.getPhase() == GAME_OVER) {
        return evaluate(ctx, ply);
    }

    const bool maximizing = ctx.board.getTurn() == PLAYER1;
    int best = evaluate(ctx, ply);
    if (maximizing) {
        if (best >= beta) {
            return best;
        }
        if (best > alpha) {
            alpha = best;
        }
    }
    else {
        if (best <= alpha) {
            return best;
        }
        if (best < beta) {
            beta = best;
        }
    }

    MoveList moves;
    if (ctx.board.getAction() == ACTION_CAPTURE) {
        generateCaptureMoves(ctx, moves);
    }
    else {
        generateMoves(ctx, moves);
        size_t kept = 0;
        for (size_t i = 0; i < moves.count; ++i) {
            const Move& move = moves.moves[i];
            const bool formsMill =
                countClaimableMillsAfterOccupy(ctx, ctx.board.getTurn(),
                    move.from, move.to) > 0;
            if (formsMill) {
                moves.moves[kept++] = move;
            }
        }
        moves.count = kept;
    }

    for (size_t i = 0; i < moves.count; ++i) {
        Snapshot snapshot;
        applyMove(ctx, moves.moves[i], snapshot);
        const int value = quiescence(ctx, alpha, beta, ply + 1, qdepth + 1);
        undoMove(ctx, snapshot);

        if (ctx.iterationAborted) {
            return value;
        }

        if (maximizing) {
            if (value > best) {
                best = value;
            }
            if (best > alpha) {
                alpha = best;
            }
        }
        else {
            if (value < best) {
                best = value;
            }
            if (best < beta) {
                beta = best;
            }
        }

        if (alpha >= beta) {
            break;
        }
    }

    return best;
}

int NineChess_AI_AB::evaluate(SearchContext& ctx, int ply) const
{
    if (ctx.board.getPhase() == GAME_OVER) {
        return evaluateTerminal(ctx, ply);
    }

    const EvalWeights& w = m_weights;

    // 掩码算一次、全函数共享（原先各 helper 各算一遍）。
    const uint32_t board1 = ctx.board.boardOf(PLAYER1) & ctx.board.m_validBoardMask;
    const uint32_t board2 = ctx.board.boardOf(PLAYER2) & ctx.board.m_validBoardMask;
    const uint32_t occupied =
        (ctx.board.m_data.player1Board | ctx.board.m_data.player2Board | ctx.board.m_data.forbiddenBoard)
        & ctx.board.m_validBoardMask;

    const int onBoardDiff = static_cast<int>(POPCOUNT32(board1))
        - static_cast<int>(POPCOUNT32(board2));
    const int inHandDiff =
        static_cast<int>(ctx.board.getPlayer1InHand())
        - static_cast<int>(ctx.board.getPlayer2InHand());

    // 可提三连、活二、成三点、三连中棋子同域扫描，一遍完成；
    // 编号规则下，历史中已登记过的同编号三连不再视为可提三连。
    int claimable1 = 0;
    int openMill1 = 0;
    int forkPoint1 = 0;
    int claimable2 = 0;
    int openMill2 = 0;
    int forkPoint2 = 0;
    uint32_t inMillMask1 = 0;
    uint32_t inMillMask2 = 0;
    countMillsAndOpenMills(ctx, PLAYER1, occupied, claimable1, openMill1, forkPoint1, inMillMask1);
    countMillsAndOpenMills(ctx, PLAYER2, occupied, claimable2, openMill2, forkPoint2, inMillMask2);
    const int millDiff = claimable1 - claimable2;
    const int openMillDiff = openMill1 - openMill2;
    const int pointValueDiff = countPointValue(ctx, board1) - countPointValue(ctx, board2);
    const int inMillDiff = static_cast<int>(POPCOUNT32(board1 & inMillMask1))
        - static_cast<int>(POPCOUNT32(board2 & inMillMask2));

    int mobility1 = 0;
    int mobility2 = 0;
    if (ctx.board.getPhase() == GAME_MID) {
        mobility1 = countMobility(ctx, PLAYER1, occupied);
        mobility2 = countMobility(ctx, PLAYER2, occupied);
    }
    const int mobilityDiff = mobility1 - mobility2;

    // 博弈阶段插值：phaseT ∈ [0, 256]，256 = 纯开局权重，0 = 纯中局权重。
    // tapered 模式下按“剩余手牌比例”连续过渡，消除摆子结束那一刻的估值跳变；
    // 传统模式下按阶段硬切换（OPENING/NOTSTARTED = 256，MID = 0），行为与旧版一致。
    int32_t phaseT = 256;
    if (m_options.taperedEval) {
        const int32_t piecesPerSide = static_cast<int32_t>(ctx.board.m_rule.piecesPerSide);
        const int32_t totalInHand = static_cast<int32_t>(ctx.board.getPlayer1InHand())
            + static_cast<int32_t>(ctx.board.getPlayer2InHand());
        phaseT = piecesPerSide > 0
            ? std::min<int32_t>(256, totalInHand * 256 / (2 * piecesPerSide))
            : 0;
    }
    else if (ctx.board.getPhase() != GAME_NOTSTARTED
        && ctx.board.getPhase() != GAME_OPENING) {
        phaseT = 0;
    }
    const auto blend = [phaseT](int32_t openingWeight, int32_t midWeight) -> int32_t {
        return (openingWeight * phaseT + midWeight * (256 - phaseT)) / 256;
    };

    int score = 0;
    score += onBoardDiff * blend(w.openingMaterial, w.midMaterial);
    score += inHandDiff * w.openingInHand;
    score += millDiff * blend(w.openingMill, w.midMill);
    score += openMillDiff * blend(w.openingOpenMill, w.midOpenMill);
    score += mobilityDiff * w.midMobility;

    // 被闷杀压力：对手机动性枯竭时陡峭加分（blockedIsLoss 规则下低机动
    // 逼近胜负；规则 2 被闷为轮空，价值较低，由权重表区分）。
    // 默认权重 0，经 vs 对抗 A/B 验证后再定值。
    if (w.stalematePressure != 0 && ctx.board.getPhase() == GAME_MID) {
        if (mobility2 <= 3) {
            score += (4 - mobility2) * w.stalematePressure;
        }
        if (mobility1 <= 3) {
            score -= (4 - mobility1) * w.stalematePressure;
        }
    }

    // 双三连威胁：成三点 ≥2 时每个"额外"成三点加一份分——
    // 对手一手只能堵一个点，互不相同的成三点才是真威胁；
    // 开局/中局都适用（摆子阶段的活二是主要威胁形态）。
    // 默认权重 0，经 vs 对抗 A/B 验证后再定值。
    if (w.forkThreat != 0) {
        const int forkBonus1 = forkPoint1 >= 2 ? forkPoint1 - 1 : 0;
        const int forkBonus2 = forkPoint2 >= 2 ? forkPoint2 - 1 : 0;
        score += (forkBonus1 - forkBonus2) * w.forkThreat;
    }

    // 封闭三连保护：处于己方三连中的棋子按规则不可被提（除非全部在三连中），
    // 是真实的子力安全度量；编号规则下已登记的三连同样保护棋子。
    // 默认权重 0，经 vs 对抗 A/B 验证后再定值。
    if (w.millSafety != 0) {
        score += inMillDiff * w.millSafety;
    }

    // 点位结构价值：优先占据穿线更多的点位（边中点 > 角点）。
    score += pointValueDiff * m_options.pointValueWeight;

    if (ctx.board.getAction() == ACTION_CAPTURE) {
        const int captureBonus =
            blend(w.captureOpening, w.captureMid)
            * static_cast<int>(ctx.board.getPendingCaptures());
        score += ctx.board.getTurn() == PLAYER1 ? captureBonus : -captureBonus;
    }

    // 双方总子力（在盘 + 手牌），胜利临近压力与步数急迫项共用。
    const int total1 = static_cast<int>(ctx.board.getPlayer1OnBoardCount())
        + static_cast<int>(ctx.board.getPlayer1InHand());
    const int total2 = static_cast<int>(ctx.board.getPlayer2OnBoardCount())
        + static_cast<int>(ctx.board.getPlayer2InHand());

    // 胜利临近压力：判负条件是“在盘 + 手牌”总数低于 minPiecesToSurvive，
    // 对手总数逼近该线时逐子加分，促使优势方主动转化子力优势，
    // 而不是把局面磨到步数上限平局（实测深度 8 自对弈平局率约九成）。
    if (m_options.winPressureWeight != 0) {
        const uint32_t threshold = ctx.board.m_rule.minPiecesToSurvive + 2u;
        const int pressure1 = total2 <= static_cast<int>(threshold)
            ? (static_cast<int>(threshold) - total2 + 1) * m_options.winPressureWeight
            : 0;
        const int pressure2 = total1 <= static_cast<int>(threshold)
            ? (static_cast<int>(threshold) - total1 + 1) * m_options.winPressureWeight
            : 0;
        score += pressure1 - pressure2;
    }

    // 步数上限急迫分：接近限步判和时，子力领先方每一子优势的“剩余价值”
    // 随步数耗尽递减——领先方必须尽快转化，否则按平局计。
    // 提示值由控制层传入（模型层不感知限步），搜索内按 ply 递减近似；
    // 领先子数封顶 3，避免大优局面过度冒险。
    if (m_options.stepsRemainingHint > 0) {
        const int remaining = std::max(1, m_options.stepsRemainingHint - ply);
        constexpr int kUrgencySpan = 24;
        if (remaining < kUrgencySpan) {
            const int lead = std::max(-3, std::min(3, total1 - total2));
            score += lead * (kUrgencySpan - remaining) * 8;
        }
    }

    return clampScore(score, -WIN_SCORE + ply, WIN_SCORE - ply);
}

int NineChess_AI_AB::evaluateTerminal(SearchContext& ctx, int ply) const
{
    if (ctx.board.getWinner() == PLAYER1) {
        return WIN_SCORE - ply;
    }
    if (ctx.board.getWinner() == PLAYER2) {
        return -WIN_SCORE + ply;
    }
    return 0;
}

void NineChess_AI_AB::generateMoves(SearchContext& ctx, MoveList& list) const
{
    list.count = 0;
    if (ctx.board.getPhase() == GAME_OVER) {
        return;
    }

    if (ctx.board.getAction() == ACTION_CAPTURE) {
        generateCaptureMoves(ctx, list);
        return;
    }

    if (ctx.board.getPhase() == GAME_NOTSTARTED || ctx.board.getPhase() == GAME_OPENING) {
        generateOpeningMoves(ctx, list);
        return;
    }

    if (ctx.board.getPhase() == GAME_MID) {
        if (ctx.board.getAction() == ACTION_PLACE && ctx.board.isValidPos(ctx.board.m_selectedPos)) {
            generateMovesFromSelected(ctx, list, ctx.board.m_selectedPos);
        }
        else {
            generateMidMoves(ctx, list);
        }
    }
}

void NineChess_AI_AB::generateOpeningMoves(SearchContext& ctx, MoveList& list) const
{
    const uint32_t occupied =
        (ctx.board.m_data.player1Board | ctx.board.m_data.player2Board | ctx.board.m_data.forbiddenBoard)
        & ctx.board.m_validBoardMask;
    uint32_t empty = (~occupied) & ctx.board.m_validBoardMask;

    while (empty != 0u && list.count < MoveList::MAX_COUNT) {
        const int32_t pos = CTZ32(empty);
        Move& move = list.moves[list.count++];
        move.type = MOVE_PLACE;
        move.from = -1;
        move.to = static_cast<int8_t>(pos);
        move.order = static_cast<int32_t>(scorePlaceOrShiftMove(ctx, -1, pos));
        empty &= empty - 1u;
    }
}

void NineChess_AI_AB::generateMidMoves(SearchContext& ctx, MoveList& list) const
{
    const NineChess::Players turn = ctx.board.getTurn();
    uint32_t pieces = ctx.board.boardOf(turn) & ctx.board.m_validBoardMask;

    while (pieces != 0u && list.count < MoveList::MAX_COUNT) {
        const int32_t fromPos = CTZ32(pieces);
        generateMovesFromSelected(ctx, list, fromPos);
        pieces &= pieces - 1u;
    }
}

void NineChess_AI_AB::generateMovesFromSelected(SearchContext& ctx, MoveList& list, int32_t fromPos) const
{
    if (!ctx.board.isValidPos(fromPos)) {
        return;
    }

    const NineChess::Players turn = ctx.board.getTurn();
    const uint32_t occupied =
        (ctx.board.m_data.player1Board | ctx.board.m_data.player2Board | ctx.board.m_data.forbiddenBoard)
        & ctx.board.m_validBoardMask;
    const uint32_t empty = (~occupied) & ctx.board.m_validBoardMask;
    uint32_t targets = 0u;

    if (ctx.board.canFly(turn)) {
        targets = empty;
    }
    else {
        targets = ctx.board.m_moveMask[fromPos] & empty & ctx.board.m_validBoardMask;
    }

    while (targets != 0u && list.count < MoveList::MAX_COUNT) {
        const int32_t toPos = CTZ32(targets);
        Move& move = list.moves[list.count++];
        move.type = MOVE_SHIFT;
        move.from = static_cast<int8_t>(fromPos);
        move.to = static_cast<int8_t>(toPos);
        move.order = static_cast<int32_t>(scorePlaceOrShiftMove(ctx, fromPos, toPos));
        targets &= targets - 1u;
    }
}

void NineChess_AI_AB::generateCaptureMoves(SearchContext& ctx, MoveList& list) const
{
    const NineChess::Players defender = NineChess::opponentOf(ctx.board.getTurn());
    uint32_t targets = ctx.board.boardOf(defender) & ctx.board.m_validBoardMask;

    if (!ctx.board.isAllInMills(defender)) {
        uint32_t filtered = 0u;
        uint32_t bits = targets;
        while (bits != 0u) {
            const int32_t pos = CTZ32(bits);
            if (!ctx.board.isPieceInMill(pos)) {
                filtered |= NineChess::bitOf(pos);
            }
            bits &= bits - 1u;
        }
        targets = filtered;
    }

    while (targets != 0u && list.count < MoveList::MAX_COUNT) {
        const int32_t pos = CTZ32(targets);
        Move& move = list.moves[list.count++];
        move.type = MOVE_CAPTURE;
        move.from = -1;
        move.to = static_cast<int8_t>(pos);
        move.order = static_cast<int32_t>(scoreCaptureMove(ctx, pos));
        targets &= targets - 1u;
    }
}

int32_t NineChess_AI_AB::orderKey(SearchContext& ctx, const Move& move, bool isRoot,
    const Move& ttMove, int ply) const
{
    // 返回完整排序键：静态启发分（move.order）+ TT/杀手/历史加成。
    // 全部加成都在 int32 范围内（根置顶 8M + TT 4M + 杀手 3M + 历史 0.5M + 静态 < 1M）。
    // 调用方（orderMoves）保证每个走法只调用一次，结果直接写回 move.order，
    // 排序阶段退化为纯整数比较（旧实现每次比较重算两个 key，是纯浪费）。
    int32_t key = move.order;
    if (isRoot) {
        if (ctx.lastCompletedDepth > 0 && isSameMove(move, ctx.bestMove)) {
            key += 8000000;
        }
    }
    else {
        if (isSameMove(move, ttMove)) {
            key += 4000000;
        }
        else if (isSameMove(move, ctx.killers[ply][0])) {
            key += 3000000;
        }
        else if (isSameMove(move, ctx.killers[ply][1])) {
            key += 2900000;
        }
        else if (move.type != MOVE_CAPTURE) {
            key += moveHistoryBonus(ctx, move);
        }
    }
    return key;
}

int NineChess_AI_AB::moveHistoryBonus(SearchContext& ctx, const Move& move) const
{
    if (move.from < -1 || move.from >= BOARD_SIZE
        || move.to < 0 || move.to >= BOARD_SIZE) {
        return 0;
    }
    const int32_t value = ctx.history[move.from + 1][move.to];
    if (value <= 0) {
        return 0;
    }
    return std::min<int32_t>(value / 8, 500000);
}

uint64_t NineChess_AI_AB::canonicalMoveKey(const Move& move) const
{
    // 对 16 个等价视角取 (type, from, to) 的最小编码：
    // 互为对称的两步走法会得到相同的类 key，用于根节点分组排序。
    // 注意这里只做点位查表，不涉及局面哈希，开销可以忽略。
    uint64_t best = UINT64_MAX;
    for (size_t s = 0; s < m_symmetry.count(); ++s) {
        const SymmetryVariant& symmetry = m_symmetry.variant(s);
        const int32_t from = move.from >= 0
            ? symmetry.posMap[static_cast<size_t>(move.from)] : -1;
        const int32_t to = symmetry.posMap[static_cast<size_t>(move.to)];
        const uint64_t key = static_cast<uint64_t>(move.type)
            | (static_cast<uint64_t>(from + 1) << 8)
            | (static_cast<uint64_t>(to + 1) << 16);
        if (key < best) {
            best = key;
        }
    }
    return best;
}

void NineChess_AI_AB::orderMoves(SearchContext& ctx, MoveList& list, bool isRoot,
    const Move& ttMove, int ply) const
{
    const size_t count = list.count;

    if (isRoot && m_options.hashMode != HashMode::Plain) {
        // 根节点对称分组：组间按组内最高排序键，组内按排序键降序。
        // 这样互为对称的走法会相邻展开，先被搜索的成员填充置换表，
        // 同类的后续成员有机会直接命中。
        struct KeyedMove {
            Move move;
            uint64_t cls = 0u;
            int64_t key = 0;
            int64_t groupMax = 0;
        };
        std::vector<KeyedMove> items;
        items.reserve(count);
        for (size_t i = 0; i < count; ++i) {
            const Move& move = list.moves[i];
            KeyedMove item;
            item.move = move;
            item.cls = canonicalMoveKey(move);
            item.key = orderKey(ctx, move, true, ttMove, ply);
            items.push_back(item);
        }
        for (size_t i = 0; i < count; ++i) {
            int64_t best = items[i].key;
            for (size_t j = 0; j < count; ++j) {
                if (items[j].cls == items[i].cls && items[j].key > best) {
                    best = items[j].key;
                }
            }
            items[i].groupMax = best;
        }
        std::sort(items.begin(), items.end(),
            [](const KeyedMove& lhs, const KeyedMove& rhs) {
                if (lhs.groupMax != rhs.groupMax) {
                    return lhs.groupMax > rhs.groupMax;
                }
                if (lhs.cls != rhs.cls) {
                    return lhs.cls < rhs.cls;
                }
                return lhs.key > rhs.key;
            });
        for (size_t i = 0; i < count; ++i) {
            list.moves[i] = items[i].move;
        }
        return;
    }

    // 普通路径：先把完整排序键写回每个走法的 order 字段（每走法一次），
    // 再按该字段降序排序——比较退化为纯整数比较，不再重算 key。
    for (size_t i = 0; i < count; ++i) {
        Move& move = list.moves[i];
        move.order = orderKey(ctx, move, isRoot, ttMove, ply);
    }
    std::sort(list.moves.begin(), list.moves.begin() + static_cast<std::ptrdiff_t>(count),
        [](const Move& lhs, const Move& rhs) {
            return lhs.order > rhs.order;
        });
}

int NineChess_AI_AB::scorePlaceOrShiftMove(SearchContext& ctx, int32_t fromPos, int32_t toPos) const
{
    const NineChess::Players turn = ctx.board.getTurn();
    const NineChess::Players opponent = NineChess::opponentOf(turn);

    int score = 0;
    score += countClaimableMillsAfterOccupy(ctx, turn, fromPos, toPos) * 2400;
    score += countOpenMillsAfterOccupy(ctx, turn, fromPos, toPos) * 240;
    score += countBlockedThreats(ctx, opponent, toPos) * 180;
    score += countLinesThroughPos(ctx, turn, toPos) * 48;

    if (fromPos >= 0) {
        score -= static_cast<int>(ctx.board.countMillsAt(fromPos)) * 160;
    }

    return score;
}

int NineChess_AI_AB::scoreCaptureMove(SearchContext& ctx, int32_t pos) const
{
    const NineChess::Players defender = NineChess::opponentOf(ctx.board.getTurn());
    int score = 3000;
    score += countLinesThroughPos(ctx, defender, pos) * 128;
    score += static_cast<int>(ctx.board.countMillsAt(pos)) * 64;
    score += static_cast<int>(ctx.board.m_posLineCount[pos]) * 32;
    return score;
}

void NineChess_AI_AB::applyMove(SearchContext& ctx, const Move& move, Snapshot& snapshot)
{
    // 逐字段快照，不整体拷贝 ChessData：
    // 编号规则的 millHistory 是 std::vector，整体拷贝会让每个节点两次堆分配，
    // 是九连棋 NPS 低于普通规则的主要原因之一。搜索期间该表只会被
    // addNewMills() 追加，记录长度即可在回溯时完整恢复。
    NineChess::ChessData& data = ctx.board.m_data;
    snapshot.status = data.status;
    snapshot.player1Board = data.player1Board;
    snapshot.player2Board = data.player2Board;
    snapshot.forbiddenBoard = data.forbiddenBoard;
    for (int i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        snapshot.numberBoards[i] = data.numberBoards[i];
    }
    snapshot.millHistorySize = data.millHistory.size();
    snapshot.hardLayerAccum = ctx.hardLayerAccum;
    snapshot.hardHistoryAccum = ctx.hardHistoryAccum;
    snapshot.winner = ctx.board.m_winner;
    snapshot.selectedPos = ctx.board.m_selectedPos;

    switch (move.type)
    {
    case MOVE_PLACE:
        ctx.board.placeFast(move.to);
        break;
    case MOVE_SHIFT:
        if (ctx.board.getAction() == ACTION_CHOOSE) {
            ctx.board.chooseFast(move.from);
        }
        ctx.board.placeFast(move.to);
        break;
    case MOVE_CAPTURE:
        ctx.board.captureFast(move.to);
        break;
    default:
        break;
    }

    // 编号规则：按差量更新 hard 哈希累加器。
    // 层项：变化的层 XOR 掉旧项、并入新项；历史项：XOR 并入本步追加的记录。
    // lite 规则的 millHistory 恒为空、编号层恒为 0，直接跳过。
    if (!ctx.board.m_rule.allowRepeatedMills) {
        for (int i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
            const uint32_t oldLayer = snapshot.numberBoards[i];
            const uint32_t newLayer = data.numberBoards[i];
            if (oldLayer == newLayer) {
                continue;
            }
            ctx.hardLayerAccum ^= hardHashLayerTerm(oldLayer, snapshot.player1Board,
                snapshot.player2Board, static_cast<uint32_t>(i))
                ^ hardHashLayerTerm(newLayer, data.player1Board,
                    data.player2Board, static_cast<uint32_t>(i));
        }
        for (size_t k = snapshot.millHistorySize; k < data.millHistory.size(); ++k) {
            ctx.hardHistoryAccum ^= hardHashHistoryTerm(data.millHistory[k]);
        }
    }
}

void NineChess_AI_AB::undoMove(SearchContext& ctx, const Snapshot& snapshot)
{
    NineChess::ChessData& data = ctx.board.m_data;
    data.status = snapshot.status;
    data.player1Board = snapshot.player1Board;
    data.player2Board = snapshot.player2Board;
    data.forbiddenBoard = snapshot.forbiddenBoard;
    for (int i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        data.numberBoards[i] = snapshot.numberBoards[i];
    }
    // millHistory 只增不减，长度回滚即可覆盖搜索路径上的全部变化。
    if (data.millHistory.size() > snapshot.millHistorySize) {
        data.millHistory.resize(snapshot.millHistorySize);
    }
    // 增量累加器与局面快照严格同步回滚。
    ctx.hardLayerAccum = snapshot.hardLayerAccum;
    ctx.hardHistoryAccum = snapshot.hardHistoryAccum;
    ctx.board.m_winner = snapshot.winner;
    ctx.board.m_selectedPos = snapshot.selectedPos;
}

bool NineChess_AI_AB::probeTransposition(SearchContext& ctx, uint64_t hash, int depth,
    int& alpha, int& beta, int& value, Move& ttMove) const
{
    // 哈希模式说明见 makeSearchHash()：hash 已由调用方算好传入（已雪崩混合）。
    // 分片由哈希低 8 位路由，桶由第 8~18 位索引，桶内 2 槽依次探测。
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    TTStore::Shard& shard = store.shards[hash & (TTStore::SHARD_COUNT - 1u)];
    TTStore::TTBucket& bucket =
        shard.buckets[(hash >> 8) & (TTStore::BUCKET_COUNT - 1u)];
    std::lock_guard<std::mutex> lock(shard.mutex);

    ++ctx.stats.ttProbes;

    TTStore::TTSlot* slot = nullptr;
    for (TTStore::TTSlot& candidate : bucket.slotArray) {
        if (candidate.key == hash) {
            slot = &candidate;
            break;
        }
    }
    if (slot == nullptr) {
        return false;
    }

    // 解包打包字段（位布局见 TTStore 注释）。
    const uint32_t metaA = slot->metaA;
    const uint32_t metaB = slot->metaB;
    const int storedDepth = static_cast<int>(metaB & 0xffu);

    if (storedDepth < depth) {
        return false;
    }

    // 只有“深度可用”的命中才刷新代数；
    // 不可用的浅条目保持旧代数，让它们能按老化规则自然淘汰。
    slot->metaB = (metaB & ~TTStore::TT_GEN_MASK)
        | ((m_generation & 0xffffu) << TTStore::TT_GEN_SHIFT);

    ++ctx.stats.ttHits;
    value = static_cast<int16_t>(metaA & 0xffffu);

    const uint32_t moveType = (metaA >> TTStore::TT_TYPE_SHIFT) & 0x3u;
    if (moveType != MOVE_NONE) {
        ttMove.type = static_cast<MoveType>(moveType);
        const uint32_t packedFrom = (metaA >> TTStore::TT_FROM_SHIFT) & 0x1fu;
        const uint32_t packedTo = (metaA >> TTStore::TT_TO_SHIFT) & 0x1fu;
        ttMove.from = packedFrom == 0u ? -1 : static_cast<int8_t>(packedFrom - 1u);
        ttMove.to = packedTo == 0u ? -1 : static_cast<int8_t>(packedTo - 1u);
    }

    const uint32_t flag = (metaA >> TTStore::TT_FLAG_SHIFT) & 0x3u;
    if (flag == TT_EXACT) {
        // 精确值可直接返回，不需要继续展开子树。
        return true;
    }
    if (flag == TT_LOWER) {
        // 该节点真实值 >= entry.value，因此 alpha 可以直接抬高。
        alpha = std::max(alpha, value);
    }
    else {
        // 该节点真实值 <= entry.value，因此 beta 可以直接压低。
        beta = std::min(beta, value);
    }

    // 窗口被收紧到 alpha >= beta 时，说明这个节点已经足够让当前搜索剪枝。
    return alpha >= beta;
}

void NineChess_AI_AB::storeTransposition(SearchContext& ctx, uint64_t hash, int depth, int value,
    int alpha, int beta, const Move& bestMove) const
{
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    TTStore::Shard& shard = store.shards[hash & (TTStore::SHARD_COUNT - 1u)];
    TTStore::TTBucket& bucket =
        shard.buckets[(hash >> 8) & (TTStore::BUCKET_COUNT - 1u)];
    std::lock_guard<std::mutex> lock(shard.mutex);

    ++ctx.stats.ttStores;

    // 打包新条目（位布局见 TTStore 注释）：
    // 若 bestValue 没有跳出原窗口，则它是精确值；
    // 若 bestValue <= alpha，说明这是一个"最多就这么好"的上界；
    // 若 bestValue >= beta，说明这是一个"至少这么好"的下界。
    uint32_t metaA = static_cast<uint32_t>(
        static_cast<uint16_t>(clampScore(value, -INF_SCORE, INF_SCORE)));
    uint32_t flag = TT_EXACT;
    if (value <= alpha) {
        flag = TT_UPPER;
    }
    else if (value >= beta) {
        flag = TT_LOWER;
    }
    metaA |= flag << TTStore::TT_FLAG_SHIFT;

    if (bestMove.type != MOVE_NONE) {
        metaA |= static_cast<uint32_t>(bestMove.type) << TTStore::TT_TYPE_SHIFT;
        if (bestMove.from >= 0) {
            metaA |= (static_cast<uint32_t>(bestMove.from) + 1u) << TTStore::TT_FROM_SHIFT;
        }
        if (bestMove.to >= 0) {
            metaA |= (static_cast<uint32_t>(bestMove.to) + 1u) << TTStore::TT_TO_SHIFT;
        }
    }

    const uint32_t packedDepth = static_cast<uint32_t>(depth) & 0xffu;
    const uint32_t metaB = packedDepth
        | ((m_generation & 0xffffu) << TTStore::TT_GEN_SHIFT);

    // 写入策略：
    // 1. 桶内已有同键槽位：维持"深度大的覆盖，浅的只刷新代数"；
    // 2. 否则优先占用空槽；两槽都非空且都非本键时，淘汰两槽中"较劣"者
    //    （更老世代优先，其次更浅深度）。
    const uint32_t currentGeneration = m_generation & 0xffffu;
    const auto slotWorseThan = [currentGeneration](
        const TTStore::TTSlot& lhs, const TTStore::TTSlot& rhs) {
        const uint32_t genL = (lhs.metaB & TTStore::TT_GEN_MASK) >> TTStore::TT_GEN_SHIFT;
        const uint32_t genR = (rhs.metaB & TTStore::TT_GEN_MASK) >> TTStore::TT_GEN_SHIFT;
        if (genL != genR) {
            // 世代按"环回后更老"处理：与当前世代差距大者更劣。
            const uint32_t ageL = currentGeneration - genL;
            const uint32_t ageR = currentGeneration - genR;
            return ageL > ageR;
        }
        return (lhs.metaB & 0xffu) < (rhs.metaB & 0xffu);
    };

    TTStore::TTSlot* victim = nullptr;
    for (TTStore::TTSlot& slot : bucket.slotArray) {
        if (slot.key == hash) {
            if ((slot.metaB & 0xffu) <= packedDepth) {
                slot.metaA = metaA;
                slot.metaB = metaB;
            }
            else {
                // 已有条目比当前更深时仍然保留旧值，但刷新代数，
                // 表示它在当前真实局面的搜索中仍然是活跃的。
                slot.metaB = (slot.metaB & ~TTStore::TT_GEN_MASK)
                    | (currentGeneration << TTStore::TT_GEN_SHIFT);
            }
            return;
        }
        if (slot.key == 0u) {
            slot.key = hash;
            slot.metaA = metaA;
            slot.metaB = metaB;
            return;
        }
        if (victim == nullptr || slotWorseThan(slot, *victim)) {
            victim = &slot;
        }
    }
    victim->key = hash;
    victim->metaA = metaA;
    victim->metaB = metaB;
}

void NineChess_AI_AB::beginTranspositionGeneration()
{
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    std::lock_guard<std::mutex> lock(store.metaMutex);
    ++store.generation;
    if (store.generation == 0u) {
        ++store.generation;
    }
    m_generation = store.generation;
}

uint64_t NineChess_AI_AB::makeSearchHash(SearchContext& ctx) const
{
    // 出口统一做一次 mix64 雪崩：
    // lite 哈希的 48 位是按点位排列的稠密位展开（未混合），
    // 若直接用低位做分片/桶索引，结构相近的局面会挤进同一槽位互相覆盖。
    // 雪崩后任意位段都均匀，索引才能正确分散。
    uint64_t hash = 0;
    switch (m_options.hashMode)
    {
    case HashMode::Plain:
        hash = makePlainHash(ctx);
        break;
    case HashMode::OpeningCanonical:
        // 开局是唯一对称收益明显高于成本的阶段；
        // 进入中局后对称破缺，回退普通哈希换取 NPS。
        if (ctx.board.getPhase() == GAME_OPENING) {
            hash = makeCanonicalHash(ctx);
        }
        else {
            hash = makePlainHash(ctx);
        }
        break;
    case HashMode::FullCanonical:
    default:
        hash = makeCanonicalHash(ctx);
        break;
    }
    return mix64(hash);
}

uint64_t NineChess_AI_AB::makePlainHash(SearchContext& ctx) const
{
    // 与 canonical 视角完全一致的普通哈希：
    // 编号规则用 hard（含编号层与历史三连），普通规则用 lite。
    // 编号规则的层项/历史项来自增量累加器（applyMove 差量更新、undoMove 快照恢复），
    // 与全量重算逐位一致；lite 部分（status + 主位棋盘）本就只有十几次位运算，每次重算。
    uint64_t hash = 0;
    if (ctx.board.m_rule.allowRepeatedMills) {
        hash = ctx.board.m_data.getHashLite();
    }
    else {
        hash = hardHashCombine(ctx.board.m_data.getHashLite(),
            ctx.hardLayerAccum, ctx.hardHistoryAccum,
            ctx.board.m_data.millHistory.size());
    }

    int32_t selectedPos = -1;
    if (ctx.board.getPhase() == GAME_MID
        && ctx.board.getAction() == ACTION_PLACE
        && ctx.board.isValidPos(ctx.board.m_selectedPos)) {
        // 处于“已选中棋子，等待落点”的状态时，selectedPos 实际上是局面的一部分。
        selectedPos = ctx.board.m_selectedPos;
    }
    return mixSelectedPos(hash, selectedPos);
}

void NineChess_AI_AB::resetHardHashAccum(SearchContext& ctx) const
{
    // 局面被整体替换（= m_root）后全量重建累加器；
    // 平时由 applyMove/undoMove 差量维护，这里只在搜索边界调用。
    ctx.hardLayerAccum = 0;
    ctx.hardHistoryAccum = 0;
    if (ctx.board.m_rule.allowRepeatedMills) {
        return; // lite 规则不使用累加器
    }

    const NineChess::ChessData& data = ctx.board.m_data;
    for (uint32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        ctx.hardLayerAccum ^= hardHashLayerTerm(data.numberBoards[i],
            data.player1Board, data.player2Board, i);
    }
    for (const NineChess::MillKey key : data.millHistory) {
        ctx.hardHistoryAccum ^= hardHashHistoryTerm(key);
    }
}

bool NineChess_AI_AB::verifyHardHashAccum(SearchContext& ctx) const
{
    const NineChess::ChessData& data = ctx.board.m_data;
    uint64_t layerAccum = 0;
    for (uint32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        layerAccum ^= hardHashLayerTerm(data.numberBoards[i],
            data.player1Board, data.player2Board, i);
    }
    if (layerAccum != ctx.hardLayerAccum) {
        return false;
    }

    uint64_t historyAccum = 0;
    for (const NineChess::MillKey key : data.millHistory) {
        historyAccum ^= hardHashHistoryTerm(key);
    }
    return historyAccum == ctx.hardHistoryAccum;
}

uint64_t NineChess_AI_AB::makeCanonicalHash(SearchContext& ctx) const
{
    // 对每个等价变换都生成一个哈希，取最小值作为 canonical key。
    // 这样无论局面是原图、镜像图还是旋转后的图，都会落到同一个 TT 桶里。
    uint64_t bestHash = makeSymmetryHash(ctx, m_symmetry.variant(0));
    for (size_t i = 1; i < m_symmetry.count(); ++i) {
        const uint64_t current = makeSymmetryHash(ctx, m_symmetry.variant(i));
        if (current < bestHash) {
            bestHash = current;
        }
    }
    return bestHash;
}

uint64_t NineChess_AI_AB::makeSymmetryHash(SearchContext& ctx, const SymmetryVariant& symmetry) const
{
    // 先取共享工具算好的“某视角下的局面哈希”（不含 selectedPos），
    // 再把 selectedPos 按同一视角映射后混入。
    uint64_t hash = m_symmetry.viewHash(ctx.board, symmetry);

    int32_t selectedPos = -1;
    if (ctx.board.getPhase() == GAME_MID
        && ctx.board.getAction() == ACTION_PLACE
        && ctx.board.isValidPos(ctx.board.m_selectedPos)) {
        // 处于“已选中棋子，等待落点”的状态时，selectedPos 实际上是局面的一部分。
        selectedPos = symmetry.posMap[static_cast<size_t>(ctx.board.m_selectedPos)];
    }

    return mixSelectedPos(hash, selectedPos);
}

uint64_t NineChess_AI_AB::mixSelectedPos(uint64_t hash, int32_t selectedPos) const
{
    if (selectedPos < 0) {
        return hash;
    }
    return mix64(hash ^ (0x9e3779b97f4a7c15ULL + static_cast<uint64_t>(selectedPos + 1)));
}


void NineChess_AI_AB::countMillsAndOpenMills(SearchContext& ctx, NineChess::Players player,
    uint32_t occupied, int& outClaimable, int& outOpen, int& outForkPoints,
    uint32_t& outInMillMask) const
{
    // 单遍完成原先多遍线扫描：可提三连（三子成线）、活二（两子成线且第三点为空）、
    // 成三点集合（活二唯一空位，用于识别真双三连威胁）、
    // 以及处于三连中的棋子掩码（封闭三连保护项，编号规则下登记与否都算）。
    const uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    int claimable = 0;
    int openMills = 0;
    uint32_t threatMask = 0;
    uint32_t inMillMask = 0;
    for (uint32_t lineId = 0; lineId < ctx.board.m_lineCount; ++lineId) {
        const uint32_t mask = ctx.board.m_lineMasks[lineId];
        const uint32_t ownCount = POPCOUNT32(board & mask);
        if (ownCount == MILL) {
            inMillMask |= mask;
            // 编号规则：历史中已登记过的同编号三连不能再提子，不再计入。
            if (ctx.board.m_rule.allowRepeatedMills
                || !isMillKeyInHistory(ctx, player, lineId)) {
                ++claimable;
            }
        }
        else if (ownCount == 2u && POPCOUNT32(occupied & mask) == 2u) {
            ++openMills;
            // 该活二的成三点（线内唯一空位）；多个活二的成三点互不相同才是真威胁，
            // 共点的双活二会被对手一手全堵死。
            threatMask |= mask & ~occupied;
        }
    }
    outClaimable = claimable;
    outOpen = openMills;
    outForkPoints = static_cast<int>(POPCOUNT32(threatMask));
    outInMillMask = inMillMask;
}

bool NineChess_AI_AB::isMillKeyInHistory(SearchContext& ctx, NineChess::Players player,
    uint32_t lineId) const
{
    if (ctx.board.m_data.millHistory.empty()) {
        return false;
    }
    const NineChess::MillKey key = ctx.board.makeMillKeyForLine(player, lineId);
    return ctx.board.hasMillKey(key);
}

bool NineChess_AI_AB::isHypotheticalMillInHistory(SearchContext& ctx, NineChess::Players player,
    uint32_t lineId, int32_t fromPos, int32_t toPos) const
{
    int32_t numbers[MILL] = { -1, -1, -1 };
    for (int32_t i = 0; i < MILL; ++i) {
        const int32_t pos = ctx.board.m_linePos[lineId][i];
        if (pos == toPos) {
            // 假设性落子位置：走子时继承来源子的编号，开局时取下一颗新子编号。
            numbers[i] = fromPos >= 0
                ? ctx.board.getPieceNumberAtPos(fromPos)
                : nextPlacementNumber(ctx, player);
        }
        else {
            numbers[i] = ctx.board.getPieceNumberAtPos(pos);
        }
        if (numbers[i] < 0) {
            // 无法确定编号时保守视为可提，避免排序误伤。
            return false;
        }
    }

    const NineChess::MillKey key = makeMillKey(player == PLAYER2, lineId,
        static_cast<uint32_t>(numbers[0]),
        static_cast<uint32_t>(numbers[1]),
        static_cast<uint32_t>(numbers[2]));
    return ctx.board.hasMillKey(key);
}

int32_t NineChess_AI_AB::nextPlacementNumber(SearchContext& ctx, NineChess::Players player) const
{
    const uint32_t inHand = player == PLAYER1
        ? ctx.board.getPlayer1InHand()
        : ctx.board.getPlayer2InHand();
    return static_cast<int32_t>(ctx.board.m_rule.piecesPerSide) - static_cast<int32_t>(inHand);
}

int NineChess_AI_AB::countMobility(SearchContext& ctx, NineChess::Players player,
    uint32_t occupied) const
{
    if (ctx.board.getPhase() != GAME_MID) {
        return 0;
    }

    const uint32_t empty = (~occupied) & ctx.board.m_validBoardMask;

    if (ctx.board.getAction() == ACTION_CAPTURE && ctx.board.getTurn() == player) {
        return static_cast<int>(ctx.board.getPendingCaptures());
    }

    if (ctx.board.getAction() == ACTION_PLACE
        && ctx.board.getTurn() == player
        && ctx.board.isValidPos(ctx.board.m_selectedPos)) {
        if (ctx.board.canFly(player)) {
            return static_cast<int>(POPCOUNT32(empty));
        }
        return static_cast<int>(POPCOUNT32(ctx.board.m_moveMask[ctx.board.m_selectedPos] & empty));
    }

    uint32_t pieces = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    int mobility = 0;
    if (ctx.board.canFly(player)) {
        const int emptyCount = static_cast<int>(POPCOUNT32(empty));
        while (pieces != 0u) {
            ++mobility;
            pieces &= pieces - 1u;
        }
        return mobility * emptyCount;
    }

    while (pieces != 0u) {
        const int32_t pos = CTZ32(pieces);
        mobility += static_cast<int>(POPCOUNT32(ctx.board.m_moveMask[pos] & empty));
        pieces &= pieces - 1u;
    }
    return mobility;
}

int NineChess_AI_AB::countBlockedThreats(SearchContext& ctx, NineChess::Players player, int32_t pos) const
{
    if (!ctx.board.isValidPos(pos)) {
        return 0;
    }

    const uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    const uint32_t occupied =
        (ctx.board.m_data.player1Board | ctx.board.m_data.player2Board | ctx.board.m_data.forbiddenBoard)
        & ctx.board.m_validBoardMask;
    int count = 0;

    for (uint32_t index = 0; index < ctx.board.m_posLineCount[pos]; ++index) {
        const int32_t lineId = ctx.board.m_posLineIds[pos][index];
        if (lineId < 0) {
            continue;
        }

        const uint32_t mask = ctx.board.m_lineMasks[lineId];
        if (POPCOUNT32(board & mask) == 2u && POPCOUNT32(occupied & mask) == 2u) {
            ++count;
        }
    }
    return count;
}

int NineChess_AI_AB::countLinesThroughPos(SearchContext& ctx, NineChess::Players player, int32_t pos) const
{
    if (!ctx.board.isValidPos(pos)) {
        return 0;
    }

    const uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    int score = 0;
    for (uint32_t index = 0; index < ctx.board.m_posLineCount[pos]; ++index) {
        const int32_t lineId = ctx.board.m_posLineIds[pos][index];
        if (lineId < 0) {
            continue;
        }
        score += static_cast<int>(POPCOUNT32(board & ctx.board.m_lineMasks[lineId]));
    }
    return score;
}

int NineChess_AI_AB::countClaimableMillsAfterOccupy(SearchContext& ctx, NineChess::Players player,
    int32_t fromPos, int32_t toPos) const
{
    uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    if (fromPos >= 0) {
        board &= ~NineChess::bitOf(fromPos);
    }
    board |= NineChess::bitOf(toPos);

    int count = 0;
    for (uint32_t index = 0; index < ctx.board.m_posLineCount[toPos]; ++index) {
        const int32_t lineId = ctx.board.m_posLineIds[toPos][index];
        if (lineId < 0 || (board & ctx.board.m_lineMasks[lineId]) != ctx.board.m_lineMasks[lineId]) {
            continue;
        }
        // 编号规则：历史中已登记过的同编号三连不能再提子，不计入。
        if (!ctx.board.m_rule.allowRepeatedMills
            && isHypotheticalMillInHistory(ctx, player, static_cast<uint32_t>(lineId), fromPos, toPos)) {
            continue;
        }
        ++count;
    }
    return count;
}

int NineChess_AI_AB::countOpenMillsAfterOccupy(SearchContext& ctx, NineChess::Players player,
    int32_t fromPos, int32_t toPos) const
{
    uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    uint32_t occupied =
        (ctx.board.m_data.player1Board | ctx.board.m_data.player2Board | ctx.board.m_data.forbiddenBoard)
        & ctx.board.m_validBoardMask;

    if (fromPos >= 0) {
        const uint32_t fromBit = NineChess::bitOf(fromPos);
        board &= ~fromBit;
        occupied &= ~fromBit;
    }

    const uint32_t toBit = NineChess::bitOf(toPos);
    board |= toBit;
    occupied |= toBit;

    int count = 0;
    for (uint32_t index = 0; index < ctx.board.m_posLineCount[toPos]; ++index) {
        const int32_t lineId = ctx.board.m_posLineIds[toPos][index];
        if (lineId < 0) {
            continue;
        }
        const uint32_t mask = ctx.board.m_lineMasks[lineId];
        if (POPCOUNT32(board & mask) == 2u && POPCOUNT32(occupied & mask) == 2u) {
            ++count;
        }
    }
    return count;
}

int NineChess_AI_AB::countPointValue(SearchContext& ctx, uint32_t board) const
{
    if (m_options.pointValueWeight == 0) {
        return 0;
    }
    board &= ctx.board.m_validBoardMask;
    int sum = 0;
    while (board != 0u) {
        sum += m_pointValue[CTZ32(board)];
        board &= board - 1u;
    }
    return sum;
}

bool NineChess_AI_AB::isSameMove(const Move& lhs, const Move& rhs) const
{
    return lhs.type == rhs.type && lhs.from == rhs.from && lhs.to == rhs.to;
}

std::string NineChess_AI_AB::formatMove(const Move& move) const
{
    if (move.type == MOVE_CAPTURE) {
        return m_root.formatCaptureCommand(move.to);
    }
    if (move.type == MOVE_SHIFT) {
        return m_root.formatMoveCommand(move.from, move.to);
    }
    if (move.type == MOVE_PLACE) {
        return m_root.formatPointCommand(move.to);
    }
    return "error!";
}

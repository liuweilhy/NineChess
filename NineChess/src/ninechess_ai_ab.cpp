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

namespace {

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

void NineChess_AI_AB::setChess(const NineChess& chess)
{
    m_root = chess;
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
        shard.table.clear();
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
    m_rootScores.clear();
    m_timeDeadline = m_options.timeLimitMs > 0
        ? startTime + m_options.timeLimitMs : 0;
    if (depth > kMaxPly - 1) {
        depth = kMaxPly - 1;
    }
    if (depth < 0) {
        depth = 0;
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
        ctx.iterationAborted = false;

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

    const int originalAlpha = alpha;
    const int originalBeta = beta;

    int ttValue = 0;
    Move ttMove;
    // 先查置换表：
    // - 精确命中时可以直接复用；
    // - 边界命中时可以先收紧窗口，再决定是否已经足够剪枝；
    // - 命中的最佳走法用于后续走法排序。
    if (probeTransposition(ctx, depth, alpha, beta, ttValue, ttMove)) {
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
        Snapshot snapshot;
        applyMove(ctx, moves.moves[i], snapshot);
        const int value = search(ctx, depth - 1, alpha, beta, ply + 1);
        undoMove(ctx, snapshot);

        if (ctx.iterationAborted) {
            return value;
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
    storeTransposition(ctx, depth, bestValue, originalAlpha, originalBeta,
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

    const int onBoardDiff =
        static_cast<int>(ctx.board.getPlayer1OnBoardCount())
        - static_cast<int>(ctx.board.getPlayer2OnBoardCount());
    const int inHandDiff =
        static_cast<int>(ctx.board.getPlayer1InHand())
        - static_cast<int>(ctx.board.getPlayer2InHand());
    // 编号规则下，历史中已登记过的同编号三连不再视为可提三连。
    const int millDiff = countClaimableMills(ctx, PLAYER1) - countClaimableMills(ctx, PLAYER2);
    const int openMillDiff = countOpenMills(ctx, PLAYER1) - countOpenMills(ctx, PLAYER2);
    const int pointValueDiff = countPointValue(ctx, PLAYER1) - countPointValue(ctx, PLAYER2);

    int mobilityDiff = 0;
    if (ctx.board.getPhase() == GAME_MID) {
        mobilityDiff = countMobility(ctx, PLAYER1) - countMobility(ctx, PLAYER2);
    }

    int score = 0;
    if (ctx.board.getPhase() == GAME_NOTSTARTED || ctx.board.getPhase() == GAME_OPENING) {
        score += onBoardDiff * 120;
        score += inHandDiff * 48;
        score += millDiff * 96;
        score += openMillDiff * 24;
    }
    else {
        score += onBoardDiff * 180;
        score += millDiff * 112;
        score += openMillDiff * 32;
        score += mobilityDiff * 10;
    }

    // 点位结构价值：优先占据穿线更多的点位（边中点 > 角点）。
    score += pointValueDiff * m_options.pointValueWeight;

    if (ctx.board.getAction() == ACTION_CAPTURE) {
        const int captureBonus =
            (ctx.board.getPhase() == GAME_OPENING ? 160 : 220)
            * static_cast<int>(ctx.board.getPendingCaptures());
        score += ctx.board.getTurn() == PLAYER1 ? captureBonus : -captureBonus;
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
        move.order = static_cast<int16_t>(scorePlaceOrShiftMove(ctx, -1, pos));
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
        move.order = static_cast<int16_t>(scorePlaceOrShiftMove(ctx, fromPos, toPos));
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
        move.order = static_cast<int16_t>(scoreCaptureMove(ctx, pos));
        targets &= targets - 1u;
    }
}

int64_t NineChess_AI_AB::orderKey(SearchContext& ctx, const Move& move, bool isRoot,
    const Move& ttMove, int ply) const
{
    int64_t key = move.order;
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

    // 普通路径：就地排序，避免每个节点都分配临时容器。
    std::sort(list.moves.begin(), list.moves.begin() + static_cast<std::ptrdiff_t>(count),
        [this, &ctx, isRoot, &ttMove, ply](const Move& lhs, const Move& rhs) {
            return orderKey(ctx, lhs, isRoot, ttMove, ply) > orderKey(ctx, rhs, isRoot, ttMove, ply);
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
    snapshot.data = ctx.board.m_data;
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
}

void NineChess_AI_AB::undoMove(SearchContext& ctx, const Snapshot& snapshot)
{
    ctx.board.m_data = snapshot.data;
    ctx.board.m_winner = snapshot.winner;
    ctx.board.m_selectedPos = snapshot.selectedPos;
}

bool NineChess_AI_AB::probeTransposition(SearchContext& ctx, int depth, int& alpha, int& beta,
    int& value, Move& ttMove) const
{
    // 哈希模式：
    // - Plain：普通哈希，一次计算；
    // - OpeningCanonical：开局节点把 16 个等价视角压成同一个 key，
    //   中局回退到普通哈希，兼顾开局复用与中局 NPS；
    // - FullCanonical：全部节点规范化（旧行为）。
    const uint64_t hash = makeSearchHash(ctx);
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    TTStore::Shard& shard = store.shards[hash & (TTStore::SHARD_COUNT - 1u)];
    std::lock_guard<std::mutex> lock(shard.mutex);

    ++ctx.stats.ttProbes;

    const std::unordered_map<uint64_t, TTEntry>::iterator it = shard.table.find(hash);
    if (it == shard.table.end()) {
        return false;
    }

    TTEntry& entry = it->second;
    entry.generation = m_generation;

    if (entry.depth < depth) {
        return false;
    }

    ++ctx.stats.ttHits;
    value = entry.value;

    if (entry.moveType != MOVE_NONE) {
        ttMove.type = static_cast<MoveType>(entry.moveType);
        ttMove.from = entry.moveFrom;
        ttMove.to = entry.moveTo;
    }

    if (entry.flag == TT_EXACT) {
        // 精确值可直接返回，不需要继续展开子树。
        return true;
    }
    if (entry.flag == TT_LOWER) {
        // 该节点真实值 >= entry.value，因此 alpha 可以直接抬高。
        alpha = std::max(alpha, static_cast<int>(entry.value));
    }
    else {
        // 该节点真实值 <= entry.value，因此 beta 可以直接压低。
        beta = std::min(beta, static_cast<int>(entry.value));
    }

    // 窗口被收紧到 alpha >= beta 时，说明这个节点已经足够让当前搜索剪枝。
    return alpha >= beta;
}

void NineChess_AI_AB::storeTransposition(SearchContext& ctx, int depth, int value,
    int alpha, int beta, const Move& bestMove) const
{
    const uint64_t hash = makeSearchHash(ctx);
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    TTStore::Shard& shard = store.shards[hash & (TTStore::SHARD_COUNT - 1u)];
    std::lock_guard<std::mutex> lock(shard.mutex);

    const size_t shardMax = std::max<size_t>(1u,
        m_options.ttMaxEntries / TTStore::SHARD_COUNT);
    if (shard.table.size() >= shardMax) {
        pruneTranspositionStore(shard, shardMax);
    }

    TTEntry entry;
    entry.value = static_cast<int16_t>(clampScore(value, -INF_SCORE, INF_SCORE));
    entry.depth = static_cast<int16_t>(depth);
    entry.flag = TT_EXACT;
    entry.generation = m_generation;
    // 若 bestValue 没有跳出原窗口，则它是精确值；
    // 若 bestValue <= alpha，说明这是一个“最多就这么好”的上界；
    // 若 bestValue >= beta，说明这是一个“至少这么好”的下界。
    if (value <= alpha) {
        entry.flag = TT_UPPER;
    }
    else if (value >= beta) {
        entry.flag = TT_LOWER;
    }

    if (bestMove.type != MOVE_NONE) {
        entry.moveType = static_cast<uint8_t>(bestMove.type);
        entry.moveFrom = bestMove.from;
        entry.moveTo = bestMove.to;
    }

    ++ctx.stats.ttStores;

    TTEntry& slot = shard.table[hash];
    if (slot.depth <= entry.depth) {
        slot = entry;
    }
    else {
        // 已有条目比当前更深时仍然保留旧值，但刷新代数，
        // 表示它在当前真实局面的搜索中仍然是活跃的。
        slot.generation = m_generation;
    }
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

void NineChess_AI_AB::pruneTranspositionStore(TTStore::Shard& shard, size_t shardMax) const
{
    if (shard.table.size() < shardMax) {
        return;
    }

    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    uint32_t generation = 0u;
    {
        std::lock_guard<std::mutex> lock(store.metaMutex);
        generation = store.generation;
    }

    struct Candidate {
        uint64_t key = 0u;
        uint32_t age = 0u;
        int16_t depth = 0;
        uint8_t flag = TT_EXACT;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(shard.table.size());

    for (const auto& item : shard.table) {
        const TTEntry& entry = item.second;
        Candidate candidate;
        candidate.key = item.first;
        candidate.age = generation >= entry.generation
            ? (generation - entry.generation)
            : 0u;
        candidate.depth = entry.depth;
        candidate.flag = entry.flag;
        candidates.push_back(candidate);
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& lhs, const Candidate& rhs) {
            if (lhs.age != rhs.age) {
                return lhs.age > rhs.age;
            }

            const int lhsExactRank = lhs.flag == TT_EXACT ? 0 : 1;
            const int rhsExactRank = rhs.flag == TT_EXACT ? 0 : 1;
            if (lhsExactRank != rhsExactRank) {
                return lhsExactRank > rhsExactRank;
            }

            if (lhs.depth != rhs.depth) {
                return lhs.depth < rhs.depth;
            }

            return lhs.key < rhs.key;
        });

    const size_t targetSize = shardMax - (shardMax / 8u);
    const size_t removeCount =
        shard.table.size() > targetSize ? (shard.table.size() - targetSize) : 0u;

    for (size_t i = 0; i < removeCount && i < candidates.size(); ++i) {
        shard.table.erase(candidates[i].key);
    }
}

uint64_t NineChess_AI_AB::makeSearchHash(SearchContext& ctx) const
{
    switch (m_options.hashMode)
    {
    case HashMode::Plain:
        return makePlainHash(ctx);
    case HashMode::OpeningCanonical:
        // 开局是唯一对称收益明显高于成本的阶段；
        // 进入中局后对称破缺，回退普通哈希换取 NPS。
        if (ctx.board.getPhase() == GAME_OPENING) {
            return makeCanonicalHash(ctx);
        }
        return makePlainHash(ctx);
    case HashMode::FullCanonical:
    default:
        return makeCanonicalHash(ctx);
    }
}

uint64_t NineChess_AI_AB::makePlainHash(SearchContext& ctx) const
{
    // 与 canonical 视角完全一致的普通哈希：
    // 编号规则用 hard（含编号层与历史三连），普通规则用 lite。
    uint64_t hash = ctx.board.m_rule.allowRepeatedMills
        ? ctx.board.m_data.getHashLite()
        : ctx.board.m_data.getHashHard();

    int32_t selectedPos = -1;
    if (ctx.board.getPhase() == GAME_MID
        && ctx.board.getAction() == ACTION_PLACE
        && ctx.board.isValidPos(ctx.board.m_selectedPos)) {
        // 处于“已选中棋子，等待落点”的状态时，selectedPos 实际上是局面的一部分。
        selectedPos = ctx.board.m_selectedPos;
    }
    return mixSelectedPos(hash, selectedPos);
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


int NineChess_AI_AB::countClaimableMills(SearchContext& ctx, NineChess::Players player) const
{
    const uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    int count = 0;
    for (uint32_t lineId = 0; lineId < ctx.board.m_lineCount; ++lineId) {
        if ((board & ctx.board.m_lineMasks[lineId]) != ctx.board.m_lineMasks[lineId]) {
            continue;
        }
        // 编号规则：历史中已登记过的同编号三连不能再提子，不再计入。
        if (!ctx.board.m_rule.allowRepeatedMills && isMillKeyInHistory(ctx, player, lineId)) {
            continue;
        }
        ++count;
    }
    return count;
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

int NineChess_AI_AB::countOpenMills(SearchContext& ctx, NineChess::Players player) const
{
    const uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
    const uint32_t occupied =
        (ctx.board.m_data.player1Board | ctx.board.m_data.player2Board | ctx.board.m_data.forbiddenBoard)
        & ctx.board.m_validBoardMask;
    int count = 0;

    for (uint32_t lineId = 0; lineId < ctx.board.m_lineCount; ++lineId) {
        const uint32_t mask = ctx.board.m_lineMasks[lineId];
        const uint32_t ownBits = board & mask;
        if (POPCOUNT32(ownBits) == 2u && POPCOUNT32(occupied & mask) == 2u) {
            ++count;
        }
    }
    return count;
}

int NineChess_AI_AB::countMobility(SearchContext& ctx, NineChess::Players player) const
{
    if (ctx.board.getPhase() != GAME_MID) {
        return 0;
    }

    const uint32_t occupied =
        (ctx.board.m_data.player1Board | ctx.board.m_data.player2Board | ctx.board.m_data.forbiddenBoard)
        & ctx.board.m_validBoardMask;
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

int NineChess_AI_AB::countPointValue(SearchContext& ctx, NineChess::Players player) const
{
    if (m_options.pointValueWeight == 0) {
        return 0;
    }
    uint32_t board = ctx.board.boardOf(player) & ctx.board.m_validBoardMask;
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

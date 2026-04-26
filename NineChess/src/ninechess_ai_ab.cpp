/****************************************************************************
** NineChess - Alpha-Beta AI
** 以Alpha-Beta算法设计的AI下棋程序，实现等价局面和置换表功能 ***************/

#include "ninechess_ai_ab.h"

#include <algorithm>
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
    : m_requiredQuit(false)
{
    m_bestMoveText = "error!";
}

void NineChess_AI_AB::setChess(const NineChess& chess)
{
    m_root = chess;
    m_search = chess;
    m_requiredQuit.store(false);
    m_iterationAborted = false;
    m_lastCompletedDepth = 0;
    m_lastCompletedValue = 0;
    m_bestMove = Move();
    m_iterationBestMove = Move();
    m_bestMoveText = "error!";
    beginTranspositionGeneration();
    buildSymmetryVariants();
}

int NineChess_AI_AB::alphaBetaPruning(int depth)
{
    // 采用迭代加深：
    // 1. 浅层结果可以为深层排序；
    // 2. 如果外部要求中断，仍然能保留“上一层完整算完”的 best move。
    m_search = m_root;
    m_iterationAborted = false;
    m_lastCompletedDepth = 0;
    m_lastCompletedValue = evaluate(0);

    MoveList rootMoves;
    generateMoves(rootMoves);
    if (rootMoves.count == 0) {
        m_bestMove = Move();
        m_bestMoveText = "error!";
        return m_lastCompletedValue;
    }

    orderMoves(rootMoves, true);
    m_bestMove = rootMoves.moves[0];
    m_bestMoveText = formatMove(m_bestMove);

    if (depth <= 0) {
        return m_lastCompletedValue;
    }

    for (int currentDepth = 1; currentDepth <= depth; ++currentDepth) {
        if (m_requiredQuit.load()) {
            break;
        }

        m_search = m_root;
        m_iterationAborted = false;
        const int value = searchRoot(currentDepth);
        if (m_iterationAborted) {
            break;
        }

        m_lastCompletedDepth = currentDepth;
        m_lastCompletedValue = value;
        m_bestMove = m_iterationBestMove;
        m_bestMoveText = formatMove(m_bestMove);
    }

    return m_lastCompletedValue;
}

const char* NineChess_AI_AB::bestMove()
{
    return m_bestMoveText.empty() ? "error!" : m_bestMoveText.c_str();
}

int NineChess_AI_AB::searchRoot(int depth)
{
    // 根节点与普通节点的区别在于：
    // 普通节点只关心分值，根节点还需要把“哪一步走到这个分值”记下来。
    MoveList moves;
    generateMoves(moves);
    if (moves.count == 0) {
        return evaluate(0);
    }

    orderMoves(moves, true);

    const bool maximizing = m_search.getTurn() == PLAYER1;
    int alpha = -INF_SCORE;
    int beta = INF_SCORE;
    int bestValue = maximizing ? -INF_SCORE : INF_SCORE;
    m_iterationBestMove = moves.moves[0];

    for (size_t i = 0; i < moves.count; ++i) {
        if (m_requiredQuit.load()) {
            m_iterationAborted = true;
            break;
        }

        Snapshot snapshot;
        applyMove(moves.moves[i], snapshot);
        const int value = search(depth - 1, alpha, beta, 1);
        undoMove(snapshot);

        if (m_iterationAborted) {
            break;
        }

        if (maximizing) {
            if (value > bestValue || (value == bestValue && i == 0u)) {
                bestValue = value;
                m_iterationBestMove = moves.moves[i];
            }
            if (bestValue > alpha) {
                alpha = bestValue;
            }
        }
        else {
            if (value < bestValue || (value == bestValue && i == 0u)) {
                bestValue = value;
                m_iterationBestMove = moves.moves[i];
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

int NineChess_AI_AB::search(int depth, int alpha, int beta, int ply)
{
    if (m_requiredQuit.load()) {
        m_iterationAborted = true;
        return evaluate(ply);
    }

    if (m_search.getPhase() == GAME_OVER) {
        return evaluateTerminal(ply);
    }

    if (depth <= 0) {
        return evaluate(ply);
    }

    const int originalAlpha = alpha;
    const int originalBeta = beta;

    int ttValue = 0;
    // 先查置换表：
    // - 精确命中时可以直接复用；
    // - 边界命中时可以先收紧窗口，再决定是否已经足够剪枝。
    if (probeTransposition(depth, alpha, beta, ttValue)) {
        return ttValue;
    }

    MoveList moves;
    generateMoves(moves);
    if (moves.count == 0) {
        return evaluate(ply);
    }

    orderMoves(moves, false);

    const bool maximizing = m_search.getTurn() == PLAYER1;
    int bestValue = maximizing ? -INF_SCORE : INF_SCORE;

    for (size_t i = 0; i < moves.count; ++i) {
        Snapshot snapshot;
        applyMove(moves.moves[i], snapshot);
        const int value = search(depth - 1, alpha, beta, ply + 1);
        undoMove(snapshot);

        if (m_iterationAborted) {
            return value;
        }

        if (maximizing) {
            if (value > bestValue) {
                bestValue = value;
            }
            if (bestValue > alpha) {
                alpha = bestValue;
            }
        }
        else {
            if (value < bestValue) {
                bestValue = value;
            }
            if (bestValue < beta) {
                beta = bestValue;
            }
        }

        // Alpha-Beta 的核心：
        // 当前节点已经找到一个“至少不比 alpha 差 / 至多不比 beta 好”的选择时，
        // 后续兄弟节点不可能再影响祖先决策，直接停止展开。
        if (alpha >= beta) {
            break;
        }
    }

    // 用进入节点时的原始窗口来决定 bestValue 是精确值、上界还是下界。
    storeTransposition(depth, bestValue, originalAlpha, originalBeta);
    return bestValue;
}

int NineChess_AI_AB::evaluate(int ply) const
{
    if (m_search.getPhase() == GAME_OVER) {
        return evaluateTerminal(ply);
    }

    const int onBoardDiff =
        static_cast<int>(m_search.getPlayer1OnBoardCount())
        - static_cast<int>(m_search.getPlayer2OnBoardCount());
    const int inHandDiff =
        static_cast<int>(m_search.getPlayer1InHand())
        - static_cast<int>(m_search.getPlayer2InHand());
    const int millDiff = countAllMills(PLAYER1) - countAllMills(PLAYER2);
    const int openMillDiff = countOpenMills(PLAYER1) - countOpenMills(PLAYER2);

    int mobilityDiff = 0;
    if (m_search.getPhase() == GAME_MID) {
        mobilityDiff = countMobility(PLAYER1) - countMobility(PLAYER2);
    }

    int score = 0;
    if (m_search.getPhase() == GAME_NOTSTARTED || m_search.getPhase() == GAME_OPENING) {
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

    if (m_search.getAction() == ACTION_CAPTURE) {
        const int captureBonus =
            (m_search.getPhase() == GAME_OPENING ? 160 : 220)
            * static_cast<int>(m_search.getPendingCaptures());
        score += m_search.getTurn() == PLAYER1 ? captureBonus : -captureBonus;
    }

    return clampScore(score, -WIN_SCORE + ply, WIN_SCORE - ply);
}

int NineChess_AI_AB::evaluateTerminal(int ply) const
{
    if (m_search.getWinner() == PLAYER1) {
        return WIN_SCORE - ply;
    }
    if (m_search.getWinner() == PLAYER2) {
        return -WIN_SCORE + ply;
    }
    return 0;
}

void NineChess_AI_AB::generateMoves(MoveList& list) const
{
    list.count = 0;
    if (m_search.getPhase() == GAME_OVER) {
        return;
    }

    if (m_search.getAction() == ACTION_CAPTURE) {
        generateCaptureMoves(list);
        return;
    }

    if (m_search.getPhase() == GAME_NOTSTARTED || m_search.getPhase() == GAME_OPENING) {
        generateOpeningMoves(list);
        return;
    }

    if (m_search.getPhase() == GAME_MID) {
        if (m_search.getAction() == ACTION_PLACE && m_search.isValidPos(m_search.m_selectedPos)) {
            generateMovesFromSelected(list, m_search.m_selectedPos);
        }
        else {
            generateMidMoves(list);
        }
    }
}

void NineChess_AI_AB::generateOpeningMoves(MoveList& list) const
{
    const uint32_t occupied =
        (m_search.m_data.player1Board | m_search.m_data.player2Board | m_search.m_data.forbiddenBoard)
        & m_search.m_validBoardMask;
    uint32_t empty = (~occupied) & m_search.m_validBoardMask;

    while (empty != 0u && list.count < MoveList::MAX_COUNT) {
        const int32_t pos = CTZ32(empty);
        Move& move = list.moves[list.count++];
        move.type = MOVE_PLACE;
        move.from = -1;
        move.to = static_cast<int8_t>(pos);
        move.order = static_cast<int16_t>(scorePlaceOrShiftMove(-1, pos));
        empty &= empty - 1u;
    }
}

void NineChess_AI_AB::generateMidMoves(MoveList& list) const
{
    const NineChess::Players turn = m_search.getTurn();
    uint32_t pieces = m_search.boardOf(turn) & m_search.m_validBoardMask;

    while (pieces != 0u && list.count < MoveList::MAX_COUNT) {
        const int32_t fromPos = CTZ32(pieces);
        generateMovesFromSelected(list, fromPos);
        pieces &= pieces - 1u;
    }
}

void NineChess_AI_AB::generateMovesFromSelected(MoveList& list, int32_t fromPos) const
{
    if (!m_search.isValidPos(fromPos)) {
        return;
    }

    const NineChess::Players turn = m_search.getTurn();
    const uint32_t occupied =
        (m_search.m_data.player1Board | m_search.m_data.player2Board | m_search.m_data.forbiddenBoard)
        & m_search.m_validBoardMask;
    const uint32_t empty = (~occupied) & m_search.m_validBoardMask;
    uint32_t targets = 0u;

    if (m_search.canFly(turn)) {
        targets = empty;
    }
    else {
        targets = m_search.m_moveMask[fromPos] & empty & m_search.m_validBoardMask;
    }

    while (targets != 0u && list.count < MoveList::MAX_COUNT) {
        const int32_t toPos = CTZ32(targets);
        Move& move = list.moves[list.count++];
        move.type = MOVE_SHIFT;
        move.from = static_cast<int8_t>(fromPos);
        move.to = static_cast<int8_t>(toPos);
        move.order = static_cast<int16_t>(scorePlaceOrShiftMove(fromPos, toPos));
        targets &= targets - 1u;
    }
}

void NineChess_AI_AB::generateCaptureMoves(MoveList& list) const
{
    const NineChess::Players defender = NineChess::opponentOf(m_search.getTurn());
    uint32_t targets = m_search.boardOf(defender) & m_search.m_validBoardMask;

    if (!m_search.isAllInMills(defender)) {
        uint32_t filtered = 0u;
        uint32_t bits = targets;
        while (bits != 0u) {
            const int32_t pos = CTZ32(bits);
            if (!m_search.isPieceInMill(pos)) {
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
        move.order = static_cast<int16_t>(scoreCaptureMove(pos));
        targets &= targets - 1u;
    }
}

void NineChess_AI_AB::orderMoves(MoveList& list, bool isRoot) const
{
    std::sort(list.moves.begin(), list.moves.begin() + static_cast<std::ptrdiff_t>(list.count),
        [this, isRoot](const Move& lhs, const Move& rhs) {
            int lhsOrder = lhs.order;
            int rhsOrder = rhs.order;
            if (isRoot && m_lastCompletedDepth > 0) {
                if (isSameMove(lhs, m_bestMove)) {
                    lhsOrder += 20000;
                }
                if (isSameMove(rhs, m_bestMove)) {
                    rhsOrder += 20000;
                }
            }
            return lhsOrder > rhsOrder;
        });
}

int NineChess_AI_AB::scorePlaceOrShiftMove(int32_t fromPos, int32_t toPos) const
{
    const NineChess::Players turn = m_search.getTurn();
    const NineChess::Players opponent = NineChess::opponentOf(turn);

    int score = 0;
    score += countMillsAfterOccupy(turn, fromPos, toPos) * 2400;
    score += countOpenMillsAfterOccupy(turn, fromPos, toPos) * 240;
    score += countBlockedThreats(opponent, toPos) * 180;
    score += countLinesThroughPos(turn, toPos) * 48;

    if (fromPos >= 0) {
        score -= static_cast<int>(m_search.countMillsAt(fromPos)) * 160;
    }

    return score;
}

int NineChess_AI_AB::scoreCaptureMove(int32_t pos) const
{
    const NineChess::Players defender = NineChess::opponentOf(m_search.getTurn());
    int score = 3000;
    score += countLinesThroughPos(defender, pos) * 128;
    score += static_cast<int>(m_search.countMillsAt(pos)) * 64;
    score += static_cast<int>(m_search.m_posLineCount[pos]) * 32;
    return score;
}

void NineChess_AI_AB::applyMove(const Move& move, Snapshot& snapshot)
{
    snapshot.data = m_search.m_data;
    snapshot.winner = m_search.m_winner;
    snapshot.selectedPos = m_search.m_selectedPos;

    switch (move.type)
    {
    case MOVE_PLACE:
        m_search.placeFast(move.to);
        break;
    case MOVE_SHIFT:
        if (m_search.getAction() == ACTION_CHOOSE) {
            m_search.chooseFast(move.from);
        }
        m_search.placeFast(move.to);
        break;
    case MOVE_CAPTURE:
        m_search.captureFast(move.to);
        break;
    default:
        break;
    }
}

void NineChess_AI_AB::undoMove(const Snapshot& snapshot)
{
    m_search.m_data = snapshot.data;
    m_search.m_winner = snapshot.winner;
    m_search.m_selectedPos = snapshot.selectedPos;
}

bool NineChess_AI_AB::probeTransposition(int depth, int& alpha, int& beta, int& value) const
{
    // makeCanonicalHash 会把 16 个等价视角压成同一个 key，
    // 因此这里一次查表，等价于“顺带查了所有镜像 / 翻转 / 旋转局面”。
    const uint64_t hash = makeCanonicalHash();
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    std::lock_guard<std::mutex> lock(store.mutex);

    const std::unordered_map<uint64_t, TTEntry>::iterator it = store.table.find(hash);
    if (it == store.table.end()) {
        return false;
    }

    TTEntry& entry = it->second;
    entry.generation = m_generation;

    if (entry.depth < depth) {
        return false;
    }

    value = entry.value;

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

void NineChess_AI_AB::storeTransposition(int depth, int value, int alpha, int beta) const
{
    const uint64_t hash = makeCanonicalHash();
    TTStore& store = s_ttStores[m_root.getRuleIndex()];
    std::lock_guard<std::mutex> lock(store.mutex);
    const std::unordered_map<uint64_t, TTEntry>::iterator existing = store.table.find(hash);

    if (existing == store.table.end() && store.table.size() >= MAX_TT_ENTRIES) {
        pruneTranspositionStore(store);
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

    TTEntry& slot = store.table[hash];
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
    std::lock_guard<std::mutex> lock(store.mutex);
    ++store.generation;
    if (store.generation == 0u) {
        ++store.generation;
    }
    m_generation = store.generation;
}

void NineChess_AI_AB::pruneTranspositionStore(TTStore& store) const
{
    if (store.table.size() < MAX_TT_ENTRIES) {
        return;
    }

    struct Candidate {
        uint64_t key = 0u;
        uint32_t age = 0u;
        int16_t depth = 0;
        uint8_t flag = TT_EXACT;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(store.table.size());

    for (const auto& item : store.table) {
        const TTEntry& entry = item.second;
        Candidate candidate;
        candidate.key = item.first;
        candidate.age = store.generation >= entry.generation
            ? (store.generation - entry.generation)
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

    const size_t targetSize = MAX_TT_ENTRIES - (MAX_TT_ENTRIES / 8u);
    const size_t removeCount =
        store.table.size() > targetSize ? (store.table.size() - targetSize) : 0u;

    for (size_t i = 0; i < removeCount && i < candidates.size(); ++i) {
        store.table.erase(candidates[i].key);
    }
}

uint64_t NineChess_AI_AB::makeCanonicalHash() const
{
    // 对每个等价变换都生成一个哈希，取最小值作为 canonical key。
    // 这样无论局面是原图、镜像图还是旋转后的图，都会落到同一个 TT 桶里。
    uint64_t bestHash = makeSymmetryHash(m_symmetries[0]);
    for (size_t i = 1; i < m_symmetryCount; ++i) {
        const uint64_t current = makeSymmetryHash(m_symmetries[i]);
        if (current < bestHash) {
            bestHash = current;
        }
    }
    return bestHash;
}

uint64_t NineChess_AI_AB::makeSymmetryHash(const SymmetryVariant& symmetry) const
{
    // 先把当前局面搬到“某个具体对称视角”下，再调用 NineChess 自带哈希。
    // 对于普通规则，轻量哈希已足够；
    // 对于九连棋，还要带上 numberBoards + millHistory 才能区分历史相关状态。
    NineChess::ChessData data;
    data.status = m_search.m_data.status;
    data.player1Board = mapBoard(m_search.m_data.player1Board, symmetry);
    data.player2Board = mapBoard(m_search.m_data.player2Board, symmetry);
    data.forbiddenBoard = mapBoard(m_search.m_data.forbiddenBoard, symmetry);

    uint64_t hash = 0u;
    if (m_search.m_rule.allowRepeatedMills) {
        hash = data.getHashLite();
    }
    else {
        for (int32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
            data.numberBoards[i] = mapBoard(m_search.m_data.numberBoards[i], symmetry);
        }

        data.millHistory.resize(m_search.m_data.millHistory.size());
        for (size_t i = 0; i < m_search.m_data.millHistory.size(); ++i) {
            data.millHistory[i] = mapMillKey(m_search.m_data.millHistory[i], symmetry);
        }
        hash = data.getHashHard();
    }

    int32_t selectedPos = -1;
    if (m_search.getPhase() == GAME_MID
        && m_search.getAction() == ACTION_PLACE
        && m_search.isValidPos(m_search.m_selectedPos)) {
        // 处于“已选中棋子，等待落点”的状态时，selectedPos 实际上是局面的一部分。
        selectedPos = symmetry.posMap[static_cast<size_t>(m_search.m_selectedPos)];
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

void NineChess_AI_AB::buildSymmetryVariants()
{
    m_symmetryCount = 0;

    // 当前棋盘的等价变换由三类离散操作组合而成：
    // 1. 左右镜像
    // 2. 内外翻转（交换三层方框）
    // 3. 0 / 左转90 / 180 / 右转90
    // 共 2 * 2 * 4 = 16 种。
    for (int mirror = 0; mirror < 2; ++mirror) {
        for (int turn = 0; turn < 2; ++turn) {
            for (int rotate = 0; rotate < 4; ++rotate) {
                SymmetryVariant& symmetry = m_symmetries[m_symmetryCount++];
                symmetry.lineMap.fill(-1);

                // 先建点位映射：原点位 -> 变换后的点位。
                for (int32_t pos = 0; pos < BOARD_SIZE; ++pos) {
                    int32_t mapped = pos;
                    if (mirror != 0) {
                        mapped = NineChess::transformPos(mapped, NineChess::TRANSFORM_MIRROR);
                    }
                    if (turn != 0) {
                        mapped = NineChess::transformPos(mapped, NineChess::TRANSFORM_TURN);
                    }

                    switch (rotate)
                    {
                    case 1:
                        mapped = NineChess::transformPos(mapped, NineChess::TRANSFORM_ROTATE_LEFT);
                        break;
                    case 2:
                        mapped = NineChess::transformPos(mapped, NineChess::TRANSFORM_ROTATE_180);
                        break;
                    case 3:
                        mapped = NineChess::transformPos(mapped, NineChess::TRANSFORM_ROTATE_RIGHT);
                        break;
                    default:
                        break;
                    }

                    symmetry.posMap[static_cast<size_t>(pos)] = static_cast<int8_t>(mapped);
                }

                // 再建线映射：原三连线 -> 变换后对应的三连线。
                // 对九连棋还要顺便记录“原线内三个编号槽位如何重排”。
                for (uint32_t lineId = 0; lineId < m_root.m_lineCount; ++lineId) {
                    const int32_t mappedPos[MILL] = {
                        symmetry.posMap[static_cast<size_t>(m_root.m_linePos[lineId][0])],
                        symmetry.posMap[static_cast<size_t>(m_root.m_linePos[lineId][1])],
                        symmetry.posMap[static_cast<size_t>(m_root.m_linePos[lineId][2])]
                    };
                    const uint32_t mappedMask =
                        NineChess::bitOf(mappedPos[0]) | NineChess::bitOf(mappedPos[1]) | NineChess::bitOf(mappedPos[2]);

                    int32_t newLineId = -1;
                    for (uint32_t candidate = 0; candidate < m_root.m_lineCount; ++candidate) {
                        if (m_root.m_lineMasks[candidate] == mappedMask) {
                            newLineId = static_cast<int32_t>(candidate);
                            break;
                        }
                    }

                    symmetry.lineMap[lineId] = static_cast<int8_t>(newLineId);
                    if (newLineId < 0) {
                        continue;
                    }

                    for (int32_t targetIndex = 0; targetIndex < MILL; ++targetIndex) {
                        const int32_t targetPos = m_root.m_linePos[newLineId][targetIndex];
                        uint8_t sourceIndex = 0u;
                        for (int32_t candidate = 0; candidate < MILL; ++candidate) {
                            if (mappedPos[candidate] == targetPos) {
                                sourceIndex = static_cast<uint8_t>(candidate);
                                break;
                            }
                        }
                        symmetry.targetSource[lineId][targetIndex] = sourceIndex;
                    }
                }
            }
        }
    }
}

uint32_t NineChess_AI_AB::mapBoard(uint32_t board, const SymmetryVariant& symmetry) const
{
    // 位棋盘映射本质上就是把每个 1 bit 搬到变换后的新位置。
    uint32_t result = 0u;
    uint32_t bits = board & m_root.m_validBoardMask;
    while (bits != 0u) {
        const int32_t pos = CTZ32(bits);
        result |= NineChess::bitOf(symmetry.posMap[static_cast<size_t>(pos)]);
        bits &= bits - 1u;
    }
    return result;
}

NineChess::MillKey NineChess_AI_AB::mapMillKey(NineChess::MillKey key, const SymmetryVariant& symmetry) const
{
    // 九连棋的历史三连不只是“哪条线成三”，
    // 还包含“该线三个位置分别是哪几个编号的棋子”。
    // 因而对称变换后必须同时：
    // 1. 映射 lineId；
    // 2. 按新线的位置顺序重排 3 个 piece 编号。
    const uint32_t oldLineId = getMillKeyLineId(key);
    if (oldLineId >= m_root.m_lineCount) {
        return key;
    }

    const int32_t newLineId = symmetry.lineMap[oldLineId];
    if (newLineId < 0) {
        return key;
    }

    const uint32_t oldPieces[MILL] = {
        getMillKeyPiece0(key),
        getMillKeyPiece1(key),
        getMillKeyPiece2(key)
    };
    uint32_t newPieces[MILL] = {};
    for (int32_t targetIndex = 0; targetIndex < MILL; ++targetIndex) {
        newPieces[targetIndex] = oldPieces[symmetry.targetSource[oldLineId][targetIndex]];
    }

    return makeMillKey(isMillKeyPlayer2(key), static_cast<uint32_t>(newLineId),
        newPieces[0], newPieces[1], newPieces[2]);
}

int NineChess_AI_AB::countAllMills(NineChess::Players player) const
{
    const uint32_t board = m_search.boardOf(player) & m_search.m_validBoardMask;
    int count = 0;
    for (uint32_t lineId = 0; lineId < m_search.m_lineCount; ++lineId) {
        if ((board & m_search.m_lineMasks[lineId]) == m_search.m_lineMasks[lineId]) {
            ++count;
        }
    }
    return count;
}

int NineChess_AI_AB::countOpenMills(NineChess::Players player) const
{
    const uint32_t board = m_search.boardOf(player) & m_search.m_validBoardMask;
    const uint32_t occupied =
        (m_search.m_data.player1Board | m_search.m_data.player2Board | m_search.m_data.forbiddenBoard)
        & m_search.m_validBoardMask;
    int count = 0;

    for (uint32_t lineId = 0; lineId < m_search.m_lineCount; ++lineId) {
        const uint32_t mask = m_search.m_lineMasks[lineId];
        const uint32_t ownBits = board & mask;
        if (POPCOUNT32(ownBits) == 2u && POPCOUNT32(occupied & mask) == 2u) {
            ++count;
        }
    }
    return count;
}

int NineChess_AI_AB::countMobility(NineChess::Players player) const
{
    if (m_search.getPhase() != GAME_MID) {
        return 0;
    }

    const uint32_t occupied =
        (m_search.m_data.player1Board | m_search.m_data.player2Board | m_search.m_data.forbiddenBoard)
        & m_search.m_validBoardMask;
    const uint32_t empty = (~occupied) & m_search.m_validBoardMask;

    if (m_search.getAction() == ACTION_CAPTURE && m_search.getTurn() == player) {
        return static_cast<int>(m_search.getPendingCaptures());
    }

    if (m_search.getAction() == ACTION_PLACE
        && m_search.getTurn() == player
        && m_search.isValidPos(m_search.m_selectedPos)) {
        if (m_search.canFly(player)) {
            return static_cast<int>(POPCOUNT32(empty));
        }
        return static_cast<int>(POPCOUNT32(m_search.m_moveMask[m_search.m_selectedPos] & empty));
    }

    uint32_t pieces = m_search.boardOf(player) & m_search.m_validBoardMask;
    int mobility = 0;
    if (m_search.canFly(player)) {
        const int emptyCount = static_cast<int>(POPCOUNT32(empty));
        while (pieces != 0u) {
            ++mobility;
            pieces &= pieces - 1u;
        }
        return mobility * emptyCount;
    }

    while (pieces != 0u) {
        const int32_t pos = CTZ32(pieces);
        mobility += static_cast<int>(POPCOUNT32(m_search.m_moveMask[pos] & empty));
        pieces &= pieces - 1u;
    }
    return mobility;
}

int NineChess_AI_AB::countBlockedThreats(NineChess::Players player, int32_t pos) const
{
    if (!m_search.isValidPos(pos)) {
        return 0;
    }

    const uint32_t board = m_search.boardOf(player) & m_search.m_validBoardMask;
    const uint32_t occupied =
        (m_search.m_data.player1Board | m_search.m_data.player2Board | m_search.m_data.forbiddenBoard)
        & m_search.m_validBoardMask;
    int count = 0;

    for (uint32_t index = 0; index < m_search.m_posLineCount[pos]; ++index) {
        const int32_t lineId = m_search.m_posLineIds[pos][index];
        if (lineId < 0) {
            continue;
        }

        const uint32_t mask = m_search.m_lineMasks[lineId];
        if (POPCOUNT32(board & mask) == 2u && POPCOUNT32(occupied & mask) == 2u) {
            ++count;
        }
    }
    return count;
}

int NineChess_AI_AB::countLinesThroughPos(NineChess::Players player, int32_t pos) const
{
    if (!m_search.isValidPos(pos)) {
        return 0;
    }

    const uint32_t board = m_search.boardOf(player) & m_search.m_validBoardMask;
    int score = 0;
    for (uint32_t index = 0; index < m_search.m_posLineCount[pos]; ++index) {
        const int32_t lineId = m_search.m_posLineIds[pos][index];
        if (lineId < 0) {
            continue;
        }
        score += static_cast<int>(POPCOUNT32(board & m_search.m_lineMasks[lineId]));
    }
    return score;
}

int NineChess_AI_AB::countMillsAfterOccupy(NineChess::Players player, int32_t fromPos, int32_t toPos) const
{
    uint32_t board = m_search.boardOf(player) & m_search.m_validBoardMask;
    if (fromPos >= 0) {
        board &= ~NineChess::bitOf(fromPos);
    }
    board |= NineChess::bitOf(toPos);

    int count = 0;
    for (uint32_t index = 0; index < m_search.m_posLineCount[toPos]; ++index) {
        const int32_t lineId = m_search.m_posLineIds[toPos][index];
        if (lineId >= 0 && (board & m_search.m_lineMasks[lineId]) == m_search.m_lineMasks[lineId]) {
            ++count;
        }
    }
    return count;
}

int NineChess_AI_AB::countOpenMillsAfterOccupy(NineChess::Players player, int32_t fromPos, int32_t toPos) const
{
    uint32_t board = m_search.boardOf(player) & m_search.m_validBoardMask;
    uint32_t occupied =
        (m_search.m_data.player1Board | m_search.m_data.player2Board | m_search.m_data.forbiddenBoard)
        & m_search.m_validBoardMask;

    if (fromPos >= 0) {
        const uint32_t fromBit = NineChess::bitOf(fromPos);
        board &= ~fromBit;
        occupied &= ~fromBit;
    }

    const uint32_t toBit = NineChess::bitOf(toPos);
    board |= toBit;
    occupied |= toBit;

    int count = 0;
    for (uint32_t index = 0; index < m_search.m_posLineCount[toPos]; ++index) {
        const int32_t lineId = m_search.m_posLineIds[toPos][index];
        if (lineId < 0) {
            continue;
        }
        const uint32_t mask = m_search.m_lineMasks[lineId];
        if (POPCOUNT32(board & mask) == 2u && POPCOUNT32(occupied & mask) == 2u) {
            ++count;
        }
    }
    return count;
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



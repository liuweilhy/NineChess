/****************************************************************************
** NineChess - 对称变换共享工具实现
****************************************************************************/

#include "ninechess_symmetry.h"

void NineChessSymmetry::build(const NineChess& chess)
{
    m_count = 0;

    // 当前棋盘的等价变换由三类离散操作组合而成：
    // 1. 左右镜像
    // 2. 内外翻转（交换三层方框）
    // 3. 0 / 左转90 / 180 / 右转90
    // 共 2 * 2 * 4 = 16 种。
    for (int mirror = 0; mirror < 2; ++mirror) {
        for (int turn = 0; turn < 2; ++turn) {
            for (int rotate = 0; rotate < 4; ++rotate) {
                SymmetryVariant& symmetry = m_variants[m_count++];
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
                for (uint32_t lineId = 0; lineId < chess.m_lineCount; ++lineId) {
                    const int32_t mappedPos[MILL] = {
                        symmetry.posMap[static_cast<size_t>(chess.m_linePos[lineId][0])],
                        symmetry.posMap[static_cast<size_t>(chess.m_linePos[lineId][1])],
                        symmetry.posMap[static_cast<size_t>(chess.m_linePos[lineId][2])]
                    };
                    const uint32_t mappedMask =
                        NineChess::bitOf(mappedPos[0]) | NineChess::bitOf(mappedPos[1]) | NineChess::bitOf(mappedPos[2]);

                    int32_t newLineId = -1;
                    for (uint32_t candidate = 0; candidate < chess.m_lineCount; ++candidate) {
                        if (chess.m_lineMasks[candidate] == mappedMask) {
                            newLineId = static_cast<int32_t>(candidate);
                            break;
                        }
                    }

                    symmetry.lineMap[lineId] = static_cast<int8_t>(newLineId);
                    if (newLineId < 0) {
                        continue;
                    }

                    for (int32_t targetIndex = 0; targetIndex < MILL; ++targetIndex) {
                        const int32_t targetPos = chess.m_linePos[newLineId][targetIndex];
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

    // 预计算逆视角：v' 满足 v'(v(pos)) == pos（点位映射是置换）。
    for (size_t i = 0; i < m_count; ++i) {
        m_inverse[i] = 0;
        for (size_t j = 0; j < m_count; ++j) {
            bool isInverse = true;
            for (int32_t pos = 0; pos < BOARD_SIZE; ++pos) {
                const int32_t mapped = m_variants[j].posMap[
                    static_cast<size_t>(m_variants[i].posMap[static_cast<size_t>(pos)])];
                if (mapped != pos) {
                    isInverse = false;
                    break;
                }
            }
            if (isInverse) {
                m_inverse[i] = j;
                break;
            }
        }
    }
}

uint32_t NineChessSymmetry::mapBoard(uint32_t board, const SymmetryVariant& view,
    const NineChess& chess) const
{
    // 位棋盘映射本质上就是把每个 1 bit 搬到变换后的新位置。
    uint32_t result = 0u;
    uint32_t bits = board & chess.m_validBoardMask;
    while (bits != 0u) {
        const int32_t pos = CTZ32(bits);
        result |= NineChess::bitOf(view.posMap[static_cast<size_t>(pos)]);
        bits &= bits - 1u;
    }
    return result;
}

NineChess::MillKey NineChessSymmetry::mapMillKey(NineChess::MillKey key, const SymmetryVariant& view,
    const NineChess& chess) const
{
    // 九连棋的历史三连不只是“哪条线成三”，
    // 还包含“该线三个位置分别是哪几个编号的棋子”。
    // 因而对称变换后必须同时：
    // 1. 映射 lineId；
    // 2. 按新线的位置顺序重排 3 个 piece 编号。
    const uint32_t oldLineId = getMillKeyLineId(key);
    if (oldLineId >= chess.m_lineCount) {
        return key;
    }

    const int32_t newLineId = view.lineMap[oldLineId];
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
        newPieces[targetIndex] = oldPieces[view.targetSource[oldLineId][targetIndex]];
    }

    return makeMillKey(isMillKeyPlayer2(key), static_cast<uint32_t>(newLineId),
        newPieces[0], newPieces[1], newPieces[2]);
}

uint64_t NineChessSymmetry::viewHash(const NineChess& chess, const SymmetryVariant& view) const
{
    // 先把当前局面搬到“某个具体对称视角”下，再计算该视角的局面哈希。
    // 对于普通规则，轻量哈希已足够；
    // 对于九连棋，还要带上 numberBoards + millHistory 才能区分历史相关状态。
    //
    // 实现注意：旧版本为每个视角构造一个带 millHistory 的临时 ChessData，
    // 编号规则下历史表非空时每视角一次堆分配（canonical 模式每节点 16 次）。
    // 现改为全程栈上计算：
    // - 位棋盘映射是逐位的单射变换，mapBoard(a) & mapBoard(b) == mapBoard(a & b)，
    //   因此“映射后的层项”与“全量重算”逐位一致；
    // - 历史项逐条 mapMillKey 后并入 XOR 累加，与集合语义一致；
    // - lite 基哈希用栈上局部 ChessData（millHistory 保持为空，无堆分配）。
    const uint32_t mappedP1 = mapBoard(chess.m_data.player1Board, view, chess);
    const uint32_t mappedP2 = mapBoard(chess.m_data.player2Board, view, chess);
    const uint32_t mappedForbidden = mapBoard(chess.m_data.forbiddenBoard, view, chess);

    NineChess::ChessData liteData;
    liteData.status = chess.m_data.status;
    liteData.player1Board = mappedP1;
    liteData.player2Board = mappedP2;
    liteData.forbiddenBoard = mappedForbidden;

    if (chess.m_rule.allowRepeatedMills) {
        return liteData.getHashLite();
    }

    uint64_t layerAccum = 0;
    for (int32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        layerAccum ^= hardHashLayerTerm(mapBoard(chess.m_data.numberBoards[i], view, chess),
            mappedP1, mappedP2, static_cast<uint32_t>(i));
    }

    uint64_t historyAccum = 0;
    for (const NineChess::MillKey key : chess.m_data.millHistory) {
        historyAccum ^= hardHashHistoryTerm(mapMillKey(key, view, chess));
    }

    return hardHashCombine(liteData.getHashLite(), layerAccum, historyAccum,
        chess.m_data.millHistory.size());
}

uint64_t NineChessSymmetry::canonicalHash(const NineChess& chess) const
{
    // 对每个等价变换都生成一个哈希，取最小值作为 canonical key。
    // 这样无论局面是原图、镜像图还是旋转后的图，都会落到同一个桶里。
    uint64_t bestHash = viewHash(chess, m_variants[0]);
    for (size_t i = 1; i < m_count; ++i) {
        const uint64_t current = viewHash(chess, m_variants[i]);
        if (current < bestHash) {
            bestHash = current;
        }
    }
    return bestHash;
}

size_t NineChessSymmetry::canonicalViewIndex(const NineChess& chess) const
{
    // 与 canonicalHash 使用完全相同的比较规则（并列取小索引），
    // 保证“规范化坐标帧”与 key 一一对应。
    uint64_t bestHash = viewHash(chess, m_variants[0]);
    size_t bestIndex = 0;
    for (size_t i = 1; i < m_count; ++i) {
        const uint64_t current = viewHash(chess, m_variants[i]);
        if (current < bestHash) {
            bestHash = current;
            bestIndex = i;
        }
    }
    return bestIndex;
}

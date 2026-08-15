/****************************************************************************
** NineChess - 对称变换共享工具
**
** 棋盘存在 16 个等价视角（左右镜像 × 内外翻转 × 4 个离散旋转）。
** 本模块提供：
**   - 16 个等价视角的映射表构建
**   - 单一视角下的局面哈希（棋盘 / 编号层 / 历史三连按视角映射）
**   - 最小规范化哈希（AI 置换表与开局库共用）
**   - 规范化视角索引与逆视角（开局库把走法映射到实际坐标时使用）
**
** AI 与开局库共用此实现，避免两处维护同一套对称数学。
****************************************************************************/

#pragma once

#include "ninechess.h"

#include <array>
#include <cstdint>

// 某一种等价变换下的完整映射关系。
struct SymmetryVariant {
    // 原点位 -> 变换后点位。
    std::array<int8_t, BOARD_SIZE> posMap = {};

    // 原三连线编号 -> 变换后线编号。
    std::array<int8_t, 20> lineMap = {};

    // 对于某条原线，变换后目标线的第 k 个位置对应原线的哪个槽位。
    // 九连棋映射 millHistory 时需要用它来重排 3 个编号槽。
    std::array<std::array<uint8_t, MILL>, 20> targetSource = {};
};

class NineChessSymmetry
{
public:
    // 镜像、内外翻转和离散旋转组合后共有 16 种等价视角。
    static constexpr size_t SYMMETRY_COUNT = 16;

    // 按给定规则棋盘构建全部等价视角及其逆视角。
    // 棋盘结构只由规则决定（斜线有无），同一规则只需构建一次。
    void build(const NineChess& chess);

    // 当前实际构建的视角数量（恒为 16）。
    size_t count() const { return m_count; }

    // 第 index 个视角。
    const SymmetryVariant& variant(size_t index) const { return m_variants[index]; }

    // 第 index 个视角的逆视角编号（v' 满足 v'(v(pos)) == pos）。
    size_t inverseIndex(size_t index) const { return m_inverse[index]; }

    // 把一个位棋盘按给定视角映射到新视角。
    uint32_t mapBoard(uint32_t board, const SymmetryVariant& view, const NineChess& chess) const;

    // 把九连棋的历史三连 key 按给定视角映射。
    // 对称变换后必须同时映射 lineId，并按新线的位置顺序重排 3 个 piece 编号。
    NineChess::MillKey mapMillKey(NineChess::MillKey key, const SymmetryVariant& view,
        const NineChess& chess) const;

    // 单一视角下的局面哈希（不含 selectedPos）：
    // 普通规则用 lite，编号规则用 hard（含编号层与历史三连）。
    uint64_t viewHash(const NineChess& chess, const SymmetryVariant& view) const;

    // 最小规范化哈希：对全部视角取最小值（并列取小索引视角）。
    // 对称等价局面共享同一 key。
    uint64_t canonicalHash(const NineChess& chess) const;

    // 达到最小规范化哈希的视角编号（并列取小索引）。
    // 开局库用它确定“规范化坐标帧”，以便把库内走法映射回实际坐标。
    size_t canonicalViewIndex(const NineChess& chess) const;

private:
    std::array<SymmetryVariant, SYMMETRY_COUNT> m_variants = {};
    std::array<size_t, SYMMETRY_COUNT> m_inverse = {};
    size_t m_count = 0;
};

/****************************************************************************
** NineChess - 开局库（基于自对弈统计）
**
** 设计：
**   - 按规则独立：不同规则的棋盘结构（斜线/禁点/棋子数）不同，
**     开局库按规则分文件保存，评分互不通用。
**   - 对称规范化：局面 key 取 16 个等价视角的最小哈希；
**     库内走法以“规范化视角”的坐标保存，查询时用查询局面的
**     规范化视角逆变换还原，因此镜像/旋转等价的开局共享同一份统计。
**   - 统计评分：recordGame() 回放一局棋，对前 bookDepth 层每个局面
**     记录“轮到方”视角的胜负分（胜 +1 / 和 0 / 负 -1）与该走法的对局数；
**     多轮对局累积后，胜率即走法质量。
**
** 本类只做数据记录与查询，不依赖 AI；走法选择策略由控制层决定。
****************************************************************************/

#pragma once

#include "ninechess.h"
#include "ninechess_symmetry.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class NineChessOpeningBook
{
public:
    // 默认记录深度（前 10 层，覆盖 9 子规则的开局摆子阶段）。
    static constexpr int DEFAULT_BOOK_DEPTH = 10;

    // 走法可信所需的最小对局数（低于此样本量的走法不参与选择）。
    static constexpr uint32_t MIN_TRUSTED_PLAYS = 10;

    // 走法编码：bit0-1 type（1=落子 2=走子 3=提子），bit2-6 from+1，bit7-11 to+1。
    static constexpr uint16_t MOVE_TYPE_MASK = 0x0003u;
    static constexpr uint16_t MOVE_FROM_SHIFT = 2;
    static constexpr uint16_t MOVE_FROM_MASK = 0x007cu;
    static constexpr uint16_t MOVE_TO_SHIFT = 7;
    static constexpr uint16_t MOVE_TO_MASK = 0x0f80u;

    // 一个走法的对局统计。
    struct MoveStats {
        uint16_t move = 0;
        uint32_t plays = 0;
        int32_t score = 0;      // 轮到方视角累加：胜 +1 / 和 0 / 负 -1
    };

    // 一个规范化局面的全部走法统计。
    struct Entry {
        uint32_t totalPlays = 0;
        std::vector<MoveStats> moves;
    };

    // 查询结果：条目指针（null 表示不在书中）+ 查询局面的规范化视角编号。
    struct ProbeResult {
        const Entry* entry = nullptr;
        size_t viewIndex = 0;
    };

    NineChessOpeningBook() = default;

    // 切换到指定规则：重建对称表并清空当前规则数据。
    // ruleChess 只需提供棋盘结构（任意同规则局面均可）。
    void setRule(uint32_t ruleIndex, const NineChess& ruleChess);

    uint32_t ruleIndex() const { return m_ruleIndex; }

    void setBookDepth(int depth) { m_bookDepth = depth > 0 ? depth : DEFAULT_BOOK_DEPTH; }
    int bookDepth() const { return m_bookDepth; }

    void clear();
    size_t entryCount() const { return m_entries.size(); }
    uint64_t totalRecords() const;

    // 调试用：直接访问条目表（key 为规范化哈希）。
    const std::unordered_map<uint64_t, Entry>& entries() const { return m_entries; }

    // 记录一局棋：outcomeP1 为 PLAYER1 视角（+1 胜 / 0 和 / -1 负）。
    // 从空盘回放 commands，对前 bookDepth 层每个局面按“轮到方”视角累加统计。
    // commands 中无法编码的命令（认输/判和等）不记录但继续回放。
    void recordGame(const NineChess& root, const std::vector<std::string>& commands, int outcomeP1);

    // 查询局面。
    ProbeResult probe(const NineChess& chess) const;

    // 把库内走法（规范化坐标）变换到查询局面的实际坐标。
    uint16_t toActualMove(uint16_t code, size_t viewIndex) const;

    // 把实际坐标的走法变换到规范化坐标（recordGame 内部使用）。
    uint16_t toCanonicalMove(uint16_t code, size_t viewIndex) const;

    // 走法编码/解码（type: 1=落子 2=走子 3=提子；from 为 -1 表示落子/提子）。
    static uint16_t encodeMove(int type, int from, int to);
    static void decodeMove(uint16_t code, int& type, int& from, int& to);

    // 从命令文本解析走法（"(c,p)" / "(c1,p1)->(c2,p2)" / "-(c,p)"）。
    // 无法解析（认输/判和等）返回 false。
    static bool parseCommandText(const std::string& text, int& type, int& from, int& to);

    // 文本格式存取（UTF-8，CRLF 由调用方环境保证）。
    bool save(const std::string& path) const;
    bool load(const std::string& path, const NineChess& ruleChess);

private:
    uint32_t m_ruleIndex = 0;
    int m_bookDepth = DEFAULT_BOOK_DEPTH;
    NineChessSymmetry m_symmetry;
    std::unordered_map<uint64_t, Entry> m_entries;
};

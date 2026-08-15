/****************************************************************************
** NineChess - 开局库实现
****************************************************************************/

#include "ninechess_book.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace {

void skipSpaces(const char*& text)
{
    while (*text != '\0' && std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }
}

bool parsePoint(const char*& text, int32_t& c, int32_t& p)
{
    skipSpaces(text);
    if (*text != '(') {
        return false;
    }
    ++text;

    if (!std::isdigit(static_cast<unsigned char>(*text))) {
        return false;
    }
    c = 0;
    while (std::isdigit(static_cast<unsigned char>(*text))) {
        c = c * 10 + (*text - '0');
        ++text;
    }
    skipSpaces(text);
    if (*text != ',') {
        return false;
    }
    ++text;
    if (!std::isdigit(static_cast<unsigned char>(*text))) {
        return false;
    }
    p = 0;
    while (std::isdigit(static_cast<unsigned char>(*text))) {
        p = p * 10 + (*text - '0');
        ++text;
    }
    skipSpaces(text);
    if (*text != ')') {
        return false;
    }
    ++text;
    return true;
}

} // namespace

void NineChessOpeningBook::setRule(uint32_t ruleIndex, const NineChess& ruleChess)
{
    m_ruleIndex = ruleIndex;
    m_symmetry.build(ruleChess);
    clear();
}

void NineChessOpeningBook::clear()
{
    m_entries.clear();
}

uint64_t NineChessOpeningBook::totalRecords() const
{
    uint64_t total = 0;
    for (const auto& item : m_entries) {
        total += item.second.totalPlays;
    }
    return total;
}

uint16_t NineChessOpeningBook::encodeMove(int type, int from, int to)
{
    const uint16_t fromCode = static_cast<uint16_t>(from + 1) & 0x1fu;
    const uint16_t toCode = static_cast<uint16_t>(to + 1) & 0x1fu;
    return static_cast<uint16_t>((static_cast<uint16_t>(type) & MOVE_TYPE_MASK)
        | (fromCode << MOVE_FROM_SHIFT)
        | (toCode << MOVE_TO_SHIFT));
}

void NineChessOpeningBook::decodeMove(uint16_t code, int& type, int& from, int& to)
{
    type = static_cast<int>(code & MOVE_TYPE_MASK);
    from = static_cast<int>((code & MOVE_FROM_MASK) >> MOVE_FROM_SHIFT) - 1;
    to = static_cast<int>((code & MOVE_TO_MASK) >> MOVE_TO_SHIFT) - 1;
}

uint16_t NineChessOpeningBook::toCanonicalMove(uint16_t code, size_t viewIndex) const
{
    int type = 0;
    int from = -1;
    int to = -1;
    decodeMove(code, type, from, to);
    const SymmetryVariant& view = m_symmetry.variant(viewIndex);
    if (from >= 0) {
        from = view.posMap[static_cast<size_t>(from)];
    }
    to = view.posMap[static_cast<size_t>(to)];
    return encodeMove(type, from, to);
}

uint16_t NineChessOpeningBook::toActualMove(uint16_t code, size_t viewIndex) const
{
    // 逆视角：把规范化坐标的走法还原到查询局面的实际坐标。
    return toCanonicalMove(code, m_symmetry.inverseIndex(viewIndex));
}

bool NineChessOpeningBook::parseCommandText(const std::string& text, int& type, int& from, int& to)
{
    const char* cursor = text.c_str();
    skipSpaces(cursor);

    int32_t c1 = -1;
    int32_t p1 = -1;
    int32_t c2 = -1;
    int32_t p2 = -1;

    if (*cursor == '-') {
        // "-(c,p)" 提子
        ++cursor;
        if (!parsePoint(cursor, c1, p1)) {
            return false;
        }
        skipSpaces(cursor);
        if (*cursor != '\0' || c1 < 0 || c1 >= RING || p1 < 0 || p1 >= SEAT) {
            return false;
        }
        type = 3;
        from = -1;
        to = c1 * SEAT + p1;
        return true;
    }

    if (!parsePoint(cursor, c1, p1)) {
        return false;
    }
    skipSpaces(cursor);
    if (cursor[0] == '-' && cursor[1] == '>') {
        // "(c1,p1)->(c2,p2)" 走子
        cursor += 2;
        if (!parsePoint(cursor, c2, p2)) {
            return false;
        }
        skipSpaces(cursor);
        if (*cursor != '\0'
            || c1 < 0 || c1 >= RING || p1 < 0 || p1 >= SEAT
            || c2 < 0 || c2 >= RING || p2 < 0 || p2 >= SEAT) {
            return false;
        }
        type = 2;
        from = c1 * SEAT + p1;
        to = c2 * SEAT + p2;
        return true;
    }

    // "(c,p)" 落子
    skipSpaces(cursor);
    if (*cursor != '\0' || c1 < 0 || c1 >= RING || p1 < 0 || p1 >= SEAT) {
        return false;
    }
    type = 1;
    from = -1;
    to = c1 * SEAT + p1;
    return true;
}

void NineChessOpeningBook::recordGame(const NineChess& root,
    const std::vector<std::string>& commands, int outcomeP1)
{
    NineChess game;
    game.setRule(root.getRuleIndex());
    game.start();

    const size_t limit = static_cast<size_t>(m_bookDepth);
    for (size_t i = 0; i < commands.size() && i < limit; ++i) {
        const std::string& cmd = commands[i];

        int type = 0;
        int from = -1;
        int to = -1;
        if (!parseCommandText(cmd, type, from, to)) {
            if (!game.command(cmd.c_str())) {
                break;
            }
            continue;
        }

        // 记录当前局面 P 与即将执行的走法 M。
        const NineChess::Players turn = game.getTurn();
        const size_t viewIndex = m_symmetry.canonicalViewIndex(game);
        const uint64_t key = m_symmetry.canonicalHash(game);
        const uint16_t canonicalCode = toCanonicalMove(encodeMove(type, from, to), viewIndex);

        // 结果换算成“轮到方”视角。
        const int score = (outcomeP1 == 0)
            ? 0
            : (turn == NineChess::PLAYER1 ? outcomeP1 : -outcomeP1);

        Entry& entry = m_entries[key];
        ++entry.totalPlays;
        MoveStats* found = nullptr;
        for (MoveStats& moveStats : entry.moves) {
            if (moveStats.move == canonicalCode) {
                found = &moveStats;
                break;
            }
        }
        if (found == nullptr) {
            MoveStats moveStats;
            moveStats.move = canonicalCode;
            moveStats.plays = 1;
            moveStats.score = score;
            entry.moves.push_back(moveStats);
        }
        else {
            ++found->plays;
            found->score += score;
        }

        if (!game.command(cmd.c_str())) {
            break;
        }
    }
}

NineChessOpeningBook::ProbeResult NineChessOpeningBook::probe(const NineChess& chess) const
{
    ProbeResult result;
    const size_t viewIndex = m_symmetry.canonicalViewIndex(chess);
    const uint64_t key = m_symmetry.canonicalHash(chess);
    const std::unordered_map<uint64_t, Entry>::const_iterator it = m_entries.find(key);
    if (it != m_entries.end()) {
        result.entry = &it->second;
        result.viewIndex = viewIndex;
    }
    return result;
}

bool NineChessOpeningBook::save(const std::string& path) const
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    out << "NINECHESSBOOK 1\n";
    out << "rule " << m_ruleIndex << "\n";
    out << "bookDepth " << m_bookDepth << "\n";
    out << "entries " << m_entries.size() << "\n";

    for (const auto& item : m_entries) {
        const Entry& entry = item.second;
        out << "K " << std::hex << item.first << std::dec
            << " " << entry.totalPlays << "\n";
        for (const MoveStats& moveStats : entry.moves) {
            out << "M " << std::hex << moveStats.move << std::dec
                << " " << moveStats.plays << " " << moveStats.score << "\n";
        }
    }
    return out.good();
}

bool NineChessOpeningBook::load(const std::string& path, const NineChess& ruleChess)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }

    std::string header;
    int version = 0;
    if (!(in >> header >> version) || header != "NINECHESSBOOK" || version != 1) {
        return false;
    }

    std::string tag;
    uint32_t ruleIndex = 0;
    int bookDepth = DEFAULT_BOOK_DEPTH;
    size_t entryCount = 0;
    while (in >> tag) {
        if (tag == "rule") {
            in >> ruleIndex;
        }
        else if (tag == "bookDepth") {
            in >> bookDepth;
        }
        else if (tag == "entries") {
            in >> entryCount;
            break;
        }
        else {
            return false;
        }
    }

    // 规则不匹配时拒绝加载（评分不可跨规则使用）。
    if (ruleIndex != m_ruleIndex) {
        return false;
    }
    if (bookDepth > 0) {
        m_bookDepth = bookDepth;
    }
    if (ruleChess.getRuleIndex() != m_ruleIndex) {
        m_symmetry.build(ruleChess);
    }

    clear();
    m_entries.reserve(entryCount);

    // 行格式：K <keyhex> <totalPlays> 或 M <movehex> <plays> <score>。
    for (size_t i = 0; i < entryCount; ++i) {
        std::string kind;
        uint64_t key = 0;
        uint32_t totalPlays = 0;
        if (!(in >> kind >> std::hex >> key >> std::dec >> totalPlays) || kind != "K") {
            clear();
            return false;
        }
        Entry& entry = m_entries[key];
        entry.totalPlays = totalPlays;

        while (in >> kind) {
            if (kind == "K") {
                // 下一个条目开始：回退 kind 标记，由外层循环重新读取。
                in.seekg(-static_cast<std::streamoff>(kind.size()), std::ios::cur);
                break;
            }
            if (kind != "M") {
                clear();
                return false;
            }
            uint16_t moveCode = 0;
            uint32_t plays = 0;
            int32_t score = 0;
            if (!(in >> std::hex >> moveCode >> std::dec >> plays >> score)) {
                clear();
                return false;
            }
            MoveStats moveStats;
            moveStats.move = moveCode;
            moveStats.plays = plays;
            moveStats.score = score;
            entry.moves.push_back(moveStats);
        }
    }

    return in.good() || in.eof();
}

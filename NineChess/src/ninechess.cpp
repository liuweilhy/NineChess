
/****************************************************************************
** NineChess - 九连棋游戏
** 作者: liuweilhy (liuweilhy@163.com)
** 日期: 2013.01.14
**
** NineChess 核心规则类。
****************************************************************************/

#include "ninechess.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace {

struct ConsoleCoord
{
    int32_t row;
    int32_t col;
};

constexpr int32_t CONSOLE_GRID_SIZE = 13;

// 24 个棋盘点位在命令行字符网格中的固定位置。
// 命令行坐标按顺时针解释：
//   p=0 -> 上边中点
//   p=1 -> 右上点
//   p=2 -> 右边中点
//   p=3 -> 右下点
//   p=4 -> 下边中点
//   p=5 -> 左下点
//   p=6 -> 左边中点
//   p=7 -> 左上点
// 三个圈分别对应 pos 0..7、8..15、16..23。
constexpr ConsoleCoord CONSOLE_COORDS[BOARD_SIZE] = {
    { 0, 6 }, { 0, 12 }, { 6, 12 }, { 12, 12 }, { 12, 6 }, { 12, 0 }, { 6, 0 }, { 0, 0 },
    { 2, 6 }, { 2, 10 }, { 6, 10 }, { 10, 10 }, { 10, 6 }, { 10, 2 }, { 6, 2 }, { 2, 2 },
    { 4, 6 }, { 4, 8 }, { 6, 8 }, { 8, 8 }, { 8, 6 }, { 8, 4 }, { 6, 4 }, { 4, 4 }
};

const char* const PLAYER1_NUMBER_TOKENS[NUMBERED_PIECE_COUNT] = {
    "⓪", "①", "②", "③", "④", "⑤", "⑥", "⑦", "⑧"
};

const char* const PLAYER2_NUMBER_TOKENS[NUMBERED_PIECE_COUNT] = {
    "⓿", "❶", "❷", "❸", "❹", "❺", "❻", "❼", "❽"
};

void skipSpaces(const char*& text)
{
    while (*text != '\0' && std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }
}

bool parseUnsignedInt(const char*& text, int32_t& value)
{
    skipSpaces(text);
    if (!std::isdigit(static_cast<unsigned char>(*text))) {
        return false;
    }

    value = 0;
    while (std::isdigit(static_cast<unsigned char>(*text))) {
        value = value * 10 + (*text - '0');
        ++text;
    }
    return true;
}

std::string trimCopy(const char* text)
{
    if (text == nullptr) {
        return std::string();
    }

    std::string value(text);
    std::string::size_type begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }

    std::string::size_type end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

void drawConsoleEdge(std::vector<std::vector<std::string>>& grid, int32_t fromPos, int32_t toPos)
{
    const ConsoleCoord from = CONSOLE_COORDS[fromPos];
    const ConsoleCoord to = CONSOLE_COORDS[toPos];

    const int32_t stepRow = to.row == from.row ? 0 : (to.row > from.row ? 1 : -1);
    const int32_t stepCol = to.col == from.col ? 0 : (to.col > from.col ? 1 : -1);

    const char* token = "─";
    if (stepRow != 0 && stepCol == 0) {
        token = "│";
    }
    else if (stepRow != 0 && stepCol != 0) {
        token = stepRow == stepCol ? "╲" : "╱";
    }

    int32_t row = from.row + stepRow;
    int32_t col = from.col + stepCol;
    while (row != to.row || col != to.col) {
        if (grid[row][col] == " ") {
            grid[row][col] = token;
        }
        row += stepRow;
        col += stepCol;
    }
}

void trimRightSpaces(std::string& text)
{
    while (!text.empty() && text.back() == ' ') {
        text.pop_back();
    }
}

const char* phaseText(NineChess::Phases phase)
{
    switch (phase)
    {
    case GAME_OPENING:
        return "开局";
    case GAME_MID:
        return "中局";
    case GAME_OVER:
        return "结束";
    default:
        return "未开局";
    }
}

const char* actionText(NineChess::Actions action)
{
    switch (action)
    {
    case ACTION_CHOOSE:
        return "选子";
    case ACTION_PLACE:
        return "落子";
    case ACTION_CAPTURE:
        return "提子";
    default:
        return "无";
    }
}

const char* playerText(NineChess::Players player)
{
    switch (player)
    {
    case PLAYER1:
        return "先手";
    case PLAYER2:
        return "后手";
    case DRAW:
        return "平局";
    default:
        return "无";
    }
}

} // namespace

const NineChess::Rule NineChess::rules[RULE_COUNT] = {
{
    "成三棋",
    "1. 双方各9颗子，开局依次摆子；\n"
    "2. 凡出现三子相连，就提掉对手一子；\n"
    "3. 不能提对手的“三连”子，除非无子可提；\n"
    "4. 同时出现两个“三连”只能提一子；\n"
    "5. 摆完后依次走子，每次只能往相邻位置走一步；\n"
    "6. 把对手棋子提到少于3颗时胜利；\n"
    "7. 走棋阶段不能行动（被“闷”）算负。",
    9, 3, false, false, false, true, false, true, true, false
},
{
    "打三棋(12连棋)",
    "1. 双方各12颗子，棋盘有斜线；\n"
    "2. 摆棋阶段被提子的位置不能再摆子，直到走棋阶段；\n"
    "3. 摆棋阶段，摆满棋盘算先手负；\n"
    "4. 走棋阶段，后摆棋的一方先走；\n"
    "5. 一步出现几个“三连”就可以提几个子；\n"
    "6. 其它规则与成三棋基本相同。",
    12, 3, true, true, true, true, true, true, true, false
},
{
    "九连棋",
    "1. 规则与成三棋基本相同，只是它的棋子有序号，\n"
    "2. 相同序号、位置的“三连”不能重复提子；\n"
    "3. 走棋阶段不能行动（被“闷”），则由对手继续走棋；\n"
    "4. 一步出现几个“三连”就可以提几个子。",
    9, 3, false, false, false, false, true, true, false, false
},
{
    "莫里斯九子棋",
    "规则与成三棋基本相同，只是在走子阶段，当一方仅剩3子时，他可以飞子到任意空位。",
    9, 3, false, false, false, true, false, true, true, true
}
};

NineChess::NineChess()
{
    setRule(2);
}

void NineChess::setRule(uint32_t ruleIndex)
{
    m_ruleIndex = ruleIndex < RULE_COUNT ? ruleIndex : 0u;
    m_rule = rules[m_ruleIndex];
    buildMoveTable();
    buildMillTable();
    reset();
}

bool NineChess::isValidCP(int32_t c, int32_t p) const
{
    return c >= 0 && c < RING && p >= 0 && p < SEAT;
}

bool NineChess::isValidPos(int32_t pos) const
{
    return pos >= 0 && pos < BOARD_SIZE;
}

int32_t NineChess::cpToPos(int32_t c, int32_t p) const
{
    return isValidCP(c, p) ? (c * SEAT + p) : -1;
}

void NineChess::posToCP(int32_t pos, int32_t& c, int32_t& p) const
{
    if (!isValidPos(pos)) {
        c = -1;
        p = -1;
        return;
    }
    c = pos / SEAT;
    p = pos % SEAT;
}

NineChess::Players NineChess::getWhosPiece(int32_t c, int32_t p) const
{
    return getWhosPiecePos(cpToPos(c, p));
}

NineChess::Players NineChess::getWhosPiecePos(int32_t pos) const
{
    if (!isValidPos(pos)) {
        return NOBODY;
    }

    const uint32_t bit = bitOf(pos);
    if ((m_data.player1Board & bit) != 0u) {
        return PLAYER1;
    }
    if ((m_data.player2Board & bit) != 0u) {
        return PLAYER2;
    }
    if ((m_data.forbiddenBoard & bit) != 0u) {
        // DRAW表示禁止点
        return DRAW;
    }
    return NOBODY;
}

bool NineChess::getPieceCP(Players player, uint32_t number, int32_t& c, int32_t& p) const
{
    if (m_rule.allowRepeatedMills || number >= NUMBERED_PIECE_COUNT) {
        return false;
    }

    const uint32_t layer = m_data.numberBoards[number] & boardOf(player);
    if (layer == 0u) {
        return false;
    }

    posToCP(CTZ32(layer), c, p);
    return true;
}

bool NineChess::getCurrentPiece(Players& player, uint32_t& number) const
{
    if (m_rule.allowRepeatedMills || !isValidPos(m_selectedPos)) {
        return false;
    }

    player = getWhosPiecePos(m_selectedPos);
    if (player == NOBODY) {
        return false;
    }

    const int32_t pieceNumber = getPieceNumberAtPos(m_selectedPos);
    if (pieceNumber < 0) {
        return false;
    }

    number = static_cast<uint32_t>(pieceNumber);
    return true;
}

std::string NineChess::getConsoleText(bool showMillHistory) const
{
    std::vector<std::vector<std::string>> grid(
        CONSOLE_GRID_SIZE, std::vector<std::string>(CONSOLE_GRID_SIZE, " "));

    for (int32_t pos = 0; pos < BOARD_SIZE; ++pos) {
        uint32_t neighbors = m_moveMask[pos] & m_validBoardMask;
        while (neighbors != 0u) {
            const int32_t nextPos = CTZ32(neighbors);
            if (pos < nextPos) {
                drawConsoleEdge(grid, pos, nextPos);
            }
            neighbors &= neighbors - 1u;
        }
    }

    for (int32_t pos = 0; pos < BOARD_SIZE; ++pos) {
        const ConsoleCoord coord = CONSOLE_COORDS[pos];
        grid[coord.row][coord.col] = formatConsolePointToken(pos);
    }

    std::ostringstream out;
    out << "规则: " << (m_rule.name != nullptr ? m_rule.name : "") << "\n";
    for (int32_t row = 0; row < CONSOLE_GRID_SIZE; ++row) {
        std::string line;
        for (int32_t col = 0; col < CONSOLE_GRID_SIZE; ++col) {
            line += grid[row][col];
        }
        trimRightSpaces(line);
        out << line << "\n";
    }

    out << "阶段: " << phaseText(getPhase())
        << "  动作: " << actionText(getAction())
        << "  当前轮次: " << playerText(getTurn()) << "\n";
    out << "手牌: 先手=" << getPlayer1InHand()
        << "  后手=" << getPlayer2InHand() << "\n";
    out << "在盘: 先手=" << getPlayer1OnBoardCount()
        << "  后手=" << getPlayer2OnBoardCount() << "\n";
    out << "待提子数: " << getPendingCaptures()
        << "  当前胜者: " << playerText(getWinner()) << "\n";

    if (isValidPos(m_selectedPos)) {
        out << "当前选择: " << formatPointCommand(m_selectedPos);

        Players selectedPlayer = NOBODY;
        uint32_t selectedNumber = 0u;
        if (getCurrentPiece(selectedPlayer, selectedNumber)) {
            out << " [" << playerText(selectedPlayer) << "#" << selectedNumber << "]";
        }
        out << "\n";
    }
    else {
        out << "当前选择: 无\n";
    }

    out << "提示: " << m_tip << "\n";
    out << "最后命令: " << (m_cmdline.empty() ? "无" : m_cmdline) << "\n";

    if (!m_rule.allowRepeatedMills && showMillHistory) {
        out << "历史三连 (" << m_data.millHistory.size() << "):\n";
        if (m_data.millHistory.empty()) {
            out << "  无\n";
        }
        else {
            for (size_t i = 0; i < m_data.millHistory.size(); ++i) {
                out << "  " << (i + 1u) << ". "
                    << formatMillHistoryEntry(m_data.millHistory[i]) << "\n";
            }
        }
    }

    return out.str();
}

bool NineChess::canChoosePos(int32_t pos) const
{
    if (!isValidPos(pos) || m_data.getPhase() != GAME_MID) {
        return false;
    }
    if (m_data.getAction() != ACTION_CHOOSE && m_data.getAction() != ACTION_PLACE) {
        return false;
    }
    if (!isOwnPieceAt(m_data.getTurn(), pos)) {
        return false;
    }

    if (canFly(m_data.getTurn())) {
        const uint32_t occupied = (m_data.player1Board | m_data.player2Board | m_data.forbiddenBoard) & m_validBoardMask;
        return occupied != m_validBoardMask;
    }

    const uint32_t occupied = (m_data.player1Board | m_data.player2Board | m_data.forbiddenBoard) & m_validBoardMask;
    return (m_moveMask[pos] & ~occupied & m_validBoardMask) != 0u;
}

bool NineChess::canPlacePos(int32_t pos) const
{
    if (!isValidPos(pos) || m_data.getPhase() == GAME_OVER) {
        return false;
    }

    if (m_data.getPhase() == GAME_NOTSTARTED) {
        return isEmptyPos(pos);
    }

    if (m_data.getAction() != ACTION_PLACE || !isEmptyPos(pos)) {
        return false;
    }

    if (m_data.getPhase() == GAME_OPENING) {
        return true;
    }

    return isValidPos(m_selectedPos)
        && (canFly(m_data.getTurn()) || isAdjacent(m_selectedPos, pos));
}

bool NineChess::canCapturePos(int32_t pos) const
{
    if (!isValidPos(pos) || m_data.getPhase() == GAME_NOTSTARTED || m_data.getPhase() == GAME_OVER) {
        return false;
    }
    if (m_data.getAction() != ACTION_CAPTURE || m_data.getPendingCaptures() == 0u) {
        return false;
    }

    const Players attacker = m_data.getTurn();
    const Players defender = opponentOf(attacker);
    return isOpponentPieceAt(attacker, pos)
        && (isAllInMills(defender) || !isPieceInMill(pos));
}

void NineChess::reset()
{
    m_data = ChessData();
    m_data.setState(GAME_NOTSTARTED, ACTION_NONE, NOBODY);
    m_data.setPlayer2InHand(m_rule.piecesPerSide);
    m_winner = NOBODY;
    m_selectedPos = -1;
    m_cmdline.clear();
    m_cmdHistory.clear();
    m_tip = "未开局";
}

void NineChess::start()
{
    if (m_data.getPhase() == GAME_OVER) {
        reset();
    }
    if (m_data.getPhase() == GAME_NOTSTARTED) {
        enterOpening();
        rebuildTip();
    }
}

void NineChess::refreshTip()
{
    if (m_data.getPhase() != GAME_OVER) {
        m_tip.clear();
    }
    rebuildTip();
}

bool NineChess::choose(int32_t c, int32_t p)
{
    return choosePos(cpToPos(c, p));
}

bool NineChess::place(int32_t c, int32_t p)
{
    return placePos(cpToPos(c, p));
}

bool NineChess::capture(int32_t c, int32_t p)
{
    return capturePos(cpToPos(c, p));
}

bool NineChess::choosePos(int32_t pos)
{
    return doChoose(pos, true, true);
}

bool NineChess::placePos(int32_t pos)
{
    return doPlace(pos, true, true);
}

bool NineChess::capturePos(int32_t pos)
{
    return doCapture(pos, true, true);
}

void NineChess::chooseFast(int32_t pos)
{
    (void)doChoose(pos, false, false);
}

void NineChess::placeFast(int32_t pos)
{
    (void)doPlace(pos, false, false);
}

void NineChess::captureFast(int32_t pos)
{
    (void)doCapture(pos, false, false);
}

bool NineChess::giveup()
{
    return giveup(m_data.getTurn());
}

bool NineChess::giveup(Players loser)
{
    if ((loser != PLAYER1 && loser != PLAYER2)
        || (m_data.getPhase() != GAME_OPENING && m_data.getPhase() != GAME_MID)) {
        return false;
    }

    const std::string command = formatGiveupCommand(loser);
    return adjudicateResult(opponentOf(loser), loser == PLAYER1
        ? "玩家1认负，恭喜玩家2获胜！"
        : "玩家2认负，恭喜玩家1获胜！", &command);
}

bool NineChess::adjudicateWin(Players winner, const std::string& tipText)
{
    if (winner != PLAYER1 && winner != PLAYER2) {
        return false;
    }

    return adjudicateResult(winner, tipText.empty()
        ? (winner == PLAYER1 ? "恭喜玩家1获胜！" : "恭喜玩家2获胜！")
        : tipText, nullptr);
}

bool NineChess::adjudicateDraw(const std::string& tipText)
{
    return adjudicateResult(DRAW, tipText.empty() ? "平局。" : tipText, nullptr);
}

bool NineChess::adjudicateResult(Players winner, const std::string& tipText,
    const std::string* recordedCommand)
{
    if (winner != PLAYER1 && winner != PLAYER2 && winner != DRAW) {
        return false;
    }
    if (m_data.getPhase() != GAME_OPENING && m_data.getPhase() != GAME_MID) {
        return false;
    }

    if (recordedCommand != nullptr) {
        setLastCommand(*recordedCommand, true);
    }
    setGameOver(winner, tipText);
    return true;
}

bool NineChess::command(const char* cmd)
{
    const std::string trimmed = trimCopy(cmd);
    if (trimmed.empty()) {
        return false;
    }

    Players loser = NOBODY;
    if (parseGiveupCommand(trimmed.c_str(), loser)) {
        return giveup(loser);
    }
    if (parseDrawCommand(trimmed.c_str())) {
        const std::string command = formatDrawCommand();
        return adjudicateResult(DRAW, "平局。", &command);
    }

    std::string lower = trimmed;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (lower == "giveup" || lower == "resign") {
        return giveup();
    }
    if (lower == "draw") {
        const std::string command = formatDrawCommand();
        return adjudicateResult(DRAW, "平局。", &command);
    }

    int32_t fromPos = -1;
    int32_t toPos = -1;
    if (parseMoveCommand(trimmed.c_str(), fromPos, toPos)) {
        // 复合走子命令需要具备“原子性”：
        // 1. 如果前半段 choose 成功、后半段 place 失败，棋局不能停留在“已选中某子”的半完成状态；
        // 2. 否则命令调用方会看到“返回 false，但局面已被部分修改”，
        //    这会让控制台 undo、批量测试和上层脚本都很难处理。
        //
        // 因而这里先保存一份完整快照；只有两步都成功时才提交结果。
        const NineChess snapshot(*this);
        if (!choosePos(fromPos) || !placePos(toPos)) {
            *this = snapshot;
            return false;
        }
        return true;
    }

    int32_t pos = -1;
    if (parseCaptureCommand(trimmed.c_str(), pos)) {
        return capturePos(pos);
    }
    if (!parseSingleCommand(trimmed.c_str(), pos)) {
        return false;
    }

    if (m_data.getAction() == ACTION_CAPTURE) {
        return capturePos(pos);
    }
    if (m_data.getPhase() == GAME_MID && m_data.getAction() == ACTION_CHOOSE) {
        return choosePos(pos);
    }
    return placePos(pos);
}

uint64_t NineChess::getHash() const
{
    return m_rule.allowRepeatedMills ? m_data.getHashLite() : m_data.getHashHard();
}

void NineChess::mirror(bool rewriteCommands)
{
    transformState(TRANSFORM_MIRROR, rewriteCommands);
    rebuildTip();
}

void NineChess::turn(bool rewriteCommands)
{
    transformState(TRANSFORM_TURN, rewriteCommands);
    rebuildTip();
}

void NineChess::flipVertical(bool rewriteCommands)
{
    // 几何上，“上下翻转”与“左右镜像 + 180 度旋转”等价。
    // 这里直接走一个独立的 TransformMode：
    // 1. 避免重复遍历位棋盘、历史三连和命令历史；
    // 2. 也避免组合调用时需要额外考虑 rewriteCommands 的传递方式。
    transformState(TRANSFORM_FLIP_VERTICAL, rewriteCommands);
    rebuildTip();
}

void NineChess::rotate(Rotates rotate, bool rewriteCommands)
{
    switch (rotate)
    {
    case ROTATE_LEFT:
        transformState(TRANSFORM_ROTATE_LEFT, rewriteCommands);
        break;
    case ROTATE_RIGHT:
        transformState(TRANSFORM_ROTATE_RIGHT, rewriteCommands);
        break;
    case ROTATE_180:
        transformState(TRANSFORM_ROTATE_180, rewriteCommands);
        break;
    default:
        return;
    }

    rebuildTip();
}

void NineChess::rotate(int32_t degrees, bool rewriteCommands)
{
    switch (degrees)
    {
    case 90:
        rotate(ROTATE_LEFT, rewriteCommands);
        break;
    case -90:
        rotate(ROTATE_RIGHT, rewriteCommands);
        break;
    case 180:
    case -180:
        rotate(ROTATE_180, rewriteCommands);
        break;
    default:
        break;
    }
}

void NineChess::buildMoveTable()
{
    for (int32_t pos = 0; pos < BOARD_SIZE; ++pos) {
        const int32_t ring = pos / SEAT;
        const int32_t seat = pos % SEAT;
        uint32_t mask = 0u;

        mask |= bitOf(ring * SEAT + ((seat + SEAT - 1) % SEAT));
        mask |= bitOf(ring * SEAT + ((seat + 1) % SEAT));

        // 默认规则下，跨圈直线位于 0/2/4/6 四个中点；
        // 带斜线规则下，8 个点都允许沿同编号跨圈连接。
        if ((seat & 1) == 0 || m_rule.hasDiagonalLines) {
            if (ring > 0) {
                mask |= bitOf((ring - 1) * SEAT + seat);
            }
            if (ring + 1 < RING) {
                mask |= bitOf((ring + 1) * SEAT + seat);
            }
        }

        m_moveMask[pos] = mask;
    }
}

void NineChess::buildMillTable()
{
    m_lineCount = 0u;
    std::fill(m_lineMasks, m_lineMasks + MAX_MILL_LINE_COUNT, 0u);
    std::fill(m_posLineCount, m_posLineCount + BOARD_SIZE, 0u);
    for (uint32_t i = 0; i < MAX_MILL_LINE_COUNT; ++i) {
        for (int32_t j = 0; j < MILL; ++j) {
            m_linePos[i][j] = -1;
        }
    }
    for (int32_t pos = 0; pos < BOARD_SIZE; ++pos) {
        for (uint32_t i = 0; i < MAX_LINES_PER_POS; ++i) {
            m_posLineIds[pos][i] = -1;
        }
    }

    for (int32_t ring = 0; ring < RING; ++ring) {
        const int32_t base = ring * SEAT;
        addMillLine(base + 7, base + 0, base + 1);
        addMillLine(base + 1, base + 2, base + 3);
        addMillLine(base + 3, base + 4, base + 5);
        addMillLine(base + 5, base + 6, base + 7);
    }

    for (int32_t seat = 0; seat < SEAT; ++seat) {
        // 默认只在四个边中点生成跨圈“三连”；
        // 有斜线的规则再额外加入四个角点方向。
        if ((seat & 1) == 0 || m_rule.hasDiagonalLines) {
            addMillLine(seat, seat + SEAT, seat + SEAT * 2);
        }
    }
}

void NineChess::addMillLine(int32_t pos0, int32_t pos1, int32_t pos2)
{
    if (m_lineCount >= MAX_MILL_LINE_COUNT) {
        return;
    }

    const uint32_t lineId = m_lineCount++;
    m_linePos[lineId][0] = static_cast<int8_t>(pos0);
    m_linePos[lineId][1] = static_cast<int8_t>(pos1);
    m_linePos[lineId][2] = static_cast<int8_t>(pos2);
    m_lineMasks[lineId] = bitOf(pos0) | bitOf(pos1) | bitOf(pos2);

    const int32_t positions[MILL] = { pos0, pos1, pos2 };
    for (int32_t i = 0; i < MILL; ++i) {
        const int32_t pos = positions[i];
        const uint8_t count = m_posLineCount[pos];
        if (count < MAX_LINES_PER_POS) {
            m_posLineIds[pos][count] = static_cast<int8_t>(lineId);
            m_posLineCount[pos] = static_cast<uint8_t>(count + 1u);
        }
    }
}

NineChess::Players NineChess::opponentOf(Players player)
{
    if (player == PLAYER1) {
        return PLAYER2;
    }
    if (player == PLAYER2) {
        return PLAYER1;
    }
    return NOBODY;
}

uint32_t NineChess::boardOf(Players player) const
{
    if (player == PLAYER1) {
        return m_data.player1Board;
    }
    if (player == PLAYER2) {
        return m_data.player2Board;
    }
    return 0u;
}

uint32_t& NineChess::boardRef(Players player)
{
    return player == PLAYER1 ? m_data.player1Board : m_data.player2Board;
}

bool NineChess::isOccupiedPos(int32_t pos) const
{
    return isValidPos(pos) && (((m_data.player1Board | m_data.player2Board) & bitOf(pos)) != 0u);
}

bool NineChess::isForbiddenPos(int32_t pos) const
{
    return isValidPos(pos) && ((m_data.forbiddenBoard & bitOf(pos)) != 0u);
}

bool NineChess::isEmptyPos(int32_t pos) const
{
    return isValidPos(pos) && !isOccupiedPos(pos) && !isForbiddenPos(pos);
}

bool NineChess::canFly(Players player) const
{
    if (!m_rule.allowFlying || m_data.getPhase() != GAME_MID) {
        return false;
    }

    return (player == PLAYER1 ? m_data.getPlayer1OnBoardCount() : m_data.getPlayer2OnBoardCount())
        <= m_rule.minPiecesToSurvive;
}

bool NineChess::isAdjacent(int32_t fromPos, int32_t toPos) const
{
    return isValidPos(fromPos)
        && isValidPos(toPos)
        && ((m_moveMask[fromPos] & bitOf(toPos)) != 0u);
}

bool NineChess::isOwnPieceAt(Players player, int32_t pos) const
{
    return isValidPos(pos) && ((boardOf(player) & bitOf(pos)) != 0u);
}

bool NineChess::isOpponentPieceAt(Players player, int32_t pos) const
{
    return isOwnPieceAt(opponentOf(player), pos);
}

bool NineChess::doChoose(int32_t pos, bool validate, bool updateView)
{
    if (validate && !canChoosePos(pos)) {
        return false;
    }

    m_selectedPos = pos;
    m_data.setAction(ACTION_PLACE);
    if (updateView) {
        setLastCommand(formatPointCommand(pos), false);
        rebuildTip();
    }
    return true;
}

bool NineChess::doPlace(int32_t pos, bool validate, bool updateView)
{
    if (validate && !canPlacePos(pos)) {
        return false;
    }

    if (m_data.getPhase() == GAME_NOTSTARTED) {
        enterOpening();
    }

    if (m_data.getPhase() == GAME_OPENING) {
        applyOpeningPlacement(pos);
        if (updateView) {
            setLastCommand(formatPointCommand(pos), true);
        }
    }
    else {
        const int32_t fromPos = m_selectedPos;
        applyMidMove(pos);
        if (updateView) {
            setLastCommand(formatMoveCommand(fromPos, pos), true);
        }
    }

    (void)tryFinishAfterPlaceOrMove(pos);
    if (updateView) {
        rebuildTip();
    }
    return true;
}

bool NineChess::doCapture(int32_t pos, bool validate, bool updateView)
{
    if (validate && !canCapturePos(pos)) {
        return false;
    }

    if (updateView) {
        setLastCommand(formatCaptureCommand(pos), true);
    }

    applyCapture(pos);
    (void)tryFinishAfterCapture();
    if (updateView) {
        rebuildTip();
    }
    return true;
}

void NineChess::applyOpeningPlacement(int32_t pos)
{
    const Players turn = m_data.getTurn();
    boardRef(turn) |= bitOf(pos);
    if (!m_rule.allowRepeatedMills) {
        placeNumberedPiece(pos);
    }
    if (turn == PLAYER2) {
        m_data.decPlayer2InHand();
    }
    m_selectedPos = pos;
}

void NineChess::applyMidMove(int32_t toPos)
{
    const Players turn = m_data.getTurn();
    const uint32_t fromBit = bitOf(m_selectedPos);
    const uint32_t toBit = bitOf(toPos);
    uint32_t& board = boardRef(turn);

    board &= ~fromBit;
    board |= toBit;
    if (!m_rule.allowRepeatedMills) {
        moveNumberedPiece(m_selectedPos, toPos);
    }
    m_selectedPos = toPos;
}

void NineChess::applyCapture(int32_t pos)
{
    const Players victim = opponentOf(m_data.getTurn());
    const uint32_t bit = bitOf(pos);

    boardRef(victim) &= ~bit;
    if (m_rule.hasForbiddenPoints && m_data.getPhase() == GAME_OPENING) {
        m_data.forbiddenBoard |= bit;
    }
    else {
        m_data.forbiddenBoard &= ~bit;
    }

    if (!m_rule.allowRepeatedMills) {
        removeNumberedPiece(pos);
    }

    m_data.setPendingCaptures(m_data.getPendingCaptures() - 1u);
    m_selectedPos = -1;
}

bool NineChess::tryFinishAfterPlaceOrMove(int32_t pos)
{
    const uint32_t newMills = addNewMills(pos);
    if (newMills > 0u) {
        // 开局最后一手如果同时出现“摆满棋盘”和“三连”，按当前约定应先执行提子。
        // 因此开局阶段的“满盘判负/判和”必须放到新三连判断之后，
        // 否则会把本应进入 ACTION_CAPTURE 的局面提前终结。
        m_data.setPendingCaptures(m_rule.allowMultiCapture ? newMills : 1u);
        m_data.setAction(ACTION_CAPTURE);
        return false;
    }

    if (m_data.getPhase() == GAME_OPENING) {
        // 开局普通落子完成后，要先把 turn/action 切到“命令结束后的稳定状态”。
        // 否则当后手刚落完一子、但尚未 toggleTurn 时，
        // getPlayer1InHand() 会把这个瞬时 ACTION_PLACE|PLAYER2 误判成
        // “先手刚好多摆了一手”的常态，从而把先手手牌少算 1。
        toggleTurn();
        m_data.setAction(ACTION_PLACE);

        if (trySetWinnerFromMaterialOrBoard()) {
            return true;
        }

        if (m_data.getPlayer1InHand() == 0u && m_data.getPlayer2InHand() == 0u) {
            enterMidgame();
            if (trySetWinnerFromMaterialOrBoard()) {
                return true;
            }
            return tryHandleBlockedTurn();
        }
        return false;
    }

    m_selectedPos = -1;
    toggleTurn();
    m_data.setAction(ACTION_CHOOSE);
    if (trySetWinnerFromMaterialOrBoard()) {
        return true;
    }
    return tryHandleBlockedTurn();
}

bool NineChess::tryFinishAfterCapture()
{
    if (trySetWinnerFromMaterialOrBoard()) {
        return true;
    }

    if (m_data.getPendingCaptures() > 0u) {
        return false;
    }

    if (m_data.getPhase() == GAME_OPENING) {
        if (m_data.getPlayer1InHand() == 0u && m_data.getPlayer2InHand() == 0u) {
            enterMidgame();
            if (trySetWinnerFromMaterialOrBoard()) {
                return true;
            }
            return tryHandleBlockedTurn();
        }

        toggleTurn();
        m_data.setAction(ACTION_PLACE);
        return false;
    }

    toggleTurn();
    m_data.setAction(ACTION_CHOOSE);
    if (trySetWinnerFromMaterialOrBoard()) {
        return true;
    }
    return tryHandleBlockedTurn();
}

bool NineChess::trySetWinnerFromMaterialOrBoard()
{
    if (m_data.getPhase() == GAME_OVER) {
        return true;
    }

    const uint32_t total1 = m_data.getPlayer1OnBoardCount() + m_data.getPlayer1InHand();
    const uint32_t total2 = m_data.getPlayer2OnBoardCount() + m_data.getPlayer2InHand();
    if (total1 < m_rule.minPiecesToSurvive) {
        setGameOver(PLAYER2, "恭喜玩家2获胜！");
        return true;
    }
    if (total2 < m_rule.minPiecesToSurvive) {
        setGameOver(PLAYER1, "恭喜玩家1获胜！");
        return true;
    }

    if (m_data.getPhase() == GAME_OPENING) {
        const uint32_t onBoardTotal = m_data.getPlayer1OnBoardCount() + m_data.getPlayer2OnBoardCount();
        if (onBoardTotal >= BOARD_SIZE) {
            if (m_rule.fullBoardIsLoss) {
                setGameOver(PLAYER2, "摆满棋盘，恭喜玩家2获胜！");
            }
            else {
                setGameOver(DRAW, "摆满棋盘，双方平局。");
            }
            return true;
        }
    }

    return false;
}

bool NineChess::tryHandleBlockedTurn()
{
    if (m_data.getPhase() != GAME_MID || m_data.getAction() != ACTION_CHOOSE) {
        return false;
    }

    if (hasAnyLegalMove(m_data.getTurn())) {
        return false;
    }

    if (m_rule.blockedIsLoss) {
        const Players loser = m_data.getTurn();
        setGameOver(opponentOf(loser), loser == PLAYER1
            ? "玩家1无子可走，恭喜玩家2获胜！"
            : "玩家2无子可走，恭喜玩家1获胜！");
        return true;
    }

    toggleTurn();
    if (!hasAnyLegalMove(m_data.getTurn())) {
        setGameOver(DRAW, "双方均无子可走，平局。");
        return true;
    }
    return false;
}

uint32_t NineChess::countMillsAt(int32_t pos) const
{
    const Players owner = getWhosPiecePos(pos);
    if (owner != PLAYER1 && owner != PLAYER2) {
        return 0u;
    }

    const uint32_t board = boardOf(owner);
    uint32_t count = 0u;
    for (uint32_t i = 0; i < m_posLineCount[pos]; ++i) {
        const int32_t lineId = m_posLineIds[pos][i];
        if (lineId >= 0 && (board & m_lineMasks[lineId]) == m_lineMasks[lineId]) {
            ++count;
        }
    }
    return count;
}

bool NineChess::isPieceInMill(int32_t pos) const
{
    return countMillsAt(pos) > 0u;
}

bool NineChess::isAllInMills(Players player) const
{
    uint32_t board = boardOf(player) & m_validBoardMask;
    while (board != 0u) {
        const int32_t pos = CTZ32(board);
        if (!isPieceInMill(pos)) {
            return false;
        }
        board &= board - 1u;
    }
    return true;
}

bool NineChess::hasAnyLegalMove(Players player) const
{
    uint32_t board = boardOf(player) & m_validBoardMask;
    if (board == 0u) {
        return false;
    }

    const uint32_t occupied = (m_data.player1Board | m_data.player2Board | m_data.forbiddenBoard) & m_validBoardMask;
    if (canFly(player)) {
        return occupied != m_validBoardMask;
    }

    while (board != 0u) {
        const int32_t pos = CTZ32(board);
        if ((m_moveMask[pos] & ~occupied & m_validBoardMask) != 0u) {
            return true;
        }
        board &= board - 1u;
    }
    return false;
}

uint32_t NineChess::addNewMills(int32_t pos)
{
    const Players player = getWhosPiecePos(pos);
    if (player != PLAYER1 && player != PLAYER2) {
        return 0u;
    }

    const uint32_t board = boardOf(player);
    uint32_t count = 0u;
    for (uint32_t i = 0; i < m_posLineCount[pos]; ++i) {
        const int32_t lineId = m_posLineIds[pos][i];
        if (lineId < 0 || (board & m_lineMasks[lineId]) != m_lineMasks[lineId]) {
            continue;
        }

        if (m_rule.allowRepeatedMills) {
            ++count;
            continue;
        }

        const MillKey key = makeMillKeyForLine(player, static_cast<uint32_t>(lineId));
        if (!hasMillKey(key)) {
            m_data.millHistory.push_back(key);
            ++count;
        }
    }
    return count;
}

int32_t NineChess::getPieceNumberAtPos(int32_t pos) const
{
    if (!isValidPos(pos)) {
        return -1;
    }

    const uint32_t bit = bitOf(pos);
    for (int32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        if ((m_data.numberBoards[i] & bit) != 0u) {
            return i;
        }
    }
    return -1;
}

void NineChess::placeNumberedPiece(int32_t pos)
{
    if (m_rule.allowRepeatedMills) {
        return;
    }

    const Players turn = m_data.getTurn();
    const uint32_t number = turn == PLAYER1
        ? (m_rule.piecesPerSide - m_data.getPlayer1InHand())
        : (m_rule.piecesPerSide - m_data.getPlayer2InHand());
    if (number < NUMBERED_PIECE_COUNT) {
        m_data.numberBoards[number] |= bitOf(pos);
    }
}

void NineChess::moveNumberedPiece(int32_t fromPos, int32_t toPos)
{
    if (m_rule.allowRepeatedMills) {
        return;
    }

    const uint32_t fromBit = bitOf(fromPos);
    const uint32_t toBit = bitOf(toPos);
    for (int32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        if ((m_data.numberBoards[i] & fromBit) != 0u) {
            m_data.numberBoards[i] &= ~fromBit;
            m_data.numberBoards[i] |= toBit;
            return;
        }
    }
}

void NineChess::removeNumberedPiece(int32_t pos)
{
    if (m_rule.allowRepeatedMills) {
        return;
    }

    const uint32_t bit = bitOf(pos);
    for (int32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        if ((m_data.numberBoards[i] & bit) != 0u) {
            m_data.numberBoards[i] &= ~bit;
            return;
        }
    }
}

bool NineChess::hasMillKey(MillKey key) const
{
    for (std::vector<MillKey>::const_iterator it = m_data.millHistory.begin(); it != m_data.millHistory.end(); ++it) {
        if (*it == key) {
            return true;
        }
    }
    return false;
}

NineChess::MillKey NineChess::makeMillKeyForLine(Players player, uint32_t lineId) const
{
    const int32_t piece0 = getPieceNumberAtPos(m_linePos[lineId][0]);
    const int32_t piece1 = getPieceNumberAtPos(m_linePos[lineId][1]);
    const int32_t piece2 = getPieceNumberAtPos(m_linePos[lineId][2]);

    return makeMillKey(player == PLAYER2, lineId,
        piece0 >= 0 ? static_cast<uint32_t>(piece0) : 0u,
        piece1 >= 0 ? static_cast<uint32_t>(piece1) : 0u,
        piece2 >= 0 ? static_cast<uint32_t>(piece2) : 0u);
}

NineChess::Players NineChess::toggleTurn()
{
    if (m_data.getTurn() == PLAYER1) {
        m_data.setTurn(PLAYER2);
        return PLAYER2;
    }
    if (m_data.getTurn() == PLAYER2) {
        m_data.setTurn(PLAYER1);
        return PLAYER1;
    }
    return NOBODY;
}

void NineChess::enterOpening()
{
    m_winner = NOBODY;
    m_selectedPos = -1;
    m_data.setState(GAME_OPENING, ACTION_PLACE, PLAYER1);
    m_data.clearPendingCaptures();
}

void NineChess::enterMidgame()
{
    m_selectedPos = -1;
    m_data.setPhase(GAME_MID);
    m_data.setAction(ACTION_CHOOSE);
    m_data.clearPendingCaptures();
    m_data.forbiddenBoard = 0u;
    m_data.setTurn(m_rule.defenderMovesFirst ? PLAYER2 : PLAYER1);
}

void NineChess::setGameOver(Players winner, const std::string& tipText)
{
    m_winner = winner;
    m_selectedPos = -1;
    m_data.setState(GAME_OVER, ACTION_NONE,
        winner == PLAYER1 ? PLAYER1 : (winner == PLAYER2 ? PLAYER2 : NOBODY));
    m_data.clearPendingCaptures();
    m_tip = tipText;
}

void NineChess::rebuildTip()
{
    if (m_data.getPhase() == GAME_OVER) {
        if (!m_tip.empty()) {
            return;
        }
        if (m_winner == PLAYER1) {
            m_tip = "恭喜玩家1获胜！";
        }
        else if (m_winner == PLAYER2) {
            m_tip = "恭喜玩家2获胜！";
        }
        else {
            m_tip = "平局。";
        }
        return;
    }

    if (m_data.getPhase() == GAME_NOTSTARTED) {
        m_tip = "未开局";
        return;
    }

    const char* playerText = m_data.getTurn() == PLAYER1 ? "玩家1" : "玩家2";
    if (m_data.getPhase() == GAME_OPENING) {
        if (m_data.getAction() == ACTION_PLACE) {
            const uint32_t inHand = m_data.getTurn() == PLAYER1 ? m_data.getPlayer1InHand() : m_data.getPlayer2InHand();
            m_tip = std::string("轮到") + playerText + "落子，剩余" + std::to_string(inHand) + "子";
        }
        else {
            m_tip = std::string("轮到") + playerText + "去子，需去" + std::to_string(m_data.getPendingCaptures()) + "子";
        }
        return;
    }

    if (m_data.getAction() == ACTION_CHOOSE) {
        m_tip = std::string("轮到") + playerText + "选子移动";
    }
    else if (m_data.getAction() == ACTION_PLACE) {
        m_tip = std::string("轮到") + playerText + "落子";
    }
    else {
        m_tip = std::string("轮到") + playerText + "去子，需去" + std::to_string(m_data.getPendingCaptures()) + "子";
    }
}

std::string NineChess::formatPointCommand(int32_t pos) const
{
    int32_t c = -1;
    int32_t p = -1;
    posToCP(pos, c, p);
    return "(" + std::to_string(c) + "," + std::to_string(p) + ")";
}

std::string NineChess::formatConsolePointToken(int32_t pos) const
{
    const Players owner = getWhosPiecePos(pos);
    if (owner == PLAYER1 || owner == PLAYER2) {
        if (!m_rule.allowRepeatedMills) {
            const int32_t number = getPieceNumberAtPos(pos);
            if (number >= 0 && number < NUMBERED_PIECE_COUNT) {
                return owner == PLAYER1
                    ? PLAYER1_NUMBER_TOKENS[number]
                    : PLAYER2_NUMBER_TOKENS[number];
            }
        }
        return owner == PLAYER1 ? "●" : "○";
    }

    if (isForbiddenPos(pos)) {
        return "×";
    }
    return "·";
}

std::string NineChess::formatMillHistoryEntry(MillKey key) const
{
    std::ostringstream out;
    const uint32_t lineId = getMillKeyLineId(key);

    out << (isMillKeyPlayer2(key) ? "后手" : "先手")
        << " line=" << lineId
        << " pieces=["
        << getMillKeyPiece0(key) << ","
        << getMillKeyPiece1(key) << ","
        << getMillKeyPiece2(key) << "]";

    if (lineId < m_lineCount) {
        out << " cells=["
            << formatPointCommand(m_linePos[lineId][0]) << ","
            << formatPointCommand(m_linePos[lineId][1]) << ","
            << formatPointCommand(m_linePos[lineId][2]) << "]";
    }

    return out.str();
}

std::string NineChess::formatMoveCommand(int32_t fromPos, int32_t toPos) const
{
    return formatPointCommand(fromPos) + "->" + formatPointCommand(toPos);
}

std::string NineChess::formatCaptureCommand(int32_t pos) const
{
    return "-" + formatPointCommand(pos);
}

std::string NineChess::formatGiveupCommand(Players loser) const
{
    return loser == PLAYER1 ? "-0" : "-1";
}

std::string NineChess::formatDrawCommand() const
{
    return "==";
}

void NineChess::setLastCommand(const std::string& cmdline, bool commitHistory)
{
    m_cmdline = cmdline;
    if (commitHistory) {
        m_cmdHistory.push_back(cmdline);
    }
}

bool NineChess::parsePoint(const char*& text, int32_t& c, int32_t& p) const
{
    skipSpaces(text);
    if (*text != '(') {
        return false;
    }
    ++text;
    if (!parseUnsignedInt(text, c)) {
        return false;
    }
    skipSpaces(text);
    if (*text != ',') {
        return false;
    }
    ++text;
    if (!parseUnsignedInt(text, p)) {
        return false;
    }
    skipSpaces(text);
    if (*text != ')') {
        return false;
    }
    ++text;
    return true;
}

bool NineChess::parseSingleCommand(const char* text, int32_t& pos) const
{
    const char* cursor = text;
    int32_t c = -1;
    int32_t p = -1;
    if (!parsePoint(cursor, c, p)) {
        return false;
    }
    skipSpaces(cursor);
    if (*cursor != '\0' || !isValidCP(c, p)) {
        return false;
    }
    pos = cpToPos(c, p);
    return true;
}

bool NineChess::parseMoveCommand(const char* text, int32_t& fromPos, int32_t& toPos) const
{
    const char* cursor = text;
    int32_t c1 = -1;
    int32_t p1 = -1;
    int32_t c2 = -1;
    int32_t p2 = -1;
    if (!parsePoint(cursor, c1, p1)) {
        return false;
    }
    skipSpaces(cursor);
    if (cursor[0] != '-' || cursor[1] != '>') {
        return false;
    }
    cursor += 2;
    if (!parsePoint(cursor, c2, p2)) {
        return false;
    }
    skipSpaces(cursor);
    if (*cursor != '\0' || !isValidCP(c1, p1) || !isValidCP(c2, p2)) {
        return false;
    }
    fromPos = cpToPos(c1, p1);
    toPos = cpToPos(c2, p2);
    return true;
}

bool NineChess::parseCaptureCommand(const char* text, int32_t& pos) const
{
    const char* cursor = text;
    skipSpaces(cursor);
    if (*cursor != '-') {
        return false;
    }
    ++cursor;

    int32_t c = -1;
    int32_t p = -1;
    if (!parsePoint(cursor, c, p)) {
        return false;
    }
    skipSpaces(cursor);
    if (*cursor != '\0' || !isValidCP(c, p)) {
        return false;
    }
    pos = cpToPos(c, p);
    return true;
}

bool NineChess::parseGiveupCommand(const char* text, Players& loser) const
{
    loser = NOBODY;

    const char* cursor = text;
    skipSpaces(cursor);
    if (cursor[0] != '-' || (cursor[1] != '0' && cursor[1] != '1')) {
        return false;
    }
    cursor += 2;
    skipSpaces(cursor);
    if (*cursor != '\0') {
        return false;
    }

    loser = cursor[-1] == '0' ? PLAYER1 : PLAYER2;
    return true;
}

bool NineChess::parseDrawCommand(const char* text) const
{
    const char* cursor = text;
    skipSpaces(cursor);
    if (cursor[0] != '=' || cursor[1] != '=') {
        return false;
    }
    cursor += 2;
    skipSpaces(cursor);
    return *cursor == '\0';
}

int32_t NineChess::transformPos(int32_t pos, TransformMode mode)
{
    const int32_t ring = pos / SEAT;
    const int32_t seat = pos % SEAT;

    switch (mode)
    {
    case TRANSFORM_MIRROR:
        return ring * SEAT + ((SEAT - seat) & 7);
    case TRANSFORM_TURN:
        return (RING - 1 - ring) * SEAT + seat;
    case TRANSFORM_FLIP_VERTICAL:
        // 上下翻转可视为 seat -> (4 - seat) mod 8。
        // 它与“左右镜像 + 180 度旋转”完全等价，但这里直接给出闭式表达式，
        // 便于一次完成整局变换。
        return ring * SEAT + ((4 - seat) & 7);
    case TRANSFORM_ROTATE_LEFT:
        return ring * SEAT + ((seat + SEAT - 2) & 7);
    case TRANSFORM_ROTATE_RIGHT:
        return ring * SEAT + ((seat + 2) & 7);
    case TRANSFORM_ROTATE_180:
        return ring * SEAT + ((seat + 4) & 7);
    default:
        return pos;
    }
}

uint32_t NineChess::transformBoard(uint32_t board, TransformMode mode) const
{
    uint32_t result = 0u;
    uint32_t bits = board & m_validBoardMask;
    while (bits != 0u) {
        const int32_t pos = CTZ32(bits);
        result |= bitOf(transformPos(pos, mode));
        bits &= bits - 1u;
    }
    return result;
}

NineChess::MillKey NineChess::transformMillKey(MillKey key, TransformMode mode) const
{
    const uint32_t oldLineId = getMillKeyLineId(key);
    if (oldLineId >= m_lineCount) {
        return key;
    }

    const int32_t mappedPos[MILL] = {
        transformPos(m_linePos[oldLineId][0], mode),
        transformPos(m_linePos[oldLineId][1], mode),
        transformPos(m_linePos[oldLineId][2], mode)
    };
    const uint32_t mappedMask = bitOf(mappedPos[0]) | bitOf(mappedPos[1]) | bitOf(mappedPos[2]);

    int32_t newLineId = -1;
    for (uint32_t lineId = 0; lineId < m_lineCount; ++lineId) {
        if (m_lineMasks[lineId] == mappedMask) {
            newLineId = static_cast<int32_t>(lineId);
            break;
        }
    }
    if (newLineId < 0) {
        return key;
    }

    const uint32_t oldPieces[MILL] = { getMillKeyPiece0(key), getMillKeyPiece1(key), getMillKeyPiece2(key) };
    uint32_t newPieces[MILL] = {};
    for (int32_t targetIndex = 0; targetIndex < MILL; ++targetIndex) {
        const int32_t targetPos = m_linePos[newLineId][targetIndex];
        for (int32_t sourceIndex = 0; sourceIndex < MILL; ++sourceIndex) {
            if (mappedPos[sourceIndex] == targetPos) {
                newPieces[targetIndex] = oldPieces[sourceIndex];
                break;
            }
        }
    }

    return makeMillKey(isMillKeyPlayer2(key), static_cast<uint32_t>(newLineId),
        newPieces[0], newPieces[1], newPieces[2]);
}

void NineChess::transformState(TransformMode mode, bool rewriteCommands)
{
    m_data.player1Board = transformBoard(m_data.player1Board, mode);
    m_data.player2Board = transformBoard(m_data.player2Board, mode);
    m_data.forbiddenBoard = transformBoard(m_data.forbiddenBoard, mode);
    for (int32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
        m_data.numberBoards[i] = transformBoard(m_data.numberBoards[i], mode);
    }

    if (!m_rule.allowRepeatedMills) {
        for (std::vector<MillKey>::iterator it = m_data.millHistory.begin(); it != m_data.millHistory.end(); ++it) {
            *it = transformMillKey(*it, mode);
        }
    }

    if (isValidPos(m_selectedPos)) {
        m_selectedPos = transformPos(m_selectedPos, mode);
    }

    if (rewriteCommands) {
        if (!m_cmdline.empty()) {
            m_cmdline = transformCommandString(m_cmdline, mode);
        }
        for (std::vector<std::string>::iterator it = m_cmdHistory.begin(); it != m_cmdHistory.end(); ++it) {
            *it = transformCommandString(*it, mode);
        }
    }
}

std::string NineChess::transformCommandString(const std::string& command, TransformMode mode) const
{
    int32_t fromPos = -1;
    int32_t toPos = -1;
    if (parseMoveCommand(command.c_str(), fromPos, toPos)) {
        return formatMoveCommand(transformPos(fromPos, mode), transformPos(toPos, mode));
    }

    int32_t pos = -1;
    if (parseCaptureCommand(command.c_str(), pos)) {
        return formatCaptureCommand(transformPos(pos, mode));
    }
    if (parseSingleCommand(command.c_str(), pos)) {
        return formatPointCommand(transformPos(pos, mode));
    }

    Players loser = NOBODY;
    if (parseGiveupCommand(command.c_str(), loser)) {
        return formatGiveupCommand(loser);
    }
    if (parseDrawCommand(command.c_str())) {
        return formatDrawCommand();
    }
    return command;
}


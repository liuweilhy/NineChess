/****************************************************************************
** NineChess - 九连棋游戏
** 作者: liuweilhy (liuweilhy@163.com)
** 日期: 2013.01.14
**
** NineChess 核心规则类。
**
** 当前版本的设计原则是：
**   - 只以 ChessData 中的紧凑状态和位棋盘为真值
**   - 同时提供安全接口与 AI 快速接口
**   - 统一负责命令编解码、局面变换和哈希入口
**
** 其中：
**   - 安全接口：带完整合法性校验，适合普通调用方
**   - 快速接口：直接按 0~23 点位操作，适合 AI 搜索热路径
****************************************************************************/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "ninechess_common.h"

class NineChess
{
    // AI 搜索类需要直接访问内部辅助表和局面数据。
    friend class NineChess_AI_AB;

public:
    // 从公共头中导出常用类型，减少外部书写成本。
    using Rule = ::Rule;
    using ChessData = ::ChessData;
    using Phases = ::Phases;
    using Actions = ::Actions;
    using Players = ::Players;
    using Rotates = ::Rotates;
    using MillKey = ::MillKey;

    // 兼容旧代码中通过类名访问的公共常量。
    static constexpr int MILL = ::MILL;
    static constexpr int RING = ::RING;
    static constexpr int SEAT = ::SEAT;
    static constexpr int BOARD_SIZE = ::BOARD_SIZE;
    static constexpr int RULE_COUNT = ::RULE_COUNT;
    static constexpr Players NOBODY = ::NOBODY;
    static constexpr Players PLAYER1 = ::PLAYER1;
    static constexpr Players PLAYER2 = ::PLAYER2;
    static constexpr Players DRAW = ::DRAW;
    static constexpr Phases GAME_NOTSTARTED = ::GAME_NOTSTARTED;
    static constexpr Phases GAME_OPENING = ::GAME_OPENING;
    static constexpr Phases GAME_MID = ::GAME_MID;
    static constexpr Phases GAME_OVER = ::GAME_OVER;
    static constexpr Actions ACTION_NONE = ::ACTION_NONE;
    static constexpr Actions ACTION_CHOOSE = ::ACTION_CHOOSE;
    static constexpr Actions ACTION_PLACE = ::ACTION_PLACE;
    static constexpr Actions ACTION_CAPTURE = ::ACTION_CAPTURE;
    static constexpr Rotates ROTATE_0 = ::ROTATE_0;
    static constexpr Rotates ROTATE_LEFT = ::ROTATE_LEFT;
    static constexpr Rotates ROTATE_RIGHT = ::ROTATE_RIGHT;
    static constexpr Rotates ROTATE_180 = ::ROTATE_180;

    // 工程内置的 4 套规则表。
    // setRule(ruleIndex) 时会从这里拷贝到 m_rule。
    static const Rule rules[RULE_COUNT];

public:
    // 默认构造当前使用“九连棋”规则（rules[2]）。
    NineChess();

    // 默认析构即可，所有成员都由其自身类型负责清理。
    ~NineChess() = default;

    // 允许按值复制整个棋局对象，便于 AI 搜索时保存和回退局面。
    NineChess(const NineChess&) = default;

    // 允许按值赋值整个棋局对象。
    NineChess& operator=(const NineChess&) = default;

    // ==================== 规则与数据 ====================
    // 切换规则会：
    // 1. 更新当前规则对象 m_rule
    // 2. 重建招法表和三连线表
    // 3. 自动 reset 到该规则的初始局面
    void setRule(uint32_t ruleIndex);

    // 返回当前规则在 rules[] 中的下标。
    uint32_t getRuleIndex() const { return m_ruleIndex; }

    // 返回当前规则对象。
    const Rule* getRule() const { return &m_rule; }

    // 直接返回当前局面数据。
    // const 版本用于查询，非 const 版本主要给 AI / 调试代码使用。
    const ChessData& getData() const { return m_data; }
    ChessData& getData() { return m_data; }

    // ==================== 基础查询 ====================
    // 坐标均为 0-based：
    // c: 0~2 表示三圈
    // p: 0~7 表示该圈上的 8 个位置
    // 判断给定的圈位坐标是否落在棋盘范围内。
    bool isValidCP(int32_t c, int32_t p) const;

    // 判断给定的一维点位编号是否在 0~23 范围内。
    bool isValidPos(int32_t pos) const;

    // 将 (圈, 位) 转成 0~23 的一维点位编号。
    // 输入非法时返回 -1。
    int32_t cpToPos(int32_t c, int32_t p) const;

    // 将一维点位编号拆成 (圈, 位)。
    // 输入非法时 c / p 均返回 -1。
    void posToCP(int32_t pos, int32_t& c, int32_t& p) const;

    // 当前局面阶段：未开局 / 开局摆子 / 中局走子 / 结局。
    Phases getPhase() const { return m_data.getPhase(); }

    // 当前动作：选子 / 落子 / 提子。
    Actions getAction() const { return m_data.getAction(); }

    // 当前轮到哪一方行动。
    Players getTurn() const { return m_data.getTurn(); }

    // 当前赢家。
    // 未分胜负返回 NOBODY，平局返回 DRAW。
    Players getWinner() const { return m_winner; }

    // 兼容旧接口名。
    Players whoWin() const { return getWinner(); }

    // 压缩后的 status 原始值。
    uint32_t getStatus() const { return m_data.status; }

    // 兼容旧接口名。
    uint16_t getFlags() const { return static_cast<uint16_t>(getStatus()); }

    // 当前被选中的棋子点位；没有选中时为 -1。
    int32_t getCurrentPos() const { return m_selectedPos; }

    // 先手当前手中的未落子数。
    uint32_t getPlayer1InHand() const { return m_data.getPlayer1InHand(); }

    // 兼容旧接口名。
    uint32_t getPlayer1_InHand() const { return getPlayer1InHand(); }

    // 后手当前手中的未落子数。
    uint32_t getPlayer2InHand() const { return m_data.getPlayer2InHand(); }

    // 兼容旧接口名。
    uint32_t getPlayer2_InHand() const { return getPlayer2InHand(); }

    // 先手当前在盘存活棋子数。
    uint32_t getPlayer1OnBoardCount() const { return m_data.getPlayer1OnBoardCount(); }

    // 后手当前在盘存活棋子数。
    uint32_t getPlayer2OnBoardCount() const { return m_data.getPlayer2OnBoardCount(); }

    // 当前还需要继续提掉多少颗子。
    uint32_t getPendingCaptures() const { return m_data.getPendingCaptures(); }

    // 兼容旧接口名。
    Players whosTurn() const { return getTurn(); }

    // 查询给定圈位坐标上的棋子归属。
    // 空点或禁点都返回 NOBODY。
    Players getWhosPiece(int32_t c, int32_t p) const;

    // 查询给定 0~23 点位上的棋子归属。
    // 空点或禁点都返回 NOBODY。
    Players getWhosPiecePos(int32_t pos) const;

    // 返回某个点位的邻接点位掩码。
    // 主要给 AI 快速生成着法时使用。
    uint32_t getMoveMask(int32_t pos) const { return isValidPos(pos) ? m_moveMask[pos] : 0u; }

    // 九连棋专用查询：按“玩家 + 编号”返回对应棋子的圈位坐标。
    // 非九连棋规则或该编号棋不在盘上时返回 false。
    bool getPieceCP(Players player, uint32_t number, int32_t& c, int32_t& p) const;

    // 九连棋专用查询：返回当前选中棋子的玩家和编号。
    // 非九连棋规则或当前没有选中棋子时返回 false。
    bool getCurrentPiece(Players& player, uint32_t& number) const;

    // ==================== 命令文本 ====================
    // 命令格式统一为：
    //   "(c,p)"            开局落子
    //   "(c1,p1)->(c2,p2)" 中局走子
    //   "-(c,p)"           提子
    //   "-0"               先手认输
    //   "-1"               后手认输
    //   "=="               外部裁定平局
    // 返回当前局面的提示文本。
    const std::string& getTip() const { return m_tip; }

    // 返回最后一次成功执行的命令文本。
    const char* getCmdLine() const { return m_cmdline.c_str(); }

    // 返回完整命令历史。
    const std::vector<std::string>& getCmdHistory() const { return m_cmdHistory; }

    // 兼容旧接口名，含义与 getCmdHistory() 相同。
    const std::vector<std::string>* getCmdList() const { return &m_cmdHistory; }

    // 生成适合命令行测试的 UTF-8 棋盘文本。
    // 文本中会包含：
    // 1. 棋盘线条和 24 个点位的当前状态；
    // 2. 双方手牌数、在盘棋子数、当前阶段/动作/轮次等摘要；
    // 3. 九连棋规则下的历史三连列表。
    // showMillHistory 为 false 时，最后一段历史三连列表会被省略。
    std::string getConsoleText(bool showMillHistory = true) const;

    // ==================== 合法性判断 ====================
    // 判断当前局面下，给定点位是否允许“选子”。
    bool canChoosePos(int32_t pos) const;

    // 判断当前局面下，给定点位是否允许“落子/移子到该点”。
    bool canPlacePos(int32_t pos) const;

    // 判断当前局面下，给定点位是否允许“提子”。
    bool canCapturePos(int32_t pos) const;

    // ==================== 游戏控制 ====================
    // 重置局面到当前规则的初始状态，但不改变规则本身。
    void reset();

    // 从未开局或已结局状态进入新对局。
    void start();

    // 根据当前局面重建提示文本。
    void refreshTip();

    // ==================== 安全操作接口 ====================
    // 安全接口会做完整校验，并维护提示文本与命令历史。

    // 按 (圈, 位) 选择己方棋子。
    bool choose(int32_t c, int32_t p);

    // 按 (圈, 位) 落子，或把已选棋子移动到目标点。
    bool place(int32_t c, int32_t p);

    // 按 (圈, 位) 提掉对手一颗棋子。
    bool capture(int32_t c, int32_t p);

    // 安全版点位选子接口，直接使用 0~23 点位编号。
    bool choosePos(int32_t pos);

    // 安全版点位落子接口，直接使用 0~23 点位编号。
    bool placePos(int32_t pos);

    // 安全版点位提子接口，直接使用 0~23 点位编号。
    bool capturePos(int32_t pos);

    // 当前轮次玩家认输。
    bool giveup();

    // 指定某一方认输。
    bool giveup(Players loser);

    // 由控制层直接裁定胜负。
    bool adjudicateWin(Players winner, const std::string& tipText = std::string());

    // 由控制层直接裁定平局。
    bool adjudicateDraw(const std::string& tipText = std::string());

    // 解析并执行一条命令文本。
    bool command(const char* cmd);

    // ==================== AI 快速接口 ====================
    // 快速接口只按 pos 执行，不做合法性验证，也不维护命令历史/提示文本。

    // 快速版选子。
    void chooseFast(int32_t pos);

    // 快速版落子/移子。
    void placeFast(int32_t pos);

    // 快速版提子。
    void captureFast(int32_t pos);

    // ==================== 变换与哈希 ====================
    // 左右镜像当前局面。
    // rewriteCommands 为 true 时，同步改写命令文本与命令历史。
    void mirror(bool rewriteCommands = true);

    // 内外圈翻转当前局面。
    void turn(bool rewriteCommands = true);

    // 上下翻转当前局面。
    // 这与“左右镜像 + 180 度旋转”等价，但单独提供接口更便于外部直接调用。
    void flipVertical(bool rewriteCommands = true);

    // 按离散角度旋转当前局面。
    void rotate(Rotates rotate, bool rewriteCommands = true);

    // 兼容旧接口：使用整数角度旋转。
    void rotate(int32_t degrees, bool rewriteCommands = true);

    // 按当前规则自动选择 lite / hard 哈希算法。
    uint64_t getHash() const;

    // 只基于主位棋盘和 status 的轻量哈希。
    uint64_t getHashLite() const { return m_data.getHashLite(); }

    // 把九连棋序号层和历史三连也混入的重哈希。
    uint64_t getHashHard() const { return m_data.getHashHard(); }

protected:
    // 棋盘上合法三连线的最大数量。
    // 无斜线为 16，有斜线时为 20，因此这里取上界 20。
    static constexpr uint32_t MAX_MILL_LINE_COUNT = 20;

    // 单个点位最多会参与 3 条三连线。
    static constexpr uint32_t MAX_LINES_PER_POS = 3;

    // 局面变换的内部编码。
    // mirror / turn / rotate 最终都会映射到这里统一处理。
    enum TransformMode : uint32_t {
        TRANSFORM_MIRROR = 0,
        TRANSFORM_TURN = 1,
        TRANSFORM_FLIP_VERTICAL = 2,
        TRANSFORM_ROTATE_LEFT = 3,
        TRANSFORM_ROTATE_RIGHT = 4,
        TRANSFORM_ROTATE_180 = 5
    };

    // 当前规则在 rules[] 中的下标。
    uint32_t m_ruleIndex = 0;

    // 当前正在使用的规则对象。
    Rule m_rule = rules[0];

    // 棋盘辅助表：
    // 1. m_moveMask[pos]       : 该点的邻接点位集合
    // 2. m_lineMasks[lineId]   : 某条三连线的位掩码
    // 3. m_linePos[lineId][k]  : 某条三连线第 k 个固定位置
    // 4. m_posLineIds[pos][i]  : 某点位参与的所有 lineId
    // 24 位有效棋盘掩码，低 24 位对应真实棋盘点位。
    uint32_t m_validBoardMask = ChessData::VALID_BOARD_MASK;

    // 每个点位的邻接点位集合。
    uint32_t m_moveMask[BOARD_SIZE] = {};

    // 当前规则下实际三连线总数。
    uint32_t m_lineCount = 0;

    // 每条三连线对应的 24 位掩码。
    uint32_t m_lineMasks[MAX_MILL_LINE_COUNT] = {};

    // 每条三连线的 3 个固定点位。
    int8_t m_linePos[MAX_MILL_LINE_COUNT][MILL] = {};

    // 每个点位一共参与多少条三连线。
    uint8_t m_posLineCount[BOARD_SIZE] = {};

    // 每个点位参与的 lineId 列表。
    int8_t m_posLineIds[BOARD_SIZE][MAX_LINES_PER_POS] = {};

    // 当前局面真值数据。
    ChessData m_data;

    // 当前赢家缓存。
    Players m_winner = NOBODY;

    // 当前被选中的棋子点位；未选中时为 -1。
    int32_t m_selectedPos = -1;

    // 最后一条成功执行的命令文本。
    std::string m_cmdline;

    // 完整命令历史。
    std::vector<std::string> m_cmdHistory;

    // 当前局面的提示文本。
    std::string m_tip;

protected:
    // 重建整张邻接招法表。
    void buildMoveTable();

    // 重建整张三连线辅助表。
    void buildMillTable();

    // 向辅助表中登记一条三连线。
    void addMillLine(int32_t pos0, int32_t pos1, int32_t pos2);

    // 计算点位 pos 对应的单比特掩码。
    static uint32_t bitOf(int32_t pos) { return 1u << pos; }

    // 返回某一方的对手。
    static Players opponentOf(Players player);

    // 返回指定玩家当前主位棋盘的值拷贝。
    uint32_t boardOf(Players player) const;

    // 返回指定玩家主位棋盘的可写引用。
    uint32_t& boardRef(Players player);

    // 判断点位上是否已有棋子。
    bool isOccupiedPos(int32_t pos) const;

    // 判断点位当前是否是禁点。
    bool isForbiddenPos(int32_t pos) const;

    // 判断点位是否为空且不是禁点。
    bool isEmptyPos(int32_t pos) const;

    // 判断指定玩家在当前局面下是否拥有飞子权。
    bool canFly(Players player) const;

    // 判断两个点位是否直接邻接。
    bool isAdjacent(int32_t fromPos, int32_t toPos) const;

    // 判断点位上是否是某一方自己的棋子。
    bool isOwnPieceAt(Players player, int32_t pos) const;

    // 判断点位上是否是某一方对手的棋子。
    bool isOpponentPieceAt(Players player, int32_t pos) const;

    // 统一的选子执行入口。
    // validate 控制是否做合法性检查；
    // updateView 控制是否维护提示文本和命令文本。
    bool doChoose(int32_t pos, bool validate, bool updateView);

    // 统一的落子/移子执行入口。
    bool doPlace(int32_t pos, bool validate, bool updateView);

    // 统一的提子执行入口。
    bool doCapture(int32_t pos, bool validate, bool updateView);

    // 执行开局阶段的一次摆子。
    void applyOpeningPlacement(int32_t pos);

    // 执行中局阶段的一次移子。
    void applyMidMove(int32_t toPos);

    // 执行一次提子。
    void applyCapture(int32_t pos);

    // 落子或移子后，处理成三、切换阶段、切换轮次和胜负判断。
    bool tryFinishAfterPlaceOrMove(int32_t pos);

    // 提子后，处理剩余提子数、切换阶段、切换轮次和胜负判断。
    bool tryFinishAfterCapture();

    // 仅从棋子数量和棋盘是否摆满角度判断是否已经分胜负。
    bool trySetWinnerFromMaterialOrBoard();

    // 在中局选子阶段处理“无子可走”的情况。
    bool tryHandleBlockedTurn();

    // 统计某个点位上的棋子当前参与了多少条三连。
    uint32_t countMillsAt(int32_t pos) const;

    // 判断某个点位上的棋子是否处于至少一条三连中。
    bool isPieceInMill(int32_t pos) const;

    // 判断某一方当前是否所有棋子都处于三连中。
    bool isAllInMills(Players player) const;

    // 判断某一方当前是否还存在至少一步合法着法。
    bool hasAnyLegalMove(Players player) const;

    // 检查某点产生的新三连，并返回本次真正新增的可提子三连数。
    uint32_t addNewMills(int32_t pos);

    // 返回某点位上的棋子编号。
    // 非九连棋或该点无编号棋时返回 -1。
    int32_t getPieceNumberAtPos(int32_t pos) const;

    // 开局落子时，为九连棋编号层登记一颗新棋子。
    void placeNumberedPiece(int32_t pos);

    // 中局移子时，同步移动编号层中的那颗棋子。
    void moveNumberedPiece(int32_t fromPos, int32_t toPos);

    // 提子时，从编号层中移除对应棋子。
    void removeNumberedPiece(int32_t pos);

    // 查询某个历史三连 key 是否已经存在。
    bool hasMillKey(MillKey key) const;

    // 按“玩家 + lineId”生成当前三连对应的 MillKey。
    MillKey makeMillKeyForLine(Players player, uint32_t lineId) const;

    // 在 PLAYER1 / PLAYER2 之间切换轮次，并返回切换后的结果。
    Players toggleTurn();

    // 把局面切换到开局摆子阶段。
    void enterOpening();

    // 把局面切换到中局走子阶段。
    void enterMidgame();

    // 设置赢家并进入 GAME_OVER 状态。
    void setGameOver(Players winner, const std::string& tipText);

    // 按当前局面重建 m_tip。
    void rebuildTip();

    // 把单点动作格式化为 "(c,p)"。
    std::string formatPointCommand(int32_t pos) const;

    // 返回命令行棋盘中某个点位应显示的字符。
    // 普通规则使用 ● / ○，九连棋使用带编号的 Unicode 圈号字符。
    std::string formatConsolePointToken(int32_t pos) const;

    // 把一条历史三连 key 格式化为便于命令行阅读的文本。
    std::string formatMillHistoryEntry(MillKey key) const;

    // 把走子动作格式化为 "(c1,p1)->(c2,p2)"。
    std::string formatMoveCommand(int32_t fromPos, int32_t toPos) const;

    // 把提子动作格式化为 "-(c,p)"。
    std::string formatCaptureCommand(int32_t pos) const;

    // 生成认输命令文本。
    // "-0" 表示先手认输，"-1" 表示后手认输。
    std::string formatGiveupCommand(Players loser) const;

    // 生成平局命令文本。
    std::string formatDrawCommand() const;

    // 更新最后命令文本，并按需把它加入命令历史。
    void setLastCommand(const std::string& cmdline, bool commitHistory);

    // 从文本中解析一个 "(c,p)" 点位。
    bool parsePoint(const char*& text, int32_t& c, int32_t& p) const;

    // 解析单点命令 "(c,p)"。
    bool parseSingleCommand(const char* text, int32_t& pos) const;

    // 解析走子命令 "(c1,p1)->(c2,p2)"。
    bool parseMoveCommand(const char* text, int32_t& fromPos, int32_t& toPos) const;

    // 解析提子命令 "-(c,p)"。
    bool parseCaptureCommand(const char* text, int32_t& pos) const;

    // 解析认输命令 "-0" / "-1"。
    bool parseGiveupCommand(const char* text, Players& loser) const;

    // 解析平局命令 "=="。
    bool parseDrawCommand(const char* text) const;

    // 统一的外部裁定入口。
    bool adjudicateResult(Players winner, const std::string& tipText,
        const std::string* recordedCommand);

    // 把单个点位按指定变换模式映射到新点位。
    static int32_t transformPos(int32_t pos, TransformMode mode);

    // 把整张位棋盘按指定变换模式映射。
    uint32_t transformBoard(uint32_t board, TransformMode mode) const;

    // 把历史三连 key 按指定变换模式映射。
    MillKey transformMillKey(MillKey key, TransformMode mode) const;

    // 对整局状态执行几何变换。
    void transformState(TransformMode mode, bool rewriteCommands);

    // 对一条命令文本执行几何变换。
    std::string transformCommandString(const std::string& command, TransformMode mode) const;
};


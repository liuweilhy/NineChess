/****************************************************************************
** NineChess - 九连棋游戏
** 作者: liuweilhy (liuweilhy@163.com)
** 日期: 2013.01.14
**
** 九连棋游戏的公共基础定义
** 包含：
**   - 棋盘与规则相关常量
**   - 紧凑 status 的位布局定义
**   - 规则参数结构体 Rule
**   - AI / 核心逻辑共用的棋局结构体 ChessData
****************************************************************************/

#pragma once

#include <cstdint>
#include <vector>

// 位操作的兼容性处理
#if __cplusplus >= 202002L
#include <bit>
#define POPCOUNT32(x) static_cast<uint32_t>(std::popcount(x))
#define POPCOUNT64(x) static_cast<uint64_t>(std::popcount(x))
#define CTZ32(x) std::countr_zero(x)
#define CTZ64(x) std::countr_zero(x)
#elif defined(__GNUC__) || defined(__clang__)
#define POPCOUNT32(x) __builtin_popcount(x)
#define POPCOUNT64(x) __builtin_popcountll(x)
#define CTZ32(x) __builtin_ctz(x)
#define CTZ64(x) __builtin_ctzll(x)
#elif defined(_MSC_VER)
#include <intrin.h>
#define POPCOUNT32(x) __popcnt(x)
#define POPCOUNT64(x) __popcnt64(x)
#define CTZ32(x) _tzcnt_u32(x)
#define CTZ64(x) _tzcnt_u64(x)
#else
#error "No popcount/ctz implementation"
#endif

// ================== 基础常量 ==================
// 三连固定为 3，同时也是大多数规则的赛点子数。
constexpr int MILL = 3;
// 棋盘一共有 3 圈：内圈、中圈、外圈。
constexpr int RING = 3;
// 每圈固定 8 个落子点。
constexpr int SEAT = 8;
// 棋盘总点位数：3 * 8 = 24。
constexpr int BOARD_SIZE = RING * SEAT;
// 项目当前内置 4 套规则。
constexpr int RULE_COUNT = 4;
// 任一规则单方最大棋子数。打三棋为 12，其余规则为 9。
constexpr int MAX_PIECES_PER_SIDE = 12;
// 序号位棋盘只服务于带编号的九连棋规则，因此最多只需要 9 层。
constexpr int NUMBERED_PIECE_COUNT = 9;

// ==================== 规则结构体 ====================
// Rule 描述一套完整规则。
// 统一使用语义化的新字段名，避免继续混用历史命名。
struct Rule {
    // 规则名称，例如“成三棋”“莫里斯九子棋”。
    const char* name = nullptr;

    // 规则说明文本。
    const char* description = nullptr;

    // 每方棋子数。当前规则只会是 9 或 12。
    uint32_t piecesPerSide = 0;

    // 判负所需的最低存活子数。通常为 3。
    uint32_t minPiecesToSurvive = 0;

    // 棋盘是否存在对角连线。打三棋有斜线，其余规则没有。
    bool hasDiagonalLines = false;

    // 摆棋阶段被提子的点位，是否在开局结束前都视为禁点。
    bool hasForbiddenPoints = false;

    // 摆子完成后，是否由后摆棋的一方先走。
    bool defenderMovesFirst = false;

    // 同位置、同序号的三连是否允许重复提子。
    bool allowRepeatedMills = false;

    // 一步同时形成多个三连时，是否允许多提子。
    bool allowMultiCapture = false;

    // 摆满棋盘时是否判先手负；若为 false，则判和。
    bool fullBoardIsLoss = false;

    // 中局无路可走时是否判负；若为 false，则视为轮空，由对手继续走。
    bool blockedIsLoss = false;

    // 剩三子时是否允许飞子到任意空位。
    bool allowFlying = false;

};

// ==================== status 位布局 ====================
// status 的设计目标是：
// 1. 让高频状态集中存储，便于压缩、复制和哈希；
// 2. 让“后手手牌数”位于最低 4 位，以支持直接 --status；
// 3. 让“待去子数 / 阶段 / 动作 / 当前轮次”都能被快速读取。
//
// 当前位布局：
// bit  0-3  : 后手手牌数（0~12）
// bit  4-5  : 待去子数（0~3）
// bit  6-8  : 局面阶段
// bit  9-11 : 当前动作
// bit 12-13 : 当前轮次
//
// 目前总共使用 14 bit，仍保留后续扩展空间。
constexpr uint32_t STATUS_PLAYER2_IN_HAND_SHIFT = 0;
constexpr uint32_t STATUS_PLAYER2_IN_HAND_MASK = 0x0000000f;
constexpr uint32_t STATUS_PENDING_CAPTURES_SHIFT = 4;
constexpr uint32_t STATUS_PENDING_CAPTURES_MASK = 0x00000030;
constexpr uint32_t STATUS_PHASE_SHIFT = 6;
constexpr uint32_t STATUS_PHASE_MASK = 0x000001c0;
constexpr uint32_t STATUS_ACTION_SHIFT = 9;
constexpr uint32_t STATUS_ACTION_MASK = 0x00000e00;
constexpr uint32_t STATUS_TURN_SHIFT = 12;
constexpr uint32_t STATUS_TURN_MASK = 0x00003000;

enum Phases : uint32_t {
    GAME_NOTSTARTED = 0x00000000,
    GAME_OPENING = 0x00000040,
    GAME_MID = 0x00000080,
    GAME_OVER = 0x00000100
};

enum Actions : uint32_t {
    ACTION_NONE = 0x00000000,
    ACTION_CHOOSE = 0x00000200,
    ACTION_PLACE = 0x00000400,
    ACTION_CAPTURE = 0x00000800
};

enum Players : uint32_t {
    NOBODY = 0x00000000,
    PLAYER1 = 0x00001000,
    PLAYER2 = 0x00002000,
    // DRAW 不再作为 status 的常规状态位使用；
    // 表示禁止点或平局。
    DRAW = PLAYER1 | PLAYER2
};

// 旋转角度定义，仅用于图形变换。
enum Rotates : uint32_t {
    ROTATE_0 = 0x00000000,
    ROTATE_LEFT = 0x00000001,
    ROTATE_RIGHT = 0x00000002,
    ROTATE_180 = 0x00000004
};

// ==================== 九连棋三连记录位布局 ====================
// 九连棋不只关心“三个点位形成了三连”，还关心：
// 1. 这是哪一方形成的；
// 2. 这是哪一条合法三连线；
// 3. 这条线 3 个固定位置上分别站的是哪 3 颗序号棋。
//
// 这里不直接存 3 个棋盘位置，而是存 lineId。
// 原因是：三连一定落在预定义的合法连线上；一旦 lineId 固定，
// “位置”就由该 lineId 的 3 个固定点位唯一确定。
//
// 采用 lineId 后，一条三连记录只需要：
// bit  0-2  : 该线第 0 个固定位置上的棋子序号（0~8）
// bit  3-5  : 该线第 1 个固定位置上的棋子序号（0~8）
// bit  6-8  : 该线第 2 个固定位置上的棋子序号（0~8）
// bit  9-12 : 三连线编号 lineId（0~15，九连棋足够）
// bit 13    : 玩家位，0 表示先手，1 表示后手
// bit 14-15 : 预留
//
// 这样一条记录只占 14 bit，完整装进 uint16_t。
using MillKey = uint16_t;
constexpr MillKey MILL_KEY_NONE = 0x0000u;
constexpr uint32_t MILL_KEY_PIECE0_SHIFT = 0;
constexpr uint32_t MILL_KEY_PIECE1_SHIFT = 3;
constexpr uint32_t MILL_KEY_PIECE2_SHIFT = 6;
constexpr uint32_t MILL_KEY_LINE_SHIFT = 9;
constexpr uint32_t MILL_KEY_PLAYER_SHIFT = 13;
constexpr MillKey MILL_KEY_PIECE_MASK = 0x0007u;
constexpr MillKey MILL_KEY_PIECE0_MASK = 0x0007u;
constexpr MillKey MILL_KEY_PIECE1_MASK = 0x0038u;
constexpr MillKey MILL_KEY_PIECE2_MASK = 0x01c0u;
constexpr MillKey MILL_KEY_LINE_MASK = 0x1e00u;
constexpr MillKey MILL_KEY_PLAYER_MASK = 0x2000u;
constexpr MillKey MILL_KEY_USED_MASK = 0x3fffu;

// 生成一条紧凑 MillKey。
// 注意：
// 1. 3 个序号必须按该 lineId 预定义的 3 个位置顺序传入；
// 2. 因为“相同位置且相同顺序的三连”才被视为同一历史记录，所以顺序本身是状态的一部分；
// 3. 序号从 0 开始，因此每颗棋只需 3 bit。
inline MillKey makeMillKey(bool isPlayer2, uint32_t lineId,
    uint32_t piece0, uint32_t piece1, uint32_t piece2)
{
    return static_cast<MillKey>(
          ((piece0 & MILL_KEY_PIECE_MASK) << MILL_KEY_PIECE0_SHIFT)
        | ((piece1 & MILL_KEY_PIECE_MASK) << MILL_KEY_PIECE1_SHIFT)
        | ((piece2 & MILL_KEY_PIECE_MASK) << MILL_KEY_PIECE2_SHIFT)
        | ((lineId & 0x000fu) << MILL_KEY_LINE_SHIFT)
        | ((static_cast<uint32_t>(isPlayer2) & 0x0001u) << MILL_KEY_PLAYER_SHIFT));
}

// 读取 MillKey 中第 0 个固定位置上的棋子序号。
inline uint32_t getMillKeyPiece0(MillKey key)
{
    return (key & MILL_KEY_PIECE0_MASK) >> MILL_KEY_PIECE0_SHIFT;
}

// 读取 MillKey 中第 1 个固定位置上的棋子序号。
inline uint32_t getMillKeyPiece1(MillKey key)
{
    return (key & MILL_KEY_PIECE1_MASK) >> MILL_KEY_PIECE1_SHIFT;
}

// 读取 MillKey 中第 2 个固定位置上的棋子序号。
inline uint32_t getMillKeyPiece2(MillKey key)
{
    return (key & MILL_KEY_PIECE2_MASK) >> MILL_KEY_PIECE2_SHIFT;
}

// 读取 MillKey 对应的三连线编号。
inline uint32_t getMillKeyLineId(MillKey key)
{
    return (key & MILL_KEY_LINE_MASK) >> MILL_KEY_LINE_SHIFT;
}

// 判断该 MillKey 是否属于后手。
// 这里返回 bool，而不是 Players：
// Players 枚举是 status 专用的预移位位段值，不适合复用到 MillKey。
inline bool isMillKeyPlayer2(MillKey key)
{
    return (key & MILL_KEY_PLAYER_MASK) != 0;
}

// ==================== 棋局数据结构 ====================
// ChessData 是 NineChess 核心逻辑和 AI 共用的局面描述。
//
// 其中：
// 1. status 用于打包最常访问的小状态，便于压缩和哈希；
// 2. 主位棋盘负责描述“谁占了哪个点”；
// 3. 序号位棋盘负责描述“该点上的棋是几号棋”；
// 4. millHistory 负责记录九连棋中已经出现过的历史三连。
struct ChessData {
    // 只允许使用低 24 位。所有位棋盘读写和哈希都应先套这个 mask。
    static constexpr uint32_t VALID_BOARD_MASK = 0x00ffffffu;

    // 只有这些位段参与常规局面哈希。
    // 高位若以后扩展临时标记，不会污染当前置换表键值。
    static constexpr uint32_t HASH_STATUS_MASK =
        STATUS_PLAYER2_IN_HAND_MASK |
        STATUS_PENDING_CAPTURES_MASK |
        STATUS_PHASE_MASK |
        STATUS_ACTION_MASK |
        STATUS_TURN_MASK;

    // phase / action / turn 三个位段经常一起读取或整体覆盖，
    // 单独提成一个组合掩码后：
    // 1. 语义更清楚；
    // 2. 避免在多个函数里重复书写同一长串按位或表达式；
    // 3. 这本身也是编译期常量，不会产生额外运行时开销。
    static constexpr uint32_t STATE_MASK =
        STATUS_PHASE_MASK |
        STATUS_ACTION_MASK |
        STATUS_TURN_MASK;

    // 先手手牌数比后手少 1 的两种开局子状态。
    // 这里直接保存为预先拼好的完整状态常量，便于热路径中一次比较完成判断。
    static constexpr uint32_t PLAYER1_PLACED_ONE_MORE_STATE_1 =
        GAME_OPENING | ACTION_PLACE | PLAYER2;
    static constexpr uint32_t PLAYER1_PLACED_ONE_MORE_STATE_2 =
        GAME_OPENING | ACTION_CAPTURE | PLAYER1;

    // 默认构造时把所有核心状态清零。
    // 这样 AI 在批量创建节点时，不会拿到未初始化的脏位棋盘。
    ChessData()
        : status(0),
          player1Board(0),
          player2Board(0),
          forbiddenBoard(0),
          numberBoards{},
          millHistory()
    {
    }

    // 显式拷贝构造：保证 numberBoards 和 millHistory 被完整复制。
    ChessData(const ChessData& other)
        : status(other.status),
          player1Board(other.player1Board),
          player2Board(other.player2Board),
          forbiddenBoard(other.forbiddenBoard),
          numberBoards{},
          millHistory(other.millHistory)
    {
        for (int i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
            numberBoards[i] = other.numberBoards[i];
        }
    }

    // 显式赋值：和拷贝构造保持一致，避免以后追加成员时漏拷贝。
    ChessData& operator=(const ChessData& other)
    {
        if (this == &other)
            return *this;

        status = other.status;
        player1Board = other.player1Board;
        player2Board = other.player2Board;
        forbiddenBoard = other.forbiddenBoard;
        for (int i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
            numberBoards[i] = other.numberBoards[i];
        }
        millHistory = other.millHistory;
        return *this;
    }

    // 打包状态字段。详见上方 status 位布局说明。
    uint32_t status = 0;

    // ==================== status 访问接口 ====================
    // 这些函数全部围绕 status 工作。
    // 设计原则：
    // 1. status 是相关小状态的唯一真值；
    // 2. get 函数只做掩码/移位；
    // 3. set 函数只覆盖对应位段，不影响其它状态位。

    inline uint32_t getPlayer2InHand() const
    {
        return status & STATUS_PLAYER2_IN_HAND_MASK;
    }

    inline uint32_t getPlayer1InHand() const
    {
        const uint32_t player2InHand = getPlayer2InHand();
        // 只存后手手牌数时，先手手牌数要靠“当前阶段 + 当前动作 + 当前轮次”推导。
        //
        // 开局阶段只有“刚刚完成落子、但尚未切换轮次”的一方，会比对手少 1 手牌：
        // 1. ACTION_PLACE 且 turn == PLAYER2：
        //    说明先手刚刚落子结束，轮到后手摆子，此时先手少 1；
        // 2. ACTION_CAPTURE 且 turn == PLAYER1：
        //    说明先手刚刚成三并正处于提子流程，此时先手也少 1。
        //
        // 其余情况两边手牌数相同。
        //
        // 这里直接从 status 中一次性取出 phase/action/turn 三个位段再做精确比较，
        // 避免多次调用 getter。由于这 3 组枚举值本身就是预移位后的互斥位段值，
        // 因此可以直接按位拼成一个完整状态常量进行比较。
        const uint32_t state = status & STATE_MASK;
        const bool player1PlacedOneMore =
            state == PLAYER1_PLACED_ONE_MORE_STATE_1
            || state == PLAYER1_PLACED_ONE_MORE_STATE_2;

        return player1PlacedOneMore
            ? (player2InHand > 0u ? (player2InHand - 1u) : 0u)
            : player2InHand;
    }

    inline void setPlayer2InHand(uint32_t inHand)
    {
        status = (status & ~STATUS_PLAYER2_IN_HAND_MASK)
            | ((inHand << STATUS_PLAYER2_IN_HAND_SHIFT) & STATUS_PLAYER2_IN_HAND_MASK);
    }

    inline void decPlayer2InHand()
    {
        // 因为后手手牌数就在最低 4 位，所以合法状态下可以直接 --status。
        // 调用方需要自己保证当前值大于 0，避免借位污染高位状态。
        --status;
    }

    inline void incPlayer2InHand()
    {
        ++status;
    }

    inline uint32_t getPendingCaptures() const
    {
        return (status & STATUS_PENDING_CAPTURES_MASK) >> STATUS_PENDING_CAPTURES_SHIFT;
    }

    inline void setPendingCaptures(uint32_t pendingCaptures)
    {
        status = (status & ~STATUS_PENDING_CAPTURES_MASK)
            | ((pendingCaptures << STATUS_PENDING_CAPTURES_SHIFT) & STATUS_PENDING_CAPTURES_MASK);
    }

    inline void clearPendingCaptures()
    {
        status &= ~STATUS_PENDING_CAPTURES_MASK;
    }

    inline Phases getPhase() const
    {
        return static_cast<Phases>(status & STATUS_PHASE_MASK);
    }

    inline void setPhase(Phases phase)
    {
        status = (status & ~STATUS_PHASE_MASK) | static_cast<uint32_t>(phase);
    }

    inline Actions getAction() const
    {
        return static_cast<Actions>(status & STATUS_ACTION_MASK);
    }

    inline void setAction(Actions action)
    {
        status = (status & ~STATUS_ACTION_MASK) | static_cast<uint32_t>(action);
    }

    inline Players getTurn() const
    {
        return static_cast<Players>(status & STATUS_TURN_MASK);
    }

    inline void setTurn(Players turn)
    {
        status = (status & ~STATUS_TURN_MASK) | static_cast<uint32_t>(turn);
    }

    inline void setState(Phases phase, Actions action, Players turn)
    {
        status &= ~STATE_MASK;
        status |= static_cast<uint32_t>(phase)
            | static_cast<uint32_t>(action)
            | static_cast<uint32_t>(turn);
    }

    // 三个主位棋盘：
    // player1Board   : 先手所有棋子的位置
    // player2Board   : 后手所有棋子的位置
    // forbiddenBoard : 当前被视为禁点的位置
    uint32_t player1Board = 0;
    uint32_t player2Board = 0;
    uint32_t forbiddenBoard = 0;

    // 序号位棋盘：
    // numberBoards[i] 表示“编号 i 的棋子”在棋盘上的位置层。
    // 同编号的先后手棋子共用同一层位棋盘。
    // 因为每方每个编号最多只有 1 颗棋，所以：
    // 1. numberBoards[i] 最多只有 2 个 1；
    // 2. 并且这 2 个 1 必然分别属于先手和后手；
    // 3. 所以在 hard hash 里，可以把每层进一步压成“先手位置 + 后手位置”两个 5 bit 数。
    //
    // 这组位棋盘只在带编号的九连棋规则中使用，因此固定为 9 层。
    uint32_t numberBoards[NUMBERED_PIECE_COUNT] = {};

    // 九连棋历史三连表。
    // 使用 vector 而不是 list，原因是：
    // 1. AI 搜索会频繁复制局面，vector 拷贝更紧凑；
    // 2. hard hash 会遍历整张表，vector 的缓存局部性更好；
    // 3. 这张表在语义上更接近“历史集合”，主要操作是追加和遍历，而不是中间频繁插删。
    //
    // 这里要求调用方只追加“去重后的历史三连”。
    // 也就是说，同一个 MillKey 最多出现一次。
    std::vector<MillKey> millHistory;

    // 动态状态说明：
    // 1. player2InHand 已经打包进 status 的最低 4 位；
    // 2. player1InHand 可由“当前阶段 + 当前轮次 + player2InHand”直接推导；
    // 3. pendingCaptures 已经打包进 status 的第 4~5 位；
    // 4. 双方盘面存活子数可直接由主位棋盘做 popcount 得到。

    // 返回先手当前在盘存活子数。
    inline uint32_t getPlayer1OnBoardCount() const
    {
        return POPCOUNT32(player1Board & VALID_BOARD_MASK);
    }

    // 返回后手当前在盘存活子数。
    inline uint32_t getPlayer2OnBoardCount() const
    {
        return POPCOUNT32(player2Board & VALID_BOARD_MASK);
    }

    // ==================== 哈希接口 ====================
    // 非九连棋规则下，三个 24 位主位棋盘在同一位置上互斥：
    //   00 -> 空位
    //   01 -> 先手棋
    //   10 -> 后手棋
    //   11 -> 禁点
    // 因而整盘可压缩为 24 个 2-bit“数字”，总计 48 bit。
    //
    // getHashLite():
    //   低 48 位编码三个主位棋盘；
    //   高位拼接 status 中实际参与局面判定的有效位。
    inline uint64_t getHashLite() const
    {
        const uint32_t player1 = player1Board & VALID_BOARD_MASK;
        const uint32_t player2 = player2Board & VALID_BOARD_MASK;
        const uint32_t forbidden = forbiddenBoard & VALID_BOARD_MASK;

        // 这里使用“两平面交织”的方法，把 24 个点位各自编码成 2 bit。
        //
        // low 平面：
        //   先手或禁点 -> 1
        // high 平面：
        //   后手或禁点 -> 1
        //
        // 于是每个点位自然形成：
        //   00 空位
        //   01 先手
        //   10 后手
        //   11 禁点
        //
        // 下面这 5 步 mask/shift 不是普通移位，而是在做 bit expand：
        // 把原来的 24 个有效 bit 打散到 48 位中的偶数位上。
        // 展开后的 low/high 再一奇一偶交织，就得到最终的 48 bit 棋盘编码。
        uint64_t low = static_cast<uint64_t>((player1 | forbidden) & VALID_BOARD_MASK);
        low = (low | (low << 16)) & 0x0000FFFF0000FFFFULL;
        low = (low | (low << 8)) & 0x00FF00FF00FF00FFULL;
        low = (low | (low << 4)) & 0x0F0F0F0F0F0F0F0FULL;
        low = (low | (low << 2)) & 0x3333333333333333ULL;
        low = (low | (low << 1)) & 0x5555555555555555ULL;

        uint64_t high = static_cast<uint64_t>((player2 | forbidden) & VALID_BOARD_MASK);
        high = (high | (high << 16)) & 0x0000FFFF0000FFFFULL;
        high = (high | (high << 8)) & 0x00FF00FF00FF00FFULL;
        high = (high | (high << 4)) & 0x0F0F0F0F0F0F0F0FULL;
        high = (high | (high << 2)) & 0x3333333333333333ULL;
        high = (high | (high << 1)) & 0x5555555555555555ULL;

        return (low | (high << 1))
            | (static_cast<uint64_t>(status & HASH_STATUS_MASK) << 48);
    }

    // 九连棋专用 hard hash。
    //
    // 先说明结论：
    // 1. status + 主位棋盘 可以在 64 bit 内做得非常紧凑；
    // 2. 但再加上 9 层序号位棋盘和历史三连表后，状态空间已经远超 64 bit；
    // 3. 因此 hard hash 不再追求“严格数学上的完美哈希”，而追求：
    //    - 对真实局面有极低冲突率
    //    - 全程只用少量位运算 / 乘法 / 遍历 9 层数组与历史表
    //    - 适合 AI 搜索中的高频调用
    //
    // 具体做法：
    // 1. 先用 getHashLite() 取得 status + 主位棋盘的紧凑基哈希；
    // 2. 对每个编号层，把“先手该号棋的位置 + 后手该号棋的位置”压成 10 bit 左右的 layerCode；
    //    其中位置 0 表示“不在盘上”，1~24 表示棋盘实际点位编号 + 1；
    // 3. 对每层 layerCode 做 64 位强混合，并按层号旋转后并入总哈希；
    // 4. 对历史三连表 millHistory 做“集合语义”的无序混合：
    //    同一个历史集合，不应因 push_back 顺序不同而得到不同哈希；
    // 5. 最后再做一次 64 位 avalanche，得到最终 hard hash。
    inline uint64_t getHashHard() const
    {
        const auto mix64 = [](uint64_t x) -> uint64_t {
            x ^= x >> 30;
            x *= 0xbf58476d1ce4e5b9ULL;
            x ^= x >> 27;
            x *= 0x94d049bb133111ebULL;
            x ^= x >> 31;
            return x;
        };

        const auto rotl64 = [](uint64_t x, uint32_t shift) -> uint64_t {
            const uint32_t realShift = shift & 63u;
            return realShift == 0u ? x : ((x << realShift) | (x >> (64u - realShift)));
        };

        uint64_t hash = mix64(getHashLite() ^ 0x6a09e667f3bcc909ULL);
        const uint32_t player1 = player1Board & VALID_BOARD_MASK;
        const uint32_t player2 = player2Board & VALID_BOARD_MASK;

        for (uint32_t i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
            const uint32_t layer = numberBoards[i] & VALID_BOARD_MASK;
            const uint32_t player1Piece = player1 & layer;
            const uint32_t player2Piece = player2 & layer;

            // 每个编号层在每一方最多只有 1 颗棋，因此可以直接用“位置”而不是整张 24 位图来编码。
            // 位置编码采用：
            //   0    -> 该编号棋子当前不在盘上
            //   1~24 -> 棋盘点位 index + 1
            const uint32_t player1Pos = player1Piece == 0u ? 0u : (CTZ32(player1Piece) + 1u);
            const uint32_t player2Pos = player2Piece == 0u ? 0u : (CTZ32(player2Piece) + 1u);

            // layerCode 的信息量已经足够唯一描述“第 i 号棋的双边位置状态”：
            // bit 0-4  : 先手该编号棋的位置（0~24）
            // bit 5-9  : 后手该编号棋的位置（0~24）
            // bit 10+  : 层号 i，自带“这是几号棋”的信息
            const uint64_t layerCode =
                static_cast<uint64_t>(player1Pos)
                | (static_cast<uint64_t>(player2Pos) << 5)
                | (static_cast<uint64_t>(i) << 10);

            const uint64_t mixedLayer = mix64(layerCode + 0x9e3779b97f4a7c15ULL);
            hash ^= rotl64(mixedLayer, 5u * i + 1u);
        }

        // millHistory 在语义上是“历史集合”，不是“操作序列”。
        // 因而这里故意采用无序混合：
        // 1. 先把 size 混进去，避免异常重复元素时纯 xor 互相抵消；
        // 2. 再把每个 MillKey 经过强混合后 xor 进去；
        // 3. 每个 key 的旋转量取决于 key 自身，这样不同 key 更不容易在低位上扎堆。
        uint64_t historyHash =
            mix64(static_cast<uint64_t>(millHistory.size()) + 0xd1b54a32d192ed03ULL);
        for (const MillKey key : millHistory) {
            const uint64_t mixedMill =
                mix64(static_cast<uint64_t>(key & MILL_KEY_USED_MASK) + 0x94d049bb133111ebULL);
            historyHash ^= rotl64(mixedMill, static_cast<uint32_t>(key) & 63u);
        }

        return mix64(hash ^ rotl64(historyHash, 29u));
    }
};


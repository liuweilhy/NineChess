/****************************************************************************
** AIBenchmark - 新旧 AI 引擎对抗基准
**
** 旧引擎：2018-12-23 提交 a588f4a 的 NineChess + NineChessAi_ab（单线程），
**         原样封装在 namespace ncold 中，零算法改动。
** 新引擎：当前 NineChess/src 核心模型 + NineChess_AI_AB（Lazy SMP 多线程）。
**
** 赛制（按用户要求）：
**   - 双方每步 10 秒强制出招（超时调用引擎的 quit() 强制中断，取已完成结果）
**   - 累计 100 条走子命令未分胜负判和（与 GUI 控制层 applyStepLimit 语义一致）
**   - 旧引擎深度 8、单线程；新引擎深度 10、8 线程（其余搜索选项为默认值，
**     含动态深度/PVS/静默搜索/强制提子延伸/LMR 等"新算法"组成特性）
**   - 奇偶局交替执先；新引擎开启根节点随机（默认 rP9+g20：仅开局前 9 条命令内
**     随机、带宽 20，每局独立种子），保证 20 局样本多样性；旧引擎保持原样（自带随机排序）
**
** 用法: AIBenchmark.exe [起始规则] [结束规则] [每规则局数] [旧深度] [新深度] [限时ms]
**                      [新线程数] [纯最优0/1] [wp] [pv] [随机手数] [随机带宽]
**       wp/pv = -1 表示用引擎默认（150/16）；pureBest=1 关闭根随机；
**       随机手数 = -1 用默认 9（仅开局随机），0 = 全程随机（Console vs 的默认配置）。
** 缺省: AIBenchmark.exe 0 3 20 8 10 10000 8 0 -1 -1 -1 -1
****************************************************************************/

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "old_engine/ninechessai_ab.h"          // ncold::NineChess / ncold::NineChessAi_ab
#include "../NineChess/src/ninechess_ai_ab.h"   // ::NineChess / ::NineChess_AI_AB

using std::string;
using std::vector;

namespace {

constexpr int kRuleCount = 4;
// 判和步数：默认 100 条命令未分胜负判和。试验用命令行尾部第 14 个可选参数
// （argv[13]）可覆盖；缺省时行为与既往版本完全一致（含棋谱 "rXsNt0" 标记）。
int g_stepsLimit = 100;
constexpr int kTimeLimitMs = 10000; // 每步限时
constexpr int kWatchdogGraceMs = 15000; // 引擎超时后再等中断生效的宽限期

// 判和终局原因的规范文本：随 g_stepsLimit 变化，供摘要与棋谱判重共用，
// 避免改判和步数后摘要仍显示旧数字。
string stepsLimitReason() { return std::to_string(g_stepsLimit) + "步判和"; }

// ------------------------- 输出工具 -------------------------

void setConsoleUtf8()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
}

FILE* g_verboseLog = nullptr;

// A/B 隔离开关（命令行注入，缺省时行为与既有版本完全一致）：
//   pureBest=1 关闭新引擎根节点随机（纯最优），用于隔离 randomPlies 的影响；
//   wp/pv=-1 表示沿用引擎默认（wp=150、pv=16），>=0 时覆盖；
//   rplies/rgap 同理（-1 = 默认 9/20）；rplies=0 即“全程随机”（Console vs 默认配置）。
int g_pureBest = 0;
int g_winPressure = -1;
int g_pointValue = -1;
int g_randomPlies = -1;
int g_randomGap = -1;

// 非 pureBest 时新引擎默认的随机配置（带宽 / 开局手数）。选项与标签共用同一
// 常量，避免"改了选项忘了改标签"而再次产生错标的汇总文件。
constexpr int kDefaultRandomGap = 20;
constexpr int kDefaultRandomPlies = 9;

// 新引擎实际配置的文本描述：横幅与汇总文件共用，避免出现与实际不符的
// 硬编码标签（此前汇总文件写死 "随机gap=60"，而代码用的是 rP9+g20）。
string describeNewEngineConfig()
{
    const string wp = g_winPressure >= 0 ? std::to_string(g_winPressure) : string("默认150");
    const string pv = g_pointValue >= 0 ? std::to_string(g_pointValue) : string("默认16");
    string rnd;
    if (g_pureBest) {
        rnd = "关";
    }
    else {
        const int gap = g_randomGap >= 0 ? g_randomGap : kDefaultRandomGap;
        const int plies = g_randomPlies >= 0 ? g_randomPlies : kDefaultRandomPlies;
        rnd = plies > 0
            ? "rP" + std::to_string(plies) + "+g" + std::to_string(gap)
            : "全程+g" + std::to_string(gap);
    }
    return "随机=" + rnd + " wp=" + wp + " pv=" + pv;
}

void verbosePrintf(const char* fmt, ...)
{
    if (!g_verboseLog) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_verboseLog, fmt, args);
    va_end(args);
    fflush(g_verboseLog);
}

// ------------------------- 坐标与命令转换 -------------------------

// 旧模型 1-based (c=1..3 圈, p=1..8 位) ↔ 新模型 0-based (c=0..2, p=0..7)。
// 棋盘布局与方向两版一致（第 1 圈=外圈，位 1/0 = 上中点，顺时针），仅基准差 1。
// delta = +1: 新 → 旧; delta = -1: 旧 → 新。
bool convertCommand(const string& in, int delta, string& out)
{
    int a = 0, b = 0, c = 0, d = 0;
    char buf[64] = {};
    if (sscanf(in.c_str(), "(%d,%d)->(%d,%d)", &a, &b, &c, &d) == 4) {
        sprintf(buf, "(%d,%d)->(%d,%d)", a + delta, b + delta, c + delta, d + delta);
        out = buf;
        return true;
    }
    if (sscanf(in.c_str(), "-(%d,%d)", &a, &b) == 2) {
        sprintf(buf, "-(%d,%d)", a + delta, b + delta);
        out = buf;
        return true;
    }
    if (sscanf(in.c_str(), "(%d,%d)", &a, &b) == 2) {
        sprintf(buf, "(%d,%d)", a + delta, b + delta);
        out = buf;
        return true;
    }
    return false;
}

// ------------------------- 状态比较 -------------------------

enum class Phase { NotStarted, Opening, Mid, Over };
enum class Action { Choose, Place, Capture };
enum class Side { Nobody, P1, P2, Draw };

Phase oldPhase(const ncold::NineChess& g)
{
    switch (g.getPhase()) {
    case ncold::NineChess::GAME_OPENING: return Phase::Opening;
    case ncold::NineChess::GAME_MID: return Phase::Mid;
    case ncold::NineChess::GAME_OVER: return Phase::Over;
    default: return Phase::NotStarted;
    }
}

Phase newPhase(const NineChess& g)
{
    switch (g.getPhase()) {
    case NineChess::GAME_OPENING: return Phase::Opening;
    case NineChess::GAME_MID: return Phase::Mid;
    case NineChess::GAME_OVER: return Phase::Over;
    default: return Phase::NotStarted;
    }
}

Action oldAction(const ncold::NineChess& g)
{
    switch (g.getAction()) {
    case ncold::NineChess::ACTION_PLACE: return Action::Place;
    case ncold::NineChess::ACTION_CAPTURE: return Action::Capture;
    default: return Action::Choose;
    }
}

Action newAction(const NineChess& g)
{
    switch (g.getAction()) {
    case NineChess::ACTION_PLACE: return Action::Place;
    case NineChess::ACTION_CAPTURE: return Action::Capture;
    default: return Action::Choose;
    }
}

Side oldSide(ncold::NineChess::Players p)
{
    switch (p) {
    case ncold::NineChess::PLAYER1: return Side::P1;
    case ncold::NineChess::PLAYER2: return Side::P2;
    case ncold::NineChess::DRAW: return Side::Draw;
    default: return Side::Nobody;
    }
}

Side newSide(NineChess::Players p)
{
    switch (p) {
    case NineChess::PLAYER1: return Side::P1;
    case NineChess::PLAYER2: return Side::P2;
    case NineChess::DRAW: return Side::Draw;
    default: return Side::Nobody;
    }
}

// 把旧模型棋盘（5×8 char 数组，行 1..3 为圈）转成位棋盘与编号层。
struct OldBoardBits {
    uint32_t p1 = 0;
    uint32_t p2 = 0;
    uint32_t forbidden = 0;
    uint32_t numbers[9] = {};
};

OldBoardBits oldBoardBits(const ncold::NineChess& g, bool numbered)
{
    OldBoardBits bits;
    const char* board = g.getBoard();
    for (int pos = 0; pos < 24; ++pos) {
        const int idx = (pos / 8 + 1) * 8 + (pos % 8);
        const char ch = board[idx];
        const uint32_t bit = 1u << pos;
        if (ch & 0x10) {
            bits.p1 |= bit;
            if (numbered) bits.numbers[(ch & 0x0F) - 1] |= bit;
        } else if (ch & 0x20) {
            bits.p2 |= bit;
            if (numbered) bits.numbers[(ch & 0x0F) - 1] |= bit;
        } else if (ch == '\x0f') {
            bits.forbidden |= bit;
        }
    }
    return bits;
}

// 双模型分歧统计（不中止对局，仅记录）：以新模型为权威，允许两版模型
// 因规则实现差异（旧模型成三判定等历史 bug）出现状态漂移。
struct DivergenceStats {
    int events = 0;      // 分歧次数
    bool logged = false; // 是否已记录过详情
};

void noteDivergence(DivergenceStats& stats, const NineChess& nw, const ncold::NineChess& od,
                    const string& detail)
{
    ++stats.events;
    if (!stats.logged) {
        stats.logged = true;
        verbosePrintf("  .. 首次模型分歧: %s (turn n%d/o%d, pend n%u/o%d, board n%08X/%08X)\n",
            detail.c_str(), (int)nw.getTurn(), (int)od.whosTurn(),
            nw.getData().getPendingCaptures(), od.getNum_NeedRemove(),
            nw.getData().player1Board, nw.getData().player2Board);
    }
}

void countDivergence(DivergenceStats& stats, const NineChess& nw, const ncold::NineChess& od)
{
    char buf[128];
    const bool over = nw.getPhase() == NineChess::GAME_OVER || od.getPhase() == ncold::NineChess::GAME_OVER;
    if (over)
        return;
    const bool numbered = !nw.getRule()->allowRepeatedMills;
    const OldBoardBits bits = oldBoardBits(od, numbered);
    const auto& cd = nw.getData();
    if (bits.p1 != cd.player1Board || bits.p2 != cd.player2Board
        || bits.forbidden != cd.forbiddenBoard) {
        sprintf(buf, "棋盘不同");
        noteDivergence(stats, nw, od, buf);
        return;
    }
    if ((int)cd.getPendingCaptures() != od.getNum_NeedRemove()) {
        sprintf(buf, "待提子数不同: old=%d new=%u", od.getNum_NeedRemove(), cd.getPendingCaptures());
        noteDivergence(stats, nw, od, buf);
        return;
    }
    const bool turnSame = (od.whosTurn() == ncold::NineChess::PLAYER1) == (nw.getTurn() == NineChess::PLAYER1);
    const bool actSame = (int)nw.getAction() / 0x200 == (int)od.getAction() / 0x100;
    if (!turnSame || !actSame) {
        sprintf(buf, "轮次/动作不同");
        noteDivergence(stats, nw, od, buf);
    }
}

// ------------------------- 带看门狗的引擎调用 -------------------------

struct ThinkOutcome {
    string cmd;          // "error!" 表示引擎无着法
    bool forced = false; // 是否被看门狗强制中断
    double wallMs = 0;   // 实际耗时
    bool hung = false;   // 中断后仍未退出（异常）
};

// 在工作线程中执行 searchFn，限时 limitMs；超时调用 stopFn 强制中断，
// 再等 graceMs；仍不退出则标记 hung（正常情况不会发生）。
ThinkOutcome thinkWithTimeout(const std::function<void()>& searchFn,
                              const std::function<void()>& stopFn,
                              int limitMs, int graceMs)
{
    using clock = std::chrono::steady_clock;
    ThinkOutcome result;
    std::atomic<bool> done{ false };
    const clock::time_point start = clock::now();
    std::thread worker([&]() {
        searchFn();
        done = true;
    });
    auto elapsedMs = [&]() {
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
    };
    while (!done && elapsedMs() < limitMs)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!done) {
        result.forced = true;
        stopFn();
        while (!done && elapsedMs() < limitMs + graceMs)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    result.wallMs = (double)elapsedMs();
    if (!done)
        result.hung = true;
    worker.join();
    return result;
}

// ------------------------- 单局对弈 -------------------------

enum class GameWinner { Old, New, Draw, Error };

struct GameReport {
    int rule = 0;
    int game = 0;
    bool oldIsPlayer1 = true;
    GameWinner winner = GameWinner::Draw;
    string endReason;
    int commands = 0;
    int forcedOld = 0;
    int forcedNew = 0;
    int rejOldByNew = 0;  // 旧引擎着法被新模型拒绝的次数
    int rejNewByOld = 0;  // 新引擎着法被旧模型拒绝的次数
    int substNew = 0;     // 给新模型执行的兜底替代着法
    int substOld = 0;     // 给旧模型执行的兜底替代着法
    int divergences = 0;  // 双模型状态分歧次数
    double oldThinkTotalMs = 0;
    double newThinkTotalMs = 0;
    double oldThinkMaxMs = 0;
    double newThinkMaxMs = 0;
    int oldMoves = 0;
    int newMoves = 0;
    uint64_t newNodes = 0;
    double newDepthTotal = 0;
    int newDepthMoves = 0;
    uint64_t seed = 0;
    string recordFile;
};

const char* winnerText(GameWinner w)
{
    switch (w) {
    case GameWinner::Old: return "旧引擎胜";
    case GameWinner::New: return "新引擎胜";
    case GameWinner::Draw: return "和棋";
    default: return "异常";
    }
}

// 着法类别：0 落子 / 1 走子 / 2 提子（仅用于日志）
int categoryOf(const string& cmd)
{
    if (!cmd.empty() && cmd[0] == '-')
        return 2;
    if (cmd.find("->") != string::npos)
        return 1;
    return 0;
}

// 兜底替代：按模型当前状态尝试合法命令，返回第一个被接受的完整着法。
// 命令在尝试成功时即已执行（两版模型的非法命令都不污染局面，可放心试探）。
// 中局选子状态的完整着法是复合命令，裸 (c,p) 只是半步，必须按动作区分。
string firstLegalCommandNew(NineChess& g)
{
    char cmd[32];
    if (g.getAction() == NineChess::ACTION_CAPTURE) {
        for (int pos = 0; pos < 24; ++pos) {
            sprintf(cmd, "-(%d,%d)", pos / 8, pos % 8);
            if (g.command(cmd)) return cmd;
        }
        return string();
    }
    if (g.getPhase() == NineChess::GAME_MID && g.getAction() == NineChess::ACTION_CHOOSE) {
        for (int from = 0; from < 24; ++from)
            for (int to = 0; to < 24; ++to) {
                if (from == to) continue;
                sprintf(cmd, "(%d,%d)->(%d,%d)", from / 8, from % 8, to / 8, to % 8);
                if (g.command(cmd)) return cmd;
            }
        return string();
    }
    // 开局落子状态；若旧模型半步卡在选子后状态，裸 (c,p) 恰好是补完落子
    for (int pos = 0; pos < 24; ++pos) {
        sprintf(cmd, "(%d,%d)", pos / 8, pos % 8);
        if (g.command(cmd)) return cmd;
    }
    return string();
}

string firstLegalCommandOld(ncold::NineChess& g)
{
    char cmd[32];
    if (g.getAction() == ncold::NineChess::ACTION_CAPTURE) {
        for (int pos = 0; pos < 24; ++pos) {
            sprintf(cmd, "-(%d,%d)", pos / 8 + 1, pos % 8 + 1);
            if (g.command(cmd)) return cmd;
        }
        return string();
    }
    if (g.getPhase() == ncold::NineChess::GAME_MID && g.getAction() == ncold::NineChess::ACTION_CHOOSE) {
        for (int from = 0; from < 24; ++from)
            for (int to = 0; to < 24; ++to) {
                if (from == to) continue;
                sprintf(cmd, "(%d,%d)->(%d,%d)", from / 8 + 1, from % 8 + 1, to / 8 + 1, to % 8 + 1);
                if (g.command(cmd)) return cmd;
            }
        return string();
    }
    for (int pos = 0; pos < 24; ++pos) {
        sprintf(cmd, "(%d,%d)", pos / 8 + 1, pos % 8 + 1);
        if (g.command(cmd)) return cmd;
    }
    return string();
}

GameReport playOneGame(int rule, int game, bool oldIsPlayer1,
                       int oldDepth, int newDepth, int timeLimitMs, int newThreads,
                       const string& resultsDir)
{
    GameReport report;
    report.rule = rule;
    report.game = game;
    report.oldIsPlayer1 = oldIsPlayer1;
    report.seed = 0x9E3779B97F4A7C15ull * (uint64_t)(rule * 1000 + game * 2 + (oldIsPlayer1 ? 1 : 2)) + 12345;

    // ---------------- 初始化双模型 ----------------
    NineChess gameNew;
    gameNew.setRule((uint32_t)rule);
    gameNew.start();

    ncold::NineChess gameOld;
    gameOld.setData(&ncold::NineChess::RULES[rule], g_stepsLimit, 0);
    gameOld.start();

    // ---------------- 初始化双引擎 ----------------
    NineChess_AI_AB aiNew;
    NineChess_AI_AB::SearchOptions options;   // 全部默认值，仅覆盖赛制项
    options.timeLimitMs = timeLimitMs;
    options.threads = (uint32_t)newThreads;
    if (g_pureBest) {                         // 纯最优：关闭根节点随机
        options.randomness = 0;
    }
    else {
        options.randomness = 1;               // 第八轮：仅开局随机制造多样性
        options.randomGap = g_randomGap >= 0 ? g_randomGap : kDefaultRandomGap;
        options.randomPlies = g_randomPlies >= 0 ? g_randomPlies : kDefaultRandomPlies;
    }
    // wp < 0 必须"不赋值"才能保留引擎默认 150：SearchOptions::winPressureWeight
    // 没有 -1 哨兵语义，直接写 -1 会让该项以 -1 参与估值（见 benchmark_may.cpp 同名注释）。
    if (g_winPressure >= 0)
        options.winPressureWeight = g_winPressure; // < 0 = 保留引擎默认 150
    if (g_pointValue >= 0)
        options.pointValueWeight = g_pointValue; // 默认 16
    options.seed = report.seed;
    aiNew.setOptions(options);
    aiNew.clearTranspositionTable();          // 每局清空，与 console vs 一致

    ncold::NineChessAi_ab aiOld;

    string sync;   // 保留变量名兼容（未再使用）
    string record; // 新模型 0-based 命令流（权威棋局）
    DivergenceStats divStats;
    (void)sync;

    verbosePrintf("=== 规则%d 第%d局 旧引擎执%s ===\n",
        rule, game + 1, oldIsPlayer1 ? "先手(P1)" : "后手(P2)");

    // 以新模型为权威棋局判定胜负与步数；两版模型各自演进，
    // 交叉走子被对方模型拒绝时，用对方模型当前第一合法着法兜底替代，
    // 保证双方模型每步都前进、对局总能完整结束（用户约定：不纠结旧模型细节）。
    while (true) {
        // ---- 终局判断（权威 = 新模型）----
        if (gameNew.whoWin() != NineChess::NOBODY) {
            const NineChess::Players w = gameNew.whoWin();
            report.endReason = "规则终局";
            if (w == NineChess::DRAW)
                report.winner = GameWinner::Draw;
            else {
                const bool p1Won = (w == NineChess::PLAYER1);
                report.winner = (p1Won == oldIsPlayer1) ? GameWinner::Old : GameWinner::New;
            }
            break;
        }
        // ---- g_stepsLimit 条命令判和（与 GUI applyStepLimit 语义一致）----
        if ((int)gameNew.getCmdList()->size() >= g_stepsLimit) {
            gameNew.command("=="); // 棋谱补记判和命令（旧模型不支持，无需同步）
            report.endReason = stepsLimitReason();
            report.winner = GameWinner::Draw;
            break;
        }
        // ---- 保险丝：正常情况下 g_stepsLimit 步内必终局 ----
        if (report.commands >= 4 * g_stepsLimit) {
            report.endReason = "步数保险丝触发，判和";
            report.winner = GameWinner::Draw;
            break;
        }

        // ---- 确定当前该哪个引擎走（权威 = 新模型轮次）----
        const bool p1ToMove = (gameNew.getTurn() == NineChess::PLAYER1);
        const bool oldToMove = (p1ToMove == oldIsPlayer1);
        string cmdFromEngine; // 引擎原生坐标（旧 1-based / 新 0-based）

        if (oldToMove) {
            aiOld.setChess(gameOld);
            ThinkOutcome think = thinkWithTimeout(
                [&]() { aiOld.alphaBetaPruning(oldDepth); },
                [&]() { aiOld.quit(); },
                timeLimitMs, kWatchdogGraceMs);
            if (think.hung) {
                report.endReason = "旧引擎搜索无法中断（异常终止）";
                report.winner = GameWinner::Error;
                break;
            }
            report.oldThinkTotalMs += think.wallMs;
            report.oldThinkMaxMs = report.oldThinkMaxMs > think.wallMs ? report.oldThinkMaxMs : think.wallMs;
            report.forcedOld += think.forced ? 1 : 0;
            ++report.oldMoves;
            cmdFromEngine = aiOld.bestMove();
            verbosePrintf("  [%3d] 旧引擎(P%d) %8.0fms%s 着法=%s\n",
                report.commands + 1, p1ToMove ? 1 : 2, think.wallMs,
                think.forced ? "(强制)" : "       ", cmdFromEngine.c_str());
            if (cmdFromEngine == "error!") {
                report.endReason = "旧引擎无法出招（其模型已终局或卡死，按认输计）";
                report.winner = GameWinner::New;
                break;
            }
            // 旧引擎着法对其自身模型必然合法；先应用到旧模型
            if (!gameOld.command(cmdFromEngine.c_str())) {
                report.endReason = "旧引擎着法被自身模型拒绝（按认输计）";
                report.winner = GameWinner::New;
                break;
            }
            // 再喂给权威模型
            string converted;
            if (!convertCommand(cmdFromEngine, -1, converted)) {
                report.endReason = "旧引擎着法无法解析";
                report.winner = GameWinner::Error;
                break;
            }
            string appliedInNew; // 权威模型实际执行的着法（0-based）
            if (gameNew.command(converted.c_str())) {
                appliedInNew = converted;
            } else {
                // 权威模型拒绝：用权威模型当前第一合法着法兜底（内部已执行）
                ++report.rejOldByNew;
                appliedInNew = firstLegalCommandNew(gameNew);
                if (!appliedInNew.empty()) {
                    ++report.substNew;
                    verbosePrintf("  .. 新模型拒绝 %s，替代执行 %s\n", converted.c_str(), appliedInNew.c_str());
                } else {
                    verbosePrintf("  .. 新模型拒绝 %s 且无替代着法，本步跳过\n", converted.c_str());
                }
            }
            if (!appliedInNew.empty()) {
                record += appliedInNew;
                record += "\n";
                ++report.commands;
            }
        } else {
            aiNew.setChess(gameNew);
            ThinkOutcome think = thinkWithTimeout(
                [&]() { aiNew.alphaBetaPruning(newDepth); },
                [&]() { aiNew.quit(); },
                timeLimitMs + 5000, kWatchdogGraceMs); // 新引擎自带时间预算，+5s 宽限仅防挂死
            if (think.hung) {
                report.endReason = "新引擎搜索无法中断（异常终止）";
                report.winner = GameWinner::Error;
                break;
            }
            report.newThinkTotalMs += think.wallMs;
            report.newThinkMaxMs = report.newThinkMaxMs > think.wallMs ? report.newThinkMaxMs : think.wallMs;
            report.forcedNew += think.forced ? 1 : 0;
            ++report.newMoves;
            NineChess_AI_AB::SearchStats stats;
            aiNew.snapshotStats(stats);
            report.newNodes += stats.nodes.load();
            report.newDepthTotal += (double)aiNew.getLastCompletedDepth();
            ++report.newDepthMoves;
            cmdFromEngine = aiNew.bestMove();
            verbosePrintf("  [%3d] 新引擎(P%d) %8.0fms%s 深度=%d 节点=%llu 着法=%s\n",
                report.commands + 1, p1ToMove ? 1 : 2, think.wallMs,
                think.forced ? "(看门狗)" : "         ", aiNew.getLastCompletedDepth(),
                (unsigned long long)stats.nodes.load(), cmdFromEngine.c_str());
            if (cmdFromEngine == "error!") {
                report.endReason = "新引擎无着法可返回（按认输计）";
                report.winner = GameWinner::Old;
                break;
            }
            // 新引擎着法对权威模型必然合法
            if (!gameNew.command(cmdFromEngine.c_str())) {
                report.endReason = "新引擎着法被权威模型拒绝（异常）";
                report.winner = GameWinner::Old;
                break;
            }
            record += cmdFromEngine;
            record += "\n";
            ++report.commands;
            // 同步旧模型
            string converted;
            if (!convertCommand(cmdFromEngine, +1, converted)) {
                report.endReason = "新引擎着法无法解析";
                report.winner = GameWinner::Error;
                break;
            }
            if (!gameOld.command(converted.c_str())) {
                ++report.rejNewByOld;
                const string subst = firstLegalCommandOld(gameOld); // 内部已执行
                if (!subst.empty()) {
                    ++report.substOld;
                    verbosePrintf("  .. 旧模型拒绝 %s，替代执行 %s\n", converted.c_str(), subst.c_str());
                } else {
                    verbosePrintf("  .. 旧模型拒绝 %s 且无替代着法，旧模型本步未前进\n", converted.c_str());
                }
            }
        }

        // ---- 双模型分歧统计（不中止）----
        countDivergence(divStats, gameNew, gameOld);
    }
    report.divergences = divStats.events;

    if (report.winner == GameWinner::Error) {
        verbosePrintf("  !! 异常: %s\n", report.endReason.c_str());
    }

    // ---------------- 保存棋谱（新模型 0-based 命令流，可回放）----------------
    const char* seatTag = oldIsPlayer1 ? "oldP1" : "newP1";
    const char* resultTag = report.winner == GameWinner::Old ? "oldwin"
        : report.winner == GameWinner::New ? "newwin"
        : report.winner == GameWinner::Draw ? "draw" : "error";
    char fname[MAX_PATH];
    sprintf(fname, "%s\\rule%d_g%02d_%s_%s.txt", resultsDir.c_str(), rule, game + 1, seatTag, resultTag);
    FILE* fp = nullptr;
    if (fopen_s(&fp, fname, "wb") == 0 && fp) {
        fprintf(fp, "r%ds%dt0\n", rule, g_stepsLimit);
        fputs(record.c_str(), fp);
        if (report.winner == GameWinner::Draw && report.endReason == stepsLimitReason())
            fprintf(fp, "==\n");
        fclose(fp);
        report.recordFile = fname;
    }
    return report;
}

} // namespace

int main(int argc, char* argv[])
{
    setConsoleUtf8();

    int ruleFirst = 0, ruleLast = 3, gamesPerRule = 20;
    int oldDepth = 8, newDepth = 10, timeLimitMs = kTimeLimitMs, newThreads = 8;
    if (argc >= 2) ruleFirst = atoi(argv[1]);
    if (argc >= 3) ruleLast = atoi(argv[2]);
    if (argc >= 4) gamesPerRule = atoi(argv[3]);
    if (argc >= 5) oldDepth = atoi(argv[4]);
    if (argc >= 6) newDepth = atoi(argv[5]);
    if (argc >= 7) timeLimitMs = atoi(argv[6]);
    if (argc >= 8) newThreads = atoi(argv[7]);
    if (argc >= 9) g_pureBest = atoi(argv[8]);
    if (argc >= 10) g_winPressure = atoi(argv[9]);
    if (argc >= 11) g_pointValue = atoi(argv[10]);
    if (argc >= 12) g_randomPlies = atoi(argv[11]);
    if (argc >= 13) g_randomGap = atoi(argv[12]);
    if (argc >= 14) g_stepsLimit = atoi(argv[13]);   // 判和步数（试验用，默认 100）

    // ---- 建输出目录 ----
    CreateDirectoryA("results", nullptr);
    const string resultsDir = "results";
    {
        char tag[64];
        __time64_t now = _time64(nullptr);
        tm tmut{};
        localtime_s(&tmut, &now);
        strftime(tag, sizeof(tag), "%Y%m%d_%H%M%S", &tmut);
        char vname[MAX_PATH];
        sprintf(vname, "results\\verbose_%s.log", tag);
        fopen_s(&g_verboseLog, vname, "wb");
    }

    printf("AIBenchmark: 旧引擎(深度%d, 单线程) vs 新引擎(深度%d, %d线程, %s)\n",
        oldDepth, newDepth, newThreads, describeNewEngineConfig().c_str());
    printf("赛制: 每步%dms强制出招, %d条命令未分胜负判和, 奇偶局交替执先\n\n",
        timeLimitMs, g_stepsLimit);
    verbosePrintf("AIBenchmark 旧深度=%d 新深度=%d 线程=%d 限时=%dms 局数=%d/规则 新引擎配置: %s\n",
        oldDepth, newDepth, newThreads, timeLimitMs, gamesPerRule,
        describeNewEngineConfig().c_str());

    int totalOld = 0, totalNew = 0, totalDraw = 0, totalError = 0;
    const auto runStart = std::chrono::steady_clock::now();

    for (int rule = ruleFirst; rule <= ruleLast; ++rule) {
        int oldWins = 0, newWins = 0, draws = 0, errors = 0;
        int forcedOldSum = 0, forcedNewSum = 0;
        int rejOldSum = 0, rejNewSum = 0, substNewSum = 0, substOldSum = 0, divSum = 0;
        double oldMsSum = 0, newMsSum = 0;
        int oldMoveSum = 0, newMoveSum = 0;
        int cmdSum = 0;
        uint64_t nodeSum = 0;
        double depthSum = 0;
        int depthMoveSum = 0;
        const auto ruleStart = std::chrono::steady_clock::now();

        printf("======== 规则 %d ========\n", rule);
        for (int g = 0; g < gamesPerRule; ++g) {
            const bool oldIsPlayer1 = (g % 2 == 0);
            GameReport r = playOneGame(rule, g, oldIsPlayer1, oldDepth, newDepth,
                timeLimitMs, newThreads, resultsDir);
            switch (r.winner) {
            case GameWinner::Old: ++oldWins; break;
            case GameWinner::New: ++newWins; break;
            case GameWinner::Draw: ++draws; break;
            default: ++errors; break;
            }
            forcedOldSum += r.forcedOld;
            forcedNewSum += r.forcedNew;
            rejOldSum += r.rejOldByNew;
            rejNewSum += r.rejNewByOld;
            substNewSum += r.substNew;
            substOldSum += r.substOld;
            divSum += r.divergences;
            oldMsSum += r.oldThinkTotalMs;
            newMsSum += r.newThinkTotalMs;
            oldMoveSum += r.oldMoves;
            newMoveSum += r.newMoves;
            cmdSum += r.commands;
            nodeSum += r.newNodes;
            depthSum += r.newDepthTotal;
            depthMoveSum += r.newDepthMoves;

            char line[384];
            sprintf(line, "  第%2d局 旧引擎执%-4s %-6s %-28s 步数=%3d 强制:旧%d/新%d 拒绝:旧%d/新%d 替代:新%d/旧%d 分歧%d 旧均时%.1fs 新均时%.1fs\n",
                g + 1, oldIsPlayer1 ? "先手" : "后手", winnerText(r.winner),
                r.endReason.c_str(), r.commands, r.forcedOld, r.forcedNew,
                r.rejOldByNew, r.rejNewByOld, r.substNew, r.substOld, r.divergences,
                r.oldMoves ? r.oldThinkTotalMs / r.oldMoves / 1000.0 : 0.0,
                r.newMoves ? r.newThinkTotalMs / r.newMoves / 1000.0 : 0.0);
            printf("%s", line);
            verbosePrintf("-- 第%d局结束: %s (%s) 命令=%d 棋谱=%s\n",
                g + 1, winnerText(r.winner), r.endReason.c_str(), r.commands, r.recordFile.c_str());
        }

        const double ruleMin = std::chrono::duration<double>(std::chrono::steady_clock::now() - ruleStart).count() / 60.0;
        printf("---- 规则%d汇总: 旧%d胜 新%d胜 和%d 异常%d | 平均%.1f命令/局 | "
               "强制出招: 旧%d次 新%d次 | 拒绝: 旧着法%d次 新着法%d次 | 替代%d/%d | 分歧%d | "
               "旧均时%.2fs 新均时%.2fs | 新引擎平均完成深度%.1f | 节点%.1fM | 用时%.1f分钟\n\n",
            rule, oldWins, newWins, draws, errors,
            gamesPerRule ? (double)cmdSum / gamesPerRule : 0.0,
            forcedOldSum, forcedNewSum,
            rejOldSum, rejNewSum, substNewSum, substOldSum, divSum,
            oldMoveSum ? oldMsSum / oldMoveSum / 1000.0 : 0.0,
            newMoveSum ? newMsSum / newMoveSum / 1000.0 : 0.0,
            depthMoveSum ? depthSum / depthMoveSum : 0.0,
            nodeSum / 1e6, ruleMin);

        totalOld += oldWins; totalNew += newWins;
        totalDraw += draws; totalError += errors;
    }

    const double totalMin = std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart).count() / 60.0;
    printf("======== 总计 ========\n旧引擎 %d 胜 | 新引擎 %d 胜 | 和棋 %d | 异常 %d | 总用时 %.1f 分钟\n",
        totalOld, totalNew, totalDraw, totalError, totalMin);

    // ---- 汇总文件 ----
    {
        char sname[MAX_PATH];
        __time64_t now = _time64(nullptr);
        tm tmut{};
        localtime_s(&tmut, &now);
        char tag[64];
        strftime(tag, sizeof(tag), "%Y%m%d_%H%M%S", &tmut);
        sprintf(sname, "results\\summary_%s.txt", tag);
        FILE* fp = nullptr;
        if (fopen_s(&fp, sname, "wb") == 0 && fp) {
            fprintf(fp, "旧引擎(2018 a588f4a, 单线程, 深度%d) vs 新引擎(当前, %d线程, 深度%d, %s)\n",
                oldDepth, newThreads, newDepth, describeNewEngineConfig().c_str());
            fprintf(fp, "赛制: 每步%dms强制出招, %d条命令判和, 奇偶局交替执先\n",
                timeLimitMs, g_stepsLimit);
            fprintf(fp, "总计: 旧%d胜 新%d胜 和%d 异常%d, 用时%.1f分钟\n",
                totalOld, totalNew, totalDraw, totalError, totalMin);
            fclose(fp);
            printf("汇总已保存: %s\n", sname);
        }
    }
    if (g_verboseLog) fclose(g_verboseLog);
    return 0;
}

/****************************************************************************
** AIBenchmarkMay - 2026年5月版引擎 vs 当前最新引擎 对抗基准
**
** 5月引擎：提交 d78811a（代码与 2026-04-26"算法优化更新"一致），
**          原样封装在 namespace ncmay 中（仅宏重命名 + 命名空间隔离，零算法改动）。
**          确认单线程（源码无 std::thread），无 SearchOptions（纯深度搜索）。
** 新引擎：当前 NineChess/src 核心模型 + NineChess_AI_AB（Lazy SMP 多线程 + randomPlies）。
**
** 赛制：每步 10 秒强制出招；100 条走子命令未分胜负判和；奇偶局交替执先。
** 两版模型同为 0-based 位棋盘实现，命令流直接互通，无需坐标转换。
**
** 用法: AIBenchmarkMay.exe [起始规则] [结束规则] [每规则局数] [5月深度] [新深度] [限时ms] [新线程数]
****************************************************************************/

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "may_engine/ninechess_ai_ab.h"         // ncmay::NineChess / ncmay::NineChess_AI_AB
#include "../NineChess/src/ninechess_ai_ab.h"   // ::NineChess / ::NineChess_AI_AB

using std::string;
using std::vector;

namespace {

// 判和步数：默认 100 条命令未分胜负判和。试验用命令行尾部第 12 个可选参数
// （argv[11]）可覆盖；缺省时行为与既往版本完全一致（含棋谱 "rXsNt0" 标记）。
int g_stepsLimit = 100;

// 判和终局原因的规范文本：随 g_stepsLimit 变化，供摘要与棋谱判重共用，
// 避免改判和步数后摘要仍显示旧数字。
string stepsLimitReason() { return std::to_string(g_stepsLimit) + "步判和"; }

// A/B 隔离开关（命令行注入）
int g_pureBest = 0;
int g_winPressure = -1;   // -1 = 引擎默认 150
int g_pointValue = -1;    // -1 = 引擎默认 16

// 非 pureBest 时新引擎使用的随机配置。此处与 describeNewEngineConfig() 共用，
// 避免两处各自写死数字而再次出现"标签与实际不符"。
constexpr int kRandomGap = 30;
constexpr int kRandomPlies = 10;

FILE* g_verboseLog = nullptr;

// 新引擎实际配置的文本描述：横幅、逐手日志与汇总文件共用。
// 此前汇总文件里写死过 "randomPlies=10+gap=30"，与 pureBest=1（关随机）的运行
// 不符，导致 evalab_A/B/C 的 summary 全部错标；改为按实际开关生成。
string describeNewEngineConfig()
{
    const string wp = g_winPressure >= 0 ? std::to_string(g_winPressure) : string("默认150");
    const string pv = g_pointValue >= 0 ? std::to_string(g_pointValue) : string("默认16");
    const string rnd = g_pureBest
        ? string("关")
        : "rP" + std::to_string(kRandomPlies) + "+g" + std::to_string(kRandomGap);
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

// ------------------------- 带看门狗的引擎调用 -------------------------

struct ThinkOutcome {
    string cmd;
    bool forced = false;
    double wallMs = 0;
    bool hung = false;
};

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

// ------------------------- 分歧统计（不中止） -------------------------

struct DivergenceStats {
    int events = 0;
    bool logged = false;
};

void noteDivergence(DivergenceStats& stats, const NineChess& nw, const ncmay::NineChess& my,
                    const string& detail)
{
    ++stats.events;
    if (!stats.logged) {
        stats.logged = true;
        verbosePrintf("  .. 首次模型分歧: %s (turn n%d/m%d, pend n%u/m%u, board n%08X/m%08X)\n",
            detail.c_str(), (int)nw.getTurn(), (int)my.getTurn(),
            nw.getData().getPendingCaptures(), my.getData().getPendingCaptures(),
            nw.getData().player1Board, my.getData().player1Board);
    }
}

void countDivergence(DivergenceStats& stats, const NineChess& nw, const ncmay::NineChess& my)
{
    char buf[128];
    const bool over = nw.getPhase() == NineChess::GAME_OVER || my.getPhase() == ncmay::NineChess::GAME_OVER;
    if (over)
        return;
    const bool numbered = !nw.getRule()->allowRepeatedMills;
    const auto& cdN = nw.getData();
    const auto& cdM = my.getData();
    if (cdN.player1Board != cdM.player1Board || cdN.player2Board != cdM.player2Board
        || cdN.forbiddenBoard != cdM.forbiddenBoard) {
        sprintf(buf, "棋盘不同");
        noteDivergence(stats, nw, my, buf);
        return;
    }
    if (numbered) {
        for (int i = 0; i < NUMBERED_PIECE_COUNT; ++i) {
            if (cdN.numberBoards[i] != cdM.numberBoards[i]) {
                sprintf(buf, "编号层 %d 不同", i + 1);
                noteDivergence(stats, nw, my, buf);
                return;
            }
        }
    }
    if (cdN.getPendingCaptures() != cdM.getPendingCaptures()) {
        sprintf(buf, "待提子数不同: may=%u new=%u", cdM.getPendingCaptures(), cdN.getPendingCaptures());
        noteDivergence(stats, nw, my, buf);
        return;
    }
    const bool turnSame = (my.getTurn() == ncmay::NineChess::PLAYER1) == (nw.getTurn() == NineChess::PLAYER1);
    const bool phaseSame = (my.getPhase() == ncmay::NineChess::GAME_MID) == (nw.getPhase() == NineChess::GAME_MID)
        && (my.getPhase() == ncmay::NineChess::GAME_OPENING) == (nw.getPhase() == NineChess::GAME_OPENING);
    const bool actSame = (my.getAction() == ncmay::NineChess::ACTION_CAPTURE) == (nw.getAction() == NineChess::ACTION_CAPTURE)
        && (my.getAction() == ncmay::NineChess::ACTION_CHOOSE) == (nw.getAction() == NineChess::ACTION_CHOOSE);
    if (!turnSame || !phaseSame || !actSame) {
        sprintf(buf, "轮次/阶段/动作不同");
        noteDivergence(stats, nw, my, buf);
    }
}

// ------------------------- 兜底替代着法 -------------------------

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
    for (int pos = 0; pos < 24; ++pos) {
        sprintf(cmd, "(%d,%d)", pos / 8, pos % 8);
        if (g.command(cmd)) return cmd;
    }
    return string();
}

string firstLegalCommandMay(ncmay::NineChess& g)
{
    char cmd[32];
    if (g.getAction() == ncmay::NineChess::ACTION_CAPTURE) {
        for (int pos = 0; pos < 24; ++pos) {
            sprintf(cmd, "-(%d,%d)", pos / 8, pos % 8);
            if (g.command(cmd)) return cmd;
        }
        return string();
    }
    if (g.getPhase() == ncmay::NineChess::GAME_MID && g.getAction() == ncmay::NineChess::ACTION_CHOOSE) {
        for (int from = 0; from < 24; ++from)
            for (int to = 0; to < 24; ++to) {
                if (from == to) continue;
                sprintf(cmd, "(%d,%d)->(%d,%d)", from / 8, from % 8, to / 8, to % 8);
                if (g.command(cmd)) return cmd;
            }
        return string();
    }
    for (int pos = 0; pos < 24; ++pos) {
        sprintf(cmd, "(%d,%d)", pos / 8, pos % 8);
        if (g.command(cmd)) return cmd;
    }
    return string();
}

// ------------------------- 单局对弈 -------------------------

enum class GameWinner { May, New, Draw, Error };

struct GameReport {
    int rule = 0;
    int game = 0;
    bool mayIsPlayer1 = true;
    GameWinner winner = GameWinner::Draw;
    string endReason;
    int commands = 0;
    int forcedMay = 0;
    int forcedNew = 0;
    int rejMayByNew = 0;
    int rejNewByMay = 0;
    int substNew = 0;
    int substMay = 0;
    int divergences = 0;
    double mayThinkTotalMs = 0;
    double newThinkTotalMs = 0;
    double mayThinkMaxMs = 0;
    double newThinkMaxMs = 0;
    int mayMoves = 0;
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
    case GameWinner::May: return "5月引擎胜";
    case GameWinner::New: return "新引擎胜";
    case GameWinner::Draw: return "和棋";
    default: return "异常";
    }
}

GameReport playOneGame(int rule, int game, bool mayIsPlayer1,
                       int mayDepth, int newDepth, int timeLimitMs, int newThreads,
                       const string& resultsDir)
{
    GameReport report;
    report.rule = rule;
    report.game = game;
    report.mayIsPlayer1 = mayIsPlayer1;
    report.seed = 0x9E3779B97F4A7C15ull * (uint64_t)(rule * 1000 + game * 2 + (mayIsPlayer1 ? 1 : 2)) + 777;

    NineChess gameNew;
    gameNew.setRule((uint32_t)rule);
    gameNew.start();

    ncmay::NineChess gameMay;
    gameMay.setRule((uint32_t)rule);
    gameMay.start();

    NineChess_AI_AB aiNew;
    NineChess_AI_AB::SearchOptions options;
    options.timeLimitMs = timeLimitMs;
    options.threads = (uint32_t)newThreads;
    // A/B 开关由命令行注入（g_pureBest / g_winPressure / g_pointValue），
    // 用于隔离 randomPlies 与新增评估项的强弱影响
    if (g_pureBest)
        options.randomness = 0;
    else {
        options.randomness = 1;
        options.randomGap = kRandomGap;
        options.randomPlies = kRandomPlies;
    }
    // wp < 0 必须"不赋值"才能保留引擎默认 150：SearchOptions::winPressureWeight
    // 没有 -1 哨兵语义（不像 stalematePressure/forkThreat/millSafety 由
    // refreshWeights 用 >=0 判断取默认），直接把 -1 写进去会让该项以 -1 参与估值。
    if (g_winPressure >= 0)
        options.winPressureWeight = g_winPressure; // < 0 = 保留引擎默认 150
    if (g_pointValue >= 0)
        options.pointValueWeight = g_pointValue; // 默认 16
    options.seed = report.seed;
    aiNew.setOptions(options);
    aiNew.clearTranspositionTable();

    ncmay::NineChess_AI_AB aiMay;

    string record;
    DivergenceStats divStats;

    verbosePrintf("=== 规则%d 第%d局 5月引擎执%s ===\n",
        rule, game + 1, mayIsPlayer1 ? "先手(P1)" : "后手(P2)");

    while (true) {
        if (gameNew.whoWin() != NineChess::NOBODY) {
            const NineChess::Players w = gameNew.whoWin();
            report.endReason = "规则终局";
            if (w == NineChess::DRAW)
                report.winner = GameWinner::Draw;
            else {
                const bool p1Won = (w == NineChess::PLAYER1);
                report.winner = (p1Won == mayIsPlayer1) ? GameWinner::May : GameWinner::New;
            }
            break;
        }
        if ((int)gameNew.getCmdList()->size() >= g_stepsLimit) {
            gameNew.command("==");
            report.endReason = stepsLimitReason();
            report.winner = GameWinner::Draw;
            break;
        }
        if (report.commands >= 4 * g_stepsLimit) {
            report.endReason = "步数保险丝触发，判和";
            report.winner = GameWinner::Draw;
            break;
        }

        const bool p1ToMove = (gameNew.getTurn() == NineChess::PLAYER1);
        const bool mayToMove = (p1ToMove == mayIsPlayer1);

        if (mayToMove) {
            aiMay.setChess(gameMay);
            ThinkOutcome think = thinkWithTimeout(
                [&]() { aiMay.alphaBetaPruning(mayDepth); },
                [&]() { aiMay.quit(); },
                timeLimitMs, 15000);
            if (think.hung) {
                report.endReason = "5月引擎搜索无法中断（异常终止）";
                report.winner = GameWinner::Error;
                break;
            }
            report.mayThinkTotalMs += think.wallMs;
            report.mayThinkMaxMs = report.mayThinkMaxMs > think.wallMs ? report.mayThinkMaxMs : think.wallMs;
            report.forcedMay += think.forced ? 1 : 0;
            ++report.mayMoves;
            const string cmd = aiMay.bestMove();
            verbosePrintf("  [%3d] 5月引擎(P%d) %8.0fms%s 着法=%s\n",
                report.commands + 1, p1ToMove ? 1 : 2, think.wallMs,
                think.forced ? "(强制)" : "       ", cmd.c_str());
            if (cmd == "error!") {
                report.endReason = "5月引擎无法出招（其模型已终局或卡死，按认输计）";
                report.winner = GameWinner::New;
                break;
            }
            if (!gameMay.command(cmd.c_str())) {
                report.endReason = "5月引擎着法被自身模型拒绝（按认输计）";
                report.winner = GameWinner::New;
                break;
            }
            // 两版命令流同为 0-based，直接互喂
            string appliedInNew;
            if (gameNew.command(cmd.c_str())) {
                appliedInNew = cmd;
            } else {
                ++report.rejMayByNew;
                appliedInNew = firstLegalCommandNew(gameNew);
                if (!appliedInNew.empty()) {
                    ++report.substNew;
                    verbosePrintf("  .. 新模型拒绝 %s，替代执行 %s\n", cmd.c_str(), appliedInNew.c_str());
                } else {
                    verbosePrintf("  .. 新模型拒绝 %s 且无替代着法，本步跳过\n", cmd.c_str());
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
                timeLimitMs + 5000, 15000);
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
            const string cmd = aiNew.bestMove();
            verbosePrintf("  [%3d] 新引擎(P%d) %8.0fms%s 深度=%d 节点=%llu 着法=%s\n",
                report.commands + 1, p1ToMove ? 1 : 2, think.wallMs,
                think.forced ? "(看门狗)" : "         ", aiNew.getLastCompletedDepth(),
                (unsigned long long)stats.nodes.load(), cmd.c_str());
            if (cmd == "error!") {
                report.endReason = "新引擎无着法可返回（按认输计）";
                report.winner = GameWinner::May;
                break;
            }
            if (!gameNew.command(cmd.c_str())) {
                report.endReason = "新引擎着法被权威模型拒绝（异常）";
                report.winner = GameWinner::May;
                break;
            }
            record += cmd;
            record += "\n";
            ++report.commands;
            if (!gameMay.command(cmd.c_str())) {
                ++report.rejNewByMay;
                const string subst = firstLegalCommandMay(gameMay);
                if (!subst.empty()) {
                    ++report.substMay;
                    verbosePrintf("  .. 5月模型拒绝 %s，替代执行 %s\n", cmd.c_str(), subst.c_str());
                } else {
                    verbosePrintf("  .. 5月模型拒绝 %s 且无替代着法，5月模型本步未前进\n", cmd.c_str());
                }
            }
        }

        countDivergence(divStats, gameNew, gameMay);
    }
    report.divergences = divStats.events;

    if (report.winner == GameWinner::Error) {
        verbosePrintf("  !! 异常: %s\n", report.endReason.c_str());
    }

    const char* seatTag = mayIsPlayer1 ? "mayP1" : "newP1";
    const char* resultTag = report.winner == GameWinner::May ? "maywin"
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
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    int ruleFirst = 0, ruleLast = 3, gamesPerRule = 20;
    int mayDepth = 10, newDepth = 10, timeLimitMs = 10000, newThreads = 8;
    if (argc >= 2) ruleFirst = atoi(argv[1]);
    if (argc >= 3) ruleLast = atoi(argv[2]);
    if (argc >= 4) gamesPerRule = atoi(argv[3]);
    if (argc >= 5) mayDepth = atoi(argv[4]);
    if (argc >= 6) newDepth = atoi(argv[5]);
    if (argc >= 7) timeLimitMs = atoi(argv[6]);
    if (argc >= 8) newThreads = atoi(argv[7]);
    if (argc >= 9) g_pureBest = atoi(argv[8]);
    if (argc >= 10) g_winPressure = atoi(argv[9]);
    if (argc >= 11) g_pointValue = atoi(argv[10]);
    if (argc >= 12) g_stepsLimit = atoi(argv[11]);   // 判和步数（试验用，默认 100）

    CreateDirectoryA("results_may", nullptr);
    const string resultsDir = "results_may";
    {
        char tag[64];
        __time64_t now = _time64(nullptr);
        tm tmut{};
        localtime_s(&tmut, &now);
        strftime(tag, sizeof(tag), "%Y%m%d_%H%M%S", &tmut);
        char vname[MAX_PATH];
        sprintf(vname, "results_may\\verbose_%s.log", tag);
        fopen_s(&g_verboseLog, vname, "wb");
    }

    printf("AIBenchmarkMay: 5月引擎(d78811a, 深度%d, 单线程) vs 新引擎(深度%d, %d线程, %s)\n",
        mayDepth, newDepth, newThreads, describeNewEngineConfig().c_str());
    printf("赛制: 每步%dms强制出招, %d条命令未分胜负判和, 奇偶局交替执先\n\n",
        timeLimitMs, g_stepsLimit);
    verbosePrintf("AIBenchmarkMay 5月深度=%d 新深度=%d 线程=%d 限时=%dms 局数=%d/规则 新引擎配置: %s\n",
        mayDepth, newDepth, newThreads, timeLimitMs, gamesPerRule,
        describeNewEngineConfig().c_str());

    int totalMay = 0, totalNew = 0, totalDraw = 0, totalError = 0;
    const auto runStart = std::chrono::steady_clock::now();

    for (int rule = ruleFirst; rule <= ruleLast; ++rule) {
        int mayWins = 0, newWins = 0, draws = 0, errors = 0;
        int forcedMaySum = 0, forcedNewSum = 0;
        int rejMaySum = 0, rejNewSum = 0, substNewSum = 0, substMaySum = 0, divSum = 0;
        double mayMsSum = 0, newMsSum = 0;
        int mayMoveSum = 0, newMoveSum = 0;
        int cmdSum = 0;
        uint64_t nodeSum = 0;
        double depthSum = 0;
        int depthMoveSum = 0;
        const auto ruleStart = std::chrono::steady_clock::now();

        printf("======== 规则 %d ========\n", rule);
        for (int g = 0; g < gamesPerRule; ++g) {
            const bool mayIsPlayer1 = (g % 2 == 0);
            GameReport r = playOneGame(rule, g, mayIsPlayer1, mayDepth, newDepth,
                timeLimitMs, newThreads, resultsDir);
            switch (r.winner) {
            case GameWinner::May: ++mayWins; break;
            case GameWinner::New: ++newWins; break;
            case GameWinner::Draw: ++draws; break;
            default: ++errors; break;
            }
            forcedMaySum += r.forcedMay;
            forcedNewSum += r.forcedNew;
            rejMaySum += r.rejMayByNew;
            rejNewSum += r.rejNewByMay;
            substNewSum += r.substNew;
            substMaySum += r.substMay;
            divSum += r.divergences;
            mayMsSum += r.mayThinkTotalMs;
            newMsSum += r.newThinkTotalMs;
            mayMoveSum += r.mayMoves;
            newMoveSum += r.newMoves;
            cmdSum += r.commands;
            nodeSum += r.newNodes;
            depthSum += r.newDepthTotal;
            depthMoveSum += r.newDepthMoves;

            char line[384];
            sprintf(line, "  第%2d局 5月引擎执%-4s %-6s %-28s 步数=%3d 强制:5月%d/新%d 拒绝:5月%d/新%d 替代:新%d/5月%d 分歧%d 5月均时%.1fs 新均时%.1fs\n",
                g + 1, mayIsPlayer1 ? "先手" : "后手", winnerText(r.winner),
                r.endReason.c_str(), r.commands, r.forcedMay, r.forcedNew,
                r.rejMayByNew, r.rejNewByMay, r.substNew, r.substMay, r.divergences,
                r.mayMoves ? r.mayThinkTotalMs / r.mayMoves / 1000.0 : 0.0,
                r.newMoves ? r.newThinkTotalMs / r.newMoves / 1000.0 : 0.0);
            printf("%s", line);
            verbosePrintf("-- 第%d局结束: %s (%s) 命令=%d 棋谱=%s\n",
                g + 1, winnerText(r.winner), r.endReason.c_str(), r.commands, r.recordFile.c_str());
        }

        const double ruleMin = std::chrono::duration<double>(std::chrono::steady_clock::now() - ruleStart).count() / 60.0;
        printf("---- 规则%d汇总: 5月%d胜 新%d胜 和%d 异常%d | 平均%.1f命令/局 | "
               "强制出招: 5月%d次 新%d次 | 拒绝: 5月着法%d次 新着法%d次 | 替代%d/%d | 分歧%d | "
               "5月均时%.2fs 新均时%.2fs | 新引擎平均完成深度%.1f | 节点%.1fM | 用时%.1f分钟\n\n",
            rule, mayWins, newWins, draws, errors,
            gamesPerRule ? (double)cmdSum / gamesPerRule : 0.0,
            forcedMaySum, forcedNewSum,
            rejMaySum, rejNewSum, substNewSum, substMaySum, divSum,
            mayMoveSum ? mayMsSum / mayMoveSum / 1000.0 : 0.0,
            newMoveSum ? newMsSum / newMoveSum / 1000.0 : 0.0,
            depthMoveSum ? depthSum / depthMoveSum : 0.0,
            nodeSum / 1e6, ruleMin);

        totalMay += mayWins; totalNew += newWins;
        totalDraw += draws; totalError += errors;
    }

    const double totalMin = std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart).count() / 60.0;
    printf("======== 总计 ========\n5月引擎 %d 胜 | 新引擎 %d 胜 | 和棋 %d | 异常 %d | 总用时 %.1f 分钟\n",
        totalMay, totalNew, totalDraw, totalError, totalMin);

    {
        char sname[MAX_PATH], tag[64];
        __time64_t now = _time64(nullptr);
        tm tmut{};
        localtime_s(&tmut, &now);
        strftime(tag, sizeof(tag), "%Y%m%d_%H%M%S", &tmut);
        sprintf(sname, "results_may\\summary_%s.txt", tag);
        FILE* fp = nullptr;
        if (fopen_s(&fp, sname, "wb") == 0 && fp) {
            fprintf(fp, "5月引擎(d78811a, 单线程, 深度%d) vs 新引擎(%d线程, 深度%d, %s)\n",
                mayDepth, newThreads, newDepth, describeNewEngineConfig().c_str());
            fprintf(fp, "赛制: 每步%dms强制出招, %d条命令判和, 奇偶局交替执先\n",
                timeLimitMs, g_stepsLimit);
            fprintf(fp, "总计: 5月%d胜 新%d胜 和%d 异常%d, 用时%.1f分钟\n",
                totalMay, totalNew, totalDraw, totalError, totalMin);
            fclose(fp);
            printf("汇总已保存: %s\n", sname);
        }
    }
    if (g_verboseLog) fclose(g_verboseLog);
    return 0;
}

/****************************************************************************
** AIBenchmarkOldMay - 2018版引擎 vs 2026年5月版引擎 对抗基准（规则0）
**
** 2018引擎：提交 a588f4a，namespace ncold（1-based 命令，char 棋盘），单线程，深度 8
** 5月引擎：提交 d78811a，namespace ncmay（0-based 命令，位棋盘），单线程，深度 8
** 权威模型：5月模型（现代规则实现，已验证与当前模型逐位同步）。
** 赛制：每步 10 秒强制出招，100 条命令判和，奇偶局交替执先。
**
** 用法: AIBenchmarkOldMay.exe [规则] [局数] [2018深度] [5月深度] [限时ms]
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

#include "old_engine/ninechessai_ab.h"          // ncold::NineChess / ncold::NineChessAi_ab
#include "may_engine/ninechess_ai_ab.h"         // ncmay::NineChess / ncmay::NineChess_AI_AB

using std::string;

namespace {

constexpr int kStepsLimit = 100;

FILE* g_verboseLog = nullptr;

void verbosePrintf(const char* fmt, ...)
{
    if (!g_verboseLog) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(g_verboseLog, fmt, args);
    va_end(args);
    fflush(g_verboseLog);
}

// 新0-based <-> 旧1-based；delta=+1 新转旧，delta=-1 旧转新
bool convertCommand(const string& in, int delta, string& out)
{
    int a = 0, b = 0, c = 0, d = 0;
    char buf[64] = {};
    if (sscanf(in.c_str(), "(%d,%d)->(%d,%d)", &a, &b, &c, &d) == 4) {
        sprintf(buf, "(%d,%d)->(%d,%d)", a + delta, b + delta, c + delta, d + delta);
        out = buf; return true;
    }
    if (sscanf(in.c_str(), "-(%d,%d)", &a, &b) == 2) {
        sprintf(buf, "-(%d,%d)", a + delta, b + delta);
        out = buf; return true;
    }
    if (sscanf(in.c_str(), "(%d,%d)", &a, &b) == 2) {
        sprintf(buf, "(%d,%d)", a + delta, b + delta);
        out = buf; return true;
    }
    return false;
}

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

// 旧模型 char 棋盘转位棋盘（同步校验用）
struct OldBoardBits {
    uint32_t p1 = 0, p2 = 0, forbidden = 0;
};

OldBoardBits oldBoardBits(const ncold::NineChess& g)
{
    OldBoardBits bits;
    const char* board = g.getBoard();
    for (int pos = 0; pos < 24; ++pos) {
        const char ch = board[(pos / 8 + 1) * 8 + pos % 8];
        const uint32_t bit = 1u << pos;
        if (ch & 0x10) bits.p1 |= bit;
        else if (ch & 0x20) bits.p2 |= bit;
        else if (ch == '\x0f') bits.forbidden |= bit;
    }
    return bits;
}

struct DivergenceStats {
    int events = 0;
    bool logged = false;
};

void countDivergence(DivergenceStats& stats, const ncmay::NineChess& my, const ncold::NineChess& od)
{
    char buf[128];
    if (my.getPhase() == ncmay::NineChess::GAME_OVER || od.getPhase() == ncold::NineChess::GAME_OVER)
        return;
    const OldBoardBits bits = oldBoardBits(od);
    const auto& cd = my.getData();
    if (bits.p1 != cd.player1Board || bits.p2 != cd.player2Board || bits.forbidden != cd.forbiddenBoard) {
        sprintf(buf, "棋盘不同");
    } else if ((int)cd.getPendingCaptures() != od.getNum_NeedRemove()) {
        sprintf(buf, "待提子数不同: old=%d may=%u", od.getNum_NeedRemove(), cd.getPendingCaptures());
    } else {
        const bool turnSame = (od.whosTurn() == ncold::NineChess::PLAYER1) == (my.getTurn() == ncmay::NineChess::PLAYER1);
        const bool actSame = (od.getAction() == ncold::NineChess::ACTION_CAPTURE) == (my.getAction() == ncmay::NineChess::ACTION_CAPTURE);
        if (turnSame && actSame)
            return;
        sprintf(buf, "轮次/动作不同");
    }
    ++stats.events;
    if (!stats.logged) {
        stats.logged = true;
        verbosePrintf("  .. 首次模型分歧: %s\n", buf);
    }
}

// 兜底替代着法（各自模型）
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

enum class GameWinner { Old, May, Draw, Error };

struct GameReport {
    int game = 0;
    bool oldIsPlayer1 = true;
    GameWinner winner = GameWinner::Draw;
    string endReason;
    int commands = 0;
    int forcedOld = 0, forcedMay = 0;
    double oldThinkTotalMs = 0, mayThinkTotalMs = 0;
    int oldMoves = 0, mayMoves = 0;
    int rejOldByMay = 0, rejMayByOld = 0;
    int substMay = 0, substOld = 0;
    int divergences = 0;
};

const char* winnerText(GameWinner w)
{
    switch (w) {
    case GameWinner::Old: return "2018引擎胜";
    case GameWinner::May: return "5月引擎胜";
    case GameWinner::Draw: return "和棋";
    default: return "异常";
    }
}

GameReport playOneGame(int rule, int game, bool oldIsPlayer1,
                       int oldDepth, int mayDepth, int timeLimitMs,
                       const string& resultsDir)
{
    GameReport report;
    report.game = game;
    report.oldIsPlayer1 = oldIsPlayer1;

    ncmay::NineChess gameMay;   // 权威模型
    gameMay.setRule((uint32_t)rule);
    gameMay.start();

    ncold::NineChess gameOld;
    gameOld.setData(&ncold::NineChess::RULES[rule], kStepsLimit, 0);
    gameOld.start();

    ncmay::NineChess_AI_AB aiMay;
    ncold::NineChessAi_ab aiOld;

    string record;
    DivergenceStats divStats;

    verbosePrintf("=== 规则%d 第%d局 2018引擎执%s ===\n",
        rule, game + 1, oldIsPlayer1 ? "先手(P1)" : "后手(P2)");

    while (true) {
        if (gameMay.whoWin() != ncmay::NineChess::NOBODY) {
            const auto w = gameMay.whoWin();
            report.endReason = "规则终局";
            if (w == ncmay::NineChess::DRAW)
                report.winner = GameWinner::Draw;
            else {
                const bool p1Won = (w == ncmay::NineChess::PLAYER1);
                report.winner = (p1Won == oldIsPlayer1) ? GameWinner::Old : GameWinner::May;
            }
            break;
        }
        if ((int)gameMay.getCmdList()->size() >= kStepsLimit) {
            gameMay.command("==");
            report.endReason = "100步判和";
            report.winner = GameWinner::Draw;
            break;
        }
        if (report.commands >= 4 * kStepsLimit) {
            report.endReason = "步数保险丝触发，判和";
            report.winner = GameWinner::Draw;
            break;
        }

        const bool p1ToMove = (gameMay.getTurn() == ncmay::NineChess::PLAYER1);
        const bool oldToMove = (p1ToMove == oldIsPlayer1);

        if (oldToMove) {
            aiOld.setChess(gameOld);
            ThinkOutcome think = thinkWithTimeout(
                [&]() { aiOld.alphaBetaPruning(oldDepth); },
                [&]() { aiOld.quit(); },
                timeLimitMs, 15000);
            if (think.hung) {
                report.endReason = "2018引擎搜索无法中断（异常终止）";
                report.winner = GameWinner::Error;
                break;
            }
            report.oldThinkTotalMs += think.wallMs;
            report.forcedOld += think.forced ? 1 : 0;
            ++report.oldMoves;
            const string cmd = aiOld.bestMove();
            verbosePrintf("  [%3d] 2018引擎(P%d) %8.0fms%s 着法=%s\n",
                report.commands + 1, p1ToMove ? 1 : 2, think.wallMs,
                think.forced ? "(强制)" : "       ", cmd.c_str());
            if (cmd == "error!") {
                report.endReason = "2018引擎无法出招（按认输计）";
                report.winner = GameWinner::May;
                break;
            }
            if (!gameOld.command(cmd.c_str())) {
                report.endReason = "2018引擎着法被自身模型拒绝（按认输计）";
                report.winner = GameWinner::May;
                break;
            }
            string converted;
            if (!convertCommand(cmd, -1, converted)) {
                report.endReason = "2018引擎着法无法解析";
                report.winner = GameWinner::Error;
                break;
            }
            string appliedInMay;
            if (gameMay.command(converted.c_str())) {
                appliedInMay = converted;
            } else {
                ++report.rejOldByMay;
                appliedInMay = firstLegalCommandMay(gameMay);
                if (!appliedInMay.empty()) {
                    ++report.substMay;
                    verbosePrintf("  .. 5月模型拒绝 %s，替代执行 %s\n", converted.c_str(), appliedInMay.c_str());
                }
            }
            if (!appliedInMay.empty()) {
                record += appliedInMay;
                record += "\n";
                ++report.commands;
            }
        } else {
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
            report.forcedMay += think.forced ? 1 : 0;
            ++report.mayMoves;
            const string cmd = aiMay.bestMove();
            verbosePrintf("  [%3d] 5月引擎(P%d) %8.0fms%s 着法=%s\n",
                report.commands + 1, p1ToMove ? 1 : 2, think.wallMs,
                think.forced ? "(强制)" : "       ", cmd.c_str());
            if (cmd == "error!") {
                report.endReason = "5月引擎无法出招（按认输计）";
                report.winner = GameWinner::Old;
                break;
            }
            if (!gameMay.command(cmd.c_str())) {
                report.endReason = "5月引擎着法被权威模型拒绝（异常）";
                report.winner = GameWinner::Old;
                break;
            }
            record += cmd;
            record += "\n";
            ++report.commands;
            string converted;
            if (!convertCommand(cmd, +1, converted)) {
                report.endReason = "5月引擎着法无法解析";
                report.winner = GameWinner::Error;
                break;
            }
            if (!gameOld.command(converted.c_str())) {
                ++report.rejMayByOld;
                const string subst = firstLegalCommandOld(gameOld);
                if (!subst.empty()) {
                    ++report.substOld;
                    verbosePrintf("  .. 旧模型拒绝 %s，替代执行 %s\n", converted.c_str(), subst.c_str());
                }
            }
        }

        countDivergence(divStats, gameMay, gameOld);
    }
    report.divergences = divStats.events;

    if (report.winner == GameWinner::Error)
        verbosePrintf("  !! 异常: %s\n", report.endReason.c_str());

    const char* seatTag = oldIsPlayer1 ? "oldP1" : "mayP1";
    const char* resultTag = report.winner == GameWinner::Old ? "oldwin"
        : report.winner == GameWinner::May ? "maywin"
        : report.winner == GameWinner::Draw ? "draw" : "error";
    char fname[MAX_PATH];
    sprintf(fname, "%s\\rule%d_g%02d_%s_%s.txt", resultsDir.c_str(), rule, game + 1, seatTag, resultTag);
    FILE* fp = nullptr;
    if (fopen_s(&fp, fname, "wb") == 0 && fp) {
        fprintf(fp, "r%ds%dt0\n", rule, kStepsLimit);
        fputs(record.c_str(), fp);
        if (report.winner == GameWinner::Draw && report.endReason == "100步判和")
            fprintf(fp, "==\n");
        fclose(fp);
    }
    return report;
}

} // namespace

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    int rule = 0, games = 20, oldDepth = 8, mayDepth = 8, timeLimitMs = 10000;
    if (argc >= 2) rule = atoi(argv[1]);
    if (argc >= 3) games = atoi(argv[2]);
    if (argc >= 4) oldDepth = atoi(argv[3]);
    if (argc >= 5) mayDepth = atoi(argv[4]);
    if (argc >= 6) timeLimitMs = atoi(argv[5]);

    CreateDirectoryA("results_oldmay", nullptr);
    const string resultsDir = "results_oldmay";
    {
        char tag[64];
        __time64_t now = _time64(nullptr);
        tm tmut{};
        localtime_s(&tmut, &now);
        strftime(tag, sizeof(tag), "%Y%m%d_%H%M%S", &tmut);
        char vname[MAX_PATH];
        sprintf(vname, "results_oldmay\\verbose_%s.log", tag);
        fopen_s(&g_verboseLog, vname, "wb");
    }

    printf("AIBenchmarkOldMay: 2018引擎(深度%d, 单线程) vs 5月引擎(深度%d, 单线程)\n", oldDepth, mayDepth);
    printf("赛制: 规则%d, 每步%dms强制出招, %d条命令判和, 奇偶局交替执先\n\n",
        rule, timeLimitMs, kStepsLimit);

    int oldWins = 0, mayWins = 0, draws = 0, errors = 0;
    int forcedOldSum = 0, forcedMaySum = 0, rejOldSum = 0, rejMaySum = 0, divSum = 0;
    double oldMsSum = 0, mayMsSum = 0;
    int oldMoveSum = 0, mayMoveSum = 0, cmdSum = 0;
    const auto runStart = std::chrono::steady_clock::now();

    for (int g = 0; g < games; ++g) {
        const bool oldIsPlayer1 = (g % 2 == 0);
        GameReport r = playOneGame(rule, g, oldIsPlayer1, oldDepth, mayDepth, timeLimitMs, resultsDir);
        switch (r.winner) {
        case GameWinner::Old: ++oldWins; break;
        case GameWinner::May: ++mayWins; break;
        case GameWinner::Draw: ++draws; break;
        default: ++errors; break;
        }
        forcedOldSum += r.forcedOld;
        forcedMaySum += r.forcedMay;
        rejOldSum += r.rejOldByMay;
        rejMaySum += r.rejMayByOld;
        divSum += r.divergences;
        oldMsSum += r.oldThinkTotalMs;
        mayMsSum += r.mayThinkTotalMs;
        oldMoveSum += r.oldMoves;
        mayMoveSum += r.mayMoves;
        cmdSum += r.commands;

        char line[384];
        sprintf(line, "  第%2d局 2018引擎执%-4s %-10s %-24s 步数=%3d 强制:2018-%d/5月-%d 分歧%d 2018均时%.2fs 5月均时%.2fs\n",
            g + 1, oldIsPlayer1 ? "先手" : "后手", winnerText(r.winner),
            r.endReason.c_str(), r.commands, r.forcedOld, r.forcedMay, r.divergences,
            r.oldMoves ? r.oldThinkTotalMs / r.oldMoves / 1000.0 : 0.0,
            r.mayMoves ? r.mayThinkTotalMs / r.mayMoves / 1000.0 : 0.0);
        printf("%s", line);
        verbosePrintf("-- 第%d局结束: %s (%s) 命令=%d\n",
            g + 1, winnerText(r.winner), r.endReason.c_str(), r.commands);
    }

    const double totalMin = std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart).count() / 60.0;
    printf("---- 规则%d汇总: 2018引擎 %d胜 | 5月引擎 %d胜 | 和棋 %d | 异常 %d | 平均%.1f命令/局 | "
           "强制出招: 2018-%d/5月-%d | 分歧%d | 2018均时%.2fs 5月均时%.2fs | 用时%.1f分钟\n",
        rule, oldWins, mayWins, draws, errors,
        games ? (double)cmdSum / games : 0.0,
        forcedOldSum, forcedMaySum, divSum,
        oldMoveSum ? oldMsSum / oldMoveSum / 1000.0 : 0.0,
        mayMoveSum ? mayMsSum / mayMoveSum / 1000.0 : 0.0,
        totalMin);

    if (g_verboseLog) fclose(g_verboseLog);
    return 0;
}

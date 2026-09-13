/****************************************************************************
** AIBenchmarkSelf - 当前最新引擎（9月版）自对弈基准
**
** 两侧均为当前 NineChess/src 引擎（相同代码、相同配置策略），各自独立 AI 实例
** （独立置换表与随机种子），用于测量先后手胜率与和棋率。
** 随机三参数（randomness/randomGap/randomPlies）与评估项均不写入，
** 直接采用引擎头文件当前默认（2026-09-14 起为 rP10+g24 闭区间，wp=150/pv=16）。
**
** 赛制：每步限时强制出招（看门狗仅防挂死）；N 条走子命令未分胜负判和；
**      每局每方独立种子保证样本不重复。同模型自对弈无跨模型分歧问题，
**      单一权威模型，着法被拒或无法出招均按认输计并标记。
**
** 用法: AIBenchmarkSelf.exe [起始规则] [结束规则] [每规则局数] [深度] [限时ms] [线程数] [判和步数,默认100]
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

#include "../NineChess/src/ninechess_ai_ab.h"   // ::NineChess / ::NineChess_AI_AB

using std::string;

namespace {

// 判和步数：默认 100 条走子命令未分胜负判和；argv[7] 可覆盖（0 = 不判和）。
int g_stepsLimit = 100;

// 判和终局原因的规范文本：随 g_stepsLimit 变化，供摘要与棋谱判重共用。
string stepsLimitReason() { return std::to_string(g_stepsLimit) + "步判和"; }

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

// ------------------------- 单局对弈 -------------------------

enum class GameWinner { P1, P2, Draw, Error };

struct GameReport {
    int rule = 0;
    int game = 0;
    GameWinner winner = GameWinner::Draw;
    string endReason;
    int commands = 0;
    int forcedP1 = 0;
    int forcedP2 = 0;
    double p1ThinkTotalMs = 0;
    double p2ThinkTotalMs = 0;
    int p1Moves = 0;
    int p2Moves = 0;
    uint64_t p1Nodes = 0;
    uint64_t p2Nodes = 0;
    double depthTotal = 0;
    int depthMoves = 0;
    uint64_t seed1 = 0;
    uint64_t seed2 = 0;
    string recordFile;
};

const char* winnerText(GameWinner w)
{
    switch (w) {
    case GameWinner::P1: return "先手胜";
    case GameWinner::P2: return "后手胜";
    case GameWinner::Draw: return "和棋";
    default: return "异常";
    }
}

GameReport playOneGame(int rule, int game, int depth, int timeLimitMs, int threads,
                       const string& resultsDir)
{
    GameReport report;
    report.rule = rule;
    report.game = game;
    // 每局每方独立种子：同引擎两侧若同种子则随机选择完全同步（对称局面下
    // 会走出镜像对局），必须错开。
    report.seed1 = 0x9E3779B97F4A7C15ull * (uint64_t)(rule * 2000 + game * 2 + 1) + 777;
    report.seed2 = 0x9E3779B97F4A7C15ull * (uint64_t)(rule * 2000 + game * 2 + 2) + 777;

    NineChess gameCore;
    gameCore.setRule((uint32_t)rule);
    gameCore.start();

    // 两个独立 AI 实例：独立置换表与搜索上下文；随机/评估参数一律不赋值，
    // 保留引擎头文件当前默认（与对抗驱动 benchmark_may.cpp 的现行约定一致）。
    NineChess_AI_AB aiP1;
    {
        NineChess_AI_AB::SearchOptions options;
        options.timeLimitMs = timeLimitMs;
        options.threads = (uint32_t)threads;
        options.seed = report.seed1;
        aiP1.setOptions(options);
        aiP1.clearTranspositionTable();
    }
    NineChess_AI_AB aiP2;
    {
        NineChess_AI_AB::SearchOptions options;
        options.timeLimitMs = timeLimitMs;
        options.threads = (uint32_t)threads;
        options.seed = report.seed2;
        aiP2.setOptions(options);
        aiP2.clearTranspositionTable();
    }

    string record;

    verbosePrintf("=== 规则%d 第%d局 ===\n", rule, game + 1);

    while (true) {
        if (gameCore.whoWin() != NineChess::NOBODY) {
            const NineChess::Players w = gameCore.whoWin();
            report.endReason = "规则终局";
            report.winner = (w == NineChess::DRAW) ? GameWinner::Draw
                : (w == NineChess::PLAYER1) ? GameWinner::P1 : GameWinner::P2;
            break;
        }
        if (g_stepsLimit > 0 && (int)gameCore.getCmdList()->size() >= g_stepsLimit) {
            gameCore.command("==");
            report.endReason = stepsLimitReason();
            report.winner = GameWinner::Draw;
            break;
        }
        if (report.commands >= 4 * g_stepsLimit) {
            report.endReason = "步数保险丝触发，判和";
            report.winner = GameWinner::Draw;
            break;
        }

        const bool p1ToMove = (gameCore.getTurn() == NineChess::PLAYER1);
        NineChess_AI_AB& ai = p1ToMove ? aiP1 : aiP2;
        int& forcedCount = p1ToMove ? report.forcedP1 : report.forcedP2;
        double& thinkTotal = p1ToMove ? report.p1ThinkTotalMs : report.p2ThinkTotalMs;
        int& moveCount = p1ToMove ? report.p1Moves : report.p2Moves;
        uint64_t& nodeCount = p1ToMove ? report.p1Nodes : report.p2Nodes;

        // 每步思考前同步权威模型当前局面（setChess 内部复制状态并按根局面
        // 重建历史/哈希），否则 AI 仍持有上一手前的旧局面。
        ai.setChess(gameCore);
        ThinkOutcome think = thinkWithTimeout(
            [&]() { ai.alphaBetaPruning(depth); },
            [&]() { ai.quit(); },
            timeLimitMs, 15000);
        if (think.hung) {
            report.endReason = "引擎搜索无法中断（异常终止）";
            report.winner = GameWinner::Error;
            break;
        }
        thinkTotal += think.wallMs;
        forcedCount += think.forced ? 1 : 0;
        ++moveCount;
        NineChess_AI_AB::SearchStats stats;
        ai.snapshotStats(stats);
        nodeCount += stats.nodes.load();
        report.depthTotal += (double)ai.getLastCompletedDepth();
        ++report.depthMoves;
        const string cmd = ai.bestMove();
        verbosePrintf("  [%3d] P%d %8.0fms%s 深度=%d 节点=%llu 着法=%s\n",
            report.commands + 1, p1ToMove ? 1 : 2, think.wallMs,
            think.forced ? "(看门狗)" : "         ", ai.getLastCompletedDepth(),
            (unsigned long long)stats.nodes.load(), cmd.c_str());
        if (cmd == "error!") {
            report.endReason = p1ToMove ? "先手方无着法可返回（按认输计）"
                                        : "后手方无着法可返回（按认输计）";
            report.winner = p1ToMove ? GameWinner::P2 : GameWinner::P1;
            break;
        }
        if (!gameCore.command(cmd.c_str())) {
            report.endReason = p1ToMove ? "先手方着法被权威模型拒绝（异常，按认输计）"
                                        : "后手方着法被权威模型拒绝（异常，按认输计）";
            report.winner = p1ToMove ? GameWinner::P2 : GameWinner::P1;
            break;
        }
        record += cmd;
        record += "\n";
        ++report.commands;
    }

    if (report.winner == GameWinner::Error) {
        verbosePrintf("  !! 异常: %s\n", report.endReason.c_str());
    }

    const char* resultTag = report.winner == GameWinner::P1 ? "p1win"
        : report.winner == GameWinner::P2 ? "p2win"
        : report.winner == GameWinner::Draw ? "draw" : "error";
    char fname[MAX_PATH];
    sprintf(fname, "%s\\rule%d_g%02d_%s.txt", resultsDir.c_str(), rule, game + 1, resultTag);
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
    int depth = 10, timeLimitMs = 5000, threads = 8;
    if (argc >= 2) ruleFirst = atoi(argv[1]);
    if (argc >= 3) ruleLast = atoi(argv[2]);
    if (argc >= 4) gamesPerRule = atoi(argv[3]);
    if (argc >= 5) depth = atoi(argv[4]);
    if (argc >= 6) timeLimitMs = atoi(argv[5]);
    if (argc >= 7) threads = atoi(argv[6]);
    if (argc >= 8) g_stepsLimit = atoi(argv[7]);   // 判和步数（0 = 不判和）

    CreateDirectoryA("results_self", nullptr);
    const string resultsDir = "results_self";
    {
        char tag[64];
        __time64_t now = _time64(nullptr);
        tm tmut{};
        localtime_s(&tmut, &now);
        strftime(tag, sizeof(tag), "%Y%m%d_%H%M%S", &tmut);
        char vname[MAX_PATH];
        sprintf(vname, "results_self\\verbose_%s.log", tag);
        fopen_s(&g_verboseLog, vname, "wb");
    }

    printf("AIBenchmarkSelf: 当前引擎(9月版)自对弈 深度%d %d线程 随机=引擎默认\n",
        depth, threads);
    printf("赛制: 每步%dms强制出招, %s条命令未分胜负判和, 每局独立种子\n\n",
        timeLimitMs, std::to_string(g_stepsLimit).c_str());
    verbosePrintf("AIBenchmarkSelf 深度=%d 线程=%d 限时=%dms 局数=%d/规则 判和=%d 随机=引擎默认\n",
        depth, threads, timeLimitMs, gamesPerRule, g_stepsLimit);

    int totalP1 = 0, totalP2 = 0, totalDraw = 0, totalError = 0;
    const auto runStart = std::chrono::steady_clock::now();

    for (int rule = ruleFirst; rule <= ruleLast; ++rule) {
        int p1Wins = 0, p2Wins = 0, draws = 0, errors = 0;
        int forcedP1Sum = 0, forcedP2Sum = 0;
        double p1MsSum = 0, p2MsSum = 0;
        int p1MoveSum = 0, p2MoveSum = 0;
        int cmdSum = 0;
        uint64_t nodeSum = 0;
        double depthSum = 0;
        int depthMoveSum = 0;
        const auto ruleStart = std::chrono::steady_clock::now();

        printf("======== 规则 %d ========\n", rule);
        for (int g = 0; g < gamesPerRule; ++g) {
            GameReport r = playOneGame(rule, g, depth, timeLimitMs, threads, resultsDir);
            switch (r.winner) {
            case GameWinner::P1: ++p1Wins; break;
            case GameWinner::P2: ++p2Wins; break;
            case GameWinner::Draw: ++draws; break;
            default: ++errors; break;
            }
            forcedP1Sum += r.forcedP1;
            forcedP2Sum += r.forcedP2;
            p1MsSum += r.p1ThinkTotalMs;
            p2MsSum += r.p2ThinkTotalMs;
            p1MoveSum += r.p1Moves;
            p2MoveSum += r.p2Moves;
            cmdSum += r.commands;
            nodeSum += r.p1Nodes + r.p2Nodes;
            depthSum += r.depthTotal;
            depthMoveSum += r.depthMoves;

            char line[384];
            sprintf(line, "  第%2d局 %-6s %-28s 步数=%3d 强制:P1 %d/P2 %d P1均时%.2fs P2均时%.2fs\n",
                g + 1, winnerText(r.winner), r.endReason.c_str(), r.commands,
                r.forcedP1, r.forcedP2,
                r.p1Moves ? r.p1ThinkTotalMs / r.p1Moves / 1000.0 : 0.0,
                r.p2Moves ? r.p2ThinkTotalMs / r.p2Moves / 1000.0 : 0.0);
            printf("%s", line);
            verbosePrintf("-- 第%d局结束: %s (%s) 命令=%d 棋谱=%s\n",
                g + 1, winnerText(r.winner), r.endReason.c_str(), r.commands, r.recordFile.c_str());
        }

        const double ruleMin = std::chrono::duration<double>(std::chrono::steady_clock::now() - ruleStart).count() / 60.0;
        printf("---- 规则%d汇总: 先手%d胜 后手%d胜 和%d 异常%d | 平均%.1f命令/局 | "
               "强制出招: P1 %d次 P2 %d次 | P1均时%.2fs P2均时%.2fs | "
               "平均完成深度%.1f | 节点%.1fM | 用时%.1f分钟\n\n",
            rule, p1Wins, p2Wins, draws, errors,
            gamesPerRule ? (double)cmdSum / gamesPerRule : 0.0,
            forcedP1Sum, forcedP2Sum,
            p1MoveSum ? p1MsSum / p1MoveSum / 1000.0 : 0.0,
            p2MoveSum ? p2MsSum / p2MoveSum / 1000.0 : 0.0,
            depthMoveSum ? depthSum / depthMoveSum : 0.0,
            nodeSum / 1e6, ruleMin);

        totalP1 += p1Wins; totalP2 += p2Wins;
        totalDraw += draws; totalError += errors;
    }

    const double totalMin = std::chrono::duration<double>(std::chrono::steady_clock::now() - runStart).count() / 60.0;
    printf("======== 总计 ========\n先手 %d 胜 | 后手 %d 胜 | 和棋 %d | 异常 %d | 总用时 %.1f 分钟\n",
        totalP1, totalP2, totalDraw, totalError, totalMin);

    {
        char sname[MAX_PATH], tag[64];
        __time64_t now = _time64(nullptr);
        tm tmut{};
        localtime_s(&tmut, &now);
        strftime(tag, sizeof(tag), "%Y%m%d_%H%M%S", &tmut);
        sprintf(sname, "results_self\\summary_%s.txt", tag);
        FILE* fp = nullptr;
        if (fopen_s(&fp, sname, "wb") == 0 && fp) {
            fprintf(fp, "当前引擎(9月版)自对弈 深度%d %d线程 随机=引擎默认\n", depth, threads);
            fprintf(fp, "赛制: 每步%dms强制出招, %s条命令判和, 每局独立种子\n",
                timeLimitMs, std::to_string(g_stepsLimit).c_str());
            fprintf(fp, "总计: 先手%d胜 后手%d胜 和%d 异常%d, 用时%.1f分钟\n",
                totalP1, totalP2, totalDraw, totalError, totalMin);
            fclose(fp);
            printf("汇总已保存: %s\n", sname);
        }
    }
    if (g_verboseLog) fclose(g_verboseLog);
    return 0;
}

// 深度耗时探测：在真实对局的不同阶段局面上，测量新旧引擎各搜索深度的实际耗时。
// 用法: DepthProbe.exe <规则0-3> <棋谱文件(0-based命令流)> <前缀命令数>
//   前缀 0 = 空盘开局。同一局面下分别用新旧引擎搜索多个深度并计时。
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "old_engine/ninechessai_ab.h"
#include "../NineChess/src/ninechess_ai_ab.h"

using std::string;

static bool convertCommand(const string& in, int delta, string& out)
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

struct TimedResult {
    double ms = 0;
    bool timeout = false;
    string move;
    int completedDepth = -1;
    uint64_t nodes = 0;
};

// 旧引擎：固定深度搜索，25 秒看门狗（超过即强制中断，视为该深度不可行）
TimedResult probeOld(ncold::NineChess& game, int depth, int limitMs)
{
    TimedResult r;
    ncold::NineChessAi_ab ai;
    ai.setChess(game);
    std::atomic<bool> done{ false };
    const auto start = std::chrono::steady_clock::now();
    std::thread worker([&]() { ai.alphaBetaPruning(depth); done = true; });
    auto elapsed = [&]() {
        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    };
    while (!done && elapsed() < limitMs)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!done) {
        ai.quit();
        r.timeout = true;
    }
    worker.join();
    r.ms = elapsed();
    r.move = ai.bestMove();
    return r;
}

TimedResult probeNew(NineChess& game, int depth, int threads)
{
    TimedResult r;
    NineChess_AI_AB ai;
    NineChess_AI_AB::SearchOptions opt;
    opt.timeLimitMs = 0;            // 不限时，测真实耗时
    opt.threads = (uint32_t)threads;
    opt.randomness = 0;             // 纯最优
    ai.setOptions(opt);
    ai.setChess(game);
    const auto start = std::chrono::steady_clock::now();
    ai.alphaBetaPruning(depth);
    r.ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    r.completedDepth = ai.getLastCompletedDepth();
    r.move = ai.bestMove();
    NineChess_AI_AB::SearchStats stats;
    ai.snapshotStats(stats);
    r.nodes = stats.nodes.load();
    return r;
}

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(CP_UTF8);
    if (argc < 4) {
        printf("usage: DepthProbe.exe <rule> <record> <prefixCmds>\n");
        return 1;
    }
    const int rule = atoi(argv[1]);
    const int prefix = atoi(argv[3]);

    std::ifstream fin(argv[2]);
    std::stringstream ss;
    ss << fin.rdbuf();
    std::vector<string> lines;
    string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (!line.empty()) lines.push_back(line);
    }
    size_t start = 0;
    int r0, s0, t0;
    if (!lines.empty() && sscanf(lines[0].c_str(), "r%ds%dt%d", &r0, &s0, &t0) == 3)
        start = 1;

    NineChess gameNew;
    gameNew.setRule((uint32_t)rule);
    gameNew.start();
    ncold::NineChess gameOld;
    gameOld.setData(&ncold::NineChess::RULES[rule], 0, 0);
    gameOld.start();

    int applied = 0;
    for (size_t i = start; i < lines.size() && applied < prefix; ++i, ++applied) {
        string converted;
        if (!convertCommand(lines[i], +1, converted) || !gameNew.command(lines[i].c_str())
            || !gameOld.command(converted.c_str())) {
            printf("prefix replay failed at %s\n", lines[i].c_str());
            return 1;
        }
    }
    const bool over = gameNew.whoWin() != NineChess::NOBODY;
    printf("rule=%d prefix=%d phase=%s\n", rule, applied,
        gameNew.getPhase() == NineChess::GAME_OPENING ? "OPENING" : "MID");

    if (!over) {
        printf("--- 旧引擎(单线程) ---\n");
        for (int d = 8; d <= 13; ++d) {
            TimedResult r = probeOld(gameOld, d, 25000);
            if (r.timeout)
                printf("  depth %2d : >25s (强制中断)  —— 不可行\n", d);
            else
                printf("  depth %2d : %8.0f ms  best=%s\n", d, r.ms, r.move.c_str());
            if (r.ms > 12000 || r.timeout) {
                printf("  (更深必然超时，停止探测)\n");
                break;
            }
        }
        printf("--- 新引擎(%d线程) ---\n", 8);
        for (int d = 8; d <= 20; d += 2) {
            TimedResult r = probeNew(gameNew, d, 8);
            printf("  depth %2d : %8.0f ms  完成深度=%d 节点=%.2fM  best=%s\n",
                d, r.ms, r.completedDepth, r.nodes / 1e6, r.move.c_str());
            if (r.ms > 12000) {
                printf("  (更深必然超时，停止探测)\n");
                break;
            }
        }
    } else {
        printf("prefix position is game over\n");
    }
    return 0;
}

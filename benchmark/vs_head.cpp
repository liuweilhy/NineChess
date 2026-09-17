// 当前工作区权重（tableWeights）vs git HEAD 权重行。
// 双方同搜索配置：depth=10、threads=8、timeLimitMs=5000、80 步判和、纯最优。
// 交替执先。git AI 源码 diff 仅权重表，故 useCustomWeights 即代码级对照。
// 用法: VsHead.exe [gamesPerRule=40] [depth=10] [seed=1] [threads=8] [timeMs=5000] [plyLimit=80]
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "../NineChess/src/ninechess.h"
#include "../NineChess/src/ninechess_ai_ab.h"

namespace {

NineChess_AI_AB::EvalWeights headRow()
{
    NineChess_AI_AB::EvalWeights w;
    w.openingMaterial = 120;
    w.openingInHand = 48;
    w.openingMill = 96;
    w.openingOpenMill = 24;
    w.midMaterial = 180;
    w.midMill = 112;
    w.midOpenMill = 32;
    w.midMobility = 10;
    w.captureOpening = 160;
    w.captureMid = 220;
    w.forkThreat = 0;
    w.stalematePressure = 0;
    w.millSafety = 0;
    return w;
}

void printRow(const char* tag, const NineChess_AI_AB::EvalWeights& w)
{
    std::printf("%s: mat %d/%d inHand %d mill %d/%d openMill %d/%d mob %d cap %d/%d ft %d sp %d ms %d\n",
        tag, w.openingMaterial, w.midMaterial, w.openingInHand,
        w.openingMill, w.midMill, w.openingOpenMill, w.midOpenMill, w.midMobility,
        w.captureOpening, w.captureMid, w.forkThreat, w.stalematePressure, w.millSafety);
}

void playRule(uint32_t ruleIndex, int games, int depth, uint64_t seed, uint32_t threads,
    int64_t timeMs, int plyLimit)
{
    const NineChess_AI_AB::EvalWeights baseline = headRow();
    const NineChess_AI_AB::EvalWeights candidate = NineChess_AI_AB::tableWeights(ruleIndex);

    std::printf("\n==== rule=%u  games=%d  depth=%d  th=%u  timeMs=%lld  plyLimit=%d  seed=%llu ====\n",
        ruleIndex, games, depth, threads, static_cast<long long>(timeMs), plyLimit,
        static_cast<unsigned long long>(seed));
    printRow("HEAD  (baseline) ", baseline);
    printRow("WORK  (candidate)", candidate);

    int wins2 = 0;
    int wins1 = 0;
    int draws = 0;
    long long totalNodes = 0;
    long long totalTimeMs = 0;
    long long timeoutHits = 0;

    for (int g = 0; g < games; ++g) {
        NineChess game;
        game.setRule(ruleIndex);
        game.start();

        const bool swapColors = (g % 2 == 1);
        NineChess_AI_AB ai1;
        NineChess_AI_AB ai2;
        NineChess_AI_AB::SearchOptions options;
        options.timeLimitMs = timeMs;
        options.randomness = 0; // 纯最优，降低权重 A/B 噪声
        options.randomGap = 0;
        options.randomPlies = 0;
        options.threads = threads;
        options.dynamicDepth = false; // 严格 depth 层
        options.useCustomWeights = true;
        options.weights = swapColors ? candidate : baseline;
        options.seed = seed + static_cast<uint64_t>(g) * 2ull + 1ull;
        ai1.setOptions(options);
        options.weights = swapColors ? baseline : candidate;
        options.seed = seed + static_cast<uint64_t>(g) * 2ull + 2ull;
        ai2.setOptions(options);

        ai1.setChess(game);
        ai2.setChess(game);
        ai1.clearTranspositionTable();
        ai2.clearTranspositionTable();

        int plies = 0;
        bool aborted = false;
        while (game.whoWin() == NineChess::NOBODY && plies < plyLimit) {
            const bool firstTurn = game.getTurn() == NineChess::PLAYER1;
            NineChess_AI_AB& ai = firstTurn ? ai1 : ai2;
            ai.setChess(game);
            ai.alphaBetaPruning(depth);
            NineChess_AI_AB::SearchStats stats;
            ai.snapshotStats(stats);
            totalNodes += static_cast<long long>(stats.nodes.load());
            const int64_t used = ai.getLastSearchTimeMs();
            totalTimeMs += used;
            if (used >= timeMs) {
                ++timeoutHits;
            }
            const char* cmd = ai.bestMove();
            if (std::strcmp(cmd, "error!") == 0) {
                aborted = true;
                break;
            }
            if (!game.command(cmd)) {
                aborted = true;
                break;
            }
            ++plies;
        }

        const NineChess::Players winner = game.whoWin();
        // 引擎2 = candidate(WORK)；换色时 candidate 执先手。
        const bool candidateWon = swapColors ? (winner == NineChess::PLAYER1)
                                             : (winner == NineChess::PLAYER2);
        const bool baselineWon = swapColors ? (winner == NineChess::PLAYER2)
                                            : (winner == NineChess::PLAYER1);
        if (aborted && winner == NineChess::NOBODY) {
            ++draws; // 异常无胜者按和计，与历史驱动口径一致
        }
        else if (candidateWon) {
            ++wins2;
        }
        else if (baselineWon) {
            ++wins1;
        }
        else {
            ++draws;
        }
        if ((g + 1) % 5 == 0 || g + 1 == games) {
            std::printf("  [%u] %d/%d  HEAD %d : WORK %d : draw %d\n",
                ruleIndex, g + 1, games, wins1, wins2, draws);
            std::fflush(stdout);
        }
    }

    std::printf("==== RULE %u RESULT  HEAD %d : WORK %d : draw %d  (timeoutHits=%lld) ====\n",
        ruleIndex, wins1, wins2, draws, timeoutHits);
    std::printf("nodes=%lld timeMs=%lld avgNPS=%lld\n",
        totalNodes, totalTimeMs,
        totalTimeMs > 0 ? totalNodes * 1000 / totalTimeMs : 0);
}

} // namespace

int main(int argc, char** argv)
{
    const int games = argc > 1 ? std::atoi(argv[1]) : 40;
    const int depth = argc > 2 ? std::atoi(argv[2]) : 10;
    const uint64_t seed = argc > 3 ? static_cast<uint64_t>(std::atoll(argv[3])) : 1ull;
    const uint32_t threads = argc > 4 ? static_cast<uint32_t>(std::atoi(argv[4])) : 8u;
    const int64_t timeMs = argc > 5 ? static_cast<int64_t>(std::atoll(argv[5])) : 5000;
    const int plyLimit = argc > 6 ? std::atoi(argv[6]) : 80;
    if (games <= 0 || depth <= 0 || threads == 0 || timeMs <= 0 || plyLimit <= 0) {
        std::printf("usage: VsHead.exe [games=40] [depth=10] [seed=1] [threads=8] [timeMs=5000] [plyLimit=80]\n");
        return 2;
    }

    std::printf("VsHead: WORK tableWeights vs HEAD row | d=%d th=%u t=%lldms ply<=%d n=%d/seed=%llu | pureBest dynamicDepth=off\n",
        depth, threads, static_cast<long long>(timeMs), plyLimit, games,
        static_cast<unsigned long long>(seed));

    for (uint32_t rule = 0; rule < RULE_COUNT; ++rule) {
        playRule(rule, games, depth, seed, threads, timeMs, plyLimit);
    }
    return 0;
}

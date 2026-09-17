// 评估权重 A/B：baseline = 历史公共行；candidate = 按 variant 单变量修改。
// 交替执先、双方同线程、固定种子；口径对齐 AI_ALPHABETA.md §5。
// 用法: EvalAbRow.exe [rule=2] [games=40] [depth=10] [seed=1] [threads=8] [variant=0]
// variant:
//   0 = 当前规则表行整行 vs legacy
//   1 = 仅 openingInHand 48→80
//   2 = 仅 stalematePressure 0→20
//   3 = 仅 millSafety 0→16
//   4 = 仅活二 ×1.5（opening 36 / mid 48）
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../NineChess/src/ninechess.h"
#include "../NineChess/src/ninechess_ai_ab.h"

namespace {

NineChess_AI_AB::EvalWeights legacyCommonRow()
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

const char* variantName(int variant)
{
    switch (variant) {
    case 0: return "table-row";
    case 1: return "inHand80";
    case 2: return "stalemate20";
    case 3: return "millSafety16";
    case 4: return "openMill1.5x";
    case 5: return "r2-refined-vs-work";
    default: return "unknown";
    }
}

NineChess_AI_AB::EvalWeights oldWorkRow2()
{
    NineChess_AI_AB::EvalWeights w;
    w.openingMaterial = 128;
    w.openingInHand = 160;
    w.openingMill = 180;
    w.openingOpenMill = 64;
    w.midMaterial = 128;
    w.midMill = 180;
    w.midOpenMill = 64;
    w.midMobility = 10;
    w.captureOpening = 160;
    w.captureMid = 180;
    w.forkThreat = 180;
    w.stalematePressure = 0;
    w.millSafety = 0;
    return w;
}

NineChess_AI_AB::EvalWeights makeCandidate(uint32_t ruleIndex, int variant)
{
    NineChess_AI_AB::EvalWeights w = legacyCommonRow();
    if (variant == 0) {
        return NineChess_AI_AB::tableWeights(ruleIndex);
    }
    if (variant == 1) {
        w.openingInHand = 80;
    }
    else if (variant == 2) {
        w.stalematePressure = 20;
    }
    else if (variant == 3) {
        w.millSafety = 16;
    }
    else if (variant == 4) {
        w.openingOpenMill = 36;
        w.midOpenMill = 48;
    }
    else if (variant == 5) {
        // candidate = 当前表规则2 行（收紧后）；baseline 在 main 里改用 oldWorkRow2
        return NineChess_AI_AB::tableWeights(ruleIndex);
    }
    return w;
}

void printRow(const char* tag, const NineChess_AI_AB::EvalWeights& w)
{
    std::printf("%s: mat %d/%d inHand %d mill %d/%d openMill %d/%d mob %d cap %d/%d ft %d sp %d ms %d\n",
        tag, w.openingMaterial, w.midMaterial, w.openingInHand,
        w.openingMill, w.midMill, w.openingOpenMill, w.midOpenMill, w.midMobility,
        w.captureOpening, w.captureMid, w.forkThreat, w.stalematePressure, w.millSafety);
}

} // namespace

int main(int argc, char** argv)
{
    const uint32_t ruleIndex = argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 2u;
    const int games = argc > 2 ? std::atoi(argv[2]) : 40;
    const int depth = argc > 3 ? std::atoi(argv[3]) : 10;
    const uint64_t seed = argc > 4 ? static_cast<uint64_t>(std::atoll(argv[4])) : 1ull;
    const uint32_t threads = argc > 5 ? static_cast<uint32_t>(std::atoi(argv[5])) : 8u;
    const int variant = argc > 6 ? std::atoi(argv[6]) : 0;
    if (ruleIndex >= RULE_COUNT || games <= 0 || depth <= 0 || threads == 0 || variant < 0 || variant > 5) {
        std::printf("usage: EvalAbRow.exe [rule0-3] [games] [depth] [seed] [threads] [variant0-5]\n");
        return 2;
    }

    NineChess_AI_AB::EvalWeights baseline = legacyCommonRow();
    if (variant == 5) {
        baseline = oldWorkRow2();
    }
    const NineChess_AI_AB::EvalWeights candidate = makeCandidate(ruleIndex, variant);

    std::printf("EvalAbRow rule=%u games=%d depth=%d seed=%llu threads=%u variant=%d (%s)\n",
        ruleIndex, games, depth, static_cast<unsigned long long>(seed), threads, variant,
        variantName(variant));
    printRow("baseline (legacy)", baseline);
    printRow("candidate         ", candidate);

    int wins2 = 0;
    int wins1 = 0;
    int draws = 0;
    long long totalNodes = 0;
    long long totalTimeMs = 0;

    for (int g = 0; g < games; ++g) {
        NineChess game;
        game.setRule(ruleIndex);
        game.start();

        const bool swapColors = (g % 2 == 1);
        NineChess_AI_AB ai1;
        NineChess_AI_AB ai2;
        NineChess_AI_AB::SearchOptions options;
        options.timeLimitMs = 0;
        options.randomness = 1;
        options.randomGap = 16;
        options.randomPlies = 10;
        options.threads = threads;
        options.dynamicDepth = false;
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
        while (game.whoWin() == NineChess::NOBODY && plies < 300) {
            const bool firstTurn = game.getTurn() == NineChess::PLAYER1;
            NineChess_AI_AB& ai = firstTurn ? ai1 : ai2;
            ai.setChess(game);
            ai.alphaBetaPruning(depth);
            NineChess_AI_AB::SearchStats stats;
            ai.snapshotStats(stats);
            totalNodes += static_cast<long long>(stats.nodes.load());
            totalTimeMs += ai.getLastSearchTimeMs();
            const char* cmd = ai.bestMove();
            if (std::strcmp(cmd, "error!") == 0) {
                break;
            }
            if (!game.command(cmd)) {
                break;
            }
            ++plies;
        }

        const NineChess::Players winner = game.whoWin();
        const bool candidateWon = swapColors ? (winner == NineChess::PLAYER1)
                                             : (winner == NineChess::PLAYER2);
        const bool baselineWon = swapColors ? (winner == NineChess::PLAYER2)
                                            : (winner == NineChess::PLAYER1);
        if (candidateWon) {
            ++wins2;
        }
        else if (baselineWon) {
            ++wins1;
        }
        else {
            ++draws;
        }
        if ((g + 1) % 5 == 0 || g + 1 == games) {
            std::printf("  progress %d/%d  baseline %d : candidate %d : draw %d\n",
                g + 1, games, wins1, wins2, draws);
            std::fflush(stdout);
        }
    }

    std::printf("==== RESULT rule=%u variant=%s th=%u d=%d  baseline %d : candidate %d : draw %d ====\n",
        ruleIndex, variantName(variant), threads, depth, wins1, wins2, draws);
    std::printf("nodes=%lld timeMs=%lld avgNPS=%lld\n",
        totalNodes, totalTimeMs,
        totalTimeMs > 0 ? totalNodes * 1000 / totalTimeMs : 0);
    std::printf("note: 8-thread identical-config rerun noise ~±5 wins at n=80 (§5)\n");
    return 0;
}

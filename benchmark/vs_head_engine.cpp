// 代码级 A/B：当前工作区引擎（../NineChess/src）vs git HEAD 引擎快照
// （head_engine/，namespace nchead；核心 ninechess.* 双方共享同一份，
// 差异只在 AI 搜索/评估代码）。用于验证搜索算法改动的净强度变化。
// 赛制 = benchmark/README.md「AI Agent 对战规范」（2026-10 定稿）：
//   规则 0..3，每规则先后手各 gamesPerSide 局（奇偶局交替执先）；
//   depth 固定（dynamicDepth 关）+ 单步 timeMs 上限 + plyLimit 步封顶；
//   threads 并行；双方各用自己引擎的默认评估权重与随机策略；
//   每局每侧独立种子（可复现）；实例按局新建（置换表不跨局携带）。
// 用法: VsHeadCode.exe [gamesPerSide=20] [depth=10] [seed=1] [threads=8]
//                        [timeMs=5000] [plyLimit=80] [ruleLo=0] [ruleHi=3]
//                        [newTie=1] [newPv=1] [newHeadRow=0] [var=0] [newLmr=1] [midGap=0]
//                        [parRoot=1] [repLead=0] [flyBehind=-1] [tieGroups=1]
// midGap = 全程层（randomPlies 之后）的近似分随机阈值；0 = 仅精确同分（默认）。
// parRoot = 根随机层等价组并行（1 = 开）；repLead = 领先时重复惩罚上限（0 = 关）；
// flyBehind = 飞子方落后时的机动性千分比（-1 = 关）；
// tieGroups = 全程同分层是否跨等价类抽签（0 = 只在最优等价组内随机［默认］）。
// var = 候选侧单变量权重变体（基于 HEAD 表行只改一列；0 = 不改，量 sprint2 引擎差异）：
//   1 = forkThreat → 180（规则2 由 0 打开）
//   2 = forkThreat → 80
//   3 = midMobility → 10（仅规则3，由 24 降）
//   4 = stalematePressure → 20（仅对表值 400 的规则0/1/3）
//   5 = captureMid = midMaterial（待提=在盘同价）
//   6 = forkThreat → 0（规则0/1/3 关闭，对照臂）
//   7 = midMobility → 16（仅规则3）
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../NineChess/src/ninechess.h"
#include "../NineChess/src/ninechess_ai_ab.h"
#include "head_engine/ninechess_ai_ab.h"

namespace {

using NewEngine = NineChess_AI_AB;
using HeadEngine = nchead::NineChess_AI_AB;

// git HEAD（42980e5）的 s_evalWeightsPerRule 四行；newHeadRow=1 时让 NEW 侧
// 以 useCustomWeights 套用同表，使双方权重逐列一致（隔离权重变量）。
const int32_t kHeadWeightRows[4][13] = {
    /* 0 */ { 128, 160, 180, 64, 128, 180, 64, 10, 160, 180, 180, 400, 0 },
    /* 1 */ { 128, 160, 180, 64, 128, 180, 64, 10, 160, 180, 180, 400, 0 },
    /* 2 */ { 128, 160, 0, 64, 180, 0, 64, 10, 160, 220, 0, 0, 0 },
    /* 3 */ { 128, 160, 180, 64, 128, 180, 64, 24, 160, 180, 180, 400, 0 },
};

struct SideStats {
    long long nodes = 0;
    long long timeMs = 0;
    long long timeoutHits = 0;
};

// 候选侧单列权重变体：始终从 HEAD 表行出发，只改 var 指定的那一列，
// 因此 baseline（HEAD 默认权重）与候选的差异严格等于这一列（单变量 isolate）。
// 变体编号与用法注释一致；对不适用的规则保持原值（如规则2 的 stalemate 已是 0）。
NineChess_AI_AB::EvalWeights candidateRow(int ruleIndex, int variant)
{
    NineChess_AI_AB::EvalWeights w;
    const int32_t* row = kHeadWeightRows[ruleIndex];
    w.openingMaterial = row[0];
    w.openingInHand = row[1];
    w.openingMill = row[2];
    w.openingOpenMill = row[3];
    w.midMaterial = row[4];
    w.midMill = row[5];
    w.midOpenMill = row[6];
    w.midMobility = row[7];
    w.captureOpening = row[8];
    w.captureMid = row[9];
    w.forkThreat = row[10];
    w.stalematePressure = row[11];
    w.millSafety = row[12];

    switch (variant) {
    case 1:
        // 规则2：forkThreat 0 → 180（与规则1 同为多提规则，取同一量级）
        w.forkThreat = 180;
        break;
    case 2:
        w.forkThreat = 80;
        break;
    case 3:
        // 规则3：飞子使机动性按"子数×空位"计量，24 的权重在 3v4 残局可达上千分
        w.midMobility = 10;
        break;
    case 4:
        w.stalematePressure = 20;
        break;
    case 5:
        // 待提 = 在盘同价：消除"兑现提子掉分"的价差错位
        w.captureMid = w.midMaterial;
        break;
    case 6:
        w.forkThreat = 0;
        break;
    case 7:
        w.midMobility = 16;
        break;
    default:
        break;
    }
    return w;
}

template <typename Engine>
void configureEngine(Engine& ai, int64_t timeMs, uint32_t threads, uint64_t seed)
{
    typename Engine::SearchOptions options;
    options.timeLimitMs = timeMs;
    options.threads = threads;
    options.dynamicDepth = false; // 严格按 depth 层（对战规范）
    options.seed = seed;
    ai.setOptions(options);
}

// 让 ai 在 game 上走一步；返回 false 表示引擎输出异常或走子被拒，
// 调用方按和棋口径中止该局（与历史驱动一致）。
template <typename Engine>
bool engineStep(Engine& ai, NineChess& game, int depth, int64_t timeMs,
    SideStats& side)
{
    ai.setChess(game);
    ai.alphaBetaPruning(depth);
    typename Engine::SearchStats stats;
    ai.snapshotStats(stats);
    side.nodes += static_cast<long long>(stats.nodes.load());
    const int64_t used = ai.getLastSearchTimeMs();
    side.timeMs += used;
    if (used >= timeMs) {
        ++side.timeoutHits;
    }
    const char* cmd = ai.bestMove();
    if (std::strcmp(cmd, "error!") == 0) {
        return false;
    }
    return game.command(cmd);
}

// NEW 侧专用配置（消融用）：两个仍在册的开关（同分层/PV 排序）可关以隔离
// 单项贡献；权重可用 HEAD 表行整行替换（隔离权重变量），或用 var 只改一列。
// 其余沿用引擎默认。
// 注：2026-10 否决的三项剪枝实验（空着/MDP/IIR）代码已整体移除，无开关可关；
// 历史臂的复现与数据见 sprint2_2026-10 报告。
void configureNewEngine(NewEngine& ai, int64_t timeMs, uint32_t threads,
    uint64_t seed, bool newTie, bool newPv,
    bool newHeadRow, int variant, bool newLmr, int midGap,
    bool parRoot, int repLead, int flyBehind, bool tieGroups, int ruleIndex)
{
    NewEngine::SearchOptions options;
    options.timeLimitMs = timeMs;
    options.threads = threads;
    options.dynamicDepth = false; // 严格按 depth 层（对战规范）
    options.seed = seed;
    options.randomTieLayer = newTie;
    options.randomTieAcrossGroups = tieGroups;
    options.rootPvOrdering = newPv;
    options.lmrFullWindowVerification = newLmr;
    options.randomGapMid = midGap; // 全程层近似分随机（0 = 仅精确同分）
    options.parallelRootScoring = parRoot;
    options.repetitionLeadScale = repLead;
    options.flyingMobilityWhenBehind = flyBehind;
    // newHeadRow（整行对齐 HEAD）优先；否则 var != 0 时套用"HEAD 行 + 单列改动"。
    if (newHeadRow || variant != 0) {
        options.useCustomWeights = true;
        options.weights = candidateRow(ruleIndex, newHeadRow ? 0 : variant);
    }
    ai.setOptions(options);
}

void playRule(uint32_t ruleIndex, int gamesPerSide, int depth, uint64_t seed,
    uint32_t threads, int64_t timeMs, int plyLimit,
    bool newTie, bool newPv, bool newHeadRow, int variant, bool newLmr, int midGap,
    bool parRoot, int repLead, int flyBehind, bool tieGroups)
{
    std::printf("\n==== rule=%u  gamesPerSide=%d  depth=%d  th=%u  timeMs=%lld  plyLimit=%d  seed=%llu  var=%d  newLmr=%d  midGap=%d  parRoot=%d  repLead=%d  flyBehind=%d  tieGroups=%d ====\n",
        ruleIndex, gamesPerSide, depth, threads, static_cast<long long>(timeMs),
        plyLimit, static_cast<unsigned long long>(seed), variant, newLmr ? 1 : 0, midGap,
        parRoot ? 1 : 0, repLead, flyBehind, tieGroups ? 1 : 0);
    if (variant != 0 && !newHeadRow) {
        const NineChess_AI_AB::EvalWeights w = candidateRow(static_cast<int>(ruleIndex), variant);
        std::printf("  NEW row: mat %d/%d hand %d mill %d/%d open %d/%d mob %d cap %d/%d ft %d sp %d ms %d\n",
            w.openingMaterial, w.midMaterial, w.openingInHand,
            w.openingMill, w.midMill, w.openingOpenMill, w.midOpenMill,
            w.midMobility, w.captureOpening, w.captureMid,
            w.forkThreat, w.stalematePressure, w.millSafety);
    }

    // 新引擎执先/执后的 胜(新):负(HEAD):和 三联计数。
    int newFirst[3] = { 0, 0, 0 };
    int newSecond[3] = { 0, 0, 0 };
    SideStats newSide;
    SideStats headSide;

    const int games = gamesPerSide * 2;
    for (int g = 0; g < games; ++g) {
        NineChess game;
        game.setRule(ruleIndex);
        game.start();

        const bool newAsFirst = (g % 2 == 0);
        NewEngine newAi;
        HeadEngine headAi;
        configureNewEngine(newAi, timeMs, threads,
            seed + static_cast<uint64_t>(g) * 2ull + 1ull,
            newTie, newPv, newHeadRow, variant, newLmr, midGap,
            parRoot, repLead, flyBehind, tieGroups, static_cast<int>(ruleIndex));
        configureEngine(headAi, timeMs, threads, seed + static_cast<uint64_t>(g) * 2ull + 2ull);
        newAi.clearTranspositionTable();
        headAi.clearTranspositionTable();

        int plies = 0;
        bool aborted = false;
        while (game.whoWin() == NineChess::NOBODY && plies < plyLimit) {
            const bool firstTurn = game.getTurn() == NineChess::PLAYER1;
            const bool newTurn = newAsFirst ? firstTurn : !firstTurn;
            const bool ok = newTurn
                ? engineStep(newAi, game, depth, timeMs, newSide)
                : engineStep(headAi, game, depth, timeMs, headSide);
            if (!ok) {
                aborted = true;
                break;
            }
            ++plies;
        }

        const NineChess::Players winner = game.whoWin();
        int code = 2; // 0 = 新引擎胜，1 = HEAD 胜，2 = 和
        if (winner != NineChess::NOBODY) {
            const bool newWon = newAsFirst
                ? (winner == NineChess::PLAYER1)
                : (winner == NineChess::PLAYER2);
            code = newWon ? 0 : 1;
        }
        (newAsFirst ? newFirst : newSecond)[code]++;

        if ((g + 1) % 5 == 0 || g + 1 == games) {
            const int nw = newFirst[0] + newSecond[0];
            const int hw = newFirst[1] + newSecond[1];
            const int dr = newFirst[2] + newSecond[2];
            std::printf("  [%u] %d/%d  HEAD %d : NEW %d : draw %d\n",
                ruleIndex, g + 1, games, hw, nw, dr);
            std::fflush(stdout);
        }
    }

    const int nw = newFirst[0] + newSecond[0];
    const int hw = newFirst[1] + newSecond[1];
    const int dr = newFirst[2] + newSecond[2];
    std::printf("==== RULE %u RESULT  HEAD %d : NEW %d : draw %d ====\n",
        ruleIndex, hw, nw, dr);
    std::printf("  NEW 执先 %d:%d:%d | 执后 %d:%d:%d  (新:HEAD:和)\n",
        newFirst[0], newFirst[1], newFirst[2],
        newSecond[0], newSecond[1], newSecond[2]);
    std::printf("  HEAD nodes=%lld timeMs=%lld timeout=%lld avgNPS=%lld\n",
        headSide.nodes, headSide.timeMs, headSide.timeoutHits,
        headSide.timeMs > 0 ? headSide.nodes * 1000 / headSide.timeMs : 0);
    std::printf("  NEW  nodes=%lld timeMs=%lld timeout=%lld avgNPS=%lld\n",
        newSide.nodes, newSide.timeMs, newSide.timeoutHits,
        newSide.timeMs > 0 ? newSide.nodes * 1000 / newSide.timeMs : 0);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    const int gamesPerSide = argc > 1 ? std::atoi(argv[1]) : 20;
    const int depth = argc > 2 ? std::atoi(argv[2]) : 10;
    const uint64_t seed = argc > 3 ? static_cast<uint64_t>(std::atoll(argv[3])) : 1ull;
    const uint32_t threads = argc > 4 ? static_cast<uint32_t>(std::atoi(argv[4])) : 8u;
    const int64_t timeMs = argc > 5 ? static_cast<int64_t>(std::atoll(argv[5])) : 5000;
    const int plyLimit = argc > 6 ? std::atoi(argv[6]) : 80;
    const int ruleLo = argc > 7 ? std::atoi(argv[7]) : 0;
    const int ruleHi = argc > 8 ? std::atoi(argv[8]) : RULE_COUNT - 1;
    const bool newTie = argc > 9 ? std::atoi(argv[9]) != 0 : true;
    const bool newPv = argc > 10 ? std::atoi(argv[10]) != 0 : true;
    const bool newHeadRow = argc > 11 ? std::atoi(argv[11]) != 0 : false;
    const int variant = argc > 12 ? std::atoi(argv[12]) : 0;
    const bool newLmr = argc > 13 ? std::atoi(argv[13]) != 0 : true;
    const int midGap = argc > 14 ? std::atoi(argv[14]) : 0;
    const bool parRoot = argc > 15 ? std::atoi(argv[15]) != 0 : true; // 引擎默认开
    const int repLead = argc > 16 ? std::atoi(argv[16]) : 0;
    const int flyBehind = argc > 17 ? std::atoi(argv[17]) : -1;
    const bool tieGroups = argc > 18 ? std::atoi(argv[18]) != 0 : true; // 引擎默认开
    if (gamesPerSide <= 0 || depth <= 0 || threads == 0 || timeMs <= 0
        || plyLimit <= 0 || ruleLo < 0 || ruleHi < ruleLo
        || ruleHi >= static_cast<int>(RULE_COUNT)
        || variant < 0 || variant > 7 || midGap < 0 || repLead < 0) {
        std::printf("usage: VsHeadCode.exe [gamesPerSide=20] [depth=10] [seed=1] [threads=8] [timeMs=5000] [plyLimit=80] [ruleLo=0] [ruleHi=3] [newTie=1] [newPv=1] [newHeadRow=0] [var=0..7] [newLmr=1] [midGap=0] [parRoot=1] [repLead=0] [flyBehind=-1] [tieGroups=1]\n");
        return 2;
    }

    std::printf("VsHeadCode: worktree engine vs git-HEAD engine snapshot (head_engine/) | d=%d th=%u t=%lldms ply<=%d n=%d/rule/side seed=%llu | TIE=%d PV=%d HEADROW=%d VAR=%d LMRFULL=%d MIDGAP=%d PARROOT=%d REPLEAD=%d FLYBEHIND=%d TIEGROUPS=%d | dynamicDepth=off\n",
        depth, threads, static_cast<long long>(timeMs), plyLimit, gamesPerSide,
        static_cast<unsigned long long>(seed),
        newTie ? 1 : 0, newPv ? 1 : 0, newHeadRow ? 1 : 0, variant,
        newLmr ? 1 : 0, midGap, parRoot ? 1 : 0, repLead, flyBehind,
        tieGroups ? 1 : 0);

    for (int rule = ruleLo; rule <= ruleHi; ++rule) {
        playRule(static_cast<uint32_t>(rule), gamesPerSide, depth, seed,
            threads, timeMs, plyLimit, newTie, newPv, newHeadRow, variant, newLmr, midGap,
            parRoot, repLead, flyBehind, tieGroups);
    }
    return 0;
}

// 确定性节点/耗时对照：单线程、固定种子、固定深度。
// 用途：验证"只影响排序/剪枝、不改变搜索结果"的改动（如根节点 PV 排序、
// mate distance 剪枝）是否真的降了节点数，而不引入对局噪声。
// 用法: OrderProbe.exe [rule=2] [depth=10] [positions=6] [warmPlies=20] [seed=1] [lmr=1] [pv=1] [equalRow=-1]
//       OrderProbeHead.exe（同参）= 同一份探测跑 git HEAD 引擎快照，
//       两个 exe 的输出逐行对照即得节点/耗时差（2026-10 起免 git stash 重建）。
//       lmr：lateMoveReductions 开关（0=关）；pv：rootPvOrdering 开关（0=关，与
//       HEAD 逐位对齐用）；equalRow>=0：双方都按 git HEAD 权重表第 <equalRow> 行
//       useCustomWeights 搜索（等权对照：根分可比，验证无意外价值漂移）。
//
// 关键约束：热身走子必须与"评估函数"无关，否则新旧引擎会走到不同局面，
// 节点数就不可比。这里不调用搜索，而是直接对"全部可能的命令文本"逐一
// 试探合法性（同一局面下两个构建的合法走法集合必然相同），按随机偏移取
// 第一个合法走法，因此每一步的选取只取决于种子与模型规则。
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "../NineChess/src/ninechess.h"
#ifdef PROBE_ENGINE_HEAD
#include "head_engine/ninechess_ai_ab.h"
using ProbeEngine = nchead::NineChess_AI_AB;
#define PROBE_ENGINE_TAG "HEAD"
#else
#include "../NineChess/src/ninechess_ai_ab.h"
using ProbeEngine = NineChess_AI_AB;
#define PROBE_ENGINE_TAG "WORK"
#endif

namespace {

// git HEAD（42980e5）的 s_evalWeightsPerRule 四行，等权对照用。
// 改 HEAD 快照时同步这里（对照 HEAD 表见 benchmark/results 各报告）。
const int32_t kHeadWeightRows[4][13] = {
    /* 0 */ { 128, 160, 180, 64, 128, 180, 64, 10, 160, 180, 180, 400, 0 },
    /* 1 */ { 128, 160, 180, 64, 128, 180, 64, 10, 160, 180, 180, 400, 0 },
    /* 2 */ { 128, 160, 0, 64, 180, 0, 64, 10, 160, 220, 0, 0, 0 },
    /* 3 */ { 128, 160, 180, 64, 128, 180, 64, 24, 160, 180, 180, 400, 0 },
};

// 把 HEAD 表行灌进任一引擎的 EvalWeights（列序 = EvalWeights 字段序）。
template <typename Engine>
typename Engine::EvalWeights headRowWeights(int ruleIndex)
{
    typename Engine::EvalWeights w;
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
    return w;
}

// 生成全部候选命令文本（落子 / 走子 / 提子），一次性构建后复用。
const std::vector<std::string>& allCommandTexts()
{
    static std::vector<std::string> texts;
    if (!texts.empty()) {
        return texts;
    }
    char buf[32] = {};
    for (int c = 0; c < 3; ++c) {
        for (int p = 0; p < 8; ++p) {
            std::snprintf(buf, sizeof(buf), "(%d,%d)", c, p);
            texts.emplace_back(buf);
        }
    }
    for (int c1 = 0; c1 < 3; ++c1) {
        for (int p1 = 0; p1 < 8; ++p1) {
            for (int c2 = 0; c2 < 3; ++c2) {
                for (int p2 = 0; p2 < 8; ++p2) {
                    std::snprintf(buf, sizeof(buf), "(%d,%d)->(%d,%d)", c1, p1, c2, p2);
                    texts.emplace_back(buf);
                }
            }
        }
    }
    for (int c = 0; c < 3; ++c) {
        for (int p = 0; p < 8; ++p) {
            std::snprintf(buf, sizeof(buf), "-(%d,%d)", c, p);
            texts.emplace_back(buf);
        }
    }
    return texts;
}

// 与评估无关的随机走子：枚举全部合法命令后按均匀分布抽取。
// （只依赖模型合法性，因此同一局面在新旧构建里得到同一份候选集。）
bool randomLegalMove(NineChess& game, std::mt19937_64& rng)
{
    const std::vector<std::string>& texts = allCommandTexts();
    std::vector<const std::string*> legal;
    legal.reserve(texts.size());
    for (const std::string& cmd : texts) {
        NineChess probe = game;
        if (probe.command(cmd.c_str())) {
            legal.push_back(&cmd);
        }
    }
    if (legal.empty()) {
        return false;
    }
    const size_t pick = static_cast<size_t>(rng() % legal.size());
    return game.command(legal[pick]->c_str());
}

void warmUp(NineChess& game, int plies, uint64_t seed)
{
    std::mt19937_64 rng(seed);
    for (int i = 0; i < plies; ++i) {
        if (game.whoWin() != NineChess::NOBODY) {
            break;
        }
        if (!randomLegalMove(game, rng)) {
            break;
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    const uint32_t rule = argc > 1 ? static_cast<uint32_t>(std::atoi(argv[1])) : 2u;
    const int depth = argc > 2 ? std::atoi(argv[2]) : 10;
    const int positions = argc > 3 ? std::atoi(argv[3]) : 6;
    const int warmPlies = argc > 4 ? std::atoi(argv[4]) : 20;
    const uint64_t seed = argc > 5 ? static_cast<uint64_t>(std::atoll(argv[5])) : 1ull;
    const bool lmr = argc > 6 ? std::atoi(argv[6]) != 0 : true;
    const bool pv = argc > 7 ? std::atoi(argv[7]) != 0 : true;
    const int equalRow = argc > 8 ? std::atoi(argv[8]) : -1;
    if (positions <= 0 || depth <= 0) {
        std::printf("usage: OrderProbe.exe [rule0-3] [depth] [positions] [warmPlies] [seed] [lmr] [pv] [equalRow]\n");
        return 2;
    }

    std::printf("OrderProbe[%s] rule=%u depth=%d positions=%d warmPlies=%d seed=%llu lmr=%d pv=%d equalRow=%d\n",
        PROBE_ENGINE_TAG, rule, depth, positions, warmPlies,
        static_cast<unsigned long long>(seed), lmr ? 1 : 0, pv ? 1 : 0, equalRow);

    ProbeEngine searcher;
    ProbeEngine::SearchOptions opt;
    opt.timeLimitMs = 0;
    opt.threads = 1;
    opt.randomness = 0;   // 纯最优，保证可复现
    opt.lateMoveReductions = lmr;
#ifndef PROBE_ENGINE_HEAD
    opt.rootPvOrdering = pv; // HEAD 快照无此选项（恒关）
#endif
    if (equalRow >= 0 && equalRow < 4) {
        opt.useCustomWeights = true;
        opt.weights = headRowWeights<ProbeEngine>(equalRow);
    }
    searcher.setOptions(opt);

    long long totalNodes = 0;
    long long totalTimeMs = 0;
    for (int p = 0; p < positions; ++p) {
        NineChess game;
        game.setRule(rule);
        game.start();
        warmUp(game, warmPlies, seed + static_cast<uint64_t>(p) * 1000ull + 7ull);
        if (game.whoWin() != NineChess::NOBODY) {
            std::printf("  pos%2d : 已终局，跳过\n", p);
            continue;
        }
        searcher.setChess(game);
        searcher.clearTranspositionTable();
        const int value = searcher.alphaBetaPruning(depth);
        ProbeEngine::SearchStats stats;
        searcher.snapshotStats(stats);
        const int64_t ms = searcher.getLastSearchTimeMs();
        totalNodes += static_cast<long long>(stats.nodes.load());
        totalTimeMs += ms;
        std::printf("  pos%2d : cmds=%d doneDepth=%d value=%d nodes=%llu timeMs=%lld pv=%s\n",
            p, static_cast<int>(game.getCmdList()->size()), searcher.getLastCompletedDepth(),
            value, static_cast<unsigned long long>(stats.nodes.load()), static_cast<long long>(ms),
            searcher.bestMove());
    }

    std::printf("TOTAL nodes=%lld timeMs=%lld nps=%lld\n",
        totalNodes, totalTimeMs,
        totalTimeMs > 0 ? totalNodes * 1000 / totalTimeMs : 0);
    return 0;
}

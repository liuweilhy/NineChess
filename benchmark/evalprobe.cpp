// 评估评分标准验证器：按 AI_ALPHABETA.md §1.8 记载的评分标准独立重算静态评估，
// 与引擎 depth-0 静态评估（alphaBetaPruning(0)）逐局面比对，验证文档与代码一致。
// 比对样本 = 固定构造场景 + 随机对局全程逐手（覆盖四套规则与多组选项配置）。
// 说明：stepsRemainingHint 急迫分需要控制层注入剩余步数，四组配置均为关闭，
// 其口径（每子每步 +8、剩余 <24 步、领先封顶 3 子）见 §1.8 文字描述。
// 用法: EvalProbe.exe [每配置随机对局数，默认 120]
#include <bitset>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "../NineChess/src/ninechess.h"
#include "../NineChess/src/ninechess_ai_ab.h"

using ::Rule;
using ::ChessData;
using ::MillKey;

// 探针局面：公有继承，把评估所需的引擎内部表（受保护）以只读形式暴露。
class ProbeChess : public NineChess
{
public:
    uint32_t validMask() const { return m_validBoardMask; }
    uint32_t lineCount() const { return m_lineCount; }
    uint32_t lineMask(int lineId) const { return m_lineMasks[lineId]; }
    int posLineCountAt(int pos) const { return static_cast<int>(m_posLineCount[pos]); }
    bool flyable(NineChess::Players player) const { return canFly(player); }
    MillKey millKeyOf(NineChess::Players player, uint32_t lineId) const
    {
        return makeMillKeyForLine(player, lineId);
    }
};

static inline int pc32(uint32_t x)
{
    return static_cast<int>(std::bitset<32>(x).count());
}

static inline int clampScore(int value, int lo, int hi)
{
    return value < lo ? lo : (value > hi ? hi : value);
}

// ============ 评分标准的独立实现（严格按文档 §1.8，P1 视角） ============

struct SpecConfig {
    // EvalWeights 默认行。
    int openingMaterial = 120, openingInHand = 48, openingMill = 96, openingOpenMill = 24;
    int midMaterial = 180, midMill = 112, midOpenMill = 32, midMobility = 10;
    int captureOpening = 160, captureMid = 220;
    int forkThreat = 0, stalematePressure = 0, millSafety = 0;
    // SearchOptions 中影响评估的项。
    int pointValueWeight = 16;
    int winPressureWeight = 150;
    bool taperedEval = false;
};

struct Breakdown {
    int phaseT = 0;
    int material = 0, inHand = 0, mill = 0, openMill = 0, mobilityTerm = 0;
    int capture = 0, pointValue = 0, winPressure = 0;
    int stalemate = 0, fork = 0, millSafety = 0;
    int claimable1 = 0, claimable2 = 0, open1 = 0, open2 = 0;
    int mob1 = 0, mob2 = 0, pv1 = 0, pv2 = 0, total1 = 0, total2 = 0;
};

static bool millKeySeen(const ProbeChess& chess, NineChess::Players player, uint32_t lineId)
{
    const MillKey key = chess.millKeyOf(player, lineId);
    const std::vector<MillKey>& history = chess.getData().millHistory;
    for (const MillKey& entry : history) {
        if (entry == key) {
            return true;
        }
    }
    return false;
}

static int specMobility(const ProbeChess& chess, NineChess::Players player,
    uint32_t occupied, bool fly)
{
    if (chess.getAction() == NineChess::ACTION_CAPTURE && chess.getTurn() == player) {
        return static_cast<int>(chess.getPendingCaptures());
    }
    if (chess.getAction() == NineChess::ACTION_PLACE
        && chess.getTurn() == player
        && chess.isValidPos(chess.getCurrentPos())) {
        if (fly) {
            return pc32(~occupied & chess.validMask());
        }
        return pc32(chess.getMoveMask(chess.getCurrentPos()) & ~occupied);
    }
    uint32_t pieces = (player == NineChess::PLAYER1 ? chess.getData().player1Board
                                                    : chess.getData().player2Board)
        & chess.validMask();
    if (fly) {
        return pc32(pieces) * pc32(~occupied & chess.validMask());
    }
    int mobility = 0;
    while (pieces != 0u) {
        const int pos = CTZ32(pieces);
        mobility += pc32(chess.getMoveMask(pos) & ~occupied);
        pieces &= pieces - 1u;
    }
    return mobility;
}

static int specEval(const ProbeChess& chess, const SpecConfig& cfg, Breakdown& bd)
{
    const Rule& rule = *chess.getRule();
    const ChessData& data = chess.getData();

    if (chess.getPhase() == NineChess::GAME_OVER) {
        if (chess.getWinner() == NineChess::PLAYER1) {
            return 30000;
        }
        if (chess.getWinner() == NineChess::PLAYER2) {
            return -30000;
        }
        return 0;
    }

    // 阶段插值：默认硬切换（未开局/摆子=256 用开局列，走子=0 用中局列）。
    if (cfg.taperedEval) {
        const int pps = static_cast<int>(rule.piecesPerSide);
        const int totalInHand = static_cast<int>(chess.getPlayer1InHand())
            + static_cast<int>(chess.getPlayer2InHand());
        bd.phaseT = pps > 0
            ? (totalInHand * 256 / (2 * pps) > 256 ? 256 : totalInHand * 256 / (2 * pps))
            : 0;
    }
    else {
        bd.phaseT = (chess.getPhase() == NineChess::GAME_NOTSTARTED
            || chess.getPhase() == NineChess::GAME_OPENING) ? 256 : 0;
    }
    auto blend = [&](int open, int mid) {
        return (open * bd.phaseT + mid * (256 - bd.phaseT)) / 256;
    };

    const uint32_t valid = chess.validMask();
    const uint32_t board1 = data.player1Board & valid;
    const uint32_t board2 = data.player2Board & valid;
    const uint32_t occupied = (data.player1Board | data.player2Board | data.forbiddenBoard) & valid;

    // 单遍线扫描：可提三连 / 活二 / 成三点（威胁点去重）/ 三连中棋子掩码。
    const bool repeatedMills = rule.allowRepeatedMills;
    int claimable1 = 0, claimable2 = 0, open1 = 0, open2 = 0, fork1 = 0, fork2 = 0;
    uint32_t inMill1 = 0, inMill2 = 0, threat1 = 0, threat2 = 0;
    for (uint32_t lineId = 0; lineId < chess.lineCount(); ++lineId) {
        const uint32_t mask = chess.lineMask(static_cast<int>(lineId));
        const uint32_t bits1 = board1 & mask;
        const uint32_t bits2 = board2 & mask;
        const uint32_t occBits = occupied & mask;
        if (bits1 == mask) {
            inMill1 |= mask;
            if (repeatedMills || !millKeySeen(chess, NineChess::PLAYER1, lineId)) {
                ++claimable1;
            }
        }
        else if (pc32(bits1) == 2 && occBits == bits1) {
            ++open1;
            threat1 |= mask & ~occupied;
        }
        if (bits2 == mask) {
            inMill2 |= mask;
            if (repeatedMills || !millKeySeen(chess, NineChess::PLAYER2, lineId)) {
                ++claimable2;
            }
        }
        else if (pc32(bits2) == 2 && occBits == bits2) {
            ++open2;
            threat2 |= mask & ~occupied;
        }
    }
    fork1 = pc32(threat1);
    fork2 = pc32(threat2);

    // 点位结构价值：每点价值 = 该点穿线数（引擎线表 m_posLineCount）。
    int pv1 = 0, pv2 = 0;
    if (cfg.pointValueWeight != 0) {
        for (uint32_t bits = board1; bits != 0u; bits &= bits - 1u) {
            pv1 += chess.posLineCountAt(CTZ32(bits));
        }
        for (uint32_t bits = board2; bits != 0u; bits &= bits - 1u) {
            pv2 += chess.posLineCountAt(CTZ32(bits));
        }
    }

    const int mob1 = chess.getPhase() == NineChess::GAME_MID
        ? specMobility(chess, NineChess::PLAYER1, occupied, chess.flyable(NineChess::PLAYER1)) : 0;
    const int mob2 = chess.getPhase() == NineChess::GAME_MID
        ? specMobility(chess, NineChess::PLAYER2, occupied, chess.flyable(NineChess::PLAYER2)) : 0;

    const int onBoardDiff = pc32(board1) - pc32(board2);
    const int inHandDiff = static_cast<int>(chess.getPlayer1InHand())
        - static_cast<int>(chess.getPlayer2InHand());
    const int inMillDiff = pc32(board1 & inMill1) - pc32(board2 & inMill2);

    int score = 0;
    bd.material = onBoardDiff * blend(cfg.openingMaterial, cfg.midMaterial);
    bd.inHand = inHandDiff * cfg.openingInHand;
    bd.mill = (claimable1 - claimable2) * blend(cfg.openingMill, cfg.midMill);
    bd.openMill = (open1 - open2) * blend(cfg.openingOpenMill, cfg.midOpenMill);
    bd.mobilityTerm = (mob1 - mob2) * cfg.midMobility;
    score += bd.material + bd.inHand + bd.mill + bd.openMill + bd.mobilityTerm;

    if (cfg.stalematePressure != 0 && chess.getPhase() == NineChess::GAME_MID) {
        if (mob2 <= 3) {
            bd.stalemate += (4 - mob2) * cfg.stalematePressure;
        }
        if (mob1 <= 3) {
            bd.stalemate -= (4 - mob1) * cfg.stalematePressure;
        }
        score += bd.stalemate;
    }

    if (cfg.forkThreat != 0) {
        const int forkBonus1 = fork1 >= 2 ? fork1 - 1 : 0;
        const int forkBonus2 = fork2 >= 2 ? fork2 - 1 : 0;
        bd.fork = (forkBonus1 - forkBonus2) * cfg.forkThreat;
        score += bd.fork;
    }

    if (cfg.millSafety != 0) {
        bd.millSafety = inMillDiff * cfg.millSafety;
        score += bd.millSafety;
    }

    bd.pointValue = (pv1 - pv2) * cfg.pointValueWeight;
    score += bd.pointValue;

    if (chess.getAction() == NineChess::ACTION_CAPTURE) {
        const int bonus = blend(cfg.captureOpening, cfg.captureMid)
            * static_cast<int>(chess.getPendingCaptures());
        bd.capture = chess.getTurn() == NineChess::PLAYER1 ? bonus : -bonus;
        score += bd.capture;
    }

    const int total1 = static_cast<int>(chess.getPlayer1OnBoardCount())
        + static_cast<int>(chess.getPlayer1InHand());
    const int total2 = static_cast<int>(chess.getPlayer2OnBoardCount())
        + static_cast<int>(chess.getPlayer2InHand());
    if (cfg.winPressureWeight != 0) {
        const int threshold = static_cast<int>(rule.minPiecesToSurvive) + 2;
        const int pressure1 = total2 <= threshold
            ? (threshold - total2 + 1) * cfg.winPressureWeight : 0;
        const int pressure2 = total1 <= threshold
            ? (threshold - total1 + 1) * cfg.winPressureWeight : 0;
        bd.winPressure = pressure1 - pressure2;
        score += bd.winPressure;
    }

    bd.claimable1 = claimable1; bd.claimable2 = claimable2;
    bd.open1 = open1; bd.open2 = open2;
    bd.mob1 = mob1; bd.mob2 = mob2;
    bd.pv1 = pv1; bd.pv2 = pv2;
    bd.total1 = total1; bd.total2 = total2;

    return clampScore(score, -30000, 30000);
}

// ============ 引擎侧：depth-0 搜索返回纯静态评估 ============

static int engineEval(NineChess_AI_AB& ai, const ProbeChess& chess,
    const NineChess_AI_AB::SearchOptions& opts)
{
    ai.setOptions(opts);
    ai.setChess(chess);
    return ai.alphaBetaPruning(0);
}

static NineChess_AI_AB::SearchOptions baseOptions()
{
    NineChess_AI_AB::SearchOptions opts;
    opts.threads = 1;
    opts.randomness = 0;
    opts.dynamicDepth = false;
    opts.timeLimitMs = 0;
    return opts;
}

// ============ 固定场景 ============

struct ScenarioResult {
    std::string name;
    int expected;
    int actual;
    bool pass;
};

static std::vector<ScenarioResult> runScenarios(NineChess_AI_AB& ai)
{
    std::vector<ScenarioResult> results;
    auto check = [&](const char* name, int expected, int actual) {
        results.push_back({ name, expected, actual, expected == actual });
    };

    // 场景 1：规则 2 摆子阶段 2:1 子差（全角点）。
    // 在盘差 +1×120；手牌差 -1×48；点位价值 (2+2-2)×16=32（直棋规则每点 2 线）。
    {
        ProbeChess chess;
        chess.setRule(2);
        chess.command("(0,0)");
        chess.command("(0,4)");
        chess.command("(0,2)");
        check("S1 规则2开局 2:1 子差(全角点)", 120 - 48 + 32,
            engineEval(ai, chess, baseOptions()));
    }

    // 场景 2：规则 0 摆子阶段成三待提（规则 0 允许重复提子，站立三连计入可提）。
    // 三连线 = (0,7)(0,0)(0,1)（边中-角-边中）。在盘差 +1×120；手牌差 -1×48；
    // 可提三连 +1×96；点位价值 2×16=32；提子阶段（开局价）+1×160。
    {
        ProbeChess chess;
        chess.setRule(0);
        chess.command("(0,7)");
        chess.command("(1,3)");
        chess.command("(0,0)");
        chess.command("(2,4)");
        chess.command("(0,1)");
        check("S2 规则0开局成三待提 1 子", 120 - 48 + 96 + 32 + 160,
            engineEval(ai, chess, baseOptions()));
    }

    // 场景 3：同局面换规则 2（九连棋：三连在成形瞬间已登记历史，
    // 站立三连不再计入"可提三连"，故无 +96 项）。
    {
        ProbeChess chess;
        chess.setRule(2);
        chess.command("(0,7)");
        chess.command("(1,3)");
        chess.command("(0,0)");
        chess.command("(2,4)");
        chess.command("(0,1)");
        check("S3 规则2开局成三待提(已登记不计可提)", 120 - 48 + 32 + 160,
            engineEval(ai, chess, baseOptions()));
    }

    return results;
}

// ============ 随机对局比对 ============

// 观测计数：各规则下"可提三连 > 0"与"活二 > 0"出现过的状态数（验证文档口径）。
static long long g_claimableSeen[4] = {};
static long long g_openMillSeen[4] = {};
static long long g_statesSeen[4] = {};

static bool runFuzz(NineChess_AI_AB& ai, const char* configName,
    const NineChess_AI_AB::SearchOptions& opts, const SpecConfig& cfg,
    int games, std::mt19937& rng)
{
    for (int game = 0; game < games; ++game) {
        ProbeChess chess;
        chess.setRule(static_cast<uint32_t>(rng() % 4));

        for (int ply = 0; ply < 600; ++ply) {
            if (chess.getPhase() == NineChess::GAME_OVER) {
                break;
            }

            // 文档口径重算 vs 引擎 depth-0 静态评估。
            Breakdown bd;
            const int expected = specEval(chess, cfg, bd);
            const int actual = engineEval(ai, chess, opts);
            const uint32_t ruleIdx = chess.getRuleIndex();
            ++g_statesSeen[ruleIdx];
            if (bd.claimable1 > 0 || bd.claimable2 > 0) {
                ++g_claimableSeen[ruleIdx];
            }
            if (bd.open1 > 0 || bd.open2 > 0) {
                ++g_openMillSeen[ruleIdx];
            }
            if (expected != actual) {
                std::printf("[MISMATCH] 配置=%s 第%d局 第%d手 期望=%d 实际=%d\n",
                    configName, game, ply, expected, actual);
                std::printf("%s", chess.getConsoleText().c_str());
                std::printf("phaseT=%d material=%d inHand=%d mill=%d(%d-%d) openMill=%d(%d-%d)\n",
                    bd.phaseT, bd.material, bd.inHand, bd.mill, bd.claimable1, bd.claimable2,
                    bd.openMill, bd.open1, bd.open2);
                std::printf("mobility=%d(%d-%d) capture=%d pointValue=%d(%d-%d) winPressure=%d\n",
                    bd.mobilityTerm, bd.mob1, bd.mob2, bd.capture,
                    bd.pointValue, bd.pv1, bd.pv2, bd.winPressure);
                std::printf("stalemate=%d fork=%d millSafety=%d total=(%d vs %d)\n",
                    bd.stalemate, bd.fork, bd.millSafety, bd.total1, bd.total2);
                return false;
            }

            // 生成当前局面的合法候选命令。
            const Rule& rule = *chess.getRule();
            std::vector<std::string> candidates;
            char buf[32];
            if (chess.getAction() == NineChess::ACTION_CAPTURE) {
                for (int pos = 0; pos < NineChess::BOARD_SIZE; ++pos) {
                    if (chess.canCapturePos(pos)) {
                        int c = 0, p = 0;
                        chess.posToCP(pos, c, p);
                        std::snprintf(buf, sizeof(buf), "-(%d,%d)", c, p);
                        candidates.emplace_back(buf);
                    }
                }
            }
            else if (chess.getPhase() == NineChess::GAME_OPENING
                || chess.getPhase() == NineChess::GAME_NOTSTARTED) {
                for (int pos = 0; pos < NineChess::BOARD_SIZE; ++pos) {
                    if (chess.canPlacePos(pos)) {
                        int c = 0, p = 0;
                        chess.posToCP(pos, c, p);
                        std::snprintf(buf, sizeof(buf), "(%d,%d)", c, p);
                        candidates.emplace_back(buf);
                    }
                }
            }
            else if (chess.getPhase() == NineChess::GAME_MID) {
                const bool fly = chess.flyable(chess.getTurn());
                const uint32_t occupied = (chess.getData().player1Board
                    | chess.getData().player2Board | chess.getData().forbiddenBoard)
                    & chess.validMask();
                for (int src = 0; src < NineChess::BOARD_SIZE; ++src) {
                    if (chess.getWhosPiecePos(src) != chess.getTurn()) {
                        continue;
                    }
                    uint32_t dests = chess.getMoveMask(src) & ~occupied;
                    if (fly) {
                        dests = ~occupied & chess.validMask();
                    }
                    int c1 = 0, p1 = 0;
                    chess.posToCP(src, c1, p1);
                    for (uint32_t d = dests; d != 0u; d &= d - 1u) {
                        int c2 = 0, p2 = 0;
                        chess.posToCP(CTZ32(d), c2, p2);
                        std::snprintf(buf, sizeof(buf), "(%d,%d)->(%d,%d)", c1, p1, c2, p2);
                        candidates.emplace_back(buf);
                    }
                }
            }

            if (candidates.empty()) {
                break; // 无路可走（被闷/终局前夜），交由下一局。
            }
            std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
            bool moved = false;
            for (size_t tries = 0; tries < candidates.size() && !moved; ++tries) {
                moved = chess.command(candidates[pick(rng)].c_str());
            }
            if (!moved) {
                break;
            }
        }
    }
    std::printf("  [PASS] 配置=%s  %d 局随机对局逐手比对一致\n", configName, games);
    return true;
}

int main(int argc, char** argv)
{
    const int games = argc > 1 ? std::atoi(argv[1]) : 120;
    std::mt19937 rng(20260913u);

    NineChess_AI_AB ai;

    // 线表几何打印：点位结构价值的单价来源（每点穿线数）。
    std::printf("== 各规则每点穿线数（点位 0..23，点位结构价值的单价来源）==\n");
    for (uint32_t r = 0; r < 4; ++r) {
        ProbeChess chess;
        chess.setRule(r);
        std::printf("规则%u(%s) 线数=%u ：", r, NineChess::rules[r].name, chess.lineCount());
        for (int pos = 0; pos < NineChess::BOARD_SIZE; ++pos) {
            std::printf("%d", chess.posLineCountAt(pos));
        }
        std::printf("\n");
    }

    std::printf("== 固定场景 ==\n");
    bool ok = true;
    for (const ScenarioResult& r : runScenarios(ai)) {
        std::printf("  [%s] %s ：期望=%d 实际=%d\n",
            r.pass ? "PASS" : "FAIL", r.name.c_str(), r.expected, r.actual);
        ok = ok && r.pass;
    }

    std::printf("== 随机对局逐手比对 ==\n");
    // 配置 0：全默认（winPressure=150、pointValue=16、休眠项 0、硬切换）。
    {
        NineChess_AI_AB::SearchOptions opts = baseOptions();
        ok = runFuzz(ai, "默认", opts, SpecConfig{}, games, rng) && ok;
    }
    // 配置 1：休眠项启用（被闷杀 60 / 双三 80 / 三连保护 20）。
    {
        NineChess_AI_AB::SearchOptions opts = baseOptions();
        opts.stalematePressureWeight = 60;
        opts.forkThreatWeight = 80;
        opts.millSafetyWeight = 20;
        SpecConfig cfg;
        cfg.stalematePressure = 60;
        cfg.forkThreat = 80;
        cfg.millSafety = 20;
        ok = runFuzz(ai, "休眠项启用", opts, cfg, games, rng) && ok;
    }
    // 配置 2：连续博弈插值。
    {
        NineChess_AI_AB::SearchOptions opts = baseOptions();
        opts.taperedEval = true;
        SpecConfig cfg;
        cfg.taperedEval = true;
        ok = runFuzz(ai, "tapered", opts, cfg, games, rng) && ok;
    }
    // 配置 3：表外项全关（winPressure=0、pointValue=0）。
    {
        NineChess_AI_AB::SearchOptions opts = baseOptions();
        opts.winPressureWeight = 0;
        opts.pointValueWeight = 0;
        SpecConfig cfg;
        cfg.winPressureWeight = 0;
        cfg.pointValueWeight = 0;
        ok = runFuzz(ai, "表外项全关", opts, cfg, games, rng) && ok;
    }

    std::printf("== 可提三连/活二 观测统计（默认配置样本）==\n");
    for (uint32_t r = 0; r < 4; ++r) {
        std::printf("规则%u：状态 %lld 个，可提三连>0 的 %lld 个，活二>0 的 %lld 个\n",
            r, g_statesSeen[r], g_claimableSeen[r], g_openMillSeen[r]);
    }

    std::printf(ok ? "== 全部一致 ==\n" : "== 存在不一致，见上方明细 ==\n");
    return ok ? 0 : 1;
}

// 搜索语义回归（白盒）：静默搜索强制提子、浅层 TT 排序复用、根随机/等价局面。
// 配套 quiescenceStrictCapture / ttMoveFromShallow 两个开关（默认 false = 旧行为），
// 用独立重实现的"强制提子链极小极大"做对照（方法学与 EvalProbe 相同：
// 被测点独立重写，非提子叶子交给引擎自身静默搜索，保证只有被测语义参与比对）。
// 根随机部分用公共 API 验证：同种子可复现、空盘 24 等价落点跨种子多样且全部
// 合法、全程层（randomGapMid=0）只从精确同分候选中选择、后手根视角、
// 跨种子根分表逐位一致。
// 用法: SearchRegression.exe [快速=0]（快速=1 时把自对弈烟雾测试缩到每规则 1 局）
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "../NineChess/src/ninechess.h"

#define NINECHESS_AI_AB_PROBE_FRIEND
#include "../NineChess/src/ninechess_ai_ab.h"

using ::NineChess;

// ============ 白盒探针：友元类，转发被测私有成员 ============
// 必须位于全局作用域：ninechess_ai_ab.h 中的 friend 声明
// （NINECHESS_AI_AB_PROBE_FRIEND 下）指名的是全局作用域类。

class NineChessAiAbProbeAccess {
public:
    using Engine = NineChess_AI_AB;
    using Context = Engine::SearchContext;
    using Move = Engine::Move;
    using MoveList = Engine::MoveList;
    using Snapshot = Engine::Snapshot;

    // 用引擎的根局面构建搜索上下文（setChess 已剥离命令历史），
    // 并全量重建 hard 哈希累加器与编号规则历史三连查表。
    // SearchContext 含原子成员不可拷贝/移动，只能出参填充（其余字段均有默认初始化）。
    static void makeContext(const Engine& engine, Context& ctx)
    {
        ctx.board = engine.m_root;
        engine.resetHardHashAccum(ctx);
    }

    static void generateMoves(const Engine& engine, Context& ctx, MoveList& list)
    {
        engine.generateMoves(ctx, list);
    }

    static bool moveIsNone(const Move& move)
    {
        return move.type == Engine::MOVE_NONE;
    }

    static bool moveEquals(const Move& lhs, const Move& rhs)
    {
        return lhs.type == rhs.type && lhs.from == rhs.from && lhs.to == rhs.to;
    }

    static int quiescence(Engine& engine, Context& ctx, int alpha, int beta,
        int ply, int qdepth)
    {
        return engine.quiescence(ctx, alpha, beta, ply, qdepth);
    }

    static int evaluate(Engine& engine, Context& ctx, int ply)
    {
        return engine.evaluate(ctx, ply);
    }

    static void generateCaptureMoves(const Engine& engine, Context& ctx, MoveList& list)
    {
        engine.generateCaptureMoves(ctx, list);
    }

    static void applyMove(Engine& engine, Context& ctx, const Move& move, Snapshot& snap)
    {
        engine.applyMove(ctx, move, snap);
    }

    static void undoMove(Engine& engine, Context& ctx, const Snapshot& snap)
    {
        engine.undoMove(ctx, snap);
    }

    static uint64_t makeSearchHash(const Engine& engine, Context& ctx)
    {
        return engine.makeSearchHash(ctx);
    }

    static void storeTransposition(const Engine& engine, Context& ctx, uint64_t hash,
        int depth, int value, int alpha, int beta, const Move& bestMove, int ply)
    {
        engine.storeTransposition(ctx, hash, depth, value, alpha, beta, bestMove, ply);
    }

    static bool probeTransposition(const Engine& engine, Context& ctx, uint64_t hash,
        int depth, int& alpha, int& beta, int& value, Move& ttMove, int ply)
    {
        return engine.probeTransposition(ctx, hash, depth, alpha, beta, value, ttMove, ply);
    }
};

namespace {

// 与 NineChess_AI_AB 的私有常量保持一致（WIN_SCORE=30000 < INF，胜负分不越界）。
constexpr int kInf = 32000;
constexpr int kQSearchCap = 12; // = NineChess_AI_AB::kMaxQuiescenceDepth

using Probe = ::NineChessAiAbProbeAccess;

// ============ 公共工具 ============

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    if (!ok) {
        ++g_failures;
        std::printf("  [FAIL] %s\n", what.c_str());
    }
}

// 生成全部候选命令文本并按模型合法性枚举（与 OrderProbe 同口径）。
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

std::vector<std::string> rootScoreCommands(const std::string& text)
{
    std::vector<std::string> cmds;
    size_t begin = 0;
    while (begin < text.size()) {
        size_t end = text.find('\n', begin);
        if (end == std::string::npos) {
            end = text.size();
        }
        const size_t sep = text.find(" = ", begin);
        if (sep != std::string::npos && sep < end) {
            cmds.emplace_back(text.substr(begin, sep - begin));
        }
        begin = end + 1;
    }
    return cmds;
}

std::vector<int> rootScoreValues(const std::string& text)
{
    std::vector<int> values;
    size_t begin = 0;
    while (begin < text.size()) {
        size_t end = text.find('\n', begin);
        if (end == std::string::npos) {
            end = text.size();
        }
        const size_t sep = text.find(" = ", begin);
        if (sep != std::string::npos && sep < end) {
            values.push_back(std::atoi(text.substr(sep + 3, end - sep - 3).c_str()));
        }
        begin = end + 1;
    }
    return values;
}

// Engine 含原子成员不可拷贝/移动，只能出参构造。
void makeEngine(uint64_t seed, bool qstrict, bool ttshallow, Probe::Engine& engine)
{
    Probe::Engine::SearchOptions options;
    options.timeLimitMs = 0;
    options.threads = 1;
    options.randomness = 0;
    options.dynamicDepth = false;
    options.quiescenceStrictCapture = qstrict;
    options.ttMoveFromShallow = ttshallow;
    options.seed = seed;
    engine.setOptions(options);
}

// ============ A. 静默搜索：强制提子节点不允许 stand-pat ============

// 独立重实现的"强制提子链"静默搜索：
// - 强制提子节点：不做 stand-pat，穷举全部提子并按极小/极大递归（被测语义）；
// - 非提子节点/链长封顶/终局：交给引擎自身静默搜索（只托底，不重写）。
int refCaptureQsearch(Probe::Engine& engine, Probe::Context& ctx,
    int alpha, int beta, int ply, int qdepth)
{
    if (qdepth >= kQSearchCap || ctx.board.getPhase() == NineChess::GAME_OVER) {
        return Probe::evaluate(engine, ctx, ply);
    }
    const bool maximizing = ctx.board.getTurn() == NineChess::PLAYER1;
    if (ctx.board.getAction() != NineChess::ACTION_CAPTURE) {
        return Probe::quiescence(engine, ctx, alpha, beta, ply, qdepth);
    }
    Probe::MoveList moves;
    Probe::generateCaptureMoves(engine, ctx, moves);
    if (moves.count == 0) {
        return Probe::evaluate(engine, ctx, ply);
    }
    // 与引擎静默搜索完全同序（order 降序）：窄窗口下 fail-high 返回的界值
    // 依赖分支顺序，不同序会让"独立重实现"在边界上与引擎假性不一致。
    std::sort(moves.moves.begin(),
        moves.moves.begin() + static_cast<std::ptrdiff_t>(moves.count),
        [](const Probe::Move& lhs, const Probe::Move& rhs) {
            return lhs.order > rhs.order;
        });
    int best = maximizing ? -kInf : kInf;
    for (size_t i = 0; i < moves.count; ++i) {
        Probe::Snapshot snap;
        Probe::applyMove(engine, ctx, moves.moves[i], snap);
        const int value = refCaptureQsearch(engine, ctx, alpha, beta, ply + 1, qdepth + 1);
        Probe::undoMove(engine, ctx, snap);
        if (maximizing) {
            if (value > best) {
                best = value;
            }
            if (best > alpha) {
                alpha = best;
            }
        }
        else {
            if (value < best) {
                best = value;
            }
            if (best < beta) {
                beta = best;
            }
        }
        if (alpha >= beta) {
            break;
        }
    }
    return best;
}

// 构造"第 target 侧第 3 颗落子成三"的强制提子根局面：
// target 的三次落子逐一穷举，对手每步固定取枚举序第一个合法命令；
// target=P1 需 5 手（第 5 手即其第 3 落），target=P2 需 6 手（第 6 手即其第 3 落）。
// 中途若对手成三（进入提子状态）则放弃该分支，保证根局面只含目标方的提子任务。
bool findCaptureRoot(uint32_t rule, NineChess::Players target,
    std::vector<std::string>& outCommands)
{
    const std::vector<std::string>& texts = allCommandTexts();
    const int totalPlies = target == NineChess::PLAYER1 ? 5 : 6;
    std::vector<std::string> seq;
    std::function<bool(const NineChess&, int)> dfs =
        [&](const NineChess& game, int ply) -> bool {
        if (ply >= totalPlies) {
            return false;
        }
        const bool targetTurn = (target == NineChess::PLAYER1) == (ply % 2 == 0);
        const bool lastPly = ply == totalPlies - 1;
        for (const std::string& cmd : texts) {
            NineChess next = game;
            if (!next.command(cmd.c_str())) {
                continue;
            }
            seq.push_back(cmd);
            if (lastPly && targetTurn) {
                if (next.getAction() == NineChess::ACTION_CAPTURE
                    && next.getTurn() == target
                    && next.whoWin() == NineChess::NOBODY) {
                    outCommands = seq;
                    return true;
                }
                seq.pop_back();
                continue;
            }
            if (next.whoWin() != NineChess::NOBODY
                || next.getPhase() == NineChess::GAME_OVER
                || (!targetTurn && next.getAction() == NineChess::ACTION_CAPTURE)) {
                seq.pop_back();
                continue;
            }
            const bool deeper = dfs(next, ply + 1);
            if (deeper) {
                return true;
            }
            seq.pop_back();
            if (!targetTurn) {
                // 对手固定取枚举序第一个合法命令：首个合法着失败即整支失败，
                // 不再枚举其余对手着法（保证枚举量 = target 侧三次落子的组合）。
                return false;
            }
        }
        return false;
    };
    NineChess game;
    game.setRule(rule);
    game.start();
    return dfs(game, 0);
}

void testQuiescenceStrictCapture()
{
    std::printf("== A. quiescenceStrictCapture（强制提子不允许 stand-pat）==\n");
    int oldDiffCases = 0;
    int comparedCases = 0;
    for (uint32_t rule = 0; rule < 4; ++rule) {
        for (auto target : { NineChess::PLAYER1, NineChess::PLAYER2 }) {
            std::vector<std::string> commands;
            if (!findCaptureRoot(rule, target, commands)) {
                std::printf("  rule %u target %s: 未找到强制提子根（跳过）\n",
                    rule, target == NineChess::PLAYER1 ? "P1" : "P2");
                continue;
            }
            NineChess game;
            game.setRule(rule);
            game.start();
            for (const std::string& cmd : commands) {
                if (!game.command(cmd.c_str())) {
                    check(false, "构造强制提子根局面时命令被拒: " + cmd);
                    return;
                }
            }
            if (game.getAction() != NineChess::ACTION_CAPTURE) {
                check(false, "构造的根局面不是强制提子状态");
                continue;
            }

            // 全窗口（精确值）与"恰好卡在 stand-pat 上的窄窗口"两种口径：
            // 旧行为在窄窗口下必然直接返回静态分（stand-pat 截断），
            // 新行为两种口径都必须与独立重实现逐位一致。
            Probe::Engine oldEngine;
            makeEngine(1, false, false, oldEngine);
            oldEngine.setChess(game);
            Probe::Context oldCtx;
            Probe::makeContext(oldEngine, oldCtx);
            const int standPat = Probe::evaluate(oldEngine, oldCtx, 0);
            const bool maximizing = game.getTurn() == NineChess::PLAYER1;
            const int narrowAlpha = maximizing ? standPat - 1 : standPat;
            const int narrowBeta = maximizing ? standPat : standPat + 1;

            int refFull = 0;
            int refNarrow = 0;
            int newFull = 0;
            int newNarrow = 0;
            int oldFull = 0;
            int oldNarrow = 0;
            {
                Probe::Engine refEngine;
                makeEngine(1, true, false, refEngine);
                refEngine.setChess(game);
                Probe::Context ctx;
                Probe::makeContext(refEngine, ctx);
                refFull = refCaptureQsearch(refEngine, ctx, -kInf, kInf, 0, 0);
                Probe::Context ctxN;
                Probe::makeContext(refEngine, ctxN);
                refNarrow = refCaptureQsearch(refEngine, ctxN, narrowAlpha, narrowBeta, 0, 0);

                Probe::Engine newEngine;
                makeEngine(1, true, false, newEngine);
                newEngine.setChess(game);
                Probe::Context ctxF;
                Probe::makeContext(newEngine, ctxF);
                newFull = Probe::quiescence(newEngine, ctxF, -kInf, kInf, 0, 0);
                Probe::Context ctxN2;
                Probe::makeContext(newEngine, ctxN2);
                newNarrow = Probe::quiescence(newEngine, ctxN2, narrowAlpha, narrowBeta, 0, 0);

                Probe::Context ctxO;
                Probe::makeContext(oldEngine, ctxO);
                oldFull = Probe::quiescence(oldEngine, ctxO, -kInf, kInf, 0, 0);
                Probe::Context ctxO2;
                Probe::makeContext(oldEngine, ctxO2);
                oldNarrow = Probe::quiescence(oldEngine, ctxO2, narrowAlpha, narrowBeta, 0, 0);
            }

            const std::string tag = "rule " + std::to_string(rule)
                + (target == NineChess::PLAYER1 ? " P1" : " P2");
            check(newFull == refFull, tag + " 新语义全窗口值 = 独立重实现");
            check(newNarrow == refNarrow, tag + " 新语义窄窗口值 = 独立重实现");
            ++comparedCases;
            if (oldFull != refFull || oldNarrow != refNarrow) {
                ++oldDiffCases;
                std::printf("  %s: 命令数=%zu 旧语义暴露差异 old(full/narrow)=%d/%d"
                    " new=%d/%d ref=%d/%d standPat=%d\n",
                    tag.c_str(), commands.size(), oldFull, oldNarrow,
                    newFull, newNarrow, refFull, refNarrow, standPat);
            }
            else {
                std::printf("  %s: 提子数=%zu 新旧一致（该局面 stand-pat 恰为真值）\n",
                    tag.c_str(), commands.size());
            }
        }
    }
    check(comparedCases >= 4, "至少 4 个规则×先后手组合完成比对");
    check(oldDiffCases >= 1, "旧行为在采样中至少暴露 1 处 stand-pat 差异");
    std::printf("  汇总: 比对 %d 例，旧语义差异 %d 例\n", comparedCases, oldDiffCases);
}

// ============ B. 浅层 TT 条目：只给排序走法，不给分值/窗口 ============

void testTtMoveFromShallow()
{
    std::printf("== B. ttMoveFromShallow（浅层 TT 只复用排序走法）==\n");
    NineChess game;
    game.setRule(2);
    game.start();
    Probe::Engine engine;
    makeEngine(7, false, false, engine);
    engine.setChess(game);
    // 先跑一次真实搜索填充置换表（桶内已有邻居条目，验证写入/探测路径非理想化）。
    engine.alphaBetaPruning(4);

    Probe::Context ctx;
    Probe::makeContext(engine, ctx);
    const uint64_t hash = Probe::makeSearchHash(engine, ctx);
    Probe::MoveList moves;
    Probe::generateMoves(engine, ctx, moves);
    check(moves.count >= 2, "根局面至少有 2 个走法");
    const Probe::Move storedMove = moves.moves[0];
    Probe::storeTransposition(engine, ctx, hash, 1, 1234, -kInf, kInf, storedMove, 0);

    // 旧行为：浅条目（depth 1 < 请求 2）完全丢弃。
    {
        Probe::Engine::SearchOptions options = engine.getOptions();
        options.ttMoveFromShallow = false;
        engine.setOptions(options);
        int alpha = -5;
        int beta = 5;
        int value = -777;
        Probe::Move ttMove;
        const bool hit = Probe::probeTransposition(engine, ctx, hash, 2,
            alpha, beta, value, ttMove, 0);
        check(!hit, "旧行为：浅条目不构成命中");
        check(Probe::moveIsNone(ttMove), "旧行为：不提供排序走法");
        check(alpha == -5 && beta == 5, "旧行为：窗口不变");
        check(value == -777, "旧行为：不触碰出参 value");
    }
    // 新行为：仍不构成分值命中，但解出排序走法，窗口/出参不动。
    {
        Probe::Engine::SearchOptions options = engine.getOptions();
        options.ttMoveFromShallow = true;
        engine.setOptions(options);
        int alpha = -5;
        int beta = 5;
        int value = -777;
        Probe::Move ttMove;
        const bool hit = Probe::probeTransposition(engine, ctx, hash, 2,
            alpha, beta, value, ttMove, 0);
        check(!hit, "新行为：浅条目仍不构成分值命中");
        check(Probe::moveEquals(ttMove, storedMove), "新行为：解出的排序走法与入库一致");
        check(alpha == -5 && beta == 5, "新行为：窗口不变");
        check(value == -777, "新行为：不触碰出参 value");

        // 深度足够的同一槽位行为不变：EXACT 直接命中。
        alpha = -5;
        beta = 5;
        value = -777;
        Probe::Move deepMove;
        const bool deepHit = Probe::probeTransposition(engine, ctx, hash, 1,
            alpha, beta, value, deepMove, 0);
        check(deepHit, "深度足够：EXACT 条目直接命中");
        check(value == 1234, "深度足够：命中值与入库一致");
        check(Probe::moveEquals(deepMove, storedMove), "深度足够：走法与入库一致");
        check(alpha == -5 && beta == 5, "深度足够：EXACT 命中不改窗口");
    }
    // 键不存在时不给走法。
    {
        Probe::Engine::SearchOptions options = engine.getOptions();
        options.ttMoveFromShallow = true;
        engine.setOptions(options);
        int alpha = -5;
        int beta = 5;
        int value = -777;
        Probe::Move ttMove;
        const bool hit = Probe::probeTransposition(engine, ctx, hash ^ 0x1234abcdULL, 2,
            alpha, beta, value, ttMove, 0);
        check(!hit && Probe::moveIsNone(ttMove), "键不存在：无走法");
    }
}

// ============ C. 根随机：可复现、等价局面多样、全程层仅精确同分 ============

std::string searchOnce(uint32_t rule, const std::vector<std::string>& warmCommands,
    uint64_t seed, int depth)
{
    NineChess game;
    game.setRule(rule);
    game.start();
    for (const std::string& cmd : warmCommands) {
        if (!game.command(cmd.c_str())) {
            return "WARM-REJECTED:" + cmd;
        }
    }
    Probe::Engine engine;
    Probe::Engine::SearchOptions options;
    options.timeLimitMs = 0;
    options.threads = 1;
    options.randomness = 1;
    options.dynamicDepth = false;
    options.seed = seed;
    engine.setOptions(options);
    engine.setChess(game);
    engine.alphaBetaPruning(depth);
    return std::string(engine.bestMove()) + "|" + engine.rootScoresText();
}

void testRootRandomness()
{
    std::printf("== C. 根随机/等价局面（开局层 softmax + 全程层精确同分）==\n");

    // C1 同种子可复现（全新实例 + 单线程 + 不限时 + 全新 TT）。
    {
        const std::string a = searchOnce(2, {}, 42, 6);
        const std::string b = searchOnce(2, {}, 42, 6);
        check(a == b && a.find("WARM-REJECTED") == std::string::npos,
            "同种子两次搜索（走法+根分表）完全一致");
    }

    // C2 空盘 24 落点同属一个 16 视角等价组：跨种子必须多样且全部合法，
    //    且每次选择都在根分表（精确候选）内。
    {
        std::set<std::string> picked;
        bool allLegal = true;
        bool allInScores = true;
        for (uint64_t seed = 1; seed <= 40; ++seed) {
            NineChess game;
            game.setRule(2);
            game.start();
            Probe::Engine engine;
            Probe::Engine::SearchOptions options;
            options.timeLimitMs = 0;
            options.threads = 1;
            options.randomness = 1;
            options.dynamicDepth = false;
            options.seed = seed;
            engine.setOptions(options);
            engine.setChess(game);
            engine.alphaBetaPruning(6);
            const std::string cmd = engine.bestMove();
            NineChess probeGame = game;
            if (cmd == std::string("error!") || !probeGame.command(cmd.c_str())) {
                allLegal = false;
            }
            const std::vector<std::string> scored = rootScoreCommands(engine.rootScoresText());
            if (std::find(scored.begin(), scored.end(), cmd) == scored.end()) {
                allInScores = false;
            }
            picked.insert(cmd);
        }
        check(allLegal, "空盘 40 种子选点全部合法");
        check(allInScores, "空盘 40 种子选点全部在精确根分表内");
        check(picked.size() >= 8, "空盘 40 种子覆盖 >= 8 个不同落点（实际 "
            + std::to_string(picked.size()) + "）");
        std::printf("  空盘 40 种子 -> %zu 个不同落点\n", picked.size());
    }

    // C3 全程层（randomGapMid=0）：候选只含精确同分走法、选择必在表内、
    //    跨种子根分表逐位一致；先后手根都覆盖。
    for (int warmPlies : { 11, 12 }) {
        std::mt19937_64 warmRng(static_cast<uint64_t>(warmPlies) * 7919ull + 3ull);
        NineChess game;
        game.setRule(2);
        game.start();
        for (int i = 0; i < warmPlies; ++i) {
            if (!randomLegalMove(game, warmRng)) {
                break;
            }
        }
        if (game.whoWin() != NineChess::NOBODY) {
            continue;
        }
        Probe::Engine::SearchOptions base;
        base.timeLimitMs = 0;
        base.threads = 1;
        base.randomness = 1;
        base.dynamicDepth = false;
        base.randomPlies = warmPlies;   // 根局面已走 warmPlies 手 -> 全程层
        base.randomGapMid = 0;          // 仅精确同分
        base.randomTieLayer = true;
        base.randomTieAcrossGroups = true;

        std::string scores3;
        std::string scores4;
        bool membership = true;
        bool uniformScores = true;
        bool legal = true;
        for (uint64_t seed : { 3ull, 4ull }) {
            Probe::Engine engine;
            Probe::Engine::SearchOptions options = base;
            options.seed = seed;
            engine.setOptions(options);
            engine.setChess(game);
            engine.alphaBetaPruning(6);
            const std::string text = engine.rootScoresText();
            if (seed == 3ull) {
                scores3 = text;
            }
            else {
                scores4 = text;
            }
            const std::string cmd = engine.bestMove();
            const std::vector<std::string> scored = rootScoreCommands(text);
            if (std::find(scored.begin(), scored.end(), cmd) == scored.end()) {
                membership = false;
            }
            const std::vector<int> values = rootScoreValues(text);
            for (int v : values) {
                if (values.empty() || v != values.front()) {
                    uniformScores = false;
                }
            }
            NineChess probeGame = game;
            if (cmd == std::string("error!") || !probeGame.command(cmd.c_str())) {
                legal = false;
            }
        }
        const std::string tag = "全程层 warm=" + std::to_string(warmPlies)
            + (game.getTurn() == NineChess::PLAYER1 ? " 先手根" : " 后手根");
        check(scores3 == scores4, tag + "：跨种子根分表逐位一致");
        check(membership, tag + "：选择在精确同分候选表内");
        check(uniformScores, tag + "：候选表全部为精确同分（gap=0）");
        check(legal, tag + "：选择合法");
        std::printf("  %s: 候选 %zu 条\n", tag.c_str(), rootScoreCommands(scores3).size());
    }
}

// ============ D. 新开关全搜索烟雾：合法性 + 增量哈希校验 ============

void testFullSearchSmoke(int gamesPerRule)
{
    std::printf("== D. 新开关（qstrict+ttshallow）全搜索烟雾 ==\n");
    for (uint32_t rule = 0; rule < 4; ++rule) {
        for (int g = 0; g < gamesPerRule; ++g) {
            NineChess game;
            game.setRule(rule);
            game.start();
            Probe::Engine engine;
            Probe::Engine::SearchOptions options;
            options.timeLimitMs = 0;
            options.threads = 1;
            options.randomness = 1;
            options.dynamicDepth = false;
            options.quiescenceStrictCapture = true;
            options.ttMoveFromShallow = true;
            options.seed = static_cast<uint64_t>(rule * 1000 + g * 2 + 1);
            engine.setOptions(options);
            engine.setHashVerification(true);

            bool failed = false;
            int plies = 0;
            while (game.whoWin() == NineChess::NOBODY && plies < 60) {
                engine.setChess(game);
                engine.alphaBetaPruning(5);
                if (engine.lastHashVerifyFailed()) {
                    check(false, "规则 " + std::to_string(rule) + " 第 " + std::to_string(g)
                        + " 局第 " + std::to_string(plies) + " 步增量哈希校验失败");
                    failed = true;
                    break;
                }
                const char* cmd = engine.bestMove();
                if (std::strcmp(cmd, "error!") == 0) {
                    break;
                }
                if (!game.command(cmd)) {
                    check(false, "规则 " + std::to_string(rule) + " 第 " + std::to_string(g)
                        + " 局第 " + std::to_string(plies) + " 步指令被拒: " + cmd);
                    failed = true;
                    break;
                }
                ++plies;
            }
            if (!failed) {
                std::printf("  规则 %u 第 %d 局: %d 步 PASS\n", rule, g, plies);
            }
        }
    }
}

} // namespace

int main(int argc, char** argv)
{
    const int fast = argc > 1 ? std::atoi(argv[1]) : 0;
    testQuiescenceStrictCapture();
    testTtMoveFromShallow();
    testRootRandomness();
    testFullSearchSmoke(fast ? 1 : 2);
    if (g_failures == 0) {
        std::printf("==== SearchRegression PASS ====\n");
        return 0;
    }
    std::printf("==== SearchRegression FAIL: %d 项 ====\n", g_failures);
    return 1;
}

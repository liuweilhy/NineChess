// NineChessConsole.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <cctype>
#include <chrono>
#include <cmath>
#include <cstring>
#include <direct.h>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ninechess.h"
#include "ninechess_ai_ab.h"
#include "ninechess_book.h"


namespace {

void initConsoleUtf8()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

std::string trimCopy(const std::string& text)
{
    std::string::size_type begin = 0;
    while (begin < text.size() && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }

    std::string::size_type end = text.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }

    return text.substr(begin, end - begin);
}

bool startsWith(const std::string& text, const char* prefix)
{
    const std::string prefixText(prefix);
    return text.size() >= prefixText.size()
        && text.compare(0, prefixText.size(), prefixText) == 0;
}

bool tryParseRuleIndex(const std::string& text, uint32_t& ruleIndex)
{
    try {
        std::string::size_type consumed = 0;
        const int index = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        if (index < 0 || index >= RULE_COUNT) {
            return false;
        }

        ruleIndex = static_cast<uint32_t>(index);
        return true;
    }
    catch (...) {
        return false;
    }
}

bool tryParseStartupRuleIndex(int argc, char* argv[], uint32_t& ruleIndex)
{
    if (argc <= 1) {
        return false;
    }

    if (argc == 2) {
        return tryParseRuleIndex(trimCopy(argv[1]), ruleIndex);
    }

    const std::string option = trimCopy(argv[1]);
    if ((option == "-r" || option == "--rule") && argc >= 3) {
        return tryParseRuleIndex(trimCopy(argv[2]), ruleIndex);
    }

    return false;
}

void printRules()
{
    std::cout << "可选规则:\n";
    for (uint32_t i = 0; i < RULE_COUNT; ++i) {
        std::cout << "  " << i << " : " << NineChess::rules[i].name << "\n";
    }
}

void printRuleDetail(uint32_t ruleIndex)
{
    if (ruleIndex >= RULE_COUNT) {
        return;
    }

    const NineChess::Rule& rule = NineChess::rules[ruleIndex];
    std::cout << "规则 " << ruleIndex << " : " << rule.name << "\n";

    if (rule.description == nullptr || rule.description[0] == '\0') {
        std::cout << "  无详细说明。\n";
        return;
    }

    std::cout << "说明:\n";
    const std::string description(rule.description);
    std::string::size_type begin = 0;
    while (begin < description.size()) {
        std::string::size_type end = description.find('\n', begin);
        if (end == std::string::npos) {
            end = description.size();
        }

        const std::string line = description.substr(begin, end - begin);
        if (!line.empty()) {
            std::cout << "  " << line << "\n";
        }

        begin = end + 1;
    }
}

void printRulesDetailed()
{
    std::cout << "可选规则:\n";
    for (uint32_t i = 0; i < RULE_COUNT; ++i) {
        if (i > 0) {
            std::cout << "\n";
        }
        printRuleDetail(i);
    }
}

void printHelp()
{
    std::cout
        << "命令说明:\n"
        << "  (c,p)              开局落子，或中局选子\n"
        << "  (c1,p1)->(c2,p2)   中局走子\n"
        << "  -(c,p)             提子\n"
        << "  -0                 先手认输\n"
        << "  -1                 后手认输\n"
        << "  board              重新打印当前棋盘\n"
        << "  history            打印命令历史\n"
        << "  undo               回退到上一个局面\n"
        << "  new                按当前规则重新开局\n"
        << "  rules              列出所有规则及说明\n"
        << "  rule N             切换到第 N 条规则、显示说明并重新开局\n"
        << "  r<s<t<          对局配置命令（棋谱首行），如 r2s100t10；段可省略，0=不限\n"
        << "  search [d] [t] [h] [r] [g] [th] [s] [pv] [a] [rep] [q] [dd] [pvs]   AI 搜索当前局面并打印统计\n"
        << "  match [n] [d] [h] [r] [th] [s] [mp] [dd]  AI 自对弈 n 局并打印汇总\n"
        << "  vs [n] [d1] [h1] [pv1] [d2] [h2] [pv2] [s] [a] [rep] [wp] [sp] [ft] [th]  不同配置引擎对抗（强度 A/B，奇偶局交替执先）\n"
        << "  booktrain [n] [d] [s]  自对弈训练开局库（无书对局，统计胜负分）\n"
        << "  selfcheck [n] [d]      4 规则随机自对弈并逐节点校验增量哈希\n"
        << "  tune [n] [d] [s] [r] [th]  按规则坐标下降自动调参（引擎对抗选值，结束后打印建议行）\n"
        << "  bookstat              查看开局库状态与样本最多的条目\n"
        << "  bookon / bookoff      启用 / 禁用开局库（match/vs 生效）\n"
        << "  booksave [path] / bookload [path]  保存 / 加载开局库（默认 books\\book_<规则>.dat）\n"
        << "  help               显示帮助\n"
        << "  quit               退出程序\n"
        << "\n"
        << "search/match/vs 参数说明（缺省用括号内默认值）:\n"
        << "  search:  d=深度(8) t=时限ms(0不限) h=哈希(1) r=随机(0) g=分差(60) th=线程(0自动) s=种子(0) pv=点位价值(16) a=窗口(1) rep=重复惩罚(40) q=静默搜索(0) dd=动态深度(1) pvs=PVS试探(0)\n"
        << "  match:   n=局数(20) d=深度(8) h=哈希(1) r=随机(1) th=线程(0自动) s=种子(0) mp=步数上限(300) dd=动态深度(1)\n"
        << "  vs:      n=局数(20) 引擎1: d1=深度(6) h1=哈希(1) pv1=点位价值(16) | 引擎2: d2=深度(8) h2=哈希(1) pv2=点位价值(16) wp=胜利压力(0) sp=闷杀压力(-1) ft=双三威胁(-1,均仅引擎2) s=种子(0) a=窗口(1) rep=重复惩罚(40) th=线程(0自动)\n"
        << "  h: 0=普通哈希 1=仅开局规范化(默认) 2=全部规范化; r: 0=纯最优, >0 启用根节点随机\n"
        << "  pv: 0=关闭点位价值; a: 0=关闭 Aspiration 窗口; rep: 0=关闭重复惩罚; q: 1=启用静默搜索\n"
        << "  th: 0=自动(CPU核数推导); dd: 中局根局面追加2层深度; pvs: 非首走法零宽窗口试探\n"
        << "\n"
        << "说明:\n"
        << "  1. 坐标全部采用 0-based，下标范围为 c=0~2, p=0~7。\n"
        << "     其中 p 按顺时针排列：0 上中，1 右上，2 右中，3 右下，\n"
        << "     4 下中，5 左下，6 左中，7 左上。\n"
        << "  2. 每次成功执行命令后，程序都会重新打印棋盘。\n"
        << "  3. 当前默认规则为九连棋。\n"
        << "  4. 启动时可传规则编号，例如: NineChessConsole.exe 2\n"
        << "     或 NineChessConsole.exe --rule 2\n";
}

void printBoard(const NineChess& chess)
{
    std::cout << "\n" << chess.getConsoleText(true);
    if (chess.getPhase() == NineChess::GAME_OVER) {
        std::cout << "对局已结束。输入 new 重新开局，或输入 rule N 切换规则。\n";
    }
    std::cout << std::flush;
}

void printHistory(const NineChess& chess)
{
    const std::vector<std::string>& history = chess.getCmdHistory();
    std::cout << "命令历史 (" << history.size() << "):\n";
    if (history.empty()) {
        std::cout << "  无\n";
        return;
    }

    for (size_t i = 0; i < history.size(); ++i) {
        std::cout << "  " << (i + 1u) << ". " << history[i] << "\n";
    }
}

// ==================== 开局库全局状态 ====================

NineChessOpeningBook g_book;
bool g_bookEnabled = false;

// 对局配置（棋谱首行 r<s<t< 的限步/限时段）：控制台识别并回显，但不执行计时
int g_setupSteps = 0;
int g_setupTime = 0;

std::string bookPathFor(uint32_t ruleIndex)
{
    return "books/book_" + std::to_string(ruleIndex) + ".dat";
}

void ensureBookDir()
{
    _mkdir("books");
}

bool loadBookFor(uint32_t ruleIndex, const NineChess& ruleChess)
{
    g_book.setRule(ruleIndex, ruleChess);
    g_bookEnabled = g_book.load(bookPathFor(ruleIndex), ruleChess);
    return g_bookEnabled;
}

std::string formatBookCommand(int type, int from, int to)
{
    const auto point = [](int pos) {
        return "(" + std::to_string(pos / SEAT) + "," + std::to_string(pos % SEAT) + ")";
    };
    if (type == 1) {
        return point(to);
    }
    if (type == 2) {
        return point(from) + "->" + point(to);
    }
    if (type == 3) {
        return "-" + point(to);
    }
    return std::string();
}

// 从开局库为当前局面选一步棋；返回 false 表示不在书中或样本不足。
// randomize 为 true 时按胜率加权随机，否则取胜率最高者。
bool tryBookMove(const NineChess& game, std::string& cmd, std::mt19937_64& rng, bool randomize)
{
    if (!g_bookEnabled) {
        return false;
    }
    const NineChessOpeningBook::ProbeResult result = g_book.probe(game);
    if (result.entry == nullptr || result.entry->moves.empty()) {
        return false;
    }

    struct Candidate {
        uint16_t code = 0;
        double winRate = 0.0;
    };
    std::vector<Candidate> candidates;
    double bestWinRate = -2.0;
    for (const NineChessOpeningBook::MoveStats& moveStats : result.entry->moves) {
        if (moveStats.plays < NineChessOpeningBook::MIN_TRUSTED_PLAYS) {
            continue;
        }
        const double winRate = static_cast<double>(moveStats.score)
            / static_cast<double>(moveStats.plays);
        if (winRate > bestWinRate) {
            bestWinRate = winRate;
        }
        Candidate candidate;
        candidate.code = moveStats.move;
        candidate.winRate = winRate;
        candidates.push_back(candidate);
    }
    if (candidates.empty()) {
        return false;
    }

    // 只在与最优胜率差距 15% 以内的走法里选。
    const double gap = 0.15;
    std::vector<Candidate> pool;
    for (const Candidate& candidate : candidates) {
        if (candidate.winRate >= bestWinRate - gap) {
            pool.push_back(candidate);
        }
    }

    uint16_t picked = pool.front().code;
    if (randomize && pool.size() > 1) {
        std::vector<double> weights;
        double total = 0.0;
        for (const Candidate& candidate : pool) {
            const double weight = (candidate.winRate - (bestWinRate - gap)) + 0.02;
            weights.push_back(weight);
            total += weight;
        }
        std::uniform_real_distribution<double> dist(0.0, total);
        const double draw = dist(rng);
        double acc = 0.0;
        for (size_t i = 0; i < pool.size(); ++i) {
            acc += weights[i];
            if (draw <= acc) {
                picked = pool[i].code;
                break;
            }
        }
    }

    const uint16_t actual = g_book.toActualMove(picked, result.viewIndex);
    int type = 0;
    int from = -1;
    int to = -1;
    NineChessOpeningBook::decodeMove(actual, type, from, to);
    cmd = formatBookCommand(type, from, to);
    return !cmd.empty();
}

int64_t steadyMs()
{
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

std::vector<std::string> splitArgs(const std::string& text)
{
    std::vector<std::string> args;
    std::string current;
    for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                args.push_back(current);
                current.clear();
            }
        }
        else {
            current += ch;
        }
    }
    if (!current.empty()) {
        args.push_back(current);
    }
    return args;
}

bool tryParseInt64(const std::string& text, int64_t& value)
{
    try {
        size_t consumed = 0;
        const long long parsed = std::stoll(text, &consumed);
        if (consumed != text.size()) {
            return false;
        }
        value = parsed;
        return true;
    }
    catch (...) {
        return false;
    }
}

NineChess_AI_AB::HashMode hashModeFromInt(int64_t value)
{
    if (value == 0) {
        return NineChess_AI_AB::HashMode::Plain;
    }
    if (value == 2) {
        return NineChess_AI_AB::HashMode::FullCanonical;
    }
    return NineChess_AI_AB::HashMode::OpeningCanonical;
}

void printSearchStats(const NineChess_AI_AB& ai)
{
    NineChess_AI_AB::SearchStats stats;
    ai.snapshotStats(stats);
    const uint64_t nodes = stats.nodes.load();
    const uint64_t probes = stats.ttProbes.load();
    const int64_t elapsed = ai.getLastSearchTimeMs();

    std::cout << "估值: " << ai.getLastCompletedValue()
        << "  完成深度: " << ai.getLastCompletedDepth()
        << "  耗时: " << elapsed << " ms\n";
    std::cout << "节点: " << nodes
        << "  NPS: " << (elapsed > 0 ? nodes * 1000 / static_cast<uint64_t>(elapsed) : 0u)
        << "  剪枝: " << stats.betaCutoffs.load() << "\n";
    std::cout << "置换表: probe=" << probes
        << " hit=" << stats.ttHits.load()
        << " store=" << stats.ttStores.load()
        << "  命中率: " << (probes > 0u ? stats.ttHits.load() * 100u / probes : 0u) << "%"
        << "  重复命中: " << stats.repetitionHits.load() << "\n";
}

// search [depth] [timeMs] [hash] [random] [gap] [threads] [seed] [pv] [asp] [rep] [q] [dd] [pvs] [ext] [lmr]
void runSearchCommand(const std::string& argText, const NineChess& chess)
{
    int64_t depth = 8;
    int64_t timeMs = 0;
    int64_t hashMode = 1;
    int64_t randomLevel = 0;
    int64_t gap = 60;
    int64_t threads = 0;
    int64_t seed = 0;
    int64_t pv = 16;
    int64_t asp = 1;
    int64_t rep = 40;
    int64_t q = 0;
    int64_t dd = 1;
    int64_t pvs = 0;
    int64_t ext = 1;
    int64_t lmr = 1;

    const std::vector<std::string> args = splitArgs(argText);
    int64_t* slots[] = { &depth, &timeMs, &hashMode, &randomLevel, &gap,
        &threads, &seed, &pv, &asp, &rep, &q, &dd, &pvs, &ext, &lmr };
    for (size_t i = 0; i < args.size() && i < 15; ++i) {
        tryParseInt64(args[i], *slots[i]);
    }

    NineChess_AI_AB::SearchOptions options;
    options.timeLimitMs = timeMs;
    options.randomness = static_cast<uint32_t>(randomLevel);
    options.randomGap = static_cast<int32_t>(gap);
    // threads <= 0 表示自动：按 CPU 逻辑核心数取默认值。
    options.threads = threads > 0
        ? static_cast<uint32_t>(threads)
        : NineChess_AI_AB::defaultThreadCount();
    options.hashMode = hashModeFromInt(hashMode);
    options.seed = static_cast<uint64_t>(seed);
    options.pointValueWeight = static_cast<int32_t>(pv);
    options.aspirationWindows = asp != 0;
    options.repetitionPenalty = static_cast<int32_t>(rep);
    options.quiescenceSearch = q != 0;
    options.dynamicDepth = dd != 0;
    options.pvSearch = pvs != 0;
    options.forcedExtension = ext != 0;
    options.lateMoveReductions = lmr != 0;

    std::cout << "AI 搜索: 规则=" << chess.getRule()->name
        << " 深度=" << depth << " 时限=" << timeMs << "ms"
        << " 哈希=" << hashMode << " 随机=" << randomLevel
        << " 线程=" << options.threads << " 种子=" << seed << " 点位价值=" << pv
        << " 窗口=" << asp << " 重复惩罚=" << rep << " 静默=" << q
        << " 动态深度=" << (dd != 0) << " PVS=" << (pvs != 0)
        << " 延伸=" << (ext != 0) << " LMR=" << (lmr != 0) << "\n";
    if (options.threads > 1) {
        std::cout << "（Lazy SMP 多线程）\n";
    }

    NineChess_AI_AB ai;
    ai.setOptions(options);
    ai.setChess(chess);
    ai.alphaBetaPruning(static_cast<int>(depth));

    std::cout << "最佳走法: " << ai.bestMove() << "\n";
    printSearchStats(ai);
    std::cout << "根走法分数:\n" << ai.rootScoresText();
}

// match [games] [depth] [hash] [random] [threads] [seed] [maxPlies] [dd]
void runMatchCommand(const std::string& argText, NineChess& chess)
{
    int64_t games = 20;
    int64_t depth = 8;
    int64_t hashMode = 1;
    int64_t randomLevel = 1;
    int64_t threads = 0;
    int64_t seed = 0;
    int64_t maxPlies = 300;
    int64_t dd = 1;

    const std::vector<std::string> args = splitArgs(argText);
    int64_t* slots[] = { &games, &depth, &hashMode, &randomLevel, &threads, &seed, &maxPlies, &dd };
    for (size_t i = 0; i < args.size() && i < 8; ++i) {
        tryParseInt64(args[i], *slots[i]);
    }
    if (games <= 0) {
        games = 20;
    }
    if (depth <= 0) {
        depth = 8;
    }
    if (maxPlies <= 0) {
        maxPlies = 300;
    }

    std::cout << "match 开始: 规则=" << chess.getRule()->name
        << " 局数=" << games << " 深度=" << depth
        << " 哈希=" << hashMode << " 随机=" << randomLevel
        << " 线程=" << (threads > 0 ? threads : static_cast<int64_t>(NineChess_AI_AB::defaultThreadCount()))
        << " 种子=" << seed << " 动态深度=" << (dd != 0) << "\n";
    if (threads > 1 || (threads <= 0 && NineChess_AI_AB::defaultThreadCount() > 1)) {
        std::cout << "（Lazy SMP 多线程）\n";
    }

    const uint32_t ruleIndex = chess.getRuleIndex();
    NineChess_AI_AB ai1;
    NineChess_AI_AB ai2;

    int wins1 = 0;
    int wins2 = 0;
    int draws = 0;
    uint64_t totalNodes = 0;
    int64_t totalTimeMs = 0;
    int64_t totalPlies = 0;
    int64_t bookUses = 0;
    int64_t bookRejects = 0;
    std::mt19937_64 matchRng(static_cast<uint64_t>(seed));

    for (int64_t g = 0; g < games; ++g) {
        NineChess game;
        game.setRule(ruleIndex);
        game.start();

        NineChess_AI_AB::SearchOptions options;
        options.timeLimitMs = 0;
        options.randomness = static_cast<uint32_t>(randomLevel);
        options.randomGap = 16;
        // threads <= 0 表示自动：按 CPU 逻辑核心数取默认值。
        options.threads = threads > 0
            ? static_cast<uint32_t>(threads)
            : NineChess_AI_AB::defaultThreadCount();
        options.hashMode = hashModeFromInt(hashMode);
        options.seed = static_cast<uint64_t>(seed + g * 2 + 1);
        options.dynamicDepth = dd != 0;
        ai1.setOptions(options);
        options.seed = static_cast<uint64_t>(seed + g * 2 + 2);
        ai2.setOptions(options);

        // 绑定规则后再清表，确保清的是本局规则对应的置换表。
        ai1.setChess(game);
        ai2.setChess(game);
        ai1.clearTranspositionTable();

        int64_t plies = 0;
        while (game.whoWin() == NineChess::NOBODY && plies < maxPlies) {
            // 开局库优先：书内走法直接执行，被模型拒绝时回退到 AI 搜索。
            std::string bookCmd;
            if (tryBookMove(game, bookCmd, matchRng, randomLevel > 0)) {
                if (game.command(bookCmd.c_str())) {
                    ++bookUses;
                    ++plies;
                    continue;
                }
                ++bookRejects;
            }
            NineChess_AI_AB& ai = game.getTurn() == NineChess::PLAYER1 ? ai1 : ai2;
            // 限步赛制：把当前剩余步数传入评估急迫项，促使优势方在限步前转化。
            // 模型层不感知限步，提示值只存在于 AI 选项中。
            {
                NineChess_AI_AB::SearchOptions currentOptions = ai.getOptions();
                currentOptions.stepsRemainingHint =
                    static_cast<int32_t>(std::max<int64_t>(1, maxPlies - plies));
                ai.setOptions(currentOptions);
            }
            ai.setChess(game);
            ai.alphaBetaPruning(static_cast<int>(depth));
            NineChess_AI_AB::SearchStats stats;
            ai.snapshotStats(stats);
            totalNodes += stats.nodes.load();
            totalTimeMs += ai.getLastSearchTimeMs();

            const char* cmd = ai.bestMove();
            if (std::strcmp(cmd, "error!") == 0) {
                break;
            }
            if (!game.command(cmd)) {
                std::cout << "  警告: AI 指令被模型拒绝: " << cmd << "\n";
                break;
            }
            ++plies;
        }
        totalPlies += plies;

        const NineChess::Players winner = game.whoWin();
        const char* resultText = winner == NineChess::PLAYER1
            ? "先手胜"
            : (winner == NineChess::PLAYER2 ? "后手胜" : "平局");
        if (winner == NineChess::PLAYER1) {
            ++wins1;
        }
        else if (winner == NineChess::PLAYER2) {
            ++wins2;
        }
        else {
            ++draws;
        }

        std::cout << "第 " << (g + 1) << " 局: " << resultText
            << "  步数: " << plies << "\n";
    }

    std::cout << "==== match 汇总 ====\n";
    std::cout << "先手胜: " << wins1 << "  后手胜: " << wins2
        << "  平局: " << draws << "\n";
    std::cout << "平均步数: " << (games > 0 ? totalPlies / games : 0)
        << "  书内走法: " << bookUses
        << "  书内被拒: " << bookRejects << "\n";
    std::cout << "总节点: " << totalNodes
        << "  总耗时: " << totalTimeMs << " ms"
        << "  平均 NPS: " << (totalTimeMs > 0
            ? totalNodes * 1000 / static_cast<uint64_t>(totalTimeMs) : 0u) << "\n";
}

// vs [games] [d1] [h1] [pv1] [d2] [h2] [pv2] [seed] [asp] [rep] [wp] [sp] [ft] [th]
// 双方采用不同配置的对抗测试，用于强度 A/B。
// asp/rep 对双方同时生效（对比"开 vs 关"的整体引擎行为）；
// wp/sp/ft/ext/lmr/ms/tp/sr 只作用于引擎2（引擎1 恒为默认/表值）；
// ext/lmr/ms/tp 取 -1 表示沿用 SearchOptions 默认值，sr 为引擎2 的剩余步数提示（0 = 关）。
// 奇偶局交替执先：偶数局引擎1 执先，奇数局引擎2 执先，
// 消除当前评估下明显的执先优势对 A/B 结果的干扰。
void runVsCommand(const std::string& argText, NineChess& chess)
{
    int64_t games = 20;
    int64_t depth1 = 6;
    int64_t hash1 = 1;
    int64_t pv1 = 16;
    int64_t depth2 = 8;
    int64_t hash2 = 1;
    int64_t pv2 = 16;
    int64_t seed = 0;
    int64_t asp = 1;
    int64_t rep = 40;
    int64_t wp = 0;
    int64_t sp = -1;
    int64_t ft = -1;
    int64_t th = 0;
    int64_t ext = -1;
    int64_t lmr = -1;
    int64_t ms = -1;
    int64_t tp = -1;
    int64_t sr = 0;

    const std::vector<std::string> args = splitArgs(argText);
    int64_t* slots[] = { &games, &depth1, &hash1, &pv1, &depth2, &hash2, &pv2,
        &seed, &asp, &rep, &wp, &sp, &ft, &th, &ext, &lmr, &ms, &tp, &sr };
    for (size_t i = 0; i < args.size() && i < 19; ++i) {
        tryParseInt64(args[i], *slots[i]);
    }
    if (games <= 0) {
        games = 20;
    }
    if (depth1 <= 0) {
        depth1 = 6;
    }
    if (depth2 <= 0) {
        depth2 = 8;
    }

    std::cout << "vs 开始: 规则=" << chess.getRule()->name
        << " 局数=" << games
        << " 引擎1: 深度=" << depth1 << " 哈希=" << hash1 << " 点位价值=" << pv1
        << " | 引擎2: 深度=" << depth2 << " 哈希=" << hash2 << " 点位价值=" << pv2
        << " 胜利压力=" << wp << " 闷杀压力=" << sp << " 双三威胁=" << ft
        << " 延伸=" << ext << " LMR=" << lmr << " 三连保护=" << ms
        << " 连续阶段=" << tp << " 步数提示=" << sr
        << " 线程=" << (th > 0 ? th : static_cast<int64_t>(NineChess_AI_AB::defaultThreadCount()))
        << " 种子=" << seed << " 窗口=" << asp << " 重复惩罚=" << rep
        << "（奇偶局交替执先）\n";

    const uint32_t ruleIndex = chess.getRuleIndex();
    NineChess_AI_AB ai1;
    NineChess_AI_AB ai2;

    int wins1 = 0;
    int wins2 = 0;
    int draws = 0;
    uint64_t totalNodes = 0;
    int64_t totalTimeMs = 0;
    int64_t bookUses = 0;
    int64_t bookRejects = 0;
    std::mt19937_64 vsRng(static_cast<uint64_t>(seed));

    for (int64_t g = 0; g < games; ++g) {
        NineChess game;
        game.setRule(ruleIndex);
        game.start();

        // 奇偶局交替执先，保证两个引擎拿到的先手局数均衡。
        const bool swapColors = (g % 2 == 1);
        NineChess_AI_AB& aiFirst = swapColors ? ai2 : ai1;
        NineChess_AI_AB& aiSecond = swapColors ? ai1 : ai2;

        NineChess_AI_AB::SearchOptions options;
        options.timeLimitMs = 0;
        options.randomness = 1;   // 双方都启用随机，保证对局有变化
        options.randomGap = 16;
        // 双方同线程数保持公平；同深度对抗下共享置换表的反馈是对称的。
        // 注意：深度不对称的 A/B（如 5v7）建议 th=1，避免跨引擎 TT 反馈不对称。
        options.threads = th > 0
            ? static_cast<uint32_t>(th)
            : NineChess_AI_AB::defaultThreadCount();
        options.aspirationWindows = asp != 0;
        options.repetitionPenalty = static_cast<int32_t>(rep);
        options.hashMode = hashModeFromInt(hash1);
        options.pointValueWeight = static_cast<int32_t>(pv1);
        options.winPressureWeight = 0;
        options.seed = static_cast<uint64_t>(seed + g * 2 + 1);
        ai1.setOptions(options);
        options.hashMode = hashModeFromInt(hash2);
        options.pointValueWeight = static_cast<int32_t>(pv2);
        options.winPressureWeight = static_cast<int32_t>(wp);
        options.stalematePressureWeight = static_cast<int32_t>(sp);
        options.forkThreatWeight = static_cast<int32_t>(ft);
        // ext/lmr/ms/tp < 0 表示沿用 SearchOptions 默认值，>= 0 时显式覆盖（A/B 用）。
        if (ext >= 0) {
            options.forcedExtension = ext != 0;
        }
        if (lmr >= 0) {
            options.lateMoveReductions = lmr != 0;
        }
        if (ms >= 0) {
            options.millSafetyWeight = static_cast<int32_t>(ms);
        }
        if (tp >= 0) {
            options.taperedEval = tp != 0;
        }
        options.stepsRemainingHint = static_cast<int32_t>(sr);
        options.seed = static_cast<uint64_t>(seed + g * 2 + 2);
        ai2.setOptions(options);

        ai1.setChess(game);
        ai2.setChess(game);
        ai1.clearTranspositionTable();

        int64_t plies = 0;
        while (game.whoWin() == NineChess::NOBODY && plies < 300) {
            // 开局库优先（与 match 相同）。
            std::string bookCmd;
            if (tryBookMove(game, bookCmd, vsRng, true)) {
                if (game.command(bookCmd.c_str())) {
                    ++bookUses;
                    ++plies;
                    continue;
                }
                ++bookRejects;
            }
            const bool firstTurn = game.getTurn() == NineChess::PLAYER1;
            NineChess_AI_AB& ai = firstTurn ? aiFirst : aiSecond;
            const int64_t depth = firstTurn
                ? (swapColors ? depth2 : depth1)
                : (swapColors ? depth1 : depth2);
            // 引擎2 的剩余步数提示随对局推进递减（sr = 0 表示关闭）。
            if (sr > 0 && &ai == &ai2) {
                NineChess_AI_AB::SearchOptions currentOptions = ai.getOptions();
                currentOptions.stepsRemainingHint =
                    static_cast<int32_t>(std::max<int64_t>(1, sr - plies));
                ai.setOptions(currentOptions);
            }
            ai.setChess(game);
            ai.alphaBetaPruning(static_cast<int>(depth));
            NineChess_AI_AB::SearchStats stats;
            ai.snapshotStats(stats);
            totalNodes += stats.nodes.load();
            totalTimeMs += ai.getLastSearchTimeMs();

            const char* cmd = ai.bestMove();
            if (std::strcmp(cmd, "error!") == 0) {
                break;
            }
            if (!game.command(cmd)) {
                std::cout << "  警告: AI 指令被模型拒绝: " << cmd << "\n";
                break;
            }
            ++plies;
        }

        const NineChess::Players winner = game.whoWin();
        const bool engine1Won = swapColors ? (winner == NineChess::PLAYER2)
                                           : (winner == NineChess::PLAYER1);
        const bool engine2Won = swapColors ? (winner == NineChess::PLAYER1)
                                           : (winner == NineChess::PLAYER2);
        const char* colorText = swapColors ? "后手" : "先手";
        const char* resultText = engine1Won
            ? "引擎1胜"
            : (engine2Won ? "引擎2胜" : "平局");
        if (engine1Won) {
            ++wins1;
        }
        else if (engine2Won) {
            ++wins2;
        }
        else {
            ++draws;
        }

        std::cout << "第 " << (g + 1) << " 局: " << resultText
            << "（胜者执" << (winner == NineChess::NOBODY ? "-" : colorText) << "）"
            << "  步数: " << plies << "\n";
    }

    std::cout << "==== vs 汇总 ====\n";
    std::cout << "引擎1胜: " << wins1 << "  引擎2胜: " << wins2
        << "  平局: " << draws << "\n";
    std::cout << "书内走法: " << bookUses
        << "  书内被拒: " << bookRejects << "\n";
    std::cout << "总节点: " << totalNodes
        << "  总耗时: " << totalTimeMs << " ms"
        << "  平均 NPS: " << (totalTimeMs > 0
            ? totalNodes * 1000 / static_cast<uint64_t>(totalTimeMs) : 0u) << "\n";
}

// booktrain <games> <depth> [seed]
// 不开书的自对弈训练：每局回放前 bookDepth 层，按“轮到方”视角累计胜负统计。
// 训练结果自动保存到 books/book_<规则号>.dat，可多次运行累积样本。
void runBookTrainCommand(const std::string& argText, NineChess& chess)
{
    int64_t games = 200;
    int64_t depth = 5;
    int64_t seed = 0;

    const std::vector<std::string> args = splitArgs(argText);
    int64_t* slots[] = { &games, &depth, &seed };
    for (size_t i = 0; i < args.size() && i < 3; ++i) {
        tryParseInt64(args[i], *slots[i]);
    }
    if (games <= 0) {
        games = 200;
    }
    if (depth <= 0) {
        depth = 5;
    }

    if (g_book.ruleIndex() != chess.getRuleIndex()) {
        g_book.setRule(chess.getRuleIndex(), chess);
    }

    std::cout << "booktrain 开始: 规则=" << chess.getRule()->name
        << " 局数=" << games << " 深度=" << depth
        << " 书深度=" << g_book.bookDepth() << " 种子=" << seed << "\n";

    const uint32_t ruleIndex = chess.getRuleIndex();
    NineChess_AI_AB ai1;
    NineChess_AI_AB ai2;
    NineChess_AI_AB::SearchOptions options;
    options.timeLimitMs = 0;
    options.randomness = 1;   // 训练必须开随机，才能覆盖不同开局分支
    options.randomGap = 16;
    options.threads = 1;
    options.seed = static_cast<uint64_t>(seed);

    int wins1 = 0;
    int wins2 = 0;
    int draws = 0;
    int64_t totalPlies = 0;

    for (int64_t g = 0; g < games; ++g) {
        NineChess game;
        game.setRule(ruleIndex);
        game.start();

        std::vector<std::string> commands;

        int64_t plies = 0;
        while (game.whoWin() == NineChess::NOBODY && plies < 300) {
            NineChess_AI_AB& ai = game.getTurn() == NineChess::PLAYER1 ? ai1 : ai2;
            options.seed = static_cast<uint64_t>(seed + g * 2 + (game.getTurn() == NineChess::PLAYER1 ? 1 : 2));
            ai.setOptions(options);
            ai.setChess(game);
            ai.alphaBetaPruning(static_cast<int>(depth));

            const char* cmd = ai.bestMove();
            if (std::strcmp(cmd, "error!") == 0) {
                break;
            }
            if (!game.command(cmd)) {
                std::cout << "  警告: AI 指令被模型拒绝: " << cmd << "\n";
                break;
            }
            commands.push_back(cmd);
            ++plies;
        }
        totalPlies += plies;

        const NineChess::Players winner = game.whoWin();
        if (winner == NineChess::PLAYER1) {
            ++wins1;
        }
        else if (winner == NineChess::PLAYER2) {
            ++wins2;
        }
        else {
            ++draws;
        }

        g_book.recordGame(game, commands,
            winner == NineChess::PLAYER1 ? 1 : (winner == NineChess::PLAYER2 ? -1 : 0));
    }

    ensureBookDir();
    const std::string path = bookPathFor(ruleIndex);
    const bool saved = g_book.save(path);

    std::cout << "==== booktrain 汇总 ====\n";
    std::cout << "先手胜: " << wins1 << "  后手胜: " << wins2
        << "  平局: " << draws << "  平均步数: " << (games > 0 ? totalPlies / games : 0) << "\n";
    std::cout << "书条目: " << g_book.entryCount()
        << "  记录数: " << g_book.totalRecords()
        << "  保存: " << (saved ? path : "失败") << "\n";
    g_bookEnabled = true;
}

// selfcheck [games] [depth]：4 条规则随机自对弈，逐节点校验增量哈希累加器。
// 任何一步校验失败或 AI 指令被模型拒绝都会立即报告并停止。
void runSelfCheckCommand(const std::string& argText)
{
    int64_t games = 6;
    int64_t depth = 4;
    const std::vector<std::string> args = splitArgs(argText);
    int64_t* slots[] = { &games, &depth };
    for (size_t i = 0; i < args.size() && i < 2; ++i) {
        tryParseInt64(args[i], *slots[i]);
    }
    if (games <= 0) {
        games = 6;
    }
    if (depth <= 0) {
        depth = 4;
    }

    std::cout << "selfcheck 开始: 每规则 " << games << " 局, 深度 " << depth
        << ", 逐节点校验增量哈希\n";

    int64_t totalPlies = 0;
    bool failed = false;
    for (uint32_t rule = 0; rule < RULE_COUNT && !failed; ++rule) {
        NineChess_AI_AB ai1;
        NineChess_AI_AB ai2;
        NineChess_AI_AB::SearchOptions options;
        options.randomness = 1;
        options.threads = 1;

        int64_t rulePlies = 0;
        for (int64_t g = 0; g < games && !failed; ++g) {
            NineChess game;
            game.setRule(rule);
            game.start();
            options.seed = static_cast<uint64_t>(rule * 100000 + g * 2 + 1);
            ai1.setOptions(options);
            options.seed = static_cast<uint64_t>(rule * 100000 + g * 2 + 2);
            ai2.setOptions(options);

            int64_t plies = 0;
            while (game.whoWin() == NineChess::NOBODY && plies < 200) {
                NineChess_AI_AB& ai = game.getTurn() == NineChess::PLAYER1 ? ai1 : ai2;
                ai.setHashVerification(true);
                ai.setChess(game);
                ai.alphaBetaPruning(static_cast<int>(depth));
                if (ai.lastHashVerifyFailed()) {
                    std::cout << "  [FAIL] 规则 " << rule << " 第 " << (g + 1)
                        << " 局第 " << plies << " 步: 增量哈希校验失败!\n";
                    failed = true;
                    break;
                }
                const char* cmd = ai.bestMove();
                if (std::strcmp(cmd, "error!") == 0) {
                    break;
                }
                if (!game.command(cmd)) {
                    std::cout << "  [FAIL] 规则 " << rule << " AI 指令被模型拒绝: "
                        << cmd << "\n";
                    failed = true;
                    break;
                }
                ++plies;
            }
            rulePlies += plies;
        }
        totalPlies += rulePlies;
        std::cout << "  规则 " << rule << " (" << NineChess::rules[rule].name
            << "): " << (failed ? "失败" : "完成")
            << "  步数: " << rulePlies << "\n";
    }

    if (!failed) {
        std::cout << "==== selfcheck PASS ====  总步数: " << totalPlies << "\n";
    }
}

// ==================== tune：按规则坐标下降自动调参 ====================

struct TuneColumn {
    const char* name;
    int32_t NineChess_AI_AB::EvalWeights::* field;
    std::vector<double> factors;          // 相对当前值的倍率候选
    std::vector<int32_t> zeroCandidates;  // 当前值为 0 时的绝对值候选
};

const std::vector<TuneColumn>& tuneColumns()
{
    static const std::vector<TuneColumn> columns = {
        { "openingMaterial", &NineChess_AI_AB::EvalWeights::openingMaterial, { 0.6, 1.6 }, {} },
        { "openingInHand",   &NineChess_AI_AB::EvalWeights::openingInHand,   { 0.0, 1.6 }, {} },
        { "openingMill",     &NineChess_AI_AB::EvalWeights::openingMill,     { 0.6, 1.6 }, {} },
        { "openingOpenMill", &NineChess_AI_AB::EvalWeights::openingOpenMill, { 0.0, 1.6 }, {} },
        { "midMaterial",     &NineChess_AI_AB::EvalWeights::midMaterial,     { 0.6, 1.6 }, {} },
        { "midMill",         &NineChess_AI_AB::EvalWeights::midMill,         { 0.6, 1.6 }, {} },
        { "midOpenMill",     &NineChess_AI_AB::EvalWeights::midOpenMill,     { 0.0, 1.6 }, {} },
        { "midMobility",     &NineChess_AI_AB::EvalWeights::midMobility,     { 0.0, 2.0, 4.0 }, {} },
        { "captureOpening",  &NineChess_AI_AB::EvalWeights::captureOpening,  { 0.0, 1.6 }, {} },
        { "captureMid",      &NineChess_AI_AB::EvalWeights::captureMid,      { 0.0, 1.6 }, {} },
        { "forkThreat",      &NineChess_AI_AB::EvalWeights::forkThreat,      {}, { 40, 80, 160 } },
        { "stalematePressure", &NineChess_AI_AB::EvalWeights::stalematePressure, {}, { 40, 80 } },
    };
    return columns;
}

// 打印一行权重为可直接回贴 s_evalWeightsPerRule 的 C++ 片段。
void printWeightsRow(const NineChess_AI_AB::EvalWeights& w)
{
    std::cout << "{ " << w.openingMaterial << ", " << w.openingInHand
        << ", " << w.openingMill << ", " << w.openingOpenMill
        << ",      " << w.midMaterial << ", " << w.midMill
        << ", " << w.midOpenMill << ", " << w.midMobility
        << ",       " << w.captureOpening << ", " << w.captureMid
        << ",    " << w.forkThreat << ",   " << w.stalematePressure << " }";
}

// baseline 行 vs candidate 行的对抗（交替执先）；返回引擎2（candidate）的平均得分率。
double playTuneMatch(uint32_t ruleIndex, const NineChess_AI_AB::EvalWeights& baselineRow,
    const NineChess_AI_AB::EvalWeights& candidateRow, int games, int depth, int64_t seedBase,
    uint32_t threads)
{
    int64_t score2x2 = 0; // 引擎2 得分×2：胜=2 和=1 负=0，避免浮点累计误差
    for (int64_t g = 0; g < games; ++g) {
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
        options.threads = threads;
        options.dynamicDepth = false;
        options.useCustomWeights = true;
        options.weights = swapColors ? candidateRow : baselineRow;
        options.seed = static_cast<uint64_t>(seedBase + g * 2 + 1);
        ai1.setOptions(options);
        options.weights = swapColors ? baselineRow : candidateRow;
        options.seed = static_cast<uint64_t>(seedBase + g * 2 + 2);
        ai2.setOptions(options);

        ai1.setChess(game);
        ai2.setChess(game);
        ai1.clearTranspositionTable();

        int64_t plies = 0;
        while (game.whoWin() == NineChess::NOBODY && plies < 300) {
            const bool firstTurn = game.getTurn() == NineChess::PLAYER1;
            NineChess_AI_AB& ai = firstTurn ? ai1 : ai2;
            ai.setChess(game);
            ai.alphaBetaPruning(depth);
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
        // 引擎2 = candidate；换色时引擎2 执先手（玩家1 位置）。
        const bool candidateWon = swapColors ? (winner == NineChess::PLAYER1)
                                             : (winner == NineChess::PLAYER2);
        const bool baselineWon = swapColors ? (winner == NineChess::PLAYER2)
                                            : (winner == NineChess::PLAYER1);
        if (candidateWon) {
            score2x2 += 2;
        }
        else if (!baselineWon) {
            score2x2 += 1;
        }
    }
    return games > 0 ? static_cast<double>(score2x2) / (2.0 * games) : 0.5;
}

// tune [games] [depth] [seed] [rounds] [threads]
// 坐标下降调参：逐列尝试候选值（引擎2=候选 vs 引擎1=当前行，交替执先），
// 取胜率 >50% 的最优者贪心更新当前行；rounds>1 时重复整轮扫描（列间交互补偿）。
// threads 默认 0 = 按 CPU 核数自动（多线程只加速搜索，同深度下双方公平）。
// 结束后与原始行做终局验证对局，并打印可直接回贴 s_evalWeightsPerRule 的行。
// 只打印建议，不改动源码中的表；采纳由人工复核后进行。
void runTuneCommand(const std::string& argText, const NineChess& chess)
{
    int64_t games = 8;
    int64_t depth = 6;
    int64_t seed = 1;
    int64_t rounds = 1;
    int64_t threads = 0;
    const std::vector<std::string> args = splitArgs(argText);
    int64_t* slots[] = { &games, &depth, &seed, &rounds, &threads };
    for (size_t i = 0; i < args.size() && i < 5; ++i) {
        tryParseInt64(args[i], *slots[i]);
    }
    if (games <= 0) {
        games = 8;
    }
    if (depth <= 0) {
        depth = 6;
    }
    if (rounds <= 0) {
        rounds = 1;
    }
    const uint32_t threadCount = threads > 0
        ? static_cast<uint32_t>(threads)
        : NineChess_AI_AB::defaultThreadCount();

    const uint32_t ruleIndex = chess.getRuleIndex();
    NineChess_AI_AB::EvalWeights baselineRow =
        NineChess_AI_AB::tableWeights(ruleIndex);
    const NineChess_AI_AB::EvalWeights originalRow = baselineRow;

    std::cout << "tune 开始: 规则=" << chess.getRule()->name
        << " 每候选 " << games << " 局 深度 " << depth
        << " 轮数 " << rounds << " 种子 " << seed
        << " 线程 " << threadCount << "\n"
        << "原始行: ";
    printWeightsRow(baselineRow);
    std::cout << "\n" << std::flush;

    for (int64_t round = 0; round < rounds; ++round) {
        for (const TuneColumn& column : tuneColumns()) {
            const int32_t currentValue = baselineRow.*column.field;
            std::vector<int32_t> candidates;
            if (currentValue == 0) {
                candidates = column.zeroCandidates;
            }
            else {
                for (const double factor : column.factors) {
                    const int32_t value = static_cast<int32_t>(
                        std::lround(currentValue * factor));
                    if (value != currentValue) {
                        candidates.push_back(value);
                    }
                }
            }
            if (candidates.empty()) {
                continue;
            }

            int32_t bestValue = currentValue;
            double bestRate = 0.5;
            for (const int32_t candidate : candidates) {
                NineChess_AI_AB::EvalWeights row = baselineRow;
                row.*column.field = candidate;
                const double rate = playTuneMatch(ruleIndex, baselineRow, row,
                    static_cast<int>(games), static_cast<int>(depth),
                    seed * 1000000 + round * 10000
                        + static_cast<int64_t>(&column - tuneColumns().data()) * 100,
                    threadCount);
                std::cout << "  列=" << column.name << " " << currentValue
                    << " -> " << candidate
                    << "  得分率 " << static_cast<int>(rate * 100) << "%\n" << std::flush;
                if (rate > bestRate) {
                    bestRate = rate;
                    bestValue = candidate;
                }
            }
            if (bestValue != currentValue) {
                baselineRow.*column.field = bestValue;
                std::cout << "  列=" << column.name << " 采用 " << bestValue << "\n" << std::flush;
            }
        }
    }

    std::cout << "调参后行: ";
    printWeightsRow(baselineRow);
    std::cout << "\n";

    // 终局验证：原始行 vs 调参后行（局数翻倍）。
    const double finalRate = playTuneMatch(ruleIndex, originalRow, baselineRow,
        static_cast<int>(games * 2), static_cast<int>(depth),
        seed * 1000000 + 999999, threadCount);
    std::cout << "终局验证: 调参行 vs 原始行 得分率 "
        << static_cast<int>(finalRate * 100) << "%（"
        << games * 2 << " 局，>50% 表示调参行更强）\n";
    std::cout << "==== tune 完成 ====\n" << std::flush;
}

// bookstat
void runBookStatCommand(const NineChess& chess)
{
    std::cout << "开局库状态: 规则=" << chess.getRule()->name
        << " 启用=" << (g_bookEnabled ? "是" : "否")
        << " 书深度=" << g_book.bookDepth()
        << " 条目=" << g_book.entryCount()
        << " 记录=" << g_book.totalRecords() << "\n";

    // 展示样本量最大的几个条目（走法按胜率排序）。
    size_t shown = 0;
    std::vector<std::pair<uint64_t, const NineChessOpeningBook::Entry*>> sorted;
    for (const auto& item : g_book.entries()) {
        sorted.push_back(std::make_pair(item.first, &item.second));
    }
    std::sort(sorted.begin(), sorted.end(),
        [](const std::pair<uint64_t, const NineChessOpeningBook::Entry*>& lhs,
            const std::pair<uint64_t, const NineChessOpeningBook::Entry*>& rhs) {
            return lhs.second->totalPlays > rhs.second->totalPlays;
        });
    for (const auto& item : sorted) {
        if (shown >= 5) {
            break;
        }
        std::cout << "  key=" << std::hex << item.first << std::dec
            << " 局数=" << item.second->totalPlays << "\n";
        for (const NineChessOpeningBook::MoveStats& moveStats : item.second->moves) {
            int type = 0;
            int from = -1;
            int to = -1;
            NineChessOpeningBook::decodeMove(moveStats.move, type, from, to);
            std::cout << "    " << formatBookCommand(type, from, to)
                << " 局数=" << moveStats.plays
                << " 胜率=" << (moveStats.plays > 0
                    ? static_cast<int>(moveStats.score * 100 / static_cast<int>(moveStats.plays)) : 0)
                << "%\n";
        }
        ++shown;
    }
}

bool tryHandleRuleCommand(const std::string& cmd, NineChess& chess, bool& changed)
{
    changed = false;

    if (cmd == "rules") {
        printRulesDetailed();
        return true;
    }

    if (!startsWith(cmd, "rule")) {
        return false;
    }

    const std::string arg = trimCopy(cmd.substr(4));
    if (arg.empty()) {
        printRulesDetailed();
        return true;
    }

    uint32_t ruleIndex = 0u;
    if (!tryParseRuleIndex(arg, ruleIndex)) {
        std::cout << "rule 命令格式错误，请使用: rule N\n";
        return true;
    }

    chess.setRule(ruleIndex);
    chess.start();
    changed = true;
    std::cout << "已切换到规则 " << ruleIndex << " : " << chess.getRule()->name << "\n";
    if (loadBookFor(ruleIndex, chess)) {
        std::cout << "开局库: 已加载 " << g_book.entryCount() << " 条（"
            << bookPathFor(ruleIndex) << "）\n";
    }
    else {
        std::cout << "开局库: 该规则暂无书文件（" << bookPathFor(ruleIndex) << "），可先 booktrain\n";
    }
    printRuleDetail(ruleIndex);
    printBoard(chess);
    return true;
}

} // namespace

int main(int argc, char* argv[])
{
    initConsoleUtf8();
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    NineChess chess;
    uint32_t startupRuleIndex = 0u;
    const bool hasStartupRule = tryParseStartupRuleIndex(argc, argv, startupRuleIndex);
    if (hasStartupRule) {
        chess.setRule(startupRuleIndex);
    }
    chess.start();
    if (loadBookFor(chess.getRuleIndex(), chess)) {
        std::cout << "开局库: 已加载 " << g_book.entryCount() << " 条\n";
    }
    else {
        std::cout << "开局库: 该规则暂无书文件，可先 booktrain\n";
    }
    std::vector<NineChess> undoStack;

    std::cout << "NineChess 命令行测试\n";
    if (hasStartupRule) {
        std::cout << "启动规则: " << startupRuleIndex << " : " << chess.getRule()->name << "\n";
    }
    else if (argc > 1) {
        std::cout << "启动参数中的规则编号无效，已回退到默认规则: "
            << chess.getRule()->name << "\n";
    }
    std::cout << "输入 help 查看命令说明。\n";
    printBoard(chess);

    for (;;) {
        std::cout << "\n> " << std::flush;

        std::string cmd;
        if (!std::getline(std::cin, cmd)) {
            std::cout << "\n输入流结束，程序退出。\n";
            break;
        }

        cmd = trimCopy(cmd);
        if (cmd.empty()) {
            continue;
        }

        if (cmd == "quit" || cmd == "exit") {
            std::cout << "程序结束。\n";
            break;
        }

        if (cmd == "help" || cmd == "?") {
            printHelp();
            continue;
        }

        // AI 命令：search / match（支持带参数形式）
        {
            const size_t space = cmd.find(' ');
            const std::string head = space == std::string::npos ? cmd : cmd.substr(0, space);
            const std::string rest = space == std::string::npos ? "" : cmd.substr(space + 1);
            if (head == "search") {
                runSearchCommand(rest, chess);
                continue;
            }
            if (head == "match") {
                runMatchCommand(rest, chess);
                continue;
            }
            if (head == "vs") {
                runVsCommand(rest, chess);
                continue;
            }
            if (head == "booktrain") {
                runBookTrainCommand(rest, chess);
                continue;
            }
            if (head == "bookstat") {
                runBookStatCommand(chess);
                continue;
            }
            if (head == "selfcheck") {
                runSelfCheckCommand(rest);
                continue;
            }
            if (head == "tune") {
                runTuneCommand(rest, chess);
                continue;
            }
            if (head == "bookon") {
                g_bookEnabled = true;
                std::cout << "开局库已启用。\n";
                continue;
            }
            if (head == "bookoff") {
                g_bookEnabled = false;
                std::cout << "开局库已禁用。\n";
                continue;
            }
            if (head == "booksave") {
                ensureBookDir();
                const std::string path = rest.empty() ? bookPathFor(chess.getRuleIndex()) : rest;
                std::cout << (g_book.save(path) ? "开局库已保存: " : "开局库保存失败: ") << path << "\n";
                continue;
            }
            if (head == "bookload") {
                const std::string path = rest.empty() ? bookPathFor(chess.getRuleIndex()) : rest;
                g_bookEnabled = g_book.load(path, chess);
                std::cout << (g_bookEnabled ? "开局库已加载: " : "开局库加载失败或规则不匹配: ") << path << "\n";
                continue;
            }
        }

        if (cmd == "board") {
            printBoard(chess);
            continue;
        }

        if (cmd == "history") {
            printHistory(chess);
            continue;
        }

        if (cmd == "undo") {
            if (undoStack.empty()) {
                std::cout << "没有可撤销的局面。\n";
                printBoard(chess);
                continue;
            }

            chess = undoStack.back();
            undoStack.pop_back();
            std::cout << "已撤销一步。\n";
            printBoard(chess);
            continue;
        }

        if (cmd == "new") {
            undoStack.push_back(chess);
            chess.reset();
            chess.start();
            printBoard(chess);
            continue;
        }

        // 对局配置命令 r<s<t<（棋谱首行）：规则段复用 rule 切换流程重开局，
        // 限步/限时段仅记录并回显——控制台不执行计时，模型层也不感知
        {
            int setupRule = -1;
            int setupSteps = -1;
            int setupTime = -1;
            if (parseSetupCommand(cmd, setupRule, setupSteps, setupTime)) {
                bool ruleChanged = false;
                if (setupRule >= 0) {
                    if (setupRule >= RULE_COUNT) {
                        std::cout << "规则编号无效: " << setupRule
                            << "（有效范围 0.." << RULE_COUNT - 1 << "）\n";
                        continue;
                    }
                    const NineChess before = chess;
                    tryHandleRuleCommand("rule " + std::to_string(setupRule), chess, ruleChanged);
                    if (ruleChanged)
                        undoStack.push_back(before);
                }
                if (setupSteps >= 0)
                    g_setupSteps = setupSteps;
                if (setupTime >= 0)
                    g_setupTime = setupTime;
                std::cout << "对局配置: 限步="
                    << (g_setupSteps > 0 ? std::to_string(g_setupSteps) + " 步" : "不限")
                    << "，限时="
                    << (g_setupTime > 0 ? std::to_string(g_setupTime) + " 分钟" : "不限")
                    << "（控制台不计时，仅记录）\n";
                if (!ruleChanged)
                    printBoard(chess);
                continue;
            }
        }

        const NineChess snapshot = chess;
        bool ruleChanged = false;
        if (tryHandleRuleCommand(cmd, chess, ruleChanged)) {
            if (ruleChanged) {
                undoStack.push_back(snapshot);
            }
            continue;
        }

        undoStack.push_back(chess);
        if (!chess.command(cmd.c_str())) {
            undoStack.pop_back();
            std::cout << "命令无效，或该命令在当前局面下不合法。\n";
            printBoard(chess);
            continue;
        }

        printBoard(chess);
    }

    return 0;
}


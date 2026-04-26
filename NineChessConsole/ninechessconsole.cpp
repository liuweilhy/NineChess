// NineChessConsole.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <cctype>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ninechess.h"

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
        << "  help               显示帮助\n"
        << "  quit               退出程序\n"
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


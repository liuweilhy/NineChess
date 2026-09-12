// 调试工具：把棋谱同时回放到新旧两个模型，逐命令打印状态并比对差异。
// 用法: ReplayDebug.exe <棋谱文件，新模型0-based命令流，含 r?s?t? 头>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

#include "old_engine/ninechess.h"
#include "../NineChess/src/ninechess.h"

static std::vector<std::string> splitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
            line.pop_back();
        if (!line.empty())
            lines.push_back(line);
    }
    return lines;
}

// 新 0-based → 旧 1-based
static std::string toOld(const std::string& cmd)
{
    int a, b, c, d;
    char buf[64] = {};
    if (sscanf(cmd.c_str(), "(%d,%d)->(%d,%d)", &a, &b, &c, &d) == 4) {
        sprintf(buf, "(%d,%d)->(%d,%d)", a + 1, b + 1, c + 1, d + 1);
        return buf;
    }
    if (sscanf(cmd.c_str(), "-(%d,%d)", &a, &b) == 2) {
        sprintf(buf, "-(%d,%d)", a + 1, b + 1);
        return buf;
    }
    if (sscanf(cmd.c_str(), "(%d,%d)", &a, &b) == 2) {
        sprintf(buf, "(%d,%d)", a + 1, b + 1);
        return buf;
    }
    return cmd;
}

int main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("usage: ReplayDebug.exe <record file>\n");
        return 1;
    }
    std::ifstream fin(argv[1]);
    if (!fin) {
        printf("cannot open %s\n", argv[1]);
        return 1;
    }
    std::stringstream ss;
    ss << fin.rdbuf();
    std::vector<std::string> lines = splitLines(ss.str());
    if (lines.empty()) {
        printf("empty record\n");
        return 1;
    }

    int ruleNo = 2, steps = 0, timelimit = 0;
    size_t startLine = 0;
    int r, s, t;
    if (sscanf(lines[0].c_str(), "r%ds%dt%d", &r, &s, &t) == 3) {
        ruleNo = r;
        steps = s;
        startLine = 1;
    }

    NineChess nw;
    nw.setRule((uint32_t)ruleNo);
    nw.start();
    ncold::NineChess od;
    od.setData(&ncold::NineChess::RULES[ruleNo], steps, 0);
    od.start();

    printf("rule=%d steps=%d\n", ruleNo, steps);
    for (size_t i = startLine; i < lines.size(); ++i) {
        const std::string& cmdNew = lines[i];
        const std::string cmdOld = toOld(cmdNew);
        bool okNew = nw.command(cmdNew.c_str());
        bool okOld = od.command(cmdOld.c_str());
        if (!okNew || !okOld) {
            printf("[%2zu] new=%s(%d) old=%s(%d) !! 命令被拒绝，停止\n",
                i - startLine + 1, cmdNew.c_str(), (int)okNew, cmdOld.c_str(), (int)okOld);
            break;
        }

        // 旧模型棋盘转位棋盘（含禁点与编号层）
        uint32_t p1 = 0, p2 = 0, forb = 0, nums[9] = {};
        const bool numbered = (ruleNo == 2);
        const char* bd = od.getBoard();
        for (int pos = 0; pos < 24; ++pos) {
            const char ch = bd[(pos / 8 + 1) * 8 + pos % 8];
            const uint32_t bit = 1u << pos;
            if (ch & 0x10) {
                p1 |= bit;
                if (numbered) nums[(ch & 0x0F) - 1] |= bit;
            } else if (ch & 0x20) {
                p2 |= bit;
                if (numbered) nums[(ch & 0x0F) - 1] |= bit;
            } else if (ch == '\x0f') {
                forb |= bit;
            }
        }
        const auto& cd = nw.getData();
        const bool over = nw.getPhase() == NineChess::GAME_OVER || od.getPhase() == ncold::NineChess::GAME_OVER;
        bool mismatch = false;
        if (p1 != cd.player1Board || p2 != cd.player2Board || forb != cd.forbiddenBoard)
            mismatch = true;
        if (numbered) {
            for (int k = 0; k < 9; ++k)
                if (nums[k] != cd.numberBoards[k])
                    mismatch = true;
        }
        // 终局时新模型清空动作/轮次位，旧模型保留最后动作，属正常表示差异
        if (!over) {
            if ((int)nw.getAction() / 0x200 != (int)od.getAction() / 0x100)  // CHOOSE/PLACE/CAPTURE 序号比对
                mismatch = true;
            if ((int)cd.getPendingCaptures() != od.getNum_NeedRemove())
                mismatch = true;
            const bool turnMismatch =
                (od.whosTurn() == ncold::NineChess::PLAYER1) != (nw.getTurn() == NineChess::PLAYER1);
            if (turnMismatch)
                mismatch = true;
        }

        printf("[%2zu] %-18s phase n%d/o%d act n%d/o%d turn n%d/o%d pend n%u/o%d | in n%u/%u o%d/%d | p1 %08X/%08X p2 %08X/%08X forb %08X/%08X %s\n",
            i - startLine + 1, cmdNew.c_str(),
            (int)nw.getPhase(), (int)od.getPhase(),
            (int)nw.getAction(), (int)od.getAction(),
            (int)nw.getTurn(), (int)od.whosTurn(),
            cd.getPendingCaptures(), od.getNum_NeedRemove(),
            nw.getPlayer1InHand(), nw.getPlayer2InHand(),
            od.getPlayer1_InHand(), od.getPlayer2_InHand(),
            cd.player1Board, p1, cd.player2Board, p2, cd.forbiddenBoard, forb,
            mismatch ? "<<<< 分歧" : "");

        if (nw.whoWin() != NineChess::NOBODY || od.whoWin() != ncold::NineChess::NOBODY) {
            printf("终局: new winner=%d, old winner=%d\n",
                (int)nw.whoWin(), (int)od.whoWin());
            break;
        }
    }
    return 0;
}

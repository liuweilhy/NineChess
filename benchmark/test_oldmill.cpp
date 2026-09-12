#include <cstdio>
#include "old_engine/ninechess.h"

int main()
{
    // 精确重放 rule1_g01 前 16 步（旧模型 1-based 坐标）
    ncold::NineChess g;
    g.setData(&ncold::NineChess::RULES[1], 100, 0);
    g.start();
    const char* cmds[] = {"(3,5)","(1,7)","(3,8)","(1,1)","(1,8)","(1,5)","(3,1)","(2,7)",
                          "(3,7)","(2,2)","(3,6)","-(1,5)","(3,3)","(2,4)","(2,1)","(2,8)"};
    int step = 0;
    for (auto c : cmds) {
        ++step;
        bool ok = g.command(c);
        printf("[%2d] %-8s ok=%d pend=%d turn=%d act=%d\n", step, c, ok, g.getNum_NeedRemove(),
            (int)g.whosTurn(), (int)g.getAction());
        if (!ok) { printf("REJECTED\n"); break; }
    }
    return 0;
}

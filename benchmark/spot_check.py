#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""spot_check.py — 对棋谱中的"不堵活二"局面做引擎定点核对。

用法: python spot_check.py <棋谱文件> <事件ply> [深度=12] [再跑一个深度=0表示跳过]

重放棋谱前 ply-1 条命令得到局面，对该局面调用 NineChessConsole 的 search
（纯最优、单线程、开 q/pvs 与引擎默认一致），解析"根走法分数"，
对比"堵点走法"与"棋谱实际所走"的分差，并打印前若干名走法。
"""
import re
import subprocess
import sys

LINES = []
for _c in range(3):
    _b = _c * 8
    for _t in ((7, 0, 1), (1, 2, 3), (3, 4, 5), (5, 6, 7)):
        LINES.append((_b + _t[0], _b + _t[1], _b + _t[2]))
for _p in (0, 2, 4, 6):
    LINES.append((_p, 8 + _p, 16 + _p))

POS_LINES = {i: [] for i in range(24)}
for _ln in LINES:
    for _q in _ln:
        POS_LINES[_q].append(_ln)

RE_PLACE = re.compile(r'^\((\d),(\d)\)$')
RE_SHIFT = re.compile(r'^\((\d),(\d)\)->\((\d),(\d)\)$')
RE_CAP = re.compile(r'^-\((\d),(\d)\)$')


def pos_of(c, p):
    return c * 8 + p


def fmt_pos(pos):
    return '(%d,%d)' % (pos // 8, pos % 8)


def forms_mill(board, player, pos):
    return any(all(board.get(q) == player for q in ln) for ln in POS_LINES[pos])


def open_two_points(board, player):
    pts = set()
    for ln in LINES:
        cnt = sum(1 for q in ln if board.get(q) == player)
        if cnt == 2:
            empt = [q for q in ln if q not in board]
            if len(empt) == 1:
                pts.add(empt[0])
    return pts


def replay_prefix(cmds):
    """回放命令前缀，返回 (board, turn)。规则0语义。"""
    board = {}
    turn = 1
    for cmd in cmds:
        cmd = cmd.strip()
        if not cmd or (cmd[0] == 'r' and len(cmd) > 1 and cmd[1].isdigit()):
            continue
        m = RE_PLACE.match(cmd)
        if m:
            pos = pos_of(int(m.group(1)), int(m.group(2)))
            board[pos] = turn
            if not forms_mill(board, turn, pos):
                turn = 3 - turn
            continue
        m = RE_CAP.match(cmd)
        if m:
            board.pop(pos_of(int(m.group(1)), int(m.group(2))), None)
            turn = 3 - turn
            continue
        m = RE_SHIFT.match(cmd)
        if m:
            frm = pos_of(int(m.group(1)), int(m.group(2)))
            to = pos_of(int(m.group(3)), int(m.group(4)))
            board.pop(frm, None)
            board[to] = turn
            if not forms_mill(board, turn, to):
                turn = 3 - turn
            continue
    return board, turn


def run_search(prefix_cmds, depth):
    """向 Console 喂前缀命令 + search，返回解析出的 [(move, score)] 列表（降序）。"""
    inp = '\n'.join(prefix_cmds) + '\n'
    inp += 'search %d 0 1 0 60 1 0 16 1 40 1 1 1 1\n' % depth
    inp += 'quit\n'
    proc = subprocess.run(
        ['bin\\NineChessConsole.exe', '0'], input=inp.encode('utf-8'),
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=r'd:\My program\QT\NineChess\benchmark',
        timeout=600)
    out = proc.stdout.decode('utf-8', errors='replace')
    scores = []
    in_sec = False
    for line in out.splitlines():
        if '根走法分数' in line:
            in_sec = True
            continue
        if in_sec:
            m = re.match(r'^\s*(\(\d,\d\))\s*=\s*(-?\d+)\s*$', line)
            if m:
                scores.append((m.group(1), int(m.group(2))))
            elif line.strip() == '':
                continue
            else:
                in_sec = False
    return scores, out


def main():
    fn, ply = sys.argv[1], int(sys.argv[2])
    depth = int(sys.argv[3]) if len(sys.argv) > 3 else 12
    depth2 = int(sys.argv[4]) if len(sys.argv) > 4 else 0

    with open(fn, encoding='utf-8') as fp:
        cmds = [l.strip() for l in fp.read().splitlines() if l.strip()]
    cmds = [c for c in cmds if not (c[0] == 'r' and len(c) > 1 and c[1].isdigit())]
    prefix = cmds[:ply - 1]
    chosen = cmds[ply - 1]

    board, turn = replay_prefix(prefix)
    opp = 3 - turn
    comp = open_two_points(board, opp)
    own = open_two_points(board, turn)
    print('文件: %s  事件ply=%d  轮走=P%d  实选招=%s' % (fn, ply, turn, chosen))
    print('对手(P%d)活二成三点: %s   自己(P%d)成三点: %s' %
          (opp, [fmt_pos(c) for c in comp], turn, [fmt_pos(c) for c in own]))

    for d in ([depth] if depth2 == 0 else [depth, depth2]):
        scores, raw = run_search(prefix, d)
        if not scores:
            print('深度%d: 未能解析根走法分数（输出异常）' % d)
            continue
        best = scores[0]
        sc = {mv: s for mv, s in scores}
        print('深度%d: 最优 %s=%d | 实选 %s=%s | 堵点 %s' %
              (d, best[0], best[1], chosen,
               sc.get(chosen, '未在列表'), [sc.get(fmt_pos(c), 'N/A') for c in comp]))
        print('  前五: %s' % ', '.join('%s=%d' % (mv, s) for mv, s in scores[:5]))
    print()


if __name__ == '__main__':
    main()

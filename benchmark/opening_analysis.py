#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""opening_analysis.py — 规则0自对弈棋谱的"开局活二(2连)堵点率"分析工具。

用法: python opening_analysis.py <棋谱目录> [更多目录...]

输入为 benchmark 驱动产出的纯命令流棋谱（首行 r0s<t0，0-based (c,p) 坐标）。
按规则0（成三棋）语义回放：摆子/走子成三后轮走方保持轮次提子，提子后换手。
对开局阶段（任一方仍有手牌）的每一次摆子，统计摆子前对手是否存在活二
（一条线上对手 2 子 + 1 空位），并检查本方摆子是否落在成三点上（堵）。

分类口径：
- blocked            : 摆子点 ∈ 对手活二的成三点集合
- n_comp             : 对手活二成三点的去重个数（≥2 表示堵不完）
- own_mill           : 本次摆子自己成三（反击性不堵）
- had_mill_opt       : 摆子前自己也有成三点（有反击选项）
- opp_mill_next      : 不堵之后对手的下一手摆子/走子立即成三（立即代价）
"""
import os
import re
import sys
from collections import Counter

# 规则0几何：3圈各4条 + 偶数位跨圈4条 = 16 条三连线
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


def winner_from_name(fn):
    if 'p1win' in fn:
        return 1
    if 'p2win' in fn:
        return 2
    return 0  # draw / 未知


def forms_mill(board, player, pos):
    return any(all(board.get(q) == player for q in ln) for ln in POS_LINES[pos])


def open_two_points(board, player):
    """player 的活二成三点集合（去重）。"""
    pts = set()
    for ln in LINES:
        cnt = sum(1 for q in ln if board.get(q) == player)
        if cnt == 2:
            empt = [q for q in ln if q not in board]
            if len(empt) == 1:
                pts.add(empt[0])
    return pts


def analyze_game(cmds, winner, fname=''):
    board = {}
    hand = {1: 9, 2: 9}
    turn = 1
    events = []
    pending = None  # 待结算的"不堵"事件：等对手下一手是否成三
    ply = 0
    for raw in cmds:
        cmd = raw.strip()
        if not cmd:
            continue
        if cmd[0] == 'r' and len(cmd) > 1 and cmd[1].isdigit():
            continue  # 棋谱头 r<rule>s<steps>t0
        ply += 1
        m = RE_PLACE.match(cmd)
        if m:
            pos = pos_of(int(m.group(1)), int(m.group(2)))
            mover, opp = turn, 3 - turn
            opening = hand[1] > 0 or hand[2] > 0
            comp_points = open_two_points(board, opp) if opening else set()
            own_mill_opts = open_two_points(board, mover)
            board[pos] = mover
            hand[mover] -= 1
            mill = forms_mill(board, mover, pos)
            # 结算上一条"不堵"事件：对手这一手摆子是否立即成三
            if pending is not None and mover == pending['wait_player']:
                pending['event']['opp_mill_next'] = mill
                pending = None
            if comp_points:
                ev = dict(ply=ply, mover=mover, n_comp=len(comp_points),
                          comp=list(comp_points), pos=pos,
                          blocked=pos in comp_points, own_mill=mill,
                          had_mill_opt=bool(own_mill_opts),
                          opp_mill_next=None, winner=winner, file=fname)
                events.append(ev)
                if not ev['blocked']:
                    pending = dict(wait_player=opp, event=ev)
            if mill:
                pass  # 成三保持轮次，等待提子命令
            else:
                turn = opp
            continue
        m = RE_CAP.match(cmd)
        if m:
            pos = pos_of(int(m.group(1)), int(m.group(2)))
            board.pop(pos, None)
            turn = 3 - turn
            continue
        m = RE_SHIFT.match(cmd)
        if m:
            frm = pos_of(int(m.group(1)), int(m.group(2)))
            to = pos_of(int(m.group(3)), int(m.group(4)))
            mover, opp = turn, 3 - turn
            board.pop(frm, None)
            board[to] = mover
            mill = forms_mill(board, mover, to)
            if pending is not None and mover == pending['wait_player']:
                pending['event']['opp_mill_next'] = mill
                pending = None
            if mill:
                pass
            else:
                turn = opp
            continue
        # -0 / -1 / == 等终局命令
    return events


def fmt_pos(pos):
    return '(%d,%d)' % (pos // 8, pos % 8)


def main():
    files = []
    for arg in sys.argv[1:]:
        if os.path.isdir(arg):
            files += [os.path.join(arg, f) for f in sorted(os.listdir(arg))
                      if f.endswith('.txt') and f.startswith('rule')]
        elif arg.endswith('.txt'):
            files.append(arg)
    if not files:
        print('no record files found')
        return

    all_events = []
    games = 0
    for fn in files:
        with open(fn, encoding='utf-8') as fp:
            cmds = fp.read().splitlines()
        evs = analyze_game(cmds, winner_from_name(os.path.basename(fn)), fn)
        all_events += evs
        games += 1
    print('棋谱 %d 份, 开局面对对手活二的摆子事件 %d 次\n' % (games, len(all_events)))

    def rate(sub, label):
        n = len(sub)
        if n == 0:
            print('%-46s 0 次' % label)
            return
        b = sum(1 for e in sub if e['blocked'])
        print('%-46s %3d 次, 堵 %2d (%.0f%%)' % (label, n, b, 100.0 * b / n))

    rate(all_events, '全部')
    rate([e for e in all_events if e['ply'] <= 10], '前10条命令内（随机窗口）')
    rate([e for e in all_events if e['ply'] > 10], '第11条起（纯最优）')
    rate([e for e in all_events if e['ply'] > 10 and e['n_comp'] == 1],
         '纯最优 & 对手仅1个成三点（必堵才防）')
    rate([e for e in all_events if e['ply'] > 10 and e['n_comp'] >= 2],
         '纯最优 & 对手≥2个成三点（堵不完）')
    rate([e for e in all_events if e['ply'] > 10 and e['own_mill']],
         '纯最优 & 本次摆子自己成三（反击）')
    rate([e for e in all_events if e['ply'] > 10 and not e['own_mill']],
         '纯最优 & 未自己成三')
    rate([e for e in all_events if e['ply'] > 10 and not e['own_mill']
          and not e['had_mill_opt']],
         '纯最优 & 未成三 & 也没有自己的成三点')

    clean = [e for e in all_events if e['ply'] > 10 and not e['blocked']
             and not e['own_mill'] and not e['had_mill_opt'] and e['n_comp'] == 1]
    print()
    print('不堵子集(纯最优&未堵&无反击&对手单点): %d 次' % len(clean))
    if clean:
        n_mill = sum(1 for e in clean if e['opp_mill_next'])
        n_win = sum(1 for e in clean if e['winner'] == e['mover'])
        print('  其中对手下一手立即成三: %d/%d' % (n_mill, len(clean)))
        print('  其中不堵方最终获胜(决胜局): %d/%d' %
              (n_win, sum(1 for e in clean if e['winner'] in (1, 2))))
        print('  案例清单（file:ply 摆子点/成三点）:')
        for e in clean:
            print('    %s ply%02d P%d 摆%s 堵点%s 对手成三=%s 胜方=%s' %
                  (os.path.basename(e['file']), e['ply'], e['mover'],
                   fmt_pos(e['pos']),
                   ','.join(fmt_pos(c) for c in e['comp']),
                   '是' if e['opp_mill_next'] else '否',
                   {1: '先手', 2: '后手', 0: '和'}[e['winner']]))

    # 给事件标注来源文件（供案例清单输出）
    # （在合并循环里补）
    print()


if __name__ == '__main__':
    main()

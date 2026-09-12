# NineChess 引擎对抗基准（benchmark）

对仓库内三代引擎做同进程直接对抗的程序与全部比赛数据。所有对局棋谱为工程标准
纯命令流格式（UTF-8 无 BOM，首行 `r<规则>s<限步>t0`，0-based 坐标），可在
`NineChessConsole` / GUI 中直接回放。

## 目录结构

```
benchmark/
├── README.md                    本文件
├── benchmark.cpp                驱动①：2018版 vs 当前版（支持随机/评估项参数）
├── benchmark_may.cpp            驱动②：2026-05版 vs 当前版（带评估项 A/B 开关）
├── benchmark_old_may.cpp        驱动③：2018版 vs 2026-05版
├── depthprobe.cpp               工具：各局面/各深度真实耗时探测
├── replay_debug.cpp             工具：双模型逐命令回放比对（调试规则分歧用）
├── test_oldmill.cpp             工具：成三判定对照小测试（调试遗留）
├── old_engine/                  2018-12-23 提交 a588f4a 源码副本（namespace ncold 隔离，
│                                仅宏/命名空间机械改动，零算法改动；其置换表在原码中即被注释停用）
├── may_engine/                  2026-05-05 提交 d78811a 源码副本（namespace ncmay 隔离，
│                                确认单线程、无 SearchOptions）
├── bin/                         编译产物（三个对局程序 + 三个工具）
├── build_*.bat                  自包含构建脚本（VS2019 v142，/O2 /utf-8）
└── results/                     全部比赛数据与报告（见 results/RESULTS_INDEX.md）
    ├── RESULTS_INDEX.md         全部实验比分总表
    ├── run01..run08_*           2018版 vs 当前版 八轮随机配置实验（规则0×20局/轮）
    ├── may_vs_current/          5月版 vs 当前版（4规则×20局=80局）
    ├── old2018_vs_may/          2018版 vs 5月版（规则0×20局，同深度8）
    └── evalab_A/B/C/            评估项隔离实验（纯最优 / 关winPressure / 再关pointValue）
```

## 构建与运行

需要 VS2019（v142 工具集）。每个脚本自包含，可单独执行：

```bat
build_benchmark.bat   &  bin\AIBenchmark.exe
build_may.bat         &  bin\AIBenchmarkMay.exe
build_oldmay.bat      &  bin\AIBenchmarkOldMay.exe
build_tools.bat       &  bin\DepthProbe.exe bin\ReplayDebug.exe bin\TestMill.exe
```

用法速查：

```text
AIBenchmark.exe     [起始规则] [结束规则] [每规则局数] [旧深度] [新深度] [限时ms] [新线程数]
                    当前源码内置新引擎配置：randomness=1, randomGap=20, randomPlies=9
AIBenchmarkMay.exe  [规则1] [规则2] [局数] [5月深度] [新深度] [限时ms] [新线程数] [纯最优0/1] [wp] [pv]
                    wp/pv=-1 表示用引擎默认（150/16）；pureBest=1 时关闭根随机
AIBenchmarkOldMay.exe [规则] [局数] [2018深度] [5月深度] [限时ms]
DepthProbe.exe      [规则] [棋谱文件] [前缀命令数]
ReplayDebug.exe     [棋谱文件]
```

对局机制约定（各驱动一致）：

- 每步限时内未算完则调用引擎 `quit()` 强制出招（2018 版行为同其 GUI：取中断时的结果）；
  新引擎自带时间预算，看门狗仅防挂死。
- 累计 100 条走子命令（摆/走/提各计 1，与 GUI 限步判和同口径）未分胜负判和。
- 奇偶局交替执先；每局独立种子保证样本不重复。
- 以现代模型为权威棋局判定胜负；对局另一方的旧模型可能存在规则实现差异
  （规则 1/2/3 上 2018 版有成三判定等历史 bug），走子被对方模型拒绝时以对方模型
  当前第一合法着法兜底替代并计数，保证对局完整结束。规则 0 下三代模型完全同步
  （全部实验 0 分歧、0 替代、0 被拒）。

## 结果速览（详细见 results/RESULTS_INDEX.md 与各目录内报告）

| 对阵 | 条件 | 比分 |
|---|---|---|
| 2018版 vs 当前版（8轮随机实验） | 规则0×20局×8轮，深度8/10 | 全局随机时当前版反而输（10:5~12:3）；受限随机/纯最优时反超（最高 1:17） |
| 5月版 vs 当前版 | 4规则×20局，深度10/10，单线程 vs 8线程 | 38:23 当前版落后（rP10+g30 配置）；纯最优+wp=0 后 15:31 反超 |
| 2018版 vs 5月版 | 规则0×20局，同深度8 | 5:14，5月版明显更强 |

## 关键结论

1. 4 月位棋盘重构是最大实力跃升：同深度 8 下 5月版 14:5 胜 2018 版，且每步快约 17 倍。
2. 当前引擎输给 5月版的原因是评估项 `winPressure=150`（隔离实验 A→B：20 胜→31 胜，
   规则1 从 8:10 翻转为 16:4），不是搜索或线程；`pointValue=16` 是正资产应保留。
3. 根节点随机在对抗中是纯负资产（带宽大小无关紧要，"开不开"才是决定性的）；
   多样性需求应交给开局库或 `randomPlies`（本轮已实现并加入引擎）。
4. 规则 0 的先手优势极大：任何一方执先对"会犯开局漂移错误的对手"近乎必胜。

## 注意事项

- `NineChess/src/ninechess_ai_ab.h/.cpp` 含本轮新增的 `randomPlies` 参数（默认 0 行为
  与旧版一致），尚未提交；建议跑一次 `tests/Run-All-RegressionTests.ps1` 后提交。
- 2018 版/5 月版源码副本仅作了机械性隔离（namespace、宏重命名），算法逻辑未动。

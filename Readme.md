# 九连棋 NineChess

`NineChess` 是一个以 Qt 编写的九子棋类游戏项目，当前仓库同时包含图形界面程序 `NineChess` 和命令行测试程序 `NineChessConsole`。
现版本的核心代码已经整理为“纯棋局模型 + 控制层赛制管理 + 位棋盘 Alpha-Beta AI”的结构，更适合继续扩展规则、调试 AI 和做回归测试。
AI 引擎已支持多线程搜索（Lazy SMP）、时间预算与中途打断、根节点随机选择、静默搜索，并附带一套按规则独立的统计式开局库；所有 AI 行为都可以在 `NineChessConsole` 中复现与验证。

## 支持规则

### 莫里斯九子棋
![莫里斯九子棋](./screenshot/莫里斯九子棋.PNG "Optional title")

Nine Men's Morris 是最经典的九子棋规则之一：

- 棋盘共有 24 个落点，双方各 9 枚棋子，轮流摆子。
- 任意一方形成“三连”后，可以提掉对手一子。
- 中局通常只能沿连线移动到相邻位置。
- 当一方只剩 3 子时，可以“飞子”到任意空位。
- 一方棋子少于 3 枚，或无法继续满足规则要求时判负。

### 成三棋

- 与莫里斯九子棋接近，但剩 3 子时不能飞子。
- 走棋阶段若被“闷”而无法行动，判负。

### 打三棋（12连棋）
![12子棋](./screenshot/12子棋.PNG "Optional title")

这套规则使用带斜线的棋盘，双方各有 12 枚棋子：

1. 摆棋阶段被提子的点位会暂时成为禁点，直到进入走棋阶段。
2. 如果开局把棋盘摆满，按规则判先手负。
3. 摆子完成后，由后摆棋的一方先走。
4. 一步若同时形成多个“三连”，可以连续提子。
5. 其余基础规则与成三棋相近。

### 九连棋
![九连棋](./screenshot/九连棋.PNG "Optional title")

九连棋是本项目默认规则，特点是棋子带编号：

1. 基础流程与成三棋相近。
2. 相同编号、相同位置形成的“三连”不能重复提子。
3. 走棋阶段若一方被“闷”，则由对手继续走棋，而不是直接判负。
4. 一步形成几个有效“三连”，就可以提几个子。

## 当前项目状态

### 图形界面
![GUI](./screenshot/GUI.PNG "Optional title")

当前仓库里的 GUI 工程已经恢复到可编译状态，并保留了原有的桌面界面风格。项目中正在持续整理旧代码与新结构之间的边界，当前比较重要的约定如下：

- `NineChess` 只负责纯棋规、局面状态、命令解析、局面变换与哈希。
- 限时、限步、超时判负、AI 超时强制出招等赛制逻辑由 `GameController` 负责。
- 外部裁定统一走 `adjudicateWin()` / `adjudicateDraw()`；`giveup()` 只表示认输。
- 命令行与文本棋谱约定使用 `(c,p)`、`(c1,p1)->(c2,p2)`、`-(c,p)`、`-0`、`-1`、`==`。

### 已实现的主要功能

1. 支持 4 套内置规则：成三棋、打三棋、九连棋、莫里斯九子棋。
2. 图形化棋盘、棋子动画、音效与状态栏提示。
3. 棋谱文本显示、命令回放与历史浏览。
4. 局面镜像、翻转、离散角度旋转，以及黑白交换等变换能力。
5. 人机对战，以及双 AI 自对弈的基础能力。
6. 独立的命令行测试程序，便于调试规则、命令和局面文本输出。
7. 位棋盘 Alpha-Beta 搜索引擎：迭代加深、分片置换表、TT 走法/杀手/历史排序、静默搜索、Aspiration 窗口、重复局面惩罚、点位结构价值评估。
8. Lazy SMP 多线程搜索，支持时间预算与中途打断。
9. 根节点随机选择（精确打分 + 分差阈值 + 加权随机，种子可复现）。
10. 基于自对弈统计的开局库：按规则独立、16 对称规范化压缩、多轮训练累积评分。

## AI 说明

当前 `NineChess_AI_AB` 已经按照位棋盘结构重写，重点包括：

- Alpha-Beta 剪枝搜索 + 迭代加深；中止或超时时保留最后一次完整算完的迭代结果；支持时间预算（每 1024 节点检查一次）与外部 `quit()` 打断。
- 走法排序管线：TT 最佳走法置顶、每层 2 个杀手走法、历史启发表、静态启发（即成三 / 活二 / 阻断威胁 / 线数）。
- 置换表：按规则独立、256 分片锁（多线程与双 AI 对弈互不阻塞），满时按"世代老化 > 非精确值 > 浅深度"清理。
- 对称规范化（实测定稿）：**仅开局节点**用 16 视角 canonical 哈希 + 根节点按对称类分组；中局回退普通哈希。实测开局 canonical 快 2.3 倍、中局全对称每节点慢约 4 倍，故默认取折中模式（`hash=1`，`0/2` 模式保留做 A/B）。
- 评估：子力、手牌、**可提三连**（编号规则下剔除已在历史中登记的同编号三连）、活二、机动性、点位结构价值（边中点优于角点；斜线规则下角点更优）。
- 静默搜索（quiescence）：深度 0 时展开"能形成新三连"的走法与强制提子，修正地平线高估（实测同一局面估值 -914 → -1094）。
- Aspiration 窗口：以上一迭代估值为中心渐进放宽（300 → 900 → 全窗口），失败时重搜。
- 重复局面惩罚：搜索内 2/4/6/8 层重复给当前轮次小负分，减少无意义互磨。
- 根节点随机选择：末层迭代对根走法精确打分，与最优分差 ≤ 阈值者进入候选集，按权重随机选；必胜 / 唯一走法不走随机；同种子可复现。
- Lazy SMP 多线程：`threads` 参数（1~N），各线程独立迭代加深、共享分片置换表与原子停止标志；实测 4 线程 NPS 约 3 倍、同一时间预算多完成一层深度。
- 开局库（`ninechess_book.*`）：`booktrain` 自对弈统计评分（胜 +1 / 和 0 / 负 -1，按"轮到方"视角），按规则独立保存 `books/book_<规则号>.dat`，16 对称规范化压缩；`match` / `vs` 对局中书内走法优先（胜率加权随机），被模型拒绝时回退搜索。
- 对称变换已抽离为共享模块 `ninechess_symmetry.*`（AI 置换表与开局库共用，含逆视角映射）。

### 实测结论（Release x64）

- 哈希模式：开局空盘深度 7，canonical 35,356 节点 / 24ms vs 普通哈希 150,854 节点 / 55ms；真实中局深度 6，全对称模式 NPS 约 0.49M vs 普通 1.30M（每节点开销约 4 倍，节点数相同）。
- 多线程：4 线程 NPS 约为单线程 3~3.4 倍；2 秒预算下完成深度 16 → 17。
- 开局库（规则 2，500 局训练）：空盘条目 530 样本，首着 `(2,0)` 胜率 81%；深度差 5v7 对抗 60 局，执先方用书后胜率 13 → 18（书内走法 180 次零拒绝）。
- 观察：当前评估下先手优势明显（深度 5~8 自对弈后手几乎不赢），多数对局以步数上限平局收场，引擎偏保守、残局转化偏慢——评估"进取性"调参是后续重点。

## 工程结构

项目目前大致遵循 MVC 思路：

### Model

- `NineChess/src/ninechess_common.h`
  核心公共常量、`Rule`、`ChessData`、位棋盘状态定义。
- `NineChess/src/ninechess.h/.cpp`
  纯棋局模型，负责规则、局面、命令、变换、哈希和胜负判定。
- `NineChess/src/ninechess_ai_ab.h/.cpp`
  Alpha-Beta AI。
- `NineChess/src/ninechess_symmetry.h/.cpp`
  16 个等价视角的对称变换共享工具（AI 置换表与开局库共用）。
- `NineChess/src/ninechess_book.h/.cpp`
  统计式开局库（按规则独立、对称压缩、自对弈评分）。

### View

- `NineChess/src/ninechesswindow.*`
  主窗口。
- `NineChess/src/gamescene.*`
  棋局场景。
- `NineChess/src/gameview.*`
  棋局视图。
- `NineChess/src/boarditem.*`
  棋盘图元。
- `NineChess/src/pieceitem.*`
  棋子图元。
- `NineChess/src/manuallistview.h`
  棋谱列表视图。

### Controller

- `NineChess/src/gamecontroller.*`
  管理对局流程、限时限步、界面同步和 AI 调度。
- `NineChess/src/aithread.*`
  AI 线程包装。

### Console Test

- `NineChessConsole/ninechessconsole.cpp`
  直接复用核心模型，适合做规则验证、命令行走子和回归测试。

## 构建说明

### Windows / Visual Studio

- 解决方案文件：`ninechess.sln`
- GUI 工程：`NineChess`
- 控制台工程：`NineChessConsole`
- 当前 GUI 工程配置已验证可在 `Qt 5.15.2 (msvc2019_64) + MSVC v142` 环境下编译。

### qmake

- GUI 工程同时保留 `NineChess/ninechess.pro`。
- 对 MSVC 已显式追加 `/utf-8`，避免无 BOM 的 UTF-8 源码被误判为本地代码页。

## 编码与文本格式

当前仓库已经统一以下约定：

- 源码、Markdown 和工程文本文件使用 `UTF-8 without BOM`。
- Windows 下统一使用 `CRLF` 换行。
- `.editorconfig`、`.gitattributes`、`AGENTS.md` 一起约束编码与换行。
- GUI、Console、核心源码都应与 `/utf-8` 编译选项保持一致。

## 命令行调试

`NineChessConsole` 适合快速验证规则、命令和 AI 行为，核心约定如下：

- 坐标全部使用 0-based。
- `rule N` 切换规则，`N` 范围为 `0..3`。
- `history` 查看命令历史，`undo` 回退一步，`new` 重新开局。
- 启动时可直接指定规则编号，例如：

```text
NineChessConsole.exe 2
NineChessConsole.exe --rule 2
```

AI 调试命令（参数缺省用默认值，`help` 有完整说明）：

- `search [d] [t] [h] [r] [g] [th] [s] [pv] [a] [rep] [q]`
  单局面搜索，打印最佳走法、估值、深度、耗时、节点、NPS、TT 统计与根走法分数。
- `match [n] [d] [h] [r] [th] [s] [mp]`
  同配置自对弈 n 局，报告胜负 / 步数 / 节点 / NPS。
- `vs [n] [d1] [h1] [pv1] [d2] [h2] [pv2] [s] [a] [rep]`
  不同配置引擎对抗（深度差、哈希模式、点位价值等），用于强度 A/B。
- `booktrain [n] [d] [s]` 自对弈训练开局库；`bookstat` 查看条目与胜率；
  `bookon` / `bookoff` 启停开局库（`match` / `vs` 生效）；`booksave` / `bookload` 存取。

参数速记：`h` 哈希模式（0 普通 / 1 仅开局规范化 / 2 全部规范化）、`r` 根节点随机、
`g` 随机分差、`th` 线程数（Lazy SMP）、`s` 种子、`pv` 点位价值权重、
`a` Aspiration 窗口、`rep` 重复惩罚、`q` 静默搜索。

## 测试与回归

仓库现在同时提供两层自动化测试：

- `tests/RuleHarness.vcxproj` + `tests/rule_harness.cpp`
  直接复用 `NineChess` 内核做规则级白盒测试，适合验证合法性判断、提子逻辑、堵死判负、三连历史、飞子规则等核心行为。
- `tests/Run-ConsoleBlackBoxTests.ps1`
  通过回放命令行输入、检查 `NineChessConsole` 输出做黑盒测试，适合验证 `rules` / `rule` / `history` / `undo` / `-0` / `==` / 启动参数等整机链路。

当前已经提供的测试入口如下：

- `tests/Test-Rule0-ChengSanQi.ps1`
  单独测试规则 0（成三棋）。
- `tests/Test-Rule1-DaSanQi.ps1`
  单独测试规则 1（打三棋 / 12 连棋）。
- `tests/Test-Rule2-JiuLianQi.ps1`
  单独测试规则 2（九连棋）。
- `tests/Test-Rule3-Morris.ps1`
  单独测试规则 3（莫里斯九子棋）。
- `tests/Run-All-RuleTests.ps1`
  顺序运行 4 个规则白盒测试，并输出汇总结果。
- `tests/Run-ConsoleBlackBoxTests.ps1`
  运行命令行黑盒回放测试，并输出汇总结果。
- `tests/Run-All-RegressionTests.ps1`
  一次性运行“规则白盒 + Console 黑盒”两层回归，是当前最推荐的总入口。
- `tests/Run-MatchBattery.ps1`
  长时间实战对局验证：批量运行多组 vs 对抗（开局库开关、深度差、点位价值等配置），输出对比表；`-Games N` 控制每组局数。

在 Windows PowerShell 中，可以直接这样执行总回归：

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\Run-All-RegressionTests.ps1"
```

如果只想跑规则测试，可以执行：

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\Run-All-RuleTests.ps1"
```

如果只想跑命令行黑盒测试，可以执行：

```powershell
powershell -ExecutionPolicy Bypass -File ".\tests\Run-ConsoleBlackBoxTests.ps1"
```

这套测试当前重点覆盖：

- 4 套规则下的开局、中局、提子与胜负判断；
- 非法招法不会污染局面与命令历史；
- 打三棋禁点复用、双三连多提子；
- 九连棋编号三连历史、被闷后的续走规则；
- 莫里斯九子棋三子飞行规则；
- `NineChessConsole` 的规则切换、历史、撤销、认输、判和与启动参数；
- AI 对局验证：`Run-MatchBattery.ps1` 批量 vs 对抗（书开关 / 深度差 / 评估配置），`match` / `vs` 自对弈与配置对比。

## 历史、许可与作者

- 更新历史见 [History.txt](./History.txt)
- 许可说明见 [Licence.txt](./Licence.txt)
- 原始项目作者：`liuweilhy`
- 联系方式：`liuweilhy@163.com`

项目最早的核心模型代码可追溯到 2013 年，Qt 图形界面版本在后续几年内逐步成形；当前仓库则在保留原始项目方向的基础上，继续整理规则层、控制层与 AI 的结构。

## 项目地址与下载

- 源码（Gitee）：[https://gitee.com/liuweilhy/NineChess](https://gitee.com/liuweilhy/NineChess)
- 发布页（Gitee）：[https://gitee.com/liuweilhy/NineChess/releases](https://gitee.com/liuweilhy/NineChess/releases)
- CSDN 资源页：[https://download.csdn.net/download/liuweilhy/10871298](https://download.csdn.net/download/liuweilhy/10871298)
- 百度网盘：[https://pan.baidu.com/s/1NZnmAUozbPt9K04fTouxMA](https://pan.baidu.com/s/1NZnmAUozbPt9K04fTouxMA)

## 捐助作者
![GUI](./screenshot/donate.png "donate")

# NineChess 代码库导览（AI_SUMMARY）

> 用途：为后续 AI 算法改进提供快速、准确的代码库认知，面向 AI Agent。
> 范围：纯模型（ninechess_common / ninechess）、NineChessConsole、tests、AI 引擎（ninechess_ai_ab）。
> Qt 界面细节从略，只保留与构建、AI 集成相关的接口约定。

## 1. 项目是什么

`NineChess`（九连棋）是一个"直棋"（九子棋类）桌面游戏仓库，位于 `D:\My program\QT\NineChess`：

- `NineChess`：Qt GUI 工程（本摘要不展开界面细节）。
- `NineChessConsole`：纯命令行测试程序，复用纯内核（无 Qt 依赖），是规则/命令/模型的首选调试工具。
- `tests`：RuleHarness 白盒测试 + PowerShell 回归脚本。
- 2026-04 已重构为"纯模型 + 控制层 + 位棋盘 Alpha-Beta AI"结构，代码注释详尽（中文），命名语义化。

默认规则是九连棋（rules[2]）。

## 2. 棋盘与坐标

- 3 圈 × 每圈 8 点 = 24 点。圈 `c`：0 = 外圈，1 = 中圈，2 = 内圈（`boarditem.cpp:26` 半径 `(RING-i)*LINE_INTERVAL` 确认圈序）。
- 位 `p`：从上方中点顺时针：0 上中、1 右上、2 右中、3 右下、4 下中、5 左下、6 左中、7 左上。
- 一维点位 `pos = c*8 + p`（0~23）；热路径全用 pos + 24-bit 位棋盘。
- 邻接：同圈 p±1；跨圈仅在偶数位（四个边中点）连通；打三棋规则全部 8 位允许跨圈（含对角线）（`buildMoveTable()`）。
- 三连线：每圈 4 条 `(7,0,1)(1,2,3)(3,4,5)(5,6,7)` → 12 条；跨圈线偶数位 4 条 → 16 条；打三棋加 4 条角位斜线 → 20 条。一点最多参与 3 条线。

## 3. 四套规则（`ninechess.cpp:174` 的 `rules[]`）

| 索引 | 名称 | 每方子数 | 斜线 | 禁点 | 后手先走 | 重复三连可再提 | 一步多提 | 摆满判负 | 被闷判负 | 三子可飞 |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | 成三棋 | 9 | 无 | 无 | 否 | 是（不编号） | 否（只提1） | 否（判和） | 是 | 否 |
| 1 | 打三棋(12连棋) | 12 | 有 | 有 | 是 | 是（不编号） | 是 | 是（先手负） | 是 | 否 |
| 2 | 九连棋（默认） | 9 | 无 | 无 | 否 | 否（编号，同编号同位置三连不可重复提） | 是 | 否（判和） | 否（轮空） | 否 |
| 3 | 莫里斯九子棋 | 9 | 无 | 无 | 否 | 是（不编号） | 否（只提1） | 否（判和） | 是 | 是 |

关键约定：`allowRepeatedMills == true` = 普通棋子；`false` = 九连棋编号棋子（启用 `numberBoards` + `millHistory`）。哈希、AI、GUI 显示都按此字段分支。

## 4. 核心数据结构（`ninechess_common.h`）

### 4.1 status 位打包（32-bit，当前用 14 bit）

- bit 0-3：后手手牌数（`--status` 即自减，调用方须保证 > 0）
- bit 4-5：待去子数 pendingCaptures（0~3）
- bit 6-8：阶段 phase
- bit 9-11：动作 action
- bit 12-13：轮次 turn

枚举值全部预移位（如 `ACTION_PLACE = 0x400`），可直接按位拼比。**只存后手手牌数，先手手牌数是推导值**（`getPlayer1InHand()`：仅当状态为 `开局|落子|后手` 或 `开局|提子|先手` 时先手少 1）。直接改 status 时极易算错先手手牌。

### 4.2 ChessData（模型真值）

- `status` + 三个主位棋盘 `player1Board / player2Board / forbiddenBoard`（各 24-bit）
- 九连棋专用：`numberBoards[9]`（编号 i 的棋子位置层，每层 ≤ 2 个 bit，分属双方）+ `millHistory`（`std::vector<MillKey>`，调用方保证去重追加）
- 显式拷贝构造/赋值（防止漏拷贝 numberBoards / millHistory）
- 只允许使用低 24 位（`VALID_BOARD_MASK`）

### 4.3 MillKey（16-bit 三连记录）

bit 0-2/3-5/6-8：该线 3 个固定位置上的棋子编号；bit 9-12：lineId；bit 13：玩家。lineId 固定则点位固定，故不存坐标。生成/读取用 `makeMillKey / getMillKeyPiece0/1/2 / getMillKeyLineId / isMillKeyPlayer2`。

### 4.4 哈希

- `getHashLite()`：三个主位棋盘交织压缩为 48-bit（每点 2-bit：00 空/01 先手/10 后手/11 禁点），高 16 位拼 status 有效位。
- `getHashHard()`：lite 基础上混入 9 层编号层（每层压成"先手位置+后手位置+层号"）+ 无序混合 millHistory（集合语义，与 push 顺序无关）。
- `NineChess::getHash()`：`allowRepeatedMills ? lite : hard` 自动选择。
- AI 侧注意：中局 `ACTION_PLACE`（已选子待落）时 selectedPos 属于搜索状态，必须混入 TT key（AGENTS.md 特别提醒）。

## 5. 状态机与对局流程

```
GAME_NOTSTARTED ──start()──▶ GAME_OPENING(PLACE, P1)
  ├─ 轮流摆子；落子成三 → ACTION_CAPTURE（pendingCaptures = 1，或多提规则下 = 新增三连数）
  ├─ 提子（不能提三连中的子，除非对手全在三连中；打三棋开局被提点位进 forbiddenBoard，中局入口清空）
  ├─ 双方手牌清空 → GAME_MID（按 defenderMovesFirst 定先手，ACTION_CHOOSE）
  │     选子 → ACTION_PLACE(selectedPos) → 移子/飞子 → 成三则 CAPTURE → 切回 CHOOSE
  └─ 结束：存活+手牌 < 3；开局摆满（按规则判负/和）；被闷（blockedIsLoss ? 负 : 轮空，双方都闷则和）
```

- 安全接口 `choose/place/capture`（校验+提示+命令历史）；快速接口 `chooseFast/placeFast/captureFast`（**零校验**，仅供 AI）。
- 复合走子命令 `(c1,p1)->(c2,p2)` 原子执行：先快照，两步任一失败整体回滚（`ninechess.cpp:608`）。
- 外部裁定统一走 `adjudicateWin()/adjudicateDraw()`；`giveup()` 只表示认输。
- 局面变换 `mirror/turn/flipVertical/rotate` 同步映射位棋盘、numberBoards、millHistory、selectedPos、命令历史。

## 6. 命令体系

走子命令：`(c,p)` 落子/选子、`(c1,p1)->(c2,p2)` 走子、`-(c,p)` 提子、`-0`/`-1` 认输、`==` 判和；另有 `giveup/resign/draw` 文本命令。GUI 棋谱、AI 出招、Console 回放全部复用这套文本格式。

## 7. 架构与文件地图

```
D:\My program\QT\NineChess\
├── ninechess.sln                 # VS 解决方案（GUI + Console + RuleHarness）
├── AGENTS.md                     # 工作区指令契约（边界、命名、编码），改动前必读
├── AI_SUMMARY.md                 # 本文档
├── NineChess\src\                # GUI 工程源码
│   ├── ninechess_common.h        # 常量、Rule、ChessData、status 位布局、MillKey、哈希
│   ├── ninechess.h/.cpp          # 纯模型：规则、状态机、命令解析、变换、胜负判定
│   ├── ninechess_ai_ab.h/.cpp    # Alpha-Beta AI（位棋盘版）← 优化主战场
│   ├── gamecontroller.*          # 控制层（赛制/计时/AI 调度/音效/动画）——含 Qt
│   ├── aithread.*                # AI 线程包装——含 Qt
│   └── （其余为界面文件，本摘要从略）
├── NineChessConsole\             # 命令行测试程序（模型 + AI，无 Qt）
├── tests\                        # RuleHarness + PowerShell 回归
└── Readme.md / History.txt / Licence.txt
```

依赖方向：模型（无 Qt）← 控制层 ← 视图；AI 通过 `friend class` 访问模型的 protected 成员（`m_data / m_selectedPos / m_rule / m_moveMask / m_lineMasks / m_validBoardMask / m_posLineIds / m_posLineCount` 等）。

## 8. AI 引擎（`ninechess_ai_ab.*`）

### 8.1 搜索流程

- Alpha-Beta + 迭代加深（1..depth），外部 `quit()`（原子标志）可中断；中断时保留上一层完整结果作为 `bestMove()`。
- 走法三类：`MOVE_PLACE / MOVE_SHIFT / MOVE_CAPTURE`；apply/undo 用快照（`ChessData + winner + selectedPos`）恢复。
- 根节点 `searchRoot` 与普通节点 `search` 分离；根节点记录最佳走法。
- 搜索状态在 `SearchContext` 中（每线程一份工作局面 + 杀手/历史/统计），类成员只保留只读共享数据（根局面、对称表、停止标志、时间预算），Lazy SMP 多线程安全。

### 8.2 置换表

- 按规则分 4 个全局 `TTStore`（同规则所有 AI 实例共享，`std::unordered_map<uint64_t, TTEntry>` + 整表 `std::mutex`），上限 256K 条目，满时按"世代老化 > 非精确值 > 浅深度"清理。
- 条目：value/depth/flag(EXACT|LOWER|UPPER)/generation；probe 时 `entry.depth >= depth` 才可用；store 时深度大的覆盖。
- **当前 TT 不存最佳走法，也不用于走法排序**（优化点）。

### 8.3 对称归一化

- 预计算 16 个等价变换（镜像 × 内外翻转 × 4 个离散旋转，`buildSymmetryVariants()`），probe/store 时对 16 个视角各算一次哈希取**最小值**作 canonical key（`makeCanonicalHash()`）。
- 编号规则的 millHistory 按 `targetSource` 槽位重排映射（`mapMillKey()`）。
- selectedPos 在 `ACTION_PLACE` 状态下混入（`mixSelectedPos()`）。
- **成本**：每个节点 probe + store 各算一次 canonical（= 16 次视角哈希）；编号规则每视角还要映射 9 层编号位棋盘 + millHistory 再跑 hard 哈希，开销可能是节点其余成本的数倍。（详见本文档第 12 节）

### 8.4 估值（`evaluate()`）

- 开局：在盘×120 + 手牌差×48 + 三连×96 + 活二×24
- 中局：在盘×180 + 三连×112 + 活二×32 + 机动性×10
- 提子阶段：pendingCaptures ×（开局 160 / 中局 220），按轮次正负
- 胜负分 ±30000（按 ply 衰减），边界 ±32000；`clampScore` 收敛
- 注意：**编号规则下三连计数不区分"可提/已提"**（millHistory 只进哈希不进估值），重复成三会被高估

### 8.5 走法排序（`orderMoves` / 静态启发）

- 落子即成三×2400、活二×240、阻断对手威胁×180、经过线数×48、拆自己三连−160
- 提子：3000 + 线数×128 + 三连×64 + 参与线数×32
- 根节点把上一迭代最佳走法 +20000 置顶（注意：这也意味着并列时永远选上一迭代的走法，与"随机性"目标冲突，最终选择逻辑需剥离该偏置）
- 无 TT 走法、无杀手走法、无历史启发（优化点）

## 9. NineChessConsole（`NineChessConsole\ninechessconsole.cpp`）

- 启动：`NineChessConsole.exe [规则号]` 或 `--rule N`；默认规则 2。
- 走子命令同第 6 节；附加命令：`board / history / undo / new / rules / rule N / help / quit`。
- 维护 undo 栈；每次成功命令后打印棋盘（`getConsoleText`，含阶段/手牌/在盘/待提/选中/提示/历史三连）。
- **已链接 AI**（`ninechess_ai_ab.cpp` 加入工程，无 Qt 依赖），并提供 `search / match / vs` 三个 AI 命令（详见第 13 节）。

## 10. 测试体系

- 白盒：`tests\rule_harness.cpp`（`RuleHarness.exe <0..3>`），直接构造 ChessData 中间局面（`setupMidgame / setupOpeningCapture`），覆盖禁点复用、双三连提子数、编号三连历史、被闷续走、飞子、堵死判负、非法命令不污染局面/历史。
- 黑盒：`Run-ConsoleBlackBoxTests.ps1` 回放命令行、检查 Console 输出。
- 总入口：`powershell -ExecutionPolicy Bypass -File ".\tests\Run-All-RegressionTests.ps1"`。
- AI 改动不影响模型测试；改动后仍应跑通回归，并另加 AI 冒烟/自对弈验证。

## 11. 构建与编码约定

- GUI 需 Qt 5.15.2（msvc2019_64）+ MSVC v142；`ninechess.pro` 与 `.vcxproj` 需同步（源文件列表、`/utf-8`）。
- 所有文本文件 UTF-8 无 BOM + CRLF；`.editorconfig` / `.gitattributes` / `AGENTS.md` 是契约。
- 命名：用 `ninechess_common.h` 的新字段名，不恢复旧别名。
- 新增源文件需同步更新：GUI 的 `.vcxproj` + `.filters` + `.pro`，Console 的 `.vcxproj`（+ `.filters`）。

## 12. AI 已知弱点与优化入口

1. ~~无 TT 走法 / 杀手走法 / 历史启发~~（已完成：TT 条目存最佳走法并置顶，每层 2 个杀手走法，历史启发表）。
2. ~~对称 canonical 哈希成本高~~（已实验定稿：默认"仅开局 canonical + 根节点对称分组"，中局普通哈希；`hash=0/2` 模式保留供 A/B。实测：开局 canonical 快 2.3 倍，中局全对称每节点慢约 4 倍）。
3. ~~MoveList 上限 128 截断在排序前~~（已提高到 256，覆盖飞子规则最大走法数）。
4. ~~编号规则评估盲区~~（已完成：可提三连 = 剔除已在 millHistory 登记的同编号三连，估值与排序同步生效）。
5. ~~无统计能力~~（已完成：节点/NPS/TT 命中/剪枝计数，`search` 命令可见）。
6. ~~TT 整表互斥锁 + 跨实例共享~~（已改为 256 分片锁，多线程与双 AI 对弈不再互相阻塞）。
7. 评估系数为魔数：开局/中局权重写死；点位价值权重已参数化（`pv` 参数，默认 16），其余系数仍待调参脚本。
8. 无 quiescence、无 aspiration window（根窗口恒为 ±INF）；无重复局面检测（长对局靠步数上限截断为平局，与 GUI 限步一致）。
9. 开局库已完成（基于自对弈统计、按规则独立、16 对称规范化压缩，见第 15 节）；残局库未做。
10. 根节点随机选择已完成（精确打分 + 分差阈值 + softmax 加权，种子可复现）；默认分差 60。
11. Lazy SMP 多线程已完成（`threads` 参数，实测 4 线程 NPS 约 3 倍、同时间预算多完成一层深度）；Root Split 未做。
12. 观察到的强度问题（待调参验证）：pv=16 下先手优势明显（深度 5~7 自对弈先手胜多）；深度 ≤7 的对局大量以步数上限平局收场，引擎偏保守，残局转化偏慢。
13. Aspiration 窗口已完成（渐进放宽：300 → 900 → 全窗口；实测固定深度省约 0.3% 节点、走法/估值与关闭时一致；本游戏估值跨层跳变大，收益有限但仍保留）。
14. 重复局面惩罚已完成（搜索内 2/4/6/8 层重复 → 当前轮次 ±penalty，统计见 `repetitionHits`；实测机制在搜索中频繁触发，但长漂移型平局（周期 > 搜索深度）不受影响）。
15. 静默搜索已完成（深度 0 展开"形成三连/提子"链，stand-pat + 战术走法；实测修正地平线高估：-914 → -1094；默认开启；与多线程兼容）。
16. 新增 SearchOptions：`aspirationWindows / repetitionPenalty / quiescenceSearch`；console `search` 增加 `a / rep / q` 参数。

## 13. Console 新增命令（AI 调试）
- `search [d] [t] [h] [r] [g] [th] [s] [pv] [a] [rep] [q]`：单局面搜索，打印最佳走法、估值、深度、耗时、节点、NPS、TT 统计、重复命中、根走法分数（随机模式下为精确候选集）。
- `match [n] [d] [h] [r] [th] [s] [mp]`：同配置自对弈 n 局，报告胜负/步数/节点/NPS。
- `vs [n] [d1] [h1] [pv1] [d2] [h2] [pv2] [s] [a] [rep]`：不同配置引擎对抗（强度 A/B，双方均开随机）。
- 参数：`h` 哈希模式（0 普通 / 1 仅开局规范化 / 2 全部规范化）；`r` 随机（0 关闭 / >0 开启）；`g` 随机分差；`th` 线程数（Lazy SMP）；`s` 随机种子（0 = 每次随机）；`pv` 点位价值权重（0 关闭）；`a` Aspiration 窗口；`rep` 重复惩罚（0 关闭）；`q` 静默搜索。

## 14. 快速索引

| 目标 | 文件 | 关键函数 |
|---|---|---|
| 规则参数 | ninechess.cpp:174 | `rules[]` |
| 局面数据/哈希 | ninechess_common.h | `ChessData::getHashLite/Hard` |
| 状态机 | ninechess.cpp | `doPlace/doCapture/tryFinishAfterPlaceOrMove/tryFinishAfterCapture` |
| 命令解析 | ninechess.cpp | `command/parse*` |
| 局面变换 | ninechess.cpp | `transformState/transformPos/transformMillKey` |
| AI 搜索 | ninechess_ai_ab.cpp | `alphaBetaPruning/searchRoot/search/quiescence` |
| AI 置换表 | ninechess_ai_ab.cpp | `probeTransposition/storeTransposition/pruneTranspositionStore` |
| AI 对称哈希 | ninechess_symmetry.h/.cpp | `NineChessSymmetry::canonicalHash/viewHash/build`（AI 与开局库共用） |
| AI 估值 | ninechess_ai_ab.cpp | `evaluate/evaluateTerminal` |
| AI 走法排序 | ninechess_ai_ab.cpp | `orderMoves/orderKey/scorePlaceOrShiftMove/scoreCaptureMove` |
| 开局库 | ninechess_book.h/.cpp | `recordGame/probe/save/load/toActualMove` |
| Console 测试 | ninechessconsole.cpp | `main/tryHandleRuleCommand` |
| 白盒测试 | tests/rule_harness.cpp | `runRule0..3/setupMidgame/setupOpeningCapture` |
| 对局电池 | tests/Run-MatchBattery.ps1 | 批量 vs 对抗（书开关/深度差/pv） |
## 15. 开局库（`ninechess_book.*`）

- **数据来源**：`booktrain <局数> <深度> [种子]` 自对弈（无书、开随机），对前 `bookDepth`（默认 10）层每个局面按"轮到方"视角累加胜负分（胜 +1 / 和 0 / 负 -1）；可多次运行累积样本，即"多轮对局的统计结果做评分调整"。
- **按规则独立**：每规则一个书文件 `books/book_<规则号>.dat`，规则切换自动加载对应书；跨规则文件加载被拒绝（评分不可通用）。
- **对称压缩**：局面 key = 16 等价视角的最小规范化哈希；库内走法以规范化坐标保存，查询时用查询局面的规范化视角逆变换还原（`toActualMove`），镜像/旋转等价开局共享同一份统计。
- **使用**：`match` / `vs` 对局中，书内走法优先执行（胜率加权随机，仅在与最优胜率差 15% 以内且样本 ≥ 10 的走法里选），被模型拒绝时回退 AI 搜索；`bookon` / `bookoff` 开关，`bookstat` 查看条目与胜率。
- **实测（规则 2，500 局训练）**：空盘条目 530 样本，首着 `(2,0)` 胜率 81%；深度差 5v7 对抗 60 局，弱引擎用书后胜率 13 → 18（+38%），书内走法 180 次零拒绝。
- **观察**：当前评估下先手优势明显（深度 5~7 自对弈后手几乎不赢），书会放大引擎自身偏好；深度 ≤7 平局率仍高（~70%），评估"进取性"与颜色偏差是后续调参重点（用 `tests/Run-MatchBattery.ps1` 批量验证）。

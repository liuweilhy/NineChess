# NineChess 代码库导览（AI_SUMMARY）

> 用途：为后续 AI 算法改进提供快速、准确的代码库认知，面向 AI Agent。
> 范围：纯模型（ninechess_common / ninechess）、NineChessConsole、tests、AI 引擎（ninechess_ai_ab）。
> Qt 界面细节从略，只保留与构建、AI 集成相关的接口约定。
> 分工：本文是**总则**（代码库导览、通用约定）；各算法的详细说明与参数手册独立成文——
> Alpha-Beta 引擎见 `AI_ALPHABETA.md`。

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
对局配置命令 `r<规则>s<限步>t<限时分钟>`（如 `r2s100t10`，段可省略、顺序不限，0=不限）：棋谱首行用它记录规则与限时限步，由 `parseSetupCommand()`（`ninechess.h`，核心层）解析、控制层与 Console 识别应用；模型不感知限时限步。棋谱因此保持纯命令流；超时判负保存时补写负方 `-0`/`-1`。

## 7. 架构与文件地图

```
D:\My program\QT\NineChess\
├── ninechess.sln                 # VS 解决方案（GUI + Console + RuleHarness）
├── AGENTS.md                     # 工作区指令契约（边界、命名、编码），改动前必读
├── AI_SUMMARY.md                 # 本文档（各算法总则）
├── AI_ALPHABETA.md               # Alpha-Beta 引擎详细说明与参数手册（自本文 §8/§12/§13 迁入）
├── NineChess\src\                # GUI 工程源码
│   ├── ninechess_common.h        # 常量、Rule、ChessData、status 位布局、MillKey、哈希
│   ├── ninechess.h/.cpp          # 纯模型：规则、状态机、命令解析、变换、胜负判定
│   ├── ninechess_ai_ab.h/.cpp    # Alpha-Beta AI（位棋盘版）← 优化主战场，详见 AI_ALPHABETA.md
│   ├── gamecontroller.*          # 控制层（赛制/计时/AI 调度/音效/动画）——含 Qt
│   ├── aithread.*                # AI 线程包装——含 Qt
│   └── （其余为界面文件，本摘要从略）
├── NineChessConsole\             # 命令行测试程序（模型 + AI，无 Qt）
├── tests\                        # RuleHarness + PowerShell 回归
└── Readme.md / History.txt / Licence.txt
```

依赖方向：模型（无 Qt）← 控制层 ← 视图；AI 通过 `friend class` 访问模型的 protected 成员（`m_data / m_selectedPos / m_rule / m_moveMask / m_lineMasks / m_validBoardMask / m_posLineIds / m_posLineCount` 等）。

## 8. AI 引擎（`ninechess_ai_ab.*`）

位棋盘 Alpha-Beta + 迭代加深引擎（搜索/置换表/对称归一化/增量哈希/估值/走法排序/
多线程的完整逻辑说明与全部参数手册，含固化常量、哨兵语义警示与 A/B 方法学）：

**→ 见 `AI_ALPHABETA.md`**（原本文 §8 的详细内容已全部迁入）。

只需记住的最小集合：`setOptions(SearchOptions)` + `alphaBetaPruning(depth)`，
外部 `quit()` 可中断（依赖方向与 friend class 访问见第 7 节）。

## 9. NineChessConsole（`NineChessConsole\ninechessconsole.cpp`）

- 启动：`NineChessConsole.exe [规则号]` 或 `--rule N`；默认规则 2。
- 走子命令同第 6 节；附加命令：`board / history / undo / new / rules / rule N / r<s<t<（对局配置，仅记录不计时）/ help / quit`。
- 维护 undo 栈；每次成功命令后打印棋盘（`getConsoleText`，含阶段/手牌/在盘/待提/选中/提示/历史三连）。
- **已链接 AI**（`ninechess_ai_ab.cpp` 加入工程，无 Qt 依赖），并提供 `search / match / vs / tune / selfcheck / booktrain` 等 AI 命令（参数与命令默认值详见 `AI_ALPHABETA.md` §4.2）。

## 10. 测试体系

- 白盒：`tests\rule_harness.cpp`（`RuleHarness.exe <0..3>`），直接构造 ChessData 中间局面（`setupMidgame / setupOpeningCapture`），覆盖禁点复用、双三连提子数、编号三连历史、被闷续走、飞子、堵死判负、非法命令不污染局面/历史。
- 黑盒：`Run-ConsoleBlackBoxTests.ps1` 回放命令行、检查 Console 输出。
- 总入口：`powershell -ExecutionPolicy Bypass -File ".\tests\Run-All-RegressionTests.ps1"`。
- AI 改动不影响模型测试；改动后仍应跑通回归，并另加 AI 冒烟/自对弈验证。

## 11. 构建与编码约定

- GUI 需 Qt 5.15.2（msvc2019_64）+ MSVC 工具集 v143（VS2022，2026-09-14 起，此前 v142）；`ninechess.pro` 与 `.vcxproj` 需同步（源文件列表、`/utf-8`）。
- 所有文本文件 UTF-8 无 BOM + CRLF；`.editorconfig` / `.gitattributes` / `AGENTS.md` 是契约。
- 命名：用 `ninechess_common.h` 的新字段名，不恢复旧别名。
- 新增源文件需同步更新：GUI 的 `.vcxproj` + `.filters` + `.pro`，Console 的 `.vcxproj`（+ `.filters`）。

## 12. AI 已知弱点与优化入口

已整体迁入 `AI_ALPHABETA.md` §6（未完成方向按预期收益排序，已完成特性保留一览清单）。

**2026-09-14 决策**：开局库与残局库均未完全实现，但当前默认配置下算法强度已经足够
（对 2018版/5月版均取得决定性优势），两者**暂时搁置不启用**，详见 §15 与 `AI_ALPHABETA.md` §6。

## 13. Console 新增命令（AI 调试）

已整体迁入 `AI_ALPHABETA.md` §4.2（`search / match / vs / tune / selfcheck / booktrain`
的参数表、命令默认值与引擎默认的差异警示）。

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

> **当前状态（2026-09-14）**：开局库与残局库均**未完全实现**——开局库只有统计式原型
> （Console `booktrain`/`bookon` 可用，GUI 未接线），残局库尚无实现。在当前默认配置下
> 搜索算法强度已经足够（对 2018版/5月版取得决定性优势，见 `benchmark/results/`），
> 因此两者**暂时搁置不启用**；残局库（3v3 WDL）与评估调参仍是长期方向
> （`AI_ALPHABETA.md` §6）。以下为开局库原型的设计说明，供将来续作参考。

- **数据来源**：`booktrain <局数> <深度> [种子]` 自对弈（无书、开随机），对前 `bookDepth`（默认 10）层每个局面按"轮到方"视角累加胜负分（胜 +1 / 和 0 / 负 -1）；可多次运行累积样本，即"多轮对局的统计结果做评分调整"。
- **按规则独立**：每规则一个书文件 `books/book_<规则号>.dat`，规则切换自动加载对应书；跨规则文件加载被拒绝（评分不可通用）。
- **对称压缩**：局面 key = 16 等价视角的最小规范化哈希；库内走法以规范化坐标保存，查询时用查询局面的规范化视角逆变换还原（`toActualMove`），镜像/旋转等价开局共享同一份统计。
- **使用**：`match` / `vs` 对局中，书内走法优先执行（胜率加权随机，仅在与最优胜率差 15% 以内且样本 ≥ 10 的走法里选），被模型拒绝时回退 AI 搜索；`bookon` / `bookoff` 开关，`bookstat` 查看条目与胜率。
- **实测（规则 2，500 局训练）**：空盘条目 530 样本，首着 `(2,0)` 胜率 81%；深度差 5v7 对抗 60 局，弱引擎用书后胜率 13 → 18（+38%），书内走法 180 次零拒绝。
- **观察**：当前评估下先手优势明显（深度 5~7 自对弈后手几乎不赢），书会放大引擎自身偏好；深度 ≤7 平局率仍高（~70%），评估"进取性"与颜色偏差是后续调参重点（用 `tests/Run-MatchBattery.ps1` 批量验证）。

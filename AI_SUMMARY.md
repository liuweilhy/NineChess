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
对局配置命令 `r<规则>s<限步>t<限时分钟>`（如 `r2s100t10`，段可省略、顺序不限，0=不限）：棋谱首行用它记录规则与限时限步，由 `parseSetupCommand()`（`ninechess.h`，核心层）解析、控制层与 Console 识别应用；模型不感知限时限步。棋谱因此保持纯命令流；超时判负保存时补写负方 `-0`/`-1`。

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
- 走法三类：`MOVE_PLACE / MOVE_SHIFT / MOVE_CAPTURE`；apply/undo 用逐字段快照（status、三块位棋盘、编号层、`millHistory` 只记长度回滚）恢复，编号规则不再每节点两次堆分配。
- PVS：非首走法先以零宽窗口试探，落回窗口内才全窗口重搜（实测开局节点 -28%，中局收益小）。
- 强制提子延伸（`forcedExtension`，默认开）：走子成三后走子方被迫提子，该强制节点深度不减 1；只在浅层（depth ≤ 2）延伸避免节点膨胀（深层地平线由静默搜索兜底）。实测中局固定深度 10 节点 +40%，换取战术链在主搜索内算清。
- LMR 迟到走法减搜（`lateMoveReductions`，默认开）：排序位次 i ≥ 4、深度 ≥ 3 的静默走法（排序键 < 2400，即不成三且无 TT/杀手/高历史加成）先减 1~2 层试探（减量随深度/位次递增，子节点至少剩 1 层），失败高恢复全深重搜。实测开局 d9 节点 -68%、中局 d12 节点 -64%，最佳走法不变。两项综合 vs 旧搜索行为：深度 7 二十四局 7:6 略优、无回归，且同深度耗时减半以上（等时间预算等效更深）。
- 重复检测覆盖根局面与真实对局历史：`positionHistory[0]` 每次迭代播种为根局面哈希（走法循环转回根局面可被常规 back=2/4/6/8 比较捕获）；`setChess` 时回放命令历史构建真实对局面哈希轨迹（`realTrail`，最多 10 条），浅层节点额外与轨迹比较（哈希含轮次/动作状态位，不会误匹配）。实测摇步局面重复命中 257 次且引擎主动避循环。
- `dynamicDepth`（默认开）：中局根局面在指定深度上追加 2 层，均衡开局/中局耗时（中局分支数约为开局 1/3，等深度节点数差百倍）。
- 根节点 `searchRoot` 与普通节点 `search` 分离；根节点记录最佳走法（根节点不做 LMR）。
- 搜索状态在 `SearchContext` 中（每线程一份工作局面 + 杀手/历史/统计 + 真实历史轨迹），类成员只保留只读共享数据（根局面、对称表、停止标志、时间预算），Lazy SMP 多线程安全。
- 置换表键每节点只计算一次，probe/store 共用（canonical 模式一次 16 视角哈希）。

### 8.2 置换表

- 按规则分 4 个全局 `TTStore`：**定长桶数组**（256 分片 × 2048 桶 × 2 槽 = 1M 条），分片互斥锁；无堆分配、无"满时全量扫描排序清理"（旧 `unordered_map` 方案已废弃，旧方案每次满表触发 O(n log n) 打嗝）。
- 槽位 16 字节：`key(8) + metaA(4) + metaB(4)`，每桶 2 槽恰好 32 字节对齐缓存行。metaA = value(16)|flag(2)|moveType(2)|from(5)|to(5)，metaB = depth(8)|generation(16)；from/to 存"值+1"与 `Move::from=-1` 语义区分（旧布局为 key + 逐字段条目，实际占 24 字节；压缩后同等内存容量翻倍，2026-09 改）。
- 键在 `makeSearchHash()` 出口统一过一次 `mix64` 雪崩——lite 哈希的位展开是稠密结构，未混合时低 18 位只取决于前几个棋盘点位的占用，直接做索引会让结构相近的局面挤进同一槽位（实测曾致节点数涨 3 倍，已修复）。
- 条目：value/depth/flag(EXACT|LOWER|UPPER)/generation + 最佳走法（用于排序置顶）；probe 时 `entry.depth >= depth` 才可用并刷新 generation（仅"深度可用"的命中才刷新，浅条目可自然老化）。
- 桶内替换策略：同键维持"深度大的覆盖，浅的只刷新代数"；跨键冲突淘汰"世代更老 > 深度更浅"者。

### 8.2.1 增量普通哈希（编号规则）

- `getHashHard()` 已拆成组件函数（`ninechess_common.h`）：`hardHashLayerTerm`（9 编号层项）/ `hardHashHistoryTerm`（历史三连项）/ `hardHashCombine`（含 lite 基哈希与表长的最终合成）。
- AI 在 `SearchContext` 里维护 `hardLayerAccum / hardHistoryAccum` 两个 XOR 累加器：`applyMove` 按层差量更新、`millHistory` 追加项并入；`undoMove` 从快照整体恢复；`board = m_root` 处调用 `resetHardHashAccum()` 全量重建。
- `makePlainHash` = 重算 lite（十几次位运算）+ 累加器合成，与 `getHashHard()` **逐位一致**（实测 5 个局面节点数/TT 统计完全相同），编号规则每节点省去 9 层 + 历史表全量重算。
- 校验兜底：常态每 2048 节点全量比对一次，`selfcheck` 命令逐节点比对（4 规则随机自对弈，失败即中止并报告）；比对失败说明差量维护有漏洞，会立即中止搜索而不是静默污染置换表。

### 8.3 对称归一化

- 预计算 16 个等价变换（镜像 × 内外翻转 × 4 个离散旋转，`buildSymmetryVariants()`），probe/store 时对 16 个视角各算一次哈希取**最小值**作 canonical key（`makeCanonicalHash()`）。
- 编号规则的 millHistory 按 `targetSource` 槽位重排映射（`mapMillKey()`）。
- selectedPos 在 `ACTION_PLACE` 状态下混入（`mixSelectedPos()`）。
- **成本**：`viewHash()` 已改为全程栈上计算（2026-09）——位棋盘映射是逐位单射变换（`mapBoard(a) & mapBoard(b) == mapBoard(a & b)`），层项/历史项直接用组件函数合成，不再构造带 `millHistory` 的临时 `ChessData`（旧实现编号规则每视角一次堆分配，canonical 模式每节点 16 次）。实测开局空盘 NPS 累计提升约 40%；与全量重算逐位一致（节点数不变）。

### 8.4 估值（`evaluate()`）

- 权重集中于 `EvalWeights` 结构 + 按规则静态表 `s_evalWeightsPerRule[RULE_COUNT]`（四行初始值相同，调参按行独立进行）；`SearchOptions.stalematePressureWeight >= 0` 可覆盖闷杀项（vs 的 `sp` 参数）。
- 开局：在盘×120 + 手牌差×48 + 三连×96 + 活二×24
- 中局：在盘×180 + 三连×112 + 活二×32 + 机动性×10
- 提子阶段：pendingCaptures ×（开局 160 / 中局 220），按轮次正负
- 胜利临近压力（`winPressureWeight`，默认 150；实测 60 局 A/B 8:2，`0` 关闭）：对手"在盘+手牌"总数 ≤ 判负线+2 时逐子加分
- 被闷杀压力（权重表 `stalematePressure`，默认 0）：中局对手机动性 ≤3 时每少一步加分（blockedIsLoss 规则收益最大）
- 双三连威胁（权重表 `forkThreat`，默认 0）：活二"成三点"去重计数，≥2 时每多 1 点加一份分；A/B 实测 ft=80 规则 2 上 0:4、规则 0 上 5:7——直棋威胁可被提子拆除，项偏负，保持 0（`vs` 的 `ft` 参数可继续实验）
- 封闭三连保护（权重表 `millSafety`，默认 0）：处于己方当前三连中的棋子数差（三连中的子按规则不可被提，编号规则下已登记的三连同样保护棋子）；`countMillsAndOpenMills` 单遍扫描顺带输出 inMill 掩码；A/B 方向一致偏正（深度 6 十二局 3:2、深度 7 二十四局 5:3，均引擎 2 用 ms=16 领先）但样本不足定值，权重保持 0（`vs` 的 `ms` 参数可继续实验）
- 步数上限急迫分（`SearchOptions.stepsRemainingHint`，默认 0 = 关）：限步赛制下由控制层传入剩余步数（模型层不感知），搜索内按 ply 递减；剩余 < 24 步时子力领先方（在盘+手牌，领先封顶 3 子）每子每步 +8，推动优势转化、缓解"磨到步数上限平局"。`match` 自动按 `mp` 传值，GUI 由 `GameController` 经 `AiThread::setStepsLimit()` 接线（`vs` 的 `sr` 参数可实验）
- 连续博弈阶段（`SearchOptions.taperedEval`，默认关）：开局/中局权重按"剩余手牌比例"线性插值（phaseT ∈ [0,256]），消除摆子结束时的估值跳变；关闭时按阶段硬切换、行为与旧版完全一致。A/B 两轮一致偏负（深度 6 十二局 2:4、深度 7 二十四局 4:6 落后），默认保持关（`vs` 的 `tp` 参数可继续实验）
- **tune 命令**：按规则坐标下降自动调参（每列若干候选、引擎2=候选 vs 引擎1=当前行、交替执先），结束与原始行做终局验证并打印可回贴的行；`th` 参数默认自动线程（14 线程下 24 局/候选约 40 分钟）。三轮战役（8/16/24 局每候选，终局验证 46%/53%/51%）均不显著——原始权重行守擂成功；深度 8 下对局约九成平局，单局信息量低，进一步的调参收益需要海量对局（SPSA 式）或更强的评估项而非更多扫描轮次。
- 热路径结构：可提三连/活二**单遍**线扫描（`countMillsAndOpenMills`），位棋盘/占用掩码每节点算一次共享；插桩实测中局评估占搜索时间约 22%、开局 6~7%，合并扫描后中局 NPS +10%
- 胜负分 ±30000（按 ply 衰减），边界 ±32000；`clampScore` 收敛
- 注意：**编号规则下三连计数不区分"可提/已提"**（millHistory 只进哈希不进估值）的问题已修复（可提三连剔除已登记项）

### 8.5 走法排序（`orderMoves` / 静态启发）

- 落子即成三×2400、活二×240、阻断对手威胁×180、经过线数×48、拆自己三连−160
- 提子：3000 + 线数×128 + 三连×64 + 参与线数×32
- 根节点把上一迭代最佳走法 +20000 置顶（注意：这也意味着并列时永远选上一迭代的走法，与"随机性"目标冲突，最终选择逻辑需剥离该偏置）
- `orderKey` 每走法只算一次：完整键（静态启发 + TT/杀手/历史加成，int32 足够）先写回 `Move.order` 再纯整数排序（2026-09 前 std::sort 每次比较重算两个 key；实测开局 NPS +6%）。`Move.order` 已从 int16 扩为 int32（生成期存静态分，排序期存完整键）
- 无 TT 走法、无杀手走法、无历史启发（优化点）

## 9. NineChessConsole（`NineChessConsole\ninechessconsole.cpp`）

- 启动：`NineChessConsole.exe [规则号]` 或 `--rule N`；默认规则 2。
- 走子命令同第 6 节；附加命令：`board / history / undo / new / rules / rule N / r<s<t<（对局配置，仅记录不计时）/ help / quit`。
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
8. 无 aspiration window（根窗口恒为 ±INF）→ 已完成（渐进放宽 300→900→全窗口；估值跨层跳变大，收益约 0.3%，保留）；无重复局面检测 → 已完成（搜索内 2/4/6/8 层重复惩罚）。
9. 开局库已完成（基于自对弈统计、按规则独立、16 对称规范化压缩，见第 15 节）；残局库未做。
10. 根节点随机选择已完成（精确打分 + 分差阈值 + softmax 加权，种子可复现）；默认分差 60。
11. Lazy SMP 多线程已完成（`threads` 参数）；默认线程数已改为按 CPU 核数自动推导（`defaultThreadCount()`：>=5 核留 2 核、<=4 核留 1 核、封顶 16），GUI 缺省即用。
12. ~~引擎偏保守 / 平局率高~~（部分缓解：新增 `winPressureWeight` 胜利临近压力分，对手总子数逼近判负线时逐子加分；实测 60 局对抗 8:2 压制关闭方，默认 150，`wp=0` 可关）。
13. Aspiration 窗口已完成（渐进放宽：300 → 900 → 全窗口；实测固定深度省约 0.3% 节点、走法/估值与关闭时一致；本游戏估值跨层跳变大，收益有限但仍保留）。
14. 重复局面惩罚已完成（搜索内 2/4/6/8 层重复 → 当前轮次 ±penalty，统计见 `repetitionHits`；2026-09 补齐两个盲区：`positionHistory[0]` 每迭代播种根局面哈希——此前"转回根局面"检测不到；`setChess` 回放命令历史构建真实对局轨迹 `realTrail`——此前只能看见搜索路径内的循环，跨搜索边界的互磨检测不到。摇步实测 257 次命中且引擎主动避循环）。
15. 静默搜索已完成（深度 0 展开"形成三连/提子"链，stand-pat + 战术走法；实测修正地平线高估：-914 → -1094；默认开启；与多线程兼容）。
16. 新增 SearchOptions：`dynamicDepth`（中局根局面 +2 层，默认开）、`winPressureWeight`（胜利临近压力分，默认 150）、`pvSearch`（PVS 零宽试探，默认开）、`forcedExtension`（强制提子延伸，默认开）、`lateMoveReductions`（LMR，默认开）、`taperedEval`（连续阶段插值，默认关）、`millSafetyWeight`/`forkThreatWeight`/`stalematePressureWeight`（评估项覆盖）、`stepsRemainingHint`（限步急迫提示，默认 0 = 关）。
17. 搜索快照/哈希开销已优化：逐字段快照 + `millHistory` 长度回滚（编号规则不再每节点两次堆分配，NPS +76%）、置换表键每节点一次（canonical 模式 NPS 再提升）；`viewHash` 栈上组件合成（编号规则 canonical 每节点 16 次堆分配归零，开局 NPS 累计 +40%）；`orderMoves` 先写回完整排序键再整数排序（每走法一次 orderKey）；TT 槽位 24→16 字节（每桶 2 槽恰 32 字节对齐缓存行，容量 512K→1M 条）；`setChess` 剥离搜索副本中的命令历史（旧实现每次迭代每线程深拷贝整局命令 string）。
18. 剩余优化方向：评估系数系统性调参（`tests/Run-MatchBattery.ps1`，`vs` 已支持奇偶局交替执先 + 单侧参数；2026-09 起新增 `ms`/`tp`/`sr`/`ext`/`lmr` 引擎 2 侧参数）；增量线计数评估（维护每线 own/occupied 计数，evaluate/排序/静默搜索共享，评估占中局搜索约 22% 的最大剩余项）；真实历史回放的 plain-hash 轨迹目前每手棋 O(n) 重放，长局可改为模型侧滚动维护；残局库（3v3 全量 WDL 约 8MB 可行）与 NNUE 式小网络为长期方向。

## 13. Console 新增命令（AI 调试）
- `search [d] [t] [h] [r] [g] [th] [s] [pv] [a] [rep] [q] [dd] [pvs] [ext] [lmr]`：单局面搜索，打印最佳走法、估值、深度、耗时、节点、NPS、TT 统计、重复命中、根走法分数（随机模式下为精确候选集）。
- `match [n] [d] [h] [r] [th] [s] [mp] [dd]`：同配置自对弈 n 局，报告胜负/步数/节点/NPS；`mp` 限步会自动传入评估急迫项（剩余步数随对局递减）。
- `vs [n] [d1] [h1] [pv1] [d2] [h2] [pv2] [s] [a] [rep] [wp] [sp] [ft] [th] [ext] [lmr] [ms] [tp] [sr]`：不同配置引擎对抗（强度 A/B）。**奇偶局交替执先**消除先手偏置；`wp`/`sp`/`ft`/`ext`/`lmr`/`ms`/`tp`/`sr` 只作用于引擎 2（胜利临近/闷杀/双三威胁/强制延伸/LMR/三连保护/连续阶段/步数提示），`ext`/`lmr`/`ms`/`tp` 取 -1 = 沿用默认值。
- `tune [n] [d] [s] [r]`：按当前规则坐标下降自动调参（每列候选、对抗选值、终局验证、打印可回贴的权重行）。
- `selfcheck [n] [d]`：4 规则随机自对弈并逐节点校验增量哈希。
- 参数：`h` 哈希模式（0 普通 / 1 仅开局规范化 / 2 全部规范化）；`r` 随机（0 关闭 / >0 开启）；`g` 随机分差；`th` 线程数（0 = 按 CPU 核数自动）；`s` 随机种子（0 = 每次随机）；`pv` 点位价值权重（0 关闭）；`a` Aspiration 窗口；`rep` 重复惩罚（0 关闭）；`q` 静默搜索；`dd` 动态深度（0 关闭，默认开）；`pvs` PVS 试探（默认开）；`ext` 强制提子延伸（默认开）；`lmr` LMR 减搜（默认开）。

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

# Alpha-Beta 引擎说明与参数手册（AI_ALPHABETA）

> 算法对象：`NineChess/src/ninechess_ai_ab.h/.cpp`（位棋盘 Alpha-Beta + 迭代加深）。
> 定位：`AI_SUMMARY.md` 是各算法的**总则**（代码库导览与通用约定）；本文是 Alpha-Beta 算法的
> **详细逻辑说明与参数手册**。原 `AI_SUMMARY.md` 中 §8（AI 引擎）、§12（AI 弱点与优化）、
> §13（Console AI 命令）的算法内容已迁入本文，后续演进在本文维护。
> 面向读者：后续 AI 算法设计者（人类或 Agent）。规则/坐标/状态机等模型知识见 `AI_SUMMARY.md`。

## 1. 算法概述

### 1.1 总体流程

```
setChess(根局面)
  ├─ 快照：对称变换表 build、点位价值表（穿线数）、真实历史轨迹 realTrail（≤10 条，回放命令历史）
  ├─ refreshWeights()：按规则取 s_evalWeightsPerRule 行，再用 SearchOptions 覆盖项修订
  └─ 剥离搜索副本中的命令历史（避免每次迭代每线程深拷贝整局命令串）
alphaBetaPruning(depth)
  ├─ dynamicDepth：中局根局面 depth + 2（写死，上限 kMaxPly-1）
  ├─ 限时模式（timeLimitMs > 0）：与纯深度模式同构——搜索到请求深度即止
  │   （含 dynamicDepth 追加），时间预算只作为超时中断的上限，中断保留
  │   最后一个完整迭代。曾试验“放开深度上限到 kMaxPly-1、用满预算”，
  │   2026-09-13 按需求回退（限时不再被占满，AI 提前出招）。
  └─ 迭代加深 1..depth：
       ├─ aspirationWindows：以上一迭代估值为中心收窄根窗口（Δ=300，失败 ×3 渐宽，再全窗）
       └─ searchRoot → search（TT probe / 重复惩罚 / 浅层剪枝 / 强制提子延伸 / LMR）
             └─ 深度 0 → quiescence（静默搜索）
  中断：外部 quit() 原子标志，随时可停；保留最后一次完整迭代的走法为 bestMove()
```

### 1.2 节点搜索（`search` / `searchRoot`）

- 根节点与普通节点分离：根节点记录最佳走法、不做 LMR、随机模式在此精确打分。
- 走法三类：`MOVE_PLACE / MOVE_SHIFT / MOVE_CAPTURE`；apply/undo 用逐字段快照恢复
  （status、三块位棋盘、编号层；`millHistory` 只记长度回滚），编号规则不再每节点两次堆分配。
- 置换表键每节点只计算一次，probe/store 共用；键在 `makeSearchHash()` 出口统一过一次 `mix64`
  雪崩（lite 哈希的稠密位展开未混合时低 18 位只取决于前几个点位，直接索引曾致节点数涨 3 倍）。
- `SearchContext` 每线程一份（工作局面 + 杀手/历史/统计 + 真实历史轨迹）；
  类成员只保留只读共享数据（根局面、对称表、停止标志、时间预算）加一份
  分片加锁的置换表（`m_tt`，实例私有、堆持有），多线程安全。

### 1.3 静默搜索（`quiescenceSearch`，默认开）

深度 0 时展开"形成三连/提子"链，stand-pat + 战术走法；上限 `kMaxQuiescenceDepth = 12`（写死）。
实测修正地平线高估（-914 → -1094）；与多线程兼容。
走法生成走 `generateTacticalMoves()`：直接只产出"能形成新三连"的落子/走子
（与旧实现“全量生成（含静态打分）再按成三过滤”逐条等价），跳过静态启发打分
（静默搜索不排序落子/走子，打分是纯浪费）；提子阶段的走法生成时已带静态分，
按分数降序尝试可更早触发静默截断（旧实现算了分却从不排序）。

### 1.4 搜索增强

- **浅层剪枝（futility，2026-09 新增，写死阈值无开关）**：
  - **逆 futility（RFC）**：depth ≤ 2 且非提子阶段时，先算静态估值，
    估值减裕度仍出窗口（maximizer：eval−margin ≥ beta；minimizer 对称）
    则直接按估值返回。裕度按单步估值最大跨层跳变定值：成三 112 + 提子
    待提 220 + 在盘 180 ≈ 500，depth 1/2 取 400/700。
  - **depth 1 futility**：排序键 < 2400 的安静走法（不成三、无 TT/杀手/
    高历史加成）在“静态分 ± 400 够不到 alpha/beta”时直接跳过。
  - **守卫**（避开本游戏的剪枝盲区）：提子阶段不剪；中局轮走方机动性 ≤2
    不做 RFC（困毙/被闷必须搜清）、对手机动性 ≤2 不剪安静走法（一手安静
    棋可能直接堵死对手）；摆满判负规则（打三棋）开局末期空位 < 5 不剪。
  - 全部走法都被剪时以“静态分 ± 400”乐观界兜底入库（UPPER/LOWER 语义
    成立），避免 ±INF 入库污染置换表。
  - 实测（固定深度 12、单线程、同局面）：中局节点 −46~50%、NPS +20~69%
    （叠加单遍估值与静默战术生成的提速），同深度耗时约减半。
- **PVS**（`pvSearch`，默认开）：非首走法先以零宽窗口试探，落回窗口内才全窗口重搜。
  实测开局节点 -28%，中局收益小。
- **强制提子延伸**（`forcedExtension`，默认开）：走子成三后走子方被迫提子，该强制节点深度
  不减 1；只在浅层（depth ≤ 2，写死）延伸，深层地平线由静默搜索兜底。实测中局固定深度 10
  节点 +40%，换取战术链在主搜索内算清。
- **LMR 迟到走法减搜**（`lateMoveReductions`，默认开）：排序位次 i ≥ 4、深度 ≥ 3 的静默走法
  （排序键 < 2400，即不成三且无 TT/杀手/高历史加成）先减 1~2 层试探
  （减量 = 1 + (depth≥6) + (i≥12)，子节点至少剩 1 层），失败高恢复全深重搜。
  实测开局 d9 节点 -68%、中局 d12 节点 -64%，最佳走法不变。两项综合 vs 旧搜索行为：
  深度 7 二十四局 7:6 略优、无回归，同深度耗时减半以上（等时间预算等效更深）。
- **重复惩罚**（`repetitionPenalty`，默认 40）：搜索内 2/4/6/8 层重复局面按当前轮次 ±罚分，
  只在前 `kRepetitionMaxPly = 10` 层生效（写死）。两个检测盲区已补齐：
  `positionHistory[0]` 每次迭代播种根局面哈希（"走法循环转回根局面"可被常规 back=2/4/6/8
  比较捕获）；`setChess` 时回放命令历史构建真实对局面哈希轨迹 `realTrail`（最多 10 条，
  浅层节点额外比较；哈希含轮次/动作状态位，不会误匹配）。摇步实测 257 次命中且引擎主动避循环。

### 1.5 置换表（`TTStore`）

- 每个 AI 实例一张私有表（2026-09 起改为实例持有，原"按规则分 4 个全局实例"的
  跨实例共享已移除：先手/后手各持一个 AI 实例即按先后手天然分表互不共享，
  A/B 两臂 / 自对弈双方不再经置换表互通知识，两侧评估配置不同也不会串味；
  同一实例一次搜索内的全部 Lazy SMP 线程仍共用这一份）：
  **定长桶数组**（256 分片 × 2048 桶 × 2 槽 = **1M 条**），分片互斥锁；
  表体一次性堆分配（`unique_ptr`，16MB 不内联进对象以免栈上实例爆栈），
  节点级无堆分配、无满表全量扫描清理（旧 `unordered_map` 方案的 O(n log n) 打嗝已废弃）。
- 槽位 16 字节：`key(8) + metaA(4) + metaB(4)`，每桶 2 槽恰好 32 字节对齐缓存行。
  metaA = value(16) | flag(2) | moveType(2) | from(5) | to(5)，metaB = depth(8) | generation(16)；
  from/to 存"值+1"与 `Move::from = -1` 语义区分（2026-09 由 24 字节逐字段布局压缩而来，
  同等内存容量翻倍）。
- 条目：value / depth / flag(EXACT|LOWER|UPPER) / generation + 最佳走法（用于排序置顶）；
  probe 时 `entry.depth >= depth` 才可用并刷新 generation（仅"深度可用"的命中才刷新，
  浅条目自然老化）。
- **胜负分 ply 换算（2026-09 修复）**：终端值 `WIN_SCORE-ply` 编码的是"距搜索根
  的绝对步数"，直接入库会让不同 ply 命中同一局面时读出错误的取胜/失败距离
  （恒偏乐观 ply 差，扰乱引擎在多杀着线间的取舍）。入库时换算为"距本节点的
  相对距离"（+ply），命中时再换算回绝对距离（-ply）。
- 桶内替换：同键"深度大的覆盖，浅的只刷新代数"；跨键冲突淘汰"世代更老 > 深度更浅"者。

### 1.6 增量普通哈希（编号规则）

- `getHashHard()` 已拆成组件函数（`ninechess_common.h`）：`hardHashLayerTerm`（9 编号层项）/
  `hardHashHistoryTerm`（历史三连项）/ `hardHashCombine`（最终合成）。
- AI 在 `SearchContext` 维护 `hardLayerAccum / hardHistoryAccum` 两个 XOR 累加器：
  `applyMove` 按层差量更新、`millHistory` 追加项并入；`undoMove` 从快照整体恢复；
  `board = m_root` 处 `resetHardHashAccum()` 全量重建。
- `makePlainHash` = 重算 lite + 累加器合成，与 `getHashHard()` **逐位一致**（实测节点数/TT
  统计完全相同），编号规则每节点省去 9 层 + 历史表全量重算。
- 校验兜底：常态每 2048 节点全量比对一次；`selfcheck` 命令逐节点比对（4 规则随机自对弈）。
  比对失败立即中止搜索而不是静默污染置换表。
- **历史三连 O(1) 查表**（2026-09 新增）：`SearchContext` 维护 MillKey 全空间
  （16 bit）的出现计数表 `millKeySeen`（64KB/线程），applyMove 对追加项 ++、
  undoMove 对回退项 --、局面整体重置时全量重建；估值/排序热路径的
  `isMillKeyInHistory / isHypotheticalMillInHistory` 用查表替代
  `NineChess::hasMillKey` 的线性扫描（九连棋中局历史三连可达数十条，
  每叶子每成三线扫一遍是可观的常数）。模型侧 `addNewMills` 仍在成三时
  线性查重（频率低），未改动。

### 1.7 对称归一化（`NineChessSymmetry`，与开局库共用）

- 预计算 16 个等价变换（镜像 × 内外翻转 × 4 个离散旋转）；probe/store 时取 16 视角哈希的
  **最小值**作 canonical key（`makeCanonicalHash()`）。编号规则的 millHistory 按 `targetSource`
  槽位重排映射（`mapMillKey()`）；selectedPos 在 `ACTION_PLACE` 状态混入（`mixSelectedPos()`）。
- 三档哈希模式（`hashMode`）：`Plain`（无规范化，中局 NPS 最优）/ `OpeningCanonical`
  （默认，仅开局节点 canonical，根节点按对称类分组）/ `FullCanonical`（旧行为）。
  实测：开局 canonical 比不规范化快 2.3 倍；中局全对称每节点慢约 4 倍，故默认只规范化开局。
- **成本**：`viewHash()` 全程栈上计算——位棋盘映射是逐位单射变换
  （`mapBoard(a) & mapBoard(b) == mapBoard(a & b)`），层项/历史项直接用组件函数合成，
  不再构造带 `millHistory` 的临时 `ChessData`（旧实现编号规则每视角一次堆分配，canonical
  模式每节点 16 次）。实测开局空盘 NPS 累计提升约 40%；与全量重算逐位一致。

### 1.8 估值（`evaluate()`）

权重集中于 `EvalWeights` 结构 + 按规则静态表 `s_evalWeightsPerRule[RULE_COUNT]`
（四行初始值相同，调参按行独立进行）。热路径：可提三连/活二**单遍线扫描同时算齐
双方**（`countMillsBothSides`，旧实现每方各扫一遍全部连线），位棋盘/占用掩码每节点
算一次共享；插桩实测评估占搜索时间中局约 22%、开局 6~7%。胜负分 ±30000（按 ply
衰减），边界 ±32000，`clampScore` 收敛。注意：编号规则下可提三连已剔除 millHistory
中已登记项（估值与排序同步生效）。`evaluate()` 另有带机动性输出的重载，供浅层
剪枝的困毙/被闷守卫使用（仅中局阶段非零）。

各项公式（权重取值见 §2.4/§2.5）：

- 开局：在盘差×120 + 手牌差×48 + 可提三连差×96 + 活二差×24
- 中局：在盘差×180 + 可提三连差×112 + 活二差×32 + 机动性差×10
- 提子阶段：pendingCaptures ×（开局 160 / 中局 220，按轮次正负）
- 胜利临近压力（`winPressureWeight`，默认 150）：对手"在盘+手牌"总数 ≤ 判负线+2 时
  逐子加分。自对弈 60 局 8:2 压制关闭方（证明自洽）；**对外** 80 局/臂对照 wp=150 vs wp=0
  仅差 3 胜，小于同配置重跑噪声 ±5 胜（2026-09-12 重测，见 `benchmark/results/evalab_*/CORRECTION.md`）
  ——对外强度效应未证实。
- 点位结构价值（`pointValueWeight`，默认 16）：双方占据点位的穿线数总和之差 × 权重。
  线表实测（EvalProbe）：直线规则（0/2/3）每点恒 2 条线、**该项差值恒 0（失效）**；
  仅斜线规则（1 打三棋）生效——角点 3 线 > 边中点 2 线，引导占角。对外 A/B 同样在噪声内。
- 被闷杀压力（权重表 `stalematePressure`，默认 0）：中局对手机动性 ≤3 时每少一步加分
  （blockedIsLoss 规则收益最大）。
- 双三连威胁（权重表 `forkThreat`，默认 0）：活二"成三点"去重计数，≥2 时每多 1 点加一份。
  A/B 实测 ft=80 偏负（规则 2 上 0:4、规则 0 上 5:7）——直棋威胁可被提子拆除，保持 0。
- 封闭三连保护（权重表 `millSafety`，默认 0）：处于己方当前三连中的棋子数差（三连中的子
  按规则不可被提，编号规则下已登记三连同样保护）。A/B 方向一致偏正（深度 6 十二局 3:2、
  深度 7 二十四局 5:3）但样本不足定值，保持 0。
- 步数上限急迫分（`stepsRemainingHint`，默认 0 = 关）：限步赛制由控制层传入剩余步数
  （模型层不感知限步），搜索内按 ply 递减；剩余 < 24 步时子力领先方（领先封顶 3 子，
  写死）每子每步 +8，推动优势转化。
- 连续博弈阶段（`taperedEval`，默认关）：开/中局权重按"剩余手牌比例"线性插值
  （phaseT ∈ [0,256]），消除摆子结束时的估值跳变；关闭时按阶段硬切换。A/B 两轮一致偏负
  （深度 6 十二局 2:4、深度 7 二十四局 4:6），保持关。
- **tune 命令**：按规则坐标下降自动调参（每列若干候选、引擎 2 = 候选 vs 引擎 1 = 当前行、
  交替执先），结束与原始行做终局验证并打印可回贴的权重行。三轮战役（8/16/24 局每候选，
  终局验证 46%/53%/51%）均不显著——原始权重行守擂成功；深度 8 下对局约九成平局，
  单局信息量低，进一步调参收益需要海量对局（SPSA 式）或更强的评估项。

#### 1.8.1 逐项计分标准表（先手视角，默认权重，全部经 EvalProbe 实测核对）

计分方向与阶段约定：

- 评估值是**先手（PLAYER1）视角**：正分利好先手、负分利好后手；搜索层按轮走方换号
  （`maximizing = turn == PLAYER1`）。因此下表"每单位 ±N"的读法是：先手多一个单位 +N，
  后手多一个单位 −N（差值项，先手计数 − 后手计数）。
- 阶段权重默认**硬切换**：未开局/摆子阶段用"开局"列，走子阶段用"中局"列；
  `taperedEval` 开启时按剩余手牌比例在两列间线性插值（phaseT ∈ [0,256]）。
- 表中"±N"表示该项作用在双方差值上；标"不参与"的项在该阶段恒为 0。

| # | 计分项 | 每单位计分 | 开局 | 中局 | 口径与开关 |
|---|---|---|---|---|---|
| 1 | 在盘子差 | 每净多 1 颗在盘棋子 | ±120 | ±180 | blend 参与项 |
| 2 | 手牌差 | 每净多 1 颗未落子 | ±48 | 不参与 | 恒用开局权重（无中局列、不插值） |
| 3 | 可提三连差 | 每个站立且可再提的三连 | ±96 | ±112 | 可提 = 站立三连且（规则允许重复提子 或 该键未登记历史）。**规则 2（九连棋）三连成形瞬间即登记历史，站立三连永远不再计入 → 该项结构性恒 0**（55,060 个评估状态实测 0 命中）；规则 0/1/3 正常生效 |
| 4 | 活二差 | 每条"己方 2 子 + 线内唯一空位" | ±24 | ±32 | 按线计数（共点双活二各计 1 条）；威胁点去重只用于第 10 项 |
| 5 | 机动性差 | 每步可用走法 | 不参与 | ±10 | 仅中局；飞子方 = 在盘子数 × 空位数；提子待提时轮走方 = 待提子数 |
| 6 | 提子待提 | 轮走方每待提 1 子 | ±160 | ±220 | 记在轮走方（必为提子方）；多提规则（1/2）一步可欠多子；blend 参与项 |
| 7 | 点位结构价值 | 每点价值 = 该点穿线数，总和差 ×16 | 两阶段同权 | 同左 | 直线规则（0/2/3）每点恒 2 线 → 差恒 0 失效；仅打三棋生效（角点 3 线 > 边中点 2 线）；`pointValueWeight` = 0 关 |
| 8 | 胜利临近压力 | 对手总数（在盘+手牌）≤ 判负线+2 时每逼近 1 子 | ±150 | ±150 | `winPressureWeight` = 0 关；阈值 = `minPiecesToSurvive`+2 = 5；单个对手最多贡献 (5−3+1)×150 = 450 |
| 9 | 被闷杀压力 | 中局对手机动性 ≤3 时每少 1 步 | 不参与 | ±(4−机动性)×权重 | 权重表 `stalematePressure`，默认 0 休眠；双方对称（己方低机动对称扣分） |
| 10 | 双三连威胁 | 互不相同成三点 ≥2 时每多 1 点 | 两阶段同权 | 同左 | 权重表 `forkThreat`，默认 0 休眠；A/B 实测 ft=80 偏负 |
| 11 | 三连保护 | 处于己方三连中的棋子数差每子 | 两阶段同权 | 同左 | 权重表 `millSafety`，默认 0 休眠 |
| 12 | 步数急迫分 | 子力领先方每子每步 | +8 | +8 | `stepsRemainingHint` > 0（限步赛制控制层传入）才启用；剩余 <24 步；领先封顶 ±3 子；最大 ≈ 3×23×8 = 552 |
| 13 | 终局 | 胜 +(30000−ply)、负 −(30000−ply)、和 0 | — | — | 评估出口 `clampScore` 到 ±(30000−ply)：估值永不掩盖杀分 |

算例（EvalProbe 场景 S2，规则 0 摆子阶段：先手 (0,7)(0,0)(0,1) 成三待提 1 子，
后手 (1,3)(2,4)）：在盘差 +1×120 + 手牌差 −1×48 + 可提三连 +1×96 + 提子待提 +1×160
+ 点位价值 (6−4)×16 = 32 → **+360**。

与规则的耦合：权重表四行当前同值，规则差异由结构参数进入评估——打三棋 20 条线
（斜线扩大三连/活二扫描集、点位价值生效）、九连棋可提三连口径（历史登记，见第 3 项）、
莫里斯飞子改变机动性量纲（第 5 项）、判负线影响压力阈值（第 8 项）。
核对工具：`benchmark/evalprobe.cpp` → `bin\EvalProbe.exe`（独立按本表重算，
与引擎 depth-0 静态评估在随机对局全程逐手比对，四组选项配置 × 15.5 万状态一致）。

### 1.9 走法排序（`orderMoves` / 静态启发）

- 落子/走子静态分（`scorePlaceOrShiftMove`）：一趟扫描 toPos 穿过的线（≤3 条）
  同时算齐四项——落子即成三 ×2400、活二 ×240、阻断对手威胁 ×180、经过线数 ×48
  （掩码类只算一次共享，数值与旧版"四个独立 helper 各自扫线"完全一致），
  外加拆自己三连 −160。
- 提子：3000 + 线数×128 + 三连×64 + 参与线数×32。
- 加成置顶：TT 最佳走法、每层 2 个杀手走法、历史启发表（`history[from][to] += depth²`）。
  根节点不套用 TT/杀手/历史加成，也不把上一迭代最佳走法置顶（2026-09-13 移除，
  原因见下）；每轮迭代的最佳走法由 `searchRoot` 搜索结果本身给出。
- `orderKey` 每走法只算一次：完整键（静态启发 + TT/杀手/历史加成，int32）先写回
  `Move.order`（已从 int16 扩为 int32）再纯整数排序（此前 std::sort 每次比较重算两个 key，
  实测开局 NPS +6%）。
- 已解决：根节点“上一迭代最佳走法 +20000 置顶”意味着分差内并列时永远选上一迭代
  的走法，与随机性目标冲突，2026-09-13 已移除（见 §2.3 备注 1）。

### 1.10 多线程（Lazy SMP）

N 线程各自独立跑同一套迭代加深，共享本实例的分片置换表（实例之间按先后手分表，
互不共享）。实测 NPS 扩展性：4 线程约 3.7 倍、
8 线程约 6.3 倍、16 线程约 8.8 倍。默认线程数由 `defaultThreadCount()` 推导
（≥5 核留 2 核、≤4 核留 1 核、封顶 16，查询失败保守 4）。
**对局实测**：每步 0.1 秒级完成的搜索，8 线程对 1 线程的强度收益基本不体现
（`benchmark/results/may_vs_current/MAY_REPORT.md` 分析 2）；短时限/浅深度场景线程宜少。

## 2. 参数手册

**作用域约定**：
- **每次搜索**：`setOptions()` 后对下一次 `alphaBetaPruning()` 生效；
- **每个根局面**：`setChess()` 时快照（如按根局面命令数判断 randomPlies）；
- **对局级**：由控制层（GUI `AiThread` / Console / benchmark 驱动）传入。

**取值范围**指引擎层接受范围；Console 命令的参数默认值可能与引擎默认不同，见 §4.2。

### 2.1 搜索主参数

| 参数 | 类型 | 默认值 | 取值范围 | 作用域 | 作用 | 备注 |
|---|---|---|---|---|---|---|
| `depth`（`alphaBetaPruning` 入参） | int | Console 8 / GUI 默认 10（`AiThread` 构造值） | 1~63（`kMaxPly=64`，入参钳到 `kMaxPly-1`） | 每次搜索 | 迭代加深的目标深度 | `dynamicDepth` 开时中局再 +2；搜索到请求深度即止，**限时模式下 depth 同样是终点而非保底（timeLimitMs 只是超时中断上限，见 §1.1；2026-09-13 回退“用满预算放开深度”的实验）** |
| `timeLimitMs` | int64 | 0 | 0 = 不限时；>0 = 预算 | 每次搜索 | 超时中断上限；中断保留上层完整结果 | GUI 恒用（秒×1000，`AiThread`）；Console `search` 默认 0。限时不再被占满：到请求深度即提前出招 |
| `threads` | uint32 | `defaultThreadCount()`（自动） | 1~16（建议）；Console `th=0` 表自动 | 每次搜索 | Lazy SMP 并行，共享分片 TT | 0.1s 级搜索多线程收益基本不体现，短时限宜 1~4 |
| `hashMode` | HashMode 枚举 | `OpeningCanonical`(1) | 0=Plain / 1=仅开局 canonical / 2=全 canonical | 每次搜索 | 对称等价局面共享 TT 条目 | 2 是旧行为（中局每节点慢约 4 倍）；1 为默认 |

### 2.2 搜索增强开关

bool 开关均为"量值写死、只能开/关"，内部阈值见备注（详见 §2.6/§1.4）。

| 参数 | 类型 | 默认值 | 取值范围 | 作用域 | 作用 | 备注 |
|---|---|---|---|---|---|---|
| `aspirationWindows` | bool | true | on/off | 每次搜索 | 根窗口收窄，失败渐进放宽 | 内部写死：depth≥3 才启用、Δ=300、失败 ×3；实测省约 0.3% 节点 |
| `pvSearch` | bool | true | on/off | 每次搜索 | PVS 零宽试探，失败高才重搜 | 开局节点 -28%；**Console `search` 命令默认 0（关），与引擎默认不一致** |
| `quiescenceSearch` | bool | true | on/off | 每次搜索 | 深度 0 静默扩展成三/提子链 | 上限 12 层写死；**Console `search` 命令默认 0（关），与引擎默认不一致** |
| `forcedExtension` | bool | true | on/off（`vs` 的 `ext`：-1=沿用默认） | 每次搜索 | 强制提子节点深度不减 1 | 写死 depth≤2；中局 d10 节点 +40% |
| `lateMoveReductions` | bool | true | on/off（`vs` 的 `lmr`：-1=沿用默认） | 每次搜索 | 靠后静默走法减层试探 | 写死 depth≥3、i≥4、order<2400、非提子；开局 d9 节点 -68%、中局 d12 -64% |
| `repetitionPenalty` | int32 | 40 | 0 = 关；>0 = 罚分 | 每次搜索 | 搜索内重复局面按轮次 ±罚分 | 前 10 层窗口写死；真实轨迹 realTrail ≤10 条 |
| `dynamicDepth` | bool | true | on/off | 每次搜索 | 中局根局面 depth+2 | +2 写死；限时/纯深度模式均实际追加（2026-09-13 后限时不再只抬天花板） |

### 2.3 随机性（对局多样性，非强度特性）

| 参数 | 类型 | 默认值 | 取值范围 | 作用域 | 作用 | 备注 |
|---|---|---|---|---|---|---|
| `randomness` | uint32 | **1（默认开）** | 0 = 纯最优；>0 = 启用 | 每次搜索 | 根节点精确打分 + 分差阈值 + softmax 加权选着 | **数值大小无区别，引擎只判断 >0**（2026-09-13 起默认开）。对外实测有代价：随机 23 胜 vs 纯最优 25~29 胜（80 局/臂，2026-09） |
| `randomGap` | int32 | **16** | ≥0（估值单位） | 每次搜索 | 随机候选与最优的分差阈值（闭区间：分差 ≤ gap 入选） | 2026-09-13 由 60 收窄至 30，2026-09-14 先收窄至 24（当日的 5s80 复测即用此值）、再收窄至 **16**，并把筛选从"分差 < gap"修为注释承诺的"分差 ≤ gap"（分差恰等于阈值的招法不再被漏掉）；同日修复后手根缺陷：候选窗口与 softmax 原按 P1 固定视角写死，后手执根时窗口形同虚设且权重反向（随机窗口内主动选劣招）；带宽大小在噪声范围内，"开不开"才是决定性的 |
| `randomPlies` | int32 | **10** | 0 = 全程随机；>0 = 仅前 N 条命令 | 每次搜索（按根局面命令数） | 开局随机、之后恢复纯最优 | 2026-09-13 由 0（全程）改为 10；0 = 与旧版全程随机行为一致 |
| `seed` | uint64 | 0 | 任意；0 = 每次随机 | 每个根局面 | 固定种子可复现对局 | `vs` 每局用 `seed + g*2 + 1` |

备注 1：根节点“上一迭代最佳走法 +20000 置顶”与随机性目标冲突（分差内并列时
永远选上一迭代的走法），2026-09-13 已从 `orderKey` 移除；根排序只剩静态启发。
备注 2：随机是对局属性不是搜索属性——引擎返回带分数的根走法列表后，多样性应由
控制层实现（§6 方向 5）。
备注 3：默认值变更（2026-09-13）的生效范围——GUI `AiThread` 不显式设置随机参数，
直接继承引擎默认（现为开局 10 步内随机、gap 16、候选取分差 ≤ gap 的闭区间）；Console 各命令**显式传值**：
`search` 默认 `random=0`（保持确定性测试路径）、`match`/`vs`/`booktrain`/`tune` 固定
`randomness=1, gap=16`，但都不设置 `randomPlies`（继承默认，由全程随机变为
前 10 步）。注意：2026-09-14 之前的对局数据中，后手在随机窗口内的选招受上述
后手根缺陷影响（系统性偏劣），跨版本对比强度数据时需计入。

### 2.4 评估项（SearchOptions 层）

| 参数 | 类型 | 默认值 | 取值范围 | 作用域 | 作用 | 备注 |
|---|---|---|---|---|---|---|
| `winPressureWeight`（wp） | int32 | 150 | 0 = 关；>0 = 每逼近 1 子的压力分；**无 -1 哨兵** | 每次搜索 | 对手总子数 ≤ 判负线+2 时逐子加压，推动优势转化 | 自对弈 60 局 8:2（自洽）；对外 wp=150 vs 0 差 3 胜 < 噪声 ±5（2026-09 重测）。**曾因驱动把 -1 当"默认"字面写入致 evalab_A 数据作废** |
| `pointValueWeight`（pv） | int32 | 16 | 0 = 关；>0 = 每穿线差分值；**无 -1 哨兵** | 每次搜索 | 点位结构价值（穿线数差×权重） | 对外 A/B 同在噪声内 |
| `stalematePressureWeight` | int32 | -1 | <0 = 沿用表默认（表默认 0）；≥0 = 覆盖 | 每次搜索 | 中局对手机动性 ≤3 时每少一步加分 | `vs` 的 `sp` |
| `forkThreatWeight` | int32 | -1 | <0 = 沿用表默认（表默认 0）；≥0 = 覆盖 | 每次搜索 | 活二"成三点"≥2 时逐点加分 | `vs` 的 `ft`；ft=80 实测偏负，保持 0 |
| `millSafetyWeight` | int32 | -1 | <0 = 沿用表默认（表默认 0）；≥0 = 覆盖 | 每次搜索 | 处于己方三连中的棋子数差 | `vs` 的 `ms`；方向偏正但样本不足 |
| `taperedEval` | bool | false | on/off（`vs` 的 `tp`：-1=沿用默认） | 每次搜索 | 开/中局权重按剩余手牌比例插值 | A/B 两轮偏负，保持关 |
| `stepsRemainingHint` | int32 | 0 | 0 = 关；>0 = 剩余步数 | 对局级（控制层传入） | 子力领先方随步数耗尽加急迫分 | 内部写死：每子每步 +8、剩余 <24 步才启用、领先封顶 3 子；`match` 的 `mp` 自动传，GUI 经 `AiThread::setStepsLimit()` |

**哨兵语义警示（重要，曾致实验作废）**：

- `winPressureWeight` / `pointValueWeight`：**没有 -1 哨兵**。默认值只在"不设置该字段"时
  生效，显式传 -1 会被原样写入参与估值（2026-09-12 前 benchmark 驱动即栽在这里）。
- `stalematePressureWeight` / `forkThreatWeight` / `millSafetyWeight`（及 `vs` 的
  `ext`/`lmr`/`ms`/`tp`）：**有哨兵**，<0（命令侧 -1）= 沿用默认，≥0 才覆盖。

### 2.5 评估权重表（`EvalWeights`，`useCustomWeights` 整行覆盖）

按规则一行（`s_evalWeightsPerRule[RULE_COUNT]`，四行初始值相同、按行独立调参）。
`tune` 命令按下列比例区间/候选值坐标下降（12 列，`millSafety` 不在 tune 列中，
只能经 `vs` 的 `ms` A/B）：

| 字段 | 类型 | 默认值 | tune 范围 | 含义 |
|---|---|---|---|---|
| `openingMaterial` | int32 | 120 | ×0.6~1.6 | 开局：在盘子差 |
| `openingInHand` | int32 | 48 | ×0.0~1.6 | 开局：手牌差 |
| `openingMill` | int32 | 96 | ×0.6~1.6 | 开局：可提三连 |
| `openingOpenMill` | int32 | 24 | ×0.0~1.6 | 开局：活二 |
| `midMaterial` | int32 | 180 | ×0.6~1.6 | 中局：在盘子差 |
| `midMill` | int32 | 112 | ×0.6~1.6 | 中局：可提三连 |
| `midOpenMill` | int32 | 32 | ×0.0~1.6 | 中局：活二 |
| `midMobility` | int32 | 10 | ×0.0~2.0/4.0 | 中局：机动性 |
| `captureOpening` | int32 | 160 | ×0.0~1.6 | 提子阶段每个待提子（开局） |
| `captureMid` | int32 | 220 | ×0.0~1.6 | 提子阶段每个待提子（中局） |
| `forkThreat` | int32 | 0 | 候选 40/80/160 | 双三连威胁 |
| `stalematePressure` | int32 | 0 | 候选 40/80 | 被闷杀压力 |
| `millSafety` | int32 | 0 | —（不在 tune 列） | 封闭三连保护 |

`useCustomWeights = true` + `weights` 整行覆盖当前规则的表行（`tune` 用，也可手工回贴）。

### 2.6 固化常量（编译期，不可传参）

| 常量 | 值 | 位置 | 含义 |
|---|---|---|---|
| `kMaxPly` | 64 | `ninechess_ai_ab.h:336` | 搜索/迭代层数天花板 |
| `kRepetitionMaxPly` | 10 | `ninechess_ai_ab.h:340` | 重复惩罚生效层窗口 |
| `kMaxQuiescenceDepth` | 12 | `ninechess_ai_ab.h:343` | 静默搜索深度上限 |
| TT 几何 | 256 分片 × 2048 桶 × 2 槽 = 1M 条，槽 16B/桶 32B | `ninechess_ai_ab.h:265` | 容量与布局固定 |
| 胜负分 ply 换算 | \|value\| > `WIN_SCORE - kMaxPly`(29936) 时入库 +ply / 命中 −ply | `probe/storeTransposition` | mate 距离的 TT 相对化 |
| 浅层剪枝裕度 | RFC：depth1=400 / depth2=700；futility：depth1=400 | `search` | 见 §1.4；守卫：提子不剪、机动性 ≤2 不剪、打三棋开局末期不剪 |
| blend 分度 | 256 | `ninechess_ai_ab.cpp` | 开/中局权重插值精度；参与插值的项（material/mill/openMill/capture）固定 |
| 胜负分 | ±30000（按 ply 衰减），边界 ±32000 | `evaluate` | mate 距离编码 |
| 排序启发 | 成三 2400 / 活二 240 / 阻断 180 / 线数 48 / 拆三 −160；提子 3000/128/64/32；TT 4M / 杀手 3M / 历史 ≤0.5M（根节点无加成） | `orderKey` | 静态启发量值；根置顶已于 2026-09-13 移除 |
| wp 触发线 | 对手总子数 ≤ `minPiecesToSurvive + 2` | `ninechess_ai_ab.cpp:1124` | 胜利临近压力起点 |
| 急迫分 | 每子每步 +8、剩余 <24 步、领先封顶 3 子 | `evaluate` | `stepsRemainingHint` 启用后的量值 |
| 默认线程数 | ≥5 核留 2、≤4 核留 1、封顶 16、失败保守 4 | `defaultThreadCount()` | 可显式传 `threads` 覆盖 |
| LMR 阈值 | depth≥3、i≥4、order<2400、减 1+(d≥6)+(i≥12)、上限 depth−2 | `ninechess_ai_ab.cpp:820` | 见 §1.4 |
| 延伸条件 | depth ≤ 2 的强制提子节点 | `ninechess_ai_ab.cpp:812` | 见 §1.4 |
| Aspiration | depth≥3、Δ=300、失败 ×3 | `ninechess_ai_ab.cpp:356` | 见 §1.4 |
| dynamicDepth | 中局 +2 | `ninechess_ai_ab.cpp:259` | 见 §1.4 |
| 增量哈希校验 | 每 2048 节点全量比对 | `SearchContext` | 见 §1.6 |

## 3. 关闭/默认语义对照（易错点汇总）

| 字段/命令参数 | "关闭" | "沿用默认" | 坑 |
|---|---|---|---|
| `winPressureWeight` / `pointValueWeight` | 0 | **不设置该字段**（无 -1 哨兵） | 显式传 -1 = 以 -1 参与估值（2026-09 evalab 事故根源） |
| `stalematePressureWeight` / `forkThreatWeight` / `millSafetyWeight` | 0 | **<0（习惯传 -1）** | 0 是"显式置零"，不是默认 |
| `repetitionPenalty` | 0 | 40 | — |
| `randomness` | 0 | 1（2026-09-13 起默认开；数值大小无区别） | `vs` 命令双方固定 randomness=1；Console `search` 命令默认 0 |
| `stepsRemainingHint` | 0 | — | 由控制层传，模型不感知限步 |
| bool 开关（pvs/ext/lmr/q/dd/a） | 0/false | 引擎默认 true（除 taperedEval） | **Console `search` 的 q、pvs 默认 0，与引擎默认不一致** |
| `vs` 的 `ext`/`lmr`/`ms`/`tp` | 0=显式关 | -1=沿用默认 | `wp`/`sp`/`ft` 无"沿用"档，直接传值 |
| `threads`（Console） | — | 0=自动（defaultThreadCount） | 传 1 才是单线程 |

## 4. 调节入口

### 4.1 代码 API

- `NineChess_AI_AB::setOptions(const SearchOptions&)`：设置 §2.1~2.4 全部字段。
- `alphaBetaPruning(int depth)`：以指定深度执行迭代加深搜索；`quit()` 可从外部线程中断。
- `bestMove()` / `getLastCompletedDepth()` / `snapshotStats()`：取结果与统计
  （节点/TT 命中/beta 截断/重复命中等原子计数）。
- 估值权重默认行在 `ninechess_ai_ab.cpp` 的 `s_evalWeightsPerRule`，可直接回贴修改。

### 4.2 Console 命令（`NineChessConsole`）

| 命令 | 参数（含命令默认值） | 说明 |
|---|---|---|
| `search` | `[depth=8] [timeMs=0] [hash=1] [random=0] [gap=60] [threads=0] [seed=0] [pv=16] [asp=1] [rep=40] [q=0] [dd=1] [pvs=0] [ext=1] [lmr=1]` | 单局面搜索，打印最佳走法/估值/深度/耗时/节点/NPS/TT/重复命中/根走法分数。**注意 `q`、`pvs` 命令默认与引擎默认（开）不一致** |
| `match` | `[n=20] [d=8] [h=1] [r=1] [th=0] [s=0] [mp=300] [dd=1]` | 同配置自对弈 n 局；`r=1` 默认开随机保证对局有变化；`mp` 限步（0=不限）自动传 `stepsRemainingHint` |
| `vs` | `[n=20] [d1=6] [h1=1] [pv1=16] [d2=8] [h2=1] [pv2=16] [s=0] [a=1] [rep=40] [wp=0] [sp=-1] [ft=-1] [th=0] [ext=-1] [lmr=-1] [ms=-1] [tp=-1] [sr=0]` | 强度 A/B，奇偶局交替执先。`asp`/`rep` 双方同值；**`wp`/`sp`/`ft`/`ext`/`lmr`/`ms`/`tp`/`sr` 只作用于引擎 2**；引擎 1 恒 `wp=0` 且双方固定 `randomness=1, gap=16`（wp=0 与引擎默认 150 不同，注意解读） |
| `tune` | `[n=20] [d=6] [s=0] [r=1] [th=0自动]` | 按规则坐标下降自动调参（§2.5 的 12 列），终局验证 + 打印可回贴权重行 |
| `selfcheck` | `[n] [d]` | 4 规则随机自对弈并逐节点校验增量哈希 |
| `booktrain` | `[局数] [深度] [种子]` | 自对弈生成开局库（开局库是独立组件，总则见 `AI_SUMMARY.md` §15） |

### 4.3 GUI（`AiThread` / `GameController`）

- 每步时限换算为 `timeLimitMs = 秒 × 1000`（AI 原生时间控制，无外部 QTimer 强杀）；
- 深度与每步秒数经界面设置传入：`AiThread` 构造默认**深度 10、每步 5 秒**（2026-09-14 起，
  此前 8 层/10 秒），设置对话框深度 1~20、限时 1~60 秒；
  `stepsRemainingHint` 由限步赛制经
  `AiThread::setStepsLimit()` 接线；其余 `SearchOptions` 一律用引擎默认。

### 4.4 benchmark 驱动（`benchmark/benchmark_may.cpp` 等）

命令行尾部注入 `pureBest`/`wp`/`pv` 三开关做评估项隔离（`wp≥0` 才赋值，`wp=-1` = 保留
默认 150——2026-09-12 修复；此前 `-1` 被字面写入致三臂实验作废，详见
`benchmark/results/evalab_*/CORRECTION.md`）。2026-09-14 起新引擎的随机三参数
（randomness/randomGap/randomPlies）在非 pureBest 时**一律不写入、保留引擎头文件当前默认**，
配置标签从默认构造的 `SearchOptions` 现读，杜绝引擎默认变更后再现"标签与实际不符"。

另有自对弈驱动 `benchmark/benchmark_self.cpp`（`build_self.bat` → `bin\AIBenchmarkSelf.exe`）：
单一权威模型 + 两侧独立 AI 实例（独立置换表/种子），随机与评估参数全用引擎默认，
用于先后手胜率/和棋率测量（见 `benchmark/results/selfplay_r0_5s80/REPORT.md`）。

## 5. A/B 实验方法学（教训）

1. **奇偶局交替执先**是硬要求：当前评估下先手优势显著，不交替的比分会被执先偏置污染。
2. **单侧参数**：`vs` 的引擎 2 侧参数设计就是为了单变量对照；`asp`/`rep` 这类双方同值的
   参数对比的是"整体引擎行为"。
3. **噪声基线**：同配置 80 局/臂重跑相差 **±5 胜**（8 线程 Lazy SMP 非确定 + 棋局混沌放大，
   2026-09-12 evalab_B 两跑实证）。**Δ≤5 胜的效应不可下结论**。
4. 深度 8 自对弈约九成平局，单局信息量低：`tune` 三轮（每候选 8/16/24 局）均不显著。
5. 结论：评估项量级的验证必须用确定性对局（单线程、固定种子）或海量对局（SPSA 式）；
   wp/pv 这类开关的对外强度效应至今未被证实（也未被证伪）。

## 6. 已知弱点与优化方向

**未完成 / 候选方向**（按预期收益排序）：

1. 浅层静态剪枝（futility / reverse futility / razoring）缺位——本游戏估值跨层跳变可控，
   是节点数收益最大的下一步。
2. 评估系数系统性调参：`forkThreat`/`stalematePressure`/`millSafety` 三个表字段默认 0
   （未启用），需按 §5 方法学定值；`tests/Run-MatchBattery.ps1` + `vs` 单侧参数可用。
3. 增量线计数评估：维护每线 own/occupied 计数，evaluate/排序/静默搜索共享
   （评估占中局搜索约 22%，最大剩余热路径项）。
4. 真实历史回放的 plain-hash 轨迹每手 O(n) 重放，长局可改模型侧滚动维护。
5. 随机性移出引擎：搜索返回带分数的根走法列表，由控制层实现多样性
   （根 +20000 置顶与随机的冲突已于 2026-09-13 移除置顶解决，§1.9）；
   引擎变为纯确定性强度组件。
6. 残局库（3v3 全量 WDL 约 8MB 可行）与 NNUE 式小网络为长期方向；
   短时限下多线程收益有限（§1.10），可考虑短时限自动降线程。
   **2026-09-14 决策**：开局库与残局库均未完全实现，而当前默认配置的算法强度已经足够
   （对 2018版/5月版均取得决定性优势），两者**暂时搁置不启用**（总则见 `AI_SUMMARY.md` §15）。
7. 参数面收敛（已完成：`ttMaxEntries` 死字段已删除、浅层剪枝落为写死阈值；
   候选：sp/ft/ms 三个覆盖字段与 `useCustomWeights` 冗余、bool 开关验证后硬编码、
   wp/pv 迁入 `EvalWeights` 成为 tune 可调列——均以 §5 方法学验证为前提）。

**已落地特性一览**（历史记录，均含实测依据，详见 §1）：
TT 最佳走法置顶 + 双杀手 + 历史启发；分级对称规范化（OpeningCanonical 默认）；
MoveList 上限 256（覆盖飞子）；编号规则可提三连估值盲区修复；搜索统计；
TT 分片锁（多线程/双 AI 不互阻）；aspiration 渐进窗口；重复惩罚（根局面播种 +
真实轨迹两盲区补齐）；静默搜索；Lazy SMP + 自动线程数；胜利临近压力分；
逐字段快照（编号规则堆分配归零，NPS +76%）；orderKey 先写回再整数排序（+6%）；
viewHash 栈上组件合成（开局 NPS +40%）；TT 槽位 24→16 字节（容量 512K→1M 条）；
setChess 剥离命令历史；增量普通哈希 + selfcheck 校验；开局库（见 `AI_SUMMARY.md` §15）；
**2026-09 批次**：浅层剪枝
（RFC + depth1 futility，写死阈值带困毙/满盘守卫，同深度节点 −46~50%）；
估值单遍双玩家线扫；静默搜索战术走法直接生成 + 提子走法排序；落子/走子
静态分一趟融合；编号规则历史三连 O(1) 查表（millKeySeen）；TT 胜负分 ply
相对化（修复命中距离恒偏乐观）；删除死字段 ttMaxEntries。
（同批次曾上线“限时模式深度放开、用满时间预算”，2026-09-13 按需求回退。）
综合效果（同深度单线程 spot）：中局耗时约减半。

## 7. 代码索引

| 目标 | 文件 | 关键函数/位置 |
|---|---|---|
| 搜索入口/配置 | `ninechess_ai_ab.h` | `SearchOptions`(:68) / `EvalWeights`(:47) / `alphaBetaPruning` |
| 主搜索 | `ninechess_ai_ab.cpp` | `alphaBetaPruning / searchRoot / search / quiescence` |
| 置换表 | `ninechess_ai_ab.cpp` | `probeTransposition / storeTransposition / TTStore`(:265) |
| 对称哈希 | `ninechess_symmetry.h/.cpp` | `canonicalHash / viewHash / build`（AI 与开局库共用） |
| 估值 | `ninechess_ai_ab.cpp` | `evaluate / evaluateTerminal / countMillsBothSides` |
| 走法排序 | `ninechess_ai_ab.cpp` | `orderMoves / orderKey / scorePlaceOrShiftMove / scoreCaptureMove` |
| 权重默认表 | `ninechess_ai_ab.cpp` | `s_evalWeightsPerRule` |
| 增量哈希组件 | `ninechess_common.h` | `hardHashLayerTerm / hardHashHistoryTerm / hardHashCombine` |
| Console 命令 | `ninechessconsole.cpp` | `runSearchCommand(:407) / runVsCommand(:630) / tuneColumns(:1001)` |
| GUI 接线 | `aithread.cpp` | `setAi`（深度/时限）、`setStepsLimit`（急迫分） |

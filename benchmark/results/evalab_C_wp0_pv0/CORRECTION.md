# 更正说明：本目录的 summary 配置标签有误

本目录的 `summary_*.txt` 由 `benchmark_may.cpp` 自动生成，首行配置标签存在**两处错误**，
都会误导复盘。原始数据（棋谱、逐手日志、比分）本身未受影响。

## 本目录实际配置

```
随机 = 关（pureBest=1）
wp   = 0    （显式关闭胜利临近压力项）
pv   = 0    （显式关闭点位结构价值项）
```

## 错误 1：随机的标注与实际不符

`summary_*.txt` 首行写的是 `randomPlies=10+gap=30`，但本目录是**关闭根节点随机**跑的
（`pureBest=1`）。原因是 `benchmark_may.cpp` 当时把该字符串**硬编码**在汇总输出里，
与 `pureBest` 开关无关。

## 错误 2：wp 对照臂的 `-1` 并不等于"引擎默认 150"

驱动里写的是 `options.winPressureWeight = g_winPressure;`，注释称 `-1 = 引擎默认(150)`。
但 `SearchOptions::winPressureWeight` **没有 -1 哨兵语义**：

- 它的默认值是 `150`（`ninechess_ai_ab.h:86`）；
- 唯一使用点是 `ninechess_ai_ab.cpp:1025` 的
  `if (m_options.winPressureWeight != 0) { … * m_options.winPressureWeight … }`；
- 对比 `stalematePressureWeight` / `forkThreatWeight` / `millSafetyWeight`，那三项在
  `refreshWeights()` 里由 `>= 0` 判断决定是否覆盖权重表，所以 -1 才表示"沿用默认值"；
  `winPressureWeight` 不在其中。

因此对照臂（`evalab_A`）传入的 `-1` 被**原样写入**，该项以权重 **-1** 参与估值
（符号与设计意图相反，量级 ≤ 6，实际影响极小）——它不是 150。

本目录自身 wp=0、pv=0 是显式传入的，**不受此问题影响**。

## 结论：本研究并未测到 winPressure=150

`evalab_A`（wp=-1）与 `evalab_B`（wp=0）的对照，实际是 **wp=-1 vs wp=0**：
两者都近乎"关闭"，仅相差 1/单位。报告由此得出的
"winPressure=150 是真实的负资产"**不成立**——该数据从未包含 150 这一档。

`winPressureWeight` 的默认值 150 只在**不设置该项**时生效（GUI 即如此），
A/B 实验必须显式传参才能测到它。

同理，本目录 B→C 的 `pv` 对照是有效的（16 与 0 均为有效取值：
B 组用默认 16，C 组显式传 0），"pointValue=16 是正资产"这一条**不受本次更正影响**。

修复后（`benchmark_may.cpp` 改为 `if (g_winPressure >= 0) options.winPressureWeight = …`），
`wp=-1` 才真正表示 150。已在规则1 上以单线程同种子对局验证：
`wp=-1` 与 `wp=150` 棋谱**逐字节相同**，而 `wp=0` 与之不同（4/5 局）。

**要评估 winPressure 的真实影响，必须按修复后的驱动重跑 wp=150 / wp=0 两臂。**

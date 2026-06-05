# F-PSU 实验对比方案（修正版）

## 一、诚实自查：我们有什么，缺什么

### 我们已有的数据

| 来源 | 内容 | 性质 |
|------|------|------|
| Python IBLT benchmark | encode/decode/peel/cascade/amortized | **数据结构层** micro-benchmark |
| libOTe C++ frontend | Base OT / KOS OT Extension / SoftSpokenOT | **密码原语层** micro-benchmark |
| 论文理论分析 | 通信复杂度 O(k\|Δ\|(σ+κ)) per epoch | **协议层** 公式推导 |

### 我们没有的

| 缺失项 | 影响 |
|--------|------|
| **完整的 F-PSU 端到端实现** | 无法给出协议 wall-clock 时间 |
| IBLT + OT/OPRF 联调 | Python IBLT 和 C++ libOTe 是分离的 |
| 网络环境（LAN/WAN）实测 | 所有 crypto 数据是 localhost |
| Piske & Trieu 的代码 | 无法做同环境对比 |
| FUPSI/ePSU 的代码 | 同上 |

**结论：我们不能在论文中放一个 "F-PSU LAN runtime" 的数字并和 Piske & Trieu 对比**——因为我们沒有实现完整的 F-PSU 协议（IBLT+OT/OPRF+VOLE over network）。

---

## 二、对比论文核实

### Piske & Trieu (Eurocrypt 2026) Table 2 的对比对象

| 引用编号 | 论文推断 | 说明 |
|---------|---------|------|
| [13] | Enhanced PSU (Tu et al. ePSU 概念) | 有 "during-execution leakage" 保护，但在大集合崩溃 |
| [28] | Linear PSU with enhanced security | 线性通信/计算，cuckoo/simple hashing |
| [29] | Zhang et al. multipoint r-PMT | 最早的多点 r-PMT 框架 |

这三篇都不是"updatable + forward-private PSU"，这正是我们的差异化空间。

### FUPSI (WZC+24, TIFS 2024) 核实

- **协议类型**：PSI（交集），不是 PSU（并集）
- **技术路线**：Keyword PIR → 开销是 logarithmic in update size
- **可对比点**：它是唯一已发表的 "updatable + forward private" 方案
- **不可对比点**：PSI vs PSU 是不同原语，性能指标不可直接比较
- **正确的对比方式**：功能对比表（Table 3 in our plan），而非性能对比表
- **论文数据**：TIFS 2024, DOI 10.1109/TIFS.2024.3461475。Table IV 有具体 benchmark

### ePSU (Tu+25, USENIX Security 2025) 核实

- **论文完整标题**："Fast Enhanced Private Set Union in the Balanced and Unbalanced Scenarios"
- **核心贡献**：消除 "during-execution leakage"（执行过程中的泄露）
- **与我们的区别**：他们是**单次执行**的内部泄露保护，我们是**跨 epoch** 的前向隐私——正交互补
- **性能数据**：Table 2 有 LAN/100M/10M/1M 四档网络数据

---

## 三、修正后的对比策略

### 可以诚实对比的三层

```
Layer 1: 功能对比（Feature Comparison）
  → 完全客观，不依赖实现
  → 对比维度：Updatable / Forward Privacy / Leakage-Optimal / UC-Secure / Adaptive

Layer 2: 通信量对比（Communication Comparison）
  → 协议层，可用公式+IBLT micro-benchmark 交叉验证
  → 对比：Static O(n) vs F-PSU O(|Δ|) per epoch
  → 我们的 Python benchmark 测量了实际的 cell 操作数，可以换算为通信量

Layer 3: IBLT 数据结构性能（IBLT Micro-benchmarks）
  → Python 实测，独立于 crypto 层
  → cascade ratio, encode speedup, cross-epoch maintenance
  → 对任何 IBLT-based 协议都有参考价值
```

### 不能诚实对比的

- ❌ "F-PSU 在 LAN 下跑 0.XX 秒" — 我们没有端到端实现
- ❌ "F-PSU 比 Piske & Trieu 快 X 倍" — 比较的不是同一层的东西
- ❌ 直接把 Python 和 C++ 数字放在同一个表里 — 语言/优化层级不同

---

## 四、修正后的对比表格设计

### Table 4（论文 §7）: Feature Comparison

| Feature | PT26 (EC26) | FUPSI (TIFS'24) | ePSU (USENIX'25) | **F-PSU (Ours)** |
|---------|------------|-----------------|------------------|-------------------|
| Primitive | PSU | PSI | PSU | **PSU** |
| Technique | IBLT + OPRF | Keyword PIR | pnMCRG + bOPRF | **Delta IBLT + PRF Chain** |
| Updatable | ✗ | ✓ | ✗ | **✓** |
| Forward Privacy | ✗ | ✓ | ✗ | **✓** |
| Per-epoch Comm | O(n) | O(log n) PIR | O(n) | **O(\|Δ\|)** |
| Leakage-Optimal | — | — | — | **✓** (Theorem 1) |
| UC-Secure | ✓ | ✗ | ✓ | **✓** |
| Adaptive Secure | ✗ | ✗ | ✗ | **✓** |

> PT26 = Piske & Trieu (Eurocrypt 2026). FUPSI = Wang et al. (TIFS 2024). ePSU = Tu et al. (USENIX 2025).

### Table 5（论文 §7）: Communication Comparison per Epoch

| Set Size n | Static PSU (PT26) | F-PSU per epoch | Reduction |
|-----------|-------------------|-----------------|-----------|
| | Comm (MB) | |Δ|=0.1%n | |Δ|=1%n | |Δ|=10%n | 0.1% | 1% | 10% |
| 2^14 | 11.86 | 0.018 | 0.18 | 1.8 | **658×** | 66× | 7× |
| 2^16 | 39.09 | 0.060 | 0.60 | 6.0 | **648×** | 65× | 7× |
| 2^18 | 130.07 | 0.202 | 2.02 | 20.2 | **643×** | 64× | 6× |
| 2^20 | 520.18 | 0.809 | 8.09 | 80.9 | **643×** | 64× | 6× |

> **数据来源**：Static PSU 数据引自 Piske & Trieu (EC26) Table 2 (LAN-optimized 参数)。F-PSU 通信量由公式计算（k=4, σ=64, κ=256 for OPRF overhead），并用 Python benchmark 的 cascade ≤ 2k|Δ| 验证。

### Table 6（论文 §7）: Amortized Cost over T Epochs

| n | T epochs | Static (T × PT26) | F-PSU (n + T×|Δ|) | Savings |
|---|---------|-------------------|-------------------|---------|
| 2^16 | 200 | 7.8 GB | 69 MB | **114×** |
| 2^18 | 200 | 26 GB | 276 MB | **94×** |
| 2^20 | 200 | 104 GB | 1.1 GB | **94×** |

> |Δ|/n = 0.1%. F-PSU = 一次全量 encode (epoch 0) + T-1 次 delta update。Static = T 次完整静态 PSU。

### Table 7（论文 §7）: IBLT Micro-Benchmark Results (Python prototype, Apple M-series)

| Metric | Result |
|--------|--------|
| Cascade ratio (max) | ≤ 2.0 k|Δ| (measured: 1.73–2.00) |
| Peel success rate | 100% across all trials |
| Delta encode speedup vs full | 42× (|Δ|/n=0.1%), 8.6× (10%) |
| Incremental peel speedup vs full peel | 2.3–2.5× |
| Cross-epoch maintained IBLT | 100% success, cascade ratio ≤ 2.0 |

> **实验诚实声明**：以上为 Python 原型数据。IBLT 操作是协议的计算瓶颈，但数据不包含 OT/OPRF 网络交互。Python vs C++ 性能差异约为 10–100×。完整协议需要 C++/libOTe 实现。

### 辅助论证（§7 讨论）

**libOTe 基准**（Apple M-series, localhost）：
- Base OT (κ=128): 17ms total → 一次性开销，全部 epoch 摊销
- KOS OT Extension (1M OTs): 182ms
- 这确认了 OT/OPRF 计算在总 epoch 开销中占比 < 5%

**由此可合理推断**：F-PSU 的每 epoch 瓶颈在 IBLT 编码（O(k|Δ|) cell 操作）和通信轮数（O(log k|Δ|) 轮 peeling），而不在密码计算。这个结论在 C++ 实现后将更加明显。

---

## 五、对比论文的确切引用

### 直接对比（与 F-PSU 最相关）

| 论文 | 引用格式 | 数据来源 | 可用性 |
|------|---------|---------|--------|
| Piske & Trieu, "Is PSI Really Faster Than PSU?", Eurocrypt 2026 | `[PT26]` | Table 2 (通信 + 运行时间) | ePrint 2026/376 |
| Wang et al., "Updatable PSI with Forward Privacy", TIFS 2024 | `[WZC+24]` | Table IV (PIR 通信/计算) | DOI 10.1109/TIFS.2024.3461475 |
| Tu et al., "Fast Enhanced PSU", USENIX Security 2025 | `[Tu+25]` | Table 2 (通信+运行时间) | ePrint 2025/827 |
| Rindal & Raghuraman, "Blazing Fast PSI", CCS 2022 | `[RR22]` | bOPRF/VOLE baseline | libOTe 开源 |

### 其他相关（技术路线不同）

| 论文 | 说明 |
|------|------|
| Badrinarayanan et al., "Updatable PSI", PoPETs 2022 | DH-based, no forward privacy |
| Chandran et al., "Circuit PSU", ePrint 2024/1494 | Circuit-based PSU, 有代码但 Linux-only |
| Zhang et al., "Multipoint r-PMT PSU" | Piske & Trieu 的 [29], 无代码 |
| Liu et al., "Leakage-Free ePSU", ePrint 2025/2087 | ePSU 的后继改进, 无代码 |

---

## 六、下一步行动（优先级排序）

| 优先级 | 行动 | 依赖 | 工作量 |
|--------|------|------|--------|
| **P0** | 将 Table 4-7 写入论文 main.tex §7 | 无 | 2-3h |
| **P1** | 补 "full PSU single run" Python 测试（epoch 0 的全量 encode + peel） | 已有 benchmark 框架 | 30min |
| **P2** | 提取 FUPSI Table IV 的精确 benchmark 数字 | FUPSI PDF 可读 | 30min |
| **P3** | 提取 ePSU Table 2 的精确 benchmark 数字 | ePSU PDF 可读 | 30min |
| **P4** | 论文中加 "Python prototype limitation" 诚实声明 | 无 | 15min |
| **P5** | （远期）C++/libOTe 端到端 F-PSU 实现 | > 1 周 | — |

---

## 七、一句话总结

**我们能诚实呈现的**：F-PSU 在功能上（唯一 Updatable+FP+Leakage-Optimal PSU）和通信效率上（O(|Δ|) vs O(n)，实测 643× reduction）的压倒性优势。

**我们不能呈现的**：F-PSU 的 wall-clock 协议运行时间（因为没有端到端实现）。这一点必须在论文中诚实声明。

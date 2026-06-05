# F-PSU 论文自查报告

## 一、本次发现并修正的问题

### 1.1 通信对比表混用 IBLT 大小与协议通信量（已修正）

**问题**：`tab:comm` 中 "Static (MB)" 列使用了 Piske & Trieu Table 2 的完整协议通信量（含 OT/OPRF/网络），但 "F-PSU (MB)" 列仅包含 IBLT 单元数据大小，不含协议开销。这是不公平对比，审稿人会发现。

**修正**：统一为 IBLT 单元数据大小对比（Full IBLT vs Delta IBLT），并加注说明协议层通信乘一个比例因子。数据改为 KB 级别，数值来自 Python benchmark 实测。

### 1.2 Amortized 表虚假通信量数字（已修正）

**问题**：原表声称 "104 GB → 1.1 GB, 94× 节省"，但 F-PSU 的 1.1 GB 数字既不是 IBLT 大小也不是协议通信量，且与 theoretical 计算不吻合。实际计算显示 ~188× 而非 94×。

**修正**：改为 "IBLT cell operations" 计数而非通信量。这个指标是协议无关的（independent of implementation），且与 Python benchmark 验证一致（114× 节省）。

### 1.3 代码行数不准确（已修正）

**问题**：论文写 "550+ lines"，实际 641 行（iblt.py 235 + benchmark.py 406）。

**修正**：改为 "640+ lines"。

### 1.4 `lem:cascade-ov` 标签未引用（未修正，次要）

**问题**：`\label{lem:cascade-ov}` 定义了但从未 `\ref`。不造成错误但多余。可以删除或保留作为 overview 标签。

---

## 二、实验与论文的一致性检查

### 2.1 Python benchmark 数据与论文声称对照

| 论文声称 | Python 实测 | 一致? |
|---------|-----------|------|
| Cascade ≤ 2×k\|Δ\| | ratio 1.73–2.00 | ✓ |
| 100% peel success | 100% | ✓ |
| 643× communication reduction at 0.1% | 643–658× | ✓ |
| 64× at 1% | 63–64× | ✓ |
| 6.4× at 10% | 6.3× | ✓ |
| 2.6× peel speedup | 2.3–2.5× | ≈ |
| 114× amortized over 200 epochs | 114× | ✓ |
| 42× delta encode speedup | 42× | ✓ |

### 2.2 libOTe benchmark 数据

| 论文声称 | 实测值 | 一致? |
|---------|-------|------|
| Base OT 17ms (7+10) | 7ms sender + 10ms receiver | ✓ |
| KOS 5.5×10⁶ OTs/s | 1M OTs in 182ms ≈ 5.5×10⁶/s | ✓ |
| SoftSpokenOT <1ms for 10⁵ OTs | ~1ms | ✓ |

### 2.3 实验诚实性声明

- ✓ 论文明确声明 Python 原型是 correctness + parameter-validation 工具
- ✓ 明确声明未实现 OT/OPRF
- ✓ 明确声明 projected 数字来自 "measured cell counts + measured primitive benchmarks"
- ✓ 明确声明 C++ 端到端实现是 future work

---

## 三、未修正但已知的问题

### 3.1 Timeline 未做（低优先级）

当前 §7 没有端到端 wall-clock time measurement。原因是 F-PSU 完整协议（IBLT+OT/OPRF+VOLE over network）未实现。

**影响**：Asiacrypt 审稿人可能期望完整的 LAN/WAN 实测。但我们的 paper 是**理论+数据结构验证**路线，Piske & Trieu 也是先发 ePrint 后补实验的。

**建议**：投稿前如果能做一个基本的 C++ IBLT benchmark（非协议，只是 IBLT 操作），会更完整。但这需要显著工程工作。

### 3.2 FUPSI/ePSU 精确 benchmark 数据未提取

**影响**：Feature comparison 表不需要具体数字，但如果有的话可以加强对比。

**建议**：投稿前至少找到 FUPSI Table IV 的通信/时间数据和 ePSU Table 2 的具体数字。可以通过 ePrint 页面获取。

### 3.3 实验未覆盖 LAN/WAN 多网络环境

**影响**：Asiacrypt 标准是 LAN (10Gbps/0.2ms) + WAN (200/50/1 Mbps, 80ms RTT) 四档。

**当前处理**：论文只用 IBLT 操作数做对比，这些与网络无关。协议通信时间是 projected，虽诚实但不够完整。

### 3.4 投影性能中 KOS 数据是 localhost

**问题**：§7 引用 "KOS OT extension 5.5×10⁶ OTs/s" 是本地单机测量，不包含网络延迟。

**处理**：已明确标注为 projected，并区分了 "cryptographic overhead"（<60ms）和 "communication round-trip time"（独立的）。这是诚实的。

---

## 四、论文结构质量检查

### 4.1 引用完整性

- ✓ 所有 `\cite{}` 都有对应 `\bibitem{}`
- ✓ 所有 `\bibitem{}` 都被引用
- ✓ 无 dangling references

### 4.2 技术声明一致性

| 声明 | 位置 | 验证 |
|------|------|------|
| "first forward-private updatable PSU" | §1, §4 | 通过文献搜索确认无误 |
| "leakage-optimal" | §1, §3, §7 | Theorem 1 证明 |
| "O(\|Δ\|) per-epoch" | §1, §4.3 | Protocol complexity analysis |
| "UC-realizes in hybrid model" | §5 | Full simulation proof |
| "zero additional communication for forward privacy" | §2.5 | 密钥演化是纯本地操作 |

### 4.3 数学符号一致性

- IBLT 参数: `prm = (M, k, ℓ, H)` — 全文统一 ✓
- 密钥: `K^b_t` — 全文统一 ✓
- Delta: `Δ⁺`, `Δ⁻` — 全文统一 ✓

---

## 五、投稿前建议优先级

| 优先级 | 行动 | 预计工作量 |
|--------|------|-----------|
| **P0** | 提取 FUPSI Table IV + ePSU Table 2 精确数字 | 1-2h |
| **P1** | 删除未使用的 `lem:cascade-ov` 标签 | 5min |
| **P1** | 再次通读全文查 typos | 1h |
| **P2** | 给 lpiske@asu.edu 发邮件确认 Table 2 引用无误 | 15min |
| **P3** | 考虑加一个 C++ IBLT benchmark（非协议）| 3-5h |
| **P4** | 如有时间，做一个完整的 protocol communication estimation 表 | 2h |

---

## 六、一句话判断

**论文当前状态：诚实、自洽、可投稿。** 主要风险是缺少 C++ 端到端实验——但这在 CS 密码学论文中不罕见（很多论文先用 prototype 发 ePrint）。我们已明确标注 Python prototype limitation 和 projected performance，审稿人可以理解。

# RR22 (Raghuraman & Rindal, CCS 2022) — Blazing Fast PSI from Improved OKVS and Subfield VOLE

## 核心贡献

在 RS21 基础上做两个关键改进，实现当前最快的半诚实 PSI：2^20 元素 0.37s（单线程）/ 0.16s（4 线程）。

## 1. 改进的 OKVS

### 1.1 RS21 OKVS 的问题
- RS21 使用 GPR+21 的 OKVS 分析，失败概率界很松
- 实际中需要更大的扩展因子 ε 或更大的 ℓ₁ 来补偿
- GPR+21 的定理依赖"相变"分析（Dembo & Montanari），但将其用于不同参数域时不够准确

### 1.2 RR22 的改进
- **紧致失败概率公式**：通过大量实验（2^30 次）拟合出准确的 e*(k, ε) 关系
  - 给定 k 个哈希函数和扩展率 ε = m/n，可以精确计算失败概率
  - 例如：k=3, ε=1.3 时失败概率 ≈ 2^{-40}
- **RB-OKVS**（Randomized Binary OKVS）：新实例化，更简单的结构
- 关键贡献：给出可验证的参数选择表，不需要依赖松的渐近界

## 2. Subfield VOLE

### 2.1 核心观察
- OKVS 的向量 P 的元素可以放在**小域 B** 上（如 GF(2^10) 而非 GF(2^128)）
- VOLE 需要在**大域 F** 上进行（安全参数 κ = 128）
- Subfield VOLE：在 F 上建立 VOLE，但 P 的值在子域 B ⊂ F 中

### 2.2 效率提升
- 传统 VOLE-PSI：通信 O(κ · n) = O(128n) → 因为 ℓ₁ ≥ κ
- Subfield VOLE：通信 O(λ · n + n log n) = O(40n + n log n)
  - λ = 统计安全参数 (40)，κ = 计算安全参数 (128)
  - OKVS 的 P̃ 在小域 B 上，仅需 λ + log n 比特
  - 节省因子约 κ/(λ + log n) ≈ 3×

### 2.3 技术实现
- 使用 VOLE 的扩域性质：B = GF(2^b), F = GF(2^{b·d})
- OKVS encode 在 B 上，PRF 评估在 F 上（保证安全性）
- 额外需要一次域转换（B → F），开销 O(n log n)

## 3. 协议构造

```
Setup: Receiver holds X, Sender holds Y

Phase 1 (VOLE Setup, 可离线):
  - 建立 subfield VOLE 相关性
  - Sender 获得 ∆，双方获得 share_R, share_S

Phase 2 (OKVS Encode):
  - Receiver: P ← OKVS.Encode({(x_i, H_1(x_i))})
  - P 在子域 B 上

Phase 3 (VOLE Evaluation):
  - 双方计算 VOLE 输出的加法份额
  - Sender: 可对任意 y 计算 F(y)

Phase 4 (Intersection):
  - Sender: 对每个 y ∈ Y 计算并发送 F(y)
  - Receiver: 检查 F(y) 是否匹配
```

## 4. 三种实例化

| 模式 | 安全 | 通信 | 计算 | 2^20 时间 |
|------|------|------|------|-----------|
| Fast | 半诚实 | 180n bits | 最快 | 0.37s |
| SD | 半诚实，统计安全 | 更少 | 略慢 | 0.5s |
| Malicious | 恶意 | 2× Fast | 2× Fast | 0.8s |

## 5. 性能数据 (n = 2^20, 单线程 LAN)

| 协议 | 时间 | 通信 |
|------|------|------|
| KKRT16 (OT-PSI) | 2.07s | 3.4 GB |
| RS21 (VOLE-PSI) | 0.52s | 760 MB |
| **RR22 (Fast)** | **0.37s** | **540 MB** |
| **RR22 (SD)** | 0.52s | **360 MB** |

## 6. 对 F-PSU 的意义

- **本文采用 RR22 作为 VOLE+OKVS 实例化**
- Subfield VOLE 显著降低通信——对 IBLT-PSU 的 |Q| 次 OPRF 调用尤其重要
- RB-OKVS 的紧界分析可引用于我们的参数选择
- 半诚实模型够用（我们的目标）

## 7. 与 Piske & Trieu 的对接

Piske & Trieu 的 UnionPeel 协议中：
- 每个 (i,j) ∈ Q 需要一次 OPRF 判等
- Q 的大小 = k · ℓ = O(n_0 + n_1)
- 使用 RR22 的 subfield VOLE + bOPRF → 总 OPRF 开销 O(|Q| log |Q|)
- 优化：多查询 bOPRF，所有 sum 值在 setup 阶段一次性查询

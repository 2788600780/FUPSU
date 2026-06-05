# RS21 (Rindal & Schoppmann, Eurocrypt 2021) — VOLE-PSI: Fast OPRF and Circuit-PSI from Vector-OLE

## 核心贡献

首个将 VOLE + OKVS 结合构造 batched OPRF (bOPRF) 的工作，奠定后续 PSI/PSU 高效协议基础。

## 1. VOLE (Vector Oblivious Linear Evaluation)

- **功能**：Sender 持有 ∆ ∈ F，Receiver 持有向量 x ∈ F^n
- **输出**：Receiver 获得 y，Sender 获得 z，满足 y + z = ∆ · x（逐分量乘积的加法份额）
- **关键性质**：类似 OT 扩展——λ 个 base OT 可生成任意多 VOLE 相关性
- **实例化**：基于 LPN 假设（BCGI18, CM20），或基于 OT（SoftSpokenOT）

## 2. OKVS (Oblivious Key-Value Store)

- **Encode({(k_i, v_i)}) → P**：将 n 个键值对编码为向量 P（长度 m = (1+ε)n）
- **Decode(P, k) → v**：对编码集中的键返回对应值，对其他键返回伪随机值
- **GPR+21 引入**，RS21 采用 PaXoS 实例化（二元线性 OKVS）
- **关键参数**：ℓ₁ = v 的位长（决定伪随机性），通常设 ≥ κ（128 位）

## 3. bOPRF 协议流程

```
Setup: Receiver holds X = {x_1,...,x_n}, Sender holds ∆

1. Receiver:
   - 选择随机值 v_i ←$ {0,1}^ℓ₁ for i=1..n
   - P ← OKVS.Encode({(x_i, v_i)})
   - P 作为 VOLE 的 Receiver 输入

2. VOLE:
   - Receiver 获得 P 的加法份额 share_R
   - Sender 获得 share_S，满足 share_R + share_S = ∆ · P

3. Sender 定义 PRF: F(y) = H(∆ · Decode(P, y) - share_S[y])
   （实际实现中通过 VOLE 份额高效计算）

4. Receiver 对 x ∈ X:
   - 从 VOLE 份额重构 F(x) = H(v_x)

5. Sender 可对任意 y 计算 F(y)（需要知道 P 的 encode 位置）
```

## 4. 性能

- 通信：O(n) VOLE 相关性 + O(n) OKVS 传输
- 计算：O(n) 对称操作（OKVS encode + VOLE 份额计算）
- 相比 KKRT16（OT-based PSI）：约 5-10× 加速

## 5. 安全模型

- **半诚实**：UC 安全，在 {F_VOLE, F_OKVS}-hybrid 模型中
- **恶意**：额外一致性检查，增加 ∼2× 开销
- **关键假设**：OKVS 的伪随机性（ℓ₁ ≥ κ），VOLE 的线性性质

## 6. 对 F-PSU 的意义

- 我们的隐私判等层采用 VOLE+OKVS bOPRF
- 在 Piske & Trieu 的 UnionPeel 协议中，OPRF 用于私密比较 sum_{0,i,j} 和 sum_{1,i,j}
- RS21 是基础，但 RR22 在效率上更优（subfield VOLE + 紧界）

## 7. 局限性（后续工作已改进）

- OKVS 失败概率分析不够紧致 → RR22 改进
- 通信依赖 κ（安全参数）→ RR22 用 subfield VOLE 消除
- 恶意安全证明有缺陷 → Han et al. (Asiacrypt 2024) 指出

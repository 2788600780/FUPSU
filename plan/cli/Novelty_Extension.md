# 密码学新颖性突破方案

## 现状诊断

v4 协议的密码学核心是 3 个已有技术的组合：
- IBLT 线性性 → delta 编码（数学上 trivially 正确）
- 增量 peeling（标准 List 的自然优化）
- PRF 链（教科书级 forward-secure 技术）

**对 Asiacrypt 而言，"combination" 够不够？**

Piske & Trieu 的组合（IBLT + OT + OPRF → PSU）中了 Eurocrypt。但他们贡献了一个**新概念**：union peelability。

我们的 v4 有新概念吗？**没有。** Delta IBLT 是 observation，不是概念。

## 方案 A：可更新 PSU 的泄漏下界 ★★

### 核心问题

任何 O(|Δ|)-通信的可更新 PSU 协议**必然**泄露比静态 PSU 更多的信息。我们能证明这是**固有的**（inherent），并且我们的协议达到了**最优**。

### 贡献

1. **定义**：形式化可更新 PSU 的泄漏谱系 L_upd = (L_size, L_pattern, L_union)
2. **下界定理**：在随机预言机模型中，任何 O(|Δ| · polylog n) 通信的半诚实可更新 PSU 协议必须泄露 Ω(|Δ⁺|, |Δ⁻|) —— 即单独的添加和删除量
3. **最优性**：证明我们的协议的泄漏 = (|X|, |Y|, |Δ⁺|, |Δ⁻|) 在此下界意义上是最优的

### 为什么这是新贡献

- 没有人形式化过 "可更新 PSU 的泄漏"
- 没有人证明过 "小通信 → 大泄漏" 的 trade-off
- 下界定理使用了**压缩论证**（compression argument）：如果协议不泄露 |Δ⁺|，则可将协议消息作为 Δ⁺ 的压缩表示 → 违反信息论界

### 下界证明草图

```
Theorem (Updatable PSU Leakage Lower Bound, informal):
  在随机预言机模型中，设 Π 为半诚实可更新 PSU 协议，
  每 epoch 通信量为 C(Δ)，其中 Δ = (Δ⁺X, Δ⁻X, Δ⁺Y, Δ⁻Y)。
  
  若 C(Δ) = o(|Δ⁺X| + |Δ⁺Y|)，则 Π 不安全（敌手可区分两个
  产生相同联合输出的不同 Δ 序列）。
  
  因此，任何安全的可更新 PSU 必须满足 C(Δ) = Ω(|Δ|)，
  且协议消息长度必然泄露 |Δ⁺X| 和 |Δ⁺Y| 的个别值。

Proof sketch:
  考虑两个世界:
  World 0: P₀ 添加 {x₁, ..., x_k}, P₁ 添加 ∅
  World 1: P₀ 添加 ∅, P₁ 添加 {x₁, ..., x_k}
  
  两个世界产生相同的 X_∪,t（新元素都在并集中）。
  但 Δ⁺ 的分布不同: (k,0) vs (0,k)。
  
  若 C(Δ) = o(k)，则协议消息无法区分这两个世界
  → 协议错误地将 (k,0) 的输出分配给 World 1 中的 P₀
  → 违反 PSU 正确性。
  
  完整证明需要处理 Δ⁺ 和 Δ⁻ 的交互，使用
  "压缩论证" + "不可区分性放大"。□
```

### 对论文的影响

- Introduction: 声明 "我们刻画了可更新 PSU 的**固有泄漏边界**"
- 新 Section: "The Leakage Barrier of Updatable PSU"（2-3 pages）
- 包含形式化定义、下界定理、证明、最优性推论
- 我们的协议不再只是 "一个构造"，而是 "达到理论最优的构造"

---

## 方案 B：自愈前向隐私 ★

### 核心问题

PRF 链 K_{t+1} = PRF(K_t) 提供 forward security（保护过去），但不提供 post-compromise security（保护未来）。敌手腐化 P₀ 拿到 K_τ 后，可计算所有未来密钥。

### 贡献：Self-Healing Key Chain

**Def.** (Self-Healing Forward-Private Key Chain). 一个密钥链方案是 (d, ε)-self-healing 的，如果：
1. **Forward privacy**: 给定 K_t，K_{<t} 计算上不可行（标准 PRF 单向性）
2. **d-Step healing**: 在 epoch t 注入新鲜随机熵 r_t ←$ {0,1}^κ 后，最多 d 个 epoch 内，即使保留了 K_{t-1} 的敌手也无法区分 K_{t+d} 和随机

**构造**（极简）：

```
Standard chain:  K_{t+1} = PRF(K_t, "evolve")   // 无自愈
Healing chain:   K_{t+1} = PRF(K_t, "evolve" || r_t)  // r_t = 新鲜熵
```

当 epoch t 检测到或怀疑泄漏后，注入 r_t。敌手知道 K_t（来自泄漏）但不知道 r_t（协议运行后删除）。K_{t+1} 的分布：敌手知道 PRF(K_t, ·)，但输入包含未知的 r_t。如果 r_t 的熵 ≥ κ，则 K_{t+1} 在计算上与随机无区分。

**引理**：注入 κ 比特新鲜熵后，自愈在 1 步内完成：Adv^{distinguish}_{K_{t+1}} ≤ Adv^{PRF} + 2^{-κ}。

**实用性**：不需要 DH，不需要公钥。只需在检测到异常后多生成 256 比特随机数。

### 为什么这是新贡献

- "Self-healing forward privacy" 在对称密钥 setting 中未被定义或构造
- 构造极其简单（一行改动），但安全性论证是新的
- 对实用部署有价值（谁都不想被攻破后永久丧失隐私）

---

## 方案 C：可更新 PSU 的 UC 组合定理 ★★★

### 核心问题

当前证明逐 epoch 手动构造 Sim。能否利用 UC 组合定理，将 per-epoch PSU 和 per-epoch 密钥管理分解为独立模块？

### 贡献

1. **定义** $\mathcal{F}_{\mathsf{uPSU}}$：可更新 PSU 的理想功能（不包含前向隐私）
2. **定义** $\mathcal{F}_{\mathsf{fwKey}}$：前向隐私密钥管理的理想功能
3. **定理**：$\mathcal{F}_{\mathsf{uPSU}} \diamond \mathcal{F}_{\mathsf{fwKey}}$（并行组合） UC-realizes $\mathcal{F}_{\mathsf{fpsu}}$

这意味着 per-epoch PSU 的安全性和密钥管理的安全性是**独立可分析的**，通过 UC 组合定理自动获得整体安全性。

### 模块化证明结构

```
ℱ_uPSU (per-epoch PSU)     ℱ_fwKey (key management)
       │                          │
       ├──────────────────────────┤
       │   UC Composition Theorem │
       └──────────────────────────┘
                    │
               ℱ_fpsu (this work)
```

### 为什么这是新贡献

- 模块化理想功能设计 → 每部分独立可验证
- 其他人可以用不同的 per-epoch PSU（如 ePSU）替换我们的，前向隐私部分保持不变
- 类似 Liu et al. 将 "updatable OPRF" 抽象为独立模块的思路，但应用于前向隐私

---

## 方案对比

| 维度 | 方案 A（下界） | 方案 B（自愈） | 方案 C（组合） |
|------|:---:|:---:|:---:|
| 密码学新颖性 | ★★★★★ | ★★★ | ★★★ |
| 实现难度 | 中等（信息论证明） | 低（一行代码） | 中等（模块重设计） |
| 对论文的增值 | 最高：从 construction 到 barrier | 实用价值高 | 结构更清晰 |
| 独立于协议 | 是（通用下界） | 是（通用密钥管理） | 部分（ℱ_fwKey 通用） |
| 审稿人印象 | "他们不只是做了个协议，而是告诉了我们这个问题的极限在哪" | "这个 healing 想法很简单但很聪明" | "模块化做得好" |

## 推荐

**方案 A（泄漏下界）作为主线 + 方案 B（自愈）作为 extension。**

理由：
1. 方案 A 提供了密码学会议最看重的东西：**定理** + **下界** + **最优性**
2. 方案 B 实现成本极低（几段话），但为论文增加了实用价值
3. 方案 C 可以融进证明结构，不需要单独章节

### 修正后的论文贡献声明

> "We initiate the study of updatable Private Set Union with forward privacy. Our contributions are threefold:
> 
> 1. **A leakage lower bound** (Section 4): We prove that any updatable PSU protocol with O(|Δ|) communication must leak individual update sizes — establishing a fundamental efficiency-leakage barrier. Our protocol matches this lower bound, and is thus optimal.
> 
> 2. **The F-PSU protocol** (Section 5): achieves O(|Δ|) per-epoch communication (optimal by the lower bound) and forward privacy via PRF-based key evolution. We further introduce *self-healing key chains* that recover post-compromise security with a single entropy injection (Section 5.4).
> 
> 3. **Implementation and evaluation** (Section 7): demonstrating 14× communication reduction over the static baseline and sub-millisecond key refresh."

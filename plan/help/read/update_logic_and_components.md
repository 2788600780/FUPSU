# 更新逻辑详解与前沿组件

## 一、更新逻辑全景

F-PSU 的更新逻辑分为**三层**，分别解决"如何高效更新 IBLT"、"如何加速 PSU 计算"、"如何保证安全性跨越 epoch"。

### 1.1 三回合 Epoch 协议

```
┌─────────────────────────────────────────────────────┐
│ Round 1: 本地 Delta IBLT 更新（计算层）               │
│  ・解密上一 epoch 存储的 IBLT                          │
│  ・利用 IBLT 线性性：插入 Δ⁺，删除 Δ⁻                 │
│  ・标记 k·|Δ| 个被修改的 cell 作为初始剥离队列 Q^(0)     │
│  ・复杂度：O(k·|Δ|) 次模加运算                          │
├─────────────────────────────────────────────────────┤
│ Round 2: UnionPeel 增量剥离（通信层）                   │
│  ・仅对 Q^(0) 中的 cell 发起 OT/OPRF 查询               │
│  ・迭代剥离 + 级联传播直到 Q^(r) = ∅                    │
│  ・复杂度：O(k·|Δ|·(σ+κ)) bits 通信                    │
├─────────────────────────────────────────────────────┤
│ Round 3: 密钥演化 + 安全存储（前向隐私层）               │
│  ・K_t ← PRF(K_{t-1})，加密 IBLT，擦除 K_{t-1}         │
│  ・复杂度：1 次 PRF + O(m) 次流密码加密，零额外通信      │
└─────────────────────────────────────────────────────┘
```

### 1.2 Round 1 详解：Delta IBLT 更新

**核心原理**：IBLT 编码具有**线性性**（源自 XOR 和 Count 域加法）：

```
对于互不相交的集合 A, B:

  Encode(A ∪ B)  = Encode(A) ⊕ Encode(B)        ← XOR 线性
  Encode(A \ B)  = Encode(A) ⊖ Encode(B)        ← 减法线性
```

每个己方维护自己集合的 IBLT（而非联合 IBLT），因此：

```
IBLT_C^(t) = IBLT_C^(t-1)           // 解密上一轮的密文
           ⊕ Encode(Δ⁺ X_t)        // 插入新增元素（每个元素影响 k 个 cell）
           ⊖ Encode(Δ⁻ X_t)        // 删除移除元素

被修改的 cell 坐标: Q^(0) = {(i, h_i(x)) : x ∈ Δ⁺ X_t ∪ Δ⁻ X_t, i ∈ [k]}
```

**关键节约**：修改 k|Δ| 个 cell vs 重新编码整个 k|X| 个 cell → 节约 n/d 倍。

### 1.3 Round 2 详解：增量剥离（Incremental Peeling）

这是整个协议中最关键的性能优化。

**传统剥离（Piske & Trieu）**：
```
Q^(0) = 全部 k·ℓ_t 个 cell  // ℓ_t 随 |X|+|Y| 增长
每次迭代 → OT/OPRF 查询全部 cell → 级联传播
```

**增量剥离（本文）**：
```
Q^(0) = 仅 k·|Δ| 个被修改的 cell  // 与 |X| 独立
每次迭代 → 仅查询 Q^(r) 中 cell → 级联传播
```

**正确性依据**（Cascade Bound Lemma, §4.3）：

1. **前置条件**：上一个 epoch 结束后，所有已恢复元素已从双方 IBLT 中删除（DeleteSet）
2. **2-core 为空**：此时每个 cell 的 cnt ∈ {0, ≥2}（没有 cnt=1 的可剥离 cell），且 2-core 以极大概率（1 - 2^{-λ}）为空
3. **Delta 效应**：新增元素修改 cell → 部分 cell 可能从 cnt ∈ {0, ≥2} 变为 cnt=1（变得可剥离）
4. **级联边界**：剥离从 k|Δ| 个修改 cell 开始，检查的 cell 总数 ≤ (1 + C_0) · k|Δ| ≤ 2 · k|Δ|，其中 C_0 = O(k²|Δ|/m) 是 delta 元素形成的 mini-2-core 因子

**实验结果验证**（§6）：
- Cascade 从未超过 2 × k|Δ|
- 100% 剥离成功率（无 List 失败）
- 2.6x 比全 IBLT 剥离更快

### 1.4 Round 3 详解：密钥演化

```
算法 Per-Epoch-Key-Evolution:
  输入: 当前密钥 K_{t-1}^C
  输出: 新密钥 K_t^C，密文 CT^C_stored

  1. K_t^C ← PRF(K_{t-1}^C, "fpsu-evolve-C")    // HMAC-SHA256
  2. N_C ← fresh_nonce()                          // 96-bit unique nonce
  3. CT_stored^C ← ChaCha20.Enc(K_t^C, N_C, IBLT_C)
  4. secure_erase(K_{t-1}^C)                      // 从内存中安全移除
  5. return (K_t^C, CT_stored^C)
```

**安全擦除的实际约束**：
- 在 RAM 模型中：覆盖内存区域 → 立即生效
- 在现实中：依赖操作系统/语言的 `zeroize` 机制
- 论文诚实声明了 Python 原型中的局限性（Python 的 GC 不保证即时释放）

---

## 二、前沿组件拆解

### 2.1 组件全景图

```
                   ┌──────────────┐
                   │  F-PSU 协议   │
                   └──────┬───────┘
          ┌───────────────┼───────────────┐
    ┌─────┴─────┐  ┌─────┴─────┐  ┌─────┴─────┐
    │  数据层    │  │  计算层    │  │  安全层    │
    │  IBLT      │  │ UnionPeel │  │ PRF Chain │
    └─────┬─────┘  └─────┬─────┘  └─────┬─────┘
          │        ┌──────┴──────┐       │
          │   ┌────┴───┐  ┌─────┴────┐  │
          │   │  OT    │  │  OPRF   │  │
          │   └────┬───┘  └────┬────┘  │
          │        │           │       │
          │   ┌────┴───────────┴────┐  │
          │   │  VOLE + OKVS (RR22) │  │
          │   └─────────────────────┘  │
          │                            │
          │   ┌────────────────────────┘
          │   │
    ┌─────┴───┴─────┐
    │ ChaCha20 +    │
    │ HMAC-SHA256   │
    └───────────────┘
```

### 2.2 组件详解

#### IBLT（Invertible Bloom Lookup Table）

| 属性 | 说明 |
|------|------|
| 论文来源 | Goodrich-Mitzenmacher (2011), 由 Piske & Trieu (EC26) 引入 PSU |
| 核心操作 | `Encode(S)`, `Decode`, `Delete(x)` |
| 关键性质 | **线性性**：Encode(A∪B) = Encode(A) ⊕ Encode(B) |
| 参数 | m cells, k hash functions, σ-bit values |
| 成功率 | Pr[List 成功] ≥ 1 - 2^{-λ}，选取 m 和 k 使膨胀因子 e = km/n ∈ [1.5, 4.5] |
| 在本文中的角色 | Delta IBLT 更新的基础数据结构 |

#### VOLE（Vector Oblivious Linear Evaluation）

| 属性 | 说明 |
|------|------|
| 论文来源 | RR22 (Rindal-Raghuraman, Eurocrypt 2022) |
| 功能 | 生成满足 a·b = c + d 的随机值，其中一方持有 a,c，另一方持有 b,d |
| 开销 | κ 次基础 OT + 对称密码运算（摊销后每个 VOLE 开销极低） |
| 在本文中的角色 | 高效实例化 OT 和 OPRF 的基础设施 |
| 重用性 | **一次性 setup，跨所有 epoch 重用**（这是 update 效率的关键前提） |

#### OKVS（Oblivious Key-Value Store）

| 属性 | 说明 |
|------|------|
| 论文来源 | RR22, Garimella et al., Bienstock et al. |
| 功能 | 编码 n 个 key-value 对为数据结构 S，使 Decode(S, key) = value |
| 关键性质 | 对不在编码集中的 key，返回值统计独立于所有被编码的 values |
| 在本文中的角色 | 与 VOLE 结合构造 bOPRF |

#### bOPRF（batch Oblivious PRF）

| 属性 | 说明 |
|------|------|
| 论文来源 | RR22, Piske & Trieu (EC26) |
| 功能 | 批量评估 OPRF：S 输入编程表，R 输入查询集合，R 获得 PRF 输出 |
| 安全性 | S 不知道 R 的查询，R 不知道 S 的编程（除输出外） |
| 在本文中的角色 | Round 2 中实现私密 tag 比较（相等元素产生相等 tag） |

#### OT（Oblivious Transfer）

| 属性 | 说明 |
|------|------|
| 在本文中的角色 | Round 2 中两处使用： |
|  | (1) P0 输入选择比特 c_{i,j}（cnt=0?），获取 P1 的 sum 值 |
|  | (2) 根据标签比较结果选择 peeled element 或 ⊥ |
| 实例化 | 通过 VOLE 扩展，摊销后几乎零开销 |

#### ChaCha20（流密码 / PRG）

| 属性 | 说明 |
|------|------|
| 论文来源 | Bernstein (2008), RFC 8439 |
| 安全性 | IND-CPA 安全（作为流密码），用作 PRG 假设 |
| 在本文中的角色 | |IBLT|·σ 位数据加密 → 每条消息只需 1 次 ChaCha20 初始化 + m 个 block |
| 选择理由 | 软件实现极快（AES-NI 非必需），常数级优于 AES |

#### HMAC-SHA256（PRF）

| 属性 | 说明 |
|------|------|
| 论文来源 | Bellare-Canetti-Krawczyk (1996) |
| 安全性 | 在 PRF 假设下安全（HMAC 不依赖 RO 模型） |
| 在本文中的角色 | K_{t+1} ← HMAC(K_t, "fpsu-evolve-b") |
| 选择理由 | Standard Model 假设（无 Random Oracle），广泛部署 |

### 2.3 组件的"前沿性"评估

| 组件 | 前沿程度 | 说明 |
|------|---------|------|
| IBLT | ★★★☆☆ | 2011 年提出，但在 PSU 中应用是 2024/2026 新发展（Piske & Trieu） |
| VOLE-based OT/OPRF | ★★★★★ | RR22 是 SOTA，是当前最高效的 OT/OPRF 实例化路线 |
| Delta IBLT（线性编码） | ★★★★☆ | 利用线性性做增量更新是已知思想，但首次在 PSU 场景中系统化应用 |
| Incremental Peeling | ★★★★★ | **原创机制**，从 delta 修改 cell 启动剥离 |
| PRF Key Chain | ★★★☆☆ | 经典技术（Signal 协议），但在可更新 PSU 中首次应用 |
| ChaCha20 + HMAC | ★★★☆☆ | 成熟标准组件，选择理由清晰（标准模型 + 软件高效） |

### 2.4 前沿着力点：VOLE 生态

F-PSU 最深的前沿依赖是 **VOLE 生态**（RR22）：

```
κ 次基础 OT（一次性）
      │
      ▼
  VOLE 实例（可重用）
      │
      ├──→ SoftSpokenOT（承诺 OT）
      │         │
      │         ▼
      │    Piske & Trieu 的 UnionPeel（Round 2）
      │
      └──→ bOPRF（OKVS-based）
                │
                ▼
          UnionPeel 中的私密 tag 比较（Round 2）
```

**VOLE 的重用性是 F-PSU 更新效率的密码学基础**：如果每个 epoch 都需要重新做 κ 次公钥操作，则 "O(|Δ|) per epoch" 的效率目标将无法实现。RR22 的设计使一次 VOLE setup 的摊销开销极低。

---

## 三、更新逻辑中的设计巧思

### 3.1 "修改了什么就查什么"

传统 PSU（静态）：需要检查全部 k|X∪Y| 个 cell → O(n) 通信
F-PSU（增量）：只检查上次剥离后"变了"的 k|Δ| 个 cell → O(|Δ|) 通信

**前提条件**：必须证明从 k|Δ| 个 cell 启动剥离就能恢复**全部**新增并集元素。这依赖：
1. 上一 epoch 的 2-core 为空（协议保证）
2. Delta 操作不会创建大型 2-core（Cascade Bound Lemma）

### 3.2 "密钥只管向前走"

PRF 链设计的精妙处：
- 不需要双方同步密钥（每方独立维护自己的 IBLT 和自己的密钥链）
- 不需要额外通信（本地 PRF + 加密）
- 前向安全是"顺便"得到的（只需在加密后擦除旧密钥）

### 3.3 "泄露最少即可"

下界定理证明：|Δ⁺| 和 |Δ⁻| 的泄露**不可避免**。F-PSU 精准泄露这些（通过 delta IBLT 大小）而不泄露更多（如元素身份、历史集合等），达到了**泄露最优**（leakage-optimal）。

---

## 四、总结

F-PSU 的更新逻辑围绕一条主线：**利用 IBLT 线性性将 O(n) 的更新 + 重新 PSU 降为 O(|Δ|)**。前沿技术栈（VOLE + OKVS + bOPRF + OT）为 Round 2 提供高效的私密计算；PRF 密钥链（ChaCha20 + HMAC）为 Round 3 提供零通信前向隐私。整体设计贯穿"只修改必要部分"的增量思维。

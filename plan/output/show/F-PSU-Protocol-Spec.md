# F-PSU 协议完整规范

## Part 0: 数学基础

### 0.1 IBLT 数据结构

IBLT 参数 `prm = (M, k, ℓ, H)`：
- `M` — 模数（通常 M=2^64）
- `k` — 哈希函数个数（k=5）
- `ℓ` — 每个子表的 cell 数
- `H = {h_i}` — k 个哈希函数，h_i: U → [ℓ]
- `m = k × ℓ` — 总 cell 数

每个 cell 存两个值：`sum` (模 M) 和 `cnt` (整数)。

**核心性质（线性）**：
```
Encode(A ∪ B) = Encode(A) ⊕ Encode(B)
Encode(A \ B) = Encode(A) ⊖ Encode(B)
```

### 0.2 IBLT 原子操作

**encode(x)**：对 k 个哈希位置，`sum += x mod M`, `cnt += 1`。

**subtract(x)**：对 k 个哈希位置，`sum -= x mod M`, `cnt -= 1`。

**is_singleton(i, j)**：`cnt[i][j] == 1`。

**peel(i, j)**：如果 cnt=1，取出 x = sum[i][j]，然后从所有 k 个 cell 中 subtract(x)。

### 0.3 Union Peel (uPeel)

PT26 的核心贡献。两个 IBLT 不合并就能联合剥离。

```
uPeel(iblt_A, iblt_B, i, j):

Case 1: cnt_A=1, cnt_B=0 → A 独有的元素
Case 2: cnt_A=0, cnt_B=1 → B 独有的元素
Case 3: cnt_A=1, cnt_B=1, sum相等 → 共享元素（交集）
Other: ⊥ (无法剥离)
```

**安全实现**：用 OT 取 sum，用 OPRF 做相等性测试。

### 0.4 密码学原语

- **PRF**：HMAC-SHA256(K, m)
- **ChaCha20**：流密码。C = P ⊕ keystream(K, nonce)
- **OT (1-out-of-2)**：发送方 (m₀,m₁)，接收方 b ∈ {0,1}，获得 m_b
- **OPRF**：发送方有 PRF key k，接收方有查询 x，获得 F(k,x)

---

## Part 1: 一次性 Setup

### 1.1 VOLE 初始化
双方运行 RR22 VOLE 协议，为后续所有 epoch 的 batched OPRF 提供基础。一次性，所有 epoch 复用。

### 1.2 初始密钥
```
P0: K₀^C ←$ {0,1}^256
P1: K₀^S ←$ {0,1}^256
```

### 1.3 IBLT 参数
```
k = 5, e = 2.0, M = 2^64
ℓ = max(64, ⌈e × (|X|+|Y|) / k⌉)
m = k × ℓ
```

---

## Part 2: Epoch 0（首次全量 PSU）

**目的**：计算 X₀ ∪ Y₀，并建立共享元素 IBLT 供后续增量使用。

### Step 1: 编码全集
```
P0: for x ∈ X₀: iblt_C.encode(x)
P1: for y ∈ Y₀: iblt_S.encode(y)
```

### Step 2: 全扫描 UnionPeel
```
扫描 iblt_C 和 iblt_S 的所有 k×ℓ 个 cell
循环调用 uPeel，cascade 直到无法剥离
→ 恢复 U₀ = X₀ ∪ Y₀
→ 每个元素附标签：P0独有 / P1独有 / 共享
```

### Step 3: 分类元素
```
共享 S₀ = {x | cnt_C=1, cnt_S=1, sum相等}
P0 独有 = {x | cnt_C=1, cnt_S=0}
P1 独有 = {x | cnt_C=0, cnt_S=1}
```

### Step 4: 重建共享 IBLT
```
// 从 Epoch 1 开始，IBLT 只存共享元素
P0: iblt_C ← Encode(S₀)
P1: iblt_S ← Encode(S₀)
// 独有元素本地管理，不进 IBLT
```

### Step 5: Key Evolution
```
P0: K₁^C ← PRF(K₀^C, "evolve-C-00")
    CT_C ← ChaCha20.Enc(K₁^C, iblt_C)
    secure_erase(K₀^C)   // 旧密钥立即销毁

P1: K₁^S ← PRF(K₀^S, "evolve-S-00")
    CT_S ← ChaCha20.Enc(K₁^S, iblt_S)
    secure_erase(K₀^S)
```

---

## Part 3: Epoch t > 0（增量更新）

### 输入状态
```
P0: X_{t-1}, K_{t-1}^C, CT_C, Δ⁺_C, Δ⁻_C
P1: Y_{t-1}, K_{t-1}^S, CT_S, Δ⁺_S, Δ⁻_S
共享: U_{t-1} (上轮并集)
|Δ| = |Δ⁺| + |Δ⁻| ≪ |X| + |Y|
```

---

### Step 2.1: 解密

```
P0: iblt_C ← ChaCha20.Dec(K_{t-1}^C, CT_C)
P1: iblt_S ← ChaCha20.Dec(K_{t-1}^S, CT_S)
// iblt_C 和 iblt_S 存的是 S_{t-1}（上轮共享元素）
// 成本: < 5ms
```

---

### Step 2.2: OT-masked Deletion（核心创新）

#### 2.2.1 P0 侧删除

对 Δ⁻_C 中每个 x，P0 通过 OT 问 P1："(1)你有 x 吗？(2)你也要删 x 吗？"

**Case 1：P1 没有 x（x 是 P0 独有）**
```
IBLT：不动（x 不在共享集里）
并集：U ← U \ {x}
P0 自集：X ← X \ {x}
```

**Case 2：P1 有 x，P1 不删（共享，对方保留）**
```
x 从"共享"变为"P1 独有"
IBLT：iblt_C.subtract(x)（k=5 cell 操作，无 cascade）
IBLT_S：不动
并集：x 保留（P1 还留着）
P0 自集：X ← X \ {x}
```

**Case 3：P1 有 x，P1 也删（共享，双方都删）**
```
IBLT：iblt_C.subtract(x)，iblt_S.subtract(x)
记录 cell 到 Q_del 供 Step 2.6 清理
并集：U ← U \ {x}
双方自集都删除 x
```

#### 2.2.2 P1 侧删除（对称操作）

P1 对自己的 Δ⁻_S 做同样操作，角色互换。

#### 2.2.3 Case 分布（实验数据）

n=1M, |Δ|=1024：Case 1≈70%, Case 2≈29%, Case 3<2%

**>98% 的删除不触发 IBLT cascade。**

---

### Step 2.3: Mini UnionPeel for Additions

双方各自把新增编码到**独立小 IBLT**（大小 ∝ |Δ⁺|）：

```
P0: iblt_ΔC (k=5, ℓ∝|Δ⁺|)，encode(Δ⁺_C)
P1: iblt_ΔS (k=5, ℓ∝|Δ⁺|)，encode(Δ⁺_S)
```

在这两个小 IBLT 上跑全扫描 UnionPeel：
```
→ V_Δ⁺ = Δ⁺_C ∪ Δ⁺_S
→ 分类：共享新增 / P0独有新增 / P1独有新增
→ 零 cascade（IBLT 是全新的，无历史状态）
```

成本：O(|Δ⁺|) cells，~80|Δ⁺| bytes。

---

### Step 2.4: Merge Additions

```
共享新增 → iblt_C.add(x), iblt_S.add(x)
P0 独有新增 → iblt_C.add(x)
P1 独有新增 → iblt_S.add(x)

U_tmp ← U_tmp ∪ Δ⁺_C ∪ Δ⁺_S
```

---

### Step 2.5: 构建新并集

```
U_t = U_{t-1}
      \ {P0 Case 1 独有删除}
      \ {P1 Case 1 独有删除}
      \ {Case 3 双方都删共享}
      ∪ {Δ⁺_C} ∪ {Δ⁺_S}

// Case 2 元素不删——对方还留着
```

---

### Step 2.6: Case 3 清理（如有）

```
如果 Q_del 非空：
  在 iblt_C, iblt_S 上扫描 Q_del
  → Π_UnionPeel 清理 cascade 残留

// >98% epoch 此步为空
```

---

### Step 2.7: Key Evolution

```
P0: K_t^C ← PRF(K_{t-1}^C, "evolve-C-" || t)
    CT_C ← ChaCha20.Enc(K_t^C, iblt_C)
    secure_erase(K_{t-1}^C)

P1: K_t^S ← PRF(K_{t-1}^S, "evolve-S-" || t)
    CT_S ← ChaCha20.Enc(K_t^S, iblt_S)
    secure_erase(K_{t-1}^S)

// 前向安全链:
// K₀ ─PRF→ K₁ ─PRF→ K₂ ─PRF→ ... ─PRF→ K_t
//  erased   erased   erased             kept
//
// t 时刻妥协: 拿到 K_t，只能解密当前 IBLT
// 历史 K_{<t} 已被擦除 + PRF 单向性 → 历史数据不可恢复
```

---

## Part 4: 复杂度

### 每 epoch 成本明细

| 步骤 | 成本 (n=1M, &#124;Δ&#124;=1024) |
|------|:--|
| 解密 | < 5ms |
| OT 删除判定 | ~3ms |
| Case 2 subtract | < 0.1ms |
| MiniPeel | ~40KB 通信 |
| Merge | < 1ms |
| Case 3 清理 | ~0 |
| Key Evolution | < 1ms |
| **总计** | **13.9MB, 0.43s** |

### 与静态 PSU 对比

| n | &#124;Δ&#124; | E0 Cells | E>0 Cells | 缩减 |
|---|------|:---:|:---:|:---:|
| 2^16 | 256 | 819K | 1,790 | 458x |
| 2^16 | 4096 | 819K | 28,670 | 29x |
| 2^18 | 256 | 3.28M | 1,790 | 1,831x |
| 2^18 | 4096 | 3.28M | 28,670 | 114x |
| 2^20 | 256 | 13.1M | 1,790 | 7,322x |
| 2^20 | 4096 | 13.1M | 28,670 | 457x |

---

## Part 5: 安全性

### 每 epoch 安全
继承 PT26：Π_UnionPeel UC-realizes F_UnionPeel in (F_OT, F_OPRF)-hybrid model，对抗半诚实自适应敌手。

### OT 成员判定安全 (Lemma 3)
OT 查询只泄露：(a) &#124;Δ⁻_C&#124;, &#124;Δ⁻_S&#124;，(b) x∈Y? 和 x∈Δ⁻_S?。两项均由最终并集 U_t 所隐含。

### 前向安全 (FP-IND)
PRF 链 + 即时擦除 → 妥协者无法解密历史 IBLT 密文。区分优势 ≤ T × Adv_PRF ≤ negl。

### 泄漏下界 (Theorem 6)
任何 1-updatable PSU 协议必须泄漏每方的 &#124;Δ⁺&#124; 和 &#124;Δ⁻&#124;。F-PSU 恰好泄这四个值，零额外泄漏——**严格泄漏最优**。

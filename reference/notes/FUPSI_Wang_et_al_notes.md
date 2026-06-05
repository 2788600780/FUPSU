# FUPSI (Wang et al., TIFS 2024) — Updatable PSI with Forward Privacy

## 核心贡献

首个同时支持添加和删除的可更新 PSI + 前向隐私。用 keyword PIR 隐藏中间参数。

## 1. 攻击：为何删除在先前 UPSI 中不可行

### 攻击场景 (Section III-C)
- UPSIα (Badrinarayanan et al. 的"UPSI with addition"扩展到删除)
- P₀ 和 P₁ 同时删除元素 m（m 在上一个 epoch 的交集中）
- P₀ 观察到新交集不包含 m → 推断 P₁ 也删除了 m
- **这泄露了 P₁ 的操作信息**，违反 PSI 隐私

### 形式化
- 旧交集 = {2, 4}，双方都删除 2，新交集 = {4}
- P₀ 推理：旧交集中的元素 2 不见了，且 P₀ 自己删除了 2 → P₁ 必然也删除了 2
- 应用场景：垂直联邦学习中的样本对齐 → 泄露哪一方删除了用户

## 2. 前向隐私定义 (Definition 2)

```
L_Updt(op, (x, y)) = L_I(X, Y)
```

更新操作的泄漏仅等于执行标准 PSI 的泄漏（交集本身），不额外泄露"谁做了什么操作"。

对比 SSE 中的前向隐私（Definition 1）：SSE 中，更新泄漏函数 L_Updt 只能泄露 (indi, μi)（修改的文档和关键词数量），不能泄露具体关键词。

## 3. FUPSI 协议

### 核心组件
- **SparsePIR** (Sarvar et al.): keyword PIR 框架，将关键字-值对编码为 d1×b 矩阵，通过分治将查询归约为标准 PIR
- **Encode**: 将 D = {(x_i, flag_i)} 编码为矩阵 E
- **Query/Answer**: 标准 PIR 流程

### 协议流程
```
FUPSI.Init: X ← ∅, Y ← ∅, 输出初始密钥 (ck, sk)
FUPSI.Update:
  1. X_old ∪ X_upd, Y_old ∪ Y_upd
  2. 三方交集分解: I_old, X_upd ∩ Y, Y_upd ∩ X
  3. P₀ 作 Server, P₁ 作 Client → XPSI(Y_upd, X)
  4. P₁ 作 Server, P₀ 作 Client → XPSI(X_upd, Y)
  5. 交换 I_x, I_y → I = I'_old ∪ I_x ∪ I_y
  6. 删除 flag=0 的元素
```

### Flag 机制
- flag=1: 添加
- flag=0: 删除
- 数据库存储 (element, flag) 对，PIR 查询时返回 flag

### 前向隐私如何实现
- **PIR 隐藏查询**：查询方通过 PIR 查询 (x, flag) → 服务器不知道查询了哪个 x
- 旧查询参数不能链接到新查询（PIR 的 query privacy）
- 即使半诚实服务器存储了所有历史 PIR 请求，也无法推断更新操作

## 4. 安全证明

- 半诚实模型，adaptive security
- G₀ (Real) → G₁ (替换 rep 为随机) → G₂₋ₐ (替换 F 输出为随机) → G₂₋₆ (替换 PIR 响应)
- 最终 G₃ 中敌手无法获得除输出交集外的任何信息
- 依赖：PIR 的 query privacy + RO 模型

## 5. 性能

- **通信**: O(log |update_set|) — 对数级！（远优于我们 IBLT 的 O(m)）
- **计算**: O(|update_set| · d1 + d1³/b), 主要来自 PIR
- 低速网络（1Mbps）中比标准 PSI 快 2-10×

## 6. 与 F-PSU 的对比

| 维度 | FUPSI | F-PSU (本文) |
|------|-------|-------------|
| 原语 | Updatable PSI | Updatable PSU |
| 输出 | 交集 | 并集 |
| 前向隐私机制 | PIR (query hiding) | PRF 链 + XOR 同态刷新 |
| 数据结构 | SparsePIR 编码矩阵 | IBLT |
| 通信复杂度 | O(log |Δ|) | O(|Δ|) |
| 密钥管理 | 无密钥链 | K_{t+1}=PRF(K_t) |
| 安全假设 | PIR + RO | PRF 单向性 + ChaCha20 PRG |
| 添加+删除 | ✅ (via flag) | ✅ (via IBLT DeleteSet) |
| 适应性安全 | ✅ | 需要考虑（Liu et al. adaptivity 问题）|

## 7. 对我们的启示

1. **前向隐私定义**：可借鉴 FUPSI 的 Definition 2 结构（L_Updt = L_I），简化为游戏语言
2. **PIR 路线 vs PRF 路线**：FUPSI 以更好的通信复杂度（log vs linear）换取 PIR 的计算开销。我们的 PRF+XOR 路线更轻量但通信更大
3. **Adaptivity**：FUPSI 声明 adaptive security，通过 PIR 的 query privacy 天然支持。我们需要论证 IBLT + PRF 也能满足 adaptivity
4. **差异化**：我们说"first forward-private updatable PSU"，对比 FUPSI 是 PSI 不是 PSU，且 PIR 路线与 IBLT 路线不同

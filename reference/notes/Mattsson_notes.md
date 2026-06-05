# Mattsson (ePrint 2024/220) — Security of Symmetric Ratchets and Key Chains

## 核心贡献

对 TLS 1.3, Signal, PQ3, MLS 等协议中使用的对称 ratchet 和密钥链进行系统安全分析，发现弱密钥、高碰撞率和 TMTO 攻击。

## 1. 分析对象

- **对称 ratchet**：K_{i+1} = HMAC(K_i, "constant" || data_i)
  - 典型场景：Signal 的对称棘轮，TLS 1.3 的密钥更新
- **密钥链**：K_{i+1} = HMAC(K_i, c)（单向，无外部输入）
  - 我们的场景

## 2. 主要发现

### 2.1 弱密钥问题
- HMAC 的密钥空间中存在"弱密钥"：某些 K 使得 HMAC(K, ·) 的输出分布不均
- 虽然比例很小（~2^{-κ}），但 ratchet 链经过多轮后遇到弱密钥的概率不可忽略
- Signal/TLS 的 ratchet 涉及多方向、多轮更新，放大风险

### 2.2 密钥碰撞率高于生日界
- 生日界预测 O(2^{κ/2}) = O(2^{64}) 次更新后出现碰撞
- 实际中由于 ratchet 链的结构，碰撞率可能高出数个数量级
- 原因：链的中间状态形成"瓶颈"，减少有效熵

### 2.3 TMTO (Time-Memory Trade-Off) 攻击
- 复杂度约 N^{1/4}（而非预期的 N^{1/2}）
- 攻击利用 ratchet 链的前缀结构预计算 rain table
- 对于 κ=128 的协议，实际安全可能降至 ~32 位

## 3. 受影响协议

| 协议 | 风险级别 | 原因 |
|------|----------|------|
| TLS 1.3 | 中等 | 多向 ratchet，但更新频率低 |
| Signal | 较高 | 频繁对称棘轮，多设备分支 |
| PQ3 (Apple) | 高 | 后量子 + 传统 ratchet 组合，碰撞面大 |
| MLS | 较高 | 群组密钥树，大量分支 |

## 4. 我们的场景：单链 PRF

```
K_{t+1} = PRF(K_t, "fpsu-key-evolve")
```

### 4.1 为何不受影响

1. **单链无分支**：
   - Mattsson 攻击依赖多方向/多设备分支间的碰撞
   - 我们的密钥链是严格线性的：K_1 → K_2 → ... → K_T
   - 每个 epoch 只有一条路径，无碰撞面

2. **确定性更新**：
   - K_{t+1} = PRF(K_t)，无外部 entropy 注入
   - 密钥空间始终为 {0,1}^κ，无"缩小"
   - TMTO 需要可预测的中间状态，我们的链仅需单向安全性

3. **更新频率低**：
   - 每个 epoch 一次更新（非每条消息）
   - 实际部署中 T ≤ 10^4（而非 Signal 的每条消息）

### 4.2 我们仍需说明的 caveat

1. **PRF 选择**：推荐 HMAC-SHA256 或 HKDF（而非直接 SHA256）
2. **Salt 考虑**：可加 epoch 计数器作为 salt，防御潜在的弱密钥累积
3. **不适用 Mattsson 的 TMTO 模型**——我们的链是单向的，敌手只能正向计算

## 5. 引用策略

在论文中：
- **Preliminaries**：简单提及 Mattsson 的分析，说明其范围（多方 ratchet）
- **Protocol**：论证 PRF 单链在其分析范围外
- **Security Proof**（Task 3.6）：正式归约——若存在敌手攻破前向隐私，则可构造敌手攻破 PRF 单向性
- **Discussion**：即使未来发现单链弱点，可升级到 HKDF 带 salt 版本

## 6. 推荐实例化

```python
# Current (simple):
K_{t+1} = HMAC-SHA256(K_t, "fpsu-key-evolve-v1")

# Conservative (if reviewer asks):
K_{t+1} = HKDF-Expand(K_t, info="fpsu-key-evolve-v1" || t, length=256)
```

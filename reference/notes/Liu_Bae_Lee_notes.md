# Liu, Bae, Lee (ePrint 2025/2087) — Leakage-Free Enhanced PSU

## 核心贡献

在 Tu et al. (USENIX 2025) ePSU 框架上做两件事：
1. 识别并修复 **hash-based leakage**（直接哈希分桶导致的额外泄漏）
2. 设计首个通信量仅依赖小集合大小的非平衡 ePSU

## 1. Hash-Based Leakage

- 问题来源：在 ePSU 中直接对输入集合做 Cuckoo/Simple hashing → Receiver 获取 X\Y 后，可推断其简单哈希表中哪些项不匹配
- 修复方案：OPRF 加密集合 + Shuffle → 打破哈希位置和元素的关联
- 与 Chandran et al. 的方案类似但结合了 ePSU 安全

## 2. 平衡 ePSU 优化

- 使用**双向 OKVS** (bidirectional OKVS) 替代单向
- 对空 Cuckoo bin 生成随机值，不编码空项 → 减少冗余
- 相比修正后的 Tu et al. 平衡 ePSU：通信 1.1-3.0×, 速度 1.2-1.6×

## 3. 非平衡 ePSU (核心贡献)

- 新原语 **OECRG** (Oblivious Equality-Conditional Randomness Generation)
- 基于 HPS + RSA 假设
- **通信 O(m) = 仅依赖小集合大小**！此前最先进的 Tu et al. 仍为 O(m log n)
- |X|=2^10, |Y|=2^20: 通信缩小 1.5-45.8×, 速度 1.3-6.7×

## 4. 双重泄漏防护

| 泄漏类型 | 原 ePSU | 本文 |
|----------|---------|------|
| During-execution leakage | ✅ 已防 | ✅ 已防 |
| Hash-based leakage | ❌ 存在 | ✅ 修复 |

## 5. 对 F-PSU 的影响

- 间接相关：进一步确认了 hash-to-bin 技术导致的隐私泄漏
- 我们的 IBLT 方案不使用 Cuckoo/Simple hashing 对输入直接编码 → 天然免疫 hash-based leakage
- 可作为 related work 中对比 ePSU 进展时的引用
- OECRG 方案在非平衡场景下的线性通信是重要参考

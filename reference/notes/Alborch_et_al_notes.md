# Alborch et al. (ePrint 2025/2147) — UPSI and Beyond via Circuit-PSI

## 核心贡献

识别现有可更新 PSI 在同时支持添加和删除时的隐私泄漏，提出用 **circuit-PSI + secure shuffling** 修复。

## 1. 识别的泄漏类型

### Leakage 1: 子问题大小泄漏
- 将大集合拆分为小集合做 circuit-PSI 时，每个子问题的输入大小泄露信息
- 例子：联系追踪中，某用户删除某联系人 → 对应 epoch 的 PSI 输入变化 → 观察者可推断删除行为
- 修复：padding 使所有子问题大小恒定

### Leakage 2: 重构输出泄漏
- 将各 circuit-PSI 输出重新组合时，敌手可能推断哪个输出来自哪个子问题
- 结合添加/删除操作的时间信息，可追踪特定联系人的存在/离开时间
- 修复：secure shuffling 打乱输出顺序

## 2. 技术方案

```
框架: Split → Circuit-PSI → Shuffle → Recombine

1. 双方将各自集合拆分为 epoch-indexed 子集合
2. 对每个 epoch pair 执行 circuit-PSI（输出 secret shares）
3. Secure shuffling 打乱 shares 顺序
4. 重组为最终交集输出
```

## 3. 为什么用 Circuit-PSI 而非直接 PSI？

- Circuit-PSI 输出 secret shares 而非明文交集
- Shares 可在 2PC 中继续计算（如 cardinality）
- Shuffle 后的 shares 不暴露对应关系

## 4. 适用场景

- UPdatable PSI（交集本身）
- Updatable Cardinality-PSI（交集大小）
- Circuit-PSI → 可扩展为任意 f(X ∩ Y)

## 5. 安全模型

- 半诚实 (honest-but-curious)
- 静态腐化
- 支持单方输出（client-server 模式）

## 6. 对 F-PSU 的影响

- 相关但不重叠：他们解决的是 UPSI 中**添加+删除操作的可观察模式泄漏**，我们解决的是**跨 epoch 密钥腐化后的历史数据泄漏**
- 他们的"shuffle circuit-PSI outputs"思路与我们"XOR key refresh"思路完全不同
- 作为 related work 引用，展示 UPSI 领域在识别新型泄漏方面的进展
- 我们协议设计时需注意：如果支持删除操作，确认是否存在类似的操作模式泄漏

# Han et al. (Asiacrypt 2024) — OKVS-based OPRF 安全性再审视

## 论文基本信息
- 标题: Revisiting OKVS-based OPRF and PSI: Cryptanalysis and Better Construction
- 作者: Kyoohyung Han, Seongkwang Kim, Byeonghak Lee (Samsung SDS), Yongha Son
- 会议: Asiacrypt 2024

## 1. 现有 OKVS-based OPRF 安全证明中的缺陷

RS21 声称将 PRF 输出长度设为 κ（128 位）足以限制恶意接收方最多获得 m 次 PRF 求值。Han et al. 指出错误：
1. PRTY bound 信息论上已不安全（n'=m, ℓ₁=128 时 2^128 次查询攻击成功概率不可忽略）
2. 安全证明逻辑漏洞：仅考虑"先固定 P 再找 x"，但恶意敌手可先大量查询 H₁，再找过拟合 P

## 2. OKVS 过拟合攻击

- 过拟合：找到 n' > m 个元素满足 Dcd(P, x) = H₁(x)
- 利用 OKVS 的二元线性性质，将过拟合归约为 k-XOR / 多重碰撞问题
- 对 PaXoS / 3H-GCT / RR22 / RB-OKVS 均有构造性攻击
- 实验：n'/m = 2 时 RR22 需 ℓ₁ ≈ 400+ 位，而非 RS21 声称的 128 位

## 3. 半诚实 vs 恶意

- **半诚实**：接收方诚实调用 Ecd 生成 P，过拟合攻击不适用（P 非恶意构造）
- **恶意**：接收方可任意构造 P，过拟合攻击发挥作用的场景
- 半诚实离线暴力搜索需要 ℓ₁ 过小才有实际影响，设为 ≥ κ 即可

## 4. 修复方案

1. 半诚实：ℓ₁ ≥ κ（128）
2. 在线过拟合博弈：salt + timeout 限制查询次数
3. 双重执行（极紧 n'≈m 场景）：第一阶段大 n''>>m OPRF → 第二阶段用{F(x)}替代{H₁(x)}
4. 中间域 Minicrypt 构造：GF(2^f) + SoftSpokenVOLE

## 5. 对 F-PSU 的影响

- ✅ **半诚实模型下不受 OKVS 过拟合攻击影响**
- 需在论文中注明：Han et al. 发现的缺陷仅影响恶意模型
- ℓ₁ 参数选择要有依据，建议 ≥ κ
- 若 Future Work 讨论恶意安全扩展，必须引用 Han et al.
- 积极面：Minicrypt 构造进展和修订的通用证明框架可作为参考

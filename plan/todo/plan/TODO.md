# F-PSU 项目任务清单

> 目标：USENIX Security 2027 Cycle 1（预计 2026 年 8 月投稿）
> 起始：2026-05-07
> 最后更新：2026-05-18（切换投稿目标，重排 TODO 为 USENIX 计划）

## 投稿时间线（预估）

| 节点 | 预计日期 | 距今 |
|---|---|---|
| CFP 发布 | 2026 年 5-6 月 | 即将 |
| Cycle 1 注册 | ~2026 年 8 月中旬 | ~3 个月 |
| **Cycle 1 提交** | **~2026 年 8 月下旬** | **~3 个月** |
| Cycle 1 通知 | ~2026 年 12 月 | |
| Cycle 2 注册 | ~2027 年 1 月 | ~8 个月（备选） |
| 会议 | 2027 年 8 月 11-13 日，丹佛 | |

> 策略：优先 Cycle 1。若被拒，用评审意见修改后补投 Cycle 2。

## 工作目录结构

```
fupsu/
├── 文献/                    # 参考论文 PDF + 精读笔记
│   ├── *.pdf                # 18 篇
│   ├── txt/                 # pdftotext 提取文本
│   └── notes/               # 7 篇精读笔记（RR22/RS21/FUPSI/Han/Alborch/Liu/Mattsson）
└── 计划/                    # 工作计划
    ├── help/                # 辅助理解文档
    │   ├── read/            #   早期辅助文档（forward privacy / update logic）
    │   └── USENIX/          #   USENIX 2023-2026 PSI/PSU 论文 (15 篇, 2026-05-18)
    ├── output/              # 成品输出
    │   ├── paper/           #   主论文（36 pages LNCS, 0 errors）
    │   ├── ppt/             #   汇报 PPT
    │   ├── show/            #   汇报文稿 / 问题分析 / 创新分析
    │   ├── code/            #   代码库（C++ libOTe 全栈 + Python 原型 + circuitPSU 参考）
    │   │   ├── cpp/         #     fpsu_net/fpsu_full + IBLT + socket + benchmark
    │   │   ├── py/          #     Python IBLT 原型 (1244 lines)
    │   │   ├── libOTe/      #     libOTe 源码 + 编译产物
    │   │   ├── circuitPSU/  #     电路 PSU 参考实现 (Chandran et al.)
    │   │   └── dujiajun-PSU/#     PSU 参考实现
    │   └── usenix/          #   USENIX 模板 + 投稿文件（待生成）
    ├── cli/                 # Claude 内部工作文件
    │   ├── Protocol.md
    │   ├── Proof.md
    │   ├── Preliminaries.md
    │   ├── Lower_Bound.md
    │   └── Novelty_Extension.md
    └── todo/
        ├── plan/TODO.md     # 本文件
        └── show/            # 汇报方案文档
```

## USENIX 格式要求速查

| 维度 | 当前（LNCS） | USENIX 要求 |
|---|---|---|
| 纸张 | A4，单栏，12.2cm 宽 | US Letter，双栏，7"×9" |
| 模板 | `llncs.cls` | `article` + `usenix.sty` |
| 正文字号 | 10pt | 10pt Times Roman，12pt 行距 |
| 正文页数 | ~36 页 | **≤13 页**（不含附录） |
| 总页数 | ~36 页 | **≤20 页**（含附录+文献） |
| 匿名 | 有作者 | **双盲**（去作者、自引用第三人称） |
| Ethical Considerations | 无 | **必须**（~½ 页） |
| Open Science | 无 | **必须**（~½ 页） |

---

## Phase 1: USENIX 格式迁移 [已完成] ✅

> 从 LNCS 单栏换到 USENIX 双栏，完成内容压缩。

- [x] **1.1** 下载 USENIX 模板（`usenix-2020-09.sty`），创建 `usenix/main.tex`
- [x] **1.2** 全文迁移到两栏，适配 section/table/figure 环境
- [x] **1.3** 所有宽表改用 `table*` 跨栏
- [x] **1.4** 协议图适配两栏（fbox+minipage 紧凑格式）
- [x] **1.5** 编译通过：12 pages, 0 errors, 0 undefined refs

## Phase 2: 内容压缩 [已完成] ✅

> 36 pages LNCS → 12 pages USENIX（正文 ~9.5 页 + 附录 ~2.5 页）。

- [x] **2.1** Section 4（Leakage Barrier）：定理陈述留正文，长证明 → 附录
- [x] **2.2** Section 6（Security Proof）：主文只放 Theorem + Proof Sketch，hybrid+simgame → 附录
- [x] **2.3** Section 7（Evaluation）：压缩到 5 页，保留核心表
- [x] **2.4** Section 2（Technical Overview）：精简到 2 页
- [x] **2.5** Related Work + Introduction 在 2 页内
- [x] **2.6** Ethical Considerations 正文末（参考文献前）
- [x] **2.7** Open Science 正文末（参考文献前）
- [x] **2.8** 总页数 12 页 ≤ 20 页
- [x] **2.9** 协议 Pseudocode 移入附录（fbox+minipage）

## Phase 3: 匿名化 [已完成] ✅

- [x] **3.1** `\author{Anonymous Submission}`
- [x] **3.2** 代码引用匿名化 `\cite{code}`

## Phase 4: 对比实验与 Related Work 强化 [已完成] ✅

> 与 USENIX PSU 系列论文做详细对比，满足审稿人对 SOTA 对比的期待。

- [x] **4.1** 精读 Tu et al. (USENIX'25) ePSU 实验部分，提取对比基线
- [x] **4.2** 精读 Jia et al. (USENIX'24) 泄漏模型，与本文 leakage barrier 对比
- [x] **4.3** Comparison with Prior Work 表增加 USENIX PSU 系列（Zhang'23 / Jia'24 / Tu'25 / Dong'25 / PT26）
- [x] **4.4** Related Work 重写：每个 USENIX 论文的具体贡献 + 与 F-PSU 的差异化
- [x] **4.5** 新增 Jia+24 / Dong+25 参考文献条目

---

## Phase 5: 审阅 + 润色 + 投递 [待开始]

- [ ] **5.1** 导师/合作者审阅
- [ ] **5.2** USENIX 审稿标准自评（novelty / soundness / presentation / experiments）
- [ ] **5.3** 全文语言润色
- [ ] **5.4** HotCRP 注册（截止前一周，约 2026 年 8 月中旬）
- [ ] **5.5** 提交 PDF + 附录 + artifacts

---

## Phase 6: Cycle 1 被拒后的补救 [条件触发]

> 若 12 月收到拒稿，利用评审意见修改后投 Cycle 2（~2027 年 1-2 月）。

- [ ] **6.1** 逐条回复审稿意见
- [ ] **6.2** 补充缺失实验/证明
- [ ] **6.3** Cycle 2 提交

---

## 已完成基础

> 以下工作已在 LNCS 版阶段完成，USENIX 阶段直接继承：

| 模块 | 内容 | 状态 |
|---|---|---|
| 文献精读 | 12 篇 PSI/PSU/IBLT/UC 核心论文 + 笔记 | ✅ |
| 协议设计 | v4 post-audit: delta IBLT + UnionPeel + PRF chain | ✅ |
| 安全性证明 | UC-realizes ℱ_fpsu, 3 hybrids, FP-IND game, Lemmas 4-9 | ✅ |
| 泄漏下界 | 压缩论证，协议达到 leakage-optimal | ✅ |
| Python 原型 | 1244 lines, IBLT encode/decode/delta/peeling, 10 tests | ✅ |
| C++ 全栈 | libOTe + IBLT + TCP socket + LAN/WAN 实测 | ✅ |
| OT/OPRF 集成 | KOS OT extension, 5.2M OTs/s, fpsu_full | ✅ |
| Static Baseline | 同硬件 fpsu_net -S 对比，delta vs full 4 档网络 | ✅ |
| Citation Audit | 36 篇参考文献全部验证正确 | ✅ |
| LNCS 论文 | 36 pages, 0 errors, 0 undefined refs | ✅ |
| USENIX 参考论文 | 15 篇 2023-2026 USENIX PSI/PSU PDF 已下载到 `help/USENIX/` | ✅ |

## 论文卖点（USENIX 审稿视角）

1. **First updatable PSU** — USENIX PSU 系列全是静态，本文第一个增量更新
2. **Forward privacy via symmetric-key chain** — PRF+ChaCha20，无公钥操作
3. **IBLT delta encoding** — $O(|\Delta|)$ 通信，工程上可直接部署
4. **Leakage lower bound** — $O(|\Delta|)$ 协议必泄露个别更新量，协议达到最优
5. **全栈 C++ 实测** — libOTe 集成 + LAN/WAN 4 档实测

## USENIX PSI/PSU 参考论文（2023-2026）

> 已下载到 `计划/help/USENIX/`，共 15 篇。

**2023:** Bienstock(OKVS) / **Zhang(Linear PSU)** / Chakraborti(Distance-Aware) / Wu(Unbalanced Cardinality)
**2024:** **Jia(Scalable PSU)** / Hao(Circuit-PSI) / Bienstock(Batch PIR) / Wu(ORing KStar) / Mahdavi(PEPSI)
**2025:** **Tu(Fast ePSU)** / **Dong(MPSU)** / Yeo(Third Party PSI)
**2026:** Blass(Fuzzy PSI) / Goel(PICS) / Meng(Fuzzy PSI L∞)

---

## 当前论文状态

| 指标 | 值 |
|------|-----|
| 页码 | 12 pages USENIX（正文 ~9.5p + 附录 ~2.5p） |
| 格式 | `article` + `usenix-2020-09.sty`，letterpaper，twocolumn，10pt |
| 错误 | 0 errors |
| 未定义引用 | 0 undefined refs |
| 参考文献 | 36 entries |
| 最后编译 | 2026-05-18（Ethics/OpenScience 移至正文末，附录仅含 Proof + Pseudocode + Prelim + SelfHeal） |
| PDF 位置 | `计划/output/usenix/main.pdf` |

# Security Proof — F-PSU (v4, post-audit)

## Theorem 1 (Main)

Let $\mathsf{prm}_t$ be IBLT parameters such that $\mathsf{List}$ fails with probability $\leq 2^{-\lambda}$ for threshold $|X_t| + |Y_t|$, for all $t \in [T]$. Let $\mathsf{ChaCha20}$ be a secure PRG and $F = \mathsf{HMAC\text{-}SHA256}$ be a secure PRF. Then $\Pi_{\mathsf{fpsu}}$ **UC-realizes** $\mathcal{F}_{\mathsf{fpsu}}$ in the $(\mathcal{F}_{\mathsf{OPRF}}, \mathcal{F}_{\mathsf{OT}})$-hybrid model against **PPT semi-honest adaptive** adversaries.

### Proof Structure

We construct a simulator $\mathcal{S}$ in two stages:
1. **Per-epoch simulation** (Lemma 4): Within each epoch, $\mathcal{S}$ uses the Piske & Trieu UnionPeel simulator, which requires only $X_{\cup,t}$ to simulate the OT/OPRF interactions.
2. **Cross-epoch simulation** (Lemma 5): $\mathcal{S}$ simulates the honest party's key chain with random independent keys (PRF security) and fresh ciphertexts with random strings (PRG security). Upon adaptive corruption, $\mathcal{F}_{\mathsf{fpsu}}$ provides exactly the state $\mathcal{S}$ needs.

---

## Simulator $\mathcal{S}$

Assume $\mathcal{A}$ corrupts $P_0$ at epoch $\tau$ ($P_1$ symmetric; both corrupted is trivial).

### State

$\mathcal{S}$ maintains for honest $P_1$:
- $\tilde{Y}_t$: simulated set, derived as $\tilde{Y}_t \leftarrow X_{\cup,t} \setminus X_t$
- $\tilde{K}^S_t \sample \{0,1\}^\kappa$: simulated key (random, not PRF-derived)
- $\widetilde{\mathsf{IBLT}}_S^{(t)}$: simulated IBLT (not used directly; UnionPeel simulation uses $\mathsf{IBLT}_\cup$)
- $\widetilde{CT}_S^{(t)}$: simulated ciphertext ($\mathsf{Enc}(\tilde{K}^S_t, \mathsf{random})$)

### Epoch $t < \tau$ (Before Corruption)

$\mathcal{S}$ receives $X_{\cup,t}$ from $\mathcal{F}_{\mathsf{fpsu}}$ and $X_t$ (as $P_0$'s input, from $\mathcal{Z}$ through $\mathcal{A}$).

**Step 1: Derive simulated set.**
$$
\tilde{Y}_t \leftarrow X_{\cup,t} \setminus X_t
$$
By construction, $X_t \cup \tilde{Y}_t = X_{\cup,t}$. The difference from real $Y_t$ is confined to $X_t \cap Y_t$ (intersection), which PSU security hides.

**Step 2: Simulate UnionPeel.** $\mathcal{S}$ computes $\mathsf{IBLT}_\cup \leftarrow \mathsf{Encode}(X_{\cup,t})$ and runs $\mathsf{List}(\mathsf{IBLT}_\cup)$ to obtain the peeling trace. In the $(\mathcal{F}_{\mathsf{OPRF}}, \mathcal{F}_{\mathsf{OT}})$-hybrid model, $\mathcal{S}$ controls these ideal functionalities and programs their outputs to match the peeling trace. This is exactly Piske & Trieu's simulator [PT26, App. E.2].

Since $\mathsf{Peel}(\mathsf{IBLT}_\cup, i, j) = \mathsf{uPeel}(\mathsf{IBLT}_C, \mathsf{IBLT}_S, i, j)$ (Theorem 2), and the simulated $\mathsf{IBLT}_S$ would produce the same $\mathsf{uPeel}$ outputs as real $\mathsf{IBLT}_S$ when combined with $P_0$'s real $\mathsf{IBLT}_C$, the programmed OT/OPRF outputs are identical to what the real protocol would produce.

**Step 3: Simulate key evolution.** $\mathcal{S}$ samples $\tilde{K}^S_t \sample \{0,1\}^\kappa$ (random, not $\mathsf{PRF}(\tilde{K}^S_{t-1})$). $\mathcal{S}$ encrypts a dummy IBLT: $\widetilde{CT}_S^{(t)} \leftarrow \mathsf{Enc}(\tilde{K}^S_t, 0^{|\mathsf{IBLT}_S|})$.

**Step 4: Simulate P₁'s local state.** $\mathcal{S}$ updates its simulated state and "erases" $\tilde{K}^S_{t-1}$.

### Epoch $t = \tau$ (Corruption)

$\mathcal{F}_{\mathsf{fpsu}}$ sends $\mathcal{S}$ the corrupted party's state: $(X_\tau, K^C_\tau)$. $\mathcal{S}$ delivers this to $\mathcal{A}$.

$\mathcal{S}$ also receives $P_1$'s state $(Y_\tau, K^S_\tau)$ **only if $P_1$ is also corrupted**. In the single-corruption case, $P_1$ remains honest, and $\mathcal{S}$ continues using $\tilde{Y}_\tau$.

### Epoch $t > \tau$ (After Corruption)

$P_0$ is now corrupted. $\mathcal{S}$ receives $X_t$ from $\mathcal{A}$ and $X_{\cup,t}$ from $\mathcal{F}_{\mathsf{fpsu}}$. Simulation proceeds identically to the pre-corruption case.

---

## Indistinguishability Proof

We prove $\mathsf{REAL} \approx_c \mathsf{IDEAL}$ via a sequence of hybrid simulators $\mathcal{S}_0 \to \mathcal{S}_1 \to \mathcal{S}_2$.

### $\mathcal{S}_0$: Real Protocol

$\mathcal{S}_0$ runs the real $\Pi_{\mathsf{fpsu}}$ with honest inputs. Identical to $\mathsf{REAL}$.

### $\mathcal{S}_1$: Random Keys (PRF Replacement)

$\mathcal{S}_1$ replaces $P_1$'s PRF-derived keys with independent random keys:

$$
K^S_1, \ldots, K^S_T \sample \{0,1\}^\kappa \quad \text{(instead of } K^S_t = \mathsf{PRF}(K^S_{t-1}, c)\text{)}
$$

**Lemma 4.** $|\Pr[\mathcal{Z}(\mathcal{S}_1)=1] - \Pr[\mathcal{Z}(\mathcal{S}_0)=1]| \leq T \cdot \mathsf{Adv}^{\mathsf{PRF}}(\kappa).$

*Proof.* Standard PRF hybrid over $T$ epochs. For each transition $t$, embed the PRF challenge: set $K^S_{t-1}$ to the hidden PRF key and $K^S_t \leftarrow \mathcal{O}(c)$ where $\mathcal{O}$ is either $\mathsf{PRF}(K, \cdot)$ or a random function. Distinguishing $\mathcal{S}_0$ from $\mathcal{S}_1$ breaks PRF security. $\square$

### $\mathcal{S}_2$: Simulated Honest Party (Final Simulator)

$\mathcal{S}_2$ replaces $P_1$'s IBLT with the simulated set $\tilde{Y}_t = X_{\cup,t} \setminus X_t$, and encrypts dummy IBLTs instead of real ones. This is exactly $\mathcal{S}$ from the previous section.

**Lemma 5.** $\Pr[\mathcal{Z}(\mathcal{S}_2)=1] = \Pr[\mathcal{Z}(\mathcal{S}_1)=1].$

*Proof.* The only difference between $\mathcal{S}_1$ and $\mathcal{S}_2$ is the content of $P_1$'s IBLT. In both, $P_1$'s keys are truly random (by Lemma 4). The IBLT content affects the view only through the UnionPeel outputs.

In the $(\mathcal{F}_{\mathsf{OPRF}}, \mathcal{F}_{\mathsf{OT}})$-hybrid model, UnionPeel's outputs are determined by $\mathsf{uPeel}(\mathsf{IBLT}_C, \mathsf{IBLT}_S, i, j)$, which by Theorem 2 equals $\mathsf{Peel}(\mathsf{IBLT}_\cup, i, j)$ where $\mathsf{IBLT}_\cup = \mathsf{Encode}(X_{\cup,t})$. This depends **only on $X_{\cup,t}$**, not on the individual $\mathsf{IBLT}_C$ or $\mathsf{IBLT}_S$.

Both $\mathcal{S}_1$ (using real $Y_t$) and $\mathcal{S}_2$ (using $\tilde{Y}_t$) produce $\mathsf{IBLT}_\cup$ encoding $X_t \cup Y_t = X_t \cup \tilde{Y}_t = X_{\cup,t}$. Therefore, the UnionPeel outputs are identical in distribution.

The encrypted IBLT ciphertexts in $\mathcal{S}_2$ are encryptions of dummy data under random keys. In $\mathcal{S}_1$, they are encryptions of real IBLTs under random keys. By PRG security of ChaCha20, these are indistinguishable. Since the adversary does not possess the decryption keys (they belong to $P_1$, which is honest before corruption), the ciphertexts reveal no information about the underlying plaintext.

After corruption at epoch $\tau$: $\mathcal{F}_{\mathsf{fpsu}}$ reveals $P_0$'s state $(X_\tau, K^C_\tau)$. $\mathcal{S}_2$ has simulated $P_1$'s messages for $t < \tau$ using only $X_{\cup,t}$ (received from $\mathcal{F}_{\mathsf{fpsu}}$). The simulated view is consistent with the revealed state because the per-epoch protocol depends only on the union output, which $\mathcal{S}_2$ received from $\mathcal{F}_{\mathsf{fpsu}}$. No inconsistency arises. $\square$

**Conclusion of Theorem 1.** $\mathcal{S}_2$ is the final simulator. The total distinguishing advantage is:

$$
|\Pr[\mathsf{REAL}=1] - \Pr[\mathsf{IDEAL}=1]| \leq T \cdot \mathsf{Adv}^{\mathsf{PRF}}(\kappa) + T \cdot \mathsf{Adv}^{\mathsf{PRG}}(\kappa) \leq \negl(\kappa). \qquad \square
$$

---

## Lemma 6 (Forward Privacy, Standalone)

If $\Pi_{\mathsf{fpsu}}$ UC-realizes $\mathcal{F}_{\mathsf{fpsu}}$ with adaptive corruptions, then it satisfies the game-based forward privacy definition (Definition 10).

*Proof.* In the ideal world, upon corruption at epoch $\tau$, $\mathcal{F}_{\mathsf{fpsu}}$ reveals only $(X_{b,\tau}, K_{b,\tau})$. Historical keys $\{K_{b,t}\}_{t<\tau}$ and sets $\{X_{b,t}\}_{t<\tau}$ are **not** revealed.

In the $\mathsf{FP\text{-}IND}$ game, the adversary receives ciphertexts $\{CT_t\}_{t<\tau}$ as the challenge. By Lemma 3, distinguishing real from random ciphertexts reduces to PRF+PRG security with advantage $\leq T \cdot (\mathsf{Adv}^{\mathsf{PRF}} + \mathsf{Adv}^{\mathsf{PRG}})$.

The UC definition implies the game-based definition because the simulator in the UC proof does not receive historical keys from $\mathcal{F}_{\mathsf{fpsu}}$, and yet produces an indistinguishable view. $\square$

---

## Lemma 7 (Cascade Analysis, Formal)

Let $\mathsf{IBLT}_\cup^{(t-1)} = \mathsf{Encode}(X_{t-1} \cup Y_{t-1})$ have an empty 2-core after full peeling. Let $\Delta = \Delta^+ X_t \cup \Delta^- X_t \cup \Delta^+ Y_t \cup \Delta^- Y_t$ be the combined delta. Then the incremental peeling procedure starting from $Q_0 = \{(i, h_i(x)) : x \in \Delta, i \in [k]\}$ examines at most $k \cdot (|\Delta| + C)$ cells, where $C$ is the size of the mini-2-core formed by $\Delta$, and:

$$
\mathbb{E}[C] \leq \binom{k|\Delta|}{2} \cdot \frac{1}{m} \cdot k = O\left(\frac{k^3 |\Delta|^2}{m}\right)
$$

with $C = 0$ with probability $\geq 1 - |\Delta|^{-k+2}$ when $|\Delta| \ll m/k$.

*Proof.* After full peeling of $\mathsf{IBLT}_\cup^{(t-1)}$, all cells have $\mathsf{cnt} \in \{0, \geq 2\}$, and with empty 2-core, all cells have $\mathsf{cnt} = 0$. Applying $\Delta$ to an all-zero IBLT is equivalent to $\mathsf{Encode}(\Delta)$ on a fresh IBLT. The peeling process on this fresh IBLT is the standard $\mathsf{List}$ algorithm. By Theorem 1, for $m > c_k \cdot |\Delta|$, peeling succeeds with probability $\geq 1 - |\Delta|^{-k+2}$, examining $O(k|\Delta|)$ cells. Since $|\Delta| \ll m/k$ by assumption, the condition holds. $\square$

With IBLT parameters chosen for the full set ($m > c_k \cdot n$), and $|\Delta| \ll n$, the condition $m > c_k \cdot |\Delta|$ holds automatically. The cascade is therefore bounded by $O(k|\Delta|)$ with overwhelming probability.

---

## Lemma 8 (Han et al. [HKL+24] Non-Applicability)

Han et al. present an OKVS overfitting attack where a **malicious** receiver constructs an adversarially overfitted OKVS to obtain extra PRF evaluations. In the semi-honest model, the receiver honestly executes $\mathsf{OKVS}.\mathsf{Encode}$, which produces an encoding where $\mathsf{Decode}(P, x)$ is correct for exactly the $n$ encoded inputs and pseudorandom for all others. No overfitting is possible without deviating from $\mathsf{Encode}$.

**Our protocol operates in the semi-honest model → Han et al. attack does not apply.**

*Caveat.* Han et al. (Section 4.1) also note that semi-honest receivers should set OKVS output length $\ell_1 \geq \kappa$ to prevent offline brute-force attacks. Our RR22 instantiation uses $\ell_1 = 128$, satisfying this. For malicious security extensions, the OKVS must be upgraded to Han et al.'s Minicrypt construction. $\square$

---

## Lemma 9 (Mattsson [Mat24] Non-Applicability)

Mattsson analyzes multi-branch symmetric ratchets (Signal, TLS 1.3, MLS) where key collisions arise from multiple update paths and external entropy injection. Our PRF chain $K_{t+1} = \mathsf{PRF}(K_t, c)$ is a **single deterministic chain** with no branching and no external entropy.

**Reduction to PRF inversion.** In the degenerate single-chain case, the Mattsson TMTO attack of complexity $\tilde{O}(N^{1/4})$ (for multi-branch ratchets over key space $N$) reduces to standard PRF key inversion: find $K_{t-1}$ given $K_t = \mathsf{PRF}(K_{t-1}, c)$. This costs $O(2^\kappa)$ by brute force. For $\kappa = 256$, no practical attack exists. $\square$

---

## Forward-Only Security (Explicit Statement)

Our protocol provides **forward security only**, not post-compromise security:

- **Forward security:** Corruption at epoch $\tau$ does **not** reveal data from epochs $t < \tau$. ✓
- **Post-compromise security:** Corruption at epoch $\tau$ **does** allow the adversary to decrypt data from epochs $t > \tau$ (since $K_{t+1} = \mathsf{PRF}(K_t)$ and the adversary knows $K_\tau$). ✗

This is an intentional design choice: post-compromise security would require public-key ratcheting (e.g., DH key exchange per epoch), conflicting with our goal of an all-symmetric protocol. The paper explicitly discusses this trade-off.

---

## Summary of Cryptographic Assumptions

| Assumption | Where Used | Reduction Loss |
|-----------|-----------|----------------|
| ChaCha20 is a PRG | IBLT storage encryption | $T \cdot \mathsf{Adv}^{\mathsf{PRG}}$ |
| HMAC-SHA256 is a PRF | Key chain $K_{t+1} = \mathsf{PRF}(K_t)$ | $T \cdot \mathsf{Adv}^{\mathsf{PRF}}$ |
| IBLT Listing succeeds | Protocol correctness | Statistical $2^{-\lambda}$ (conditions whole proof) |
| RR22 bOPRF UC-secure | UnionPeel sub-protocol | Inherited from [RR22] |
| Piske & Trieu UnionPeel UC-secure | Per-epoch PSU | Inherited from [PT26, Thm 3-4] |

All assumptions are standard. No new cryptographic assumptions are introduced.

## References

[PT26] Piske, Trieu. "Is PSI Really Faster Than PSU?" Eurocrypt 2026.
[RR22] Raghuraman, Rindal. "Blazing Fast PSI." CCS 2022.
[HKL+24] Han, Kim, Lee, Son. "Revisiting OKVS-based OPRF." Asiacrypt 2024.
[Mat24] Mattsson. "Security of Symmetric Ratchets." ePrint 2024/220.
[Can01] Canetti. "Universally Composable Security." FOCS 2001.

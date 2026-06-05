# Preliminaries — F-PSU (v4, post-audit)

## Notation

| Symbol | Meaning |
|--------|---------|
| $\kappa$ | Computational security parameter ($\kappa = 128$) |
| $\lambda$ | Statistical security parameter ($\lambda = 40$) |
| $[n]$ | $\{1, \ldots, n\}$ |
| $x \sample \mathcal{D}$ | $x$ sampled uniformly from $\mathcal{D}$ |
| $a \| b$ | String concatenation |
| $\negl(\kappa)$ | Negligible function in $\kappa$ |
| $U$ | Element universe, $\{0,1\}^\sigma$ |
| $P_0, P_1$ | Two parties |
| $X_t, Y_t$ | Sets of $P_0, P_1$ at epoch $t$ |
| $\Delta^+ X_t, \Delta^- X_t$ | Elements added / deleted by $P_0$ at epoch $t$ |
| $X_{\cup,t}$ | $X_t \cup Y_t$, the union at epoch $t$ |

IBLT parameters: $k$ hash functions, $m = k\ell$ cells, modulus $M$.

---

## 1 Invertible Bloom Lookup Tables

We adopt the IBLT variant from Piske & Trieu [PT26], itself from Goodrich & Mitzenmacher [GM11].

**Definition 1 (IBLT).** An IBLT with $\mathsf{prm}= (M, k, \ell, H)$ consists of two $k \times \ell$ matrices $\mathsf{sum} \in \mathbb{Z}_M^{k \times \ell}$ and $\mathsf{cnt} \in \mathbb{N}^{k \times \ell}$, and $k$ hash functions $H = \{h_i : U \to [\ell]\}_{i \in [k]}$.

**Definition 2 (Encode).** $\mathsf{Encode}_{\mathsf{prm}}(X)$:
1. $\mathsf{sum}[i][j] \leftarrow 0$, $\mathsf{cnt}[i][j] \leftarrow 0$ for all $i \in [k], j \in [\ell]$
2. For each $x \in X$, $i \in [k]$: $j \leftarrow h_i(x)$, $\mathsf{sum}[i][j] \leftarrow \mathsf{sum}[i][j] + x \bmod M$, $\mathsf{cnt}[i][j] \leftarrow \mathsf{cnt}[i][j] + 1$
3. Return $(\mathsf{sum}, \mathsf{cnt})$

**Definition 3 (DeleteSet).** Inverse of Encode: subtract $x$ and decrement $\mathsf{cnt}$ at positions $\{h_i(x)\}_{i \in [k]}$.

**Definition 4 (Peel).** $\mathsf{Peel}(\mathsf{IBLT}, i, j) = \mathsf{sum}[i][j]$ if $\mathsf{cnt}[i][j] = 1$, else $\bot$.

**Definition 5 (List).** Iteratively peel: $V^{(r)} \leftarrow \{\mathsf{Peel}(\mathsf{IBLT}^{(r)}, i, j)\}$, $\mathsf{IBLT}^{(r+1)} \leftarrow \mathsf{DeleteSet}(\mathsf{IBLT}^{(r)}, V^{(r)})$, repeat until no peelable cells. Return $\bigcup_r V^{(r)}$.

**Definition 6 (IBLT Linearity).** For disjoint $A, B \subseteq U$:
$$
\mathsf{Encode}(A \cup B) = \mathsf{Encode}(A) \oplus \mathsf{Encode}(B)
$$
where $\oplus$ is cell-wise addition: $\mathsf{sum}_\oplus = \mathsf{sum}_A + \mathsf{sum}_B \pmod M$, $\mathsf{cnt}_\oplus = \mathsf{cnt}_A + \mathsf{cnt}_B$.

Similarly, $\mathsf{Encode}(A \setminus B) = \mathsf{Encode}(A) \ominus \mathsf{Encode}(B)$ where $\ominus$ is cell-wise subtraction.

This linearity is the foundation of our delta IBLT encoding technique.

**Theorem 1 (IBLT Listing [GM11, PT26]).** For $m = k\ell > (c_k + \varepsilon) \cdot \tau$ with $n \leq \tau$ elements:
$$
\Pr[\mathsf{List}\text{ fails}] = O(\tau^{-k+2})
$$
with $c_3 \approx 1.222$, $c_4 \approx 1.295$, $c_5 \approx 1.425$. For $k=4$ and expansion $m/\tau \in [1.5, 4.5]$, failure probability $\leq 2^{-\lambda}$ [PT26, §7].

**Definition 7 (2-Core).** The 2-core of an IBLT is the maximal sub-IBLT where every cell has $\mathsf{cnt} \geq 2$. It is what remains after $\mathsf{List}$ terminates without recovering all elements. For $m > c_k \cdot n$, the 2-core is empty with probability $\geq 1 - O(n^{-k+2})$.

**Lemma 1 (Cascade Bound).** Starting from an IBLT with empty 2-core, applying delta $\Delta$ produces a new IBLT whose peeling cascade examines at most $k \cdot |\Delta| + O(|\Delta|^2/m)$ cells in expectation, and $O(k|\Delta|)$ cells with probability $\geq 1 - 2^{-\lambda}$ when $|\Delta| \ll m/k$.

*Proof sketch.* With empty 2-core, all cells have $\mathsf{cnt}=0$ after full peeling. Delta elements are placed into empty cells; collisions among delta elements create a mini-2-core of expected size $O(k^2|\Delta|^2/m)$. Peeling this mini-2-core touches $O(k|\Delta| + k \cdot \text{mini-core})$ cells. By the same hypergraph analysis as Theorem 1, the mini-core is small when $|\Delta| \ll m/k$. $\square$

---

## 2 Union Peelability [PT26]

**Definition 8 (Union Peelability).** $\{\mathsf{IBLT}_0, \mathsf{IBLT}_1, \mathsf{IBLT}_\cup\}$ are union peelable if $\mathsf{IBLT}_0 = \mathsf{Encode}(X_0)$, $\mathsf{IBLT}_1 = \mathsf{Encode}(X_1)$, $\mathsf{IBLT}_\cup = \mathsf{Encode}(X_0 \cup X_1)$, all with identical $\mathsf{prm}$.

**Definition 9 ($\mathsf{uPeel}$).**
$$
\mathsf{uPeel}(\mathsf{IBLT}_0, \mathsf{IBLT}_1, i, j) = \begin{cases}
\mathsf{sum}_0[i][j] & \text{if } \mathsf{cnt}_0 = 1 \land \mathsf{cnt}_1 = 0 \\
\mathsf{sum}_1[i][j] & \text{if } \mathsf{cnt}_0 = 0 \land \mathsf{cnt}_1 = 1 \\
\mathsf{sum}_0[i][j] & \text{if } \mathsf{cnt}_0 = \mathsf{cnt}_1 = 1 \land \mathsf{sum}_0 = \mathsf{sum}_1 \\
\bot & \text{otherwise}
\end{cases}
$$

**Theorem 2 (Union Peel Equivalence [PT26, Thm 2]).** For union peelable IBLTs:
$\mathsf{Peel}(\mathsf{IBLT}_\cup, i, j) = \mathsf{uPeel}(\mathsf{IBLT}_0, \mathsf{IBLT}_1, i, j)$.

---

## 3 Cryptographic Primitives

### 3.1 Stream Cipher (ChaCha20)

$\mathsf{ChaCha20} : \{0,1\}^{256} \times \{0,1\}^{96} \to \{0,1\}^*$ is a secure PRG. Encryption: $C = M \oplus \mathsf{ChaCha20}(K, N)$. Chosen over AES-CTR for its native PRF design (no birthday bound) and software performance.

### 3.2 Pseudorandom Function (HMAC-SHA256)

$F : \{0,1\}^\kappa \times \{0,1\}^* \to \{0,1\}^\kappa$ is a secure PRF. Instantiated as $F(K, x) = \mathsf{HMAC\text{-}SHA256}(K, x)$.

**Lemma 2 (PRF Key One-Wayness).** If $F$ is a secure PRF, then for any PPT $\mathcal{A}$ and fixed constant $c$:
$\Pr[\mathcal{A}(F(K, c)) = K : K \sample \{0,1\}^\kappa] \leq \negl(\kappa)$.

**Forward Security vs Post-Compromise Security.** A PRF chain $K_{t+1} = F(K_t, c)$ provides **forward security**: given $K_{t+1}$, $K_t$ is computationally hidden (by Lemma 2). It does **not** provide **post-compromise security**: given $K_t$, all future keys $K_{t+1}, K_{t+2}, \ldots$ are deterministically computable. Our protocol provides forward security only; post-compromise security can be achieved by injecting fresh entropy (e.g., DH ratchet) at the cost of public-key operations. We leave this to future work.

### 3.3 VOLE + OKVS + bOPRF

**VOLE** (Vector Oblivious Linear Evaluation) over field $\mathbb{F}$: Sender inputs $\Delta \in \mathbb{F}$, Receiver inputs $\vec{x} \in \mathbb{F}^n$. Receiver gets $\vec{y}$, Sender gets $\vec{z}$ with $\vec{y} + \vec{z} = \Delta \cdot \vec{x}$.

**Subfield VOLE [RR22].** When $\vec{x}$ has entries in subfield $\mathbb{B} \subset \mathbb{F}$, communication reduces from $O(n \log|\mathbb{F}|)$ to $O(n \log|\mathbb{B}| + n \log n + \kappa \log|\mathbb{F}|)$.

**OKVS** (Oblivious Key-Value Store). $\mathsf{Encode}(\{(k_i, v_i)\}_{i=1}^n) \to P \in \mathbb{B}^m$. $\mathsf{Decode}(P, k_i) = v_i$ for encoded keys; pseudorandom otherwise. We use RR22's RB-OKVS.

**bOPRF via VOLE+OKVS [RS21, RR22].** Receiver encodes $\{(x_i, r_i)\}$ with random $r_i$ into OKVS vector $P$. VOLE produces additive shares of $\Delta \cdot P$. PRF: $F_\Delta(x) = H(\Delta \cdot \mathsf{Decode}(P, x) - \mathsf{share}_S[x])$. Receiver computes $F_\Delta(x_i) = H(r_i)$ for $x_i \in X$. Sender evaluates on any $y$. We instantiate with RR22.

---

## 4 UC Framework [Can01]

We work in the UC framework with **adaptive** corruptions (adversary may corrupt parties at any epoch), PPT environment $\mathcal{Z}$, and the $(\mathcal{F}_{\mathsf{OPRF}}, \mathcal{F}_{\mathsf{OT}})$-hybrid model.

---

## 5 Ideal Functionality $\mathcal{F}_{\mathsf{fpsu}}$

```
Ideal Functionality F_fpsu

Parameters: P_0, P_1. Universe U. Epoch bound T = poly(κ).

State per P_b:
  X_b ⊆ U              // accumulated set, initially ∅
  K_b ∈ {0,1}^κ        // PRF key, initially ⊥
  t_b ∈ N              // epoch counter, initially 0

Epoch t ∈ {1, ..., T}:

  On (Update, Δ⁺_b, Δ⁻_b) from P_b:
    1. X_b ← (X_b ∪ Δ⁺_b) \ Δ⁻_b
    2. Send (Leak, |X_b|, |Δ⁺_b|, |Δ⁻_b|) to S
       // Leaks total set size AND delta sizes (see Section 5.1)

  When both parties submitted for epoch t:
    3. X_∪ ← X_0 ∪ X_1
    4. If t = 1: K_b ←$ {0,1}^κ
       Else:     K_b ← PRF(K_b, "evolve"), erase old K_b
    5. Output (Union, X_∪) to both parties

Adaptive Corruption:

  On (Corrupt, P_b) from S:
    - Mark P_b as corrupted
    - Send (X_b, K_b) to S
    // Forward privacy: S does NOT receive {X_b^{(<t)}, K_b^{(<t)}}
    // After corruption, S controls P_b's future inputs
```

### 5.1 Leakage Discussion

$\mathcal{F}_{\mathsf{fpsu}}$ leaks $(|X_b|, |\Delta^+_b|, |\Delta^-_b|)$ at each epoch. This is more than the net change $||\Delta^+_b| - |\Delta^-_b||$ (which would be inferrable from $|X_b|$ alone). The additional leakage of individual add/delete counts reflects a real protocol-level leakage: the delta IBLT size reveals how many elements were added or deleted. We accept this as inherent to size-varying data exchange, analogous to how TLS reveals record lengths.

---

## 6 Game-Based Forward Privacy (Auxiliary)

**Game $\mathsf{FP\text{-}IND}_{\mathcal{A}}(\kappa, T)$:**

```
1. b ←$ {0,1}; K_{b,0} ←$ {0,1}^κ; X_{b,0} ← ∅
2. For t = 1 to T:
     (Δ⁺_t, Δ⁻_t) ← A(state_t)
     X_{b,t} ← (X_{b,t-1} ∪ Δ⁺_t) \ Δ⁻_t
     K_{b,t} ← PRF(K_{b,t-1}, "evolve")
     CT_t ← Enc(K_{b,t}, Encode(X_{b,t}))
     state_t ← CT_t; Erase(K_{b,t-1})
3. τ ← A(state_T)
4. view ← (X_{b,τ}, K_{b,τ})
5. c ←$ {0,1}
   If c = 0: challenge ← (CT_1,...,CT_{τ-1})
   Else:     challenge ← (R_1,...,R_{τ-1})  // random, |R_t|=|CT_t|
6. c' ← A(view, challenge)
7. Output (c = c')
```

**Definition 10 (Forward Privacy).** $\mathsf{Adv}^{\mathsf{FP\text{-}IND}}_{\mathcal{A}}(\kappa) = |\Pr[\text{output}=1] - \frac{1}{2}| \leq \negl(\kappa)$.

**Lemma 3.** If $F$ is a secure PRF and ChaCha20 is a secure PRG:
$\mathsf{Adv}^{\mathsf{FP\text{-}IND}}_{\mathcal{A}}(\kappa) \leq T \cdot (\mathsf{Adv}^{\mathsf{PRF}}(\kappa) + \mathsf{Adv}^{\mathsf{PRG}}(\kappa))$.

*Proof.* Game 1: replace $K_{b,1},\ldots,K_{b,\tau-1}$ with independent random keys (PRF hybrid, $T \cdot \mathsf{Adv}^{\mathsf{PRF}}$). Game 2: replace $CT_1,\ldots,CT_{\tau-1}$ with random strings (PRG hybrid, $T \cdot \mathsf{Adv}^{\mathsf{PRG}}$). In Game 2, challenge is independent of $c$, so $\Pr = 1/2$. $\square$

## References

[GM11] Goodrich, Mitzenmacher. "Invertible Bloom Lookup Tables." Allerton 2011.
[PT26] Piske, Trieu. "Is PSI Really Faster Than PSU?" Eurocrypt 2026.
[RS21] Rindal, Schoppmann. "VOLE-PSI." Eurocrypt 2021.
[RR22] Raghuraman, Rindal. "Blazing Fast PSI." CCS 2022.
[HKL+24] Han, Kim, Lee, Son. "Revisiting OKVS-based OPRF." Asiacrypt 2024.
[Can01] Canetti. "Universally Composable Security." FOCS 2001.

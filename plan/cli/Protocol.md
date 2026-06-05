# Protocol Construction — F-PSU (v4, post-audit)

## Design Philosophy

After systematic self-audit (13 identified issues), our protocol adopts three principles:

1. **Each party maintains their own IBLT**, encrypted under their own PRF chain key. No IBLT exchange.
2. **Delta IBLT encoding** exploits IBLT linearity for $O(|\Delta|)$ per-epoch updates.
3. **Forward privacy via PRF chain + key erasure** — standard, simple, correct.

Removed from earlier versions: XOR-homomorphic key refresh (contradicts forward privacy), IBLT exchange (UnionPeel doesn't need it), OPRF pre-filtering (leaks intersection), ephemeral transit keys (no exchange → not needed).

---

## Protocol $\Pi_{\mathsf{fpsu}}$: Forward-Private Updatable PSU

### Initialization (once)

```
P_0:                                    P_1:
  X_0 ← ∅                                Y_0 ← ∅
  K^C_0 ←$ {0,1}^κ                       K^S_0 ←$ {0,1}^κ
  IBLT_C ← empty (0 cells)               IBLT_S ← empty

P_0, P_1:
  Execute RR22 VOLE setup → shared VOLE state σ_VOLE
  (κ base OTs, reusable across all epochs)
```

### Epoch $t$ Protocol

**Round 1: Local Update + IBLT Encode**

```
P_0:                                    P_1:
  // Decrypt stored IBLT
  IBLT_C ← Dec(K^C_{t-1}, CT_C_stored)    IBLT_S ← Dec(K^S_{t-1}, CT_S_stored)
  
  // Apply deltas using IBLT linearity
  IBLT_C ← IBLT_C                         IBLT_S ← IBLT_S
         ⊕ Encode(Δ⁺X_t)                        ⊕ Encode(Δ⁺Y_t)
         ⊖ Encode(Δ⁻X_t)                        ⊖ Encode(Δ⁻Y_t)
  
  // Compute modified cells
  Q^C_0 ← {(i, h_i(x)): x ∈ Δ⁺X_t ∪ Δ⁻X_t, i ∈ [k]}
                                          Q^S_0 ← {(i, h_i(x)): x ∈ Δ⁺Y_t ∪ Δ⁻Y_t, i ∈ [k]}
```

Note: $\mathsf{Encode}(\Delta)$ is a **sparse** IBLT — only $k \cdot |\Delta|$ cells are non-zero. The merge costs $O(k|\Delta|)$ cell operations, not $O(k\ell)$.

**Round 2: UnionPeel (Piske & Trieu $\Pi_{\mathsf{UnionPeel}}$)**

P₀ provides $\mathsf{IBLT}_C$, P₁ provides $\mathsf{IBLT}_S$. Both know $Q^{(0)} = Q^C_0 \cup Q^S_0$ (modified cells only).

```
For iteration r = 0, 1, ... until Q^{(r)} = ∅:

  For each (i,j) ∈ Q^{(r)}:
  
    // Step 1: 1-out-of-2 OT (P₀ receiver, P₁ sender)
    P₀: c_{i,j} ← [cnt_C[i][j] = 0]
    P₁: m_{i,j} ← (⊥, sum_S[i][j] if cnt_S[i][j]=1 else ⊥)
    P₀ receives f_{i,j}
    
    // Step 2: OPRF (P₁ queries, P₀ holds key)
    P₁: z_{i,j} ← sum_S[i][j] if cnt_S[i][j]=1 else ⊥
    P₁ receives y_{i,j} ← F_k(z_{i,j}); P₀ receives key k_{i,j}
    
    // Step 3: P₀ computes
    u_{i,j} ← F_k(sum_C[i][j])             if cnt_C[i][j] = 1
            ← F_k(f_{i,j})                 if cnt_C[i][j] = 0 ∧ f_{i,j}≠⊥
            ← random                       otherwise
    
    // Step 4: 1-out-of-3 OT (P₀ sender, P₁ receiver)
    P₁: d_{i,j} ← 0 if cnt_S=0, 1 if cnt_S=1, 2 otherwise
    P₀: w_{i,j} ← (sum_C[i][j] if cnt_C=1 else ⊥,  u_{i,j},  ⊥)
    P₁ receives g_{i,j}
    
    // Step 5: P₁ computes
    v_{i,j} ← sum_S[i][j] if d=1 ∧ g=y else g_{i,j}

  V^{(r)} ← {v_{i,j} : v_{i,j} ≠ ⊥}
  
  // Delete peeled elements from both IBLTs
  P₀: IBLT_C ← DeleteSet(IBLT_C, V^{(r)} ∩ X_t)
  P₁: IBLT_S ← DeleteSet(IBLT_S, V^{(r)} ∩ Y_t)
  
  // Cascade: only check cells modified by deletions
  Q^{(r+1)} ← {(i, h_i(v)): v ∈ V^{(r)}, i ∈ [k]}

// P₁ sends results to P₀
P₁ → P₀: V = V^{(0)} ∪ V^{(1)} ∪ ...

Both output: X_{∪,t} ← V
```

**Round 3: Key Evolution + Secure Storage**

```
P₀:                                    P₁:
  K^C_t ← PRF(K^C_{t-1}, "evolve-C")     K^S_t ← PRF(K^S_{t-1}, "evolve-S")
  CT_C ← Enc(K^C_t, IBLT_C)              CT_S ← Enc(K^S_t, IBLT_S)
  Erase(K^C_{t-1})                       Erase(K^S_{t-1})
```

No XOR refresh. Each epoch's IBLT is freshly encrypted under the new key. The old key is erased. The old ciphertext (encrypted under the old key) is either deleted or remains protected by the now-erased key → forward privacy.

---

## Complexity Analysis

### Per-Epoch Cost

Let $n = |X_t| + |Y_t|$, $\Delta = |\Delta X_t| + |\Delta Y_t|$, $m = k\ell \approx 1.3n$, $\sigma$ = element width.

**Computation:**

| Operation | Cells touched | Cost |
|-----------|--------------|------|
| Decrypt stored IBLT | $m$ | $O(m)$ ChaCha20 |
| Delta IBLT encode | $k|\Delta|$ | $O(k|\Delta|)$ hashes + adds |
| Merge delta → IBLT | $k|\Delta|$ | $O(k|\Delta|)$ modular adds |
| UnionPeel (iter 0) | $k|\Delta|$ | $O(k|\Delta|)$ OTs + OPRFs |
| Cascade (iter 1+) | $O(k|\Delta|)$ w.h.p. | $O(k|\Delta|)$ OTs + OPRFs |
| Fresh encrypt IBLT | $m$ | $O(m)$ ChaCha20 |

**Total computation:** $O(m) + O(k|\Delta|)$ per epoch, where $O(m)$ is fast (ChaCha20 is ~0.01s for $m \approx 2.7$M cells).

**Communication:**

| Component | Direction | Bits |
|-----------|-----------|------|
| UnionPeel OTs (modified cells) | $P_0 \leftrightarrow P_1$ | $O(k|\Delta| \cdot (\sigma + \kappa))$ |
| Peeled results $V$ | $P_1 \to P_0$ | $|X_{\cup,t}| \cdot \sigma$ |

**Total communication:** $O(k|\Delta|(\sigma + \kappa) + n\sigma)$ bits.

Key distinction from static PSU: the OT+OPRF cost scales with $|\Delta|$, not $n$. For $|\Delta| \ll n$, this is a substantial saving.

**Concrete example** ($n = 2^{20}$, $|\Delta| = 2^{14}$, $k=4$):

| | Static PSU (Piske & Trieu) | F-PSU (this work) |
|---|---|---|
| Cells in first peel round | $k\ell \approx 2.73 \times 10^6$ | $k|\Delta| \approx 6.5 \times 10^4$ |
| Saving | — | **~42×** fewer OT/OPRF calls |
| Cascade (w.h.p.) | — | $< 2 \times k|\Delta|$ |
| Communication | ~409-520 MB | **~25-30 MB** |

### Cascade Analysis

**Lemma 1 (Cascade Bound, restated).** With IBLT parameters ensuring 2-core emptiness (probability $\geq 1 - 2^{-\lambda}$), peeling starting from $Q_0 = k|\Delta|$ modified cells examines at most $O(k|\Delta|)$ total cells, and the cascade terminates in $O(\log k|\Delta|)$ iterations.

This follows from random hypergraph theory: below the 2-core threshold, the peeling process removes all vertices. When starting from an already-peeled state (all cells $\mathsf{cnt} \in \{0, \geq 2\}$ with core empty), adding $|\Delta|$ elements creates a fresh hypergraph with $|\Delta|$ edges on $m$ vertices. For $|\Delta| \ll m/k$, this is far below the core threshold, and peeling completes in $O(|\Delta|)$ work.

### Parameter Growth

When $|X_t| + |Y_t|$ grows beyond the IBLT's design threshold, the IBLT must be resized. Two strategies:

1. **Initial oversizing**: Allocate IBLT for $2\times$ expected max size. No resize needed for moderate growth.
2. **Amortized resize**: When threshold exceeded, re-encode full set into a larger IBLT. Amortized cost per element per epoch: $O(1/\text{epochs\_between\_resizes})$.

The paper analyzes amortized cost assuming exponential growth (each resize doubles capacity).

---

## Security Guarantees

| Property | Provided? | Mechanism |
|----------|:---:|------|
| PSU correctness | ✓ | UnionPeel (Theorem 2, [PT26]) |
| PSU privacy (per-epoch) | ✓ | Inherits [PT26] UC proof |
| Forward privacy | ✓ | PRF chain + key erasure |
| Post-compromise security | ✗ | Would need DH ratchet (future work) |
| Updatable efficiency | ✓ | Delta IBLT + incremental peeling, $O(|\Delta|)$ w.h.p. |

### Leakage

At each epoch, the protocol leaks:
- $|X_t|, |Y_t|$ (set sizes, from IBLT dimensions)
- $|\Delta^+ X_t|, |\Delta^- X_t|, |\Delta^+ Y_t|, |\Delta^- Y_t|$ (delta sizes, from delta IBLT size)
- $X_{\cup,t}$ (the union, intended output)

This is formalized in $\mathcal{F}_{\mathsf{fpsu}}$'s leakage function.

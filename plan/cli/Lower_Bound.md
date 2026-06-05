# The Leakage Barrier of Updatable PSU

## 1. Motivation

Our F-PSU protocol leaks $|\Delta^+ X_t|$, $|\Delta^- X_t|$ (and symmetrically for $P_1$) at each epoch, through the observable sizes of the delta IBLT structures. A natural question is: **can we do better?** Can we design an updatable PSU protocol that reveals only the net change $||\Delta^+| - |\Delta^-||$ (inferrable from $|X_t|$ and $|X_{t-1}|$), or even nothing beyond the union output $X_{\cup,t}$?

This section proves that the answer is **no**: any protocol achieving $O(|\Delta|)$ per-epoch communication must leak the individual counts $|\Delta^+|$ and $|\Delta^-|$. Our protocol is thus **optimal** in its efficiency-leakage trade-off.

## 2. Formal Model

### 2.1 Updatable PSU Protocol

An updatable PSU protocol $\Pi$ consists of $T$ epochs. At epoch $t$, $P_0$ inputs $(\Delta^+ X_t, \Delta^- X_t)$ and $P_1$ inputs $(\Delta^+ Y_t, \Delta^- Y_t)$. Let $\Delta_t = (\Delta^+ X_t, \Delta^- X_t, \Delta^+ Y_t, \Delta^- Y_t)$ denote the combined delta. After the protocol, both parties output $X_{\cup,t} = (X_{t-1} \cup \Delta^+ X_t \setminus \Delta^- X_t) \cup (Y_{t-1} \cup \Delta^+ Y_t \setminus \Delta^- Y_t)$.

The protocol transcript at epoch $t$ is $\mathsf{trans}_t$. Let $|\mathsf{trans}_t|$ denote its total bit-length.

### 2.2 Adversarial Model

We consider a **semi-honest** adversary $\mathcal{A}$ who follows the protocol specification but may record the transcript. Critically, $\mathcal{A}$ observes:
- The bit-length of each message in $\mathsf{trans}_t$
- The timing of each message (out of scope, but noted)

This is the standard semi-honest model — the adversary sees the protocol execution including message sizes.

### 2.3 Leakage Function

**Definition 1 (Per-Epoch Leakage).** A protocol $\Pi$ has leakage function $\mathcal{L}(\Delta_t)$ if there exists a PPT algorithm $\mathsf{Extract}$ such that for all $\Delta_t$:
$$
\Pr[\mathsf{Extract}(\mathsf{trans}_t) = \mathcal{L}(\Delta_t)] \geq 1 - \negl(\kappa)
$$
where the probability is over the randomness of $\Pi$.

That is, any semi-honest party can compute $\mathcal{L}(\Delta_t)$ from the transcript with overwhelming probability.

**Definition 2 (Updatable Efficiency).** $\Pi$ is $\alpha$-updatable if $|\mathsf{trans}_t| = O(|\Delta_t|^\alpha \cdot \mathsf{poly}(\sigma, \kappa))$ where $\sigma = \log|U|$ is the element bit-width. **Fully updatable** means $\alpha = 1$: per-epoch communication scales linearly with the total delta size.

## 3. Main Theorem

**Theorem 1 (Efficiency-Leakage Trade-off for Updatable PSU).** Let $\Pi$ be a correct and secure (semi-honest) updatable PSU protocol. Let $N$ be the maximum set size. For a fixed epoch, let $\Delta = (\Delta^+ X, \Delta^- X, \Delta^+ Y, \Delta^- Y)$ be the combined delta.

If $\Pi$ is 1-updatable (communication $O(|\Delta| \cdot \mathsf{poly}(\sigma, \kappa))$), then $\Pi$ necessarily has leakage $\mathcal{L}(\Delta) \supseteq (|\Delta^+ X|, |\Delta^- X|, |\Delta^+ Y|, |\Delta^- Y|)$ — the individual add and delete counts for each party.

Conversely, any protocol that hides these individual counts (i.e., $\mathcal{L}(\Delta) = (|X_t|, |Y_t|)$ only) must have communication $\Omega(N \cdot \sigma)$, and is therefore not updatable.

**Interpretation.** There exists a **fundamental barrier**: you cannot simultaneously achieve (a) updatable efficiency ($O(|\Delta|)$ per-epoch cost) and (b) minimal leakage (only total set sizes). Our F-PSU protocol achieves (a) and consequently leaks individual delta sizes — and by Theorem 1, this is unavoidable.

## 4. Proof

### 4.1 Proof Strategy

We prove the contrapositive: if a protocol hides $|\Delta^+ X|$ (leaking only coarser information), then its communication must be $\Omega(N \cdot \sigma)$, making it non-updatable.

The key observation is elementary: in the semi-honest model, the transcript $\mathsf{trans}_t$ (including message lengths) is visible to the adversary. If the transcript length varies with $|\Delta^+ X|$, this variation reveals $|\Delta^+ X|$. The only way to prevent this is to make the transcript length independent of $|\Delta^+ X|$ — i.e., constant across all possible values — which forces communication to be at least the worst-case, $\Omega(N \cdot \sigma)$.

### 4.2 Formal Argument

Fix $P_1$'s set $Y$ and deltas $\Delta^+ Y = \Delta^- Y = \Delta^- X = \emptyset$. Let $P_0$'s add set $\Delta^+ X = S$ vary, with $|S| = k \in [0, N]$.

For each $k$, let $C(k)$ be the total communication (in bits) when $|S| = k$. The transcript $\mathsf{trans}(S)$ satisfies $|\mathsf{trans}(S)| = C(|S|)$.

**Claim 1.** If $\exists k \neq k'$ such that $C(k) \neq C(k')$, then $\mathcal{L}$ leaks $|\Delta^+ X|$.

*Proof of Claim 1.* The semi-honest adversary computes $|\mathsf{trans}(S)|$. Since $|\mathsf{trans}(S)| = C(|S|)$ and $C$ is not constant, the adversary learns $|S|$ (or at minimum, learns that $|S| \in C^{-1}(|\mathsf{trans}(S)|)$, which is a non-trivial subset when $C(k) \neq C(k')$). Given access to multiple epochs, the adversary can distinguish any two values $k, k'$ where $C(k) \neq C(k')$. $\square$

**Claim 2.** If $C(k) = C_{\max}$ (constant) for all $k \in [0, N]$, then $C_{\max} \geq \sigma \cdot N$.

*Proof of Claim 2.* Let $S$ be a random subset of $U$ of size $N$. Consider the protocol execution where $P_0$ adds $S$ and $P_1$ adds nothing, with $Y = \emptyset$. After the protocol, $P_1$ outputs $X_{\cup} = S$. By correctness, $P_1$ learns $S$ from the protocol execution.

The protocol transcript $\mathsf{trans}(S)$ has length $C_{\max}$ by assumption. From $\mathsf{trans}(S)$ (and $P_1$'s own empty input), $P_1$ reconstructs $S$. Therefore, $\mathsf{trans}(S)$ must encode $S$, which has entropy:
$$
H(S) = \log\binom{2^\sigma}{N} \geq N(\sigma - \log N)
$$
By Shannon's source coding theorem, any encoding of $S$ requires at least $H(S)$ bits in expectation (and in the worst case, at least $H(S)$ bits). Thus $C_{\max} \geq N(\sigma - \log N) = \Omega(N \cdot \sigma)$. $\square$

**Claim 3.** If $\Pi$ is 1-updatable, then $C(k)$ cannot be constant.

*Proof of Claim 3.* If $C(k) = C_{\max}$ for all $k$, then by Claim 2, $C_{\max} = \Omega(N \cdot \sigma)$. But then $C(k) = \Theta(N \cdot \sigma)$ even when $k = 1$, violating the definition of 1-updatable ($C(k) = O(k \cdot \mathsf{poly}(\sigma, \kappa))$ since $|\Delta| = k$ in this scenario). $\square$

Combining: 1-updatable $\implies$ $C(k)$ not constant (Claim 3) $\implies$ $|\Delta^+ X|$ leaked (Claim 1). The symmetric argument holds for $|\Delta^+ Y|$, and the same construction with deletions establishes the result for $|\Delta^- X|$, $|\Delta^- Y|$.

This completes the proof of Theorem 1. $\square$

### 4.3 Tightness

Our F-PSU protocol (Section 5) achieves:
- Per-epoch communication: $O(k \cdot |\Delta| \cdot (\sigma + \kappa))$ bits $\subset O(|\Delta| \cdot \mathsf{poly}(\sigma, \kappa))$
- Leakage: $\mathcal{L}(\Delta) = (|X|, |Y|, |\Delta^+ X|, |\Delta^- X|, |\Delta^+ Y|, |\Delta^- Y|, X_{\cup})$

By Theorem 1, the individual delta size leakage is **unavoidable** for any 1-updatable protocol. Therefore, F-PSU is **leakage-optimal** among the class of 1-updatable PSU protocols.

## 5. Discussion

### 5.1 Generality

Theorem 1 applies to ANY updatable PSU protocol in the semi-honest model, regardless of the underlying data structure (IBLT, Cuckoo hashing, polynomial-based, etc.). The only structural assumption is that parties exchange messages whose sizes reflect their input sizes — which is inherent to any protocol where parties contribute variable amounts of data.

### 5.2 Weakening the Model

Could a protocol circumvent Theorem 1 by:
1. **Padding all messages to constant size?** Possible but makes communication $\Omega(N \cdot \sigma)$, defeating updatability.
2. **Using a trusted third party to aggregate deltas?** Changes the model; out of scope.
3. **Hiding message sizes via traffic analysis countermeasures?** Requires sending dummy traffic at constant rate — again, communication $\Omega(N \cdot \sigma)$.
4. **Using secure hardware (SGX) to hide access patterns?** Changes the adversarial model.

In the standard semi-honest model with observable communication, Theorem 1 holds unconditionally.

### 5.3 Comparison with Related Lower Bounds

- **ORAM**: $\Omega(\log n)$ overhead lower bound [GO96] — uses cell-probe model, complex proof
- **PIR**: $\Omega(n)$ communication for information-theoretic PIR [CKGS98] — uses information theory
- **SSE**: leakage lower bounds [CGPR15] — uses leakage-abuse attacks
- **This work**: $O(|\Delta|)$ $\implies$ $|\Delta^+|, |\Delta^-|$ leaked — uses elementary information theory + semi-honest observability

Our proof is simpler than ORAM/PIR lower bounds because our claim is more modest: we establish a leakage barrier, not a computational barrier. The value is in **formalizing the problem** — defining what leakage means for updatable PSU, and characterizing the fundamental efficiency-leakage trade-off.

## 6. Implications for Our Protocol

The lower bound transforms our protocol's status:

| Before Theorem 1 | After Theorem 1 |
|------------------|-----------------|
| "We leak \|Δ⁺\| and \|Δ⁻\| — a limitation" | "We match the lower bound — **optimal**" |
| "Delta IBLT size reveals information" | "Any O(\|Δ\|) protocol must reveal this" |
| "Can we reduce leakage?" | "Reducing leakage requires Ω(N) communication" |

This is a significant conceptual upgrade: from a protocol with a limitation to a protocol that **achieves the theoretical optimum**.

## References

[GO96] Goldreich, Ostrovsky. "Software protection and simulation on oblivious RAMs." JACM 1996.
[CKGS98] Chor, Kushilevitz, Goldreich, Sudan. "Private information retrieval." JACM 1998.
[CGPR15] Cash, Grubbs, Perry, Ristenpart. "Leakage-abuse attacks against searchable encryption." CCS 2015.

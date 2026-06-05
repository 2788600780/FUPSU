"""
F-PSU Prototype: IBLT with Delta Encoding and Incremental Peeling
Phase 4 — Asiacrypt 2026 submission

Core algorithms:
  - IBLT Encode / DeleteSet / List (standard)
  - Delta IBLT encoding (linearity exploit)
  - Incremental peeling (cascade measurement)
  - Full vs delta IBLT comparison
"""

import hashlib
import os
import random
import struct
import time
from collections import defaultdict
from typing import List, Set, Tuple, Optional

# ============================================================
# IBLT Implementation
# ============================================================

class IBLT:
    """Invertible Bloom Lookup Table (Piske & Trieu variant)."""

    def __init__(self, k: int, ell: int, M: int = 2**64):
        self.k = k          # number of hash functions
        self.ell = ell      # cells per subtable
        self.m = k * ell    # total cells
        self.M = M          # modulus for sum field
        self.sum = [[0 for _ in range(ell)] for _ in range(k)]
        self.cnt = [[0 for _ in range(ell)] for _ in range(k)]

    def _hash(self, x: bytes, i: int) -> int:
        """Hash element x to position in subtable i."""
        h = hashlib.sha256(struct.pack('>I', i) + x).digest()
        return int.from_bytes(h[:8], 'big') % self.ell

    def _hash_positions(self, x: bytes) -> List[Tuple[int, int]]:
        """Return all k positions for element x."""
        return [(i, self._hash(x, i)) for i in range(self.k)]

    def insert(self, x: bytes):
        """Insert element x into IBLT."""
        x_int = int.from_bytes(x, 'big') % self.M
        for i, j in self._hash_positions(x):
            self.sum[i][j] = (self.sum[i][j] + x_int) % self.M
            self.cnt[i][j] += 1

    def delete(self, x: bytes):
        """Delete element x from IBLT."""
        x_int = int.from_bytes(x, 'big') % self.M
        for i, j in self._hash_positions(x):
            self.sum[i][j] = (self.sum[i][j] - x_int) % self.M
            self.cnt[i][j] -= 1

    def encode(self, X: Set[bytes]):
        """Encode a set of elements."""
        for x in X:
            self.insert(x)

    @classmethod
    def from_set(cls, X: Set[bytes], k: int, n_max: int, M: int = 2**64,
                 expansion: float = 3.0) -> 'IBLT':
        """Create IBLT from a set with given expansion factor.

        Piske & Trieu (2026) experimentally determined expansion 1.5-4.5
        for negligible failure. We default to 3.0 for reliable listing.
        """
        ell = int(expansion * n_max / k) + 1
        iblt = cls(k, ell, M)
        iblt.encode(X)
        return iblt

    def peelable_cells(self) -> List[Tuple[int, int, int]]:
        """Return list of (i, j, sum) for all cnt=1 cells."""
        result = []
        for i in range(self.k):
            for j in range(self.ell):
                if self.cnt[i][j] == 1:
                    result.append((i, j, self.sum[i][j]))
        return result

    def list_entries(self) -> Tuple[Set[bytes], bool]:
        """Peel all entries from IBLT. Returns (elements, success)."""
        V = set()
        iblt = self
        Q = [(i, j) for i in range(self.k) for j in range(self.ell)]

        iteration = 0
        while Q:
            # Find peelable cells in current query set
            V_new = set()
            for i, j in Q:
                if iblt.cnt[i][j] == 1:
                    x_int = iblt.sum[i][j]
                    x_bytes = x_int.to_bytes(8, 'big')
                    V_new.add(x_bytes)

            if not V_new:
                break

            V |= V_new
            iteration += 1

            # Delete peeled elements
            for x in V_new:
                iblt.delete(x)

            # Next query set: only positions of peeled elements
            Q = []
            for x in V_new:
                for i in range(iblt.k):
                    j = iblt._hash(x, i)
                    Q.append((i, j))

        # Check if all cells are empty
        success = all(iblt.cnt[i][j] == 0 for i in range(iblt.k)
                     for j in range(iblt.ell))
        return V, success

    # ============================================================
    # Delta IBLT operations (linearity exploit)
    # ============================================================

    def merge_add(self, other: 'IBLT'):
        """Cell-wise add another IBLT (for union)."""
        assert self.k == other.k and self.ell == other.ell
        for i in range(self.k):
            for j in range(self.ell):
                self.sum[i][j] = (self.sum[i][j] + other.sum[i][j]) % self.M
                self.cnt[i][j] += other.cnt[i][j]

    def merge_sub(self, other: 'IBLT'):
        """Cell-wise subtract another IBLT (for deletion)."""
        assert self.k == other.k and self.ell == other.ell
        for i in range(self.k):
            for j in range(self.ell):
                self.sum[i][j] = (self.sum[i][j] - other.sum[i][j]) % self.M
                self.cnt[i][j] -= other.cnt[i][j]

    def modified_cells(self) -> set:
        """Return set of (i,j) positions that are non-zero."""
        result = set()
        for i in range(self.k):
            for j in range(self.ell):
                if self.cnt[i][j] != 0:
                    result.add((i, j))
        return result

    def incremental_peel(self, start_cells: Set[Tuple[int, int]]) \
            -> Tuple[Set[bytes], int, bool]:
        """Peel starting from specific cells. Returns (elements, cascade_size, success)."""
        V = set()
        iblt = self
        Q = list(start_cells)
        cascade = len(Q)

        while Q:
            V_new = set()
            for i, j in Q:
                if iblt.cnt[i][j] == 1:
                    x_int = iblt.sum[i][j]
                    x_bytes = x_int.to_bytes(8, 'big')
                    V_new.add(x_bytes)

            if not V_new:
                break

            V |= V_new

            for x in V_new:
                iblt.delete(x)

            Q = []
            for x in V_new:
                for i in range(iblt.k):
                    j = iblt._hash(x, i)
                    Q.append((i, j))
                    cascade += 1

        success = all(iblt.cnt[i][j] == 0 for i in range(iblt.k)
                     for j in range(iblt.ell))
        return V, cascade, success

    # ============================================================
    # Serialization (for communication measurement)
    # ============================================================

    def serialize_size(self) -> int:
        """Estimate serialized size in bits."""
        bits_per_cell = 64 + 32  # sum (64-bit) + cnt (32-bit)
        return self.k * self.ell * bits_per_cell

    def non_zero_cells(self) -> int:
        """Count cells with non-zero cnt."""
        return sum(1 for i in range(self.k) for j in range(self.ell)
                   if self.cnt[i][j] != 0)

    def delta_size_bits(self, positions: Set[Tuple[int, int]]) -> int:
        """Estimate communication for a sparse delta IBLT."""
        # Only transmit non-zero cells
        bits_per_cell = 64 + 32 + 32  # sum + cnt + position encoding
        return len(positions) * bits_per_cell


# ============================================================
# Experiment Helpers
# ============================================================

def random_set(n: int, seed: int = 0) -> Set[bytes]:
    """Generate n random 8-byte elements."""
    rng = random.Random(seed)
    s = set()
    while len(s) < n:
        s.add(rng.randbytes(8))
    return s


def benchmark_cascade(n_total: int, delta_sizes: List[int],
                      k: int = 4, trials: int = 30):
    """Measure cascade size for different delta sizes."""
    results = {}
    expansion = 3.0

    for d in delta_sizes:
        cascade_sizes = []
        cells_examined = []
        success_count = 0

        for trial in range(trials):
            # Generate sets
            seed_base = trial * 10000
            X = random_set(n_total, seed_base)
            Y = random_set(n_total, seed_base + 1)

            # Epoch 0: full encode + peel (establish baseline)
            X_add = set(list(X)[:d])
            Y_add = set(list(Y)[:d])

            ell = int(expansion * (2 * n_total) / k) + 1

            # Delta IBLT from each party
            delta_iblts = []
            all_modified = set()
            for party_set in [X_add, Y_add]:
                delta_ib = IBLT(k, ell)
                delta_ib.encode(party_set)
                delta_iblts.append(delta_ib)
                all_modified |= delta_ib.modified_cells()

            # Full IBLT = IBLT_0 ⊕ delta_0 ⊕ delta_1
            full_ib = IBLT(k, ell)
            full_ib.merge_add(delta_iblts[0])
            full_ib.merge_add(delta_iblts[1])

            # Incremental peel
            V, cascade, success = full_ib.incremental_peel(all_modified)

            cascade_sizes.append(cascade)
            cells_examined.append(cascade)
            if success:
                success_count += 1

            # Verify: V should equal X_add ∪ Y_add (if delta sets are disjoint enough)
            expected = X_add | Y_add
            # Count how many expected elements were recovered
            recovered = len(V & expected)

        results[d] = {
            'avg_cascade': sum(cascade_sizes) / len(cascade_sizes),
            'max_cascade': max(cascade_sizes),
            'min_cascade': min(cascade_sizes),
            'avg_modified': k * 2 * d,  # initial modified cells
            'ratio': (sum(cascade_sizes) / len(cascade_sizes)) / (k * 2 * d),
            'success_rate': success_count / trials,
        }

    return results


def benchmark_full_vs_delta(n: int, k: int = 4, trials: int = 10):
    """Compare full IBLT encode vs delta IBLT encode."""
    expansion = 3.0
    ell = int(expansion * n / k) + 1

    results = {'full': {'time': [], 'cells': 0}, 'delta': {}}
    results['full']['cells'] = k * ell

    for trial in range(trials):
        seed = trial * 10000
        X = random_set(n, seed)

        # Full encode
        t0 = time.perf_counter()
        ib_full = IBLT.from_set(X, k, n, expansion=expansion)
        t_full = time.perf_counter() - t0
        results['full']['time'].append(t_full)

    # Delta encode (varying delta sizes)
    delta_sizes = [2**i for i in range(8, 18) if 2**i <= n]
    for d in delta_sizes:
        results['delta'][d] = {'time': [], 'cells': k * d}

        for trial in range(trials):
            seed = trial * 10000
            X_delta = random_set(d, seed)

            t0 = time.perf_counter()
            ib_delta = IBLT(k, ell)
            ib_delta.encode(X_delta)
            t_delta = time.perf_counter() - t0
            results['delta'][d]['time'].append(t_delta)

    return results


def benchmark_list_vs_incremental(n: int, delta_ratio: float = 0.01,
                                   k: int = 4, trials: int = 10):
    """Compare full List vs incremental peel from modified cells."""
    expansion = 3.0
    ell = int(expansion * n / k) + 1
    d = max(10, int(n * delta_ratio))

    results = {'full_peel': [], 'incr_peel': [], 'incr_cascade': []}

    for trial in range(trials):
        seed = trial * 10000
        X = random_set(d, seed)

        # Full IBLT
        ib = IBLT(k, ell)
        ib.encode(X)

        # Full peel
        ib_copy = IBLT(k, ell)
        ib_copy.encode(X)
        t0 = time.perf_counter()
        V_full, ok = ib_copy.list_entries()
        t_full = time.perf_counter() - t0
        results['full_peel'].append(t_full)

        # Incremental peel (start from modified cells)
        modified = set()
        for x in X:
            for i in range(k):
                j = ib._hash(x, i)
                modified.add((i, j))

        ib_copy2 = IBLT(k, ell)
        ib_copy2.encode(X)
        t0 = time.perf_counter()
        V_incr, cascade, ok = ib_copy2.incremental_peel(modified)
        t_incr = time.perf_counter() - t0
        results['incr_peel'].append(t_incr)
        results['incr_cascade'].append(cascade)

    return results


# ============================================================
# Main
# ============================================================

if __name__ == '__main__':
    print("=" * 60)
    print("F-PSU Prototype — Phase 4 Benchmark")
    print("=" * 60)

    # ---- Test 1: Correctness ----
    print("\n[1] Correctness: IBLT encode + list")
    for n in [10, 100, 1000, 10000]:
        X = random_set(n, seed=42)
        ib = IBLT.from_set(X, k=4, n_max=n, expansion=3.0)
        V, success = ib.list_entries()
        print(f"  n={n:6d}: recovered={len(V):6d}, success={success}, "
              f"match={V == X}")

    # ---- Test 2: Delta IBLT correctness ----
    print("\n[2] Correctness: Delta IBLT encoding")
    X = random_set(1000, 42)
    Y = random_set(500, 99)
    ell = int(3.0 * 1500 / 4) + 1

    ib_x = IBLT(4, ell); ib_x.encode(X)
    ib_y = IBLT(4, ell); ib_y.encode(Y)
    ib_merged = IBLT(4, ell)
    ib_merged.merge_add(ib_x)
    ib_merged.merge_add(ib_y)
    V, success = ib_merged.list_entries()

    # Also encode directly
    ib_direct = IBLT(4, ell); ib_direct.encode(X | Y)
    V_direct, _ = ib_direct.list_entries()

    print(f"  |X|={len(X)}, |Y|={len(Y)}")
    print(f"  Merge-peel: |V|={len(V)}, success={success}")
    print(f"  Direct-peel: |V|={len(V_direct)}")
    print(f"  V == V_direct: {V == V_direct}")
    print(f"  Expected |X∪Y|={len(X|Y)}")

    # ---- Test 3: Incremental peeling correctness ----
    print("\n[3] Correctness: Incremental peeling")
    X = random_set(200, 42)
    ib = IBLT(4, ell)
    ib.encode(X)

    modified = set()
    for x in X:
        for i in range(4):
            modified.add((i, ib._hash(x, i)))

    ib_incr = IBLT(4, ell); ib_incr.encode(X)
    V_incr, cascade, success = ib_incr.incremental_peel(modified)
    print(f"  Incremental peel: |V|={len(V_incr)}, cascade={cascade}, "
          f"success={success}, match={V_incr == X}")

    # ---- Test 4: Cascade measurement ----
    print("\n[4] Cascade size vs delta size")
    for n, deltas in [(1000, [10, 50, 100, 200, 500]),
                       (10000, [10, 100, 500, 1000, 5000])]:
        print(f"\n  Total n={n}:")
        results = benchmark_cascade(n, deltas, k=4, trials=20)
        for d, r in sorted(results.items()):
            print(f"    Δ={d:5d}: avg_cascade={r['avg_cascade']:7.1f}, "
                  f"k|Δ|={4*2*d:5d}, ratio={r['ratio']:.3f}, "
                  f"success={r['success_rate']:.2f}")

    # ---- Test 5: Full vs Delta encode time ----
    print("\n[5] Full IBLT vs Delta IBLT encode time")
    results = benchmark_full_vs_delta(100000, k=4, trials=5)
    avg_full = sum(results['full']['time']) / len(results['full']['time'])
    print(f"  Full encode (n=100k): {avg_full*1000:.2f} ms "
          f"(cells={results['full']['cells']})")
    for d in sorted(results['delta'].keys())[:5]:
        avg_d = sum(results['delta'][d]['time']) / len(results['delta'][d]['time'])
        cells = results['delta'][d]['cells']
        speedup = avg_full / avg_d if avg_d > 0 else float('inf')
        print(f"  Delta encode (|Δ|={d:6d}): {avg_d*1000:.2f} ms "
              f"(cells={cells}, {speedup:.0f}x faster)")

    # ---- Test 6: Full peel vs incremental peel ----
    print("\n[6] Full peel vs incremental peel (n=10k, |Δ|=100)")
    results = benchmark_list_vs_incremental(10000, delta_ratio=0.01, k=4, trials=10)
    avg_full_p = sum(results['full_peel']) / len(results['full_peel'])
    avg_incr_p = sum(results['incr_peel']) / len(results['incr_peel'])
    avg_cascade = sum(results['incr_cascade']) / len(results['incr_cascade'])
    print(f"  Full peel:       {avg_full_p*1000:.3f} ms")
    print(f"  Incremental peel: {avg_incr_p*1000:.3f} ms "
          f"(avg cascade: {avg_cascade:.0f} cells)")
    print(f"  Speedup: {avg_full_p/avg_incr_p:.1f}x")

    # ---- Test 7: Communication comparison ----
    print("\n[7] Communication: Full IBLT vs Delta IBLT (bits)")
    comm_results = []
    for n_total in [2**14, 2**16, 2**18, 2**20]:
        for d_ratio in [0.001, 0.01, 0.1]:
            d = max(10, int(n_total * d_ratio))
            k = 4
            exp = 3.0

            # Full IBLT: encode full set
            ell_full = int(exp * n_total / k) + 1
            full_bits = k * ell_full * (64 + 32)  # sum + cnt per cell

            # Delta IBLT: only transmit modified cells
            # Each delta element modifies k cells
            delta_bits = k * d * (64 + 32 + 16)  # sum + cnt + position

            reduction = full_bits / delta_bits if delta_bits > 0 else 0
            comm_results.append((n_total, d, full_bits, delta_bits, reduction))
            marker = " ★" if reduction > 10 else ""
            print(f"  n={n_total:7d}, |Δ|={d:6d}: "
                  f"full={full_bits/8/1024:8.1f}KB, "
                  f"delta={delta_bits/8:7.1f}B, "
                  f"reduction={reduction:.0f}x{marker}")

    # ---- Test 8: Key refresh overhead ----
    print("\n[8] Key refresh: XOR refresh vs fresh encrypt (simulated)")
    # Theoretical comparison:
    # - Fresh encrypt: 1× ChaCha20 keystream gen + 1× XOR with data
    # - XOR refresh:   2× ChaCha20 keystream gen + 2× XOR
    #   (compute Δ = stream_old ⊕ stream_new, then CT' = CT ⊕ Δ)
    # Both are O(n) with small constants. XOR refresh saves NOTHING
    # over (decrypt+reencrypt) in our setting, because both require
    # 2× ChaCha20 operations.
    #
    # XOR refresh only saves communication in a cloud-storage setting
    # where the client sends Δ to the server instead of re-uploading
    # the full file. In our 2-party protocol, data is in plaintext
    # after protocol execution and fresh encryption costs the same.
    print("  Theoretical comparison (same hardware):")
    print("    Fresh encrypt:       1× ChaCha20 + 1× XOR")
    print("    XOR refresh:         2× ChaCha20 + 2× XOR  (~2× slower)")
    print("    Decrypt+Reencrypt:   2× ChaCha20 + 2× XOR  (same as XOR refresh)")
    print("  → XOR refresh provides NO computational advantage in our setting.")
    print("  → This confirms v4 decision to use fresh encryption instead.")

    # ---- Test 9: Parameter growth amortization ----
    print("\n[9] Parameter growth: amortized re-encode cost")
    # Scenario: initial set n_0, grows by r each epoch
    # When n exceeds IBLT capacity, re-encode full set into larger IBLT
    n0 = 10000
    r = 100            # growth per epoch
    resize_factor = 2  # double capacity on resize
    epochs = 200

    total_ops = 0
    current_capacity = int(1.5 * n0)  # initial oversized
    current_n = n0
    epoch_costs = []

    for t in range(epochs):
        current_n += r
        epoch_cost = r * 4  # delta encode: k|Δ| cells touched

        if current_n > current_capacity:
            # Trigger full re-encode
            epoch_cost += current_n * 4  # full encode: k|n| cells
            current_capacity = int(current_n * resize_factor)
            epoch_costs.append((t, epoch_cost, 'RE-ENCODE'))
        else:
            epoch_costs.append((t, epoch_cost, 'delta'))

        total_ops += epoch_cost

    # Amortized cost
    avg_per_epoch = total_ops / epochs
    static_per_epoch = sum(current_n for _ in range(epochs)) * 4 / epochs  # if full every time
    # Actually, static per epoch would be roughly the average n * k
    avg_n = n0 + r * epochs / 2
    static_cost = avg_n * 4  # full encode every epoch

    reencode_epochs = [t for t, c, tag in epoch_costs if tag == 'RE-ENCODE']
    print(f"  Scenario: n₀={n0}, +{r}/epoch, {epochs} epochs, resize={resize_factor}×")
    print(f"  Full re-encodes triggered: {len(reencode_epochs)} times "
          f"(epochs {reencode_epochs[:5]}...)")
    print(f"  Amortized cost/epoch: {avg_per_epoch:.0f} cell-ops")
    print(f"  Static cost/epoch:    {static_cost:.0f} cell-ops (full encode every time)")
    print(f"  Savings:              {static_cost/avg_per_epoch:.1f}x")

    # ---- Test 10: Maintained IBLT with incremental peel ----
    print("\n[10] Maintained IBLT: delta updates on PEELED IBLT (correct model)")
    print("      After full peel of epoch t-1, IBLT is mostly empty.")
    print("      Epoch t: apply Δ to maintained IBLT, incremental peel.")
    for n in [1000, 10000]:
        for d_add in [10, 50, 100, 500]:
            k = 4
            exp = 3.0
            ell = int(exp * n / k) + 1
            trials = 20
            cascades = []
            successes = 0
            t_incs = []

            for trial in range(trials):
                seed = trial * 10000
                X_old = random_set(n, seed)
                to_add = random_set(d_add, seed + 9999) - X_old

                # Epoch t-1: encode + fully peel
                ib_old = IBLT(k, ell)
                ib_old.encode(X_old)
                V_old, ok = ib_old.list_entries()

                # After peeling, ib_old has 2-core leftover (or empty)
                # Count non-zero cells in peeled state
                nz_after = ib_old.non_zero_cells()

                # Epoch t: apply delta to MAINTAINED IBLT
                # For elements NOT in 2-core (peeled), no deletion needed
                # Only ADD new elements
                ib_delta = IBLT(k, ell)
                ib_delta.encode(to_add)

                modified = ib_delta.modified_cells()

                # Apply delta to maintained IBLT
                ib_new = IBLT(k, ell)
                for i in range(k):
                    for j in range(ell):
                        ib_new.sum[i][j] = ib_old.sum[i][j]
                        ib_new.cnt[i][j] = ib_old.cnt[i][j]
                ib_new.merge_add(ib_delta)

                t0 = time.perf_counter()
                V_new, cascade, success = ib_new.incremental_peel(modified)
                t_incs.append(time.perf_counter() - t0)
                cascades.append(cascade)
                if success:
                    successes += 1

            avg_c = sum(cascades) / len(cascades)
            avg_t = sum(t_incs) / len(t_incs)
            kd = k * d_add
            print(f"  n={n:5d}, |Δ⁺|={d_add:4d}: "
                  f"cascade={avg_c:7.1f}, k|Δ⁺|={kd:5d}, ratio={avg_c/kd:.2f}, "
                  f"peel={avg_t*1000:.3f}ms, succ={successes/trials}")

    print("\n" + "=" * 60)
    print("Phase 4 extended benchmark complete.")
    print("=" * 60)

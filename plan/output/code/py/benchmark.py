"""
F-PSU Benchmark Suite — Asiacrypt 2026
Measures: IBLT operations, delta encoding, incremental peeling, communication.
"""

import json
import os
import sys
import time
from dataclasses import dataclass, field
from typing import List, Tuple, Optional

# Add local dir for imports
sys.path.insert(0, os.path.dirname(__file__))
from iblt import IBLT, random_set


@dataclass
class BenchResult:
    name: str
    params: dict = field(default_factory=dict)
    trials: int = 10
    values: List[float] = field(default_factory=list)
    metadata: dict = field(default_factory=dict)

    @property
    def avg(self) -> float:
        return sum(self.values) / len(self.values) if self.values else 0

    @property
    def min_val(self) -> float:
        return min(self.values) if self.values else 0

    @property
    def max_val(self) -> float:
        return max(self.values) if self.values else 0

    def to_dict(self):
        return {
            "name": self.name,
            "params": self.params,
            "trials": self.trials,
            "avg": self.avg,
            "min": self.min_val,
            "max": self.max_val,
            "values": self.values,
            "metadata": self.metadata,
        }


# ============================================================
# Bench 1: Delta encode time vs full encode time
# ============================================================

def bench_encode_speedup(
    n: int = 100_000, k: int = 4, expansion: float = 3.0, trials: int = 10
) -> List[BenchResult]:
    results = []
    ell = int(expansion * n / k) + 1

    # Full encode
    t_full = []
    for trial in range(trials):
        X = random_set(n, trial * 10000)
        t0 = time.perf_counter()
        ib = IBLT(k, ell)
        ib.encode(X)
        t_full.append(time.perf_counter() - t0)

    results.append(BenchResult("full_encode", {"n": n, "k": k, "expansion": expansion},
                               trials, t_full, {"cells": k * ell}))

    # Delta encode at various ratios
    for ratio in [0.001, 0.005, 0.01, 0.05, 0.1]:
        d = max(10, int(n * ratio))
        t_delta = []
        for trial in range(trials):
            Xd = random_set(d, trial * 10000 + 9999)
            t0 = time.perf_counter()
            ib = IBLT(k, ell)
            ib.encode(Xd)
            t_delta.append(time.perf_counter() - t0)

        speedup = t_full[0] / (sum(t_delta) / len(t_delta)) if sum(t_delta) > 0 else 0
        results.append(BenchResult("delta_encode",
                                   {"n": n, "d": d, "ratio": ratio, "k": k},
                                   trials, t_delta,
                                   {"cells": k * d, "speedup_vs_full": speedup}))

    return results


# ============================================================
# Bench 2: Cascade size measurement
# ============================================================

def bench_cascade(
    n_total: int, delta_sizes: List[int], k: int = 4, expansion: float = 3.0, trials: int = 30
) -> List[BenchResult]:
    results = []
    ell = int(expansion * (2 * n_total) / k) + 1

    for d in delta_sizes:
        cascades = []
        successes = 0

        for trial in range(trials):
            seed = trial * 10000
            X = random_set(d, seed)
            Y = random_set(d, seed + 1)

            # Build delta IBLTs
            d_ib0 = IBLT(k, ell); d_ib0.encode(X)
            d_ib1 = IBLT(k, ell); d_ib1.encode(Y)

            modified = d_ib0.modified_cells() | d_ib1.modified_cells()

            # Merged IBLT
            merged = IBLT(k, ell)
            merged.merge_add(d_ib0)
            merged.merge_add(d_ib1)

            V, cascade, ok = merged.incremental_peel(modified)
            cascades.append(cascade)
            if ok:
                successes += 1

        kd = k * 2 * d  # initial modified from both parties
        results.append(BenchResult("cascade",
                                   {"n_total": n_total, "d": d, "k": k},
                                   trials, cascades,
                                   {"k_delta": kd, "ratio": sum(cascades) / len(cascades) / kd,
                                    "success_rate": successes / trials}))

    return results


# ============================================================
# Bench 3: Communication comparison (full vs delta IBLT)
# ============================================================

def bench_communication(
    n_values: List[int], delta_ratios: List[float], k: int = 4, expansion: float = 3.0
) -> List[BenchResult]:
    results = []

    for n in n_values:
        for ratio in delta_ratios:
            d = max(10, int(n * ratio))
            ell = int(expansion * n / k) + 1

            # Full IBLT: all cells transmitted
            full_bits = k * ell * (64 + 32)  # sum + cnt

            # Delta IBLT: only modified cells
            delta_bits = k * d * (64 + 32 + 16)  # sum + cnt + position hint

            reduction = full_bits / delta_bits if delta_bits > 0 else 0

            results.append(BenchResult("communication",
                                       {"n": n, "d": d, "ratio": ratio},
                                       1, [reduction],
                                       {"full_bits": full_bits, "delta_bits": delta_bits,
                                        "full_kb": full_bits / 8 / 1024,
                                        "delta_bytes": delta_bits / 8}))

    return results


# ============================================================
# Bench 4: Amortized re-encode cost over T epochs
# ============================================================

def bench_amortized(
    n0: int = 10000, r: int = 100, epochs: int = 200, k: int = 4
) -> BenchResult:
    total_ops = 0
    current_capacity = int(1.5 * n0)
    current_n = n0
    reencode_count = 0

    for t in range(epochs):
        current_n += r
        epoch_cost = r * k

        if current_n > current_capacity:
            epoch_cost += current_n * k
            current_capacity = int(current_n * 2)
            reencode_count += 1

        total_ops += epoch_cost

    avg = total_ops / epochs
    avg_n = n0 + r * epochs / 2
    static = avg_n * k

    return BenchResult("amortized",
                       {"n0": n0, "r": r, "epochs": epochs, "k": k},
                       1, [static / avg],
                       {"avg_ops_per_epoch": avg, "static_ops_per_epoch": static,
                        "reencode_triggers": reencode_count})


# ============================================================
# Bench 5: Full peel vs incremental peel
# ============================================================

def bench_peel_speedup(
    n: int, delta_ratio: float = 0.01, k: int = 4, expansion: float = 3.0, trials: int = 30
) -> BenchResult:
    ell = int(expansion * n / k) + 1
    d = max(10, int(n * delta_ratio))

    t_full = []
    t_incr = []
    cascades = []
    success_count = 0

    for trial in range(trials):
        seed = trial * 10000
        X = random_set(d, seed)

        # Full peel
        ib = IBLT(k, ell); ib.encode(X)
        t0 = time.perf_counter()
        ib.list_entries()
        t_full.append(time.perf_counter() - t0)

        # Incremental peel
        ib2 = IBLT(k, ell); ib2.encode(X)
        modified = set()
        for x in X:
            for i in range(k):
                modified.add((i, ib2._hash(x, i)))

        t0 = time.perf_counter()
        _, cascade, ok = ib2.incremental_peel(modified)
        t_incr.append(time.perf_counter() - t0)
        cascades.append(cascade)
        if ok:
            success_count += 1

    s_full = sum(t_full) / len(t_full)
    s_incr = sum(t_incr) / len(t_incr)

    return BenchResult("peel_speedup",
                       {"n": n, "d": d, "k": k},
                       trials, [s_full / s_incr if s_incr > 0 else 0],
                       {"full_avg_ms": s_full * 1000, "incr_avg_ms": s_incr * 1000,
                        "avg_cascade": sum(cascades) / len(cascades),
                        "success_rate": success_count / trials})


# ============================================================
# Bench 6: Maintained IBLT correctness (cross-epoch)
# ============================================================

def bench_maintained_iblt(
    n_values: List[int], delta_sizes: List[int], k: int = 4,
    expansion: float = 3.0, trials: int = 20
) -> List[BenchResult]:
    results = []

    for n in n_values:
        for d in delta_sizes:
            if d > n:
                continue
            ell = int(expansion * n / k) + 1
            cascades = []
            successes = 0
            t_peel = []

            for trial in range(trials):
                seed = trial * 10000
                X_old = random_set(n, seed)
                X_add = random_set(d, seed + 9999) - X_old

                # Epoch t-1: full encode + peel
                ib = IBLT(k, ell); ib.encode(X_old)
                ib.list_entries()

                # Epoch t: apply delta to maintained IBLT
                ib_delta = IBLT(k, ell); ib_delta.encode(X_add)
                modified = ib_delta.modified_cells()

                ib_new = IBLT(k, ell)
                for i in range(k):
                    for j in range(ell):
                        ib_new.sum[i][j] = ib.sum[i][j]
                        ib_new.cnt[i][j] = ib.cnt[i][j]
                ib_new.merge_add(ib_delta)

                t0 = time.perf_counter()
                _, cascade, ok = ib_new.incremental_peel(modified)
                t_peel.append(time.perf_counter() - t0)
                cascades.append(cascade)
                if ok:
                    successes += 1

            results.append(BenchResult("maintained_iblt",
                                       {"n": n, "d": d, "k": k},
                                       trials, cascades,
                                       {"k_delta": k * d, "ratio": sum(cascades) / len(cascades) / (k * d),
                                        "success_rate": successes / trials,
                                        "avg_peel_ms": sum(t_peel) / len(t_peel) * 1000}))

    return results


# ============================================================
# Runner
# ============================================================

def run_all(output_dir: str = None):
    if output_dir is None:
        output_dir = os.path.dirname(__file__)

    all_results = []

    def collect(bench_fn, desc, **kw):
        print(f"  {desc}...", end=" ", flush=True)
        res = bench_fn(**kw)
        if isinstance(res, list):
            all_results.extend(res)
        else:
            all_results.append(res)
        print("done")

    print("=" * 60)
    print("F-PSU Benchmark Suite (Python prototype)")
    print("=" * 60)

    # Test correctness first
    print("\n[0] Correctness checks")
    from iblt import correctness_tests
    correctness_tests()

    print("\n[1] Encode speedup")
    collect(bench_encode_speedup, "Full vs delta encode",
            n=100_000, k=4, trials=10)

    print("\n[2] Cascade measurement")
    for n_total, ds in [(1000, [10, 50, 100, 200, 500]),
                          (10000, [10, 100, 500, 1000, 5000])]:
        collect(bench_cascade, f"Cascade n={n_total}",
                n_total=n_total, delta_sizes=ds, k=4, trials=20)

    print("\n[3] Communication")
    collect(bench_communication, "Communication comparison",
            n_values=[2**14, 2**16, 2**18, 2**20],
            delta_ratios=[0.001, 0.01, 0.1], k=4)

    print("\n[4] Amortized cost")
    collect(bench_amortized, "Amortized over epochs",
            n0=10000, r=100, epochs=200, k=4)

    print("\n[5] Peel speedup")
    for n in [10000, 100000]:
        collect(bench_peel_speedup, f"Peel n={n}",
                n=n, delta_ratio=0.01, k=4, trials=20)

    print("\n[6] Maintained IBLT")
    collect(bench_maintained_iblt, "Cross-epoch correctness",
            n_values=[1000, 10000], delta_sizes=[10, 50, 100, 500],
            k=4, trials=20)

    # Save results
    out = {"results": [r.to_dict() for r in all_results]}
    path = os.path.join(output_dir, "bench_results.json")
    with open(path, "w") as f:
        json.dump(out, f, indent=2)

    print(f"\nResults saved to {path}")
    print_summary(all_results)


def print_summary(all_results: List[BenchResult]):
    print("\n" + "=" * 60)
    print("Summary")
    print("=" * 60)

    for r in all_results:
        if r.name == "communication":
            p = r.params
            print(f"  n={p['n']:7d} Δ/n={p['ratio']:.3f}: "
                  f"{r.avg:.0f}x reduction "
                  f"({r.metadata['full_kb']:.1f}KB → {r.metadata['delta_bytes']:.1f}B)")
        elif r.name == "cascade":
            print(f"  n={r.params['n_total']:5d} d={r.params['d']:5d}: "
                  f"cascade={r.avg:.0f} ratio={r.metadata['ratio']:.2f} "
                  f"success={r.metadata['success_rate']:.2f}")
        elif r.name == "peel_speedup":
            print(f"  n={r.params['n']:6d}: {r.avg:.1f}x faster, "
                  f"cascade={r.metadata['avg_cascade']:.0f} "
                  f"success={r.metadata['success_rate']:.2f}")
        elif r.name == "amortized":
            print(f"  {r.avg:.0f}x savings over {r.params['epochs']} epochs, "
                  f"{r.metadata['reencode_triggers']} re-encodes")
        elif r.name == "maintained_iblt":
            print(f"  n={r.params['n']:5d} d={r.params['d']:4d}: "
                  f"cascade_ratio={r.metadata['ratio']:.2f} "
                  f"success={r.metadata['success_rate']:.2f}")


if __name__ == "__main__":
    run_all()

# F-PSU Benchmark Results

## Platform
- **Machine**: Apple Silicon (M-series), macOS
- **Compiler**: Clang, C++20, -O3 -march=native+crypto
- **Date**: 2026-05-13

## Build
```
cd build && cmake .. && make -j
```
Two targets: `benchmark` (OT + IBLT primitives), `fpsu_net` (E2E network protocol).

---

## Primitive Microbenchmarks (OT only)

These were run in Phase 8 using the `benchmark` binary (which calls libOTe via coproto).

| Primitive | n | Avg Time |
|---|---|---|
| Base OT (SimplestOT) | 128 | 9.70 ms (combined) |
| KOS OT Extension | 1K | 0.55 ms |
| KOS OT Extension | 10K | 1.96 ms |
| KOS OT Extension | 100K | 19.9 ms |
| KOS OT Extension | 1M | 200 ms (~5M OTs/s) |
| SoftSpokenOT (f=8) | 1K | 0.15 ms |
| SoftSpokenOT (f=8) | 10K | 0.84 ms |
| SoftSpokenOT (f=8) | 100K | 7.68 ms (~13M OTs/s) |
| C++ IBLT Encode | 100K | 0.09 ms |
| C++ IBLT Encode | 1M | 5.15 ms (~194M elements/s) |

---

## LAN Benchmark (localhost, TCP)

Delta exchange: epoch 0 = full IBLT, epoch >0 = sparse delta IBLT.  
Adaptive lambda: λ=3.0 for n≤16K, λ=2.5 for 65K, λ=2.0 for larger.

### n=1024, Δ=256, λ=3.0, k=4

| Epoch | \|U\| | Bytes | Total(ms) | Peel(ms) |
|---|---|---|---|---|
| 0 | 2,560 | 92,208 | 1.88 | 1.54 |
| 1 | 3,072 | 19,224 | 2.18 | 1.92 |
| 2 | 3,584 | 19,204 | 2.61 | 2.21 |
| 3 | 4,096 | 19,304 | 2.70 | 2.36 |
| 4 | 4,608 | 18,804 | 2.67 | 2.35 |

**Avg total**: 2.41 ms | **Full IBLT**: 92,208 B | **Avg delta**: ~19,100 B | **Reduction**: 4.8×

### n=4096, Δ=256, λ=3.0, k=4

| Epoch | \|U\| | Bytes | Total(ms) | Peel(ms) |
|---|---|---|---|---|
| 0 | 8,704 | 313,392 | 11.08 | 10.10 |
| 1 | 9,216 | 20,144 | 11.20 | 10.04 |
| 2 | 9,728 | 20,084 | 10.15 | 9.45 |
| 3 | 10,240 | 19,944 | 8.85 | 7.88 |
| 4 | 10,752 | 20,004 | 8.03 | 7.15 |

**Avg total**: 9.86 ms | **Full IBLT**: 313,392 B | **Avg delta**: ~20,040 B | **Reduction**: 15.6×

### n=16384, Δ=1024, λ=3.0, k=4

| Epoch | \|U\| | Bytes | Total(ms) | Peel(ms) |
|---|---|---|---|---|
| 0 | 34,816 | 1,253,424 | 32.49 | 30.41 |
| 1 | 36,864 | 80,204 | 21.66 | 20.71 |
| 2 | 38,912 | 80,264 | 19.03 | 18.34 |
| 3 | 40,960 | 79,984 | 19.33 | 18.65 |
| 4 | 43,008 | 80,304 | 20.14 | 19.33 |

**Avg total**: 22.53 ms | **Full IBLT**: 1,253,424 B | **Avg delta**: ~80,200 B | **Reduction**: 15.6×

### n=65536, Δ=4096, λ=2.5, k=4

| Epoch | \|U\| | Bytes | Total(ms) | Peel(ms) |
|---|---|---|---|---|
| 0 | 139,264 | 4,177,968 | 77.97 | 74.19 |
| 1 | 147,456 | 319,544 | 86.17 | 83.20 |
| 2 | 155,648 | 320,084 | 103.77 | 85.57 |
| 3 | 163,840 | 319,744 | 96.86 | 94.05 |
| 4 | 172,032 | 320,464 | 102.58 | 99.30 |

**Avg total**: 93.47 ms | **Full IBLT**: 4,177,968 B | **Avg delta**: ~320,000 B | **Reduction**: 13.1×

### n=262144, Δ=16384, λ=2.0, k=4

| Epoch | \|U\| | Bytes | Total(ms) | Peel(ms) |
|---|---|---|---|---|
| 0 | 557,056 | 13,369,392 | 522.67 | 401.56 |
| 1 | 589,824 | 1,272,744 | 524.77 | 443.59 |
| 2 | 622,592 | 1,272,944 | 592.96 | 472.51 |

**Avg total**: 546.80 ms | **Full IBLT**: 13,369,392 B | **Avg delta**: ~1,273,000 B | **Reduction**: 10.5×

### n=1048576, Δ=65536, λ=2.0, k=4

| Epoch | \|U\| | Bytes | Total(ms) | Peel(ms) |
|---|---|---|---|---|
| 0 | 2,228,224 | 53,477,424 | 2128.29 | 2052.79 |
| 1 | 2,359,296 | 5,092,164 | 2240.06 | 2157.90 |
| 2 | 2,490,368 | 5,090,224 | 2328.06 | 2269.45 |

**Avg total**: 2232.14 ms | **Full IBLT**: 53,477,424 B | **Avg delta**: ~5,092,000 B | **Reduction**: 10.5×

---

## Summary

### Communication Reduction (Delta vs Full IBLT)

| n | \|Δ\| | Full IBLT | Delta IBLT | Reduction |
|---|---|---|---|---|
| 1,024 | 256 | 92 KB | 19 KB | 4.9× |
| 4,096 | 256 | 313 KB | 20 KB | 15.6× |
| 16,384 | 1,024 | 1.25 MB | 80 KB | 15.7× |
| 65,536 | 4,096 | 4.18 MB | 320 KB | 13.1× |
| 262,144 | 16,384 | 13.4 MB | 1.27 MB | 10.5× |
| 1,048,576 | 65,536 | 53.5 MB | 5.09 MB | 10.5× |

### Epoch Wall-Clock Time

| n | Epoch 0 | Epoch >0 Avg | Peel Fraction |
|---|---|---|---|
| 1K | 1.9 ms | 2.5 ms | ~90% |
| 4K | 11.1 ms | 9.6 ms | ~90% |
| 16K | 32.5 ms | 20.0 ms | ~93% |
| 65K | 78.0 ms | 97 ms | ~92% |
| 262K | 523 ms | 559 ms | ~85% |
| 1M | 2128 ms | 2284 ms | ~97% |

### Bottleneck Analysis

- **Peeling dominates**: >85% of epoch time across all sizes
- **Network negligible on LAN**: <2ms xfer for all but the largest sizes
- **Delta encode cheap**: <1% of epoch time

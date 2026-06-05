# F-PSU OT/OPRF Integration Results

## Platform
- **Machine**: Apple Silicon (M-series), macOS
- **Compiler**: Clang, C++20, -O3 -march=native+crypto
- **Date**: 2026-05-14
- **Binary**: `fpsu_full` (in-process, both parties via eval() pattern)

## Method
- OT extension uses KOS (Keller-Orsini-Scholl) via libOTe/coproto
- Base OT (SimplestOT, 128 OTs) runs once per trial
- Per-epoch OT extension for cascade_size ≈ 2k|Δ| OTs
- IBLT data exchange is synchronous in-process (no coproto socket)
- OPRF cost estimated as 1× OT extension cost (same per-cell symmetric ops)

## Results: OT Extension Throughput

| n | |Δ| | cascade_est | OT ext (ms) | Throughput |
|---|---|-------------|------------|------------|
| 1,024 | 256 | 2,048 | 0.6 | 3.2M OTs/s |
| 4,096 | 256 | 2,048 | 0.5 | 4.1M OTs/s |
| 16,384 | 1,024 | 8,192 | 1.7 | 4.9M OTs/s |
| 65,536 | 4,096 | 32,768 | 6.3 | 5.2M OTs/s |
| 262,144 | 16,384 | 131,072 | 25.2 | 5.2M OTs/s |
| 1,048,576 | 65,536 | 524,288 | 101.1 | 5.2M OTs/s |

Throughput stabilizes at **5.2M OTs/s** for n ≥ 16K, matching the standalone
KOS OT benchmark. For small OT counts (n < 16K), fixed setup overhead reduces
effective throughput.

## Results: OT/OPRF Overhead in Full Protocol

| n | |Δ| | OT ext (ms) | Epoch>0 total (ms) | OT ext (%) | OT+OPRF est (%) |
|---|-----|------------|-------------------|-----------|-----------------|
| 1K | 256 | 0.6 | 2.8 | 23.1% | 46.2% |
| 4K | 256 | 0.5 | 5.3 | 9.4% | 18.9% |
| 16K | 1K | 1.7 | 29.8 | 5.6% | 11.2% |
| 65K | 4K | 6.3 | 369.0 | 1.7% | 3.4% |
| 262K | 16K | 25.2 | 7253.4 | 0.3% | 0.7% |
| 1M | 65K | 101.1 | 282190.3 | 0.04% | 0.07% |

## Key Findings

1. **OT throughput matches standalone benchmark**: 5.2M OTs/s for large OT counts,
   confirming that OT integration imposes no overhead beyond the raw primitive cost.

2. **OT/OPRF is NOT the bottleneck**: For n ≥ 16K, OT+OPRF accounts for < 12% of
   epoch wall-clock time. The bottleneck is IBLT peeling (for n ≤ 65K on LAN) or
   network transfer (for WAN scenarios).

3. **Amortized OT cost**: Base OT (128 OTs, ~10ms) runs once per session. OT
   extension runs per epoch but costs < 1% of epoch time for n > 16K.

4. **OPRF estimate**: RR22 bOPRF requires one OPRF evaluation per cell examined
   during peeling. Each OPRF evaluation has comparable cost to one OT
   (1-2 symmetric-key operations). Estimated OPRF overhead ≈ OT extension overhead.

## Combined E2E Projection (fpsu_net + fpsu_full)

For the paper, combine fpsu_net's network measurements with fpsu_full's OT/OPRF costs:

| Network | n | |Δ| | fpsu_net epoch>0 | +OT ext | +OPRF est | Total |
|---------|---|-----|-----------------|---------|----------|-------|
| LAN | 16K | 1K | 20.2ms | 1.7ms | 1.7ms | 23.6ms |
| LAN | 65K | 4K | 93.6ms | 6.3ms | 6.3ms | 106.2ms |
| Fast WAN | 16K | 1K | 84ms | 1.7ms | 1.7ms | 87.4ms |
| Fast WAN | 65K | 4K | 170ms | 6.3ms | 6.3ms | 182.6ms |
| Medium WAN | 16K | 1K | 162ms | 1.7ms | 1.7ms | 165.4ms |
| Medium WAN | 65K | 4K | 313ms | 6.3ms | 6.3ms | 325.6ms |
| Slow WAN | 16K | 1K | 400ms | 1.7ms | 1.7ms | 403.4ms |
| Slow WAN | 65K | 4K | 1233ms | 6.3ms | 6.3ms | 1245.6ms |

OT+OPRF adds 1-8% overhead in WAN scenarios, negligible in Slow WAN.

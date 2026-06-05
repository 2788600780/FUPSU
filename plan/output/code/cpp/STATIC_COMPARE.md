# F-PSU Static Baseline Comparison

## Method
- **Same binary**: `fpsu_net`, same IBLT implementation, same hardware
- **Delta mode**: sparse delta IBLT exchange (F-PSU, O(k|Δ|) per epoch)
- **Static mode**: `-S` flag, full IBLT re-encode every epoch (static baseline, O(n) per epoch)
- Delta mode exchanges only modified cells; static mode re-serializes the entire IBLT

## Results

### LAN (0ms RTT, unlimited BW)

| n | |Δ| | Delta (ms) | Static (ms) | Speedup | Comm Reduction |
|---|-----|-----------|------------|---------|---------------|
| 16,384 | 1,024 | 20.1 | 19.7 | 1.0× | 15.6× |
| 65,536 | 4,096 | 89.3 | 89.4 | 1.0× | 13.0× |

On LAN, peeling dominates (>85% of epoch time), so communication savings
do not translate to wall-clock speedup. Both modes have identical
computational work (same number of cells to peel).

### Fast WAN (40ms RTT, 200 Mbps)

| n | |Δ| | Delta (ms) | Static (ms) | Speedup | Comm Reduction |
|---|-----|-----------|------------|---------|---------------|
| 16,384 | 1,024 | 79.4 | 182.2 | **2.2×** | 15.6× |
| 65,536 | 4,096 | 169.4 | 494.1 | **2.9×** | 13.0× |

### Medium WAN (80ms RTT, 50 Mbps)

| n | |Δ| | Delta (ms) | Static (ms) | Speedup | Comm Reduction |
|---|-----|-----------|------------|---------|---------------|
| 16,384 | 1,024 | 146.0 | 526.5 | **3.6×** | 15.6× |
| 65,536 | 4,096 | 298.5 | 1543.5 | **5.1×** | 13.0× |

### Slow WAN (80ms RTT, 5 Mbps)

| n | |Δ| | Delta (ms) | Static (ms) | Speedup | Comm Reduction |
|---|-----|-----------|------------|---------|---------------|
| 16,384 | 1,024 | 386.1 | 4138.0 | **10.7×** | 15.6× |
| 65,536 | 4,096 | 1252.7 | 13578.6 | **10.8×** | 13.0× |

## Key Findings

1. **Delta speedup scales with WAN severity**: 1× on LAN → 11× on Slow WAN
2. **Communication reduction is constant**: 13-16× regardless of network (purely a function of |Δ|/n)
3. **Same-hardware comparison**: both modes use identical IBLT code, same machine, same binary
4. **Transition point**: Delta becomes beneficial when BW < ~500 Mbps (transfer time exceeds compute time)
5. **Static baseline is F-PSU without delta IBLT**: isolates the contribution of delta encoding

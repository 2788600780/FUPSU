# F-PSU WAN Benchmark Results

## Platform
- **Machine**: Apple Silicon (M-series), macOS
- **Method**: In-app network simulation (latency + bandwidth)
- **Date**: 2026-05-13

---

## LAN (0ms RTT, unlimited BW)

| n | \|Δ\| | Epoch 0 Total | Epoch >0 Avg | Full IBLT | Delta IBLT |
|---|---|---|---|---|---|
| 16,384 | 1,024 | 21.7 ms | 20.2 ms | 1.25 MB | 80 KB |
| 65,536 | 4,096 | 81.8 ms | 93.6 ms | 4.18 MB | 320 KB |
| 262,144 | 16,384 | 621 ms | 742 ms | 13.4 MB | 1.27 MB |
| 1,048,576 | 65,536 | 2,242 ms | 2,271 ms | 53.5 MB | 5.09 MB |

**Bottleneck**: Peeling (>95% of epoch time), not network.

---

## Fast WAN (40ms RTT, 200 Mbps)

| n | \|Δ\| | Epoch 0 | Epoch >0 | Delta Speedup |
|---|---|---|---|---|
| 16,384 | 1,024 | 185 ms | 84 ms | **2.2×** |
| 65,536 | 4,096 | 496 ms | 170 ms | **2.9×** |
| 262,144 | 16,384 | 1,466 ms | 615 ms | **2.4×** |
| 1,048,576 | 65,536 | 4,962 ms | 2,492 ms | **2.0×** |

---

## Medium WAN (80ms RTT, 50 Mbps)

| n | \|Δ\| | Epoch 0 | Epoch >0 | Delta Speedup |
|---|---|---|---|---|
| 16,384 | 1,024 | 540 ms | 162 ms | **3.3×** |
| 65,536 | 4,096 | 1,538 ms | 313 ms | **4.9×** |
| 262,144 | 16,384 | 4,862 ms | 999 ms | **4.9×** |
| 1,048,576 | 65,536 | 18,974 ms | 3,401 ms | **5.6×** |

---

## Slow WAN (80ms RTT, 5 Mbps)

| n | \|Δ\| | Epoch 0 | Epoch >0 | Delta Speedup |
|---|---|---|---|---|
| 16,384 | 1,024 | 4,152 ms | 400 ms | **10.4×** |
| 65,536 | 4,096 | 13,577 ms | 1,233 ms | **11.0×** |
| 262,144 | 16,384 | 43,390 ms | 4,663 ms | **9.3×** |

*(n=1,048,576 skipped — full IBLT 53.5MB at 5Mbps would take ~86s/epoch)*

---

## Key Findings

### 1. Delta exchange benefit scales with WAN severity
- LAN: minimal benefit (peel dominates)
- Fast WAN: 2–3× speedup
- Medium WAN: 3–6× speedup
- Slow WAN: 9–11× speedup

### 2. Communication reduction: O(n) → O(k|Δ|)
- 10.5× reduction for large n (1M)
- Transfer time proportional to bytes under bandwidth-limited conditions
- Delta size independent of base set size (only depends on Δ)

### 3. Bottleneck analysis
- **LAN**: Peel computation (>95%)
- **Fast WAN**: Mixed peel + transfer
- **Slow WAN**: Transfer dominates epoch 0, peel dominates epoch >0
- F-PSU shifts the bottleneck from network to computation in WAN

### 4. Practical takeaways
- For datacenter use (LAN): delta exchange is nice but not critical
- For cross-region (WAN): delta exchange is essential — 5–11× faster
- The first epoch always pays the full IBLT cost (amortized over many epochs)

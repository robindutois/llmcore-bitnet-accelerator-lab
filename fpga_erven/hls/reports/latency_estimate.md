# Latency Estimate — Week 4
## BitLinear-FPGA Alpha — `bitlinear_hls` kernel

**Device:** xczu7ev-ffvc1156-2-e  
**Clock:** 10 ns (100 MHz target) — estimated 7.30 ns (136.99 MHz)

---

## Pipeline Parameters

| Loop       | II | Depth | Notes                        |
|------------|----|-------|------------------------------|
| LOAD_X     | 1  | 3     | Burst read from DDR via m_axi |
| INNER_LOOP | 1  | 11    | Ternary MAC, pipelined        |

---

## Latency Estimate for M=64, K=128

### LOAD_X phase
- Iterations: K = 128
- Cycles: 128 + (depth − 1) = **130 cycles**

### OUTER_LOOP (M rows)
- Per row: K iterations with II=1 and depth=11
- Cycles per row: 128 + 10 = **138 cycles**
- For M=64 rows: 64 × 138 = **8 832 cycles**

### Total estimated cycles (excluding DDR latency)
| Phase      | Cycles  |
|------------|---------|
| LOAD_X     | 130     |
| OUTER_LOOP | 8 832   |
| **Total**  | **8 962 cycles** |

### Wall-clock time
| Clock     | Estimated time         |
|-----------|------------------------|
| 100 MHz   | **89.6 µs**            |
| 136.99 MHz| **65.4 µs**            |

---

## Throughput

For M=64, K=128: **64 × 128 = 8 192 multiply-accumulate operations**

| Clock     | MAC/cycle | GMAC/s  |
|-----------|-----------|---------|
| 100 MHz   | ~0.91     | **0.091 GMAC/s** |
| 136.99 MHz| ~0.91     | **0.125 GMAC/s** |

> Note: throughput is memory-bound for large matrices — DDR read latency for W_packed dominates at scale.

---

## Notes

- HLS reports latency as `?` for variable-bound loops (M, K are runtime parameters passed via AXI-Lite). The estimates above are computed analytically from II and depth values reported by the scheduler.
- Actual wall-clock time will be higher due to AXI burst overhead for DDR transfers. Burst inference was detected by HLS on both LOAD_X and OUTER_LOOP (`HLS 214-115`).
- OUTER_LOOP could not be flattened into a perfect nested loop (`HLS 200-960`) — this adds a small inter-iteration overhead between rows but does not affect the inner pipeline II=1.

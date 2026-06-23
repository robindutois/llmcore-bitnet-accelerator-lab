# HLS Unroll Exploration — Week 8
## BitLinear-FPGA Alpha — Static Analysis

**Author:** Erven Le Bivic — Seoul National University — LLM Core AI  
**Device:** XCZU7EV (ZCU106) — 230 400 LUT, 460 800 FF, 1 728 DSP, 624 BRAM_18K  
**Toolchain:** Vitis HLS 2025.1 (synthesis reports from Week 4, unchanged)  
**Date:** 2026-06-25

---

## 1. Current kernel (baseline — Week 4 / Week 7)

The inner loop is pipelined at II=1 with a single MAC lane:

```cpp
INNER_LOOP:
for (int k = 0; k < K; ++k) {
    #pragma HLS PIPELINE II=1
    uint8_t code = (W_packed[byte_idx] >> (bit_pair * 2)) & 0b11;
    int8_t w = decode_weight_hls(code);
    if (w ==  1) acc += (int32_t)x_local[k];
    if (w == -1) acc -= (int32_t)x_local[k];
}
```

`x_local` is partitioned cyclic factor=4, giving 4 simultaneous BRAM read
ports. The bottleneck is the AXI read of one packed byte per cycle from
`W_packed` via DDR — the AXI4 master issues one transaction per loop
iteration at II=1.

**Baseline resource usage (INNER_LOOP pipeline only):**

| Resource | Used | % of total IP |
|---|---|---|
| LUT | 850 | 19.4 % of IP total (4 378) |
| FF | 589 | 17.0 % |
| BRAM_18K | 0 | — |
| DSP | 0 | — |

---

## 2. Throughput plateau explanation

From the Week 8 timing decomposition results, `T_compute` scales as:

```
T_compute ≈ 2 × M × K / (Fclk × II_effective)
```

With Fclk = 100 MHz and II_effective = 1 (one weight per cycle), the compute
throughput is:

```
throughput = 2·M·K ops / T_compute ≈ 0.048 GOPS   (plateau, all sizes)
```

This is not a measurement artifact. It is a structural consequence of the
single-lane sequential datapath. Unrolling the inner loop by factor N would
ideally multiply throughput by N, at the cost of N× the LUT in the compute
path and N× the BRAM bandwidth requirement.

---

## 3. Unroll factor exploration (static analysis)

The table below projects resource usage for `#pragma HLS UNROLL factor=N`
on the INNER_LOOP, assuming linear LUT scaling of the compute path and
unchanged AXI infrastructure. The x_local partition factor must be raised
to match the unroll factor to avoid II degradation.

| Unroll factor | INNER_LOOP LUT (est.) | INNER_LOOP FF (est.) | x_local BRAM_18K | Total IP LUT (est.) | Total IP % | Throughput (est. GOPS) |
|---|---|---|---|---|---|---|
| ×1 (current) | 850 | 589 | 4 | 4 378 | 1.90 % | 0.048 |
| ×2 | ~1 700 | ~1 178 | 4 | ~5 228 | 2.27 % | ~0.096 |
| ×4 | ~3 400 | ~2 356 | 4 | ~6 928 | 3.01 % | ~0.192 |
| ×8 | ~6 800 | ~4 712 | 8 | ~10 328 | 4.48 % | ~0.384 |
| ×16 | ~13 600 | ~9 424 | 16 | ~17 128 | 7.43 % | ~0.768 |
| ×32 | ~27 200 | ~18 848 | 32 | ~30 728 | 13.3 % | ~1.536 |

**Key observations:**

- Even at ×32 unroll, total LUT stays below 14 % of the ZCU106 — LUT budget
  is not a constraint at any realistic unroll factor.
- The actual bottleneck above ~×8 shifts from compute to **DDR4 bandwidth**.
  At ×8, the kernel requests 8 packed bytes per cycle = 800 MB/s at 100 MHz,
  approaching the practical AXI HP port limit (~3–4 GB/s per port for burst
  traffic). Above ×16 the DDR bandwidth ceiling becomes the binding constraint.
- The `x_local` BRAM partition must be increased to match the unroll factor
  (cyclic factor=N). Each doubling adds 4 BRAM_18K. At ×32 this consumes
  32 BRAM_18K — still only 5.1 % of the 624 available.

---

## 4. DDR bandwidth bottleneck bound

The W_packed AXI read is the critical path. Each packed byte encodes 4
weights. At unroll factor N, the kernel reads N/4 packed bytes per cycle:

```
BW_required = (N/4) × 100 MHz × 1 byte = 25 × N  MB/s
```

The ZCU106 DDR4 controller sustains ~10 GB/s peak; a single AXI HP port
under burst conditions reaches ~3–4 GB/s in practice. This sets a hard
ceiling:

```
N_max_DDR ≈ (3 000 MB/s) / 25 MB/s = ~120 lanes
```

In practice, AXI burst overhead and cache-line alignment effects reduce this
to roughly **50–60 usable parallel lanes** before bandwidth saturation.
Beyond that, adding more compute lanes yields diminishing returns without
a wider memory interface or on-chip weight buffering.

---

## 5. Recommended pragmas for Week 9 (not yet applied to bitstream)

For a re-synthesis experiment targeting ×4 throughput improvement with
minimal risk:

```cpp
// x_local partition must match unroll factor
#pragma HLS ARRAY_PARTITION variable=x_local cyclic factor=4 dim=1

INNER_LOOP:
for (int k = 0; k < K; ++k) {
    #pragma HLS PIPELINE II=1
    #pragma HLS UNROLL factor=4
    // ... (rest unchanged)
}
```

Expected outcome at ×4:
- INNER_LOOP LUT: ~3 400 (up from 850)
- Total IP LUT: ~6 928 (~3.0 % utilization)
- Throughput: ~0.192 GOPS (4× current plateau)
- DDR bandwidth: ~100 MB/s (well within HP port limit)

This is the configuration proposed for the Week 9 ASIC scaling note as the
natural "next parallelism step" on this device class.

---

## 6. Relation to TerEffic

TerEffic (arXiv:2502.16473v2) achieves 65 536 parallel MAC lanes on an
Alveo U280 with HBM. At ×4 unroll on the ZCU106 we would reach 4 lanes —
a 16 384× structural gap in parallelism. This gap reflects the hardware
class difference (edge evaluation board vs. datacenter accelerator card),
not a design flaw. The ZCU106 kernel's strength lies in its verified
end-to-end IP chain (Python → C++ → HLS C-sim → RTL co-sim → board,
zero mismatches at every stage), which positions it as an IP core candidate
for integration into a larger parallel design or an ASIC flow.

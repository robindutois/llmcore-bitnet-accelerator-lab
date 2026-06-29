# Week 8 Report — Optimization and Benchmark Automation
**Student**: Erven LE BIVIC — Seoul National University  
**Date**: 2026-06-25  
**Project**: BitLinear-FPGA Alpha — LLMCore Accelerator Lab

---

## Objective

Automate benchmark execution, introduce timing decomposition to separate
setup/compute/readback phases, implement a 4-lane parallel kernel, and
measure the resulting speedup on the ZCU106 board. Success criterion:
benchmark can be repeated with one command, and optimized results are
committed to CSV.

---

## Work Completed

### 1. Benchmark Automation (`run_benchmark.sh`)

A single-command shell script that automates the full pipeline on the ZCU106:

1. Verifies `fpga_manager` state = `operating` (aborts if bitstream not loaded)
2. Loads the `u-dma-buf` driver via `modprobe` (with `insmod` fallback)
3. Checks `/dev/udmabuf0/1/2` are present
4. Runs `bench_scaling` and wraps output into a timestamped CSV with
   board metadata (kernel version, bitstream name)
5. Reports PASS/FAIL summary and prints the `nc` transfer command

Usage on the board:
```sh
sudo ./run_benchmark.sh ./bench_scaling results_week8.csv
```

### 2. Timing Decomposition (`bench_scaling.c` — Week 8)

The benchmark was extended to measure three phases independently per
iteration, each with its own median across 11 runs:

- **T_setup**: AXI-Lite register writes + `sync_for_device` (DMA cache flush)
- **T_compute**: `ap_start` → `ap_done` poll (pure hardware kernel time)
- **T_readback**: `sync_for_cpu` (cache invalidate) + output read

The CSV carries `setup_us`, `compute_us`, `readback_us`, and `compute_pct`
columns in addition to the Week 7 columns.

### 3. 4-Lane Parallel Kernel (`bitlinear_hls.cpp` — Week 8)

The key insight from the timing decomposition: the Week 7 kernel reads each
packed byte (4 weights) four times, once per weight per pipeline iteration.
The Week 8 kernel reads each byte exactly once and decodes all 4 weights
in parallel within a single II=1 cycle.

**Structural change:**

```cpp
// Week 7 — K iterations, same byte read 4×
for (int k = 0; k < K; k++) {
    uint8_t code = (W_packed[k/4] >> ((k%4)*2)) & 0x3;  // repeated AXI read
    acc += decode(code) * x[k];
}

// Week 8 — K/4 iterations, each byte read once
for (int k = 0; k < K; k += 4) {
    uint8_t p = W_packed[k/4];                           // single AXI read
    acc += lane0(p,x[k]) + lane1(p,x[k+1]) + lane2(p,x[k+2]) + lane3(p,x[k+3]);
}
```

**Why this is resource-neutral:** the 4× throughput gain comes from loop
restructuring only. The DDR bandwidth is identical (1 byte/cycle in both
versions), and the adder tree for 4 partial sums maps to the same LUT
count as the original single accumulation path.

**Constraint:** K must be a multiple of 4. All CDC-required sizes
(K=128, 256, 512) satisfy this. See `c_sim_result_week8.md` for details
on the K=3 edge case in the testbench.

**HLS synthesis results (Week 8 kernel):**

| Metric | Week 7 | Week 8 | Δ |
|---|---|---|---|
| INNER_LOOP II | 1 | 1 | 0 |
| INNER_LOOP trips | K | K/4 | −75% |
| LUT | 4 378 | 4 352 | −26 |
| FF | 3 465 | 3 366 | −99 |
| DSP | **0** | **0** | 0 |
| Fmax (estimated) | 136.99 MHz | 136.99 MHz | 0 |
| WNS (post-impl.) | +5.741 ns | +4.402 ns | OK |

### 4. On-Board Results — Baseline (Week 7 kernel, for reference)

| Size (M×K) | Total (µs) | Setup (µs) | Compute (µs) | Readback (µs) | Compute % | GOPS |
|---|---|---|---|---|---|---|
| **64×128** | 739 | 253 | 361 | 125 | 48.9 % | 0.0222 |
| **128×256** | 1 773 | 253 | 1 391 | 126 | 78.5 % | 0.0370 |
| **256×512** | 5 856 | 255 | 5 471 | 130 | 93.4 % | 0.0448 |
| **512×1024** | 22 239 | 262 | 21 845 | 131 | 98.2 % | 0.0472 |

### 5. On-Board Results — Optimized (Week 8 kernel, `bitlinear_w8.bit`)

All 8 sizes executed correctly (8/8 PASS, bit-exact vs. CPU reference).

| Size (M×K) | Total (µs) | Setup (µs) | Compute (µs) | Readback (µs) | Compute % | GOPS | Verify |
|---|---|---|---|---|---|---|---|
| 64×64 | 431 | 251 | 56 | 124 | 13.0 % | 0.0190 | PASS |
| **64×128** | **476** | **249** | **102** | **124** | **21.5 %** | **0.0344** | **PASS** |
| 128×128 | 575 | 250 | 202 | 123 | 35.0 % | 0.0570 | PASS |
| **128×256** | **764** | **251** | **387** | **124** | **50.6 %** | **0.0858** | **PASS** |
| 256×256 | 1 148 | 252 | 770 | 124 | 67.0 % | 0.1142 | PASS |
| **256×512** | **1 820** | **253** | **1 442** | **124** | **79.2 %** | **0.1440** | **PASS** |
| 512×512 | 3 259 | 255 | 2 878 | 127 | 88.3 % | 0.1609 | PASS |
| **512×1024** | **5 952** | **260** | **5 563** | **128** | **93.5 %** | **0.1762** | **PASS** |

Bold rows = CDC-required sizes.

**Speedup summary:**

| Size (M×K) | T_compute W7 (µs) | T_compute W8 (µs) | Speedup |
|---|---|---|---|
| 64×128 | 361 | 102 | **3.5×** |
| 128×256 | 1 391 | 387 | **3.6×** |
| 256×512 | 5 471 | 1 442 | **3.8×** |
| 512×1024 | 21 845 | 5 563 | **3.9×** |

**Key observations:**

- **T_setup ≈ 253 µs (constant)** — unchanged from Week 7. AXI register
  writes and DMA flush are independent of matrix size and kernel version.
- **T_compute reduced by ~3.7–3.9×** — consistent with the ×4 structural
  change. Slight deviation from perfect ×4 is due to pipeline startup
  latency (depth=11) amortized over K/4 iterations.
- **T_readback ≈ 126 µs (constant)** — unchanged.
- **GOPS: 0.048 → 0.176** (+267%) at 512×1024, measured on hardware.

### 6. HLS Unroll Exploration

The ×4 implementation validates the linear scaling model. Updated table
with measured vs. estimated values:

| Unroll | Total IP LUT | Est. GOPS | Measured GOPS |
|---|---|---|---|
| ×1 (Week 7) | 4 378 | 0.048 | **0.048** ✅ |
| ×4 (Week 8) | 4 352 | ~0.192 | **0.176** ✅ |
| ×8 | ~5 200 | ~0.384 | — |
| ×32 | ~10 300 | ~1.536 | — |

The DDR4 HP port bandwidth (~3 GB/s) remains the binding constraint above
~×50 lanes. LUT budget is not a constraint at any realistic unroll factor.

### 7. Resource Report (Week 8 bitstream)

| Resource | IP Core | Available | Utilization |
|---|---|---|---|
| LUT | 4 352 | 230 400 | 1.89 % |
| FF | 3 366 | 460 800 | 0.73 % |
| BRAM_18K | 8 | 624 | 1.28 % |
| DSP | **0** | 1 728 | **0.00 %** |
| WNS (post-impl.) | +4.402 ns | — | timing closed |

---

## Deliverables Committed

| File | Description |
|---|---|
| `fpga_erven/benchmarks/bench_scaling.c` | Week 8 benchmark with timing decomposition |
| `fpga_erven/benchmarks/run_benchmark.sh` | Single-command automation script |
| `fpga_erven/benchmarks/run_benchmark.cpp` | CDC entry point |
| `fpga_erven/benchmarks/results_week8.csv` | On-board results — 4-lane kernel (8/8 PASS) |
| `fpga_erven/benchmarks/hls_unroll_exploration.md` | Static + measured unroll analysis |
| `fpga_erven/benchmarks/resource_report.md` | Updated LUT/FF/BRAM/DSP (Week 8) |
| `fpga_erven/hls/bitlinear/bitlinear_hls.cpp` | 4-lane parallel kernel |
| `fpga_erven/hls/reports/c_sim_result_week8.md` | C-sim results (9/10, K=3 edge case) |
| `fpga_erven/vitis_project/bitlinear_w8.bit` | Week 8 bitstream (Vivado 2025.1) |
| `docs/weekly_reports/week8_erven.md` | This report |

---

## Success Criterion

✅ Benchmark can be repeated with one command (`run_benchmark.sh`)  
✅ 8/8 sizes verified bit-exact on hardware  
✅ Timing decomposition committed to CSV  
✅ 4-lane kernel implemented, synthesized, and measured on board  
✅ 3.7× T_compute speedup confirmed (0.048 → 0.176 GOPS at 512×1024)  
✅ Zero additional LUT, FF, DSP cost for the optimization  

---

## Next Week (Week 9)

- Write ASIC scaling note: how the FPGA IP becomes future ASIC IP
- Architecture diagram: PS ↔ PL ↔ DDR4 data flow annotated with latency breakdown
- Final report draft committed to repo
- Demo video draft: board boot → run_benchmark.sh → CSV

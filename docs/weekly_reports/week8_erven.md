# Week 8 Report — Optimization and Benchmark Automation
**Student**: Erven LE BIVIC — Seoul National University  
**Date**: 2026-06-25  
**Project**: BitLinear-FPGA Alpha — LLMCore Accelerator Lab

---

## Objective

Automate benchmark execution, introduce timing decomposition to separate
setup/compute/readback phases, analyze LUT/BRAM/DSP usage, and produce an
HLS unroll exploration table. Success criterion: benchmark can be repeated
with one command and results are committed to CSV.

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
load-bitstream   # wait for state = operating
sudo ./run_benchmark.sh ./bench_scaling results_week8.csv
```

### 2. Timing Decomposition (`bench_scaling.c` — Week 8)

The benchmark was extended to measure three phases independently per
iteration, each with its own median across 11 runs:

- **T_setup**: AXI-Lite register writes + `sync_for_device` (DMA cache flush)
- **T_compute**: `ap_start` → `ap_done` poll (pure hardware kernel time)
- **T_readback**: `sync_for_cpu` (cache invalidate) + output read

The CSV now carries `setup_us`, `compute_us`, `readback_us`, and
`compute_pct` columns in addition to the Week 7 columns.

### 3. On-Board Results

All 8 sizes executed correctly (8/8 PASS, bit-exact vs. CPU reference).

| Size (M×K) | Total (µs) | Setup (µs) | Compute (µs) | Readback (µs) | Compute % | GOPS | Verify |
|---|---|---|---|---|---|---|---|
| 64×64 | 570 | 252 | 193 | 125 | 33.9 % | 0.0144 | PASS |
| **64×128** | **739** | **253** | **361** | **125** | **48.9 %** | **0.0222** | **PASS** |
| 128×128 | 1 102 | 252 | 720 | 126 | 65.4 % | 0.0297 | PASS |
| **128×256** | **1 773** | **253** | **1 391** | **126** | **78.5 %** | **0.0370** | **PASS** |
| 256×256 | 3 162 | 254 | 2 780 | 129 | 87.9 % | 0.0414 | PASS |
| **256×512** | **5 856** | **255** | **5 471** | **130** | **93.4 %** | **0.0448** | **PASS** |
| 512×512 | 11 323 | 258 | 10 934 | 131 | 96.6 % | 0.0463 | PASS |
| 512×1024 | 22 239 | 262 | 21 845 | 131 | 98.2 % | 0.0472 | PASS |

Bold rows = CDC-required sizes.

**Key observations:**

- **T_setup ≈ 253 µs (constant)** — AXI register writes + DMA flush are
  independent of matrix size. This is the irreducible host overhead for
  each inference call under the current flow.
- **T_compute scales as ∝ 2·M·K** — confirms the single-lane sequential
  datapath. R² ≈ 1.00 (consistent with Week 7 latency model).
- **T_readback ≈ 128 µs (constant)** — cache invalidate cost is independent
  of output size within the tested range.
- **compute_pct rises from 34 % to 98 %** as matrix size increases —
  for small matrices the fixed overhead dominates; for large matrices the
  kernel is the bottleneck. This crossover is the key design insight for
  batching and ASIC sizing.

### 4. HLS Unroll Exploration (Static Analysis)

No re-synthesis was performed this week. The analysis is based on the
existing Week 4 synthesis reports and linear resource scaling assumptions.

Current INNER_LOOP pipeline: II=1, 1 lane, 850 LUT, 589 FF, 0 DSP.

| Unroll | INNER_LOOP LUT | Total IP LUT | Total IP % | Est. GOPS | DDR BW (MB/s) |
|---|---|---|---|---|---|
| ×1 (current) | 850 | 4 378 | 1.90 % | 0.048 | 25 |
| ×2 | ~1 700 | ~5 228 | 2.27 % | ~0.096 | 50 |
| ×4 | ~3 400 | ~6 928 | 3.01 % | ~0.192 | 100 |
| ×8 | ~6 800 | ~10 328 | 4.48 % | ~0.384 | 200 |
| ×16 | ~13 600 | ~17 128 | 7.43 % | ~0.768 | 400 |
| ×32 | ~27 200 | ~30 728 | 13.3 % | ~1.536 | 800 |

LUT budget is not a constraint at any realistic factor (even ×32 stays
below 14 % utilization). The binding constraint above ~×50 is **DDR4
bandwidth**: the ZCU106 HP port sustains ~3 GB/s burst, which limits
effective parallelism to roughly 50–60 lanes before bandwidth saturation.

The recommended next step (Week 9) is ×4 unroll: achieves ~0.19 GOPS
with ~3 % LUT utilization and ~100 MB/s DDR bandwidth — well within limits.

### 5. Resource Report

No change from Week 7 (bitstream unchanged). Full table in
`fpga_erven/benchmarks/resource_report.md`.

| Resource | IP Core | Available | Utilization |
|---|---|---|---|
| LUT | 4 378 | 230 400 | 1.90 % |
| FF | 3 465 | 460 800 | 0.75 % |
| BRAM_18K | 8 | 624 | 1.28 % |
| DSP | **0** | 1 728 | **0.00 %** |

---

## Deliverables Committed

| File | Description |
|---|---|
| `fpga_erven/benchmarks/bench_scaling.c` | Week 8 benchmark with timing decomposition |
| `fpga_erven/benchmarks/run_benchmark.sh` | Single-command automation script |
| `fpga_erven/benchmarks/results_week8.csv` | On-board results (8/8 PASS) |
| `fpga_erven/benchmarks/hls_unroll_exploration.md` | Static unroll analysis |
| `docs/weekly_reports/week8_erven.md` | This report |

---

## Success Criterion

✅ Benchmark can be repeated with one command (`run_benchmark.sh`)  
✅ 8/8 sizes verified bit-exact on hardware  
✅ Timing decomposition committed to CSV  
✅ HLS unroll exploration documented  

---

## Next Week (Week 9)

- Write ASIC scaling note: how the FPGA IP becomes future ASIC IP
- Estimate what changes at ×4 unroll on a larger FPGA or ASIC
- Prepare final report draft and architecture diagram
- Record demo video draft

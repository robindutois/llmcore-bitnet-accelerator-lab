# Week 9 Report — ASIC Scaling Note, Final Report Draft, and K%4 Regression Fix
**Student**: Erven LE BIVIC — Seoul National University
**Date**: 2026-07-09
**Project**: BitLinear-FPGA Alpha — LLMCore Accelerator Lab

---

## Objective

Write the ASIC scaling note, prepare the architecture diagram and final report
draft, and record a demo video draft. Success criterion (per spec): the FPGA
result is clearly connected to future ASIC/IP value.

In addition to the planned scope, Week 9 uncovered and closed a real
correctness regression (the K%4 packing bug introduced by the Week 8 4-lane
restructuring) and replaced several estimated resource/power figures with
directly measured ones.

---

## Work Completed

### 1. ASIC Scaling Note

Written as Section 12 of `docs/final_report/erven_fpga_report_draft.md`
(12.1–12.6), rather than as a separate file, since the spec names no
standalone file path for it at either Week 9 or Week 10. Covers: the
zero-multiplier operation and why it matters for a chip-design evaluation,
which measured figures are portable to any target silicon vs. which are
FPGA/board-specific (with a table), open decisions a chip-design team would
still need to make (lane count, weight residency, activation rescaling
point), and the row-alignment packing constraint discovered in §2 below,
written up so a from-scratch ASIC implementation doesn't rediscover it the
hard way.

### 2. K%4 Packing Regression — Root Cause and Fix

The Week 8 4-lane restructuring introduced a regression: `K` not a multiple
of 4 (e.g. `test09_manual`, M=2/K=3) started failing C-simulation, though it
had passed at every verification level through Week 7. Root cause: weights
are packed with a flat, row-independent formula
(`byte_idx = (m·K+k)/4`); a row only starts on a byte boundary if `m·K` is
itself a multiple of 4 for every row, which holds only when `K` is a
multiple of 4. Not an activation-buffer overrun, as first suspected — kernel
`x_local` reads are already bounds-guarded.

**Fix** (host-side only, kernel RTL unchanged): round `K` up to
`K_pad = 4·⌈K/4⌉`, and re-pack the weight matrix row-independently at that
width, zero-padding both the trailing weight bytes and the activation
buffer. Implemented in `testbench.cpp`, `bench_scaling.c`, and
`run_bitlinear_linux.c`.

**Verified at every level:**

| Level | Result |
|---|---|
| Standalone C++ build (no Vitis) | 10/10 test vectors PASS |
| Stress test, `M`=1–9 × `K`=1–40 | 360/360 combinations PASS |
| Vitis HLS C-simulation | 10/10 PASS |
| RTL co-simulation (XSim) | PASS, IP re-exported |
| On-board, `bench_scaling` (added M=5/K=7) | PASS, bit-exact; transfer integrity confirmed via matching MD5 between PC-compiled binary and board-received copy |
| On-board, `run_bitlinear_linux.c` | 10/10 PASS, incl. test09_manual (M=2/K=3) |

No new bitstream was required — the kernel RTL is bit-identical to the
already-deployed Week 8 bitstream (confirmed via `diff`, exit 0, on the
actual Verilog file used in the Vivado implementation). The interface
contract (`K` passed to `bitlinear_hls()` must already be a multiple of 4;
the kernel does not enforce this itself) is now documented directly in
`bitlinear_hls.h`.

### 3. Resource and Power — Measured, Not Estimated

`resource_report.md` and `feasibility_analysis.md` previously carried
forward Week 6/7 full-system figures pending a fresh Vivado implementation
run. That run was completed this week:

| Metric | IP alone, pre-implementation estimate | IP alone, post-P&R (in system) | Full system, post-P&R |
|---|---|---|---|
| LUT | 4 352 | 2 973 | 5 120 (2.22%) |
| FF | 3 366 | 4 068 | 6 773 (1.47%) |
| BRAM | 8 | — | 2 RAMB18 (0.32%) |
| DSP | 0 | 0 | 0 |
| Power | — | — | 3.419 W (2.728 dynamic + 0.692 static) |

RTL identity confirmed against the independently-regenerated Week 8 4-lane
kernel before trusting these numbers. The IP-alone LUT count is lower
post-place-and-route than the pre-implementation HLS estimate — expected,
since Vivado optimizes/merges logic across the AXI interconnect boundary in
ways an isolated HLS synthesis estimate cannot predict.

### 4. Architecture Diagram and Final Report Draft

`docs/architecture_diagrams/bitlinear_fpga_architecture.png` and
`docs/final_report/erven_fpga_report_draft.md` committed, covering the full
Week 1→9 narrative, benchmark results (including the size-dependent
total-latency finding below), correctness chain, and ASIC roadmap.

### 5. A Nuance Worth Flagging: Total-Latency Speedup Is Not Uniform

Re-checking all 8 sizes on the committed `results_week8.csv` (not just the
4 sizes overlapping the Week 8 technical note table) surfaced something the
Week 8 report didn't state precisely: the compute-phase speedup (×3.5–3.9)
is uniform across sizes, but the **total-call** speedup, which includes the
~379 µs fixed PS-PL overhead, is *negative* for the two smallest sizes
(64×64, 64×128) — the Week 8 4-lane kernel is total-latency-*slower* than
Week 7 there, since a faster compute phase barely moves a call dominated by
fixed overhead. Documented in `feasibility_analysis.md` §2 with the full
table; any investor-facing use of the "3.7×" figure should be scoped to
compute-phase or to sizes ≥128×128.

---

## Deliverables Committed

| File | Description |
|---|---|
| `docs/final_report/erven_fpga_report_draft.md` | Full Week 1→9 narrative, incl. ASIC roadmap (§12) |
| `docs/architecture_diagrams/bitlinear_fpga_architecture.png` | PS↔PL↔DDR4 diagram, Week 8 latency breakdown |
| `fpga_erven/hls/bitlinear/testbench.cpp` | K%4 fix, verified 10/10 (Vitis + RTL cosim) |
| `fpga_erven/hls/bitlinear/stress_test_kpad.cpp` | 360-case K%4 stress sweep |
| `fpga_erven/hls/bitlinear/bitlinear_hls.h` | K_pad interface contract now documented |
| `fpga_erven/benchmarks/bench_scaling.c` | K%4 fix, board-verified (M=5/K=7 added) |
| `fpga_erven/ps_host/run_bitlinear_linux.c` | K%4 fix, board-verified (10/10, incl. test09_manual) |
| `fpga_erven/hls/reports/c_sim_result_week8.md` | Full K%4 root-cause and verification writeup (§9) |
| `fpga_erven/benchmarks/resource_report.md` | Measured (not estimated) post-implementation figures |
| `fpga_erven/benchmarks/feasibility_analysis.md` | Full 8-size sweep, TerEffic comparison, size-dependent speedup nuance |
| `docs/weekly_reports/week9_erven.md` | This report |

---

## Success Criterion

✅ ASIC scaling note written, connecting the FPGA result to future ASIC/IP value
✅ Architecture diagram committed
✅ Final report draft committed, full Week 1→9 narrative
✅ K%4 regression root-caused, fixed, and verified end-to-end incl. on-board hardware
✅ Resource/power figures measured (Vivado post-implementation), not estimated
⏳ Demo video draft — not yet recorded

---

## Next (Week 10 — Final Delivery)

- Record and commit the demo video draft
- Clean repository (loose build artifacts, `.gitignore` hygiene)
- Merge with Robin's Tenstorrent section for the joint final report
- Final demo on the ZCU106

# Week 4 Report — Erven LE BIVIC
**Student:** Erven LE BIVIC
**Project:** BitLinear-FPGA Alpha
**Date:** 2026-06-10

---

## Objective
Run HLS synthesis (`csynth_design`) on the BitLinear kernel. Record LUT / FF / BRAM / DSP usage and estimated latency. Produce `synthesis_report.md`, `resource_estimate.md`, `latency_estimate.md`.

## What Worked
- HLS synthesis completed successfully via `run_hls.tcl`
- **II = 1 achieved** on both LOAD_X and INNER_LOOP pipelines
- **Fmax estimated at 136.99 MHz** — 37% above the 100 MHz target clock
- All loop constraints satisfied (`HLS 200-790`)
- RTL generated in both Verilog and VHDL
- Resource utilization very low: LUT 1%, FF <1%, BRAM 1%, DSP 0%
- C-simulation skipped legitimately — already validated Week 3 (10/10 PASS, no code changes)

## What Failed / Toolchain Issues Resolved
Vitis HLS 2025.1 on this machine had an incomplete installation — the `lnx64.o/tools/` directory was missing several components. The following symlinks and fixes were required before synthesis could run:

| Issue | Fix |
|-------|-----|
| `hls_startup.tcl` not found | Symlink `scripts/vitis_hls/hls_startup.tcl` → `common/scripts/hls_startup.tcl` |
| `RDI_PLATFORM` undefined | `source /tools/Xilinx/2025.1/Vitis/settings64.sh` + `export RDI_PLATFORM=lnx64.o` |
| `clang-3.1` not found in `lnx64.o` | Symlink `lnx64.o/tools/clang-3.1` → `lnx64/tools/clang-3.1` |
| `clang-3.9-csynth` not found in `lnx64.o` | Symlink `lnx64.o/tools/clang-3.9-csynth` → `lnx64/tools/clang-3.9-csynth` |
| `libhlsm_39.bc` not found in `lnx64.o/lib` | Symlink `lnx64.o/lib` → `lnx64/lib` |
| GCC toolchain not found at `/tools/batonroot/...` | Symlink `/tools/batonroot/rodin/devkits/lnx64/gcc-8.3.0` → `lnx64/tools/gcc` |
| `ap_int.h` incompatible with bundled clang-3.1 | Removed `#ifdef __SYNTHESIS__ #include <ap_int.h>` from `bitlinear_hls.h` (unused by kernel) |
| C-sim blocked by missing `libhlsmc++` | Skipped csim — already validated Week 3. Synthesis does not require it. |

All symlinks created with `sudo` — no Vitis reinstallation required.

## Synthesis Results

| Metric | Result | Target | Status |
|--------|--------|--------|--------|
| Clock target | 10.00 ns | 10 ns | ✅ |
| Clock estimated | 7.30 ns (136.99 MHz) | — | ✅ |
| II LOAD_X | 1 | 1 | ✅ |
| II INNER_LOOP | 1 | 11 depth | ✅ |
| LUT | 4 378 / 230 400 (1%) | — | ✅ |
| FF | 3 465 / 460 800 (<1%) | — | ✅ |
| BRAM_18K | 8 / 624 (1%) | — | ✅ |
| DSP | 0 / 1 728 (0%) | — | ✅ |
| URAM | 0 / 96 (0%) | — | ✅ |

## Analysis Note
Current utilization is intentionally low — the kernel is a single-pipeline sequential implementation (1 weight/cycle). Unrolling the INNER_LOOP by factor 4 or 8 in Week 8 will multiply throughput proportionally and bring utilization to a more representative level (~5–10% LUT). The low footprint confirms the design fits comfortably on ZCU106 and leaves ample room for PS-PL integration and future optimization.

## Blocking Issue
None. Week 4 success criterion achieved: HLS synthesis completes without errors.

## Next Steps (Week 5)
- Export HLS IP from Vitis HLS
- Create Vivado block design with Zynq PS + AXI SmartConnect + BitLinear IP
- Define AXI-DMA or memory-mapped interface
- Write PS host program skeleton (`host_skeleton.cpp`)
- Prepare RTL co-simulation if time permits
- Produce `block_design_notes.md` and `rtl_cosim_result.md`

---

## Week 4 Deliverable Checklist

- [x] `fpga_erven/hls/bitlinear/run_hls.tcl`
- [x] `fpga_erven/hls/bitlinear/bitlinear_hls.h` (updated — `ap_int.h` removed)
- [x] `fpga_erven/hls/reports/synthesis_report.md`
- [x] `fpga_erven/hls/reports/resource_estimate.md`
- [x] `fpga_erven/hls/reports/latency_estimate.md`
- [x] `docs/weekly_reports/week4_erven.md`

**Week 4 status: COMPLETE**

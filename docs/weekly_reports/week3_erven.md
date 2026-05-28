# Week 3 — Daily Report
**Student:** Erven LE BIVIC
**Project:** BitLinear-FPGA Alpha
**Date:** 2026-05-28

---

## Today's target
Implement 2-bit ternary packing, write HLS BitLinear kernel v1, run C-simulation in Vitis HLS.

## What worked
- 2-bit ternary packing implemented (`pack_ternary_2bit.cpp`, `unpack_ternary_2bit.cpp`)
- Packing standalone tests: 10/10 PASS including 1000 random matrices
- HLS kernel `bitlinear_hls.cpp` written with PIPELINE and UNROLL pragmas
- Standalone g++ testbench: 10/10 PASS (HLS kernel matches C++ reference)
- Vitis HLS 2025.1 C-Simulation: 10/10 PASS
- Cross-validation against Week 2 binary test vectors: 10/10 PASS

## What failed
- First Vitis project failed: source folder path had spaces (`files(1) (2)`) — fixed by copying sources to `~/erven_workspace/hls_src/`
- `ap_int.h` not found in standalone g++ — fixed with `#ifdef __SYNTHESIS__` guard
- `testbench.cpp` used relative path `../packing/pack_ternary_2bit.h` — fixed to flat include
- **Packing layout bug found:** kernel used row-by-row packing, Week 2 uses flat packing. Only revealed by test09_manual (K=3, not multiple of 4). Fixed by switching kernel to flat index: `flat_idx = m*K + k`.

## Benchmark result
- Packing round-trip: 1000/1000 random matrices PASS
- C-simulation: 10/10 PASS (HLS output == C++ reference, exact int32 match)
- Week 2 cross-validation: 10/10 PASS (HLS output == Python reference binary files)

## Blocking issue
None. Week 3 targets fully achieved.

## Tomorrow's target
Run HLS Synthesis in Vitis HLS. Record LUT / FF / BRAM / DSP usage and estimated latency. Fill `synthesis_report.md`.

---

## Week 3 Deliverable Checklist

- [x] `fpga_erven/hls/packing/pack_ternary_2bit.h`
- [x] `fpga_erven/hls/packing/pack_ternary_2bit.cpp`
- [x] `fpga_erven/hls/packing/unpack_ternary_2bit.cpp`
- [x] `fpga_erven/hls/packing/tests/test_packing.cpp`
- [x] `fpga_erven/hls/bitlinear/bitlinear_hls.h`
- [x] `fpga_erven/hls/bitlinear/bitlinear_hls.cpp`
- [x] `fpga_erven/hls/bitlinear/testbench.cpp`
- [x] `fpga_erven/hls/bitlinear/testbench_week2_vectors.cpp`
- [x] `fpga_erven/hls/bitlinear/run_hls.tcl`
- [x] `fpga_erven/hls/reports/c_sim_result.md`
- [x] `fpga_erven/hls/reports/correctness_summary.md`

**Week 3 status: COMPLETE**

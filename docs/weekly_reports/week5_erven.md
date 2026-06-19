# Week 5 Report — Erven LE BIVIC
**Student:** Erven LE BIVIC
**Project:** BitLinear-FPGA Alpha
**Date:** 2026-06-10

---

## Objective
Run RTL co-simulation to validate that the generated Verilog matches the C++ reference.
Export the HLS IP as a Vivado IP catalog archive for block design integration.

## What Worked

- **RTL co-simulation: 10 / 10 PASS** — XSim confirmed the Verilog RTL produces
  bit-exact outputs matching the C++ reference across all 10 test cases,
  including K not a multiple of 4, boundary int8 values, and M=128 K=256.
- **IP exported**: `llmcore_hls_bitlinear_hls_1_0.zip` ready for Vivado import.
- Synthesis results confirmed identical to Week 4: II=1, Fmax 136.99 MHz.
- `vitis-run --mode hls` identified as the correct launcher (replaces deprecated `vitis_hls`).

## What Failed / Issues Resolved

| Issue | Fix |
|-------|-----|
| `vitis_hls` command not found in PATH | Used `/tools/Xilinx/2025.1/Vitis/bin/vitis-run --mode hls --tcl` |
| `features.tcl` load error with unwrapped binary | Switched to `vitis-run` wrapper — resolves all Tcl path issues |
| `pack_flat` linker error in co-sim | Added `pack_ternary_2bit.cpp` and `unpack_ternary_2bit.cpp` as `-tb` files |
| SIGSEGV in co-sim instrumented environment | VLAs in original `testbench.cpp` replaced by static global buffers — unified back into `testbench.cpp` |
| Duplicate `main` symbol | `open_project -reset` + `open_solution -reset` to clear cached `testbench.cpp` |

## Correctness

**RTL Co-Simulation: 10 / 10 PASS**

The complete verification chain is now:

| Step | Result |
|------|--------|
| Python reference | ✅ Week 2 — 11/11 PASS |
| C++ reference | ✅ Week 2 — 21/21 PASS |
| HLS C-Simulation | ✅ Week 3 — 10/10 PASS |
| HLS Synthesis | ✅ Week 4 — II=1, Fmax 136.99 MHz |
| RTL Co-Simulation | ✅ **Week 5 — 10/10 PASS** |
| ZCU106 board execution | ⋯ Week 6 — pending |

## Performance

RTL simulation total time: 831 585 ns (simulated clock cycles at 1 ps resolution).
Largest test (M=128, K=256): completed at 692 085 ns simulation time.

## Blocking Issue

None. Week 5 success criterion achieved: HLS IP exported and validated by RTL co-simulation.

## Next Steps (Week 6)

1. Import `llmcore_hls_bitlinear_hls_1_0.zip` into Vivado as custom IP
2. Build block design: Zynq UltraScale+ PS + AXI SmartConnect + BitLinear IP
3. Connect AXI HP ports (MEM_IN, MEM_W, MEM_OUT) and GP port (CTRL)
4. Generate bitstream and program ZCU106
5. Write PS host program `run_bitlinear.cpp`
6. Run BitLinear on board, compare output with CPU reference, measure latency

---

## Week 5 Deliverable Checklist

- [x] `fpga_erven/hls/bitlinear/testbench.cpp` (unified C-sim + co-sim testbench)
- [x] `fpga_erven/hls/bitlinear/run_hls.tcl` (canonical script: csim + csynth + cosim + export)
- [x] `fpga_erven/hls/reports/rtl_cosim_result.md`
- [x] `docs/weekly_reports/week5_erven.md`
- [x] IP archive: `bitlinear_hls_project/solution1/impl/ip/llmcore_hls_bitlinear_hls_1_0.zip`

**Week 5 status: COMPLETE ✅**

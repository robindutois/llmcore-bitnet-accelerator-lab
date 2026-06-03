# RTL Co-Simulation Result — Week 5
## BitLinear-FPGA Alpha — `bitlinear_hls` kernel

**Tool:** Vitis HLS 2025.1 / Vivado XSim 2025.1
**Date:** 2026-06-01
**Target device:** xczu7ev-ffvc1156-2-e (ZCU106)

---

## Co-Simulation Runs

Two co-simulation runs were performed, together closing the full validation chain.

### Run 1 — `testbench_cosim.cpp` (randomised data)

Validates that the RTL Verilog produces bit-exact results vs. the inline C++ reference
across a broad set of matrix shapes and weight distributions.

| Metric | Value |
|--------|-------|
| Tests | 10 / 10 PASS |
| RTL simulation time | 831 585 ns |
| Testbench | `testbench_cosim.cpp` (static buffers, `rand()` data) |

---

### Run 2 — `testbench_cosim_vectors.cpp` (Week 2 binary test vectors)

Closes the full chain: **Python reference → .bin files → RTL Verilog**.

The testbench loads the real binary test vectors generated in Week 2 by
`bitlinear_reference.py` and performs three checks per test vector:

| Check | Comparison | Meaning |
|-------|-----------|---------|
| [A] | HLS RTL output vs `expected_output_int32.bin` | RTL matches Python ground truth |
| [B] | C++ ref output vs `expected_output_int32.bin` | C++ ref matches Python ground truth |
| [C] | HLS RTL output vs C++ ref output | RTL and C++ ref agree |

#### Results

| Test | M | K | [A] HLS vs Python | [B] C++ vs Python | [C] HLS vs C++ |
|------|---|---|-------------------|-------------------|----------------|
| test01_random   | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test02_W_zero   | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test03_W_plus1  | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test04_W_minus1 | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test05_sparse   | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test06_dense    | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test07_xmax     | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test08_xmin     | 32  | 64  | ✅ PASS | ✅ PASS | ✅ PASS |
| test09_manual   | 2   | 3   | ✅ PASS | ✅ PASS | ✅ PASS |
| test10_large    | 256 | 512 | ✅ PASS | ✅ PASS | ✅ PASS |

**STATUS: ✅ PASS — 10 / 10 — All three checks pass on all test vectors.**

---

## Complete Validation Chain

```
Python reference — bitlinear_reference.py  (Week 2 — 11/11 PASS)
        ↕  bit-exact match
.bin test vectors — reference/test_vectors/ (Week 2 — 10 test sets)
        ↕  bit-exact match  [Check B above]
C++ reference — bitlinear_reference.cpp     (Week 2 — 21/21 PASS)
        ↕  bit-exact match
HLS C-Simulation — bitlinear_hls.cpp        (Week 3 — 10/10 PASS)
        ↕  bit-exact match
HLS Synthesis — csynth_design               (Week 4 — II=1, Fmax 136.99 MHz)
        ↕  bit-exact match  [Check A above]
RTL Co-Simulation — XSim Verilog            (Week 5 — 10/10 PASS)
        ↕  pending
ZCU106 board execution                      (Week 6)
```

**The chain from Python reference to synthesised RTL is fully closed and verified.**

---

## Technical Notes

### Testbench design for co-sim compatibility
- No VLAs — static global buffers sized to `TV_MAX_M=256`, `TV_MAX_K=512`
- No `std::vector` / `std::ifstream` — C-style `fopen`/`fread` only
- `TV_BASE_PATH` injected at compile time via `-mflags` in `run_hls_week5_vectors.tcl`
  using `file normalize [file join [pwd] "../../../reference/test_vectors"]`
- Fallback default path covers the case where the macro is not set

### Packing format consistency verified
- Python: `pack_ternary_2bit(W.flatten())` → LSB-first, `code << (j*2)`
- C++: `pack_flat()` → same bit layout
- HLS kernel: `flat_idx = m*K+k`, `bit_pair*2` shift → identical decoding
- K non-multiple of 4: padding bytes are never read by the kernel (flat_max = M*K-1)

---

## Files

| File | Role |
|------|------|
| `testbench_cosim.cpp` | Co-sim testbench with randomised data (Run 1) |
| `testbench_cosim_vectors.cpp` | Co-sim testbench loading Week 2 .bin files (Run 2) |
| `run_hls_week5.tcl` | TCL for Run 1 |
| `run_hls_week5_vectors.tcl` | TCL for Run 2 (injects `TV_BASE_PATH`) |

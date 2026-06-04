# RTL Co-Simulation Result — Week 5
## BitLinear-FPGA Alpha — `bitlinear_hls` kernel

**Tool:** Vitis HLS 2025.1 / Vivado XSim 2025.1  
**Date:** 2026-06-01  
**Target device:** xczu7ev-ffvc1156-2-e (ZCU106)

---

## Co-Simulation Result

RTL co-simulation was run via `run_hls.tcl` using `testbench.cpp`.

The testbench loads the real binary test vectors generated in Week 2 by
`bitlinear_reference.py` and performs three checks per test vector:

| Check | Comparison | Meaning |
|-------|-----------|---------|
| [A] | HLS RTL output vs `expected_output_int32.bin` | RTL matches Python ground truth |
| [B] | C++ ref output vs `expected_output_int32.bin` | C++ ref matches Python ground truth |
| [C] | HLS RTL output vs C++ ref output | RTL and C++ ref agree |

### Results

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

RTL simulation total time: 2 384 325 ns (XSim, 1 ps resolution).

---

## Complete Validation Chain

```
Python reference — bitlinear_reference.py  (Week 2 — 11/11 PASS)
        ↕  bit-exact match
.bin test vectors — reference/test_vectors/ (Week 2 — 10 test sets × 5 files)
        ↕  bit-exact match  [Check B]
C++ reference — bitlinear_reference.cpp     (Week 2 — 21/21 PASS)
        ↕  bit-exact match
HLS C-Simulation — bitlinear_hls.cpp        (Week 3 — 10/10 PASS)
        ↕  bit-exact match
HLS Synthesis — csynth_design               (Week 4 — II=1, Fmax 136.99 MHz)
        ↕  bit-exact match  [Check A]
RTL Co-Simulation — XSim Verilog            (Week 5 — 10/10 PASS)
        ↕  pending
ZCU106 board execution                      (Week 6)
```

**The chain from Python reference to synthesised RTL is fully closed and verified.**

---

## Technical Notes

### Testbench design for co-sim compatibility (`testbench.cpp`)
- No VLAs — static global buffers sized to HLS pragma depth values:
  - `s_x[1024]` matching `depth=1024` on `port=x`
  - `s_W_packed[131072]` matching `depth=131072` on `port=W_packed`
  - `s_y_hls[512]` matching `depth=512` on `port=y`
- C-style `fopen`/`fread` only (no `std::vector` / `std::ifstream`)
- `TV_BASE_PATH` written to `tv_path_generated.h` by `run_hls.tcl` at runtime
  using `file normalize [file join [pwd] "../../../reference/test_vectors"]`

### Packing format consistency verified
- Python: `pack_ternary_2bit(W.flatten())` → LSB-first, `code << (j*2)`
- C++: `pack_flat()` → identical bit layout
- HLS kernel: `flat_idx = m*K+k`, `(W_packed[byte_idx] >> (bit_pair*2)) & 0b11`

### Run command
```bash
cd fpga_erven/hls/bitlinear/
/tools/Xilinx/2025.1/Vitis/bin/vitis-run --mode hls --tcl run_hls.tcl
```

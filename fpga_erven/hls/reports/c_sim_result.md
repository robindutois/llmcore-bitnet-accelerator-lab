# C-Simulation Result Report
**Date:** 2026-05-28
**Student:** Erven LE BIVIC
**Project:** BitLinear-FPGA Alpha — Week 3

---

## 1. Environment

| Item | Value |
|---|---|
| Vitis HLS version | 2025.1 (Build 6137779, 2025-05-21) |
| FPGA part | xczu7ev-ffvc1156-2-e (ZCU106) |
| Target clock | 10 ns (100 MHz) |
| Host OS | Ubuntu (dllab) |
| Compiler (standalone test) | g++ (C++14) |
| Compiler (Vitis C-sim) | Clang (Vitis HLS internal) |

---

## 2. Files

| File | Role |
|---|---|
| `bitlinear_hls.cpp` | HLS kernel — top-level synthesizable function |
| `bitlinear_hls.h` | Kernel header (ap_int.h guarded for standalone build) |
| `pack_ternary_2bit.cpp` | 2-bit ternary weight packing |
| `unpack_ternary_2bit.cpp` | 2-bit ternary weight unpacking |
| `pack_ternary_2bit.h` | Packing header |
| `testbench.cpp` | C-simulation testbench (10 test cases) |
| `tests/test_packing.cpp` | Standalone packing correctness test |

---

## 3. C-Simulation Command

Inside Vitis HLS GUI:
```
Flow Navigator → Run C Simulation → csim.clean ✓ → OK
```

Equivalent CLI:
```
vitis-run --mode hls --csim --config hls_config.cfg --work_dir bitlinear_hls_project
```

Standalone g++ test (no Vitis required):
```bash
# Packing test
g++ -o test_packing test_packing.cpp pack_ternary_2bit.cpp unpack_ternary_2bit.cpp
./test_packing

# Kernel + testbench
g++ -o testbench testbench.cpp bitlinear_hls.cpp pack_ternary_2bit.cpp unpack_ternary_2bit.cpp -I. -std=c++14
./testbench
```

---

## 4. Packing Test Results (standalone g++)

```
=== 2-bit Ternary Packing Tests ===

[Test 1] Single weight encode/decode: PASS
[Test 2] All zeros (K=8): PASS
[Test 2] All zeros (K=128): PASS
[Test 3] All +1   (K=16): PASS
[Test 4] All -1   (K=16): PASS
[Test 5] K not multiple of 4 (K=7): PASS
[Test 6] Matrix pack/unpack (M=4, K=8): PASS
[Test 6] Matrix pack/unpack (M=64, K=128): PASS
[Test 6] Matrix pack/unpack (M=256, K=512): PASS
[Test 7] 1000 random matrices: 1000 PASS, 0 FAIL -> PASS

=== Result: 10 / 10 tests PASSED ===
```

---

## 5. C-Simulation Test Results

### Standalone g++ (bitlinear_hls.cpp vs reference)

```
=== HLS BitLinear C-Simulation Testbench ===

[all W=0                            ] M=8    K=16   -> PASS
[all W=+1                           ] M=8    K=16   -> PASS
[all W=-1                           ] M=8    K=16   -> PASS
[x=max(127)                         ] M=8    K=16   -> PASS
[x=min(-128)                        ] M=8    K=16   -> PASS
[K not multiple of 4 (K=7)          ] M=4    K=7    -> PASS
[small manual (M=2 K=4)             ] M=2    K=4    -> PASS
[random M=64 K=128                  ] M=64   K=128  -> PASS
[random M=128 K=256                 ] M=128  K=256  -> PASS
[sparse W (80% zero)                ] M=64   K=128  -> PASS

=== C-Simulation Result: 10 / 10 PASSED ===
STATUS: PASS — HLS kernel matches C++ reference on all tests.
```

### Vitis HLS C-Simulation (Clang compiler)

| Test | M | K | Result |
|---|---|---|---|
| all W=0 | 8 | 16 | ✅ PASS |
| all W=+1 | 8 | 16 | ✅ PASS |
| all W=-1 | 8 | 16 | ✅ PASS |
| x=max(127) | 8 | 16 | ✅ PASS |
| x=min(-128) | 8 | 16 | ✅ PASS |
| K not multiple of 4 | 4 | 7 | ✅ PASS |
| small manual (M=2 K=4) | 2 | 4 | ✅ PASS |
| random M=64 K=128 | 64 | 128 | ✅ PASS |
| random M=128 K=256 | 128 | 256 | ✅ PASS |
| sparse W (80% zero) | 64 | 128 | ✅ PASS |
| **TOTAL** | | | **10 / 10** |

---

## 6. Correctness Verification

The HLS kernel output is verified against the C++ golden reference for every test case.

Verification method: byte-exact integer comparison (`int32_t` output).
No floating-point is involved — results must be exactly equal, not approximately equal.

**Result: HLS kernel == C++ reference on all 10 test cases.**

---

## 7. Known Issues and Workarounds

| Issue | Workaround |
|---|---|
| `ap_int.h` not found in standalone g++ build | Guarded with `#ifdef __SYNTHESIS__` in `bitlinear_hls.h` |
| Vitis path error with spaces in folder name `files(1) (2)` | Copied all source files to `~/erven_workspace/hls_src/` (no spaces) |
| `testbench.cpp` used relative path `../packing/pack_ternary_2bit.h` | Changed to flat include `pack_ternary_2bit.h` for Vitis compatibility |

---

## 8. Week 3 Status

- [x] 2-bit ternary packing implemented and tested (10/10 PASS, 1000 random matrices)
- [x] HLS BitLinear kernel written and compiles cleanly
- [x] HLS kernel output matches C++ reference exactly (10/10 PASS)
- [x] Vitis HLS C-Simulation passes (10/10 PASS)
- [x] All source files committed to repository

**Overall Week 3 C-Sim Status: ✅ PASS**

---

## 9. Next Step — Week 4

Run HLS Synthesis:
- Add HLS pragmas (PIPELINE, UNROLL already in place)
- Run `csynth_design` in Vitis HLS
- Record LUT / FF / BRAM / DSP usage
- Record estimated latency in cycles
- Fill `synthesis_report.md` and `resource_estimate.md`

# BitLinear-FPGA Alpha — Week 2 Correctness Report

**Project:** BitLinear-FPGA Alpha  
**Student:** Erven LE BIVIC  
**Week:** 2  
**Date:** 2026-05-27  
**Status:** COMPLETE — ALL PASS ✓

---

## 1. Objective

Implement and validate the CPU golden reference for the BitLinear operation:

```
y[m] = Σ_k  W[m,k] × x[k]

  x[k]    : int8  activation vector
  W[m,k]  : ternary weight ∈ {-1, 0, +1}
  y[m]    : int32 accumulator output
```

All future HLS and FPGA results must match this reference exactly.

---

## 2. Deliverables Produced

| File | Description |
|------|-------------|
| `reference/bitlinear_reference.py` | Python golden reference + packing + test vectors generator |
| `fpga_erven/hls/reference/bitlinear_reference.hpp` | C++ header |
| `fpga_erven/hls/reference/bitlinear_reference.cpp` | C++ implementation |
| `fpga_erven/hls/reference/bitlinear_reference_test.cpp` | C++ testbench (10 tests + cross-check) |
| `reference/test_vectors/` | 10 binary test vector sets |

---

## 3. Build Instructions

```bash
# Build
g++ -O2 -std=c++17 -o bitlinear_test \
    fpga_erven/hls/reference/bitlinear_reference.cpp \
    fpga_erven/hls/reference/bitlinear_reference_test.cpp

# Run unit tests only
./bitlinear_test

# Run unit tests + cross-check against Python .bin files
./bitlinear_test reference/test_vectors/
```

**Build environment:**
- Compiler: g++ (Ubuntu 13.3.0) — C++17
- Architecture: x86_64
- Python: NumPy-based reference

---

## 4. Unit Test Results (C++)

```
============================================================
  BitLinear C++ Reference — Test Suite
============================================================
  ✓ [PASS]  Test 1 — random x, random W
  ✓ [PASS]  Test 2 — all W = 0  →  y = 0
  ✓ [PASS]  Test 3 — all W = +1  →  y[m] = sum(x)
  ✓ [PASS]  Test 4 — all W = -1  →  y[m] = -sum(x)
  ✓ [PASS]  Test 5 — sparse W (~10% nonzero)
  ✓ [PASS]  Test 6 — dense W (~90% nonzero)
  ✓ [PASS]  Test 7 — x = max int8 (+127)
  ✓ [PASS]  Test 8 — x = min int8 (-128)
  ✓ [PASS]  Test 9 — small matrix manually verified
  ✓ [PASS]  Test 10 — large matrix stress (256×512)
  ✓ [PASS]  Pack round-trip (1000 random matrices)

--- Cross-check against Python test vectors ---
  ✓ [PASS]  Python cross-check: test01_random (M=32 K=64)
  ✓ [PASS]  Python cross-check: test02_W_zero (M=32 K=64)
  ✓ [PASS]  Python cross-check: test03_W_plus1 (M=32 K=64)
  ✓ [PASS]  Python cross-check: test04_W_minus1 (M=32 K=64)
  ✓ [PASS]  Python cross-check: test05_sparse (M=32 K=64)
  ✓ [PASS]  Python cross-check: test06_dense (M=32 K=64)
  ✓ [PASS]  Python cross-check: test07_xmax (M=32 K=64)
  ✓ [PASS]  Python cross-check: test08_xmin (M=32 K=64)
  ✓ [PASS]  Python cross-check: test09_manual (M=2 K=3)
  ✓ [PASS]  Python cross-check: test10_large (M=256 K=512)

============================================================
  Results: 21 PASS, 0 FAIL
  Overall: ALL PASS ✓
============================================================
```

---

## 5. Test Case Descriptions

| # | Test | Matrix Size | Purpose |
|---|------|-------------|---------|
| 1 | Random x, random W | M=32, K=64 | General correctness |
| 2 | All W = 0 | M=32, K=64 | Zero weights → zero output |
| 3 | All W = +1 | M=32, K=64 | y[m] = sum(x) for all m |
| 4 | All W = -1 | M=32, K=64 | y[m] = −sum(x) for all m |
| 5 | Sparse W (~10% nonzero) | M=32, K=64 | Skip behaviour |
| 6 | Dense W (~90% nonzero) | M=32, K=64 | Near-full accumulation |
| 7 | x = +127 (max int8) | M=32, K=64 | Positive overflow boundary |
| 8 | x = −128 (min int8) | M=32, K=64 | Negative overflow boundary |
| 9 | Small matrix (manual) | M=2, K=3 | Hand-verified ground truth |
| 10 | Large matrix stress | M=256, K=512 | Scale validation |
| — | Pack round-trip | 8×8 × 1000 | 2-bit encode/decode integrity |

### Test 9 — Manual Ground Truth

```
x = [1, -2, 3]

W = [[ 1,  0, -1],    →  y[0] = 1×1 + 0×(−2) + (−1)×3 = −2
     [ 0,  1,  1]]    →  y[1] = 0×1 + 1×(−2) +   1 ×3 =  1

Expected: y = [−2, 1]
Result:   y = [−2, 1]  ✓
```

---

## 6. 2-bit Ternary Packing

### Encoding Table

| 2-bit code | Ternary value |
|-----------|---------------|
| `00` | 0 |
| `01` | +1 |
| `10` | −1 |
| `11` | reserved (treated as 0) |

4 weights packed per byte, LSB-first: `byte = [w3 | w2 | w1 | w0]`

### Round-Trip Validation

```
Original W  →  pack_ternary_2bit()  →  unpack_ternary_2bit()  →  Recovered W
Recovered W == Original W  for 1000 random 8×8 matrices  ✓
```

---

## 7. Test Vector Inventory

Binary files stored in `reference/test_vectors/<label>/`:

| Label | M | K | activation_int8.bin | weight_unpacked.bin | weight_packed.bin | output_int32.bin |
|-------|---|---|-------|-------|-------|-------|
| test01_random | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test02_W_zero | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test03_W_plus1 | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test04_W_minus1 | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test05_sparse | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test06_dense | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test07_xmax | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test08_xmin | 32 | 64 | 64 B | 2048 B | 512 B | 128 B |
| test09_manual | 2 | 3 | 3 B | 6 B | 2 B | 8 B |
| test10_large | 256 | 512 | 512 B | 131072 B | 32768 B | 1024 B |

Each directory also contains `metadata.json` with M, K, dtype, and encoding fields.

---

## 8. Python Reference Results

```
Running BitLinear reference tests and generating test vectors...

  ✓ [PASS]  Test 1 — random x, random W
  ✓ [PASS]  Test 2 — all W = 0
  ✓ [PASS]  Test 3 — all W = +1
  ✓ [PASS]  Test 4 — all W = -1
  ✓ [PASS]  Test 5 — sparse W (~10% nonzero)
  ✓ [PASS]  Test 6 — dense W (~90% nonzero)
  ✓ [PASS]  Test 7 — x = max int8 (+127)
  ✓ [PASS]  Test 8 — x = min int8 (-128)
  ✓ [PASS]  Test 9 — small matrix manual check
  ✓ [PASS]  Test 10 — large matrix stress (256×512)
  ✓ [PASS]  Pack round-trip (1000 random matrices)

  Overall: ALL PASS ✓
```

---

## 9. Correctness Guarantee

The C++ reference output matches the Python reference output **bit-for-bit** on all
10 test vector sets. This cross-check was performed by:

1. Generating `.bin` files from the Python reference (`bitlinear_reference.py`).
2. Loading the same `.bin` files in the C++ testbench.
3. Running `bitlinear_reference()` in C++ on the loaded inputs.
4. Comparing every `int32` output element against the Python-generated expected output.

Result: **0 mismatches across all 10 test sets.**

---

## 10. Week 2 Success Criteria — Checklist

| Criterion | Status |
|-----------|--------|
| C++ BitLinear reference implemented | ✅ |
| Python BitLinear reference implemented | ✅ |
| All 10 required unit tests pass (C++) | ✅ |
| All 10 required unit tests pass (Python) | ✅ |
| Python output == C++ output for all tests | ✅ |
| 2-bit packing round-trip verified (≥1000 matrices) | ✅ |
| Binary test vector files generated | ✅ |
| metadata.json format correct | ✅ |

**Week 2 success criterion met: Python reference == C++ reference for all tests.**

---

## 11. Next Steps — Week 3

1. Implement `bitlinear_hls.cpp` — synthesizable Vitis HLS kernel using 2-bit packed weights.
2. Write HLS testbench (`testbench.cpp`) using Week 2 test vectors as input.
3. Run HLS C simulation (`csim`) and verify output matches this reference.
4. Target: **HLS C simulation output == C++ reference output.**

---

*AI-assisted code generation was used for this implementation.*  
*All code was verified by compiling, running all 10 unit tests, comparing with the*  
*Python reference output byte-for-byte, and reviewing the logic manually.*

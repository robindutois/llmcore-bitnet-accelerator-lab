# Weekly Report — Week 2
**Student:** Erven LE BIVIC  
**Project:** BitLinear-FPGA Alpha — ZCU106 FPGA BitNet Accelerator  
**Date:** 2026-05-27  

---

## Slide 1 — Goal of the Week

Implement and validate the CPU golden reference for the BitLinear operation:

```
y[m] = Σ_k  W[m,k] × x[k]
  x ∈ int8,  W ∈ {-1, 0, +1},  y ∈ int32
```

Deliverables targeted:
- Python reference implementation
- C++ reference implementation
- 2-bit ternary weight packing utilities
- Binary test vectors (10 sets)
- Correctness report

---

## Slide 2 — Working Artifact

### Files produced

```
reference/
  bitlinear_reference.py          (362 lines)
  packing_utils.py                (197 lines)
  test_vectors/
    test01_random/   test06_dense/
    test02_W_zero/   test07_xmax/
    test03_W_plus1/  test08_xmin/
    test04_W_minus1/ test09_manual/
    test05_sparse/   test10_large/

fpga_erven/hls/reference/
  bitlinear_reference.hpp         (  )
  bitlinear_reference.cpp         (123 lines)
  bitlinear_reference_test.cpp    (363 lines)

fpga_erven/hls/reports/
  c_ref_results.md
```

### Commands that work

```bash
# Python reference + test vector generation
python reference/bitlinear_reference.py

# C++ build and test
cd fpga_erven/hls/reference/
g++ -O2 -std=c++17 -o bitlinear_test \
    bitlinear_reference.cpp bitlinear_reference_test.cpp
export LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu
./bitlinear_test ../../../reference/test_vectors/
```

---

## Slide 3 — Correctness

### Python — 11/11 PASS

```
✓ Test 1 — random x, random W
✓ Test 2 — all W = 0
✓ Test 3 — all W = +1
✓ Test 4 — all W = -1
✓ Test 5 — sparse W (~10% nonzero)
✓ Test 6 — dense W (~90% nonzero)
✓ Test 7 — x = max int8 (+127)
✓ Test 8 — x = min int8 (-128)
✓ Test 9 — small matrix manual check
✓ Test 10 — large matrix stress (256×512)
✓ Pack round-trip (1000 random matrices)
Overall: ALL PASS ✓
```

### C++ — 21/21 PASS (including Python cross-check)

```
✓ Test 1 — random x, random W
✓ Test 2 — all W = 0  →  y = 0
✓ Test 3 — all W = +1  →  y[m] = sum(x)
✓ Test 4 — all W = -1  →  y[m] = -sum(x)
✓ Test 5 — sparse W (~10% nonzero)
✓ Test 6 — dense W (~90% nonzero)
✓ Test 7 — x = max int8 (+127)
✓ Test 8 — x = min int8 (-128)
✓ Test 9 — small matrix manually verified
✓ Test 10 — large matrix stress (256×512)
✓ Pack round-trip (1000 random matrices)

✓ Python cross-check: test01_random   (M=32 K=64)
✓ Python cross-check: test02_W_zero   (M=32 K=64)
✓ Python cross-check: test03_W_plus1  (M=32 K=64)
✓ Python cross-check: test04_W_minus1 (M=32 K=64)
✓ Python cross-check: test05_sparse   (M=32 K=64)
✓ Python cross-check: test06_dense    (M=32 K=64)
✓ Python cross-check: test07_xmax     (M=32 K=64)
✓ Python cross-check: test08_xmin     (M=32 K=64)
✓ Python cross-check: test09_manual   (M=2  K=3 )
✓ Python cross-check: test10_large    (M=256 K=512)

Results: 21 PASS, 0 FAIL
Overall: ALL PASS ✓
```

### Manual verification — Test 9

```
x = [1, -2, 3]
W = [[ 1,  0, -1],   →  y[0] = 1×1 + 0×(−2) + (−1)×3 = −2
     [ 0,  1,  1]]   →  y[1] = 0×1 + 1×(−2) +   1 ×3 =  1
Expected: [−2, 1]
Got:      [−2, 1]  ✓
```

**Python output == C++ output: verified byte-for-byte on all 10 test sets.**

---

## Slide 4 — Performance

This week is the reference implementation week — performance is not the target.  
The reference runs on CPU without optimization.

| Matrix size | Python time | C++ time |
|-------------|-------------|----------|
| M=32, K=64 | < 1 ms | < 1 ms |
| M=256, K=512 | ~50 ms (scalar loop) | ~5 ms |

The scalar Python loop is intentionally unoptimized — it is the golden reference,  
not a performance target. The C++ version is also unoptimized by design.

Performance measurement begins in Week 7 (larger matrix benchmark).

---

## Slide 5 — Next Week (Week 3)

**Target:** First synthesizable Vitis HLS BitLinear kernel.

Tasks:
1. Implement `bitlinear_hls.cpp` — HLS kernel using 2-bit packed weights
2. Write HLS testbench (`testbench.cpp`) using Week 2 test vectors
3. Run HLS C simulation (`csim`)
4. Verify: HLS C sim output == C++ reference output
5. Prepare `run_hls.tcl` script

**Week 3 success criterion:**  
HLS C simulation output matches C++ reference on all test vectors.

**Files to produce:**
```
fpga_erven/hls/bitlinear/
  bitlinear_hls.cpp
  bitlinear_hls.h
  testbench.cpp
  run_hls.tcl
fpga_erven/hls/reports/
  c_sim_result.md
```

---

## Week 2 Checklist

| Item | Status |
|------|--------|
| C++ reference implemented | ✅ |
| Python reference implemented | ✅ |
| packing_utils.py standalone module | ✅ |
| 10 unit tests pass (Python) | ✅ |
| 10 unit tests pass (C++) | ✅ |
| Python == C++ cross-check (10/10) | ✅ |
| 2-bit packing round-trip (1000 matrices) | ✅ |
| Binary test vectors generated (10 sets) | ✅ |
| Correctness report written | ✅ |
| GitHub push clean | ✅ |

**Week 2 success criterion met.**

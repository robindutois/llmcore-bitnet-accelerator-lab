# Correctness Summary — Week 5
**Project:** BitLinear-FPGA Alpha
**Student:** Erven LE BIVIC
**Last updated:** Week 5 (2026-06-01)

---

## Correctness Principle

All hardware outputs must match the C++ golden reference exactly.
No approximation is acceptable — the comparison is integer exact (`int32_t`).

---

## Verification Chain — Current State

```
Python reference (bitlinear_reference.py)       ✅  Week 2 — 11/11 PASS
        ↕  bit-exact match
C++ reference (bitlinear_reference.cpp)         ✅  Week 2 — 21/21 PASS
        ↕  bit-exact match
HLS C-Simulation (bitlinear_hls.cpp)            ✅  Week 3 — 10/10 PASS
        ↕  bit-exact match
HLS Synthesis (csynth_design)                   ✅  Week 4 — II=1, Fmax 136.99 MHz
        ↕  bit-exact match
RTL Co-Simulation (XSim Verilog)                ✅  Week 5 — 10/10 PASS
        ↕  pending
ZCU106 board execution                          ⏳  Week 6 — in progress
```

---

## Layer-by-Layer Results

### Layer 1 — Python reference (Week 2)

| Test | Result |
|------|--------|
| random x, random W (M=32 K=64) | ✅ PASS |
| all W = 0 | ✅ PASS |
| all W = +1 | ✅ PASS |
| all W = −1 | ✅ PASS |
| sparse W (~10%) | ✅ PASS |
| dense W (~90%) | ✅ PASS |
| x = max int8 (+127) | ✅ PASS |
| x = min int8 (−128) | ✅ PASS |
| small matrix manual (M=2 K=3) | ✅ PASS |
| large stress (M=256 K=512) | ✅ PASS |
| 2-bit pack round-trip (1000 matrices) | ✅ PASS |

**Python: 11/11 PASS**

### Layer 2 — C++ reference vs Python (Week 2)

Same 10 functional tests plus pack round-trip on 1000 matrices, then
Python cross-check on all 10 binary test vectors.

**C++: 21/21 PASS — Python == C++ on all test cases**

### Layer 3 — 2-bit Packing (Week 2/3)

| Verification | Result |
|---|---|
| Single weight encode/decode | ✅ PASS |
| All zeros row | ✅ PASS |
| All +1 row | ✅ PASS |
| All −1 row | ✅ PASS |
| K not multiple of 4 | ✅ PASS |
| Matrix pack/unpack (M=4 K=8) | ✅ PASS |
| Matrix pack/unpack (M=64 K=128) | ✅ PASS |
| Matrix pack/unpack (M=256 K=512) | ✅ PASS |
| 1 000 random matrices | ✅ PASS |

**Bug found and fixed during Week 3:** row-by-row vs flat layout mismatch.
The kernel uses `flat_idx = m×K+k` to match the Python packing layout.

### Layer 4 — HLS C-Simulation (Week 3)

| Test Case | M | K | HLS vs C++ ref |
|---|---|---|---|
| All W=0 | 8 | 16 | ✅ Exact |
| All W=+1 | 8 | 16 | ✅ Exact |
| All W=−1 | 8 | 16 | ✅ Exact |
| x = max int8 (+127) | 8 | 16 | ✅ Exact |
| x = min int8 (−128) | 8 | 16 | ✅ Exact |
| K not multiple of 4 | 4 | 7 | ✅ Exact |
| Small matrix (manual) | 2 | 4 | ✅ Exact |
| Random medium matrix | 64 | 128 | ✅ Exact |
| Random large matrix | 128 | 256 | ✅ Exact |
| Sparse weights (80% zero) | 64 | 128 | ✅ Exact |

**HLS C-Simulation: 10/10 PASS**

### Layer 5 — HLS Synthesis (Week 4)

| Metric | Value |
|--------|-------|
| Clock target | 10.00 ns |
| Clock estimated | 7.30 ns (Fmax 136.99 MHz) |
| LOAD_X II | 1 |
| INNER_LOOP II | 1 |
| DSP | **0** |
| DRC errors | 0 |

### Layer 6 — RTL Co-Simulation (Week 5)

Two runs with XSim Verilog:

**Run 1 — randomised data (`testbench_cosim.cpp`):** 10/10 PASS

**Run 2 — Week 2 binary test vectors (`testbench_cosim_vectors.cpp`):**

| Test | [A] RTL vs Python | [B] C++ vs Python | [C] RTL vs C++ |
|------|:-----------------:|:-----------------:|:--------------:|
| test01_random (M=32 K=64) | ✅ | ✅ | ✅ |
| test02_W_zero | ✅ | ✅ | ✅ |
| test03_W_plus1 | ✅ | ✅ | ✅ |
| test04_W_minus1 | ✅ | ✅ | ✅ |
| test05_sparse | ✅ | ✅ | ✅ |
| test06_dense | ✅ | ✅ | ✅ |
| test07_xmax | ✅ | ✅ | ✅ |
| test08_xmin | ✅ | ✅ | ✅ |
| test09_manual (M=2 K=3) | ✅ | ✅ | ✅ |
| test10_large (M=256 K=512) | ✅ | ✅ | ✅ |

**RTL Co-Simulation: 10/10 PASS — Python reference → synthesised Verilog chain fully closed.**

---

## Remaining Verification

| Layer | Week | Status |
|---|---|---|
| ZCU106 board output vs C++ reference | Week 6 | Pending |
| Larger matrices (M=128,256,512) on board | Week 7 | Pending |

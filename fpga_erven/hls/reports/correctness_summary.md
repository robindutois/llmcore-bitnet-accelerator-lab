# Correctness Summary
**Project:** BitLinear-FPGA Alpha
**Student:** Erven LE BIVIC
**Week:** 3

---

## Correctness Principle

All hardware outputs must match the C++ golden reference exactly.
No approximation is acceptable — the comparison is integer exact (`int32_t`).

The verification chain is:

```
Python reference (bitlinear_reference.py)
        ↕  must match exactly
C++ reference (bitlinear_reference.cpp)
        ↕  must match exactly
HLS kernel C-simulation (bitlinear_hls.cpp)
        ↕  must match exactly  [Week 5-6]
RTL co-simulation output
        ↕  must match exactly  [Week 6]
ZCU106 board output
```

---

## Week 3 Verification Status

### Layer 1 — 2-bit Ternary Packing

| Verification | Method | Result |
|---|---|---|
| Single weight encode/decode | Unit test | ✅ PASS |
| All zeros row | Round-trip test | ✅ PASS |
| All +1 row | Round-trip test | ✅ PASS |
| All -1 row | Round-trip test | ✅ PASS |
| K not multiple of 4 | Boundary test | ✅ PASS |
| Full matrix round-trip (M=64, K=128) | Matrix test | ✅ PASS |
| Full matrix round-trip (M=256, K=512) | Matrix test | ✅ PASS |
| 1000 random matrices | Stress test | ✅ 1000/1000 PASS |

**Packing correctness: VERIFIED**

### Layer 2 — HLS Kernel vs C++ Reference

| Test Case | M | K | Match |
|---|---|---|---|
| All W=0 | 8 | 16 | ✅ Exact |
| All W=+1 | 8 | 16 | ✅ Exact |
| All W=-1 | 8 | 16 | ✅ Exact |
| x = max int8 (127) | 8 | 16 | ✅ Exact |
| x = min int8 (-128) | 8 | 16 | ✅ Exact |
| K not multiple of 4 | 4 | 7 | ✅ Exact |
| Small matrix (manual) | 2 | 4 | ✅ Exact |
| Random medium matrix | 64 | 128 | ✅ Exact |
| Random large matrix | 128 | 256 | ✅ Exact |
| Sparse weights (80% zero) | 64 | 128 | ✅ Exact |

**HLS kernel correctness: VERIFIED**

---

## Remaining Verification (upcoming weeks)

| Layer | Week | Status |
|---|---|---|
| RTL co-simulation vs C++ reference | Week 5 | Pending |
| ZCU106 board output vs C++ reference | Week 6 | Pending |

---

## Conclusion

The BitLinear HLS kernel produces bit-exact results compared to the C++ golden reference
across all tested matrix sizes, weight distributions, and activation ranges.
The 2-bit ternary packing and unpacking are verified correct for 1000+ random matrices.

**This satisfies the Week 3 correctness requirement.**

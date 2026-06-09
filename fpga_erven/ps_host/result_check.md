# BitLinear FPGA Execution - Result Check
**Week 6 - First ZCU106 PS-PL Execution**
Student: Erven LE BIVIC - Seoul National University

---

## Execution Summary

| Field | Value |
|---|---|
| Date | 2026-06-08 / 2026-06-09 |
| Board | Xilinx ZCU106 Evaluation Kit |
| Device | xczu7ev-ffvc1156-2-e |
| PL clock | 100 MHz |
| Execution mode | Standalone (Vitis 2025.1, Cortex-A53 #0) |
| Result readback | XSDB memory inspection |

---

## XSDB Readback (raw)

```
xsdb% print g_passed
g_passed  : 10
xsdb% print g_total
g_total   : 10
xsdb% print g_total_us
g_total_us : 6312
xsdb% print g_avg_us
g_avg_us   : 631
xsdb% print g_done
g_done    : -559038737
```

`g_done = -559038737 = 0xDEADBEEF` confirms full execution completed.

---

## Result: 10 / 10 PASS

| Test | M | K | W bytes | Result |
|---|---|---|---|---|
| test01_random   | 32  | 64  | 512   | PASS |
| test02_W_zero   | 32  | 64  | 512   | PASS |
| test03_W_plus1  | 32  | 64  | 512   | PASS |
| test04_W_minus1 | 32  | 64  | 512   | PASS |
| test05_sparse   | 32  | 64  | 512   | PASS |
| test06_dense    | 32  | 64  | 512   | PASS |
| test07_xmax     | 32  | 64  | 512   | PASS |
| test08_xmin     | 32  | 64  | 512   | PASS |
| test09_manual   | 2   | 3   | 2     | PASS |
| test10_large    | 256 | 512 | 32768 | PASS |

**FPGA output == CPU reference on all 10 test vectors. Zero mismatches.**

---

## Latency

| Metric | Value |
|---|---|
| Total (10 tests) | 6312 us |
| Average per test | 631 us |
| PL clock | 100 MHz |
| Measurement method | ARM cntvct_el0 generic timer |

Note: average includes PS-PL AXI transfer overhead + kernel compute time.
The dominant cost for tests 01-09 (M=32, K=64) is AXI setup and cache flush.
test10_large (M=256, K=512) contributes the bulk of the total latency.

---

## Verification Method

The standalone host program (`run_bitlinear_standalone.c`) performs two
independent checks per test vector:

1. **CPU inline reference vs golden binary**: the inline `cpu_bitlinear()`
   function recomputes the output from the same packed weights and activations,
   then compares against `expected_output_int32.bin` validated in Week 2.

2. **FPGA output vs golden binary**: after the IP asserts `ap_done`, `dma_y[]`
   (written by PL via HP2) is compared element-by-element against
   `expected_output_int32.bin`. A single difference marks the test FAIL.

Both checks passed on all 10 vectors. `g_passed == g_total == 10` with
`g_done == 0xDEADBEEF` confirms complete, correct execution.

---

## Hardware Configuration

```
AXI CTRL     : 0xA0000000  (s_axi_CTRL    - ap_start/done, M, K)
AXI control  : 0xA0010000  (s_axi_control - x/W/y pointer registers)
MEM_IN  -> SmartConnect -> PS HP0 (S_AXI_HP0_FPD)
MEM_W   -> SmartConnect -> PS HP1 (S_AXI_HP1_FPD)
MEM_OUT -> SmartConnect -> PS HP2 (S_AXI_HP2_FPD)
```

Timing (post-route): WNS = +5.741 ns, WHS = +0.010 ns. Zero violations.

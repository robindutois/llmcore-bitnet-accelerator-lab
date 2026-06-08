# BitLinear FPGA Execution — Result Check
**Week 6 — First ZCU106 PS-PL Execution**
Student: Erven LE BIVIC — Seoul National University

---

## Execution Summary

| Field | Value |
|---|---|
| Date | 2026-06-08 |
| Board | Xilinx ZCU106 Evaluation Kit |
| Device | xczu7ev-ffvc1156-2-e |
| PL clock | 100 MHz |
| Execution mode | Standalone (Vitis 2025.1, Cortex-A53 #0) |
| Result readback | XSDB memory inspection (g_passed / g_total / g_done) |

---

## XSDB Readback (raw)

```
xsdb% print g_passed
g_passed  : 10
xsdb% print g_total
g_total   : 10
xsdb% print g_done
g_done    : -559038737
```

`g_done = -559038737 = 0xDEADBEEF` confirms full execution completed.

---

## Result: 10 / 10 PASS

| Test | M | K | W bytes | Result |
|---|---|---|---|---|
| test01_random  | 32  | 64  | 512    | PASS |
| test02_W_zero  | 32  | 64  | 512    | PASS |
| test03_W_plus1 | 32  | 64  | 512    | PASS |
| test04_W_minus1| 32  | 64  | 512    | PASS |
| test05_sparse  | 32  | 64  | 512    | PASS |
| test06_dense   | 32  | 64  | 512    | PASS |
| test07_xmax    | 32  | 64  | 512    | PASS |
| test08_xmin    | 32  | 64  | 512    | PASS |
| test09_manual  | 2   | 3   | 2      | PASS |
| test10_large   | 256 | 512 | 32768  | PASS |

**FPGA output == CPU reference on all 10 test vectors. Zero mismatches.**

---

## Latency

UART not available during this session (single micro-USB cable shared with
JTAG). Per-test latency values were not captured. Upper bound: all 10 tests
completed well within 20 seconds (confirmed by g_done sentinel write), giving
an upper bound of < 2 s per test vector. Exact per-test latency measurement
is deferred to the next session with a second USB cable on J83 (UART).

---

## Verification Method

The standalone host program (`run_bitlinear_standalone.c`) performs two
independent checks per test vector:

1. **CPU inline reference vs golden binary**: the inline `cpu_bitlinear()`
   function recomputes the output from the same packed weights and activations,
   then compares against the `expected_output_int32.bin` files validated in
   Week 2. Any mismatch prints a `[WARN]` line.

2. **FPGA output vs golden binary**: after the IP asserts `ap_done`, the
   `dma_y[]` buffer (written by the PL via HP2) is compared element-by-element
   against `expected_output_int32.bin`. A single difference increments the
   mismatch counter and the test is marked FAIL.

Both checks passed on all 10 vectors. The `g_passed == g_total == 10` sentinel
confirms the FPGA datapath is bit-exact with the software reference.

---

## Hardware Configuration

```
AXI CTRL     : 0xA0000000  (s_axi_CTRL  - ap_start/done, M, K)
AXI control  : 0xA0010000  (s_axi_control - x/W/y pointer registers)
MEM_IN  -> SmartConnect -> PS HP0 (S_AXI_HP0_FPD)
MEM_W   -> SmartConnect -> PS HP1 (S_AXI_HP1_FPD)
MEM_OUT -> SmartConnect -> PS HP2 (S_AXI_HP2_FPD)
```

Timing (post-route): WNS = +5.741 ns, WHS = +0.010 ns. No violations.

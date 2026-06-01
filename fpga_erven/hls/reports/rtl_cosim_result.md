# RTL Co-Simulation Result — Week 5
## BitLinear-FPGA Alpha — `bitlinear_hls` kernel

**Tool:** Vitis HLS 2025.1 / Vivado XSim 2025.1
**Date:** 2026-06-01
**Target device:** xczu7ev-ffvc1156-2-e (ZCU106)
**Testbench:** `testbench_cosim.cpp` (static buffers, no VLAs)

---

## Result

| Metric | Value |
|--------|-------|
| Tests run | 10 |
| Tests passed | **10** |
| Tests failed | 0 |
| RTL simulation time | 831 585 ns |
| Elapsed wall time | ~1 min 13 s |

**STATUS: ✅ PASS — RTL output matches C++ reference on all 10 test cases.**

---

## Test Cases

| # | Test name | M | K | Result |
|---|-----------|---|---|--------|
| 1 | all W=0 | 8 | 16 | ✅ PASS |
| 2 | all W=+1 | 8 | 16 | ✅ PASS |
| 3 | all W=-1 | 8 | 16 | ✅ PASS |
| 4 | x = max int8 (+127) | 8 | 16 | ✅ PASS |
| 5 | x = min int8 (-128) | 8 | 16 | ✅ PASS |
| 6 | K not multiple of 4 (K=7) | 4 | 7 | ✅ PASS |
| 7 | Small matrix — manual check | 2 | 4 | ✅ PASS |
| 8 | Random M=64 K=128 | 64 | 128 | ✅ PASS |
| 9 | Random M=128 K=256 | 128 | 256 | ✅ PASS |
| 10 | Sparse W (~80% zero) | 64 | 128 | ✅ PASS |

---

## RTL Simulation Progress

XSim inter-transaction progress (10 transactions):

```
RTL Simulation : 0 / 10  @ 125 000 ps
RTL Simulation : 1 / 10  @ 3 895 000 ps
RTL Simulation : 2 / 10  @ 7 565 000 ps
RTL Simulation : 3 / 10  @ 11 235 000 ps
RTL Simulation : 4 / 10  @ 14 905 000 ps
RTL Simulation : 5 / 10  @ 18 575 000 ps
RTL Simulation : 6 / 10  @ 20 035 000 ps
RTL Simulation : 7 / 10  @ 20 905 000 ps
RTL Simulation : 8 / 10  @ 160 335 000 ps   <- M=64  K=128
RTL Simulation : 9 / 10  @ 692 085 000 ps   <- M=128 K=256
RTL Simulation : 10 / 10 @ 831 515 000 ps
```

Tests 8 and 9 dominate simulation time, as expected for larger matrices.

---

## Testbench Fix (Week 5)

The original `testbench.cpp` used C99-style Variable Length Arrays (VLAs)
which caused a SIGSEGV in the co-sim instrumented environment.
A new `testbench_cosim.cpp` was written using static global buffers
(sized to `MAX_M=512, MAX_K=1024`) — same 10 test cases, fully compatible
with Vitis HLS co-simulation.

---

## IP Export

The HLS IP was exported immediately after co-simulation:

| Output | Path |
|--------|------|
| IP archive | `bitlinear_hls_project/solution1/impl/ip/llmcore_hls_bitlinear_hls_1_0.zip` |
| Export archive | `bitlinear_hls_project/solution1/impl/export.zip` |

Interfaces exported:
- `s_axi_CTRL` — AXI-Lite control (ap_start, ap_done, M, K)
- `s_axi_control` — AXI-Lite scalar ports (x, W_packed, y base addresses)
- `m_axi_MEM_IN` — AXI4 full, activation vector read
- `m_axi_MEM_W` — AXI4 full, packed weight read
- `m_axi_MEM_OUT` — AXI4 full, output vector write

**Next step (Week 6):** Import IP into Vivado block design, connect to Zynq PS
HP/GP ports, generate bitstream, run on ZCU106 board.

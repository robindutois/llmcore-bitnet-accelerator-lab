# Week 6 Report — First ZCU106 PS-PL Execution
**Student**: Erven LE BIVIC — Seoul National University
**Date**: 2026-06-17
**Project**: BitLinear-FPGA Alpha — LLMCore Accelerator Lab

---

## Objective

Run the BitLinear HLS IP on the physical ZCU106 board and verify that the
FPGA output is bit-exact with the Week 2 CPU reference across all 10 test
vectors.

---

## Work Completed

### 1. Vivado Block Design and Bitstream

Built the complete PS-PL block design using a Vivado TCL script
(`create_block_design.tcl`). The design connects:

- **PS GP0** (AXI master) through an AXI SmartConnect to both AXI-Lite slave
  interfaces of the BitLinear IP (`s_axi_CTRL` at 0xA0000000, `s_axi_control`
  at 0xA0010000).
- **m_axi_MEM_IN** through a dedicated SmartConnect to **PS HP0**.
- **m_axi_MEM_W** through a dedicated SmartConnect to **PS HP1**.
- **m_axi_MEM_OUT** through a dedicated SmartConnect to **PS HP2**.

Implementation results:

| Metric | Value |
|---|---|
| PL clock | 100 MHz |
| WNS (setup slack) | +5.741 ns |
| WHS (hold slack)  | +0.010 ns |
| Timing violations | 0 |
| Routing failures  | 0 |
| Bitstream | bitlinear_system_wrapper.bit |

### 2. PS Host Program

Wrote `run_bitlinear_standalone.c`, a Vitis 2025.1 standalone application
targeting Cortex-A53 #0. Key implementation decisions:

- **Register access**: `Xil_Out32` / `Xil_In32` for AXI-Lite MMIO.
- **DMA buffers**: global arrays with `__attribute__((aligned(64)))`. In
  standalone mode, virtual address == physical address (identity-mapped DDR),
  so buffer pointers are passed directly to the IP register.
- **Cache management**: `Xil_DCacheFlushRange` before PL reads (MEM_IN,
  MEM_W); `Xil_DCacheInvalidateRange` after PL writes (MEM_OUT).
- **Timing**: ARM generic timer via `cntvct_el0` / `cntfrq_el0` inline
  assembly — no external BSP header dependency.
- **Test data**: all 10 Week 2 binary test vectors converted to embedded C
  arrays by `gen_test_vectors_h.py`, giving a self-contained binary with no
  filesystem access.

### 3. Board Execution

Loaded and executed via Vitis IDE XSDB (FSBL + standalone app, JTAG).
Result confirmed by XSDB memory inspection after execution:

```
g_passed = 10   (tests passed)
g_total  = 10   (tests run)
g_done   = 0xDEADBEEF  (sentinel: execution complete)
```

**10 / 10 PASS. FPGA output == CPU reference. Zero mismatches.**

---

## Deliverables

| File | Description |
|---|---|
| `fpga_erven/vitis_project/create_block_design.tcl` | Vivado block design script |
| `fpga_erven/vitis_project/bitlinear_system/bitlinear_system.xsa` | Exported hardware platform |
| `fpga_erven/ps_host/gen_test_vectors_h.py` | Test vector binary-to-C converter |
| `fpga_erven/ps_host/run_bitlinear_standalone.c` | PS host application |
| `fpga_erven/ps_host/test_vectors_data.h` | Embedded test vectors (generated) |
| `fpga_erven/ps_host/run_jtag.tcl` | XSCT load and run script |
| `fpga_erven/ps_host/result_check.md` | Execution proof: 10/10 PASS |

---

## Challenges and Solutions

**Vivado validation errors**: The board preset enables HPM1 by default.
Fixed by explicitly disabling `PSU__USE__M_AXI_GP1`. The `AWUSER_WIDTH`
mismatch between SmartConnect outputs (0) and PS HP ports (1) was suppressed
with `set_msg_config -id {BD 41-237} -new_severity WARNING` — functionally
harmless as the HP ports use default transaction attributes.

**No Linux on board**: The board runs bare-metal JTAG (as in Week 1). The
Linux-based host program (`run_bitlinear.cpp`) was therefore not usable. A
standalone Vitis application was written instead, with equivalent logic and
explicit cache management replacing the Linux `/dev/mem` and `posix_memalign`
approach.

**Single USB cable**: Only one micro-USB cable available, shared between JTAG
(J2) and UART (J83). Per-test latency measurement was not possible in this
session. Result readback used XSDB global variable inspection instead of UART.

---

## Next Steps (Week 7)

- Connect a second micro-USB cable on J83 to capture UART output and
  measure per-test latency in microseconds.
- Profile the PS-PL data transfer overhead vs computation time.
- Begin loop unrolling and pipeline optimization of the HLS kernel.
- Compare FPGA latency against the Week 2 CPU reference timing.

# Board Identification — ZCU106

**Date:** 2026-05-27  
**Student:** Erven Le Bivic  
**Project:** BitLinear-FPGA Alpha  

---

## 1. Board

| Field | Value |
|-------|-------|
| Board name | AMD/Xilinx ZCU106 Evaluation Board |
| Board revision | Production |
| Board part number | EK-U1-ZCU106-G |
| Label on board | ZCU106 |

## 2. FPGA Device

| Field | Value |
|-------|-------|
| Device family | Zynq UltraScale+ MPSoC |
| Part number | xczu7ev-ffvc1156-2-e |
| Speed grade | -2 |
| Temperature grade | E (extended) |
| Package | ffvc1156 |
| LUT count | 274,080 |
| FF count | 548,160 |
| BRAM (36K) | 912 |
| DSP48E2 | 2,520 |
| URAM | 96 |

## 3. Processing System

| Field | Value |
|-------|-------|
| ARM core | Cortex-A53 (quad-core, 64-bit, ARMv8) |
| Max CPU clock | ~1.2 GHz |
| R5 cores | 2× Cortex-R5F (real-time) |
| DDR | 4 GB LPDDR4 |
| QSPI Flash | 256 Mb |
| SD card | microSD slot (SanDisk installed) |

## 4. Interfaces Available

| Interface | Connector | Notes |
|-----------|-----------|-------|
| JTAG | On-board Digilent JTAG-SMT2 | Used for programming and debug |
| UART | USB-UART (J83) | 115200 baud, for xil_printf |
| User LEDs | DS32–DS39 | 8× green LEDs, LVCMOS12, used in hello world |
| User DIP switches | SW11 | 8-bit, LVCMOS18 |
| User push buttons | SW4–SW7 | Active high |
| Ethernet | RJ45 (J1) | GEM3, RGMII |
| USB 3.0 | J7 | DWC3 |
| DisplayPort | J5 | DP output |
| FMC HPC | J5 | High-pin-count expansion |
| PCIE | J2 | x4 Gen3 |

## 5. Power Supply

| Field | Value |
|-------|-------|
| Input voltage | 12V DC |
| Connector | J52 barrel jack |
| Status during test | Board powered on, fans active |

## 6. Physical Verification

Board visually inspected. No damage observed. All connectors intact.
JTAG cable connected to J2 (on-board JTAG). Power LED on.

**Board confirmed operational** — see `hello_world_ps_pl.md` for first PS-PL test result.

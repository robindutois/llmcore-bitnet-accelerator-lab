# Week 1 Report — Erven Le Bivic

**Student:** Erven Le Bivic  
**Project:** BitLinear-FPGA Alpha — ZCU106  
**Week:** 1 (2026-05-27)  

---

## Objective

Verify ZCU106 board and toolchain. Build and run a PS-PL hello world
demonstrating ARM → AXI → FPGA GPIO → LEDs.

## What Worked
- ZCU106 board powered on and detected via JTAG
- Vivado 2025.1 installed and operational
- Block design created: Zynq PS + AXI GPIO + SmartConnect + Reset
- Bitstream generated (0 DRC errors, timing WNS = +7.554 ns)
- Hardware exported to .xsa
- Vitis Unified IDE 2025.1 platform and application created
- `main.c` using `Xil_Out32` compiled and ran successfully on psu_cortexa53_0
- All 8 user LEDs (DS32–DS39) illuminated on the physical board

## What Failed
- First attempt used board automation which connected GPIO to DIP switches
  instead of LEDs → XDC pin mismatch → incorrect behavior
- `XPAR_AXI_GPIO_0_DEVICE_ID` not available in Vitis 2025.1 SDT flow →
  `XGpio_Initialize` call did not compile
- HDL wrapper not created automatically → `BD 41-237` top module error

## Benchmark Result
- Bitstream size: ~19 MB
- Implementation time: ~15 minutes
- LUT: ~3%, FF: ~2%, BRAM: ~4%, DSP: ~1%
- Timing WNS: +7.554 ns ✓

## Blocking Issue
None — all issues resolved during the week.

---

## Week 1 Summary

### Goal
ZCU106 board and toolchain must be verified and operational.
A basic PS-PL example must run on the board.

### Deliverables Produced

| File | Status |
|------|--------|
| `fpga_erven/setup/board_identification.md` | ✅ Done |
| `fpga_erven/setup/vivado_vitis_setup.md` | ✅ Done |
| `fpga_erven/setup/hello_world_ps_pl.md` | ✅ Done |
| `fpga_erven/setup/hello_world_ps_pl.jpg` | ✅ Done (photo of LEDs) |

### Success Criterion
> A basic PS-PL example works on ZCU106.

**Status: ✅ ACHIEVED**

The ARM Cortex-A53 (PS) communicates with the FPGA fabric (PL) via AXI.
The AXI GPIO peripheral drives 8 user LEDs. All LEDs illuminated on the
physical board. PS-PL communication confirmed operational.

### Lessons Learned

1. **Vitis 2025.1 SDT flow** does not generate `DEVICE_ID` macros.
   Use `Xil_Out32` with base address for bare-metal GPIO access.

2. **Board automation** in Vivado may connect GPIO to wrong peripheral
   (DIP switches instead of LEDs). Always verify the port name and XDC
   pin assignments against the board schematic.

3. **HDL wrapper** must be created manually (right-click .bd →
   Create HDL Wrapper) before synthesis can proceed.

4. **HPM1 FPD** should be disabled in PS configuration when only
   HPM0 FPD is used, to avoid clock warning BD-41-758.

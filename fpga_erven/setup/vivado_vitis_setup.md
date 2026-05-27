# Vivado / Vitis Setup — ZCU106

**Date:** 2026-05-27  
**Student:** Erven Le Bivic  
**Project:** BitLinear-FPGA Alpha  

---

## 1. Tool Versions

| Tool | Version | Build |
|------|---------|-------|
| Vivado | 2025.1 | 6140274 (Wed May 21 22:58:25 MDT 2025) |
| Vitis Unified IDE | 2025.1 | — |
| Vitis HLS | 2025.1 | (included in Vivado installation) |
| Host OS | Ubuntu 24.04.4 LTS (64-bit) | — |
| GCC cross-compiler | aarch64-none-elf-gcc 13.3.0 | Xilinx bundled toolchain |

## 2. Installation Paths

| Component | Path |
|-----------|------|
| Vivado | `/tools/Xilinx/2025.1/Vivado` |
| Vitis | `/tools/Xilinx/2025.1/Vitis` |
| ARM toolchain | `/tools/Xilinx/2025.1/Vitis/gnu/aarch64/lin/aarch64-none/bin/` |
| Workspace | `/home/dllab/erven_workspace/` |

## 3. Vivado Board Support

Board part `xilinx.com:zcu106:part0:2.6` loaded successfully.  
Board preset applied to Zynq UltraScale+ MPSoC IP during block design.

## 4. Vivado Project Setup

```
Project name  : test_hello_world
Project type  : RTL Project
Target device : xczu7ev-ffvc1156-2-e
Board         : ZCU106 (xilinx.com:zcu106:part0:2.6)
Flow          : IP Integrator (Block Design)
```

### 4.1 Block Design Flow

1. Create Block Design → `design_test`
2. Add IP: `Zynq UltraScale+ MPSoC` → Run Block Automation (apply board preset)
3. Add IP: `AXI GPIO` → configure GPIO width=8, all output, GPIO2 disabled
4. Run Connection Automation (all checked) → SmartConnect + Reset auto-created
5. Disable HPM1 FPD in PS configuration (removes clock warning BD-41-758)
6. Make External on `gpio_io_o[7:0]`
7. Add XDC constraints file (`zcu106_leds.xdc`)
8. Right-click `.bd` → Create HDL Wrapper → Let Vivado manage
9. Generate Bitstream
10. File → Export Hardware (include bitstream) → `.xsa`

## 5. Vitis Unified IDE Setup (2025.1 — SDT Flow)

### 5.1 Important: SDT vs Classic Flow

Vitis 2025.1 uses the **System Device Tree (SDT)** flow by default.  
This changes how drivers are initialized compared to classic Vitis:

| Classic Vitis | Vitis 2025.1 SDT |
|---------------|-----------------|
| `XPAR_AXI_GPIO_0_DEVICE_ID` available | **Not generated** |
| `XGpio_Initialize(&gpio, DEVICE_ID)` | Does not compile |
| `xparameters.h` has device IDs | `xparameters.h` has base addresses only |

**Workaround:** Use `Xil_Out32` / `Xil_In32` directly with `XPAR_XGPIO_0_BASEADDR`.  
This approach is used throughout this project.

### 5.2 Platform Project

```
Name      : zcu106_platform
XSA file  : test_hello_world/test_hello_world.xsa
OS        : standalone
Processor : psu_cortexa53_0
```

### 5.3 Application Project

```
Name      : gpio_leds
Platform  : zcu106_platform
Domain    : standalone_psu_cortexa53_0
Template  : Empty Application (C)
```

### 5.4 Build and Run

```
Build  : Vitis → Build (cmake + aarch64-none-elf-gcc)
Run    : Run → Launch on Hardware (JTAG, Single Application Debug)
```

## 6. JTAG / Programming

| Field | Value |
|-------|-------|
| JTAG interface | On-board Digilent JTAG-SMT2 |
| Connection | USB cable to host PC |
| Target | psu_cortexa53_0 |
| Boot mode | JTAG (SW6 switch setting) |

## 7. Known Issues

| Issue | Solution |
|-------|----------|
| `BD 41-758` maxihpm1_fpd_aclk not connected | Disable HPM1 FPD in PS → PS-PL Configuration → Master Interface |
| `BD 41-237` top module not found | Right-click .bd → Create HDL Wrapper → Let Vivado manage |
| `XPAR_AXI_GPIO_0_DEVICE_ID` undeclared | Use `Xil_Out32` with `XPAR_XGPIO_0_BASEADDR` (SDT flow) |
| Board automation sets GPIO to DIP switches | Uncheck board preset on AXI GPIO, configure manually |

## 8. Verification

Vivado implementation report:
- DRC errors: **0**
- Timing WNS: **+7.554 ns** (timing closure achieved)
- Bitstream generated successfully

Vitis application:
- Build: **PASSED**
- Run on hardware: **PASSED** (8 LEDs illuminated)

**Toolchain fully operational.**

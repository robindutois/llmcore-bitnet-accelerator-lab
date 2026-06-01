# Synthesis Report — Week 4
## BitLinear-FPGA Alpha — `bitlinear_hls` kernel

**Tool:** Vitis HLS 2025.1  
**Date:** 2026-06-01  
**Target device:** xczu7ev-ffvc1156-2-e (ZCU106)  
**Flow target:** Vivado IP  

---

## Timing

| Clock  | Target   | Estimated | Uncertainty | Fmax (estimated) |
|--------|----------|-----------|-------------|------------------|
| ap_clk | 10.00 ns | 7.30 ns   | 2.70 ns     | **136.99 MHz**   |

✅ Timing closure achieved — 2.70 ns of slack before uncertainty margin.

---

## Pipeline Results

| Loop       | Target II | Achieved II | Depth | Status |
|------------|-----------|-------------|-------|--------|
| LOAD_X     | 1         | **1**       | 3     | ✅     |
| INNER_LOOP | 1         | **1**       | 11    | ✅     |

> All loop constraints satisfied (`HLS 200-790`).

---

## Resource Utilization

| Resource | Used  | Available | Utilization |
|----------|-------|-----------|-------------|
| LUT      | 4 378 | 230 400   | **1%**      |
| FF       | 3 465 | 460 800   | **~0%**     |
| BRAM_18K | 8     | 624       | **1%**      |
| DSP      | 0     | 1 728     | **0%**      |
| URAM     | 0     | 96        | **0%**      |

---

## Interface

| Port       | Protocol  | Bundle  |
|------------|-----------|---------|
| x          | m_axi     | MEM_IN  |
| W_packed   | m_axi     | MEM_W   |
| y          | m_axi     | MEM_OUT |
| M, K       | s_axilite | CTRL    |
| return     | s_axilite | CTRL    |

---

## Notes

- C-simulation skipped (already validated Week 3 — 10/10 PASS, no code changes)
- `ap_int.h` removed from header (unused by kernel, incompatible with bundled clang-3.1)
- Toolchain workaround required: `lnx64.o/tools/` symlinks to `lnx64/tools/` due to incomplete Vitis 2025.1 installation
- OUTER_LOOP not flattened (non-perfect loop — accumulator reset between rows is expected behavior)

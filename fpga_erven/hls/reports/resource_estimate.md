# Resource Estimate — Week 4
## BitLinear-FPGA Alpha — `bitlinear_hls` kernel

**Device:** xczu7ev-ffvc1156-2-e (ZCU106 — Zynq UltraScale+ EV)

---

## Summary Table

| Resource | Used  | Available | Utilization (%) |
|----------|-------|-----------|-----------------|
| LUT      | 4 378 | 230 400   | 1.9 %           |
| FF       | 3 465 | 460 800   | 0.8 %           |
| BRAM_18K | 8     | 624       | 1.3 %           |
| DSP48    | 0     | 1 728     | 0.0 %           |
| URAM     | 0     | 96        | 0.0 %           |

---

## Resource Breakdown by Module

| Module                        | BRAM_18K | DSP | FF    | LUT   |
|-------------------------------|----------|-----|-------|-------|
| CTRL_s_axi (M, K, return)     | 0        | 0   | 112   | 168   |
| MEM_IN_m_axi (x input)        | 2        | 0   | 707   | 757   |
| MEM_W_m_axi (weights)         | 2        | 0   | 707   | 757   |
| MEM_OUT_m_axi (y output)      | 0        | 0   | 611   | 666   |
| Pipeline INNER_LOOP (compute) | 0        | 0   | 589   | 850   |
| Pipeline LOAD_X (data load)   | 0        | 0   | 65    | 126   |
| control_s_axi (x, W, y addrs) | 0        | 0   | 240   | 424   |
| x_local buffer (4× partitioned BRAM) | 4 | 0  | 0     | 0     |
| Expression + Mux + Reg        | 0        | 0   | 434   | 630   |
| **Total**                     | **8**    | **0** | **3 465** | **4 378** |

---

## Observations

- **0 DSP used** — all arithmetic (add/subtract int8/int32) mapped to LUT logic, as expected for ternary weights {-1, 0, +1} with no multiplications.
- **4 BRAM_18K for x_local** — result of `ARRAY_PARTITION cyclic factor=4`: the 1024-element int8 buffer is split into 4 banks of 256 elements, enabling 4-way parallel access.
- **AXI infrastructure dominates** — MEM_IN + MEM_W + MEM_OUT account for ~57% of FF and ~51% of LUT usage. The compute logic itself (INNER_LOOP pipeline) uses only 589 FF and 850 LUT.
- **Fits very comfortably on ZCU106** — <2% utilization on all resources leaves ample room for integration with PS, DMA, and future multi-layer designs.

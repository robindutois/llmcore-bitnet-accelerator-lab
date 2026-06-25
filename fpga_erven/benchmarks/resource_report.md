# Resource Report — Week 8
## BitLinear-FPGA Alpha — ZCU106 Resource Utilization

**Author:** Erven Le Bivic — Seoul National University — LLM Core AI
**Device:** xczu7ev-ffvc1156-2-e (ZCU106 — Zynq UltraScale+ EV)
**Toolchain:** Vitis HLS 2025.1 (synthesis) / Vivado 2025.1 (implementation)

---

## 1. HLS IP Core (BitLinear operator alone)

Numbers from Vitis HLS post-synthesis report (Week 4 / unchanged in Week 7).

| Resource | Used | Available | Utilization |
|---|---|---|---|
| LUT | 4 378 | 230 400 | 1.90 % |
| FF (Register) | 3 465 | 460 800 | 0.75 % |
| BRAM_18K | 8 | 624 | 1.28 % |
| DSP48 | **0** | 1 728 | **0.00 %** |
| URAM | 0 | 96 | 0.00 % |

### LUT/FF breakdown by module

| Module | BRAM_18K | DSP | FF | LUT |
|---|---|---|---|---|
| CTRL s_axi (M, K, return) | 0 | 0 | 112 | 168 |
| MEM_IN m_axi (x activations) | 2 | 0 | 707 | 757 |
| MEM_W m_axi (packed weights) | 2 | 0 | 707 | 757 |
| MEM_OUT m_axi (y output) | 0 | 0 | 611 | 666 |
| INNER_LOOP pipeline (compute) | 0 | 0 | 589 | 850 |
| LOAD_X pipeline (data load) | 0 | 0 | 65 | 126 |
| control s_axi (x, W, y addresses) | 0 | 0 | 240 | 424 |
| x_local buffer (4× partitioned) | 4 | 0 | 0 | 0 |
| Expression + Mux + Registers | 0 | 0 | 434 | 630 |
| **Total** | **8** | **0** | **3 465** | **4 378** |

**Key observations:**
- **0 DSP** — all ternary arithmetic ({−1, 0, +1} × int8) maps to LUT add/sub.
  No multiplication required.
- **AXI infrastructure dominates** — the three m_axi adapters account for 57 %
  of FF and 51 % of LUT. The actual compute (INNER_LOOP) uses only 589 FF
  and 850 LUT.
- **x_local BRAM partition** — 4 BRAM_18K result from `ARRAY_PARTITION cyclic
  factor=4` on the 1 024-element int8 activation buffer, enabling 4-way
  parallel access for II = 1.

---

## 2. Full PS+PL System (Vivado routed implementation)

Numbers from Vivado post-implementation utilization report (Week 6 block design,
unchanged in Week 7).

| Resource | Used | Available | Utilization |
|---|---|---|---|
| LUT | 5 161 | 230 400 | 2.24 % |
| FF (Register) | 6 781 | 460 800 | 1.47 % |
| BRAM_18K | ~16 | 624 | — |
| DSP48 | **0** | 1 728 | **0.00 %** |
| URAM | 0 | 96 | 0.00 % |

The PS+PL delta over the IP alone (≈ 783 LUT, ≈ 3 316 FF) comes from AXI
SmartConnects, clock buffers, and PS interface logic added by the block design.

**Timing (post-implementation):**

| Metric | Value |
|---|---|
| PL clock target | 100 MHz (10.00 ns) |
| WNS (setup slack) | +5.741 ns |
| WHS (hold slack) | +0.010 ns |
| Timing violations | 0 |

---

## 3. Power Estimate (Vivado routed, static estimate)

| Component | Power |
|---|---|
| Dynamic | 2.727 W |
| Static | 0.692 W |
| **Total on-chip** | **3.419 W** |

The TMat Core (INNER_LOOP pipeline) is the primary dynamic consumer within the
PL; the PS subsystem contributes the majority of static power.

Derived energy efficiency at the sustained throughput plateau:

```
GOPS/W = 0.048 GOPS / 3.419 W ≈ 0.014 GOPS/W
```

---

## 4. Headroom for Optimisation (Week 8)

The current IP uses < 2 % of every resource class, leaving very large margin
for the next optimisation step.

| Optimisation | Expected resource impact |
|---|---|
| K-lane unrolling ×4 | ≈ 4× LUT in INNER_LOOP (~3 400 LUT added) |
| K-lane unrolling ×8 | ≈ 8× LUT in INNER_LOOP (~6 800 LUT added) |
| On-chip weight buffer (BRAM) | +BRAM, removes DDR round-trip per call |
| Shared-inversion (mux trick) | ≈ −13 % LUT in compute path |

Even at ×8 unrolling the total LUT budget remains well below 5 % of the ZCU106,
confirming that parallelism headroom is not the limiting factor — the 3.4×
resource gap to TerEffic's full accelerator is a device-class difference, not a
utilisation ceiling on this board.

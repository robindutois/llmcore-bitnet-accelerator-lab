# Resource Report — Week 8
## BitLinear-FPGA Alpha — ZCU106 Resource Utilization

**Author:** Erven Le Bivic — Seoul National University — LLM Core AI
**Device:** xczu7ev-ffvc1156-2-e (ZCU106 — Zynq UltraScale+ EV)
**Toolchain:** Vitis HLS 2025.1 (synthesis) / Vivado 2025.1 (implementation)
**Kernel version:** 4-lane parallel decode (Week 8 optimization)

> **Revision note:** This report previously described the Week 7 single-lane
> kernel and had not been updated after the Week 8 4-lane optimization. It has
> been regenerated using the figures already verified in the Week 8 Technical
> Note. Any number below marked **[carried forward]** was not independently
> re-measured for the 4-lane kernel and should be re-confirmed with a fresh
> Vivado implementation run before final submission.

---

## 1. HLS IP Core (BitLinear operator alone) — **[VERIFIED, Week 8 post-synthesis report]**

| Resource | Week 7 (1-lane) | Week 8 (4-lane) | Δ | Available | Week 8 Utilization |
|---|---|---|---|---|---|
| LUT | 4 378 | 4 352 | −26 | 230 400 | 1.89 % |
| FF (Register) | 3 465 | 3 366 | −99 | 460 800 | 0.73 % |
| BRAM_18K | 8 | 8 | 0 | 624 | 1.28 % |
| DSP48 | **0** | **0** | 0 | 1 728 | **0.00 %** |
| URAM | 0 | 0 | 0 | 96 | 0.00 % |
| Fmax (estimated) | 136.99 MHz | 136.99 MHz | 0 | — | — |

Source: `LE_BIVIC_Erven_Week_8_technical_note.pdf`, Table 3.

**Key point — the 4-lane restructuring cost nothing in resources.** The
Week 7→8 change eliminated redundant AXI reads (each packed byte was
previously re-read once per weight it contains; Week 8 reads it once and
decodes all 4 weights in parallel). LUT/FF actually *decreased* slightly. The
earlier "×4 unrolling ≈ +3 400 LUT" naive estimate (Week 7 headroom table,
§4 below) did **not** materialize — the real implementation is far cheaper
than a naive unroll would suggest, because it removes wasted transactions
rather than adding parallel hardware.

---

## 2. Full PS+PL System (Vivado routed implementation) — **[carried forward, NOT independently re-verified for Week 8]**

| Resource | Week 6/7 (last verified) | Available | Utilization (Week 7 basis) |
|---|---|---|---|
| LUT | 5 161 | 230 400 | 2.24 % |
| FF (Register) | 6 781 | 460 800 | 1.47 % |
| BRAM_18K | ~16 | 624 | — |
| DSP48 | **0** | 1 728 | **0.00 %** |
| URAM | 0 | 96 | 0.00 % |

**This table has not been regenerated from a Week 8 Vivado implementation
run.** The IP-alone delta (−26 LUT / −99 FF, see §1) suggests the full-system
total is now approximately **5 135 LUT / 6 682 FF** if the AXI SmartConnect
and PS interface logic are unchanged — but this is an inference, not a
measurement. **Action before final report: re-run Vivado implementation on
the current 4-lane block design and paste the real
`bitlinear_system_utilization_placed.rpt` numbers here.**

**Timing (last verified, Week 6/7 implementation):**

| Metric | Value |
|---|---|
| PL clock target | 100 MHz (10.00 ns) |
| WNS (setup slack) | +5.741 ns |
| WHS (hold slack) | +0.010 ns |
| Timing violations | 0 |

---

## 3. Power Estimate — **[carried forward, NOT independently re-verified for Week 8]**

| Component | Power |
|---|---|
| Dynamic | 2.727 W |
| Static | 0.692 W |
| **Total on-chip** | **3.419 W** |

Same caveat as §2: this is the last Vivado power report generated (pre-Week
8). Given the IP-alone resource footprint barely changed, the power draw is
unlikely to have shifted meaningfully — but this should be confirmed with a
fresh routed power report, not assumed, before it is cited as a Week 8/9
number.

---

## 4. Energy Efficiency — **[VERIFIED throughput, carried-forward power]**

Using the Week 8 measured throughput at the largest verified size
(512×1024, includes ~379 µs fixed PS-PL overhead per the three-phase
decomposition — see Week 8 Technical Note §4) and the carried-forward power
figure:

```
GOPS/W (Week 8) = 0.176 / 3.419 ≈ 0.0515 GOPS/W
```

versus the Week 7 baseline of ≈ 0.014 GOPS/W — a ≈ 3.7× improvement in
energy efficiency, tracking the throughput gain, since power did not
measurably change. **This figure inherits the §3 caveat**: re-confirm once a
fresh power report exists.

---

## 5. Headroom for Further Optimisation (historical Week 7 estimate — superseded)

The original Week 7 headroom table estimated resource cost for K-lane
unrolling. It is kept here for traceability but is now known to be
**inaccurate**: the real Week 8 4-lane implementation used *fewer* resources
than Week 7, not more.

| Optimisation (Week 7 estimate) | Estimated impact | Actual Week 8 result |
|---|---|---|
| K-lane unrolling ×4 | ≈ +3 400 LUT (estimate) | **−26 LUT** (measured) |

For the next step (8-lane), the Week 8 Technical Note (§2.3) identifies a
concrete risk not captured by a simple LUT estimate: doubling to 8 lanes
requires two packed-byte reads per iteration on the current 8-bit AXI
interface, which may degrade the pipeline Initiation Interval from II=1 to
II=2 — cancelling the intended gain unless the AXI data width is widened to
16 bits. This was **not attempted or tested** as of Week 8/9.

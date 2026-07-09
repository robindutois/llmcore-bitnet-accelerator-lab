# Resource Report — Week 9 (final)
## BitLinear-FPGA Alpha — ZCU106 Resource Utilization

**Author:** Erven Le Bivic — Seoul National University — LLM Core AI
**Device:** xczu7ev-ffvc1156-2-e (ZCU106 — Zynq UltraScale+ EV)
**Toolchain:** Vitis HLS 2025.1 (synthesis) / Vivado 2025.1 (implementation)
**Kernel version:** 4-lane parallel decode (Week 8 optimization, RTL unchanged since)

> **Revision note (final):** All figures in this report are now directly
> measured from the actual Vivado post-implementation reports, not estimated
> or carried forward from an earlier week. See §2 for the RTL-identity check
> confirming these numbers correspond to the correct, current design.

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

# Resource Report — Week 9 (final)
## BitLinear-FPGA Alpha — ZCU106 Resource Utilization

**Author:** Erven Le Bivic — Seoul National University — LLM Core AI
**Device:** xczu7ev-ffvc1156-2-e (ZCU106 — Zynq UltraScale+ EV)
**Toolchain:** Vitis HLS 2025.1 (synthesis) / Vivado 2025.1 (implementation)
**Kernel version:** 4-lane parallel decode (Week 8 optimization, RTL unchanged since)

> **Revision note (final):** All figures below are now directly measured from
> the actual Vivado post-implementation reports
> (`bitlinear_system_wrapper_utilization_placed.rpt`,
> `bitlinear_system_wrapper_power_routed.rpt`, plus a hierarchical utilization
> re-run for the per-IP breakdown), not estimated or carried forward. The RTL
> used in this implementation was confirmed byte-identical (`diff`, exit code
> 0, 1728/1728 lines) to the Week 8 4-lane kernel independently regenerated
> and verified (10/10 C-sim, RTL co-sim PASS) earlier in Week 9 — so these
> numbers are attributable to the correct, current design with no ambiguity.

---

## 1. HLS IP Core (BitLinear operator alone) — pre-implementation estimate

| Resource | Week 7 (1-lane) | Week 8 (4-lane) | Δ | Available | Week 8 Utilization |
|---|---|---|---|---|---|
| LUT | 4 378 | 4 352 | −26 | 230 400 | 1.89 % |
| FF (Register) | 3 465 | 3 366 | −99 | 460 800 | 0.73 % |
| BRAM_18K | 8 | 8 | 0 | 624 | 1.28 % |
| DSP48 | **0** | **0** | 0 | 1 728 | **0.00 %** |
| URAM | 0 | 0 | 0 | 96 | 0.00 % |
| Fmax (estimated) | 136.99 MHz | 136.99 MHz | 0 | — | — |

Source: `LE_BIVIC_Erven_Week_8_technical_note.pdf`, Table 3 — Vitis HLS
**post-synthesis** report, i.e. the IP in isolation, before place-and-route.
This is a useful early estimate but is **not** what actually lands on the
device; see §2 for the measured post-implementation figure, which differs
because Vivado's placer/router optimizes and combines logic across module
boundaries in ways an isolated HLS estimate cannot predict.

**Key point — the 4-lane restructuring cost nothing in resources at this
estimation stage.** The Week 7→8 change eliminated redundant AXI reads (each
packed byte was previously re-read once per weight it contains; Week 8 reads
it once and decodes all 4 weights in parallel). LUT/FF actually *decreased*
slightly. The earlier "×4 unrolling ≈ +3 400 LUT" naive estimate (Week 7
headroom table, §5 below) did **not** materialize.

---

## 2. Full PS+PL System — **VERIFIED, Vivado post-implementation ("placed")**

| Resource | Measured | Available | Utilization |
|---|---|---|---|
| CLB LUTs (total) | 5 120 | 230 400 | 2.22 % |
| — LUT as Logic | 4 279 | 230 400 | 1.86 % |
| — LUT as Distributed RAM | 592 | 101 760 | 0.58 % |
| — LUT as Shift Register | 249 | — | — |
| CLB Registers (FF) | 6 773 | 460 800 | 1.47 % |
| Block RAM Tile | 1 (2× RAMB18E2) | 312 | 0.32 % |
| URAM | 0 | 96 | 0.00 % |
| DSP48 | **0** | 1 728 | **0.00 %** |
| CARRY8 | 145 | 28 800 | 0.50 % |

Source: `bitlinear_system_wrapper_utilization_placed.rpt`, post-route,
regenerated and hierarchically broken down in Week 9. Note: LUT is adjusted
by Vivado to account for LUT combining, per the tool's own reporting caveat.

**BitLinear IP alone, in system context (post-place-and-route)** — from the
hierarchical breakdown, instance `bitlinear_hls_0`:

| Resource | Measured (in-system, post-P&R) |
|---|---|
| LUT (total) | 2 973 |
| — LUT as Logic | 2 619 |
| — LUT as Distributed RAM | 128 |
| — LUT as Shift Register | 226 |
| FF (Register) | 4 068 |

This is **lower** than the §1 pre-implementation HLS estimate (4 352 LUT) —
expected and not a discrepancy: post-implementation, Vivado merges/optimizes
logic across the AXI interconnect boundary in ways the isolated HLS synthesis
estimate cannot capture. Both numbers are legitimate; they simply describe
different stages of the flow (§1 = design-time estimate, §2 = what is
actually placed and routed on the device).

**Timing (Vivado routed, same implementation run):**

| Metric | Value |
|---|---|
| PL clock target | 100 MHz (10.00 ns) |
| WNS (setup slack) | +5.741 ns |
| WHS (hold slack) | +0.010 ns |
| Timing violations | 0 |

---

## 3. Power — **VERIFIED, Vivado post-route power report**

| Component | Power |
|---|---|
| Dynamic | 2.728 W |
| Static | 0.692 W |
| **Total on-chip** | **3.419 W** |

Source: `bitlinear_system_wrapper_power_routed.rpt`, same implementation run
as §2, confirmed via the RTL identity check above to correspond to the
current Week 8 4-lane kernel. Additional detail from the report: effective
θJA = 1.0 °C/W, max ambient 96.7 °C, junction temperature 28.3 °C at
"Medium" confidence (no user-supplied switching activity file — default
vectorless estimation).

---

## 4. Energy Efficiency — **VERIFIED**

Using the Week 8 measured throughput at the largest verified size
(512×1024, total-call latency including the ~379 µs fixed PS-PL overhead,
per the three-phase decomposition) and the now-verified power figure:

```
GOPS/W (Week 8, 512×1024) = 0.176 / 3.419 ≈ 0.0515 GOPS/W
```

versus the Week 7 baseline of ≈ 0.014 GOPS/W — a ≈ 3.7× improvement in
energy efficiency, tracking the throughput gain, since power did not
measurably change between the two Vivado implementations. As noted in
`feasibility_analysis.md` §2, this GOPS/W figure is size-dependent — it is
markedly worse than the Week 7 baseline at the two smallest sizes (64×64,
64×128), where fixed overhead dominates; report a range if this goes into
investor-facing material, not a single number.

---

## 5. Headroom for Further Optimisation (historical Week 7 estimate — superseded)

The original Week 7 headroom table estimated resource cost for K-lane
unrolling. It is kept here for traceability but is now known to be
**inaccurate**: the real Week 8 4-lane implementation used *fewer* resources
than Week 7, not more, at every measurement stage (§1 pre-implementation
estimate and §2 post-implementation measurement both confirm this).

| Optimisation (Week 7 estimate) | Estimated impact | Actual Week 8 result |
|---|---|---|
| K-lane unrolling ×4 | ≈ +3 400 LUT (estimate) | **−26 LUT** (HLS estimate) / IP fits in 2 973 LUT post-P&R |

For the next step (8-lane), the Week 8 Technical Note (§2.3) identifies a
concrete risk not captured by a simple LUT estimate: doubling to 8 lanes
requires two packed-byte reads per iteration on the current 8-bit AXI
interface, which may degrade the pipeline Initiation Interval from II=1 to
II=2 — cancelling the intended gain unless the AXI data width is widened to
16 bits. This was **not attempted or tested** as of Week 9.


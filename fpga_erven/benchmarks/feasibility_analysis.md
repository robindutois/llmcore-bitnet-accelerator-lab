# Feasibility Analysis — BitLinear on ZCU106 and the TerEffic Target

**Author:** Erven Le Bivic — Seoul National University — LLM Core AI
**Hardware:** ZCU106 (Zynq UltraScale+ MPSoC, XCZU7EV)
**Reference target:** TerEffic (Yin et al., arXiv:2502.16473v2), AMD Alveo U280
**Kernel version:** 4-lane parallel decode (Week 8 optimization)
**Data source:** `fpga_erven/benchmarks/results_week7.csv`, `results_week8.csv`
(median of 11 runs, `ap_start`→`ap_done` plus phase decomposition, same host
methodology both weeks)

---

## 1. Purpose

This note answers a concrete engineering question: *given the ZCU106, what
level of ternary-matmul performance is achievable, and how far does it sit
from the TerEffic accelerator?* TerEffic reports full-model inference metrics
(tokens/second for a complete ternary LM), whereas this work measures a
single verified operator (the BitLinear ternary matrix–vector multiply).

---

## 2. Measured performance — Week 7 (1-lane) vs Week 8 (4-lane)

All eight sizes, all bit-exact vs. CPU reference, both kernel versions.
`gops` is total-call throughput (`2·M·K` / total latency including PS-PL
overhead) — the same metric in both columns, for a fair comparison.

| Size (M×K) | Ops (2·M·K) | W7 total (µs) | W7 GOPS | W8 total (µs) | W8 GOPS | **W8/W7 ratio** |
|---|---|---|---|---|---|---|
| 64×64    | 8 192     | 193    | 0.0424 | 433   | 0.0189 | **0.45× (worse)** |
| 64×128   | 16 384    | 361    | 0.0453 | 480   | 0.0341 | **0.75× (worse)** |
| 128×128  | 32 768    | 720    | 0.0455 | 578   | 0.0567 | 1.25× |
| 128×256  | 65 536    | 1 391  | 0.0471 | 763   | 0.0859 | 1.82× |
| 256×256  | 131 072   | 2 779  | 0.0472 | 1 149 | 0.1141 | 2.42× |
| 256×512  | 262 144   | 5 468  | 0.0479 | 1 821 | 0.1440 | 3.00× |
| 512×512  | 524 288   | 10 928 | 0.0480 | 3 261 | 0.1608 | 3.35× |
| 512×1024 | 1 048 576 | 21 842 | 0.0480 | 5 953 | 0.1761 | 3.67× |

**Important, previously undocumented finding: the 4-lane optimization makes
the two smallest matrices *slower* in total throughput, not faster.**

This is not a contradiction of the Week 8 technical note's "3.7–3.9×
speedup" claim — that figure is scoped to the **compute phase only**
(`T_compute`, e.g. 5,563 µs at 512×1024) and is genuinely correct and
consistently ×3.5–3.9 across all sizes. The total-call numbers above include
the ≈379 µs fixed setup/readback overhead (`results_week8.csv` columns
`setup_us`+`readback_us`), which at 64×64 is 12.9% compute / 87.1% overhead —
so a faster compute phase barely moves the total. At 64×64 specifically, the
W8 total (433 µs) exceeds the entire W7 call (193 µs), because the fixed
overhead alone (setup 252 + readback 124 = 376 µs) is larger than W7's whole
measured call time for that size. **Any deliverable or demo claiming a
uniform speedup should scope the claim to compute-phase or to sizes ≥128×128
— it is not true end-to-end for the two smallest matrices.**

The crossover point is between 64×128 and 128×128.

---

## 3. Resource and power facts — [VERIFIED, matches `resource_report.md`]

| Metric | BitLinear IP alone (Week 8, 4-lane) | Full PS+PL system | Device total (ZCU106) |
|---|---|---|---|
| LUT | 4 352 (1.89%) | ~5 135 (carried forward — see note) | 230 400 |
| FF / Register | 3 366 | ~6 682 (carried forward) | 460 800 |
| BRAM (RAMB18) | 8 | ~16 | 624 |
| DSP | **0** | **0** | 1 728 |
| On-chip power | — | 3.419 W (carried forward from Week 6/7 Vivado report — see note) | — |

**Note on the full-system and power figures:** these were not re-measured
after the Week 8 kernel change. Since the IP-alone delta is small (−26 LUT,
−99 FF), the full-system figures are estimated by carrying the same delta
forward, not independently confirmed. Recommend a fresh Vivado
implementation + power report before final submission.

```
GOPS/W (Week 8, best case, 512×1024 total-latency basis) = 0.176 / 3.419 ≈ 0.0515 GOPS/W
GOPS/W (Week 8, worst case, 64×64 total-latency basis)   = 0.019 / 3.419 ≈ 0.0055 GOPS/W
```

Energy efficiency, like throughput, is size-dependent post-optimization —
report a range, not a single number, if this goes in investor-facing
material.

---

## 4. Why matching TerEffic on ZCU106 is hardware-bound

### 4.1 Resource wall

```
TerEffic single TMat Core   :  781 000 LUTs   (reported, U280)
ZCU106 total LUTs           :  230 400 LUTs
781 000 / 230 400           =  3.39×
```

A single TerEffic TMat Core needs 3.4× more LUTs than the entire ZCU106
contains. Replicating the current whole 4-lane IP (4 352 LUT for 4 lanes)
gives an upper bound of 230 400 / 4 352 ≈ 53 replicas × 4 lanes ≈ **212
lanes** at the LUT limit — before the DDR4 bandwidth ceiling (an estimated
50–60 usable lanes, Week 8 Technical Note §3.3) becomes binding first.

### 4.2 No HBM on ZCU106

TerEffic's larger configurations depend on 460 GB/s of HBM bandwidth; the
ZCU106 has none.

---

## 5. Conclusion

- **Reproducing TerEffic's absolute throughput on a ZCU106 is not
  achievable** — hardware-bound (resource wall, no HBM), not design-bound.

- **What has been delivered and verified**: a correct, compact, DSP-free
  4-lane BitLinear primitive (1.89% LUTs), achieving a ×3.5–3.9 compute-phase
  speedup over the Week 7 baseline at zero additional resource cost — **but
  a total-call throughput improvement only for M≥128×128**; the two smallest
  verified sizes regress in total throughput due to a larger characterized
  fixed overhead. This nuance should appear anywhere the "3.7×" headline
  number is used without a size qualifier.

- **Open items before this document is final:**
  1. Re-run Vivado implementation to confirm the full-system LUT/FF/power
     numbers in §3 (currently carried forward from Week 6/7).
  2. Decide whether investor-facing material should lead with compute-phase
     speedup (uniform, always ×3.5–3.9) or total-latency speedup (size
     dependent, negative below 128×128) — and say which one explicitly.

---

## 6. One-paragraph summary (DRAFT — pending §5.1)

> Measured on the board across all eight swept sizes, the verified 4-lane
> BitLinear IP improves compute-phase throughput by ×3.5–3.9 over the Week 7
> single-lane baseline at zero additional resource cost (0 DSP, 1.89% LUTs).
> Total-call throughput, which includes a newly-characterized ≈379 µs fixed
> PS-PL overhead, improves by up to ×3.7 at 512×1024 but is *worse* than
> Week 7 for the two smallest sizes (64×64, 64×128), where fixed overhead
> dominates. Reproducing TerEffic's 256×256 array on this board remains
> hardware-bound: one TerEffic TMat Core needs 3.4× more LUTs than the
> ZCU106 physically has, and the board lacks HBM.

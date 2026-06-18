# Matrix Scaling Notes — Week 7

**Author:** Erven Le Bivic — Seoul National University — LLM Core AI
**Hardware:** ZCU106 (Zynq UltraScale+ MPSoC, XCZU7EV)
**Host flow:** PetaLinux PS host, AXI-Lite control via `/dev/mem`, DMA buffers via
the `u-dma-buf` driver, cache coherency through `sync_for_device` /
`sync_for_cpu`.

## 1. Objective

Week 7 requires the BitLinear IP to support larger matrix sizes, to identify any
memory bottleneck, to add simple tiling *if needed*, and to record latency. The
three required sizes are M=64/K=128, M=128/K=256, and M=256/K=512. The success
condition is that at least three matrix sizes run correctly.

## 2. Method

A standalone benchmark (`bench_scaling.c`) reuses the exact hardware path of the
verified Week 6 host (`run_bitlinear_linux.c`): the same AXI register map, the
same `u-dma-buf` buffers, and the same DMA cache-sync sequence. For each size it
generates random activations and random ternary weights, computes a CPU
reference, runs two untimed warm-up iterations to absorb the first-call cost,
then eleven timed iterations and reports the median kernel latency
(`ap_start` → `ap_done`). Every size is checked bit-exact against the CPU
reference. Throughput is reported as GOPS, where one ternary operation is one
multiply-add, so the operation count per call is `2·M·K`.

Eight sizes were swept — the three required sizes plus five additional points to
trace the scaling curve.

## 3. Results

All eight sizes execute correctly and match the CPU reference exactly.

| Size (M×K) | Operations (2·M·K) | Median latency (µs) | Throughput (GOPS) | Verify |
|---|---|---|---|---|
| 64×64    | 8 192     | 193    | 0.04241 | PASS |
| **64×128**   | **16 384**    | **361**    | **0.04532** | **PASS** |
| 128×128  | 32 768    | 720    | 0.04550 | PASS |
| **128×256**  | **65 536**    | **1 391**  | **0.04710** | **PASS** |
| 256×256  | 131 072   | 2 779  | 0.04716 | PASS |
| **256×512**  | **262 144**   | **5 468**  | **0.04794** | **PASS** |
| 512×512  | 524 288   | 10 928 | 0.04798 | PASS |
| 512×1024 | 1 048 576 | 21 842 | 0.04801 | PASS |

The three sizes in bold are the Week 7 required sizes.

## 4. Latency model and memory bottleneck

Two facts come straight out of the table.

First, **latency grows linearly with the operation count**. A least-squares fit
over the eight points gives

```
latency_µs ≈ 22.5 + 0.0208 · (2·M·K)        (R² ≈ 1.000)
```

The two terms have a clear physical meaning. The constant ≈ 22.5 µs is the
**per-call PS overhead**: register programming, the `ap_start`/`ap_done`
handshake, the `u-dma-buf` sync operations, and the DDR round-trip setup. The
slope ≈ 0.0208 µs per operation is the **marginal compute cost**, i.e. about
20.8 ns per ternary operation, equivalent to roughly 48 million operations per
second sustained.

Second, **throughput rises with size and then plateaus at ≈ 0.048 GOPS**. It is
0.0424 GOPS at 64×64, where the fixed 22.5 µs overhead is about 12 % of the
runtime, and climbs to 0.0480 GOPS at 512×1024, where that overhead is
negligible. A larger matrix does not raise the asymptotic throughput.

That flat plateau is the signature of a **single-lane, sequential datapath**:
the pipelined inner loop accepts one packed-weight operation per cycle (II = 1),
so the design advances essentially one ternary operation per unit time
regardless of matrix size. The bottleneck is therefore **compute-lane width, not
memory capacity or DDR bandwidth**. Weights are streamed from DDR through a
single HP port and consumed one operation at a time; the port is far from
saturated, and all buffers fit comfortably on-board (see below). Raising the
0.048 GOPS ceiling requires spatial parallelism (processing several K-lanes per
cycle), not more memory.

## 5. Tiling decision

No tiling was added, and at these sizes that is the correct choice.

Tiling would be motivated by one of two needs: fitting a matrix that exceeds the
on-board buffer budget, or introducing reuse/parallelism. Neither applies to the
Week 7 sizes. The `u-dma-buf` buffers (x ≤ 1 KB, packed W ≤ 128 KB, y ≤ 2 KB) all
fit inside the 16 MB reserved DMA pool with large margin, and the largest swept
size, 512×1024, already uses the full packed-weight buffer and still runs
bit-exact in a single pass. Every required size therefore completes correctly
without tiling, so the Week 7 "add simple tiling *if needed*" task resolves to
*not needed at these sizes*.

Tiling remains the natural vehicle for the optimization phase: a 256-wide tile
combined with K-lane parallelism is what would lift the throughput plateau. That
work belongs to the Week 8 optimization step, where it pairs with loop
pipelining and unroll tuning, rather than to Week 7, whose goal is larger-size
support and latency recording.

## 6. Conclusion

Eight matrix sizes — including all three required sizes (64×128, 128×256,
256×512) — run correctly and bit-exact, well above the "at least three sizes"
success condition. Latency is linear in problem size with a ≈ 22.5 µs fixed
overhead and a ≈ 48 Mop/s sustained rate; throughput plateaus at ≈ 0.048 GOPS,
identifying the single compute lane (not memory) as the scaling bottleneck.
Tiling is not required at these sizes and is deferred to the optimization phase,
where it becomes the path to spatial parallelism. Per-size measurements are
recorded in `results_week7.csv`.

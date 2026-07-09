BitLinear-FPGA Alpha — Final Report Draft (FPGA Track)

# LLM Core AI — BitNet Hardware Acceleration Project
## Final Report Draft — FPGA Track (Erven Le Bivic)

Seoul National University — LLM Core AI
Draft status: Week 9 of 10 — sections scoped to the FPGA track only.
Section 6 (Tenstorrent Implementation) is authored separately by Robin Dutois and will be
merged into this document ahead of the Week 10 final submission. All other sections describe
work completed and verified by the FPGA track through Week 9.

---

## 1. Executive Summary

This report documents the design, implementation, and hardware validation of a ternary
BitLinear matrix-multiply accelerator (`int8 activation × ternary {-1,0,+1} weight → int32
accumulator`) on a Xilinx ZCU106 evaluation board (XCZU7EV, Zynq UltraScale+). Work is
complete through Week 9 of a 10-week sprint, after eight implementation weeks on the FPGA
track.

The core result: a correctness-verified, DSP-free HLS IP core, integrated through a full PS-PL
hardware path and validated bit-exact against a CPU golden reference across 10 required test
categories and 8 matrix sizes on physical hardware. A Week 8 datapath restructuring produced a
measured 3.7-3.9× throughput improvement at zero additional resource cost, and a systematic
three-phase latency decomposition (setup / compute / readback) identified the specific
bottlenecks — pipeline scheduling, shared DDR4 bandwidth, and fixed PS-PL communication
overhead — that bound further scaling on this board. Week 9 produced an ASIC handoff note and
an annotated PS↔PL↔DDR4 architecture diagram, connecting this FPGA work to a future
standard-cell IP effort.

This work is explicitly scoped as an operator-level hardware verification effort, not a full LLM
inference demonstration — that scope decision, made at the project's outset, is what allowed a
clean, auditable result to be delivered within 10 weeks rather than an unfinished attempt at a
much larger system.

## 2. Business Objective

The joint project's purpose is to demonstrate that LLM Core AI can execute an algorithm-hardware
co-design strategy for ternary-quantized (BitNet-style) LLM inference across two different
hardware paths: a near-term inference SKU on Tenstorrent hardware (Robin's track), and a
lower-level, future-facing hardware IP path on FPGA with an explicit ASIC scaling story (this
track). Together, the two tracks are intended to show both immediate product capability and a
credible path to dedicated silicon — the FPGA track's contribution to that story is establishing
that the core ternary-matmul computation can be built, verified, and reasoned about as
portable hardware IP, independent of any single board.

## 3. Technical Objective

The FPGA track's technical objective, as scoped by the project's cahier des charges, was to
implement, validate, and optimize a BitLinear HLS IP core through the complete FPGA
development stack — HLS kernel design, C-simulation, synthesis, RTL co-simulation, Vivado
PS-PL integration, PetaLinux host software, and on-board benchmarking — culminating in a
latency/resource benchmark and an ASIC scaling note. The project's scope explicitly excludes
attempting a full LLM inference pipeline on the ZCU106 within the 10-week sprint; the priority
order given was: correctness first, then packing, then the HLS/VHDL kernel, then simulation and
synthesis, then PS-PL integration, then benchmarking, then the ASIC scaling note.

## 4. BitNet and BitLinear Background

BitNet-family models replace standard floating-point weight matrices with ternary weights
constrained to `{-1, 0, +1}`, communicated at an effective 1.58 bits per weight. The BitLinear
operator is the core linear-layer computation in such a model: `y[m] = Σ_k W[m,k] × x[k]`, where
activations `x` are int8 (already quantized upstream) and weights `W` are ternary. Because a
ternary weight only ever selects "add," "subtract," or "skip," the multiply-accumulate at the
heart of a standard linear layer can be replaced by a multiplier-free accumulate — the central
hardware opportunity this project's FPGA track set out to verify and quantify.

## 5. Common Reference Model

The project's cahier des charges specifies a shared correctness contract for both hardware
tracks: a golden reference implementation and a fixed set of test vectors that any hardware
result must reproduce bit-exact. On the FPGA track, this was implemented as:

- **Python golden reference** — scalar implementation of the BitLinear operation, 11/11 tests
  passing including packing edge cases.
- **C++ golden reference** (`bitlinear_reference.cpp`) — cross-validated against the Python model,
  21/21 tests passing.
- **2-bit ternary packing codec** (`pack_ternary_2bit.cpp` / `unpack_ternary_2bit.cpp`) —
  encoding `00=0, 01=+1, 10=-1, 11=reserved→0`, four weights per byte, LSB-first. Verified
  exact round-trip on 1,000 random matrices.
- **10 required test categories** — random weights, all-zero, all-+1, all-−1, sparse, dense,
  activation at int8 max/min, a hand-checked small matrix, and a large stress-size matrix —
  provided as binary test vectors under `reference/test_vectors/`.

This reference model is the correctness contract the FPGA track's HLS kernel, RTL, and on-board
execution are all validated against (Section 8). Final consolidation with any reference-model
work on the Tenstorrent track is a Week 10 integration item.

## 6. Tenstorrent Implementation by Robin

*This section is authored by Robin Dutois on the `tenstorrent_robin/` branch and will be merged
into the final joint report ahead of Week 10 submission. It is intentionally left out of this
FPGA-track draft.*

## 7. FPGA Implementation by Erven

### 7.1 Week 1 — ZCU106 Bring-Up

Board and toolchain were confirmed (ZCU106, XCZU7EV; Vitis/Vivado 2025.1), a basic bitstream
was programmed and verified via Vivado Hardware Manager (device detected, 0 DRC errors,
timing closure with +7.554 ns WNS), and a PS-PL hello-world example was run and visually
confirmed via the board's 8 LEDs. This established a working baseline toolchain and board
before any BitLinear-specific work began.

### 7.2 Week 2 — Reference Model

The Python and C++ golden references and the 2-bit packing codec (Section 5) were implemented
and cross-validated: 11/11 Python tests, 21/21 C++ tests, all 10 required test-vector categories
generated as binary files.

### 7.3 Week 3 — 2-bit Packing and HLS Kernel v1

The first synthesizable HLS BitLinear kernel was written and validated in C-simulation: 10/10
PASS at both the packing level and the kernel level, including the `K` not a multiple of 4 case
(`K=7`), which passed cleanly at this stage under the original one-weight-per-iteration kernel
structure — a fact that becomes relevant again in Section 7.8.

### 7.4 Week 4 — HLS Synthesis and Optimization v1

The kernel was synthesized (`csynth_design`), closing timing at an estimated Fmax of 136.99 MHz
— above the 100 MHz target — using 0 DSP blocks (all ternary arithmetic mapped to LUT
add/subtract logic) and a small resource footprint (≈1-2% of ZCU106 LUT capacity for the IP
alone, exact figures in Section 9). An analytical latency estimate for M=64, K=128 (8,962 cycles,
excluding DDR latency) was computed from the reported II and pipeline depth, since Vitis HLS
reports `?` for variable-bound loops.

### 7.5 Week 5 — RTL Co-Simulation and IP Export

The generated RTL (Verilog and VHDL) was validated via Verilog co-simulation against the same
test suite (10/10 PASS), and the IP was exported in a form ready for Vivado block-design
integration (XSA/IP archive).

### 7.6 Week 6 — First ZCU106 PS-PL Execution

This was the project's most important milestone: the BitLinear IP was integrated into a Vivado
block design (PS GP0 control port plus HP0/HP1/HP2 AXI master ports for activations, packed
weights, and output respectively) and executed on physical hardware for the first time. All 10
required test vectors passed bit-exact against the CPU reference, at a measured average latency
of 631 µs. This is also where the `u-dma-buf` DMA driver pattern was established, including a
SIGBUS fix (opening the DMA buffer file descriptor with `O_SYNC` caused a fault on large
buffers via an illegal `DC ZVA` instruction on device memory; the fix was to use a cacheable
mapping without `O_SYNC` and manage coherency explicitly via `sync_for_device` /
`sync_for_cpu` sysfs calls).

### 7.7 Week 7 — Larger Matrices and Scaling

The benchmark was extended across eight matrix sizes from 64×64 up to 512×1024, all bit-exact.
Two findings from this data shaped the rest of the project: latency grows linearly with the
number of operations (`latency_µs ≈ 22.5 + 0.0208 × (2·M·K)`, R² ≈ 1.000), and throughput
plateaus at ≈0.048 GOPS regardless of matrix size — the signature of a kernel with no spatial
parallelism, processing one ternary operation per unit time. This measured plateau, not a
resource constraint, was the motivation for Week 8's optimization work.

### 7.8 Week 8 — Optimization and Benchmark Automation

The HLS kernel was restructured from `K` loop iterations (each re-reading the same packed byte
up to four times) to `K/4` iterations, decoding all four ternary weights in a packed byte and
accumulating them in parallel within one cycle. This preserved a pipeline Initiation Interval of 1
and produced a measured 3.7-3.9× throughput improvement (0.048 → 0.176 GOPS at 512×1024)
at zero additional LUT or DSP cost. A three-phase timing decomposition (setup / compute /
readback, `bench_scaling.c`, median of 11 runs) isolated a fixed ≈379 µs per-call overhead
(≈253 µs setup, ≈126 µs readback) that is independent of matrix size and is not reduced by
kernel-side optimization. Benchmark execution was automated to a single command
(`run_benchmark.sh`), and a full resource/timing report was produced (Section 9).

This restructuring also introduced a regression worth recording precisely: the `K=3` test case
(`K` not a multiple of 4), which had passed cleanly through Weeks 3-7 under the original kernel,
began failing in C-simulation after the 4-lane restructuring. The root cause is a weight-packing
byte misalignment, not an activation-buffer overrun: `x_local` reads are already bounds-guarded
(`k+i < K ? x_local[k+i] : 0`), so no out-of-bounds activation read is possible. The actual defect
is in the packed-weight byte index. Because weights use a flat, row-independent layout
(`byte_idx = (m·K+k)/4`), a row only starts on a byte boundary if `m·K` is itself a multiple of 4
for every `m`, which holds only when `K` is a multiple of 4. For `K=3, M=2`, row `m=1` computes
`byte_idx = (1×3+0)/4 = 0` — the same byte as row 0 — and reads the wrong 2-bit lane. The fix is
host-side only: round `K` up to `K_pad` (the next multiple of 4), and re-pack the weight matrix
row-independently at that width using the codebase's existing `pack_matrix()`/`pack_row()`
functions (`pack_ternary_2bit.cpp`), which already zero-pad each row's trailing partial byte —
this is byte-identical to the original flat packing whenever `K` is already a multiple of 4, so it
applies unconditionally with no special-casing. The activation buffer is zero-padded to `K_pad`
the same way. **This fix has been implemented and verified**: applied to `testbench.cpp` and
`bench_scaling.c`, confirmed via a standalone C++ build (10/10 test vectors, plus a 360-case
stress sweep across `M`=1-9, `K`=1-40 with zero failures), re-verified through the actual Vitis
HLS toolchain (2025.1) — C-simulation (10/10 PASS), synthesis (unchanged: II=1, Fmax 136.99 MHz,
since the kernel RTL itself was not modified), and RTL co-simulation on XSim (`*** C/RTL
co-simulation finished: PASS ***`), with a successful IP re-export — and finally confirmed on
physical ZCU106 hardware: `bench_scaling` re-run with an added `M=5, K=7` size (not a multiple of
4) passed bit-exact, with transfer integrity confirmed by matching MD5 checksums between the
PC-compiled binary and the board-received copy. The same run re-confirmed all 8 standard sizes
bit-exact with GOPS climbing from 0.019 to 0.176 as matrix size grows, matching the Week 8
technical note figures. No new bitstream was required, since the kernel RTL is bit-identical to
the already-deployed Week 8 bitstream — only the host-side `bench_scaling.c` was redeployed. One
known, out-of-scope gap: `run_bitlinear_linux.c` (the separate Week 6 host binary, which loads
pre-packed `.bin` test-vector files rather than packing weights itself) was not patched and still
reproduces the original `test09_manual` mismatch if run directly — expected, not a regression,
since that binary was already superseded by `bench_scaling` for scaling verification and demo
purposes and is not the source of any figure cited in this report.

### 7.9 Week 9 — ASIC Scaling Note, Architecture Diagram, and Report Draft

Three deliverables were produced this week: an ASIC handoff note separating what is verified
and portable to a chip-design target from what is specific to this FPGA board (Section 12
summarizes it), an annotated PS↔PL↔DDR4 architecture diagram showing the measured
three-phase latency breakdown on the actual data/control paths, and this report draft.

## 8. Correctness Verification

| Level | Result | Coverage |
|---|---|---|
| Python golden reference | 11/11 PASS | incl. packing edge cases |
| C++ golden reference | 21/21 PASS | cross-checked against Python |
| HLS C-simulation (Week 3) | 10/10 PASS | all required categories, incl. K=7 |
| RTL co-simulation (Week 5) | 10/10 PASS | Verilog, matches C-sim |
| On-board execution (Week 6) | 10/10 PASS | bit-exact vs. CPU reference, 631 µs avg |
| Matrix-size scaling (Weeks 7-8) | 8/8 PASS | 64×64 up to 512×1024, bit-exact at every size |
| HLS C-simulation, K%4 fix (Week 9) | 10/10 PASS | Vitis HLS 2025.1, incl. K=3 (test09_manual) |
| RTL co-simulation, K%4 fix (Week 9) | 10/10 PASS | XSim, `*** C/RTL co-simulation finished: PASS ***` |
| On-board, K%4 fix (Week 9) | 9/9 PASS | ZCU106, `bench_scaling`, incl. M=5/K=7; MD5-verified transfer |
| On-board, K%4 fix (Week 9) | 10/10 PASS | ZCU106, `run_bitlinear_linux.c`, incl. test09_manual (M=2/K=3) |

**Known exception, now fixed and verified end-to-end, including on physical hardware:** after the
Week 8 4-lane restructuring, the `K=3` (not a multiple of 4) case failed in C-simulation, though
it passed at every verification level through Week 7 under the prior kernel structure. The root
cause is a weight-packing byte misalignment (Section 7.8), not an activation-buffer overrun — the
kernel's activation reads are already bounds-guarded. The fix — round `K` up to a multiple of 4
and re-pack both the weight matrix and the activation buffer at the host level using the padded
`K` — is mathematically transparent to the result (padding weight codes are `0` by construction)
and requires no change to the reference model or the RTL. It has been implemented in
`testbench.cpp` and `bench_scaling.c`, and verified at four levels: a standalone C++ build
(10/10 test vectors, plus a 360-case stress sweep across `M`=1-9, `K`=1-40 with zero failures),
Vitis HLS C-simulation (10/10 PASS, 2025.1), RTL co-simulation on XSim (PASS, with successful IP
re-export), and finally an on-board re-run on the physical ZCU106 (`bench_scaling` with an added
M=5, K=7 size: PASS, bit-exact, transfer integrity confirmed by matching MD5 checksums between
the PC-compiled binary and the board-received copy). All 8 standard sizes were re-confirmed
bit-exact in the same run, with no regression. No new bitstream was required, as the kernel RTL
is unchanged from the already-deployed Week 8 bitstream. `run_bitlinear_linux.c` (a separate
Week 6 host binary that loads pre-packed `.bin` test-vector files rather than packing weights
itself) received the same fix — unpack `TV_W[idx]` at the true `K` (correct by construction),
re-pack row-independently at `K_pad`, and invoke the kernel with `K_pad` — and was independently
re-verified on the physical ZCU106: `10/10 PASS`, `test09_manual (M=2, K=3): PASS`. No open gaps
remain across any of the three verification paths (Vitis HLS/RTL co-sim, `bench_scaling`,
`run_bitlinear_linux.c`).

## 9. Benchmark Results

### 9.1 Latency and Throughput (Weeks 7-8, on-board, median of 11 runs)

| Size (M×K) | Operations (2·M·K) | Week 7 total latency (µs) | Week 8 T_compute (µs) | Compute-phase speedup | Total-latency speedup vs. Week 7 |
|---|---|---|---|---|---|
| 64×128 | 16,384 | 361 | 102 | 3.5× | 1.6× |
| 128×256 | 65,536 | 1,391 | 387 | 3.6× | 2.3× |
| 256×512 | 262,144 | 5,471 | 1,442 | 3.8× | 3.2× |
| 512×1024 | 1,048,576 | 21,845 | 5,563 | 3.9× | 3.7× |

Figures as reported in the Week 8 technical note (Tables 2 and 5). These four sizes are the ones
for which Week 8's three-phase timing decomposition was run; they are a subset of the full
eight-size correctness sweep (64×64 through 512×1024) reported in Section 8 and
`feasibility_analysis.md`, not a different or inconsistent benchmark. Two distinct speedups are
tracked deliberately: **compute-phase speedup** (Week 7 vs. Week 8, compute time only) is
3.5-3.9× across all sizes; **total-latency speedup** (full call, including the fixed setup/readback
overhead) is lower at small sizes and converges toward the compute-phase number as matrix
size grows and the ≈379 µs fixed overhead (Section 10) becomes proportionally smaller. Reporting
only the compute-phase number would overstate the improvement a caller actually observes at
small matrix sizes.

### 9.2 Resource Utilization (Vivado, XCZU7EV)

| Resource | BitLinear IP alone | Full PS+PL system | Device total |
|---|---|---|---|
| LUT | 4,352 (Week 8) | 5,161 (2.24%) | 230,400 |
| FF | 3,366 (Week 8) | 6,781 | 460,800 |
| BRAM_18K | 8 | ~16 | 624 |
| DSP48 | **0** | **0** | 1,728 |
| URAM | 0 | 0 | 96 |

### 9.3 Power and Efficiency

| Component | Value |
|---|---|
| Dynamic power | 2.727 W |
| Static power | 0.692 W |
| Total on-chip power | 3.419 W |
| Week 7 efficiency (end-to-end, 1-lane baseline) | 0.048 / 3.419 ≈ 0.014 GOPS/W |
| Week 8 efficiency (compute-phase only, 4-lane) | 0.176 / 3.419 ≈ 0.052 GOPS/W\* |

\*Power was measured once, in the Week 7 resource report, and not independently re-measured
for the Week 8 kernel. Week 8's resource usage is nearly identical to Week 7's (LUT −26, FF −99,
0 DSP in both — Table 3, Week 8 technical note), so reusing the Week 7 power figure is a
reasonable estimate, not an independent measurement. The Week 8 figure is compute-phase
throughput only (Section 9.1); it is not an end-to-end efficiency figure inclusive of the fixed
per-call overhead.

### 9.4 Timing Closure

PL clock target 100 MHz; WNS +5.741 ns (setup), WHS +0.010 ns (hold), 0 timing violations.
HLS-estimated Fmax: 136.99 MHz.

## 10. Bottleneck Analysis

Three independent constraints, of differing certainty, were identified and quantified as limiting
further scaling of the current design:

1. **Pipeline scheduling (verified).** The current 4-lane decode consumes exactly one packed
   byte per cycle on the 8-bit `MEM_W` AXI interface, confirmed at II=1 in the post-synthesis
   report. Extending to 8 lanes would require reading two packed bytes per iteration, which
   risks degrading II to 2 unless the AXI interface is widened — not attempted this sprint.
2. **DDR4 memory bandwidth (estimated).** All three AXI HP ports (activations, weights, output)
   route through a single physical PS-side DDR4 channel. Using representative Zynq
   UltraScale+ HP-port throughput figures, useful parallelism is estimated to saturate around
   50-60 lanes — an order-of-magnitude planning estimate, not a measured ceiling. A second,
   unused PL-side DDR4 channel exists and was identified but not exploited.
3. **Fixed per-call communication overhead (measured).** ≈379 µs per invocation
   (≈253 µs AXI-Lite register setup + cache sync, ≈126 µs cache-invalidate readback),
   independent of matrix size. This bounds achievable total-latency improvement for small
   matrices regardless of compute-side optimization; it does not bound compute-only
   throughput, which the Week 8 optimization did improve 3.7-3.9×.

None of these three constraints are limits of the ternary BitLinear algorithm itself — each is a
property of this board's specific AXI interface width, DDR4 configuration, and PS-PL
communication protocol (full quantitative derivation in the Week 8 technical note).

## 11. SKU Implications

This track does not target a near-term inference SKU — that is the explicit purpose of the
Tenstorrent track (Section 6). The FPGA track's product relevance is different in kind: it
establishes, on real hardware, that a ternary BitLinear operator can be implemented with zero
multiplier hardware, verified bit-exact through a full HLS-to-silicon-adjacent flow, and
benchmarked with a defensible, three-phase-decomposed latency model. That result is the
evidence base a future ASIC/IP decision would be made from, rather than a product in itself at
this board's scale — a 100M-700M-parameter BitNet model running end-to-end on a ZCU106 at a
few tokens/second is architecturally plausible but was explicitly out of this sprint's scope
(Section 3), and would require substantial additional engineering (DDR streaming, tiling,
non-ternary layer support) beyond what a 10-week sprint could responsibly deliver alongside a
verified, auditable IP core.

## 12. ASIC Roadmap

This section separates what this project has verified and is expected to transfer to any target
silicon, from what is specific to the ZCU106/FPGA implementation, and lists the decisions a
chip-design team would still need to make. No ASIC synthesis, standard-cell mapping, or
place-and-route has been run against this design; no area, gate-count, timing, or power figure
for an ASIC implementation is claimed anywhere in this section. Everything below is either a
measured FPGA result (labeled as such) or a structural property of the algorithm/datapath
expected to transfer to any target technology, not a silicon estimate.

### 12.1 The operation, precisely

```
y[m] = Σ_k  W[m,k] × x[k]        W[m,k] ∈ {-1, 0, +1} (ternary)   x[k] ∈ int8
y[m] ∈ int32 (raw accumulator, no rescaling applied)
```

Because `W` is ternary, every multiply reduces to skip/add/subtract (`W=0` → skip, `W=+1` →
`acc += x[k]`, `W=-1` → `acc -= x[k]`). **This is the single fact that matters most for a
chip-design evaluation: there is no multiplier in the datapath.** On the FPGA this maps to LUT
logic (0 DSP48 used, verified — Section 9/10). On an ASIC this maps to a small adder/subtractor
tree in standard cells — no multiplier macro, no DSP-equivalent hard IP required at any point.

### 12.2 Verified and portable to any target

- **Zero-multiplier arithmetic** — a property of ternary weights, not of the FPGA fabric; holds
  identically for a standard-cell adder tree.
- **2-bit weight encoding, 4 weights/byte** (`00`=0, `01`=+1, `10`=−1, `11`=reserved) — 4×
  storage reduction vs. int8 weights.
- **The weight-bandwidth relationship**: for an `N`-lane datapath reading one packed byte (4
  weights) per cycle-group at clock frequency `f`, `BW_required(N) = (N/4) × f × 1 byte`. This
  follows directly from the packing density and lane count, and holds on any target.
- **The end-to-end verification chain** (Python → C++ → HLS C-sim → RTL co-sim → board,
  bit-exact at every stage, including the `K` not-a-multiple-of-4 case — Section 8) is a
  methodology, not a hardware artifact, and is directly reusable for an ASIC RTL handoff.
- **Correctness margin**: for the largest supported size (M=512, K=1024), max int8 magnitude,
  `127 × 1024 = 130,048 ≪ 2^31`. Wide int32 accumulator headroom at every currently supported
  size.

### 12.3 Not portable — FPGA/board-specific

| Figure | Value (measured) | Why it doesn't transfer |
|---|---|---|
| Fixed per-call overhead | ≈379 µs (≈253 µs setup, ≈126 µs readback) | Artifact of the AXI-Lite register protocol, Linux, and the `u-dma-buf` driver stack on this PS-PL boundary. An ASIC IP block with a direct on-chip bus interface would have its own, uncharacterized-here, interface latency. |
| DDR4 bandwidth ceiling | ≈17 GB/s (ZCU106-specific) | A board configuration fact (SODIMM part, controller, single channel). Says nothing about an ASIC design's memory bandwidth, which depends entirely on the memory technology chosen (§12.4). |
| 136.99 MHz Fmax | HLS-estimated, FPGA place-and-route dependent | An FPGA fabric routing characteristic. Not indicative of an ASIC clock target, which depends on standard-cell library, process node, and physical design — none evaluated here. |
| 3.419 W on-chip power | Vivado routed estimate (Section 10) | FPGA power model (LUT/FF toggle-rate-based). Not translatable to an ASIC power figure without a standard-cell power analysis. |
| ~310 usable parallel lanes at the LUT limit | Order-of-magnitude estimate (feasibility_analysis.md §4.1), bandwidth-bound in practice at ~50-60 | Specific to this board's DDR4 controller and shared AXI HP ports. An ASIC design's usable lane count is bounded by whatever memory system it is paired with (§12.4). |

### 12.4 Open decisions for a chip-design flow (not answered by this project)

- **Datapath width beyond 4 lanes** — this project verified 4 lanes at zero additional resource
  cost over 1 lane (Section 10), and identified but did not implement a path to 8 lanes,
  contingent on widening the weight-read interface (Week 8 Technical Note §2.3). A chip design
  targeting higher throughput needs to decide the target lane count as a primary sizing decision.
- **Where weights reside during compute** — two real precedents exist in TerEffic's own
  published work (arXiv:2502.16473v2), cited as prior art, not as this project's result: a
  fully-on-chip variant (weights in on-die SRAM, small-model-only, lowest latency) and an
  HBM-assisted variant (off-chip weights, larger models, batch-parallelism needed to hide HBM
  latency). This project used off-chip DDR4 throughout — it demonstrates the ternary MAC
  operator is correct and multiplier-free, not either TerEffic memory pattern.
- **Activation rescaling integration point** — the verified kernel outputs a raw int32
  accumulator with no rescaling applied. A full BitNet/BitLinear layer requires activation
  quantization (absmax-based, per the BitNet 1.58b paper) with a scale factor applied before or
  after this operator; where that step is integrated is deliberately left open here, since it is
  orthogonal to verifying the ternary MAC itself.
- **1.6-bit weight packing** — TerEffic's 5-weights-per-byte encoding (vs. this project's 2
  bits/weight) is denser and was not implemented here; adopting it would require re-deriving,
  not reusing, this project's packing/indexing logic (see §12.5).

### 12.5 A packing detail worth carrying forward: row-alignment constraint

Not a resource or performance number, but a correctness constraint a chip-design implementation
of this same packing scheme should be aware of from the start, since this project discovered it
as a Week 8 regression (Section 7.8, Section 8): if a datapath reads one packed byte (4 weights)
per cycle-group and computes the byte index from a flat, row-independent `(row×K+col)/4`
formula, every row only starts on a byte boundary if `row×K` is a multiple of 4 for every row —
true only when `K` itself is a multiple of 4. A from-scratch ASIC datapath is free to either
enforce a `K % 4 == 0` (or `K % lanes == 0`) precondition at the interface contract level, or
build row-aware indexing in natively — either is simpler than discovering this after the fact.

### 12.6 Summary

What this project hands off is a **verified, multiplier-free ternary MAC operator** — correct
bit-exact through a full Python-to-hardware chain, measured at zero DSP usage and a compact FPGA
footprint (2,973 LUT post-place-and-route for a 4-lane datapath, 1.29% of a 230,400-LUT device —
Section 10) — together with a precise, measured account of which numbers describe the algorithm
(portable, §12.2) and which describe this particular board and toolchain (not portable, §12.3).
It is evidence that the core arithmetic scales without multiplier hardware, at the scale this
project verified it. It is not an ASIC area, power, or timing estimate, and none of the open
decisions in §12.4 are answered here; they are the actual starting points for a chip-design
evaluation.

## 13. Lessons Learned

- **The throughput wall through Week 7 was DDR4 bandwidth and kernel structure, not
  ZCU106 resource capacity.** The IP uses well under 2% of the board's LUTs; Week 8 proved
  that a purely structural change (reading each packed byte once instead of four times)
  recovers a 3.7-3.9× speedup at zero additional resource cost.
- **The device-class gap to TerEffic is architectural, not generational.** Both designs sit on
  the same TSMC 16nm-class process node; the throughput gap is dominated by device size
  (LUT capacity, DDR4 vs. HBM2) rather than by any decision made in this project — verified
  directly against TerEffic's own published tables rather than a secondary summary.
- **Optimizations can introduce regressions that earlier-passing tests would not catch by
  default** — the Week 8 K/4 restructuring silently broke the `K=3` case, which had passed at
  every verification level through Week 7. Re-running the full test suite after every structural
  change, not just a subset, is what caught this. The root cause (a weight-packing byte
  misalignment when `K` isn't 4-aligned, not an activation-buffer overrun) and a host-side fix —
  round `K` up to a multiple of 4 and re-pack both weights and activations at that padded length
  — were identified and implemented without needing to touch the reference model or the RTL,
  which is the more general lesson: a hardware interface constraint discovered late is often
  cheaper to fix at the calling convention than by changing the verified core logic.
- **The T_compute / total-latency distinction matters for honest reporting.** A "3.7× speedup"
  claim is only accurate when scoped to the compute phase; total-latency speedup for small
  matrices is bounded by the fixed ≈379 µs overhead regardless of kernel optimization.
- **Toolchain discipline compounds over a multi-week sprint.** Running Vitis HLS with a conda
  environment active caused failures; cross-compilation plus direct Ethernet transfer reduced
  board iteration time from roughly one hour to about 15 seconds, which materially changed how
  much could be tested per week.
- **Rigor in cross-project comparisons requires reading primary sources directly.** Numbers
  describing another team's published work (TerEffic) are safest when re-verified against the
  paper's own tables rather than propagated through an intermediate summary — an inherited
  imprecision (describing a full accelerator's resource footprint as "one compute core") was
  caught and corrected during this project's own review process.

## 14. Next 3-Month Plan

Immediate (Week 10, within the current sprint):
- Final delivery: cleaned repository, final demo, final report (this document merged with
  Robin's Tenstorrent section).
- **K%4 fix — complete, no longer a Week 10 item.** Verified through the full chain across all
  three code paths that touch weight packing: standalone C++ (10/10, 360/360 stress sweep),
  Vitis HLS C-simulation (10/10), RTL co-simulation (PASS, XSim), and on-board hardware, both
  via `bench_scaling` (M=5/K=7 PASS, transfer MD5-verified, all 8 standard sizes re-confirmed
  with no regression) and via `run_bitlinear_linux.c` (10/10 PASS, including test09_manual
  M=2/K=3, previously the one case this specific binary had not been re-verified against).

Near-term (1-3 months, beyond the current sprint):
- Widen the `MEM_W` AXI interface (e.g., 16-bit) to test whether 8-lane decode holds II=1, as
  identified but not attempted in Week 8.
- Evaluate exploiting the currently-unused PL-side DDR4 channel to relieve the single-channel
  bandwidth ceiling identified in Section 10.
- Prototype on-chip weight buffering (BRAM/URAM) to remove the per-call DDR round-trip for
  workloads with a static weight matrix, informed by TerEffic's own two published
  architectures as precedent (Section 12).
- Investigate batching multiple BitLinear calls per PS-PL invocation to amortize the fixed
  ≈379 µs overhead identified in Section 10.
- Begin a scoped ASIC feasibility exploration using the handoff note (Section 12) as the starting
  specification, engaging a chip-design-capable collaborator for actual standard-cell synthesis
  — explicitly out of reach of this project's own toolchain and timeline.

---

*Companion artifacts referenced in this report:
`bitlinear_fpga_architecture.png` (PS↔PL↔DDR4 diagram with measured latency), `resource_report.md`,
`feasibility_analysis.md`, and the Week 7/Week 8 weekly reports and technical notes, all under the
`erven_1` branch of `robindutois/llmcore-bitnet-accelerator-lab`. (Section 12 contains the full
ASIC handoff analysis directly — no separate handoff-note file is committed, per the CDC's Week
9/10 deliverable list, which names no such standalone file.)*

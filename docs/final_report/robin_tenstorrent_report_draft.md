# BitNet b1.58 on Tenstorrent Blackhole — Final Report

**Project:** EdgeBox-TT — a non-NVIDIA LLM inference accelerator
**Track:** Tenstorrent Blackhole (ternary BitLinear operator)
**Author:** Robin Dutois — Seoul National University / LLM Core AI
**Status:** Core operator implemented, hardware-validated, and benchmarked. Living document — to be updated as work progresses.

---

## 1. Executive summary

This track brings up and benchmarks the **core compute primitive of BitNet** — the ternary
**BitLinear** operator (`out = W_ternary · x_int8 → int32`) — directly on a **Tenstorrent
Blackhole** accelerator card, programmed bare-metal with **tt-metal (TT-Metalium)**.

Two implementations were built and validated on the physical board:

| Implementation | Where the math runs | Correctness | Max size | Throughput (1 core) |
|---|---|---|---|---|
| **Scalar** | one RISC-V (writer kernel) | **bit-exact**, 9/9 reference vectors | 16 384 MAC | ~0.10 GOP/s |
| **Matrix engine** | `matmul_tiles` on the matrix unit | 10/10 vectors, PCC ≈ 1 (bf16) | 8.4 M MAC (512×512) | **~78 GOP/s** |

Moving the compute onto the matrix engine gives **~790× more throughput** and **~720× lower
latency per vector**, and scales to matrices the scalar version cannot run — all on a **single
Tensix core out of ~140**.

What is **not** done yet: a full end-to-end BitNet LLM (attention, all layers, tokenizer, text
generation). This report covers the validated operator and its benchmarks, which are the
foundation the rest builds on.

---

## 2. Background

### 2.1 BitNet b1.58

A standard LLM layer computes `y = W · x` — a matrix–vector (or matrix–matrix) product. The
weights `W` are normally 16-bit floats, so a model is huge and every token costs billions of
**multiplications**.

**BitNet b1.58** constrains every weight to be **ternary**: `{−1, 0, +1}` (≈ 1.58 bits =
`log₂ 3`), and quantizes activations to **int8**. Two consequences:

- **~10× smaller weights** — the model fits in a fraction of the memory.
- **Multiply-free compute** — because `W ∈ {−1, 0, +1}`, `W·x` becomes *add / subtract / skip*.
  No real multipliers needed → lower energy and silicon cost.

The operator that does this, present in **every** linear layer of the transformer (attention
Q/K/V/O + feed-forward), is **BitLinear**:

```
out[m] = Σₖ  W[m][k] · x[k]      W ∈ {−1,0,+1},  x ∈ int8,  out ∈ int32
```

The ternary quantization happens **once, at training time** (quantization-aware training). At
inference the weights are already ternary; on-device we only *decode* them from a packed format
and compute.

### 2.2 Tenstorrent Blackhole

Blackhole is a Tenstorrent AI accelerator card (here on a **PCIe ×4** link). Key structure:

- **~140 Tensix cores.** Each Tensix = **5 small RISC-V cores** (data movement + control) plus a
  **matrix engine** (the "FPU") that operates natively on **32×32 tiles**.
- **Memory hierarchy:** large **DRAM** on the card ↔ small, fast per-core **L1** SRAM. Kernels
  stream tiles from DRAM into L1, compute, and stream results back.
- **tt-metal (TT-Metalium):** the bare-metal SDK. We write three kinds of kernels — *data
  movement* (reader/writer, on the RISC-V cores) and *compute* (drives the matrix engine on the
  3 "TRISC" cores: unpack / math / pack).

The matrix engine is the whole point of the chip: it does 32×32 tile matmuls in hardware,
thousands of multiply-accumulates per cycle.

### 2.3 Ternary weight packing (2-bit)

To exploit BitNet's small weights, ternary values are stored **2 bits each, 4 per byte**:

| Weight | Code |
|---|---|
| 0 | `00` |
| +1 | `01` |
| −1 | `10` |

**Pack:** `byte = c0 | (c1<<2) | (c2<<4) | (c3<<6)`.
**Unpack (on device):** for global index `idx = m*K + k`:
`two_bit = (packed[idx>>2] >> ((idx&3)*2)) & 0x3`, then decode via `{0:0, 1:+1, 2:−1, 3:0}`.

This is ~4× smaller than int8 and ~8× smaller than fp16 — the memory side of BitNet's advantage.

---

## 3. What we built

### 3.1 Pipeline

The computation is a 3-kernel pipeline on a single Tensix core (`{0,0}`):

```
 reader            compute              writer
 DRAM → L1   →   W · x  (MAC)   →   L1 → DRAM
```

Inputs and outputs travel through **circular buffers** in L1: `c_in0` (weights), `c_in1`
(activations), `c_out0` (int32 result).

### 3.2 Scalar implementation (correctness anchor)

- `kernels/reader.cpp` — loads packed weights + int8 activations from DRAM into L1.
- `kernels/writer.cpp` — **does the MAC** in a plain loop (`if w==1 acc+=x; else if w==−1 acc−=x`)
  and writes the int32 result back. (The compute kernel is intentionally empty here — the correct
  L1 address helpers `get_read_ptr`/`get_write_ptr` are only available in the data-movement
  context.)
- `kernels/compute_bitlinear.cpp` — empty.
- `host_metal.cpp` — loads a reference test vector, runs the kernel, compares the output to the
  golden `expected_output_int32.bin`.

This version is **bit-exact** (integer add/sub, no rounding), so it *proves the logic* — decoding,
indexing, the operation — is correct. It is also the performance baseline.

### 3.3 Matrix-engine implementation (performance)

Maps the BitLinear `out[M] = W[M×K]·x[K]` onto a tiled matmul `A[M×K]·B[K×N]` with `N` padded to
32 (a full tile; column 0 carries `x`, or all columns for a batch of activations):

- `kernels_mm/{reader_mm, mm, writer_mm}.cpp` — the **official tt-metal `matmul_single_core`
  kernels, reused unchanged**. `mm.cpp` runs `mm_init` + `matmul_tiles` accumulating over K, then
  `pack_tile`.
- `bitnet_mm.cpp` — host: decodes ternary weights to **bf16**, activations to **bf16**, tilizes
  inputs, runs the matmul, untilizes, compares to the golden.

Ternary weights and int8 activations are both **exact in bf16**; the matrix engine accumulates in
fp32 and the output CB is fp32. Results validate at **PCC ≈ 1** (correlation with the golden). They
are *not* bit-exact because the engine's K-accumulation rounds intermediates in bf16 — the error
grows with the number of accumulated terms (≤ 1 for sparse, up to ~96 at K=512), which is
negligible for BitNet (outputs are re-quantized downstream) and matches how tt-metal's own matmul
example validates (PCC).

> Note: using the matrix engine feeds ternary weights as int8 into the hardware **multipliers** —
> fast, but no longer the "multiply-free" datapath. That pure add/sub advantage is the domain of
> the FPGA/ASIC track.

---

## 4. Results (measured on the board)

### 4.1 Correctness

- **Scalar:** 9/9 core reference vectors **bit-exact** (random, all-zero / +1 / −1 weights, sparse,
  dense, x_max, x_min, manual).
- **Matrix engine:** 10/10 vectors pass at **PCC ≈ 1** (incl. `test10_large`, 256×512), several
  bit-exact.

### 4.2 Scalar vs matrix engine (single core)

| Config (M×K) | MAC | Scalar µs/vec | Scalar GOP/s | Matrix µs/vec | Matrix GOP/s | Speedup |
|---|---|---|---|---|---|---|
| 32×64   | 2 048   | 43.2  | 0.095 | 0.153 | 26.8 | ×283 |
| 64×128  | 8 192   | 166.7 | 0.098 | 0.245 | 66.8 | ×680 |
| 128×128 | 16 384  | 331.9 | 0.099 | 0.459 | 71.4 | ×723 |
| 256×512 | 131 072 | —     | —     | 3.37  | 77.8 | (scalar N/A) |
| 512×512 | 262 144 | —     | —     | 6.70  | 78.2 | (scalar N/A) |

*(GOP/s counts 2 operations per MAC. Matrix engine runs a batch of N=32 activation vectors; µs/vec
is the amortized per-vector latency. DMA/PCIe round-trip is a fixed ~24 µs, separate from compute.)*

**Headline:** ~790× peak throughput, ~720× lower per-vector latency, and scaling to 8.4 M MAC that
the single-tile scalar loader could not handle — on one core out of ~140.

---

## 5. How to run

### 5.1 Prerequisites

- A **Tenstorrent Blackhole** card with driver (KMD) installed, and **tt-metal** built at
  `$TT_METAL_HOME` (with `build_Release/`).
- `tt-smi` available (used to reset the card between runs).
- The reference vectors in `reference/test_vectors/` (part of this repo).

```bash
export TT_METAL_HOME=/home/<user>/tt-metal      # adjust to your install
```

### 5.2 Build

```bash
cd tenstorrent_robin
cmake -B build
cmake --build build          # builds: run_bitnet, run_bench, run_bitnet_mm, run_bench_mm
```

> If the board is in a bad state after a crash/Ctrl-C, reset it first: `tt-smi -r 0`.
> Run all executables **from `tenstorrent_robin/`** so the relative `kernels/…` paths resolve.

### 5.3 Correctness (reference vectors)

```bash
# scalar (bit-exact)
./run_reference_tests.sh
# matrix engine (PCC)
./run_reference_tests_mm.sh
# a single case:
./build/run_bitnet    ../reference/test_vectors/test01_random 32 64
./build/run_bitnet_mm ../reference/test_vectors/test10_large 256 512
```

### 5.4 Benchmarks

```bash
./run_benchmark_hw.sh      # scalar sweep      -> benchmarks/bitlinear_hw_results.csv
./run_benchmark_mm.sh      # matrix sweep      -> benchmarks/bitlinear_mm_results.csv
# single point:  ./build/run_bench_mm <M> <K> <N> <iters>
```

### 5.5 GPU cross-comparison (optional, on the NVIDIA GPU)

```bash
pip install torch nvidia-ml-py      # in your conda env
# on a Slurm server:  srun -p gpu --gres=gpu:1 -t 00:15:00 ./benchmarks/run_gpu_benchmark.sh
./benchmarks/run_gpu_benchmark.sh   # -> benchmarks/gpu_results.csv  (int8 + fp16, perf/watt)
```

---

## 6. Repository layout (Tenstorrent track)

```
tenstorrent_robin/
├── host_metal.cpp            # scalar host (reference-vector test)
├── bitnet_mm.cpp             # matrix-engine host (reference-vector test)
├── bench_metal.cpp           # scalar benchmark
├── bench_mm.cpp              # matrix-engine benchmark
├── bitlinear_cpu.cpp         # CPU golden reference (packed ternary)
├── kernels/                  # scalar: reader, writer (does MAC), compute_bitlinear (empty)
├── kernels_mm/               # matrix engine: reader_mm, mm (matmul_tiles), writer_mm
├── benchmarks/               # gpu_bench.py, run_gpu_benchmark.sh, *_results.csv
├── run_reference_tests*.sh   # validation sweeps (scalar / mm)
├── run_benchmark_*.sh        # benchmark sweeps (scalar / mm)
└── CMakeLists.txt
reference/test_vectors/       # golden vectors (activation, packed weights, expected int32)
```

---

## 7. Current limitations

1. **bf16, not bit-exact (matrix engine).** ~0.1% off the int32 golden; negligible for BitNet, but
   an int8→int32 matmul mode would make it exact.
2. **Single core.** 78 GOP/s uses 1 Tensix of ~140 → the vast majority of the chip is idle.
3. **PCIe ×4 overhead.** A fixed ~24 µs host↔device round-trip; real inference must keep weights
   resident on the card.
4. **Operator, not full model.** The core BitLinear is validated; the end-to-end LLM (attention,
   all layers, tokenizer, autoregressive generation) is not yet running.
5. **Batch-1 padding.** A single activation vector pads N to 32, wasting 31/32 of a tile; real
   batched inference removes this.

---

## 8. Next steps

1. **int8 → int32 matmul** — bit-exact results and likely higher throughput than bf16.
2. **Multi-core scaling** — distribute the output rows across the ~140 Tensix cores
   (start from tt-metal's `matmul_multi_core`); expected order-of-magnitude throughput gain.
3. **Head-to-head vs the NVIDIA GPU** — same operator on the lab's Blackwell GPU, comparing
   throughput, latency, and especially **performance-per-watt** — the core SKU argument.
4. **Full BitNet inference** — wire the operator into a real model: activation quantization,
   RMSNorm, attention, all layers, tokenizer, autoregressive loop; then a REST API + demo.

---

## 9. Optimization & improvement proposals

- **Keep weights resident on the card** across tokens (stream only activations) to amortize the
  fixed PCIe cost — decisive for autoregressive generation.
- **Fuse the dequant/scale step** (int32 → activation scale) into the writer/pack stage instead of
  a separate host pass.
- **Exploit sparsity / zeros** — a large fraction of BitNet weights are 0; skipping zero tiles or
  using structured sparsity can cut work.
- **Overlap data movement and compute** (double/triple-buffered CBs) so the matrix engine never
  stalls waiting on DRAM.
- **Tune tile/block sizes and `MathFidelity`** — trade precision for speed where BitNet tolerates
  it (e.g., LoFi for the multiply since ternary needs few mantissa bits).
- **Batch tokens** (prefill / multi-sequence) to fill N and run the matrix engine at full
  utilization.
- **A native int8 path end-to-end** (int8 activations + int8 ternary weights → int32) to get both
  exactness and the fastest matrix-engine mode.
- Longer term, this operator informs the **FPGA/ASIC track**, where the true multiply-free add/sub
  datapath yields the best performance-per-watt.

---

## 10. References

- BitNet b1.58 — ternary-weight LLMs (Microsoft Research).
- Tenstorrent tt-metal / TT-Metalium documentation and `programming_examples/matmul`.
- Reference vectors and CPU golden: `reference/`, `tenstorrent_robin/bitlinear_cpu.cpp`.

# EdgeBox-TT — Tenstorrent Blackhole track

**Ternary BitLinear (BitNet b1.58) on Tenstorrent Blackhole — hardware bring-up & benchmarks.**
Robin Dutois — Seoul National University / LLM Core AI.

BitNet's core operator `out = W_ternary · x_int8 → int32` runs and is validated on a real
Blackhole card (tt-metal), in two forms: a **scalar** version (bit-exact, 9/9 reference vectors)
and a **matrix-engine** version (`matmul_tiles`, 10/10 at PCC ≈ 1, ~790× faster, scales to
512×512). See **[REPORT.md](REPORT.md)** for the full write-up (background, results, roadmap).

## Quick start

```bash
export TT_METAL_HOME=/home/<user>/tt-metal
cd tenstorrent_robin
cmake -B build && cmake --build build      # run_bitnet, run_bench, run_bitnet_mm, run_bench_mm

./run_reference_tests.sh                    # scalar, bit-exact
./run_reference_tests_mm.sh                 # matrix engine, PCC
./run_benchmark_hw.sh                       # scalar benchmark  -> benchmarks/bitlinear_hw_results.csv
./run_benchmark_mm.sh                       # matrix benchmark  -> benchmarks/bitlinear_mm_results.csv
```

Run executables from `tenstorrent_robin/` (relative kernel paths). If the board hangs after a
crash, reset with `tt-smi -r 0`.

## Layout

| Path | What |
|---|---|
| `kernels/` | scalar: `reader`, `writer` (does the MAC), `compute_bitlinear` (empty) |
| `kernels_mm/` | matrix engine: `reader_mm`, `mm` (`matmul_tiles`), `writer_mm` |
| `host_metal.cpp` / `bitnet_mm.cpp` | hosts (scalar / matrix) — run & check vs reference vectors |
| `bench_metal.cpp` / `bench_mm.cpp` | benchmarks (scalar / matrix) |
| `bitlinear_cpu.cpp` | CPU golden reference |
| `benchmarks/` | GPU comparison (`gpu_bench.py`) + results CSVs |

## Status

Core operator: **validated + benchmarked** on hardware. Full end-to-end LLM inference (attention,
all layers, tokenizer, generation) is future work — see the roadmap in REPORT.md.

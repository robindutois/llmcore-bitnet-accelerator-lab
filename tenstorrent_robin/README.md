# EdgeBox-TT Alpha — Tenstorrent Track

**Tenstorrent Blackhole-based BitNet-oriented LLM inference SKU prototype**
**Student:** Robin Dutois — Seoul National University / LLM Core AI

---

## What this track builds

EdgeBox-TT Alpha is the near-term, SKU-like counterpart to the FPGA track: a working
LLM inference server backed by the same ternary BitLinear operation defined in
`reference/`, running on Tenstorrent Blackhole hardware.

## Pipeline

```
Safetensors model
      |  compiler.py (2-bit packing, reuses reference/packing_utils.py)
      v
Packed weights
      |
      v
TT-Metalium kernel (kernels/reader.cpp, compute_bitlinear.cpp, writer.cpp)
      |  tiled ternary decode + accumulate on Tenstorrent Blackhole
      v
libbitlinear.so   (built by run_week8.sh)
      |  loaded via ctypes
      v
FastAPI inference server (inference_server/api.py)  ->  POST /generate
```

## Directory contents

| Path | Contents |
|---|---|
| `bitlinear_cpu.cpp`, `test_reference.cpp`, `test_reference_integration.cpp` | CPU reference implementation, cross-validated against the shared golden vectors from `reference/test_vectors/` |
| `kernels/` | TT-Metalium device kernels: `reader.cpp` (input load), `compute_bitlinear.cpp` (tiled ternary decode + matmul, face-indexed tile layout), `writer.cpp` (output store) |
| `host_metal.cpp` | Host-side device orchestration (MeshDevice / MeshBuffer / MeshWorkload) |
| `inference_server/` | FastAPI server (`api.py`, `POST /generate`), model compiler (`compiler.py`), mock-model generator (`generate_mock.py`), sample outputs (`sample_outputs.md`) |
| `benchmarks/` | Benchmark harness (`run_benchmark.py`, `benchmark_prompts.json`, `results.csv`) |
| `run_week8.sh` | Builds the TT-Metalium kernel into `libbitlinear.so` and deploys it to the inference server |
| `CMakeLists.txt` | Build configuration (requires `TT_METAL_HOME`) |

## Current status

- CPU reference (`bitlinear_cpu.cpp`) validated against the shared golden vectors.
- FastAPI inference server functional, with a hardware bridge (`ctypes` -> `libbitlinear.so`,
  toggled by `hardware_mode`) and a mock fallback (`generate_mock.py`) for development
  without the device attached.
- TT-Metalium low-level BitLinear kernel implemented (tiled ternary decode on-device);
  on-hardware validation is in progress — the most recent iteration is not yet passing
  end to end.
- `benchmarks/results.csv` currently holds API-level latency data from an earlier
  validation pass; a BitLinear-specific hardware microbenchmark is still to be produced
  once the kernel above is validated on-device.

## Investor message

EdgeBox-TT Alpha will prove that LLM Core AI can build a near-term
non-NVIDIA LLM inference appliance on Tenstorrent Blackhole hardware.

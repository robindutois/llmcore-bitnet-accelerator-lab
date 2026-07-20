# LLM Core AI — BitNet Acceleration Lab

**BitNet-oriented inference acceleration on two hardware platforms.**

This repository demonstrates that LLM Core AI can build both a near-term working LLM
inference appliance and future custom accelerator IP, using the same BitLinear ternary
arithmetic as a common technical foundation.

---

## Project structure

```
llmcore-bitnet-accelerator-lab/
├── reference/                  # Common BitLinear reference (Python + test vectors)
│   ├── bitlinear_reference.py
│   ├── packing_utils.py
│   └── test_vectors/           # 10 binary test sets (shared by both platforms)
│
├── fpga_erven/                 # FPGA track — BitLinear-FPGA Alpha
│   ├── setup/                  # ZCU106 board bring-up documentation
│   ├── hls/                    # Vitis HLS kernel, packing, reference, reports
│   ├── ps_host/                # ARM PS host programs (PetaLinux + standalone) and test-vector generation
│   ├── benchmarks/              # Latency/resource benchmark scripts, CSV results, scaling analysis
│   ├── petalinux/              # PetaLinux app recipe + device-tree overlay for the board's Linux image
│   ├── vitis_project/          # Bitstream, block design script, PS-PL notes
│   └── README.md               # FPGA track quick-start
│
├── tenstorrent_robin/           # Tenstorrent track — EdgeBox-TT Alpha
│   ├── inference_server/        # FastAPI inference server (POST /generate)
│   ├── kernels/                 # TT-Metalium BitLinear kernel (reader / compute / writer)
│   └── README.md                # Tenstorrent track quick-start
│
├── scripts/
│   └── run_fpga_software_tests.sh   # One-command software test runner (no board/Vivado required)
│
└── docs/
    ├── weekly_reports/         # Weekly progress reports (Erven, weeks 1–9; Robin, weeks 1–2)
    ├── final_report/           # Joint final technical report draft
    └── architecture_diagrams/  # PS↔PL↔DDR4 architecture diagram
```

---

## The common operation

Both tracks implement the same BitLinear ternary matrix-vector multiply:

```
y[m] = Σ_k  W[m,k] × x[k]

  x[k]    : int8  activation  [-128 … 127]
  W[m,k]  : ternary weight    {-1, 0, +1}
  y[m]    : int32 accumulator
```

Because W is ternary, multiplication is replaced by:

```
W = +1  →  acc += x[k]
W = -1  →  acc -= x[k]
W =  0  →  (skip — free)
```

This eliminates all multipliers and reduces memory footprint by 4× via 2-bit packing.

---

## Quick start — software tests (no FPGA required)

All software-level tests run with Python 3 and a C++14 compiler. No Vivado or Vitis
installation required.

```bash
# 1. Run from the repo root
bash scripts/run_fpga_software_tests.sh
```

Or step by step:

```bash
# Python reference + test vector generation
python reference/bitlinear_reference.py
python reference/packing_utils.py

# C++ reference cross-validation against Python test vectors
cd fpga_erven/hls/reference
g++ -O2 -std=c++17 -o bitlinear_test \
    bitlinear_reference.cpp bitlinear_reference_test.cpp
./bitlinear_test ../../../reference/test_vectors/

# C++ 2-bit packing tests
cd ../packing/tests
make && make test

# HLS kernel software simulation (g++)
cd ../bitlinear
make && make test
```

Expected output for each step: all tests PASS.

---

## Hardware results (FPGA track — Week 10, final delivery)

| Metric | Value |
|--------|-------|
| Platform | AMD/Xilinx ZCU106 (xczu7ev-ffvc1156-2-e) |
| Tool | Vitis HLS 2025.1 |
| Clock | 10 ns target → 7.30 ns estimated (136.99 MHz) |
| II (inner loop) | 1 (4 weights per cycle, Week 8 4-lane decode) |
| LUT | 4 352 / 230 400 (1.9%) |
| FF | 3 366 / 460 800 (0.7%) |
| BRAM | 8 / 624 (1.3%) |
| **DSP** | **0** — no multipliers, all ternary add/sub |
| Throughput (512×1024, on-board) | 0.176 GOPS total-call — see `feasibility_analysis.md` for the full 8-size sweep (compute-phase speedup is a uniform ×3.5-3.9; total-call speedup is size-dependent and *negative* below ~128×128 due to fixed PS-PL overhead) |
| Correctness | Python → C++ → HLS C-sim → RTL co-sim → board: 10/10, including K%4 fix — verified on physical ZCU106 hardware via both `bench_scaling` (M=5/K=7, MD5-verified transfer) and `run_bitlinear_linux.c` (10/10, incl. test09_manual M=2/K=3); see `fpga_erven/hls/reports/c_sim_result_week8.md` §9 |

### Validation chain

```
Python reference   (Week 2 — 11/11 PASS)
       ↕ bit-exact
C++ reference      (Week 2 — 21/21 PASS)
       ↕ bit-exact
HLS C-Simulation   (Week 3 — 10/10 PASS)
       ↕ bit-exact
HLS Synthesis      (Week 4 — II=1, Fmax 136.99 MHz)
       ↕ bit-exact
RTL Co-Simulation  (Week 5 — 10/10 PASS)
       ↕ bit-exact
ZCU106 board       (Week 8/9 — 8/8 PASS + K%4 fix 10/10 PASS, 0.176 GOPS, 3.7× speedup)
```

---

## Prerequisites

**Software tests only:**
- Python ≥ 3.8 with NumPy
- g++ with C++14 support (C++17 for the reference test)

**Full FPGA flow:**
- Vivado 2025.1
- Vitis HLS 2025.1
- AMD/Xilinx ZCU106 board

---

## Repository status

| Track | Status |
|-------|--------|
| `reference/` — Common BitLinear reference | Complete |
| `fpga_erven/` — FPGA BitLinear-FPGA Alpha | Week 10 — final delivery. All CDC layers/deliverables present; K%4 packing fix verified end-to-end (standalone C++, HLS C-sim, RTL co-sim, physical ZCU106 hardware); final report drafted (`docs/final_report/erven_fpga_report_draft.md`) |
| `tenstorrent_robin/` — Tenstorrent EdgeBox-TT Alpha | CPU reference validated against shared golden vectors; FastAPI inference server and TT-Metalium BitLinear kernel implemented. *Status current as of last sync with the FPGA branch — confirm latest state with Robin before final submission, as `main` has since received further Tenstorrent commits not yet merged into `erven_1`.* |

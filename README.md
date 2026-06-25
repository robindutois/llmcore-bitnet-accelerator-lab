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
│   ├── ps_host/                # ARM PS host program skeleton
│   ├── vitis_project/          # Block design notes
│   └── README.md               # FPGA track quick-start
│
├── tenstorrent_robin/          # Tenstorrent track — EdgeBox-TT Alpha (in progress)
│
└── docs/
    └── weekly_reports/         # Weekly progress reports (Erven, weeks 1–8)
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

## Hardware results (FPGA track — current state: Week 8 of 10)

| Metric | Value |
|--------|-------|
| Platform | AMD/Xilinx ZCU106 (xczu7ev-ffvc1156-2-e) |
| Tool | Vitis HLS 2025.1 |
| Clock | 10 ns target → 7.30 ns estimated (136.99 MHz) |
| II (inner loop) | 1 (1 weight per cycle) |
| LUT | 4 378 / 230 400 (1.9%) |
| FF | 3 465 / 460 800 (0.8%) |
| BRAM | 8 / 624 (1.3%) |
| **DSP** | **0** — no multipliers, all ternary add/sub |
| Latency (M=64, K=128) | ~89.6 µs @ 100 MHz (analytical) |
| Correctness | Python → C++ → HLS C-sim → RTL co-sim: all PASS |

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
       ↕ pending
ZCU106 board       (Week 8 — 8/8 PASS, 0.176 GOPS, 3.7× speedup)
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
| `fpga_erven/` — FPGA BitLinear-FPGA Alpha | Week 8/10 complete |
| `tenstorrent_robin/` — Tenstorrent EdgeBox-TT Alpha | Not included in this archive |

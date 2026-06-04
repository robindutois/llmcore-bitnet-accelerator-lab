# BitLinear-FPGA Alpha — FPGA Track

**ZCU106-based BitNet/TerEffic-style ternary BitLinear accelerator**
**Student:** Erven LE BIVIC — Seoul National University / LLM Core AI
**Current state:** Week 5 of 10 complete

---

## What this does

This track implements and verifies the core BitNet hardware operation on the
AMD/Xilinx ZCU106 FPGA:

```
int8 activation × ternary weight  →  int32 output
```

The Vitis HLS kernel replaces all multiplications with add/subtract/skip operations,
resulting in **0 DSP usage** and a resource footprint below 2% of the ZCU106.

---

## Directory map

```
fpga_erven/
├── setup/
│   ├── board_identification.md       # ZCU106 hardware details (xczu7ev-ffvc1156-2-e)
│   ├── vivado_vitis_setup.md         # Vivado/Vitis 2025.1 installation notes
│   ├── hello_world_ps_pl.md          # PS-PL bring-up: AXI GPIO → LEDs
│   └── hello_world_src/main.c        # Bare-metal LED demo (Xil_Out32)
│
├── hls/
│   ├── reference/
│   │   ├── bitlinear_reference.hpp   # C++ reference declarations
│   │   ├── bitlinear_reference.cpp   # C++ golden reference implementation
│   │   └── bitlinear_reference_test.cpp  # 21-test suite + Python cross-check
│   │
│   ├── packing/
│   │   ├── pack_ternary_2bit.h/cpp   # 2-bit ternary encoder (00=0, 01=+1, 10=-1)
│   │   ├── unpack_ternary_2bit.cpp   # 2-bit ternary decoder
│   │   └── tests/                   # Packing unit tests (10/10 PASS)
│   │
│   ├── bitlinear/
│   │   ├── bitlinear_hls.h           # HLS kernel header (MAX_M=512, MAX_K=1024)
│   │   ├── bitlinear_hls.cpp         # Synthesisable HLS kernel (m_axi, s_axilite)
│   │   ├── testbench.cpp             # Inline C-simulation testbench (10/10 PASS)
│   │   ├── testbench_week2_vectors.cpp  # Cross-validation vs Python .bin files
│   │   ├── testbench_cosim.cpp       # RTL co-simulation testbench (10/10 PASS)
│   │   ├── testbench_cosim_vectors.cpp  # Co-sim testbench loading Week 2 vectors
│   │   ├── run_hls.tcl               # Vitis HLS script (C-sim + synthesis + export)
│   │   └── Makefile                  # g++ build for software-only testing
│   │
│   └── reports/
│       ├── c_ref_results.md          # Python + C++ validation results (Week 2)
│       ├── c_sim_result.md           # HLS C-simulation results (Week 3)
│       ├── correctness_summary.md    # Full validation chain summary
│       ├── synthesis_report.md       # Timing + resources (Week 4)
│       ├── resource_estimate.md      # Per-module resource breakdown (Week 4)
│       ├── latency_estimate.md       # Latency analysis for M=64,K=128 (Week 4)
│       └── rtl_cosim_result.md       # RTL co-simulation results (Week 5)
│
├── ps_host/
│   └── host_skeleton.cpp             # ARM PS host skeleton (Week 6 target)
│
└── vitis_project/
    └── block_design_notes.md         # Planned Vivado block design (Week 6)
```

---

## Running the software tests

### Prerequisites

- Python ≥ 3.8 + NumPy
- g++ with C++14 support (C++17 for the reference test)

### All-in-one script (from repo root)

```bash
bash scripts/run_fpga_software_tests.sh
```

### Step by step (from repo root)

```bash
# 1. Python reference
python reference/bitlinear_reference.py

# 2. Python packing utilities
python reference/packing_utils.py

# 3. C++ reference (must be run from repo root to locate test vectors)
g++ -O2 -std=c++17 \
    fpga_erven/hls/reference/bitlinear_reference.cpp \
    fpga_erven/hls/reference/bitlinear_reference_test.cpp \
    -o /tmp/bitlinear_ref_test
/tmp/bitlinear_ref_test reference/test_vectors/

# 4. C++ packing tests
cd fpga_erven/hls/packing/tests
make && make test
cd ../../../..

# 5. HLS kernel software simulation
cd fpga_erven/hls/bitlinear
make          # builds testbench and testbench_week2_vectors
make test     # runs both; test vectors path is resolved automatically
cd ../../..
```

Expected result for every step: **all tests PASS**.

---

## Running the HLS flow (requires Vitis HLS 2025.1)

```bash
# Source Vitis environment
source /tools/Xilinx/2025.1/Vitis/settings64.sh

# Run C-simulation, synthesis, co-simulation, and IP export
cd fpga_erven/hls/bitlinear
/tools/Xilinx/2025.1/Vitis/bin/vitis-run --mode hls --tcl run_hls.tcl
```

### Known Vitis 2025.1 toolchain issues and fixes

| Issue | Fix |
|-------|-----|
| `vitis_hls` not in PATH | Use `vitis-run --mode hls` instead |
| `RDI_PLATFORM` undefined | `source settings64.sh && export RDI_PLATFORM` |
| `clang-3.9-csynth` missing in `lnx64.o` | Symlink: `lnx64.o/ → lnx64/` |
| `libhlsm_39.bc` not found | Symlink: `lnx64.o/lib → lnx64/lib` |
| GCC toolchain path wrong | Symlink to `lnx64/tools/gcc` |
| `ap_int.h` incompatible with clang-3.1 | Removed from header (unused by kernel) |

---

## Synthesis results summary

| Metric | Value |
|--------|-------|
| Target device | xczu7ev-ffvc1156-2-e |
| Clock target | 10.00 ns |
| Clock estimated | 7.30 ns (Fmax 136.99 MHz) |
| Slack | +2.70 ns |
| LOAD_X II | 1 |
| INNER_LOOP II | 1 |
| LUT | 4 378 / 230 400 (1.9%) |
| FF | 3 465 / 460 800 (0.8%) |
| BRAM_18K | 8 / 624 (1.3%) |
| **DSP** | **0 / 1 728 (0%)** |
| Latency M=64, K=128 | ~8 962 cycles / ~89.6 µs @ 100 MHz |

---

## Validation chain (current state)

| Layer | Result |
|-------|--------|
| Python reference | ✅ Week 2 — 11/11 PASS |
| C++ reference | ✅ Week 2 — 21/21 PASS |
| 2-bit packing (C++) | ✅ Week 2/3 — 10/10 PASS + 1000 random matrices |
| HLS C-Simulation | ✅ Week 3 — 10/10 PASS |
| HLS Synthesis | ✅ Week 4 — II=1, Fmax 136.99 MHz, 0 DRC errors |
| RTL Co-Simulation | ✅ Week 5 — 10/10 PASS (randomised + Week 2 binary vectors) |
| ZCU106 board execution | ⏳ Week 6 — in progress |

---

## Planned PS-PL architecture (Week 6)

```
ARM Cortex-A53 (PS)
      │
      ├─ GP0 (AXI Master) ──→ AXI SmartConnect ──→ s_axi_CTRL    (ap_start, M, K)
      │                                         ──→ s_axi_control  (x, W, y addresses)
      │
      ├─ HP0 (AXI Slave)  ←── m_axi_MEM_IN   (activation read from DDR)
      ├─ HP1 (AXI Slave)  ←── m_axi_MEM_W    (packed weights read from DDR)
      └─ HP2 (AXI Slave)  ←── m_axi_MEM_OUT  (output write to DDR)

                         [ BitLinear HLS IP — PL ]
```

---

## 2-bit ternary weight encoding

| 2-bit code | Ternary value | Action |
|------------|---------------|--------|
| `00` | 0 | skip |
| `01` | +1 | `acc += x[k]` |
| `10` | −1 | `acc -= x[k]` |
| `11` | reserved | treated as 0 |

4 weights per byte, LSB-first layout:
`byte = [ w3 | w2 | w1 | w0 ]`

Memory reduction: 4× vs int8 unpacked representation.

---

## Overflow safety

For the maximum supported size (M=512, K=1024) with maximum int8 activation (+127):

```
127 × 1024 = 130 048  <<  2^31 = 2 147 483 648
```

int32 accumulator is safe for all supported matrix dimensions.

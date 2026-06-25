# BitLinear-FPGA Alpha — FPGA Track

**ZCU106-based BitNet/TerEffic-style ternary BitLinear accelerator**
**Student:** Erven LE BIVIC — Seoul National University / LLM Core AI
**Current state:** Week 8 of 10 complete

---

## What this does

This track implements and verifies the core BitNet hardware operation on the
AMD/Xilinx ZCU106 FPGA:

```
int8 activation × ternary weight  →  int32 output
```

The Vitis HLS kernel replaces all multiplications with add/subtract/skip operations,
resulting in **0 DSP usage** and a resource footprint below 2% of the ZCU106.

The Week 8 kernel processes **4 ternary weights per pipeline cycle** (one packed
byte per iteration), achieving **3.7× throughput improvement** over the Week 7
baseline with zero additional resources.

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
│   │   ├── bitlinear_hls.cpp         # Week 8 — 4-lane parallel kernel (K/4 trips, II=1)
│   │   ├── testbench.cpp             # C-simulation testbench (10 Week 2 test vectors)
│   │   ├── run_hls.tcl               # Vitis HLS script (C-sim + synthesis + export)
│   │   └── Makefile                  # g++ build for software-only testing
│   │
│   └── reports/
│       ├── c_ref_results.md          # Python + C++ validation results (Week 2)
│       ├── c_sim_result.md           # HLS C-simulation results — Week 3 kernel (10/10)
│       ├── c_sim_result_week8.md     # HLS C-simulation results — Week 8 kernel (9/10, K=3 edge case)
│       ├── correctness_summary.md    # Full validation chain summary
│       ├── synthesis_report.md       # Timing + resources (Week 4)
│       ├── resource_estimate.md      # Per-module resource breakdown (Week 4)
│       ├── latency_estimate.md       # Latency analysis for M=64,K=128 (Week 4)
│       └── rtl_cosim_result.md       # RTL co-simulation results (Week 5)
│
├── ps_host/
│   ├── run_bitlinear_linux.c         # PetaLinux host — /dev/mem + udmabuf (Week 7)
│   ├── run_bitlinear_standalone.c    # Standalone bare-metal host (Week 6)
│   └── run_jtag.tcl                  # JTAG validation script (Week 6)
│
├── benchmarks/
│   ├── bench_scaling.c               # Week 8 — timing decomposition benchmark
│   ├── run_benchmark.sh              # Single-command automation script (PetaLinux)
│   ├── run_benchmark.cpp             # CDC entry point (see run_benchmark.sh)
│   ├── results_week7.csv             # Baseline results — sequential kernel (8/8 PASS)
│   ├── results_week8.csv             # Optimized results — 4-lane kernel (8/8 PASS)
│   ├── matrix_scaling_notes.md       # Latency scaling law analysis (Week 7)
│   ├── hls_unroll_exploration.md     # Static unroll trade-off analysis (Week 8)
│   └── resource_report.md            # LUT/FF/BRAM/DSP utilization (Week 8)
│
└── vitis_project/
    ├── bitlinear_w8.bit              # Week 8 bitstream (4-lane kernel, ZCU106)
    ├── create_block_design.tcl       # Vivado block design script
    └── block_design_notes.md         # PS-PL architecture notes
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

# 3. C++ reference
g++ -O2 -std=c++17 \
    fpga_erven/hls/reference/bitlinear_reference.cpp \
    fpga_erven/hls/reference/bitlinear_reference_test.cpp \
    -o /tmp/bitlinear_ref_test
/tmp/bitlinear_ref_test reference/test_vectors/

# 4. C++ packing tests
cd fpga_erven/hls/packing/tests
make && make test
cd ../../../..
```

Expected result for every step: **all tests PASS**.

---

## Running the HLS flow (requires Vitis HLS 2025.1)

```bash
# conda must be deactivated before launching Vitis
conda deactivate

cd fpga_erven/hls/bitlinear
/tools/Xilinx/2025.1/Vitis/bin/vitis-run --mode hls --tcl run_hls.tcl
```

**Note:** `conda deactivate` is mandatory. With conda active, the Vitis Tcl
runtime fails to load `features.tcl` and the session aborts immediately.

---

## Running the benchmark on the ZCU106 board

```bash
# 1. Cross-compile on PC (conda deactivated)
aarch64-linux-gnu-gcc -O2 -static -o bench_scaling fpga_erven/benchmarks/bench_scaling.c

# 2. Transfer to board (PC serves, board fetches)
python3 -m http.server 8080          # on PC
wget http://192.168.50.20:8080/bench_scaling  # on board
wget http://192.168.50.20:8080/bitlinear_w8.bit

# 3. Load bitstream
cp bitlinear_w8.bit /lib/firmware/
echo 0 > /sys/class/fpga_manager/fpga0/flags
echo bitlinear_w8.bit > /sys/class/fpga_manager/fpga0/firmware

# 4. Run benchmark (single command)
modprobe u-dma-buf
chmod +x bench_scaling
./bench_scaling results_week8.csv
```

---

## Synthesis results summary

### Week 8 kernel (4-lane parallel)

| Metric | Value |
|--------|-------|
| Target device | xczu7ev-ffvc1156-2-e |
| Clock target | 10.00 ns (100 MHz) |
| Fmax (estimated) | 136.99 MHz |
| INNER_LOOP II | 1 |
| Loop trips | K/4 (vs K in Week 7) |
| LUT | 4 352 / 230 400 (1.9%) |
| FF | 3 366 / 460 800 (0.7%) |
| BRAM_18K | 8 / 624 (1.3%) |
| **DSP** | **0 / 1 728 (0%)** |
| WNS (post-implementation) | +4.402 ns |

---

## Validation chain (Week 8 state)

| Layer | Result |
|-------|--------|
| Python reference | ✅ Week 2 — 11/11 PASS |
| C++ reference | ✅ Week 2 — 21/21 PASS |
| 2-bit packing (C++) | ✅ Week 2/3 — 10/10 PASS + 1000 random matrices |
| HLS C-Simulation (Week 3 kernel) | ✅ Week 3 — 10/10 PASS |
| HLS Synthesis | ✅ Week 4 — II=1, Fmax 136.99 MHz, 0 DRC errors |
| RTL Co-Simulation | ✅ Week 5 — 10/10 PASS (Week 2 binary vectors) |
| ZCU106 board — PetaLinux PS-PL | ✅ Week 6 — 10/10 PASS, 631 µs avg |
| ZCU106 board — latency scaling | ✅ Week 7 — 8/8 PASS, T∝2·M·K (R²≈1.00) |
| HLS C-Simulation (Week 8 kernel) | ⚠️ Week 8 — 9/10 (K=3 edge case, see c_sim_result_week8.md) |
| ZCU106 board — 4-lane optimized | ✅ Week 8 — 8/8 PASS, 3.7× speedup, 0.176 GOPS |

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

**Week 8 optimization:** the kernel now reads each packed byte exactly once
and decodes all 4 weights in parallel, eliminating redundant AXI reads.

---

## Overflow safety

For the maximum supported size (M=512, K=1024) with maximum int8 activation (+127):

```
127 × 1024 = 130 048  <<  2^31 = 2 147 483 648
```

int32 accumulator is safe for all supported matrix dimensions.

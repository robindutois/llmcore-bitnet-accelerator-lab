# Week 7 Report — Matrix Scaling and Latency Benchmarking
**Student**: Erven LE BIVIC — Seoul National University
**Date**: 2026-06-24
**Project**: BitLinear-FPGA Alpha — LLMCore Accelerator Lab

---

## Objective

Validate that the BitLinear HLS IP runs correctly across larger matrix sizes,
identify the memory and compute bottleneck, and record latency per size. The
three required sizes are M=64/K=128, M=128/K=256, and M=256/K=512. Success
criterion: at least three sizes run correctly and latency is committed to CSV.

---

## Work Completed

### 1. PetaLinux Host Bring-Up

Week 6 ran bare-metal JTAG (no Linux, no per-test latency). Week 7 completed
the PetaLinux bring-up to enable proper end-to-end measurement:

- Booted PetaLinux 2025.1 via JTAG (`petalinux-boot jtag --kernel --fpga
  --bitstream`), kernel 6.12 on Cortex-A53.
- Loaded the `u-dma-buf` module (`modprobe u-dma-buf`), exposing
  `/dev/udmabuf0..2` — the three physically-contiguous DMA regions declared
  in `system-user.dtsi` (x: 1 KB, W: 128 KB, y: 2 KB).
- Set up Ethernet link (static IP, direct PC↔board cable) and transferred
  binaries via netcat. No SD card reader required.
- Confirmed the PL responds to AXI-Lite control: `ap_ctrl = 0x00000004`
  (AP_IDLE bit set) before first run.

### 2. Scaling Benchmark

Wrote `bench_scaling.c`, a standalone C benchmark that:

- Generates random activations and ternary weights in-process (seed fixed to
  42 for reproducibility — no external `.bin` test vector directories).
- Mirrors the exact hardware path of `run_bitlinear_linux.c`: AXI-Lite MMIO
  via `/dev/mem`, DMA via `u-dma-buf`, cache coherency via `sync_for_device`
  / `sync_for_cpu` sysfs.
- Runs 2 untimed warm-up iterations (absorbs first-call overhead) then 11
  timed iterations; reports the median kernel latency (`ap_start` → `ap_done`)
  and the minimum.
- Verifies every size bit-exact against an inline CPU reference.
- Writes results directly to `results_week7.csv`.

Eight sizes were swept, covering a 128× range of problem size (2·M·K).

### 3. Results

All eight sizes executed correctly. FPGA output is bit-exact with the CPU
reference on every size.

| Size (M×K) | Operations (2·M·K) | Median latency (µs) | Throughput (GOPS) | Verify |
|---|---|---|---|---|
| 64×64    | 8 192     | 193    | 0.0424 | PASS |
| **64×128**   | **16 384**    | **361**    | **0.0453** | **PASS** |
| 128×128  | 32 768    | 720    | 0.0455 | PASS |
| **128×256**  | **65 536**    | **1 391**  | **0.0471** | **PASS** |
| 256×256  | 131 072   | 2 779  | 0.0472 | PASS |
| **256×512**  | **262 144**   | **5 468**  | **0.0479** | **PASS** |
| 512×512  | 524 288   | 10 928 | 0.0480 | PASS |
| 512×1024 | 1 048 576 | 21 842 | 0.0480 | PASS |

The three bold rows are the Week 7 required sizes. All run correctly.

### 4. Bottleneck Analysis

**Latency model.** A linear fit over the eight points:

```
latency_µs ≈ 22.5 + 0.0208 × (2·M·K)     (R² ≈ 1.000)
```

The ≈ 22.5 µs constant is fixed per-call PS overhead (register programming,
`ap_start`/`ap_done` handshake, `u-dma-buf` sync, DDR round-trip setup). The
slope is ≈ 20.8 ns per ternary operation.

**Throughput plateau.** GOPS rises from 0.0424 (64×64) and plateaus at
≈ 0.048 (512×1024). The ceiling does not move with problem size: the
bottleneck is **compute-lane width**, not memory capacity or DDR bandwidth.
The current IP processes essentially one ternary operation per clock cycle
(II = 1 inner loop, single lane). Raising the ceiling requires spatial
parallelism (multiple K-lanes per cycle), which is the Week 8 optimization
target.

### 5. Tiling Decision

No tiling was added. At these sizes the `u-dma-buf` buffers (W ≤ 128 KB,
declared in device tree) accommodate the largest swept size (512×1024 =
131 072 packed bytes) in a single pass with no overflow. The Week 7 "add
tiling if needed" task therefore resolves to *not needed*: every required
size runs correctly without it. Tiling is deferred to the Week 8
optimization step, where it pairs with K-lane unrolling as the path to
spatial parallelism.

---

## Deliverables

| File | Description |
|---|---|
| `fpga_erven/benchmarks/bench_scaling.c` | Scaling benchmark (u-dma-buf path) |
| `fpga_erven/benchmarks/results_week7.csv` | Latency and throughput per size |
| `fpga_erven/benchmarks/matrix_scaling_notes.md` | Bottleneck analysis and tiling decision |
| `fpga_erven/benchmarks/resource_report.md` | LUT/FF/BRAM/DSP resource breakdown |
| `docs/weekly_reports/week7_erven.md` | This report |

---

## Challenges and Solutions

**AXI hang on first run.** The benchmark immediately froze the kernel (RCU
stall, CPU blocked on the AXI bus) on the first attempt. Root cause: the
bitstream had not been loaded into the PL before launching the program, so
the AXI transaction at `0xA0000000` went unanswered. Fix: always run
`load-bitstream` (wait for `fpga_manager state = operating`) and
`modprobe u-dma-buf` before the binary. This is now documented as the
mandatory startup sequence.

**Wrong DMA path in first benchmark version.** The first benchmark was
written against `run_bitlinear.cpp` (the bare-metal host, which uses
`posix_memalign` + `/proc/self/pagemap` for physical address translation and
ARM `dc civac` cache instructions). That path does not exist under Linux.
The benchmark was rewritten against `run_bitlinear_linux.c`, which uses the
`u-dma-buf` driver and its sysfs `sync_for_device` / `sync_for_cpu`
interface — the approach already validated in Week 6.

**No g++ on the board.** PetaLinux does not ship with a compiler. The
benchmark was cross-compiled on the host PC with
`aarch64-linux-gnu-gcc -O2 -static` and transferred to the board via
netcat (PC listens, board connects — the BusyBox `nc` on the board supports
connect-only mode, not listen mode).

---

## Next Steps (Week 8)

- Unroll the inner K-loop (factor 4 or 8) to process multiple ternary
  operations per cycle and break the 0.048 GOPS ceiling.
- Re-synthesise and measure the new Fmax vs resource trade-off.
- Compare the new throughput against the Week 7 baseline in a unified table.
- Evaluate the on-chip weight buffering option (BRAM/URAM) to remove the
  per-call DDR round-trip overhead (currently ≈ 22.5 µs fixed cost).

---

## Week 7 Deliverable Checklist

- [x] `fpga_erven/benchmarks/bench_scaling.c`
- [x] `fpga_erven/benchmarks/results_week7.csv`
- [x] `fpga_erven/benchmarks/matrix_scaling_notes.md`
- [x] `fpga_erven/benchmarks/resource_report.md`
- [x] `docs/weekly_reports/week7_erven.md`

**Week 7 status: COMPLETE ✅**

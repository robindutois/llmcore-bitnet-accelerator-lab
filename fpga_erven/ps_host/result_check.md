# Week 6 — ZCU106 PS–PL Execution: Correctness Report

**Project:** BitLinear-FPGA Alpha
**Author:** Erven Le Bivic — Seoul National University — LLM Core AI
**Board:** ZCU106 (Zynq UltraScale+ MPSoC, XCZU7EV)
**Toolchain:** Vivado / Vitis 2025.1, PetaLinux 2025.1

---

## 1. Goal

Run the BitLinear accelerator on the physical ZCU106 board and prove that the
hardware output matches the CPU golden reference across all test vectors.

Target operation:

```
y[m] = Σ_k W[m,k] · x[k]
  x[k]   = int8 activation
  W[m,k] = ternary weight in {-1, 0, +1}, 2-bit packed (4 weights/byte)
  y[m]   = int32 accumulator
```

**Success criterion:** FPGA output == CPU reference output.

---

## 2. System Architecture

```
ARM PS (Cortex-A53, Linux)
   run-bitlinear  (PS host program)
        │   AXI4-Lite control     (s_axi_CTRL    @ 0xA0000000)
        │   buffer base addresses (s_axi_CONTROL @ 0xA0010000)
        ▼
BitLinear HLS IP  (PL)
   2-bit ternary decode → add/sub accumulate → int32 output
        │   AXI4 master
        ▼
DDR4  (udmabuf physically contiguous buffers)
   udmabuf0: x  (int8 activations)
   udmabuf1: W  (2-bit packed ternary weights)
   udmabuf2: y  (int32 output)
```

The accelerator is mapped at base address `0xA0000000`, confirmed present in the
Linux device tree (`bitlinear_hls@a0000000`). Control and buffer-pointer
registers are accessed from user space through `/dev/mem`; DMA buffers use the
`u-dma-buf` kernel module to obtain physically contiguous, address-stable
memory regions whose physical addresses are programmed into the IP.

---

## 3. Execution Method

1. The PL is configured with the BitLinear bitstream at boot.
2. Linux boots on the PS (PetaLinux 2025.1, kernel 6.12).
3. The `u-dma-buf` module is loaded, exposing `/dev/udmabuf0..2`.
4. For each test vector, the host program:
   - copies `x` and packed `W` into the DMA buffers,
   - writes `M`, `K` and the buffer physical addresses to the IP registers,
   - flushes inputs to DDR (`sync_for_device`),
   - asserts `ap_start` and polls `ap_done`,
   - invalidates the output region (`sync_for_cpu`),
   - compares the FPGA output `y` against the golden reference.

---

## 4. Results

All ten required test vectors pass. FPGA output is bit-exact with the CPU
reference, including the large stress case (M=256, K=512).

| Test            | M   | K   | Latency (µs) | Result |
|-----------------|-----|-----|--------------|--------|
| test01_random   | 32  | 64  | 98           | PASS   |
| test02_W_zero   | 32  | 64  | 97           | PASS   |
| test03_W_plus1  | 32  | 64  | 97           | PASS   |
| test04_W_minus1 | 32  | 64  | 98           | PASS   |
| test05_sparse   | 32  | 64  | 97           | PASS   |
| test06_dense    | 32  | 64  | 97           | PASS   |
| test07_xmax     | 32  | 64  | 97           | PASS   |
| test08_xmin     | 32  | 64  | 98           | PASS   |
| test09_manual   | 2   | 3   | 1            | PASS   |
| test10_large    | 256 | 512 | 5470         | PASS   |

```
=== RESULT: 10/10 PASS  total=6256 us  avg=625 us ===
```

**Success criterion met: FPGA output == CPU reference output on all vectors.**

---

## 5. Notes on Latency

The small 32×64 cases sit at a flat ~97–98 µs floor. At this size the compute
is negligible, so the measured time is dominated by fixed per-call overhead
(AXI register setup, IP start, `ap_done` polling, and the Linux `clock_gettime`
syscall). The 2×3 manual case (1 µs) confirms this floor.

The large 256×512 case rises to 5470 µs, where the actual matrix work
(256×512 = 131 072 MAC-equivalent add/sub operations) becomes the dominant
cost. This is the first real point of the latency-vs-size curve and is the
natural starting point for the Week 7 tiling / scaling work.

These figures are end-to-end host-measured latencies under Linux; they include
PS-side overhead and are not the raw IP compute latency reported by synthesis.

---

## 6. Engineering Note: DMA Cache Coherency

A subtle issue was resolved during bring-up. The udmabuf region was initially
opened with `O_SYNC`, which maps it as non-cacheable device memory. On AArch64
this causes `memset()` to fault (SIGBUS) on larger buffers because glibc uses
the `DC ZVA` cache-zeroing instruction, which is illegal on device memory —
hence only the large test (test10) crashed while the small ones passed.

The fix is the standard ZynqMP DMA pattern: map the buffer **cacheable**
(without `O_SYNC`) and manage coherency explicitly:

- `sync_for_device` (cache clean) on `x`/`W` before starting the IP, so the
  accelerator reads fresh data from DDR;
- `sync_for_cpu` (cache invalidate) on `y` after `ap_done`, so the CPU reads
  what the IP actually wrote.

This removed the crash and restored bit-exact results.

---

## 7. Deliverable Files

| File | Description |
|------|-------------|
| `fpga_erven/ps_host/run_bitlinear_linux.c` | Linux PS host (PetaLinux flow) |
| `fpga_erven/ps_host/run_bitlinear_standalone.c` | Bare-metal PS host (XSCT flow) |
| `fpga_erven/ps_host/result_check.md` | This report |
| `fpga_erven/petalinux/recipes-apps/run-bitlinear/` | PetaLinux app recipe |
| `fpga_erven/petalinux/recipes-bsp/device-tree/files/system-user.dtsi` | udmabuf device-tree nodes |

The Vivado/Vitis project build artifacts (`vitis_project/bitlinear_system/`)
are intentionally excluded from version control due to size; the block design
is reproducible from the exported IP and the documented address map above.

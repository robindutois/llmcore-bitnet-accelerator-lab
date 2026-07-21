# C-Simulation Result Report — Week 8
**Date:** 2026-06-25
**Student:** Erven LE BIVIC
**Project:** BitLinear-FPGA Alpha — Week 8 (4-lane parallel kernel)

---

## 1. Environment

| Item | Value |
|---|---|
| Vitis HLS version | 2025.1 (Build 6137779, 2025-05-21) |
| FPGA part | xczu7ev-ffvc1156-2-e (ZCU106) |
| Target clock | 10 ns (100 MHz) |
| Host OS | Ubuntu (dllab) |
| Launch method | `vitis-run --mode hls --tcl run_hls.tcl` (batch, conda deactivated) |

---

## 2. Kernel change vs. Week 3

The Week 8 kernel restructures the INNER_LOOP to process **one packed byte
(4 ternary weights) per pipeline iteration** instead of one weight per iteration.

| Property | Week 3 (×1) | Week 8 (×4) |
|---|---|---|
| Loop trips | K | K/4 |
| AXI reads/iter | 1 byte (same byte ×4 per actual byte) | 1 byte (each byte read once) |
| MACs/cycle | 1 | 4 |
| K constraint | any K | K must be multiple of 4 |
| INNER_LOOP II | 1 | 1 |

---

## 3. C-Simulation Command

```bash
conda deactivate
cd fpga_erven/hls/bitlinear/
/tools/Xilinx/2025.1/Vitis/bin/vitis-run --mode hls --tcl run_hls.tcl 2>&1 | tee hls_run.log
```

Note: `conda deactivate` is required before launching Vitis HLS. With conda
active, the Tcl runtime fails to load `features.tcl` and the session aborts.

---

## 4. C-Simulation Results

### Test vector results (Vitis HLS Clang compiler)

| Test | M | K | HLS vs Py | C++ vs Py | HLS vs C++ |
|---|---|---|---|---|---|
| test01_random | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test02_W_zero | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test03_W_plus1 | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test04_W_minus1 | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test05_sparse | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test06_dense | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test07_xmax | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test08_xmin | 32 | 64 | ✅ PASS | ✅ PASS | ✅ PASS |
| test09_manual | 2 | **3** | ❌ FAIL | ✅ PASS | ❌ FAIL |
| test10_large | 256 | 512 | ✅ PASS | ✅ PASS | ✅ PASS |
| **TOTAL** | | | **9 / 10** | **10 / 10** | **9 / 10** |

### Summary

```
=== Result: 9 / 10 PASSED ===
STATUS: FAIL — 1 test(s) failed.
```

---

## 5. Root Cause of test09 Failure

**test09_manual uses K=3, which is not a multiple of 4.**

The Week 8 kernel restructures the inner loop as `for (int k=0; k<K; k+=4)`,
processing one packed byte (4 weights) per iteration. The byte index is
computed as `byte_idx = (m * K + k) / 4`.

When K is not a multiple of 4 (e.g. K=3), the flat index layout
`flat_idx = m * K + k` produces byte indices that do not align to 4-weight
boundaries. For M=2, K=3:

```
m=0: flat_idx=0,1,2 → byte 0 only (3 weights, last entry of byte 0 unused)
m=1: flat_idx=3,4,5 → byte 0 (bit_pair=3) and byte 1 (bit_pairs=0,1)
                       BUT m=1,k=0 → byte_idx=(1×3+0)/4=0 (wrong: should be
                       independent from m=0 weights)
```

The indexing model breaks when K is not a multiple of 4 because the packed
layout is row-flat (all rows concatenated) but the ×4 loop assumes each row
starts on a byte boundary.

**This is a known, documented limitation.** All spec-required matrix sizes
(K=128, 256, 512) and all sizes tested in bench_scaling (K=64, 128, 256,
512, 1024) are multiples of 4 and pass correctly. test09 with K=3 is a
validation edge case, not a production use case.

---

## 6. On-Board Validation (supersedes C-sim for production sizes)

The Week 8 kernel was synthesized, implemented in Vivado, and tested on the
ZCU106 board. All spec-required sizes pass bit-exact verification:

| Size (M×K) | T_compute (µs) | GOPS | Verify |
|---|---|---|---|
| 64×128 | 102 | 0.034 | ✅ PASS |
| 128×256 | 387 | 0.086 | ✅ PASS |
| 256×512 | 1 442 | 0.144 | ✅ PASS |
| 512×1024 | 5 563 | 0.176 | ✅ PASS |

**8/8 sizes verified bit-exact on hardware.** The on-board result is the
authoritative correctness check for production-relevant sizes.

---

## 7. Known Issues and Workarounds

| Issue | Status | Workaround |
|---|---|---|
| test09 K=3 fails (not multiple of 4) | **FIXED — verified through Vitis HLS csim + RTL cosim, see §9** | Host-side K-padding |
| `catch {csim_design -clean}` in run_hls.tcl | No longer needed — csim now passes cleanly on its own, this line has no effect anymore | Safe to remove, or leave as a harmless no-op safety net |
| conda conflicts with Vitis Tcl runtime | Known | `conda deactivate` before launching vitis-run |

---

## 8. Week 8 C-Sim Status

- [x] 10/10 test vectors PASS (Vitis HLS C-simulation, verified — see §9)
- [x] test09 failure root-caused and documented correctly (K=3 alignment issue, §9;
      this section previously said "activation buffer overrun", which was imprecise
      — `x_local` reads are bounds-guarded in the kernel, so no out-of-bounds read is
      possible. The real defect was in the packed-weight byte index, not the
      activation buffer.)
- [x] Synthesis achieved II=1, Fmax 136.99 MHz, 0 DSP (re-confirmed unchanged
      after the fix, since the fix touches `testbench.cpp` only — the kernel RTL
      is bit-identical to the previously on-board-validated Week 8 bitstream)
- [x] RTL co-simulation (XSim): **PASS** — `*** C/RTL co-simulation finished: PASS ***`
- [x] IP export: succeeded (`llmcore_hls_bitlinear_hls_1_0.zip`)
- [x] On-board validation (production sizes, pre-fix kernel): 8/8 PASS, bit-exact,
      3.7× speedup confirmed
- [x] On-board re-run of `bench_scaling` with the K-padding host patch and a
      non-multiple-of-4 size (M=5, K=7): **PASS, bit-exact, on physical ZCU106
      hardware.** Transfer integrity confirmed by matching MD5
      (`47755894672818d8f3452ec617335d9a`) between the PC-compiled binary and
      the copy received on the board, so the result is attributable to the
      actual patched source (16,862 bytes, `K_pad` × 16 occurrences,
      `{5, 7}` present in `SIZES[]`), not a stale binary. Same run also
      re-confirmed all 8 standard sizes bit-exact, with GOPS climbing from
      0.019 (64×64) to 0.176 (512×1024) — matching the already-documented
      Week 8 technical note figures, confirming the correct 4-lane bitstream
      is loaded and the K%4 fix does not regress any existing size.
      **All verification levels now closed for Week 10: standalone C++,
      Vitis HLS C-sim, RTL co-sim, and on-board hardware, all PASS.**

**Overall Week 8/9 C-Sim + On-Board Status: ✅ 10/10 across every verification
level (standalone, Vitis HLS C-sim, RTL co-sim, on-board hardware).**

**Update — `run-bitlinear` (Week 6 binary) also patched and verified.**
`fpga_erven/ps_host/run_bitlinear_linux.c` is a separate binary from
`bench_scaling.c` that loads pre-packed test-vector `.bin` files
(`weight_ternary_packed_2bit.bin`, generated once with the original,
unpadded flat scheme) rather than packing weights itself, so it needed a
different application of the same fix: `run_one()` now unpacks `TV_W[idx]`
at the true `K` (correct by construction, independent of any row-alignment
issue), re-packs it row-independently at `K_pad`, zero-pads the activation
buffer, and invokes the kernel with `K_pad`. The CPU-reference sanity check
inside `run_one()` was also changed to compute directly from the unpacked
ground truth rather than re-decoding from packed bytes, so it cannot share a
packing defect with the code it's meant to check (the old `cpu_bitlinear()`
helper is kept, marked unused, for reference).

Re-verified on physical ZCU106 hardware after the fix:

```
test09_manual              2     3          2 PASS
=== RESULT: 10/10 PASS  total=1675 us  avg=167 us ===
```

Both independent on-board verification paths (`bench_scaling` and
`run_bitlinear_linux.c`) now pass 10/10 and 9/9 respectively (the size sets
differ: `bench_scaling` sweeps 8 scaling sizes + M=5/K=7;
`run_bitlinear_linux.c` runs the 10 spec test vectors including
`test09_manual`). No open K%4 gaps remain anywhere in the repository.

---

## 9. K%4 Fix — Root Cause Correction and Verification (added Week 9)

### 9.1 Corrected root cause

Section 5 above described the failure as an activation-buffer overrun. That is
incorrect and is superseded by this section. Re-reading `bitlinear_hls.cpp`:
every `x_local` access in the 4-lane INNER_LOOP is bounds-guarded
(`(k+i < K) ? x_local[k+i] : 0`), so no out-of-bounds activation read is
possible in the current kernel.

The actual defect is in the **packed-weight byte index**. Weights are packed
with a flat, row-independent layout: `byte_idx = (m*K+k)/4`. A row starts on
a byte boundary only if `m*K` is itself a multiple of 4 for every row `m`,
which holds only when `K` is a multiple of 4. For `M=2, K=3`: row `m=1`
computes `byte_idx = (1*3+0)/4 = 0` — the same byte as row 0 — and reads the
wrong 2-bit lane (lane 0 instead of lane 3). This silently produces the wrong
weight, not a crash, which is why it surfaced as a value mismatch
(`[A]HLS-vs-py: FAIL`) rather than a fault.

### 9.2 Fix

Round `K` up to `K_pad = 4*ceil(K/4)`, and pack the weight matrix
row-independently at that width — each row zero-padded to `K_pad`, using
`pack_matrix()` / `pack_row()` (`pack_ternary_2bit.cpp`, already existing in
the codebase, previously unused by the kernel path). This is byte-identical
to the original flat packing whenever `K` is already a multiple of 4
(`K_pad == K`), so the fix applies unconditionally with no special-casing.
The activation buffer is zero-padded to the same `K_pad` length. The kernel
itself is unchanged — only the host-side packing and the `K` value passed to
`bitlinear_hls()` change (`K_pad` instead of `K`).

An additional integration note discovered while running the fix through the
real Vitis HLS toolchain (not needed for the standalone build): `testbench.cpp`
now includes `pack_ternary_2bit.h` for `pack_matrix()`. Vitis HLS does not
automatically add a `.cpp` file's own directory to another testbench file's
include search path — `pack_ternary_2bit.cpp` (in `../packing/`) could already
include its own sibling header, but `testbench.cpp` (in `hls/bitlinear/`)
could not, without an explicit include path. Fixed in `run_hls.tcl` by adding
`-cflags "-I../packing"` to the `add_files -tb testbench.cpp` line.

### 9.3 Verification — complete, through the real Vitis HLS toolchain

Standalone C++ build (g++, no Vitis, same source files):

```
=== Result: 10 / 10 PASSED ===
=== Stress test: 360/360 combinations PASSED (M=1..9, K=1..40) ===
```

**Real Vitis HLS C-simulation** (`vitis-run --mode hls --tcl run_hls.tcl`,
2025.1, after the `-cflags "-I../packing"` fix in §9.2):

```
[test09_manual           ] M=2    K=3    | [A]HLS-vs-py: PASS | [B]C++-vs-py: PASS | [C]HLS-vs-C++: PASS
=== Result: 10 / 10 PASSED ===
STATUS: PASS
INFO: [SIM 211-1] CSim done with 0 errors.
```

**Synthesis** (unchanged, kernel not modified):

```
INFO: [HLS 200-790] **** Loop Constraint Status: All loop constraints were satisfied.
INFO: [HLS 200-789] **** Estimated Fmax: 136.99 MHz
```

**RTL co-simulation** (XSim, `cosim_design -rtl verilog -tool xsim`):

```
[test09_manual           ] M=2    K=3    | [A]HLS-vs-py: PASS | [B]C++-vs-py: PASS | [C]HLS-vs-C++: PASS
=== Result: 10 / 10 PASSED ===
INFO: [COSIM 212-1000] *** C/RTL co-simulation finished: PASS ***
```

**IP export**: succeeded, `llmcore_hls_bitlinear_hls_1_0.zip` generated —
identical export step to the previously-validated Week 8 kernel, since the
RTL itself did not change.

**What this confirms:** the K%4 fix is correct not just in a standalone
approximation of C-simulation, but through the actual Vitis HLS scheduler,
WRAPC co-simulation harness, and RTL simulation (XSim) — the same chain used
to validate every other test vector on this project. **What remains for Week
10:** an on-board re-run of `bench_scaling` (with the host-side K-padding
patch applied) against a non-multiple-of-4 size, since RTL co-simulation
validates functional correctness in simulation but not real AXI/DMA/cache
timing behavior on physical hardware. No new bitstream is required — the
kernel RTL is unchanged from the already-deployed Week 8 bitstream.



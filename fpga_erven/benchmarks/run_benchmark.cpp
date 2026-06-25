// =============================================================================
// run_benchmark.cpp — Week 8
// BitLinear-FPGA Alpha — Benchmark automation entry point
// =============================================================================
//
// This file is the CDC-required C++ entry point for the benchmark automation.
// The actual implementation is in run_benchmark.sh, which orchestrates the
// complete pipeline on the ZCU106 board (PetaLinux environment):
//
//   1. Verify fpga_manager state = operating
//   2. Load u-dma-buf driver
//   3. Run bench_scaling binary (compiled from bench_scaling.c)
//   4. Wrap results into a timestamped CSV
//
// Rationale for shell implementation:
//   The benchmark runs on PetaLinux (BusyBox environment). A shell script
//   is the appropriate tool for orchestrating system-level checks (sysfs,
//   modprobe, device nodes) before invoking the compiled C benchmark binary.
//   A C++ wrapper would require POSIX system() calls for the same operations,
//   adding complexity without benefit.
//
// Usage on the ZCU106 board:
//   sudo ./run_benchmark.sh ./bench_scaling results_week8.csv
//
// Cross-compile bench_scaling (on PC, conda deactivated):
//   aarch64-linux-gnu-gcc -O2 -static -o bench_scaling bench_scaling.c
// =============================================================================

// See run_benchmark.sh for the complete implementation.

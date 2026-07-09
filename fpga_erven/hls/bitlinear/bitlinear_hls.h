#ifndef BITLINEAR_HLS_H
#define BITLINEAR_HLS_H
#include <stdint.h>
#define MAX_M 512
#define MAX_K 1024
#define MAX_PACKED_K (MAX_K / 4)

/*
 * Kernel interface contract (Week 9, see hls/reports/c_sim_result_week8.md
 * §9 and docs/final_report/erven_fpga_report_draft.md §7.8 for the full
 * root-cause writeup):
 *
 *   - K passed to bitlinear_hls() MUST be a multiple of 4 (K_pad, not the
 *     true/logical K if the true K isn't already a multiple of 4).
 *   - W_packed MUST be packed row-independently at that K_pad width (each
 *     row zero-padded to K_pad, e.g. via pack_matrix()/pack_row() in
 *     pack_ternary_2bit.cpp), NOT flat-packed at the true K.
 *   - x[K_true .. K_pad-1] MUST be zero-padded.
 *
 * The kernel itself does not enforce or check this — callers are
 * responsible for the K_pad transformation. testbench.cpp, bench_scaling.c,
 * and run_bitlinear_linux.c all implement this transformation; a caller
 * that instead uses the raw, flat-packed weight_ternary_packed_2bit.bin
 * test-vector files directly with the true (non-padded) K will get wrong
 * results for any K not already a multiple of 4 (e.g. test09_manual, K=3).
 */
void bitlinear_hls(const int8_t* x, const uint8_t* W_packed, int32_t* y, int M, int K);
#endif


// =============================================================================
// bitlinear_hls.cpp — Week 8 optimization
// HLS BitLinear Kernel — 4-lane parallel ternary MAC
// =============================================================================
//
// Key change vs Week 7:
//   The inner loop now processes ONE packed byte per pipeline iteration instead
//   of one weight per iteration. Each byte encodes 4 ternary weights, so all 4
//   are decoded and accumulated in parallel within a single II=1 cycle.
//
//   This gives 4× throughput with IDENTICAL DDR bandwidth:
//     - Before: K iterations × 1 AXI read (same byte read 4× per byte)
//     - After : K/4 iterations × 1 AXI read (each byte read exactly once)
//
// Constraint: K must be a multiple of 4.
//   All CDC-required sizes (K=128, 256, 512) satisfy this.
//
// Weight encoding (2-bit packed, FLAT layout — unchanged from Week 2):
//   00 = 0   01 = +1   10 = -1   11 = reserved (treated as 0)
//   flat_index = m * K + k
//   byte_index = flat_index / 4  (4 weights per byte)
// =============================================================================

#include "bitlinear_hls.h"

// =============================================================================
// bitlinear_hls — top-level synthesizable function
// =============================================================================
void bitlinear_hls(
    const int8_t*  x,
    const uint8_t* W_packed,
    int32_t*       y,
    int            M,
    int            K
) {
    // --- HLS Interface Pragmas (unchanged from Week 7) ---
    #pragma HLS INTERFACE m_axi port=x        depth=1024   offset=slave bundle=MEM_IN
    #pragma HLS INTERFACE m_axi port=W_packed depth=131072 offset=slave bundle=MEM_W
    #pragma HLS INTERFACE m_axi port=y        depth=512    offset=slave bundle=MEM_OUT
    #pragma HLS INTERFACE s_axilite port=M      bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=K      bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL

    // Local activation buffer — partitioned cyclic×4 to allow 4 simultaneous
    // reads per cycle (one per parallel lane). Factor matches the unroll width.
    int8_t x_local[MAX_K];
    #pragma HLS ARRAY_PARTITION variable=x_local cyclic factor=4 dim=1

    // Load activations from DDR into local BRAM (unchanged)
    LOAD_X:
    for (int k = 0; k < K; ++k) {
        #pragma HLS PIPELINE II=1
        x_local[k] = x[k];
    }

    // Outer loop: one output row per iteration
    OUTER_LOOP:
    for (int m = 0; m < M; ++m) {
        int32_t acc = 0;

        // Inner loop: one packed byte = 4 weights per cycle
        // Loop trips K/4 times (vs K times in Week 7) → 4× fewer iterations
        // Note: all CDC-required sizes have K multiple of 4 (128, 256, 512).
        // For non-multiple-of-4 K (e.g. test09 K=3), lanes beyond K are masked.
        INNER_LOOP:
        for (int k = 0; k < K; k += 4) {
            #pragma HLS PIPELINE II=1

            // One AXI read fetches all 4 weights for positions k, k+1, k+2, k+3
            int byte_idx = (m * K + k) >> 2;   // divide by 4
            uint8_t packed = W_packed[byte_idx];

            // --- 4 parallel decode-and-accumulate lanes ---
            // Each lane is independent → HLS infers parallel logic
            // Bounds guard: mask lanes that fall outside [0, K)

            // Lane 0: bits [1:0]
            uint8_t c0 = (packed >> 0) & 0x3u;
            int32_t x0 = (k + 0 < K) ? (int32_t)x_local[k + 0] : 0;
            int32_t p0 = (c0 == 0x1u) ? x0 : (c0 == 0x2u) ? -x0 : 0;

            // Lane 1: bits [3:2]
            uint8_t c1 = (packed >> 2) & 0x3u;
            int32_t x1 = (k + 1 < K) ? (int32_t)x_local[k + 1] : 0;
            int32_t p1 = (c1 == 0x1u) ? x1 : (c1 == 0x2u) ? -x1 : 0;

            // Lane 2: bits [5:4]
            uint8_t c2 = (packed >> 4) & 0x3u;
            int32_t x2 = (k + 2 < K) ? (int32_t)x_local[k + 2] : 0;
            int32_t p2 = (c2 == 0x1u) ? x2 : (c2 == 0x2u) ? -x2 : 0;

            // Lane 3: bits [7:6]
            uint8_t c3 = (packed >> 6) & 0x3u;
            int32_t x3 = (k + 3 < K) ? (int32_t)x_local[k + 3] : 0;
            int32_t p3 = (c3 == 0x1u) ? x3 : (c3 == 0x2u) ? -x3 : 0;

            // Adder tree: 4 partials → 1 accumulation per cycle
            acc += p0 + p1 + p2 + p3;
        }

        y[m] = acc;
    }
}

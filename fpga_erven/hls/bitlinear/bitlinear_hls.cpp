// =============================================================================
// bitlinear_hls.cpp
// HLS BitLinear Kernel — BitLinear-FPGA Alpha
// =============================================================================
// Operation:
//   y[m] = sum_k  W[m,k] * x[k]
//   where x[k] is int8 and W[m,k] is ternary in {-1, 0, +1}
//
// Weight encoding (2-bit packed, FLAT layout — matches Week 2 format):
//   The full M*K weight matrix is packed as a single flat array.
//   Total packed bytes = ceil(M*K / 4)
//   Weight W[m,k] is at flat index (m*K + k).
//
//   00 = 0   01 = +1   10 = -1   11 = reserved (treated as 0)
//
// This matches the packing_utils.py and bitlinear_reference.cpp from Week 2.
// =============================================================================

#include "bitlinear_hls.h"

// =============================================================================
// decode_weight_hls — inline 2-bit decoder
// =============================================================================
static inline int8_t decode_weight_hls(uint8_t code) {
    #pragma HLS INLINE
    if (code == 0b01) return  1;
    if (code == 0b10) return -1;
    return 0;
}

// =============================================================================
// bitlinear_hls — top-level synthesizable function
//
// W_packed layout: FLAT — the entire M*K matrix packed as one array.
//   flat_index = m * K + k
//   byte_index = flat_index / 4
//   bit_pair   = flat_index % 4
// =============================================================================
void bitlinear_hls(
    const int8_t*  x,
    const uint8_t* W_packed,
    int32_t*       y,
    int            M,
    int            K
) {
    // --- HLS Interface Pragmas ---
    #pragma HLS INTERFACE m_axi port=x        depth=1024  offset=slave bundle=MEM_IN
    #pragma HLS INTERFACE m_axi port=W_packed depth=131072 offset=slave bundle=MEM_W
    #pragma HLS INTERFACE m_axi port=y        depth=512   offset=slave bundle=MEM_OUT
    #pragma HLS INTERFACE s_axilite port=M      bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=K      bundle=CTRL
    #pragma HLS INTERFACE s_axilite port=return bundle=CTRL

    // Local buffer for activation vector (avoids repeated DDR reads)
    int8_t x_local[MAX_K];
    #pragma HLS ARRAY_PARTITION variable=x_local cyclic factor=4 dim=1

    // Load activation into local buffer
    LOAD_X:
    for (int k = 0; k < K; ++k) {
        #pragma HLS PIPELINE II=1
        x_local[k] = x[k];
    }

    // Outer loop: one output element per row m
    OUTER_LOOP:
    for (int m = 0; m < M; ++m) {
        int32_t acc = 0;

        // Inner loop: iterate over K weights using FLAT index
        INNER_LOOP:
        for (int k = 0; k < K; ++k) {
            #pragma HLS PIPELINE II=1

            // Flat index of W[m,k]
            int flat_idx = m * K + k;
            int byte_idx = flat_idx / 4;
            int bit_pair = flat_idx % 4;

            // Extract 2-bit code
            uint8_t code = (W_packed[byte_idx] >> (bit_pair * 2)) & 0b11;

            // Decode and accumulate
            int8_t w = decode_weight_hls(code);
            if (w ==  1) acc += (int32_t)x_local[k];
            if (w == -1) acc -= (int32_t)x_local[k];
        }

        y[m] = acc;
    }
}

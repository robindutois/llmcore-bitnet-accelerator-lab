// =============================================================================
// unpack_ternary_2bit.cpp
// 2-bit Ternary Weight Unpacking — BitLinear-FPGA Alpha
// =============================================================================
// Decoding:
//   00 -> 0
//   01 -> +1
//   10 -> -1
//   11 -> 0  (reserved, treated as zero)
// =============================================================================

#include "pack_ternary_2bit.h"

// -----------------------------------------------------------------------------
// decode_weight: decode a 2-bit code into a ternary weight
// Input:  2-bit code (only bits [1:0] are read)
// Output: ternary weight in {-1, 0, +1}
// -----------------------------------------------------------------------------
int8_t decode_weight(uint8_t code) {
    code &= 0b11; // mask to 2 bits only
    if (code == 0b00) return  0;
    if (code == 0b01) return  1;
    if (code == 0b10) return -1;
    return 0; // 0b11 reserved -> treat as zero
}

// -----------------------------------------------------------------------------
// unpack_row: unpack ceil(K/4) bytes into K ternary weights
// Input:
//   w_packed : packed byte array of size ceil(K/4)
//   K        : number of weights to recover
// Output:
//   w_unpacked : pointer to output buffer of K int8_t weights
// -----------------------------------------------------------------------------
void unpack_row(const uint8_t* w_packed, int8_t* w_unpacked, int K) {
    for (int k = 0; k < K; ++k) {
        int byte_idx = k / 4;
        int bit_pair = k % 4;

        // Extract the 2-bit code for weight k
        uint8_t code = (w_packed[byte_idx] >> (bit_pair * 2)) & 0b11;

        w_unpacked[k] = decode_weight(code);
    }
}

// -----------------------------------------------------------------------------
// unpack_matrix: unpack a full packed M x ceil(K/4) matrix
// Input:
//   W_packed : M x ceil(K/4) packed matrix (uint8_t)
//   M, K     : original matrix dimensions
// Output:
//   W_unpacked : M x K recovered ternary weights (int8_t)
// -----------------------------------------------------------------------------
void unpack_matrix(const uint8_t* W_packed, int8_t* W_unpacked, int M, int K) {
    int packed_K = (K + 3) / 4;

    for (int m = 0; m < M; ++m) {
        unpack_row(
            W_packed   + m * packed_K,
            W_unpacked + m * K,
            K
        );
    }
}

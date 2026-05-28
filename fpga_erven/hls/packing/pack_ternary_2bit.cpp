// =============================================================================
// pack_ternary_2bit.cpp
// 2-bit Ternary Weight Packing — BitLinear-FPGA Alpha
// =============================================================================
// Encoding:
//   00 = 0
//   01 = +1
//   10 = -1
//   11 = reserved (invalid)
//
// 4 weights are packed into 1 byte (uint8_t).
// Bit layout inside one byte:
//   [w3 w3 | w2 w2 | w1 w1 | w0 w0]
//   MSB                           LSB
// =============================================================================

#include "pack_ternary_2bit.h"
#include <stdexcept>

// -----------------------------------------------------------------------------
// encode_weight: encode a single ternary weight into its 2-bit code
// Input:  w in {-1, 0, +1}
// Output: 2-bit code as uint8_t (only bits [1:0] are used)
// -----------------------------------------------------------------------------
uint8_t encode_weight(int8_t w) {
    if (w == 0)  return 0b00;
    if (w == 1)  return 0b01;
    if (w == -1) return 0b10;
    return 0b11; // invalid/reserved
}

// -----------------------------------------------------------------------------
// pack_row: pack K ternary weights (int8_t) into ceil(K/4) bytes (uint8_t)
// Input:
//   w_unpacked : pointer to K ternary weights in {-1, 0, +1}
//   K          : number of weights
// Output:
//   w_packed   : pointer to output buffer of size ceil(K/4)
// -----------------------------------------------------------------------------
void pack_row(const int8_t* w_unpacked, uint8_t* w_packed, int K) {
    int num_bytes = (K + 3) / 4; // ceil(K/4)

    for (int byte_idx = 0; byte_idx < num_bytes; ++byte_idx) {
        uint8_t packed_byte = 0;

        for (int bit_pair = 0; bit_pair < 4; ++bit_pair) {
            int k = byte_idx * 4 + bit_pair;
            uint8_t code = 0b00; // default: 0

            if (k < K) {
                code = encode_weight(w_unpacked[k]);
            }

            // Place the 2-bit code at the correct position in the byte
            // w0 at bits [1:0], w1 at bits [3:2], w2 at bits [5:4], w3 at bits [7:6]
            packed_byte |= (code << (bit_pair * 2));
        }

        w_packed[byte_idx] = packed_byte;
    }
}

// -----------------------------------------------------------------------------
// pack_matrix: pack a full M x K ternary weight matrix
// Input:
//   W_unpacked : M x K matrix of ternary weights (row-major, int8_t)
//   M, K       : matrix dimensions
// Output:
//   W_packed   : M x ceil(K/4) packed matrix (uint8_t)
// -----------------------------------------------------------------------------
void pack_matrix(const int8_t* W_unpacked, uint8_t* W_packed, int M, int K) {
    int packed_K = (K + 3) / 4; // number of bytes per row

    for (int m = 0; m < M; ++m) {
        pack_row(
            W_unpacked + m * K,       // pointer to row m of unpacked matrix
            W_packed   + m * packed_K, // pointer to row m of packed matrix
            K
        );
    }
}

// -----------------------------------------------------------------------------
// pack_flat: pack the full M*K matrix as one flat array (Week 2 format)
// This matches the format used in reference/packing_utils.py
// -----------------------------------------------------------------------------
void pack_flat(const int8_t* W_unpacked, uint8_t* W_packed, int M, int K) {
    pack_row(W_unpacked, W_packed, M * K);
}

void unpack_flat(const uint8_t* W_packed, int8_t* W_unpacked, int M, int K) {
    unpack_row(W_packed, W_unpacked, M * K);
}

// =============================================================================
// pack_ternary_2bit.h
// Header for 2-bit ternary packing/unpacking — BitLinear-FPGA Alpha
// =============================================================================

#ifndef PACK_TERNARY_2BIT_H
#define PACK_TERNARY_2BIT_H

#include <cstdint>

// Single weight encode/decode
uint8_t encode_weight(int8_t w);
int8_t  decode_weight(uint8_t code);

// Row-level pack/unpack
void pack_row  (const int8_t*  w_unpacked, uint8_t* w_packed,   int K);
void unpack_row(const uint8_t* w_packed,   int8_t*  w_unpacked, int K);

// Matrix-level pack/unpack
void pack_matrix  (const int8_t*  W_unpacked, uint8_t* W_packed,   int M, int K);
void unpack_matrix(const uint8_t* W_packed,   int8_t*  W_unpacked, int M, int K);

#endif // PACK_TERNARY_2BIT_H

// Flat packing: pack the full M*K matrix as a single 1D array (Week 2 format)
// Output size: ceil(M*K / 4) bytes
void pack_flat(const int8_t* W_unpacked, uint8_t* W_packed, int M, int K);
void unpack_flat(const uint8_t* W_packed, int8_t* W_unpacked, int M, int K);

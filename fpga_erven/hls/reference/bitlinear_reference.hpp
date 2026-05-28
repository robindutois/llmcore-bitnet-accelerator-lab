#pragma once
/**
 * bitlinear_reference.hpp
 * =======================
 * Project : BitLinear-FPGA Alpha (Erven LE BIVIC)
 * Week    : 2
 * Purpose : Header for the CPU golden reference.
 *           All HLS/FPGA results must match these functions exactly.
 */

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// ---------------------------------------------------------------------------
// Core reference function
// ---------------------------------------------------------------------------

/**
 * BitLinear golden reference.
 *
 *   y[m] = sum_k  W[m*K + k] * x[k]
 *
 *   x      : int8  activation vector,  length K
 *   W      : int8  ternary weight matrix, row-major, shape [M][K], values in {-1, 0, +1}
 *   y      : int32 output accumulator, length M
 */
void bitlinear_reference(
    const int8_t*  x,
    const int8_t*  W,
    int32_t*       y,
    int            M,
    int            K
);

// ---------------------------------------------------------------------------
// 2-bit ternary packing helpers
// ---------------------------------------------------------------------------
// Encoding:  00 = 0   01 = +1   10 = -1   11 = reserved (treat as 0)
// 4 weights per byte, LSB-first: byte = [w3|w2|w1|w0]

uint8_t  encode_weight(int8_t w);
int8_t   decode_weight(uint8_t code);

/**
 * Pack a flat int8 ternary array (length n) into ceil(n/4) uint8 bytes.
 * If n is not a multiple of 4, zero-padding is added.
 */
std::vector<uint8_t> pack_ternary_2bit(const int8_t* W_flat, int n);

/**
 * Unpack a uint8 packed array back to int8 ternary (returns exactly n_weights values).
 */
std::vector<int8_t>  unpack_ternary_2bit(const uint8_t* packed, int n_weights);

// ---------------------------------------------------------------------------
// Binary I/O helpers (shared test vector format)
// ---------------------------------------------------------------------------

std::vector<int8_t>   load_int8_bin (const std::string& path);
std::vector<int32_t>  load_int32_bin(const std::string& path);
bool                  save_int32_bin(const std::string& path,
                                     const int32_t* data, int n);

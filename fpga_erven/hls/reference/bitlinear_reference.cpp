/**
 * bitlinear_reference.cpp
 * =======================
 * Project : BitLinear-FPGA Alpha (Erven LE BIVIC)
 * Week    : 2
 * Purpose : CPU golden reference implementation.
 *
 * NOTE: This code was generated with AI assistance.
 * Verified by: compiling, running all 10 unit tests, and comparing
 * output byte-for-byte against the Python reference (bitlinear_reference.py).
 */

#include "bitlinear_reference.hpp"

#include <fstream>
#include <stdexcept>
#include <cassert>

// ---------------------------------------------------------------------------
// Core reference function
// ---------------------------------------------------------------------------

void bitlinear_reference(
    const int8_t*  x,
    const int8_t*  W,
    int32_t*       y,
    int            M,
    int            K
) {
    for (int m = 0; m < M; ++m) {
        int32_t acc = 0;
        for (int k = 0; k < K; ++k) {
            int8_t w = W[m * K + k];
            if (w == 1) {
                acc += static_cast<int32_t>(x[k]);
            } else if (w == -1) {
                acc -= static_cast<int32_t>(x[k]);
            }
            // w == 0: skip (no multiply needed)
        }
        y[m] = acc;
    }
}

// ---------------------------------------------------------------------------
// 2-bit ternary packing
// ---------------------------------------------------------------------------

uint8_t encode_weight(int8_t w) {
    if (w ==  0) return 0b00;
    if (w ==  1) return 0b01;
    if (w == -1) return 0b10;
    return 0b11; // invalid / reserved
}

int8_t decode_weight(uint8_t code) {
    switch (code & 0b11) {
        case 0b00: return  0;
        case 0b01: return  1;
        case 0b10: return -1;
        default:   return  0; // 0b11 reserved → 0
    }
}

std::vector<uint8_t> pack_ternary_2bit(const int8_t* W_flat, int n) {
    // Pad to next multiple of 4
    int n_padded = (n + 3) & ~3;
    int n_bytes  = n_padded / 4;

    std::vector<uint8_t> packed(n_bytes, 0);
    for (int i = 0; i < n_bytes; ++i) {
        uint8_t byte = 0;
        for (int j = 0; j < 4; ++j) {
            int idx = i * 4 + j;
            int8_t w = (idx < n) ? W_flat[idx] : 0;
            byte |= static_cast<uint8_t>(encode_weight(w) << (j * 2));
        }
        packed[i] = byte;
    }
    return packed;
}

std::vector<int8_t> unpack_ternary_2bit(const uint8_t* packed, int n_weights) {
    std::vector<int8_t> result(n_weights);
    for (int i = 0; i < n_weights; ++i) {
        int byte_idx = i / 4;
        int bit_off  = (i % 4) * 2;
        uint8_t code = (packed[byte_idx] >> bit_off) & 0b11;
        result[i] = decode_weight(code);
    }
    return result;
}

// ---------------------------------------------------------------------------
// Binary I/O helpers
// ---------------------------------------------------------------------------

std::vector<int8_t> load_int8_bin(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    std::streamsize size = f.tellg();
    f.seekg(0);
    std::vector<int8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::vector<int32_t> load_int32_bin(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    std::streamsize size = f.tellg();
    f.seekg(0);
    std::vector<int32_t> data(size / sizeof(int32_t));
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

bool save_int32_bin(const std::string& path, const int32_t* data, int n) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write(reinterpret_cast<const char*>(data), n * sizeof(int32_t));
    return f.good();
}

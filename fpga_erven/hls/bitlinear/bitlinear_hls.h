// =============================================================================
// bitlinear_hls.h
// HLS BitLinear Kernel Header — BitLinear-FPGA Alpha
// =============================================================================

#ifndef BITLINEAR_HLS_H
#define BITLINEAR_HLS_H

#include <cstdint>

// ap_int.h is only available inside Vitis HLS — guard it for standalone g++ compilation
#ifdef __SYNTHESIS__
#include <ap_int.h>
#endif

// Maximum supported dimensions (for HLS array sizing)
#define MAX_M 512
#define MAX_K 1024
#define MAX_PACKED_K (MAX_K / 4)

// Top-level HLS function
void bitlinear_hls(
    const int8_t*  x,        // int8 activation vector [K]
    const uint8_t* W_packed, // 2-bit packed ternary weights [M * ceil(K/4)]
    int32_t*       y,        // int32 output vector [M]
    int            M,        // number of output rows
    int            K         // number of input columns
);

#endif // BITLINEAR_HLS_H

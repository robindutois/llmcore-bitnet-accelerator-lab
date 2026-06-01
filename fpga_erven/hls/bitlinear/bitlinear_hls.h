#ifndef BITLINEAR_HLS_H
#define BITLINEAR_HLS_H
#include <stdint.h>
#define MAX_M 512
#define MAX_K 1024
#define MAX_PACKED_K (MAX_K / 4)
void bitlinear_hls(const int8_t* x, const uint8_t* W_packed, int32_t* y, int M, int K);
#endif

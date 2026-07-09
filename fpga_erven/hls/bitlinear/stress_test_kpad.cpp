// stress_test_kpad.cpp — extra validation of the K%4 fix beyond test09_manual
// Sweeps many (M,K) pairs, including K not a multiple of 4, M=1..9, random
// weights/activations, compares bitlinear_hls (with the K_pad fix applied)
// against the plain unpacked C++ reference.
#include "bitlinear_hls.h"
#include "pack_ternary_2bit.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>

static void cpu_ref(const int8_t* x, const int8_t* W, int32_t* y, int M, int K) {
    for (int m = 0; m < M; ++m) {
        int32_t acc = 0;
        for (int k = 0; k < K; ++k) {
            int8_t w = W[m * K + k];
            if (w == 1) acc += x[k];
            if (w == -1) acc -= x[k];
        }
        y[m] = acc;
    }
}

int main() {
    srand(12345);
    int fails = 0, total = 0;
    static int8_t  W_unpacked[64 * 64];
    static int8_t  x[64];
    static int32_t y_ref[64];
    static int32_t y_hls[64];
    static uint8_t W_packed[64 * 64 / 4 + 64]; // generous margin for padding

    for (int M = 1; M <= 9; ++M) {
        for (int K = 1; K <= 40; ++K) {
            total++;
            for (int i = 0; i < M * K; ++i) {
                int r = rand() % 3;
                W_unpacked[i] = (r == 0) ? 0 : (r == 1) ? 1 : -1;
            }
            for (int k = 0; k < K; ++k)
                x[k] = (int8_t)((rand() % 255) - 127);

            cpu_ref(x, W_unpacked, y_ref, M, K);

            int K_pad = ((K + 3) / 4) * 4;
            memset(W_packed, 0, sizeof(W_packed));
            pack_matrix(W_unpacked, W_packed, M, K);

            static int8_t x_padded[64];
            memset(x_padded, 0, sizeof(x_padded));
            memcpy(x_padded, x, K);

            memset(y_hls, 0, sizeof(y_hls));
            bitlinear_hls(x_padded, W_packed, y_hls, M, K_pad);

            int ok = 1;
            for (int m = 0; m < M; ++m)
                if (y_hls[m] != y_ref[m]) { ok = 0; break; }

            if (!ok) {
                fails++;
                printf("FAIL  M=%d K=%d (K_pad=%d)\n", M, K, K_pad);
            }
        }
    }
    printf("\n=== Stress test: %d/%d combinations PASSED (M=1..9, K=1..40) ===\n",
           total - fails, total);
    return fails ? 1 : 0;
}

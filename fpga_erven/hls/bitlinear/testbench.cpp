// =============================================================================
// testbench.cpp
// HLS C-Simulation Testbench — BitLinear-FPGA Alpha
// =============================================================================
// This testbench:
//   1. Generates test vectors
//   2. Runs the C++ golden reference
//   3. Packs weights with pack_matrix()
//   4. Runs the HLS kernel bitlinear_hls()
//   5. Compares outputs — must be EXACTLY equal
//
// Compile for standalone test (no HLS):
//   g++ -o testbench testbench.cpp bitlinear_hls.cpp \
//       ../packing/pack_ternary_2bit.cpp ../packing/unpack_ternary_2bit.cpp
//
// For Vitis HLS C-Sim: open project, set testbench.cpp as testbench file.
// =============================================================================

#include "bitlinear_hls.h"
#include "../packing/pack_ternary_2bit.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// =============================================================================
// C++ Golden Reference
// =============================================================================
void bitlinear_reference(
    const int8_t* x,
    const int8_t* W,
    int32_t*      y,
    int M, int K
) {
    for (int m = 0; m < M; ++m) {
        int32_t acc = 0;
        for (int k = 0; k < K; ++k) {
            int8_t w = W[m * K + k];
            if (w ==  1) acc += (int32_t)x[k];
            if (w == -1) acc -= (int32_t)x[k];
        }
        y[m] = acc;
    }
}

// =============================================================================
// Helper: generate random ternary weight in {-1, 0, +1}
// =============================================================================
int8_t random_ternary() {
    int r = rand() % 3;
    if (r == 0) return  0;
    if (r == 1) return  1;
    return -1;
}

// =============================================================================
// Run one test case
// Returns true if HLS output matches reference
// =============================================================================
bool run_test(const char* name, int M, int K,
              int8_t* x, int8_t* W_unpacked)
{
    int packed_K = (K + 3) / 4;

    // Allocate buffers
    uint8_t W_packed[(M * K + 3) / 4];
    int32_t y_ref[M];
    int32_t y_hls[M];

    // Pack weights
    pack_flat(W_unpacked, W_packed, M, K);

    // Golden reference
    bitlinear_reference(x, W_unpacked, y_ref, M, K);

    // HLS kernel (C-simulation)
    bitlinear_hls(x, W_packed, y_hls, M, K);

    // Compare
    bool pass = true;
    for (int m = 0; m < M; ++m) {
        if (y_hls[m] != y_ref[m]) {
            pass = false;
            printf("  MISMATCH at m=%d: HLS=%d  REF=%d\n", m, y_hls[m], y_ref[m]);
            if (m > 4) { printf("  (further mismatches suppressed)\n"); break; }
        }
    }

    printf("[%-35s] M=%-4d K=%-4d -> %s\n", name, M, K, pass ? "PASS" : "FAIL");
    return pass;
}

// =============================================================================
// main
// =============================================================================
int main() {
    srand((unsigned)time(nullptr));

    printf("=== HLS BitLinear C-Simulation Testbench ===\n\n");

    int total = 0, passed = 0;

    // ---------------------------------------------------------------
    // Test 1: all weights = 0
    // ---------------------------------------------------------------
    {
        int M = 8, K = 16;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) W[i] = 0;
        total++; if (run_test("all W=0", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 2: all weights = +1
    // ---------------------------------------------------------------
    {
        int M = 8, K = 16;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) W[i] = 1;
        total++; if (run_test("all W=+1", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 3: all weights = -1
    // ---------------------------------------------------------------
    {
        int M = 8, K = 16;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) W[i] = -1;
        total++; if (run_test("all W=-1", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 4: max int8 activations
    // ---------------------------------------------------------------
    {
        int M = 8, K = 16;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = 127;
        for (int i = 0; i < M * K; ++i) W[i] = random_ternary();
        total++; if (run_test("x=max(127)", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 5: min int8 activations
    // ---------------------------------------------------------------
    {
        int M = 8, K = 16;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = -128;
        for (int i = 0; i < M * K; ++i) W[i] = random_ternary();
        total++; if (run_test("x=min(-128)", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 6: K not a multiple of 4
    // ---------------------------------------------------------------
    {
        int M = 4, K = 7;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) W[i] = random_ternary();
        total++; if (run_test("K not multiple of 4 (K=7)", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 7: small matrix (manually verifiable)
    // ---------------------------------------------------------------
    {
        int M = 2, K = 4;
        int8_t x[4]    = {1, 2, 3, 4};
        int8_t W[2][4] = {{1, -1, 0, 1}, {0, 1, -1, -1}};
        // Row 0: 1*1 + (-1)*2 + 0*3 + 1*4 = 1 - 2 + 0 + 4 = 3
        // Row 1: 0*1 + 1*2 + (-1)*3 + (-1)*4 = 0 + 2 - 3 - 4 = -5
        total++; if (run_test("small manual (M=2 K=4)", M, K, x, (int8_t*)W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 8: random medium matrix
    // ---------------------------------------------------------------
    {
        int M = 64, K = 128;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) W[i] = random_ternary();
        total++; if (run_test("random M=64 K=128", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 9: larger matrix
    // ---------------------------------------------------------------
    {
        int M = 128, K = 256;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) W[i] = random_ternary();
        total++; if (run_test("random M=128 K=256", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Test 10: sparse weights (mostly zero)
    // ---------------------------------------------------------------
    {
        int M = 64, K = 128;
        int8_t x[K], W[M * K];
        for (int i = 0; i < K;     ++i) x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) {
            // ~80% zeros, 10% +1, 10% -1
            int r = rand() % 10;
            W[i] = (r == 0) ? 1 : (r == 1) ? -1 : 0;
        }
        total++; if (run_test("sparse W (80% zero)", M, K, x, W)) passed++;
    }

    // ---------------------------------------------------------------
    // Summary
    // ---------------------------------------------------------------
    printf("\n=== C-Simulation Result: %d / %d PASSED ===\n", passed, total);

    if (passed == total) {
        printf("STATUS: PASS — HLS kernel matches C++ reference on all tests.\n");
        return 0;
    } else {
        printf("STATUS: FAIL — %d test(s) failed.\n", total - passed);
        return 1;
    }
}

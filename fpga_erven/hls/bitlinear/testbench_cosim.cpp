// =============================================================================
// testbench_cosim.cpp
// HLS Co-Simulation Testbench — BitLinear-FPGA Alpha — Week 5
// =============================================================================
// Fixed for Vitis HLS co-simulation:
//   - No VLAs (Variable Length Arrays) — all buffers are statically sized
//     or heap-allocated via malloc
//   - No large stack arrays in main()
//   - Same 10 test cases as Week 3 C-sim testbench
//
// Standalone compile (no HLS):
//   g++ -o testbench_cosim testbench_cosim.cpp bitlinear_hls.cpp \
//       ../packing/pack_ternary_2bit.cpp ../packing/unpack_ternary_2bit.cpp
// =============================================================================

#include "bitlinear_hls.h"
#include "../packing/pack_ternary_2bit.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// Maximum sizes — must match bitlinear_hls.h MAX_M / MAX_K
#define TB_MAX_M    512
#define TB_MAX_K    1024
#define TB_MAX_MK   (TB_MAX_M * TB_MAX_K)
#define TB_MAX_PACK ((TB_MAX_MK + 3) / 4)

// =============================================================================
// Static buffers — allocated once, reused across test cases
// Avoids VLAs and large stack allocations in functions
// =============================================================================
static int8_t  s_x       [TB_MAX_K];
static int8_t  s_W       [TB_MAX_MK];
static uint8_t s_W_packed[TB_MAX_PACK];
static int32_t s_y_ref   [TB_MAX_M];
static int32_t s_y_hls   [TB_MAX_M];

// =============================================================================
// C++ Golden Reference (inlined — avoids external dependency in co-sim)
// =============================================================================
static void bitlinear_reference(
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
// Helper: random ternary weight in {-1, 0, +1}
// =============================================================================
static int8_t random_ternary() {
    int r = rand() % 3;
    if (r == 0) return  0;
    if (r == 1) return  1;
    return -1;
}

// =============================================================================
// run_test — uses static buffers, no VLAs
// =============================================================================
static bool run_test(const char* name, int M, int K) {
    // Pack weights into static buffer
    pack_flat(s_W, s_W_packed, M, K);

    // Golden reference
    bitlinear_reference(s_x, s_W, s_y_ref, M, K);

    // HLS kernel
    bitlinear_hls(s_x, s_W_packed, s_y_hls, M, K);

    // Compare
    bool pass = true;
    for (int m = 0; m < M; ++m) {
        if (s_y_hls[m] != s_y_ref[m]) {
            pass = false;
            printf("  MISMATCH at m=%d: HLS=%d  REF=%d\n",
                   m, s_y_hls[m], s_y_ref[m]);
            if (m >= 4) { printf("  (further mismatches suppressed)\n"); break; }
        }
    }

    printf("[%-35s] M=%-4d K=%-4d -> %s\n",
           name, M, K, pass ? "PASS" : "FAIL");
    return pass;
}

// =============================================================================
// main
// =============================================================================
int main() {
    srand(42);  // fixed seed for reproducibility

    printf("=== HLS BitLinear Co-Simulation Testbench ===\n\n");

    int total = 0, passed = 0;

    // ------------------------------------------------------------------
    // Test 1: all W = 0
    // ------------------------------------------------------------------
    {
        int M = 8, K = 16;
        for (int i = 0; i < K;     ++i) s_x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) s_W[i] = 0;
        total++; if (run_test("all W=0", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 2: all W = +1
    // ------------------------------------------------------------------
    {
        int M = 8, K = 16;
        for (int i = 0; i < K;     ++i) s_x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) s_W[i] = 1;
        total++; if (run_test("all W=+1", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 3: all W = -1
    // ------------------------------------------------------------------
    {
        int M = 8, K = 16;
        for (int i = 0; i < K;     ++i) s_x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) s_W[i] = -1;
        total++; if (run_test("all W=-1", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 4: x = max int8 (+127)
    // ------------------------------------------------------------------
    {
        int M = 8, K = 16;
        for (int i = 0; i < K;     ++i) s_x[i] = 127;
        for (int i = 0; i < M * K; ++i) s_W[i] = random_ternary();
        total++; if (run_test("x=max(+127)", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 5: x = min int8 (-128)
    // ------------------------------------------------------------------
    {
        int M = 8, K = 16;
        for (int i = 0; i < K;     ++i) s_x[i] = -128;
        for (int i = 0; i < M * K; ++i) s_W[i] = random_ternary();
        total++; if (run_test("x=min(-128)", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 6: K not a multiple of 4
    // ------------------------------------------------------------------
    {
        int M = 4, K = 7;
        for (int i = 0; i < K;     ++i) s_x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) s_W[i] = random_ternary();
        total++; if (run_test("K not multiple of 4 (K=7)", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 7: small matrix — manually verifiable
    //   x = [1, 2, 3, 4]
    //   W[0] = [ 1, -1,  0,  1]  -> 1 - 2 + 0 + 4 =  3
    //   W[1] = [ 0,  1, -1, -1]  -> 0 + 2 - 3 - 4 = -5
    // ------------------------------------------------------------------
    {
        int M = 2, K = 4;
        s_x[0] =  1; s_x[1] =  2; s_x[2] =  3; s_x[3] =  4;
        s_W[0] =  1; s_W[1] = -1; s_W[2] =  0; s_W[3] =  1;
        s_W[4] =  0; s_W[5] =  1; s_W[6] = -1; s_W[7] = -1;
        total++; if (run_test("small manual (M=2 K=4)", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 8: random M=64 K=128
    // ------------------------------------------------------------------
    {
        int M = 64, K = 128;
        for (int i = 0; i < K;     ++i) s_x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) s_W[i] = random_ternary();
        total++; if (run_test("random M=64 K=128", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 9: random M=128 K=256
    // ------------------------------------------------------------------
    {
        int M = 128, K = 256;
        for (int i = 0; i < K;     ++i) s_x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) s_W[i] = random_ternary();
        total++; if (run_test("random M=128 K=256", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Test 10: sparse W (~80% zeros)
    // ------------------------------------------------------------------
    {
        int M = 64, K = 128;
        for (int i = 0; i < K;     ++i) s_x[i] = (int8_t)(rand() % 256 - 128);
        for (int i = 0; i < M * K; ++i) {
            int r = rand() % 10;
            s_W[i] = (r == 0) ? 1 : (r == 1) ? -1 : 0;
        }
        total++; if (run_test("sparse W (80% zero)", M, K)) passed++;
    }

    // ------------------------------------------------------------------
    // Summary
    // ------------------------------------------------------------------
    printf("\n=== Co-Simulation Result: %d / %d PASSED ===\n", passed, total);

    if (passed == total) {
        printf("STATUS: PASS — RTL matches C++ reference on all tests.\n");
        return 0;
    } else {
        printf("STATUS: FAIL — %d test(s) failed.\n", total - passed);
        return 1;
    }
}

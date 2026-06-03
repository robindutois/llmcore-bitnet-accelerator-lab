// =============================================================================
// testbench_cosim_vectors.cpp
// HLS Co-Simulation Testbench — BitLinear-FPGA Alpha — Week 5 (corrected)
// =============================================================================
// Closes the full validation chain:
//   Python ref (S2) → .bin files → C++ ref → HLS C-sim (S3) → RTL co-sim (S5)
//
// IMPORTANT — buffer sizing rule for HLS co-sim (WRAPC):
//   The WRAPC wrapper copies exactly `depth` elements from each pointer passed
//   to the HLS function. The three HLS interface buffers must therefore be sized
//   to AT LEAST the depth value in the corresponding pragma:
//     s_x        : depth=1024   (int8_t)
//     s_W_packed : depth=131072 (uint8_t)   — 128 KB
//     s_y_hls    : depth=512    (int32_t)
//   Other buffers (s_W_unpacked, s_y_ref, s_y_expected) are not HLS function
//   arguments; they can be sized to the actual test vector maximums.
//
// Standalone compile:
//   g++ -o testbench_cosim_vectors testbench_cosim_vectors.cpp \
//       bitlinear_hls.cpp ../packing/pack_ternary_2bit.cpp \
//       ../packing/unpack_ternary_2bit.cpp \
//       -I. -I../packing -std=c++14 \
//       -DTV_BASE_PATH=\"$(realpath ../../../reference/test_vectors)\"
// =============================================================================

#include "bitlinear_hls.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// =============================================================================
// TV_BASE_PATH — injected by tv_path_generated.h (written by TCL before cosim)
// =============================================================================
#if defined(__has_include) && __has_include("tv_path_generated.h")
  #include "tv_path_generated.h"
#endif
#ifndef TV_BASE_PATH
  // Fallback: 7 levels up from wrapc/ reaches repo root
  #define TV_BASE_PATH "../../../../../../../reference/test_vectors"
#endif

// =============================================================================
// Buffer dimensions
//
// HLS interface buffers — must match pragma depth values in bitlinear_hls.cpp:
//   #pragma HLS INTERFACE m_axi port=x        depth=1024
//   #pragma HLS INTERFACE m_axi port=W_packed depth=131072
//   #pragma HLS INTERFACE m_axi port=y        depth=512
// =============================================================================
#define HLS_DEPTH_X       1024    // pragma depth for x       (int8_t)
#define HLS_DEPTH_W_PACK  131072  // pragma depth for W_packed (uint8_t) — 128KB
#define HLS_DEPTH_Y       512     // pragma depth for y       (int32_t)

// Non-HLS buffers — sized to actual test vector maximums (test10_large: M=256 K=512)
#define TV_MAX_M   256
#define TV_MAX_K   512
#define TV_MAX_MK  (TV_MAX_M * TV_MAX_K)

// HLS function arguments — sized to pragma depth (WRAPC requirement)
static int8_t   s_x        [HLS_DEPTH_X];       // 1 024 bytes
static uint8_t  s_W_packed [HLS_DEPTH_W_PACK];  // 131 072 bytes (128 KB)
static int32_t  s_y_hls    [HLS_DEPTH_Y];       // 2 048 bytes

// Non-HLS buffers — only sized to what the test vectors actually need
static int8_t   s_W_unpacked[TV_MAX_MK];         // 131 072 bytes (128 KB)
static int32_t  s_y_ref     [TV_MAX_M];          // 1 024 bytes
static int32_t  s_y_expected[TV_MAX_M];          // 1 024 bytes

// =============================================================================
// C++ golden reference (inlined — same logic as Week 2 bitlinear_reference.cpp)
// =============================================================================
static void bitlinear_cpp_ref(
    const int8_t* x, const int8_t* W, int32_t* y, int M, int K
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
// Binary file loaders — C-style only (co-sim compatible)
// =============================================================================
static int load_int8_bin(const char* path, int8_t* buf, int max_elems) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("  ERROR: cannot open %s\n", path); return -1; }
    int n = (int)fread(buf, sizeof(int8_t), max_elems, f);
    fclose(f);
    return n;
}

static int load_uint8_bin(const char* path, uint8_t* buf, int max_elems) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("  ERROR: cannot open %s\n", path); return -1; }
    int n = (int)fread(buf, sizeof(uint8_t), max_elems, f);
    fclose(f);
    return n;
}

static int load_int32_bin(const char* path, int32_t* buf, int max_elems) {
    FILE* f = fopen(path, "rb");
    if (!f) { printf("  ERROR: cannot open %s\n", path); return -1; }
    int n = (int)fread(buf, sizeof(int32_t), max_elems, f);
    fclose(f);
    return n;
}

static int parse_meta(const char* path, int* M_out, int* K_out) {
    FILE* f = fopen(path, "r");
    if (!f) { printf("  ERROR: cannot open %s\n", path); return -1; }
    char line[256];
    *M_out = -1; *K_out = -1;
    while (fgets(line, sizeof(line), f)) {
        int val;
        if (strstr(line, "\"M\"") && sscanf(line, " \"M\": %d", &val) == 1) *M_out = val;
        if (strstr(line, "\"K\"") && sscanf(line, " \"K\": %d", &val) == 1) *K_out = val;
    }
    fclose(f);
    return (*M_out >= 0 && *K_out >= 0) ? 0 : -1;
}

// =============================================================================
// run_test_vector — [A] HLS vs Python, [B] C++ vs Python, [C] HLS vs C++
// =============================================================================
static int run_test_vector(const char* base_path, const char* test_name) {
    char path[512];

    snprintf(path, sizeof(path), "%s/%s/metadata.json", base_path, test_name);
    int M, K;
    if (parse_meta(path, &M, &K) < 0) {
        printf("[%-24s] ERROR: cannot parse metadata\n", test_name);
        return 0;
    }
    if (M > TV_MAX_M || K > TV_MAX_K) {
        printf("[%-24s] ERROR: M=%d K=%d exceeds buffer limits\n", test_name, M, K);
        return 0;
    }
    int packed_size = (M * K + 3) / 4;
    if (K > HLS_DEPTH_X || packed_size > HLS_DEPTH_W_PACK || M > HLS_DEPTH_Y) {
        printf("[%-24s] ERROR: exceeds HLS pragma depth limits\n", test_name);
        return 0;
    }

    // Load inputs into HLS-interface buffers (zeroed first to avoid WRAPC reading garbage)
    memset(s_x,        0, sizeof(s_x));
    memset(s_W_packed, 0, sizeof(s_W_packed));
    memset(s_y_hls,    0, sizeof(s_y_hls));

    snprintf(path, sizeof(path), "%s/%s/activation_int8.bin", base_path, test_name);
    if (load_int8_bin(path, s_x, HLS_DEPTH_X) < 0) return 0;

    snprintf(path, sizeof(path), "%s/%s/weight_ternary_unpacked.bin", base_path, test_name);
    if (load_int8_bin(path, s_W_unpacked, TV_MAX_MK) < 0) return 0;

    snprintf(path, sizeof(path), "%s/%s/weight_ternary_packed_2bit.bin", base_path, test_name);
    if (load_uint8_bin(path, s_W_packed, HLS_DEPTH_W_PACK) < 0) return 0;

    snprintf(path, sizeof(path), "%s/%s/expected_output_int32.bin", base_path, test_name);
    if (load_int32_bin(path, s_y_expected, TV_MAX_M) < 0) return 0;

    // [B] C++ reference vs expected (Python ground truth)
    bitlinear_cpp_ref(s_x, s_W_unpacked, s_y_ref, M, K);
    int ref_ok = 1;
    for (int m = 0; m < M; ++m)
        if (s_y_ref[m] != s_y_expected[m]) { ref_ok = 0; break; }

    // [A] HLS kernel (RTL in co-sim) vs expected (Python ground truth)
    bitlinear_hls(s_x, s_W_packed, s_y_hls, M, K);
    int hls_ok = 1;
    for (int m = 0; m < M; ++m)
        if (s_y_hls[m] != s_y_expected[m]) { hls_ok = 0; break; }

    // [C] HLS vs C++ reference
    int cross_ok = 1;
    for (int m = 0; m < M; ++m)
        if (s_y_hls[m] != s_y_ref[m]) { cross_ok = 0; break; }

    int all_ok = ref_ok && hls_ok && cross_ok;
    printf("[%-24s] M=%-4d K=%-4d | [A]HLS-vs-py: %s | [B]C++-vs-py: %s | [C]HLS-vs-C++: %s\n",
           test_name, M, K,
           hls_ok   ? "PASS" : "FAIL",
           ref_ok   ? "PASS" : "FAIL",
           cross_ok ? "PASS" : "FAIL");
    return all_ok;
}

// =============================================================================
// main
// =============================================================================
int main() {
    printf("=== HLS BitLinear Co-Simulation — Week 2 Test Vector Validation ===\n");
    printf("TV_BASE_PATH: %s\n\n", TV_BASE_PATH);

    char check_path[512];
    snprintf(check_path, sizeof(check_path), "%s/test01_random/metadata.json", TV_BASE_PATH);
    FILE* check = fopen(check_path, "r");
    if (!check) {
        printf("ERROR: Cannot open test vectors at: %s\n", TV_BASE_PATH);
        return 1;
    }
    fclose(check);
    printf("Path check: OK\n\n");

    static const char* tests[] = {
        "test01_random",  "test02_W_zero",  "test03_W_plus1", "test04_W_minus1",
        "test05_sparse",  "test06_dense",   "test07_xmax",    "test08_xmin",
        "test09_manual",  "test10_large",   0
    };

    int total = 0, passed = 0;
    for (int i = 0; tests[i]; ++i) {
        total++;
        if (run_test_vector(TV_BASE_PATH, tests[i])) passed++;
    }

    printf("\n=== Validation Result: %d / %d PASSED ===\n", passed, total);
    if (passed == total) {
        printf("STATUS: PASS\n");
        printf("  Full chain: Python(S2) -> .bin -> C++ ref -> HLS-csim(S3) -> RTL(S5)\n");
        return 0;
    }
    printf("STATUS: FAIL — %d test(s) failed.\n", total - passed);
    return 1;
}

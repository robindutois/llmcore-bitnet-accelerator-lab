// =============================================================================
// testbench_week2_vectors.cpp
// Cross-validation: HLS kernel vs Week 2 binary test vectors
// =============================================================================
// This testbench loads the REAL binary files generated in Week 2:
//   - activation_int8.bin
//   - weight_ternary_packed_2bit.bin   (used by HLS kernel directly)
//   - weight_ternary_unpacked.bin      (used by C++ reference)
//   - expected_output_int32.bin        (ground truth from Python reference)
//
// It runs THREE comparisons per test vector:
//   [A] HLS kernel output    vs expected_output_int32.bin
//   [B] C++ reference output vs expected_output_int32.bin
//   [C] HLS kernel output    vs C++ reference output
//
// All three must be EXACT (int32 byte-for-byte match).
//
// Compile:
//   g++ -o testbench_week2_vectors testbench_week2_vectors.cpp \
//       bitlinear_hls.cpp pack_ternary_2bit.cpp unpack_ternary_2bit.cpp \
//       -I. -std=c++14
//
// Run (from the hls/bitlinear/ directory):
//   ./testbench_week2_vectors ../../../../reference/test_vectors
//
// The path argument must point to the test_vectors/ folder in the repo.
// =============================================================================

#include "bitlinear_hls.h"
#include "pack_ternary_2bit.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include <dirent.h>

// =============================================================================
// Binary I/O helpers (mirrors Week 2 reference)
// =============================================================================

std::vector<int8_t> load_int8_bin(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    auto size = f.tellg(); f.seekg(0);
    std::vector<int8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::vector<uint8_t> load_uint8_bin(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    auto size = f.tellg(); f.seekg(0);
    std::vector<uint8_t> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

std::vector<int32_t> load_int32_bin(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) throw std::runtime_error("Cannot open: " + path);
    auto size = f.tellg(); f.seekg(0);
    std::vector<int32_t> data(size / sizeof(int32_t));
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

// =============================================================================
// C++ golden reference (identical to Week 2 bitlinear_reference.cpp)
// =============================================================================
void bitlinear_reference(
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
// Run one test vector directory
// =============================================================================
bool run_one_test(const std::string& test_dir) {
    // --- Load metadata (simple parse: just read M, K from filenames/sizes) ---
    std::string act_path      = test_dir + "/activation_int8.bin";
    std::string wpacked_path  = test_dir + "/weight_ternary_packed_2bit.bin";
    std::string wunpacked_path= test_dir + "/weight_ternary_unpacked.bin";
    std::string expected_path = test_dir + "/expected_output_int32.bin";

    std::vector<int8_t>  x_data, W_unpacked;
    std::vector<uint8_t> W_packed;
    std::vector<int32_t> expected;

    try {
        x_data     = load_int8_bin(act_path);
        W_packed   = load_uint8_bin(wpacked_path);
        W_unpacked = load_int8_bin(wunpacked_path);
        expected   = load_int32_bin(expected_path);
    } catch (const std::exception& e) {
        printf("  ERROR loading files: %s\n", e.what());
        return false;
    }

    int M = (int)expected.size();
    int K = (int)x_data.size();

    // Sanity check
    if ((int)W_unpacked.size() != M * K) {
        printf("  ERROR: W_unpacked size mismatch (%zu vs M*K=%d)\n",
               W_unpacked.size(), M * K);
        return false;
    }

    // --- [B] C++ reference vs expected ---
    std::vector<int32_t> y_ref(M);
    bitlinear_reference(x_data.data(), W_unpacked.data(), y_ref.data(), M, K);

    bool ref_ok = true;
    for (int m = 0; m < M; ++m) {
        if (y_ref[m] != expected[m]) {
            ref_ok = false;
            printf("  [B] MISMATCH at m=%d: ref=%d expected=%d\n",
                   m, y_ref[m], expected[m]);
            if (m > 3) { printf("  (suppressed)\n"); break; }
        }
    }

    // --- [A] HLS kernel vs expected ---
    std::vector<int32_t> y_hls(M);
    bitlinear_hls(x_data.data(), W_packed.data(), y_hls.data(), M, K);

    bool hls_ok = true;
    for (int m = 0; m < M; ++m) {
        if (y_hls[m] != expected[m]) {
            hls_ok = false;
            printf("  [A] MISMATCH at m=%d: hls=%d expected=%d\n",
                   m, y_hls[m], expected[m]);
            if (m > 3) { printf("  (suppressed)\n"); break; }
        }
    }

    // --- [C] HLS kernel vs C++ reference ---
    bool cross_ok = true;
    for (int m = 0; m < M; ++m) {
        if (y_hls[m] != y_ref[m]) {
            cross_ok = false;
            printf("  [C] MISMATCH at m=%d: hls=%d ref=%d\n",
                   m, y_hls[m], y_ref[m]);
            if (m > 3) { printf("  (suppressed)\n"); break; }
        }
    }

    bool all_ok = ref_ok && hls_ok && cross_ok;
    printf("  M=%-4d K=%-4d | [A] HLS vs expected: %s | [B] ref vs expected: %s | [C] HLS vs ref: %s\n",
           M, K,
           hls_ok   ? "PASS" : "FAIL",
           ref_ok   ? "PASS" : "FAIL",
           cross_ok ? "PASS" : "FAIL");

    return all_ok;
}

// =============================================================================
// main
// =============================================================================
int main(int argc, char* argv[]) {
    const char* base_dir = "../../../../reference/test_vectors";
    if (argc > 1) base_dir = argv[1];

    printf("=== Week 2 Test Vector Cross-Validation ===\n");
    printf("Loading from: %s\n\n", base_dir);

    // List of known test vector subdirectories (in order)
    const char* tests[] = {
        "test01_random",
        "test02_W_zero",
        "test03_W_plus1",
        "test04_W_minus1",
        "test05_sparse",
        "test06_dense",
        "test07_xmax",
        "test08_xmin",
        "test09_manual",
        "test10_large",
        nullptr
    };

    int total = 0, passed = 0;

    for (int i = 0; tests[i] != nullptr; ++i) {
        std::string test_dir = std::string(base_dir) + "/" + tests[i];
        printf("[%s]\n", tests[i]);
        bool ok = run_one_test(test_dir);
        printf("  => %s\n\n", ok ? "PASS" : "FAIL");
        total++;
        if (ok) passed++;
    }

    printf("=== Cross-Validation Result: %d / %d PASSED ===\n", passed, total);

    if (passed == total) {
        printf("STATUS: PASS\n");
        printf("  HLS kernel matches Week 2 Python reference on all test vectors.\n");
        printf("  Week 2 -> Week 3 correctness chain: VERIFIED.\n");
        return 0;
    } else {
        printf("STATUS: FAIL — %d test(s) failed.\n", total - passed);
        return 1;
    }
}

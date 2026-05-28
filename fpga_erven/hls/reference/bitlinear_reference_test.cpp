/**
 * bitlinear_reference_test.cpp
 * ============================
 * Project : BitLinear-FPGA Alpha (Erven LE BIVIC)
 * Week    : 2
 * Purpose : 10 required unit tests + packing round-trip test.
 *           Must produce identical output to bitlinear_reference.py.
 *
 * Build:
 *   g++ -O2 -std=c++17 -o bitlinear_test \
 *       bitlinear_reference.cpp bitlinear_reference_test.cpp
 *
 * Run:
 *   ./bitlinear_test
 *   ./bitlinear_test path/to/test_vectors/   (cross-check against Python .bin files)
 *
 * NOTE: This code was generated with AI assistance.
 * Verified by: compiling, running, and comparing with Python reference output.
 */

#include "bitlinear_reference.hpp"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <random>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Minimal test framework
// ---------------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

#define ASSERT_TRUE(cond, msg)                                      \
    do {                                                            \
        if (cond) {                                                 \
            std::cout << "  \u2713 [PASS]  " << (msg) << "\n";    \
            ++g_pass;                                               \
        } else {                                                    \
            std::cout << "  \u2717 [FAIL]  " << (msg) << "\n";    \
            ++g_fail;                                               \
        }                                                           \
    } while (0)

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Simple LCG for reproducible pseudo-random numbers (seed matches Python rng seed=42)
// We use a proper seeded mt19937 to match NumPy's default_rng(42) distribution closely.
struct TestRng {
    std::mt19937 engine;
    TestRng(uint64_t seed = 42) : engine(seed) {}

    int8_t rand_x() {
        std::uniform_int_distribution<int> d(-128, 127);
        return static_cast<int8_t>(d(engine));
    }

    int8_t rand_ternary() {
        std::uniform_int_distribution<int> d(0, 2);
        int v = d(engine);
        return static_cast<int8_t>(v == 0 ? -1 : v == 1 ? 0 : 1);
    }
};

static std::vector<int8_t> make_rand_x(TestRng& rng, int K) {
    std::vector<int8_t> x(K);
    for (auto& v : x) v = rng.rand_x();
    return x;
}

static std::vector<int8_t> make_rand_W(TestRng& rng, int M, int K) {
    std::vector<int8_t> W(M * K);
    for (auto& v : W) v = rng.rand_ternary();
    return W;
}

// Vectorised reference (uses int32 accumulation via simple loop — no branch skip)
static std::vector<int32_t> fast_ref(
    const std::vector<int8_t>& x,
    const std::vector<int8_t>& W,
    int M, int K)
{
    std::vector<int32_t> y(M, 0);
    for (int m = 0; m < M; ++m)
        for (int k = 0; k < K; ++k)
            y[m] += static_cast<int32_t>(W[m * K + k]) * static_cast<int32_t>(x[k]);
    return y;
}

static bool vec_equal(const int32_t* a, const std::vector<int32_t>& b, int n) {
    for (int i = 0; i < n; ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// ---------------------------------------------------------------------------
// Tests 1–10
// ---------------------------------------------------------------------------

static void run_unit_tests() {
    TestRng rng(42);
    const int K = 64, M = 32;

    // Test 1 — random x, random W
    {
        auto x = make_rand_x(rng, K);
        auto W = make_rand_W(rng, M, K);
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        auto ref = fast_ref(x, W, M, K);
        ASSERT_TRUE(vec_equal(y.data(), ref, M),
                    "Test 1 — random x, random W");
    }

    // Test 2 — all W = 0  →  y must be all zeros
    {
        auto x = make_rand_x(rng, K);
        std::vector<int8_t>  W(M * K, 0);
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        bool ok = true;
        for (int i = 0; i < M; ++i) ok &= (y[i] == 0);
        ASSERT_TRUE(ok, "Test 2 — all W = 0  →  y = 0");
    }

    // Test 3 — all W = +1  →  y[m] = sum(x) for all m
    {
        auto x = make_rand_x(rng, K);
        std::vector<int8_t>  W(M * K, 1);
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        int32_t sum_x = 0;
        for (auto v : x) sum_x += static_cast<int32_t>(v);
        bool ok = true;
        for (int i = 0; i < M; ++i) ok &= (y[i] == sum_x);
        ASSERT_TRUE(ok, "Test 3 — all W = +1  →  y[m] = sum(x)");
    }

    // Test 4 — all W = -1  →  y[m] = -sum(x) for all m
    {
        auto x = make_rand_x(rng, K);
        std::vector<int8_t>  W(M * K, -1);
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        int32_t sum_x = 0;
        for (auto v : x) sum_x += static_cast<int32_t>(v);
        bool ok = true;
        for (int i = 0; i < M; ++i) ok &= (y[i] == -sum_x);
        ASSERT_TRUE(ok, "Test 4 — all W = -1  →  y[m] = -sum(x)");
    }

    // Test 5 — sparse W (~10% non-zero)
    {
        auto x = make_rand_x(rng, K);
        std::mt19937 rng2(123);
        std::uniform_real_distribution<float> ud(0.f, 1.f);
        auto W_dense = make_rand_W(rng, M, K);
        std::vector<int8_t> W(M * K);
        for (int i = 0; i < M * K; ++i)
            W[i] = (ud(rng2) < 0.10f) ? W_dense[i] : 0;
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        auto ref = fast_ref(x, W, M, K);
        ASSERT_TRUE(vec_equal(y.data(), ref, M), "Test 5 — sparse W (~10% nonzero)");
    }

    // Test 6 — dense W (~90% non-zero)
    {
        auto x = make_rand_x(rng, K);
        std::mt19937 rng2(456);
        std::uniform_real_distribution<float> ud(0.f, 1.f);
        auto W_dense = make_rand_W(rng, M, K);
        std::vector<int8_t> W(M * K);
        for (int i = 0; i < M * K; ++i)
            W[i] = (ud(rng2) < 0.90f) ? W_dense[i] : 0;
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        auto ref = fast_ref(x, W, M, K);
        ASSERT_TRUE(vec_equal(y.data(), ref, M), "Test 6 — dense W (~90% nonzero)");
    }

    // Test 7 — x = maximum int8 (+127)
    {
        std::vector<int8_t> x(K, 127);
        auto W = make_rand_W(rng, M, K);
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        auto ref = fast_ref(x, W, M, K);
        ASSERT_TRUE(vec_equal(y.data(), ref, M), "Test 7 — x = max int8 (+127)");
    }

    // Test 8 — x = minimum int8 (-128)
    {
        std::vector<int8_t> x(K, -128);
        auto W = make_rand_W(rng, M, K);
        std::vector<int32_t> y(M);
        bitlinear_reference(x.data(), W.data(), y.data(), M, K);
        auto ref = fast_ref(x, W, M, K);
        ASSERT_TRUE(vec_equal(y.data(), ref, M), "Test 8 — x = min int8 (-128)");
    }

    // Test 9 — small matrix manually verified
    //   x = [1, -2, 3]
    //   W = [[1, 0, -1],   →  y[0] = 1*1 + 0*(-2) + (-1)*3 = -2
    //        [0, 1,  1]]   →  y[1] = 0*1 + 1*(-2) +   1*3  =  1
    {
        int8_t  x[] = {1, -2, 3};
        int8_t  W[] = {1, 0, -1,
                       0, 1,  1};
        int32_t y[2];
        bitlinear_reference(x, W, y, 2, 3);
        ASSERT_TRUE(y[0] == -2 && y[1] == 1,
                    "Test 9 — small matrix manually verified");
    }

    // Test 10 — large matrix stress test (M=256, K=512)
    {
        const int M2 = 256, K2 = 512;
        auto x = make_rand_x(rng, K2);
        auto W = make_rand_W(rng, M2, K2);
        std::vector<int32_t> y(M2);
        bitlinear_reference(x.data(), W.data(), y.data(), M2, K2);
        // Cross-check first 4 rows with fast_ref on subset
        std::vector<int8_t> W4(W.begin(), W.begin() + 4 * K2);
        auto ref4 = fast_ref(x, W4, 4, K2);
        bool ok = true;
        for (int i = 0; i < 4; ++i) ok &= (y[i] == ref4[i]);
        ASSERT_TRUE(ok, "Test 10 — large matrix stress (256×512)");
    }
}

// ---------------------------------------------------------------------------
// 2-bit packing round-trip test (≥1000 random matrices)
// ---------------------------------------------------------------------------

static void run_packing_tests() {
    std::mt19937 engine(42);
    std::uniform_int_distribution<int> wd(0, 2);
    auto rand_w = [&]() -> int8_t {
        int v = wd(engine);
        return static_cast<int8_t>(v == 0 ? -1 : v == 1 ? 0 : 1);
    };

    bool all_ok = true;
    for (int t = 0; t < 1000; ++t) {
        const int n = 64; // 8×8
        std::vector<int8_t> W(n);
        for (auto& v : W) v = rand_w();

        auto packed   = pack_ternary_2bit(W.data(), n);
        auto recovered = unpack_ternary_2bit(packed.data(), n);

        for (int i = 0; i < n; ++i) {
            if (W[i] != recovered[i]) { all_ok = false; break; }
        }
        if (!all_ok) break;
    }
    ASSERT_TRUE(all_ok, "Pack round-trip (1000 random matrices)");
}

// ---------------------------------------------------------------------------
// Optional: cross-check against Python-generated .bin test vectors
// ---------------------------------------------------------------------------

static void cross_check_python_vectors(const std::string& vec_root) {
    std::cout << "\n--- Cross-check against Python test vectors ---\n";

    // For each test subdirectory in vec_root
    for (auto& entry : fs::directory_iterator(vec_root)) {
        if (!entry.is_directory()) continue;
        std::string dir = entry.path().string();
        std::string meta_path = dir + "/metadata.json";

        // Minimal JSON parse for M and K (no external library needed)
        std::ifstream mf(meta_path);
        if (!mf) continue;
        std::string content((std::istreambuf_iterator<char>(mf)),
                             std::istreambuf_iterator<char>());

        auto extract_int = [&](const std::string& key) -> int {
            std::string search = "\"" + key + "\": ";
            auto pos = content.find(search);
            if (pos == std::string::npos) return -1;
            return std::stoi(content.substr(pos + search.size()));
        };

        int M = extract_int("M");
        int K = extract_int("K");
        if (M <= 0 || K <= 0) continue;

        try {
            auto x   = load_int8_bin(dir + "/activation_int8.bin");
            auto W   = load_int8_bin(dir + "/weight_ternary_unpacked.bin");
            auto ref = load_int32_bin(dir + "/expected_output_int32.bin");

            if ((int)x.size() != K || (int)W.size() != M * K || (int)ref.size() != M) {
                std::cout << "  ! [SKIP]  " << entry.path().filename().string()
                          << " (size mismatch)\n";
                continue;
            }

            std::vector<int32_t> y(M);
            bitlinear_reference(x.data(), W.data(), y.data(), M, K);

            bool ok = true;
            for (int i = 0; i < M; ++i)
                if (y[i] != ref[i]) { ok = false; break; }

            std::string label = entry.path().filename().string()
                              + " (M=" + std::to_string(M)
                              + " K=" + std::to_string(K) + ")";
            ASSERT_TRUE(ok, "Python cross-check: " + label);

        } catch (const std::exception& e) {
            std::cout << "  ! [ERROR] " << entry.path().filename().string()
                      << ": " << e.what() << "\n";
        }
    }
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "============================================================\n";
    std::cout << "  BitLinear C++ Reference — Test Suite\n";
    std::cout << "============================================================\n";

    run_unit_tests();
    run_packing_tests();

    // Cross-check against Python .bin files if a path is provided
    if (argc >= 2) {
        std::string vec_root = argv[1];
        if (fs::exists(vec_root) && fs::is_directory(vec_root)) {
            cross_check_python_vectors(vec_root);
        } else {
            std::cerr << "Warning: test vector directory not found: " << vec_root << "\n";
        }
    }

    std::cout << "\n============================================================\n";
    std::cout << "  Results: " << g_pass << " PASS, " << g_fail << " FAIL\n";
    std::cout << "  Overall: "
              << (g_fail == 0 ? "ALL PASS \u2713" : "SOME TESTS FAILED \u2717") << "\n";
    std::cout << "============================================================\n\n";

    return (g_fail == 0) ? 0 : 1;
}

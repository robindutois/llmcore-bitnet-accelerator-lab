// =============================================================================
// test_packing.cpp
// Correctness test for 2-bit ternary packing/unpacking
// =============================================================================
// Compile:  g++ -o test_packing test_packing.cpp pack_ternary_2bit.cpp unpack_ternary_2bit.cpp
// Run:      ./test_packing
// Expected: All tests PASS
// =============================================================================

#include "../pack_ternary_2bit.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

// -------------------------------------------------------
// Helper: generate random ternary weight in {-1, 0, +1}
// -------------------------------------------------------
int8_t random_ternary() {
    int r = rand() % 3;
    if (r == 0) return  0;
    if (r == 1) return  1;
    return -1;
}

// -------------------------------------------------------
// Helper: compare two int8 arrays, return true if equal
// -------------------------------------------------------
bool arrays_equal(const int8_t* a, const int8_t* b, int n) {
    for (int i = 0; i < n; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

// -------------------------------------------------------
// Test 1: encode/decode single weights
// -------------------------------------------------------
bool test_single_weights() {
    bool ok = true;

    ok &= (decode_weight(encode_weight( 0)) ==  0);
    ok &= (decode_weight(encode_weight( 1)) ==  1);
    ok &= (decode_weight(encode_weight(-1)) == -1);

    printf("[Test 1] Single weight encode/decode: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

// -------------------------------------------------------
// Test 2: all zeros
// -------------------------------------------------------
bool test_all_zeros(int K) {
    int8_t  original[K], recovered[K];
    uint8_t packed[(K + 3) / 4];

    for (int k = 0; k < K; ++k) original[k] = 0;

    pack_row(original, packed, K);
    unpack_row(packed, recovered, K);

    bool ok = arrays_equal(original, recovered, K);
    printf("[Test 2] All zeros (K=%d): %s\n", K, ok ? "PASS" : "FAIL");
    return ok;
}

// -------------------------------------------------------
// Test 3: all +1
// -------------------------------------------------------
bool test_all_plus_one(int K) {
    int8_t  original[K], recovered[K];
    uint8_t packed[(K + 3) / 4];

    for (int k = 0; k < K; ++k) original[k] = 1;

    pack_row(original, packed, K);
    unpack_row(packed, recovered, K);

    bool ok = arrays_equal(original, recovered, K);
    printf("[Test 3] All +1   (K=%d): %s\n", K, ok ? "PASS" : "FAIL");
    return ok;
}

// -------------------------------------------------------
// Test 4: all -1
// -------------------------------------------------------
bool test_all_minus_one(int K) {
    int8_t  original[K], recovered[K];
    uint8_t packed[(K + 3) / 4];

    for (int k = 0; k < K; ++k) original[k] = -1;

    pack_row(original, packed, K);
    unpack_row(packed, recovered, K);

    bool ok = arrays_equal(original, recovered, K);
    printf("[Test 4] All -1   (K=%d): %s\n", K, ok ? "PASS" : "FAIL");
    return ok;
}

// -------------------------------------------------------
// Test 5: K not a multiple of 4 (boundary test)
// -------------------------------------------------------
bool test_non_multiple_of_4() {
    const int K = 7; // deliberately not a multiple of 4
    int8_t  original[K] = {1, -1, 0, 1, -1, 0, 1};
    int8_t  recovered[K];
    uint8_t packed[(K + 3) / 4];

    pack_row(original, packed, K);
    unpack_row(packed, recovered, K);

    bool ok = arrays_equal(original, recovered, K);
    printf("[Test 5] K not multiple of 4 (K=%d): %s\n", K, ok ? "PASS" : "FAIL");
    return ok;
}

// -------------------------------------------------------
// Test 6: full matrix pack/unpack
// -------------------------------------------------------
bool test_matrix(int M, int K) {
    int8_t  original[M * K], recovered[M * K];
    int     packed_K = (K + 3) / 4;
    uint8_t packed[M * packed_K];

    for (int i = 0; i < M * K; ++i) original[i] = random_ternary();

    pack_matrix(original, packed, M, K);
    unpack_matrix(packed, recovered, M, K);

    bool ok = arrays_equal(original, recovered, M * K);
    printf("[Test 6] Matrix pack/unpack (M=%d, K=%d): %s\n", M, K, ok ? "PASS" : "FAIL");
    return ok;
}

// -------------------------------------------------------
// Test 7: 1000 random matrices
// -------------------------------------------------------
bool test_random_1000() {
    int pass = 0;
    int fail = 0;

    for (int trial = 0; trial < 1000; ++trial) {
        int M = (rand() % 16) + 1;
        int K = (rand() % 64) + 1;

        int8_t  original[M * K], recovered[M * K];
        int     packed_K = (K + 3) / 4;
        uint8_t packed[M * packed_K];

        for (int i = 0; i < M * K; ++i) original[i] = random_ternary();

        pack_matrix(original, packed, M, K);
        unpack_matrix(packed, recovered, M, K);

        if (arrays_equal(original, recovered, M * K)) {
            pass++;
        } else {
            fail++;
            printf("  FAIL at trial %d: M=%d K=%d\n", trial, M, K);
        }
    }

    bool ok = (fail == 0);
    printf("[Test 7] 1000 random matrices: %d PASS, %d FAIL -> %s\n",
           pass, fail, ok ? "PASS" : "FAIL");
    return ok;
}

// -------------------------------------------------------
// main
// -------------------------------------------------------
int main() {
    srand((unsigned)time(nullptr));

    printf("=== 2-bit Ternary Packing Tests ===\n\n");

    int total = 0, passed = 0;

    auto run = [&](bool result) {
        total++;
        if (result) passed++;
    };

    run(test_single_weights());
    run(test_all_zeros(8));
    run(test_all_zeros(128));
    run(test_all_plus_one(16));
    run(test_all_minus_one(16));
    run(test_non_multiple_of_4());
    run(test_matrix(4, 8));
    run(test_matrix(64, 128));
    run(test_matrix(256, 512));
    run(test_random_1000());

    printf("\n=== Result: %d / %d tests PASSED ===\n", passed, total);

    return (passed == total) ? 0 : 1;
}

#!/usr/bin/env bash
# =============================================================================
# run_fpga_software_tests.sh
# Run all software-level tests for the FPGA track (no Vivado/Vitis required).
# Execute from the repo root.
# =============================================================================

set -e
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PASS=0
FAIL=0
ERRORS=()

green()  { printf "\033[32m%s\033[0m\n" "$*"; }
red()    { printf "\033[31m%s\033[0m\n" "$*"; }
header() { printf "\n\033[1m=== %s ===\033[0m\n" "$*"; }

run_test() {
    local name="$1"; shift
    if "$@"; then
        green "  PASS: $name"
        PASS=$((PASS + 1))
    else
        red   "  FAIL: $name"
        FAIL=$((FAIL + 1))
        ERRORS+=("$name")
    fi
}

cd "$REPO_ROOT"

# ---------------------------------------------------------------------------
header "1. Python reference + test vector generation"
run_test "bitlinear_reference.py" python reference/bitlinear_reference.py

# ---------------------------------------------------------------------------
header "2. Python packing utilities"
run_test "packing_utils.py" python reference/packing_utils.py

# ---------------------------------------------------------------------------
header "3. C++ reference — build and cross-validate vs Python test vectors"
TMP_REF=/tmp/bitlinear_ref_test_$$
g++ -O2 -std=c++17 \
    fpga_erven/hls/reference/bitlinear_reference.cpp \
    fpga_erven/hls/reference/bitlinear_reference_test.cpp \
    -o "$TMP_REF"
run_test "C++ reference (21 tests)" "$TMP_REF" "$REPO_ROOT/reference/test_vectors/"
rm -f "$TMP_REF"

# ---------------------------------------------------------------------------
header "4. C++ 2-bit packing tests"
PACKING_DIR="$REPO_ROOT/fpga_erven/hls/packing/tests"
TMP_PACK=/tmp/test_packing_$$
g++ -O2 -std=c++17 \
    -I "$REPO_ROOT/fpga_erven/hls/packing" \
    "$PACKING_DIR/test_packing.cpp" \
    "$REPO_ROOT/fpga_erven/hls/packing/pack_ternary_2bit.cpp" \
    "$REPO_ROOT/fpga_erven/hls/packing/unpack_ternary_2bit.cpp" \
    -o "$TMP_PACK"
run_test "2-bit packing (10 tests)" "$TMP_PACK"
rm -f "$TMP_PACK"

# ---------------------------------------------------------------------------
header "5. HLS kernel — software simulation (g++)"
BL_DIR="$REPO_ROOT/fpga_erven/hls/bitlinear"
PACK_SRC="$REPO_ROOT/fpga_erven/hls/packing/pack_ternary_2bit.cpp $REPO_ROOT/fpga_erven/hls/packing/unpack_ternary_2bit.cpp"

TMP_TB=/tmp/testbench_$$
g++ -O2 -std=c++14 \
    -I "$BL_DIR" \
    -I "$REPO_ROOT/fpga_erven/hls/packing" \
    "$BL_DIR/testbench.cpp" \
    "$BL_DIR/bitlinear_hls.cpp" \
    $PACK_SRC \
    -o "$TMP_TB"
run_test "HLS kernel inline testbench (10 tests)" "$TMP_TB"
rm -f "$TMP_TB"

TMP_VEC=/tmp/testbench_week2_$$
g++ -O2 -std=c++14 \
    -I "$BL_DIR" \
    -I "$REPO_ROOT/fpga_erven/hls/packing" \
    "$BL_DIR/testbench_week2_vectors.cpp" \
    "$BL_DIR/bitlinear_hls.cpp" \
    $PACK_SRC \
    -o "$TMP_VEC"
run_test "HLS kernel vs Week 2 vectors (10 tests)" \
    "$TMP_VEC" "$REPO_ROOT/reference/test_vectors/"
rm -f "$TMP_VEC"

# ---------------------------------------------------------------------------
header "Summary"
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
if [ "$FAIL" -eq 0 ]; then
    green "  ALL TESTS PASSED"
    exit 0
else
    red   "  FAILED: ${ERRORS[*]}"
    exit 1
fi

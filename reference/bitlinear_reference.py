"""
BitLinear Reference Implementation — Python
===========================================
Project : BitLinear-FPGA Alpha (Erven LE BIVIC)
Week    : 2
Purpose : CPU golden reference for int8 activation × ternary weight → int32 output.
          All hardware results (HLS, FPGA) must match this output exactly.

Operation:
    y[m] = Σ_k  W[m,k] * x[k]
    where x ∈ int8, W ∈ {-1, 0, +1}, y ∈ int32
"""

import numpy as np
import struct
import json
import os
from pathlib import Path


# ---------------------------------------------------------------------------
# Core reference function
# ---------------------------------------------------------------------------

def bitlinear_reference(x: np.ndarray, W: np.ndarray) -> np.ndarray:
    """
    Compute BitLinear: y = W @ x using ternary weights.

    Parameters
    ----------
    x : np.ndarray, shape [K], dtype int8
        Activation vector.
    W : np.ndarray, shape [M, K], dtype int8, values in {-1, 0, +1}
        Ternary weight matrix.

    Returns
    -------
    y : np.ndarray, shape [M], dtype int32
        Output accumulator vector.
    """
    assert x.ndim == 1,               "x must be a 1-D vector"
    assert W.ndim == 2,               "W must be a 2-D matrix"
    assert x.dtype == np.int8,        f"x dtype must be int8, got {x.dtype}"
    assert W.dtype == np.int8,        f"W dtype must be int8, got {W.dtype}"
    assert set(np.unique(W)).issubset({-1, 0, 1}), \
        f"W values must be in {{-1, 0, +1}}, found {np.unique(W)}"

    M, K = W.shape
    assert x.shape[0] == K, f"Shape mismatch: x has {x.shape[0]} elements, W has K={K}"

    y = np.zeros(M, dtype=np.int32)
    for m in range(M):
        acc = np.int32(0)
        for k in range(K):
            w = W[m, k]
            if w == 1:
                acc += np.int32(x[k])
            elif w == -1:
                acc -= np.int32(x[k])
        y[m] = acc
    return y


def bitlinear_reference_fast(x: np.ndarray, W: np.ndarray) -> np.ndarray:
    """
    Vectorised equivalent of bitlinear_reference (for large stress tests).
    Used only to cross-check the scalar version; not the golden reference itself.
    """
    assert x.dtype == np.int8 and W.dtype == np.int8
    return W.astype(np.int32) @ x.astype(np.int32)


# ---------------------------------------------------------------------------
# 2-bit ternary packing / unpacking
# ---------------------------------------------------------------------------
# Encoding:  00 = 0   01 = +1   10 = -1   11 = reserved
# 4 weights packed per byte, LSB-first: byte = [w3|w2|w1|w0]

def encode_weight(w: int) -> int:
    if w == 0:   return 0b00
    if w == 1:   return 0b01
    if w == -1:  return 0b10
    raise ValueError(f"Invalid ternary weight: {w}")


def decode_weight(code: int) -> int:
    if code == 0b00: return  0
    if code == 0b01: return  1
    if code == 0b10: return -1
    return 0  # 0b11 reserved → treat as 0


def pack_ternary_2bit(W_flat: np.ndarray) -> np.ndarray:
    """
    Pack a flat int8 ternary array into 2-bit packed uint8 array.
    Input length must be a multiple of 4.
    """
    assert W_flat.dtype == np.int8
    n = len(W_flat)
    pad = (4 - n % 4) % 4
    if pad:
        W_flat = np.concatenate([W_flat, np.zeros(pad, dtype=np.int8)])
    n_bytes = len(W_flat) // 4
    packed = np.zeros(n_bytes, dtype=np.uint8)
    for i in range(n_bytes):
        byte = 0
        for j in range(4):
            code = encode_weight(int(W_flat[i * 4 + j]))
            byte |= (code << (j * 2))
        packed[i] = byte
    return packed


def unpack_ternary_2bit(packed: np.ndarray, n_weights: int) -> np.ndarray:
    """
    Unpack a uint8 2-bit packed array back to int8 ternary array.
    """
    assert packed.dtype == np.uint8
    result = np.zeros(len(packed) * 4, dtype=np.int8)
    for i, byte in enumerate(packed):
        for j in range(4):
            code = (byte >> (j * 2)) & 0b11
            result[i * 4 + j] = decode_weight(code)
    return result[:n_weights]


# ---------------------------------------------------------------------------
# Test vector generation and I/O
# ---------------------------------------------------------------------------

def save_test_vectors(x: np.ndarray, W: np.ndarray, y: np.ndarray,
                      output_dir: str, label: str = "default") -> None:
    """Save a complete test vector set (activation, weights, output) to binary files."""
    out = Path(output_dir) / label
    out.mkdir(parents=True, exist_ok=True)

    M, K = W.shape
    W_packed = pack_ternary_2bit(W.flatten())

    x.tofile(str(out / "activation_int8.bin"))
    W.flatten().tofile(str(out / "weight_ternary_unpacked.bin"))
    W_packed.tofile(str(out / "weight_ternary_packed_2bit.bin"))
    y.tofile(str(out / "expected_output_int32.bin"))

    meta = {
        "label": label,
        "M": M,
        "K": K,
        "activation_dtype": "int8",
        "weight_dtype": "ternary",
        "weight_encoding": "2bit",
        "output_dtype": "int32",
        "n_packed_bytes": int(len(W_packed))
    }
    with open(str(out / "metadata.json"), "w") as f:
        json.dump(meta, f, indent=2)

    print(f"  [saved] {out}")


def load_test_vectors(test_dir: str):
    """Load test vectors from binary files. Returns (x, W, y_expected, meta)."""
    d = Path(test_dir)
    with open(str(d / "metadata.json")) as f:
        meta = json.load(f)
    M, K = meta["M"], meta["K"]
    x = np.fromfile(str(d / "activation_int8.bin"), dtype=np.int8)
    W = np.fromfile(str(d / "weight_ternary_unpacked.bin"), dtype=np.int8).reshape(M, K)
    y = np.fromfile(str(d / "expected_output_int32.bin"), dtype=np.int32)
    return x, W, y, meta


# ---------------------------------------------------------------------------
# Unit tests (10 required cases)
# ---------------------------------------------------------------------------

def run_all_tests(vector_dir: str = "test_vectors") -> bool:
    """
    Run all 10 required unit tests.
    Returns True if all pass, False otherwise.
    """
    rng = np.random.default_rng(42)

    def rand_ternary(shape):
        return rng.choice(np.array([-1, 0, 1], dtype=np.int8), size=shape)

    def rand_x(K):
        return rng.integers(-128, 127, size=K, dtype=np.int8)

    results = {}

    # -----------------------------------------------------------------------
    # Test 1 — Random x, random W
    # -----------------------------------------------------------------------
    K, M = 64, 32
    x = rand_x(K)
    W = rand_ternary((M, K))
    y_ref  = bitlinear_reference(x, W)
    y_fast = bitlinear_reference_fast(x, W)
    ok = np.array_equal(y_ref, y_fast)
    results["Test 1 — random x, random W"] = ok
    if ok:
        save_test_vectors(x, W, y_ref, vector_dir, "test01_random")

    # -----------------------------------------------------------------------
    # Test 2 — All W = 0  →  y must be all zeros
    # -----------------------------------------------------------------------
    W = np.zeros((M, K), dtype=np.int8)
    y = bitlinear_reference(x, W)
    ok = np.all(y == 0)
    results["Test 2 — all W = 0"] = ok
    if ok:
        save_test_vectors(x, W, y, vector_dir, "test02_W_zero")

    # -----------------------------------------------------------------------
    # Test 3 — All W = +1  →  y[m] = sum(x)
    # -----------------------------------------------------------------------
    W = np.ones((M, K), dtype=np.int8)
    y = bitlinear_reference(x, W)
    expected = np.full(M, int(x.astype(np.int32).sum()), dtype=np.int32)
    ok = np.array_equal(y, expected)
    results["Test 3 — all W = +1"] = ok
    if ok:
        save_test_vectors(x, W, y, vector_dir, "test03_W_plus1")

    # -----------------------------------------------------------------------
    # Test 4 — All W = -1  →  y[m] = -sum(x)
    # -----------------------------------------------------------------------
    W = np.full((M, K), -1, dtype=np.int8)
    y = bitlinear_reference(x, W)
    expected = np.full(M, -int(x.astype(np.int32).sum()), dtype=np.int32)
    ok = np.array_equal(y, expected)
    results["Test 4 — all W = -1"] = ok
    if ok:
        save_test_vectors(x, W, y, vector_dir, "test04_W_minus1")

    # -----------------------------------------------------------------------
    # Test 5 — Sparse W (≈10% non-zero)
    # -----------------------------------------------------------------------
    W_dense = rand_ternary((M, K))
    mask = rng.random((M, K)) < 0.10
    W = np.where(mask, W_dense, np.int8(0)).astype(np.int8)
    y = bitlinear_reference(x, W)
    y_fast = bitlinear_reference_fast(x, W)
    ok = np.array_equal(y, y_fast)
    results["Test 5 — sparse W (~10% nonzero)"] = ok
    if ok:
        save_test_vectors(x, W, y, vector_dir, "test05_sparse")

    # -----------------------------------------------------------------------
    # Test 6 — Dense W (≈90% non-zero)
    # -----------------------------------------------------------------------
    mask = rng.random((M, K)) < 0.90
    W = np.where(mask, rand_ternary((M, K)), np.int8(0)).astype(np.int8)
    y = bitlinear_reference(x, W)
    y_fast = bitlinear_reference_fast(x, W)
    ok = np.array_equal(y, y_fast)
    results["Test 6 — dense W (~90% nonzero)"] = ok
    if ok:
        save_test_vectors(x, W, y, vector_dir, "test06_dense")

    # -----------------------------------------------------------------------
    # Test 7 — x = maximum int8 values (+127)
    # -----------------------------------------------------------------------
    x_max = np.full(K, 127, dtype=np.int8)
    W = rand_ternary((M, K))
    y = bitlinear_reference(x_max, W)
    y_fast = bitlinear_reference_fast(x_max, W)
    ok = np.array_equal(y, y_fast)
    results["Test 7 — x = max int8 (+127)"] = ok
    if ok:
        save_test_vectors(x_max, W, y, vector_dir, "test07_xmax")

    # -----------------------------------------------------------------------
    # Test 8 — x = minimum int8 values (-128)
    # -----------------------------------------------------------------------
    x_min = np.full(K, -128, dtype=np.int8)
    y = bitlinear_reference(x_min, W)
    y_fast = bitlinear_reference_fast(x_min, W)
    ok = np.array_equal(y, y_fast)
    results["Test 8 — x = min int8 (-128)"] = ok
    if ok:
        save_test_vectors(x_min, W, y, vector_dir, "test08_xmin")

    # -----------------------------------------------------------------------
    # Test 9 — Small matrix manually verified
    #   x = [1, -2, 3]
    #   W = [[1, 0, -1],   → y[0] = 1*1 + 0*(-2) + (-1)*3 = -2
    #        [0, 1,  1]]   → y[1] = 0*1 + 1*(-2) +   1 *3 =  1
    # -----------------------------------------------------------------------
    x_small = np.array([1, -2, 3], dtype=np.int8)
    W_small = np.array([[1, 0, -1], [0, 1, 1]], dtype=np.int8)
    y = bitlinear_reference(x_small, W_small)
    ok = np.array_equal(y, np.array([-2, 1], dtype=np.int32))
    results["Test 9 — small matrix manual check"] = ok
    if ok:
        save_test_vectors(x_small, W_small, y, vector_dir, "test09_manual")

    # -----------------------------------------------------------------------
    # Test 10 — Large matrix stress test (M=256, K=512)
    # -----------------------------------------------------------------------
    M2, K2 = 256, 512
    x_large = rand_x(K2)
    W_large = rand_ternary((M2, K2))
    y = bitlinear_reference_fast(x_large, W_large)   # vectorised for speed
    y_check = bitlinear_reference(x_large, W_large[:4])  # scalar on first 4 rows
    ok = np.array_equal(y[:4], y_check)
    results["Test 10 — large matrix stress (256×512)"] = ok
    if ok:
        save_test_vectors(x_large, W_large, y, vector_dir, "test10_large")

    # -----------------------------------------------------------------------
    # Packing round-trip test (≥1000 random matrices)
    # -----------------------------------------------------------------------
    pack_ok = True
    for _ in range(1000):
        W_rt = rand_ternary((8, 8))
        flat = W_rt.flatten()
        packed = pack_ternary_2bit(flat)
        recovered = unpack_ternary_2bit(packed, len(flat)).reshape(8, 8)
        if not np.array_equal(W_rt, recovered):
            pack_ok = False
            break
    results["Pack round-trip (1000 random matrices)"] = pack_ok

    # -----------------------------------------------------------------------
    # Print summary
    # -----------------------------------------------------------------------
    print("\n" + "=" * 60)
    print("  BitLinear Python Reference — Test Results")
    print("=" * 60)
    all_pass = True
    for name, ok in results.items():
        status = "PASS" if ok else "FAIL"
        marker = "✓" if ok else "✗"
        print(f"  {marker} [{status}]  {name}")
        if not ok:
            all_pass = False
    print("=" * 60)
    print(f"  Overall: {'ALL PASS ✓' if all_pass else 'SOME TESTS FAILED ✗'}")
    print("=" * 60 + "\n")
    return all_pass


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import sys

    vector_dir = "test_vectors"
    os.makedirs(vector_dir, exist_ok=True)

    print("Running BitLinear reference tests and generating test vectors...")
    print(f"Output directory: {os.path.abspath(vector_dir)}\n")

    success = run_all_tests(vector_dir)
    sys.exit(0 if success else 1)

"""
packing_utils.py
================
Project : BitLinear-FPGA Alpha (Erven LE BIVIC)
Week    : 2
Purpose : Standalone 2-bit ternary weight packing/unpacking utilities.
          Used by both the Python reference and the HLS testbench generator.

Encoding:
    00 = 0
    01 = +1
    10 = -1
    11 = reserved (treated as 0)

Layout: 4 weights per byte, LSB-first
    byte = [w3 | w2 | w1 | w0]
    each w uses 2 bits

This code was generated with AI assistance.
Verified by: running round-trip tests on 1000+ random matrices and
cross-checking against the C++ implementation (pack_ternary_2bit / unpack_ternary_2bit).
"""

import numpy as np


# ---------------------------------------------------------------------------
# Single-weight encode / decode
# ---------------------------------------------------------------------------

def encode_weight(w: int) -> int:
    """
    Encode a single ternary weight into a 2-bit code.

    Parameters
    ----------
    w : int
        Ternary weight value: must be -1, 0, or +1.

    Returns
    -------
    int
        2-bit code: 0b00, 0b01, or 0b10.
    """
    if w ==  0: return 0b00
    if w ==  1: return 0b01
    if w == -1: return 0b10
    raise ValueError(f"Invalid ternary weight: {w}. Must be -1, 0, or +1.")


def decode_weight(code: int) -> int:
    """
    Decode a 2-bit code back to a ternary weight.

    Parameters
    ----------
    code : int
        2-bit code (0b00, 0b01, 0b10, or 0b11).

    Returns
    -------
    int
        Ternary weight: 0, +1, or -1.
        0b11 (reserved) is treated as 0.
    """
    code = code & 0b11
    if code == 0b00: return  0
    if code == 0b01: return  1
    if code == 0b10: return -1
    return 0  # 0b11 reserved → 0


# ---------------------------------------------------------------------------
# Array pack / unpack
# ---------------------------------------------------------------------------

def pack_ternary_2bit(W_flat: np.ndarray) -> np.ndarray:
    """
    Pack a flat int8 ternary array into a 2-bit packed uint8 array.

    4 weights are stored per byte, LSB-first:
        byte = [w3 | w2 | w1 | w0]

    If len(W_flat) is not a multiple of 4, zero-padding is added.

    Parameters
    ----------
    W_flat : np.ndarray
        1-D int8 array of ternary weights (values in {-1, 0, +1}).

    Returns
    -------
    np.ndarray
        1-D uint8 array of packed weights, length = ceil(len(W_flat) / 4).
    """
    assert W_flat.ndim == 1,          "W_flat must be 1-D"
    assert W_flat.dtype == np.int8,   f"W_flat dtype must be int8, got {W_flat.dtype}"

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
    Unpack a uint8 2-bit packed array back to an int8 ternary array.

    Parameters
    ----------
    packed : np.ndarray
        1-D uint8 array of packed weights.
    n_weights : int
        Number of weights to recover (original length before packing).

    Returns
    -------
    np.ndarray
        1-D int8 array of ternary weights, length = n_weights.
    """
    assert packed.ndim == 1,         "packed must be 1-D"
    assert packed.dtype == np.uint8, f"packed dtype must be uint8, got {packed.dtype}"

    result = np.zeros(len(packed) * 4, dtype=np.int8)
    for i, byte in enumerate(packed):
        for j in range(4):
            code = (byte >> (j * 2)) & 0b11
            result[i * 4 + j] = decode_weight(code)

    return result[:n_weights]


def pack_matrix(W: np.ndarray) -> np.ndarray:
    """
    Convenience wrapper: pack a 2-D ternary weight matrix row-major.

    Parameters
    ----------
    W : np.ndarray
        2-D int8 array of shape [M, K], values in {-1, 0, +1}.

    Returns
    -------
    np.ndarray
        1-D uint8 packed array, length = ceil(M * K / 4).
    """
    assert W.ndim == 2, "W must be 2-D"
    return pack_ternary_2bit(W.flatten())


def unpack_matrix(packed: np.ndarray, M: int, K: int) -> np.ndarray:
    """
    Convenience wrapper: unpack a packed array back to a 2-D matrix.

    Parameters
    ----------
    packed : np.ndarray
        1-D uint8 packed array.
    M, K : int
        Output matrix shape.

    Returns
    -------
    np.ndarray
        2-D int8 array of shape [M, K].
    """
    flat = unpack_ternary_2bit(packed, M * K)
    return flat.reshape(M, K)


# ---------------------------------------------------------------------------
# Self-test
# ---------------------------------------------------------------------------

def _run_tests() -> bool:
    rng = np.random.default_rng(42)
    all_pass = True

    def rand_ternary(shape):
        return rng.choice(np.array([-1, 0, 1], dtype=np.int8), size=shape)

    # Test 1: single weight round-trip
    for w in [-1, 0, 1]:
        assert decode_weight(encode_weight(w)) == w
    print("  ✓ [PASS]  Single weight encode/decode")

    # Test 2: flat array round-trip
    W_flat = rand_ternary(64)
    packed = pack_ternary_2bit(W_flat)
    recovered = unpack_ternary_2bit(packed, 64)
    ok = np.array_equal(W_flat, recovered)
    print(f"  {'✓' if ok else '✗'} [{'PASS' if ok else 'FAIL'}]  Flat array round-trip (n=64)")
    all_pass &= ok

    # Test 3: non-multiple-of-4 length
    W_odd = rand_ternary(13)
    packed = pack_ternary_2bit(W_odd)
    recovered = unpack_ternary_2bit(packed, 13)
    ok = np.array_equal(W_odd, recovered)
    print(f"  {'✓' if ok else '✗'} [{'PASS' if ok else 'FAIL'}]  Non-multiple-of-4 length (n=13)")
    all_pass &= ok

    # Test 4: matrix round-trip
    W = rand_ternary((32, 64))
    packed = pack_matrix(W)
    recovered = unpack_matrix(packed, 32, 64)
    ok = np.array_equal(W, recovered)
    print(f"  {'✓' if ok else '✗'} [{'PASS' if ok else 'FAIL'}]  Matrix round-trip (32x64)")
    all_pass &= ok

    # Test 5: 1000 random matrices
    ok = True
    for _ in range(1000):
        W_rt = rand_ternary((8, 8))
        packed = pack_matrix(W_rt)
        recovered = unpack_matrix(packed, 8, 8)
        if not np.array_equal(W_rt, recovered):
            ok = False
            break
    print(f"  {'✓' if ok else '✗'} [{'PASS' if ok else 'FAIL'}]  1000 random matrix round-trips")
    all_pass &= ok

    # Test 6: encoding table verification
    assert encode_weight( 0) == 0b00
    assert encode_weight( 1) == 0b01
    assert encode_weight(-1) == 0b10
    assert decode_weight(0b00) ==  0
    assert decode_weight(0b01) ==  1
    assert decode_weight(0b10) == -1
    assert decode_weight(0b11) ==  0  # reserved → 0
    print("  ✓ [PASS]  Encoding table verification")

    return all_pass


if __name__ == "__main__":
    import sys
    print("\n============================================================")
    print("  packing_utils.py — Self-Test")
    print("============================================================")
    ok = _run_tests()
    print("============================================================")
    print(f"  Overall: {'ALL PASS ✓' if ok else 'SOME TESTS FAILED ✗'}")
    print("============================================================\n")
    sys.exit(0 if ok else 1)

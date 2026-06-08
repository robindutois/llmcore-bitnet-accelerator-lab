#!/usr/bin/env python3
"""
gen_test_vectors_h.py
Converts the 10 Week 2 binary test vectors into a C header file
that can be included in the Vitis standalone application.

Usage (run from repo root):
    python3 fpga_erven/ps_host/gen_test_vectors_h.py

Output:
    fpga_erven/ps_host/test_vectors_data.h
"""

import os
import json
import struct
import sys

REPO_ROOT   = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
TV_BASE     = os.path.join(REPO_ROOT, "reference", "test_vectors")
OUTPUT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "test_vectors_data.h")

TV_DIRS = [
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
]

def read_bin(path):
    with open(path, "rb") as f:
        return f.read()

def bytes_to_c_int8_array(name, data):
    vals = [struct.unpack_from("b", data, i)[0] for i in range(len(data))]
    lines = [f"static const int8_t {name}[{len(vals)}] = {{"]
    row = []
    for i, v in enumerate(vals):
        row.append(f"{v:4d}")
        if len(row) == 16 or i == len(vals) - 1:
            lines.append("    " + ", ".join(row) + ("," if i < len(vals) - 1 else ""))
            row = []
    lines.append("};")
    return "\n".join(lines)

def bytes_to_c_uint8_array(name, data):
    lines = [f"static const uint8_t {name}[{len(data)}] = {{"]
    row = []
    for i, v in enumerate(data):
        row.append(f"0x{v:02X}")
        if len(row) == 16 or i == len(data) - 1:
            lines.append("    " + ", ".join(row) + ("," if i < len(data) - 1 else ""))
            row = []
    lines.append("};")
    return "\n".join(lines)

def bytes_to_c_int32_array(name, data):
    n = len(data) // 4
    vals = list(struct.unpack_from(f"<{n}i", data))
    lines = [f"static const int32_t {name}[{n}] = {{"]
    row = []
    for i, v in enumerate(vals):
        row.append(f"{v:10d}")
        if len(row) == 8 or i == len(vals) - 1:
            lines.append("    " + ", ".join(row) + ("," if i < len(vals) - 1 else ""))
            row = []
    lines.append("};")
    return "\n".join(lines)

def main():
    print(f"Reading test vectors from: {TV_BASE}")

    sections = []
    meta_entries = []

    for idx, tv_dir in enumerate(TV_DIRS):
        path = os.path.join(TV_BASE, tv_dir)

        meta_path = os.path.join(path, "metadata.json")
        x_path    = os.path.join(path, "activation_int8.bin")
        w_path    = os.path.join(path, "weight_ternary_packed_2bit.bin")
        y_path    = os.path.join(path, "expected_output_int32.bin")

        for p in [meta_path, x_path, w_path, y_path]:
            if not os.path.exists(p):
                print(f"ERROR: Missing file: {p}", file=sys.stderr)
                sys.exit(1)

        with open(meta_path) as f:
            meta = json.load(f)
        M = meta["M"]
        K = meta["K"]
        n_packed = meta["n_packed_bytes"]

        x_data = read_bin(x_path)
        w_data = read_bin(w_path)
        y_data = read_bin(y_path)

        assert len(x_data) == K,        f"{tv_dir}: x size mismatch: {len(x_data)} != {K}"
        assert len(w_data) == n_packed,  f"{tv_dir}: W size mismatch: {len(w_data)} != {n_packed}"
        assert len(y_data) == M * 4,     f"{tv_dir}: y size mismatch: {len(y_data)} != {M*4}"

        tag = f"tv{idx+1:02d}"
        sections.append(f"/* --- {tv_dir}  M={M} K={K} --- */")
        sections.append(bytes_to_c_int8_array(f"x_{tag}",      x_data))
        sections.append(bytes_to_c_uint8_array(f"W_{tag}",     w_data))
        sections.append(bytes_to_c_int32_array(f"yref_{tag}",  y_data))
        sections.append("")

        meta_entries.append((tv_dir, M, K, n_packed, tag))
        print(f"  [{idx+1:2d}/10] {tv_dir:20s}  M={M:4d} K={K:4d}  "
              f"x={len(x_data):5d}B  W={len(w_data):7d}B  y={len(y_data):5d}B")

    # Build index arrays
    n = len(meta_entries)
    names_arr  = "static const char *TV_NAMES[TV_COUNT] = {\n    " + \
                 ", ".join(f'"{e[0]}"' for e in meta_entries) + "\n};"
    m_arr      = "static const int TV_M[TV_COUNT] = {" + \
                 ", ".join(str(e[1]) for e in meta_entries) + "};"
    k_arr      = "static const int TV_K[TV_COUNT] = {" + \
                 ", ".join(str(e[2]) for e in meta_entries) + "};"
    wpk_arr    = "static const int TV_W_BYTES[TV_COUNT] = {" + \
                 ", ".join(str(e[3]) for e in meta_entries) + "};"

    # Pointer arrays
    x_ptrs     = "static const int8_t  *TV_X[TV_COUNT]    = {" + \
                 ", ".join(f"x_{e[4]}"    for e in meta_entries) + "};"
    w_ptrs     = "static const uint8_t *TV_W[TV_COUNT]    = {" + \
                 ", ".join(f"W_{e[4]}"    for e in meta_entries) + "};"
    yref_ptrs  = "static const int32_t *TV_YREF[TV_COUNT] = {" + \
                 ", ".join(f"yref_{e[4]}" for e in meta_entries) + "};"

    header = f"""\
/**
 * test_vectors_data.h
 * Auto-generated by gen_test_vectors_h.py from reference/test_vectors/
 * DO NOT EDIT MANUALLY.
 *
 * Contains all 10 Week 2 binary test vectors as C arrays.
 * Included by run_bitlinear_standalone.c.
 */

#ifndef TEST_VECTORS_DATA_H
#define TEST_VECTORS_DATA_H

#include <stdint.h>

#define TV_COUNT 10

/* =========================================================
 * Per-test arrays (x: int8, W_packed: uint8, y_ref: int32)
 * ========================================================= */

{chr(10).join(sections)}
/* =========================================================
 * Index tables
 * ========================================================= */

{names_arr}
{m_arr}
{k_arr}
{wpk_arr}

{x_ptrs}
{w_ptrs}
{yref_ptrs}

#endif /* TEST_VECTORS_DATA_H */
"""

    with open(OUTPUT_PATH, "w") as f:
        f.write(header)

    size_kb = os.path.getsize(OUTPUT_PATH) / 1024
    print(f"\nGenerated: {OUTPUT_PATH}  ({size_kb:.1f} KB)")
    print("Include this file in your Vitis project alongside run_bitlinear_standalone.c")

if __name__ == "__main__":
    main()

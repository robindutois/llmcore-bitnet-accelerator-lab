#!/usr/bin/env bash
# Valide la version MULTI-COEUR (run_bitnet_mc) sur tous les vecteurs de reference.
# A lancer depuis tenstorrent_robin/ (build/run_bitnet_mc + kernels_mc/ accessibles).
set -u
BIN=./build/run_bitnet_mc
VEC_DIR=../reference/test_vectors
if [[ ! -x "$BIN" ]]; then echo "Binaire introuvable : $BIN (compile d'abord)"; exit 2; fi

declare -a PASS=() FAIL=()
for d in "$VEC_DIR"/test*/; do
    name=$(basename "$d")
    M=$(grep '"M"' "$d/metadata.json" | grep -o '[0-9]\+')
    K=$(grep '"K"' "$d/metadata.json" | grep -o '[0-9]\+')
    echo "=================================================="
    echo ">>> $name (M=$M K=$K)"
    "$BIN" "$d" "$M" "$K" 2>/dev/null | grep -E "Test (PASS|FAIL)|coeurs|PCC|MISMATCH"
    rc=${PIPESTATUS[0]}
    if [[ $rc -eq 0 ]]; then PASS+=("$name"); else FAIL+=("$name (rc=$rc)"); fi
done
echo "=================================================="
echo "RECAP multi-coeur : ${#PASS[@]} PASS / ${#FAIL[@]} FAIL"
[[ ${#PASS[@]} -gt 0 ]] && printf '  PASS: %s\n' "${PASS[@]}"
[[ ${#FAIL[@]} -gt 0 ]] && printf '  FAIL: %s\n' "${FAIL[@]}"
[[ ${#FAIL[@]} -eq 0 ]]

#!/usr/bin/env bash
# Lance run_bitnet sur tous les vecteurs de référence et affiche un récapitulatif.
# À exécuter depuis tenstorrent_robin/ (là où se trouve build/run_bitnet et kernels/).
set -u

BIN=./build/run_bitnet
VEC_DIR=../reference/test_vectors

if [[ ! -x "$BIN" ]]; then
    echo "Binaire introuvable : $BIN (compile d'abord)"; exit 2
fi

declare -a PASS=() FAIL=()

for d in "$VEC_DIR"/test*/; do
    name=$(basename "$d")
    # M et K sont lus directement depuis metadata.json (dimensions variables)
    M=$(grep '"M"' "$d/metadata.json" | grep -o '[0-9]\+')
    K=$(grep '"K"' "$d/metadata.json" | grep -o '[0-9]\+')
    if [[ -z "$M" || -z "$K" ]]; then FAIL+=("$name (metadata illisible)"); continue; fi

    echo "=================================================="
    echo ">>> $name (M=$M, K=$K)"
    "$BIN" "$d" "$M" "$K" 2>/dev/null | grep -E "Test (PASS|FAIL)|MISMATCH"
    rc=${PIPESTATUS[0]}
    if [[ $rc -eq 0 ]]; then PASS+=("$name"); else FAIL+=("$name (rc=$rc)"); fi
done

echo "=================================================="
echo "RÉCAPITULATIF : ${#PASS[@]} PASS / ${#FAIL[@]} FAIL"
[[ ${#PASS[@]} -gt 0 ]] && printf '  PASS: %s\n' "${PASS[@]}"
[[ ${#FAIL[@]} -gt 0 ]] && printf '  FAIL: %s\n' "${FAIL[@]}"
[[ ${#FAIL[@]} -eq 0 ]]

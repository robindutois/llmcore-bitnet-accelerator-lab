#!/usr/bin/env bash
# Sweep du benchmark unite matricielle (batch N=32).
# A lancer depuis tenstorrent_robin/ (build/run_bench_mm + kernels_mm/ accessibles).
set -u

BIN=./build/run_bench_mm
ITERS="${ITERS:-1000}"
N="${N:-32}"
CSV=benchmarks/bitlinear_mm_results.csv
if [[ ! -x "$BIN" ]]; then echo "Binaire introuvable : $BIN (compile d'abord)"; exit 2; fi

rm -f "$CSV"; mkdir -p benchmarks

# Memes (M,K) que le sweep scalaire, + des tailles hors de portee du scalaire
SIZES=(
  "32 64"
  "32 256"
  "64 128"
  "32 512"
  "64 256"
  "128 128"
  "256 512"    # taille test10 (hors limite du scalaire mono-tuile)
  "512 512"
)

for s in "${SIZES[@]}"; do
    read -r M K <<< "$s"
    echo "=================================================="
    echo ">>> M=$M K=$K N=$N (iters=$ITERS)"
    "$BIN" "$M" "$K" "$N" "$ITERS" 2>/dev/null | grep -E "M=|Latence|Debit|ligne"
done

echo "=================================================="
echo "CSV : $CSV"
[[ -f "$CSV" ]] && cat "$CSV"

#!/usr/bin/env bash
# Sweep de benchmark du BitLinear sur Blackhole (kernel scalaire mono-core).
# À lancer depuis tenstorrent_robin/ (build/run_bench + kernels/ accessibles).
# Contrainte kernel mono-tuile : poids packés = M*K/4 <= 4096  ->  M*K <= 16384.
set -u

BIN=./build/run_bench
ITERS="${ITERS:-2000}"
CSV=benchmarks/bitlinear_hw_results.csv

if [[ ! -x "$BIN" ]]; then echo "Binaire introuvable : $BIN (compile d'abord)"; exit 2; fi

# On repart d'un CSV propre pour ce sweep
rm -f "$CSV"
mkdir -p benchmarks

# Tailles (M K) couvrant du petit (dispatch-bound) au gros (compute-bound)
SIZES=(
  "32 64"
  "32 256"
  "64 128"
  "32 512"
  "64 256"
  "128 128"
)

for s in "${SIZES[@]}"; do
    read -r M K <<< "$s"
    echo "=================================================="
    echo ">>> M=$M K=$K (iters=$ITERS)"
    "$BIN" "$M" "$K" "$ITERS" 2>/dev/null | grep -E "M=|Latence|Debit|Overhead|ligne"
done

echo "=================================================="
echo "CSV : $CSV"
[[ -f "$CSV" ]] && cat "$CSV"

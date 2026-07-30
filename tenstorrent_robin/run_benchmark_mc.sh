#!/usr/bin/env bash
# Sweep du benchmark MULTI-COEUR : M croissant -> de plus en plus de coeurs Tensix engages.
# A lancer depuis tenstorrent_robin/ (build/run_bench_mc + kernels_mc/).
set -u
BIN=./build/run_bench_mc
ITERS="${ITERS:-500}"
N="${N:-32}"
K="${K:-512}"
CSV=benchmarks/bitlinear_mc_results.csv
if [[ ! -x "$BIN" ]]; then echo "Binaire introuvable : $BIN (compile d'abord)"; exit 2; fi
rm -f "$CSV"; mkdir -p benchmarks

# M croissant -> tuiles de sortie (Mt = M/32) reparties sur de plus en plus de coeurs
for M in 256 512 1024 2048 4096; do
    echo "=================================================="
    echo ">>> M=$M K=$K N=$N (iters=$ITERS)"
    "$BIN" "$M" "$K" "$N" "$ITERS" 2>/dev/null | grep -E "coeurs|Latence|Debit|ligne"
done
echo "=================================================="
echo "CSV : $CSV"; [[ -f "$CSV" ]] && cat "$CSV"

#!/bin/sh
# =============================================================================
# run_benchmark.sh — Week 8
# BitLinear-FPGA Alpha — Single-command benchmark automation (PetaLinux)
# Student : Erven LE BIVIC — Seoul National University
# =============================================================================
#
# Automates the complete benchmark pipeline on the ZCU106 board:
#   1. Verify the bitstream is loaded (fpga_manager state check)
#   2. Load the u-dma-buf driver
#   3. Run bench_scaling for the standard size sweep (Week 7 baseline)
#   4. Run bench_scaling for the Week 8 extended sweep (timing decomposition)
#   5. Collect results into a timestamped CSV
#
# Usage:
#   sudo ./run_benchmark.sh [BENCH_BINARY] [OUT_CSV]
#
# Defaults:
#   BENCH_BINARY  = ./bench_scaling   (must be in the same directory)
#   OUT_CSV       = results_week8.csv
#
# Requirements:
#   - Bitstream already loaded via load-bitstream before calling this script
#   - u-dma-buf module built and available for modprobe
#   - bench_scaling compiled for aarch64 (see below)
#
# Cross-compile bench_scaling on the PC before transfer:
#   (conda deactivate first, then:)
#   aarch64-linux-gnu-gcc -O2 -static -o bench_scaling bench_scaling.c
#   nc -l -p 5001 > bench_scaling       # on PC
#   cat bench_scaling | nc 192.168.50.20 5001   # on board
#   chmod +x bench_scaling
# =============================================================================

set -e

BENCH="${1:-./bench_scaling}"
OUT_CSV="${2:-results_week8.csv}"
FPGA_STATE_PATH="/sys/class/fpga_manager/fpga0/state"
UDMABUF_MODULE="u-dma-buf"

# ---- Colour output (safe even without a tty) ----
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'
info()  { printf "${GREEN}[INFO]${NC}  %s\n" "$*"; }
warn()  { printf "${YELLOW}[WARN]${NC}  %s\n" "$*"; }
error() { printf "${RED}[ERROR]${NC} %s\n" "$*" >&2; }

# =============================================================================
# Step 1 — Check bitstream state
# =============================================================================
info "Step 1/4 — Checking FPGA manager state..."
if [ ! -f "$FPGA_STATE_PATH" ]; then
    error "Cannot read $FPGA_STATE_PATH — is the fpga_manager driver loaded?"
    exit 1
fi
FPGA_STATE=$(cat "$FPGA_STATE_PATH")
if [ "$FPGA_STATE" != "operating" ]; then
    error "fpga_manager state = '$FPGA_STATE' (expected 'operating')"
    error "Run 'load-bitstream' first and wait until state = 'operating'."
    exit 1
fi
info "fpga_manager state = '$FPGA_STATE'  OK"

# =============================================================================
# Step 2 — Load u-dma-buf driver
# =============================================================================
info "Step 2/4 — Loading u-dma-buf driver..."
if lsmod 2>/dev/null | grep -q u_dma_buf; then
    info "u-dma-buf already loaded — skipping modprobe"
else
    if modprobe "$UDMABUF_MODULE" 2>/dev/null; then
        info "modprobe $UDMABUF_MODULE  OK"
    else
        warn "modprobe failed — trying insmod fallback..."
        KMOD_PATH=$(find /lib/modules -name "u-dma-buf.ko" 2>/dev/null | head -1)
        if [ -n "$KMOD_PATH" ]; then
            insmod "$KMOD_PATH" && info "insmod $KMOD_PATH  OK" || { error "insmod failed"; exit 1; }
        else
            error "u-dma-buf module not found — cannot continue"
            exit 1
        fi
    fi
fi

# Verify udmabuf devices appeared
for i in 0 1 2; do
    if [ ! -c "/dev/udmabuf$i" ]; then
        error "/dev/udmabuf$i not found — check device tree (system-user.dtsi)"
        exit 1
    fi
done
info "/dev/udmabuf0/1/2 present  OK"

# =============================================================================
# Step 3 — Check benchmark binary
# =============================================================================
info "Step 3/4 — Checking benchmark binary..."
if [ ! -x "$BENCH" ]; then
    error "Benchmark binary '$BENCH' not found or not executable"
    error "Cross-compile: aarch64-linux-gnu-gcc -O2 -static -o bench_scaling bench_scaling.c"
    exit 1
fi
info "Binary '$BENCH'  OK"

# =============================================================================
# Step 4 — Run benchmark and collect CSV
# =============================================================================
info "Step 4/4 — Running benchmark..."
TIMESTAMP=$(date +"%Y-%m-%d_%H-%M-%S")
TMP_CSV="/tmp/bench_out_${TIMESTAMP}.csv"

# bench_scaling writes its own CSV; capture stdout for the console log
"$BENCH" "$TMP_CSV"
BENCH_EXIT=$?

if [ $BENCH_EXIT -ne 0 ]; then
    error "bench_scaling exited with code $BENCH_EXIT — check output above"
    exit 1
fi

# Append a metadata comment block to the CSV (grep-friendly prefix #)
{
    printf "# BitLinear-FPGA Alpha — Week 8 benchmark results\n"
    printf "# Timestamp : %s\n" "$TIMESTAMP"
    printf "# Board     : ZCU106 (XCZU7EV)\n"
    printf "# Bitstream : %s\n" "$(cat /sys/class/fpga_manager/fpga0/name 2>/dev/null || echo 'unknown')"
    printf "# Kernel    : %s\n" "$(uname -r)"
    printf "#\n"
    cat "$TMP_CSV"
} > "$OUT_CSV"

rm -f "$TMP_CSV"

info "Results written to: $OUT_CSV"
info ""

# =============================================================================
# Summary
# =============================================================================
TOTAL=$(grep -c "^[0-9]" "$OUT_CSV" 2>/dev/null || echo 0)
PASSED=$(grep -c "PASS$" "$OUT_CSV" 2>/dev/null || echo 0)
FAILED=$(grep -c "FAIL$" "$OUT_CSV" 2>/dev/null || echo 0)

info "====================================================="
info "  Sizes run   : $TOTAL"
info "  PASS        : $PASSED"
info "  FAIL        : $FAILED"
if [ "$FAILED" -eq 0 ] && [ "$TOTAL" -gt 0 ]; then
    info "  Status      : ALL PASS"
else
    error "  Status      : FAILURES DETECTED"
    exit 1
fi
info "====================================================="
info "Done. Transfer CSV to PC:"
info "  PC listens : nc -l -p 5001 > $OUT_CSV"
info "  Board sends: cat $OUT_CSV | nc 192.168.50.20 5001"

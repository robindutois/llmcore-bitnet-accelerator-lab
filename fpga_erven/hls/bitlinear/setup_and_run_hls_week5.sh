#!/bin/bash
# =============================================================================
# setup_and_run_hls_week5.sh
# BitLinear-FPGA Alpha — Week 5
#
# Run this script from the directory containing run_hls.tcl:
#   cd /path/to/fpga_erven/hls/bitlinear
#   bash setup_and_run_hls_week5.sh
#
# This script:
#   1. Recreates all Vitis 2025.1 symlinks required on this machine
#   2. Sources the Vitis environment
#   3. Runs vitis_hls with run_hls_week5.tcl (co-sim + export enabled)
# =============================================================================

set -e

VITIS_ROOT="/tools/Xilinx/2025.1/Vitis"

echo "=== Step 1: Recreating Vitis symlinks (sudo required) ==="

# Fix 1: hls_startup.tcl
sudo ln -sfn \
  "$VITIS_ROOT/common/scripts/hls_startup.tcl" \
  "$VITIS_ROOT/scripts/vitis_hls/hls_startup.tcl"
echo "  [OK] hls_startup.tcl"

# Fix 2: features.tcl (new error seen in Week 5)
if [ ! -f "$VITIS_ROOT/lib/scripts/rdi/misc/features.tcl" ]; then
  sudo mkdir -p "$VITIS_ROOT/lib/scripts/rdi/misc"
  # Try to find features.tcl elsewhere in the installation
  FEATURES=$(find "$VITIS_ROOT" -name "features.tcl" 2>/dev/null | head -1)
  if [ -n "$FEATURES" ]; then
    sudo ln -sfn "$FEATURES" "$VITIS_ROOT/lib/scripts/rdi/misc/features.tcl"
    echo "  [OK] features.tcl -> $FEATURES"
  else
    echo "  [WARN] features.tcl not found anywhere — trying lnx64 path"
    LNXFEATURES=$(find /tools/Xilinx/2025.1 -name "features.tcl" 2>/dev/null | head -1)
    if [ -n "$LNXFEATURES" ]; then
      sudo ln -sfn "$LNXFEATURES" "$VITIS_ROOT/lib/scripts/rdi/misc/features.tcl"
      echo "  [OK] features.tcl -> $LNXFEATURES"
    else
      echo "  [ERROR] features.tcl not found — check Vivado installation too"
    fi
  fi
fi

# Fix 3: lnx64.o -> lnx64 tool symlinks
sudo mkdir -p "$VITIS_ROOT/lnx64.o/tools"

for tool in clang-3.1 clang-3.9-csynth gcc; do
  if [ ! -e "$VITIS_ROOT/lnx64.o/tools/$tool" ]; then
    sudo ln -sfn "$VITIS_ROOT/lnx64/tools/$tool" "$VITIS_ROOT/lnx64.o/tools/$tool"
    echo "  [OK] lnx64.o/tools/$tool"
  else
    echo "  [SKIP] lnx64.o/tools/$tool already exists"
  fi
done

# Fix 4: lnx64.o/lib -> lnx64/lib
if [ ! -e "$VITIS_ROOT/lnx64.o/lib" ]; then
  sudo ln -sfn "$VITIS_ROOT/lnx64/lib" "$VITIS_ROOT/lnx64.o/lib"
  echo "  [OK] lnx64.o/lib"
else
  echo "  [SKIP] lnx64.o/lib already exists"
fi

# Fix 5: /tools/batonroot GCC path
BATONROOT_GCC="/tools/batonroot/rodin/devkits/lnx64/gcc-8.3.0"
if [ ! -e "$BATONROOT_GCC" ]; then
  sudo mkdir -p "$(dirname $BATONROOT_GCC)"
  sudo ln -sfn "$VITIS_ROOT/lnx64/tools/gcc" "$BATONROOT_GCC"
  echo "  [OK] batonroot gcc symlink"
else
  echo "  [SKIP] batonroot gcc already exists"
fi

echo ""
echo "=== Step 2: Sourcing Vitis environment ==="
source "$VITIS_ROOT/settings64.sh"
export RDI_PLATFORM=lnx64.o
echo "  [OK] RDI_PLATFORM=$RDI_PLATFORM"

echo ""
echo "=== Step 3: Verifying we are in the right directory ==="
if [ ! -f "bitlinear_hls.cpp" ]; then
  echo "  [ERROR] bitlinear_hls.cpp not found."
  echo "  Run this script from fpga_erven/hls/bitlinear/"
  exit 1
fi
echo "  [OK] bitlinear_hls.cpp found"

echo ""
echo "=== Step 4: Running Vitis HLS (co-sim + export) ==="
echo "  Using: run_hls_week5.tcl"
echo "  This may take 10-20 minutes for RTL co-simulation."
echo ""
vitis_hls -f run_hls_week5.tcl

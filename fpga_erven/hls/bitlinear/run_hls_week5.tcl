# =============================================================================
# run_hls_week5.tcl
# Vitis HLS Script — BitLinear-FPGA Alpha — Week 5
# =============================================================================
# Usage:
#   /tools/Xilinx/2025.1/Vitis/bin/vitis-run --mode hls --tcl run_hls_week5.tcl
# =============================================================================

set project_name "bitlinear_hls_project"
set top_function  "bitlinear_hls"
set part          "xczu7ev-ffvc1156-2-e"
set clock_period  "10"

# Reset project to clear cached testbench (removes old testbench.cpp)
open_project -reset $project_name
set_top $top_function

# Design files
add_files bitlinear_hls.cpp
add_files bitlinear_hls.h

# Testbench files (cosim-compatible — no VLAs)
add_files -tb testbench_cosim.cpp
add_files -tb ../packing/pack_ternary_2bit.cpp
add_files -tb ../packing/pack_ternary_2bit.h
add_files -tb ../packing/unpack_ternary_2bit.cpp

open_solution -reset "solution1"
set_part $part
create_clock -period $clock_period -name default

# =============================================================================
# Step 0: Synthesis (required after open_project -reset)
# Identical to Week 4 — II=1, Fmax 136.99 MHz expected
# =============================================================================
puts ">>> Step 0: Synthesis..."
csynth_design
puts ">>> Synthesis done."

# =============================================================================
# Step 1: RTL Co-Simulation
# =============================================================================
puts ">>> Step 1: RTL Co-Simulation (XSim)..."
cosim_design -rtl verilog -tool xsim -trace_level none
puts ">>> Co-simulation done. Check: $project_name/solution1/sim/report/"

# =============================================================================
# Step 2: Export IP for Vivado block design
# =============================================================================
puts ">>> Step 2: Exporting IP..."
export_design \
    -format ip_catalog \
    -description "BitLinear HLS Accelerator — ternary int8 matvec" \
    -vendor "llmcore" \
    -library "hls" \
    -ipname "bitlinear_hls" \
    -version "1.0"

puts ""
puts ">>> Week 5 complete."
puts ">>> Co-sim report : $project_name/solution1/sim/report/"
puts ">>> Exported IP   : $project_name/solution1/impl/ip/"

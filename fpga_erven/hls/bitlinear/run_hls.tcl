# =============================================================================
# run_hls.tcl
# Vitis HLS Script — BitLinear-FPGA Alpha
# =============================================================================
# Usage inside Vitis HLS Tcl console:
#   source run_hls.tcl
#
# Or from command line:
#   vitis_hls -f run_hls.tcl
# =============================================================================

# --- Project setup ---
set project_name "bitlinear_hls_project"
set top_function  "bitlinear_hls"
set part          "xczu7ev-ffvc1156-2-e"   ;# ZCU106 FPGA part number
set clock_period  "10"                      ;# 10ns = 100 MHz target

# --- Clean and create project ---
open_project -reset $project_name
set_top $top_function

# --- Add HLS source files ---
add_files bitlinear_hls.cpp
add_files bitlinear_hls.h
add_files ../packing/pack_ternary_2bit.h
add_files ../packing/pack_ternary_2bit.cpp
add_files ../packing/unpack_ternary_2bit.cpp

# --- Add testbench ---
add_files -tb testbench.cpp

# --- Solution setup ---
open_solution -reset "solution1"
set_part $part
create_clock -period $clock_period -name default

# =============================================================================
# Step 1: C Simulation
# =============================================================================
puts ">>> Running C Simulation..."
csim_design -clean

# =============================================================================
# Step 2: Synthesis
# =============================================================================
puts ">>> Running HLS Synthesis..."
csynth_design

# =============================================================================
# Step 3: RTL Co-Simulation (optional — comment out if taking too long)
# =============================================================================
# puts ">>> Running RTL Co-Simulation..."
# cosim_design -trace_level all

# =============================================================================
# Step 4: Export IP (for Vivado block design)
# =============================================================================
# puts ">>> Exporting IP..."
# export_design -format ip_catalog -description "BitLinear HLS IP" -version "1.0"

puts ">>> HLS flow complete."
puts ">>> Check reports in: $project_name/solution1/syn/report/"

# =============================================================================
# create_block_design.tcl — Week 6
# Vivado Block Design Script — BitLinear-FPGA Alpha
# Student : Erven LE BIVIC — Seoul National University
# =============================================================================
#
# Creates the complete Vivado block design, runs implementation, and generates
# the bitstream for the ZCU106 board.
#
# Usage
# -----
#   cd fpga_erven/vitis_project/
#   vivado -mode batch -source create_block_design.tcl
#
# Or interactively:
#   vivado -mode tcl
#   source create_block_design.tcl
#
# Prerequisites
# -------------
#   - Vivado 2025.1 installed and sourced
#   - IP archive at:
#       ../hls/bitlinear/bitlinear_hls_project/solution1/impl/ip/
#     (contains component.xml for llmcore:hls:bitlinear_hls:1.0)
#
# Output
# ------
#   bitlinear_system/                  — Vivado project
#   bitlinear_system/bitlinear_system.xsa  — exported hardware for Vitis
#   bitlinear_system/bitlinear_system.runs/impl_1/
#       bitlinear_system_wrapper.bit   — bitstream for ZCU106
#
# AXI Address Map (as assigned by this script)
# ---------------------------------------------
#   s_axi_CTRL     : 0xA000_0000  64 KB  (M, K, ap_start/done)
#   s_axi_control  : 0xA001_0000  64 KB  (x/W/y physical addresses)
#
#   Update CTRL_PHYS_BASE and CONTROL_PHYS_BASE in run_bitlinear.cpp if
#   Vivado assigns different addresses (check: Address Editor → Data tab).
# =============================================================================

# =============================================================================
# Configuration — edit only if your toolchain layout differs
# =============================================================================

set PART          "xczu7ev-ffvc1156-2-e"
set BOARD_PART    "xilinx.com:zcu106:part0:2.6"
set PROJ_NAME     "bitlinear_system"
set BD_NAME       "bitlinear_system"
set IP_REPO_PATH  "../hls/bitlinear/bitlinear_hls_project/solution1/impl/ip"

# AXI-Lite base addresses for the BitLinear IP (must match run_bitlinear.cpp)
set CTRL_BASEADDR    0xA0000000
set CONTROL_BASEADDR 0xA0010000

# PL clock frequency (MHz) — must be achievable by PS PLL, 100 MHz is safe
set PL_CLK_MHZ 100

# =============================================================================
# Step 0 — Sanity check
# =============================================================================

set ip_repo_abs [file normalize $IP_REPO_PATH]
if {![file exists "$ip_repo_abs/component.xml"]} {
    puts "ERROR: IP component.xml not found at: $ip_repo_abs"
    puts "       Run run_hls.tcl first to export the IP."
    exit 1
}
puts ">>> IP repo found : $ip_repo_abs"

# =============================================================================
# Step 1 — Create Vivado project
# =============================================================================

create_project $PROJ_NAME ./$PROJ_NAME -part $PART -force
set_property board_part $BOARD_PART [current_project]

# Register the BitLinear HLS IP so the catalog can see it
set_property ip_repo_paths [list $ip_repo_abs] [current_project]
update_ip_catalog -rebuild

puts ">>> Project created : $PROJ_NAME  ($PART)"

# =============================================================================
# Step 2 — Create block design
# =============================================================================

create_bd_design $BD_NAME
current_bd_design $BD_NAME

# =============================================================================
# Step 3 — Zynq UltraScale+ MPSoC (PS)
# =============================================================================

puts ">>> Adding Zynq UltraScale+ MPSoC..."
set ps [create_bd_cell -type ip \
    -vlnv xilinx.com:ip:zynq_ultra_ps_e:3.5 zynq_ultra_ps_e_0]

# Apply ZCU106 board preset (DDR, MIO, clocks)
apply_bd_automation \
    -rule xilinx.com:bd_rule:zynq_ultra_ps_e \
    -config {apply_board_preset "1"} \
    [get_bd_cells zynq_ultra_ps_e_0]

set_property CONFIG.PSU__USE__M_AXI_GP1 {0} $ps

# Enable GP0 AXI master (PS → PL control path)
set_property CONFIG.PSU__USE__M_AXI_GP0 {1} $ps

# Enable HP0, HP1, HP2 AXI slave ports (PL → PS DDR read/write paths)
# PSU__USE__S_AXI_GP2 → S_AXI_HP0_FPD
# PSU__USE__S_AXI_GP3 → S_AXI_HP1_FPD
# PSU__USE__S_AXI_GP4 → S_AXI_HP2_FPD
set_property CONFIG.PSU__USE__S_AXI_GP2 {1} $ps
set_property CONFIG.PSU__USE__S_AXI_GP3 {1} $ps
set_property CONFIG.PSU__USE__S_AXI_GP4 {1} $ps

# 128-bit data width on HP ports (maximum bandwidth, matches DDR controller)
set_property CONFIG.PSU__SAXIGP2__DATA_WIDTH {128} $ps
set_property CONFIG.PSU__SAXIGP3__DATA_WIDTH {128} $ps
set_property CONFIG.PSU__SAXIGP4__DATA_WIDTH {128} $ps

# PL clock 0 at target frequency
set_property CONFIG.PSU__CRL_APB__PL0_REF_CTRL__FREQMHZ $PL_CLK_MHZ $ps

puts ">>> Zynq PS configured (GP0 master, HP0/HP1/HP2 slaves, ${PL_CLK_MHZ} MHz)"

# =============================================================================
# Step 4 — Processor System Reset
# =============================================================================

puts ">>> Adding proc_sys_reset..."
set rst [create_bd_cell -type ip \
    -vlnv xilinx.com:ip:proc_sys_reset:5.0 proc_sys_reset_0]

# Connect clock and reset from PS
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
               [get_bd_pins proc_sys_reset_0/slowest_sync_clk]
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_resetn0] \
               [get_bd_pins proc_sys_reset_0/ext_reset_in]

# =============================================================================
# Step 5 — BitLinear HLS IP
# =============================================================================

puts ">>> Adding BitLinear HLS IP (llmcore:hls:bitlinear_hls:1.0)..."
set bl [create_bd_cell -type ip \
    -vlnv llmcore:hls:bitlinear_hls:1.0 bitlinear_hls_0]

# Connect AP clock and reset
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
               [get_bd_pins bitlinear_hls_0/ap_clk]
connect_bd_net [get_bd_pins proc_sys_reset_0/peripheral_aresetn] \
               [get_bd_pins bitlinear_hls_0/ap_rst_n]

# =============================================================================
# Step 6 — AXI SmartConnect: PS GP0 → BitLinear AXI-Lite slaves
# =============================================================================
# One SmartConnect fanning out from the PS AXI master to two AXI-Lite slaves:
#   M00 → s_axi_CTRL    (ap_start/done, M, K)
#   M01 → s_axi_control (x, W_packed, y pointer registers)

puts ">>> Adding AXI SmartConnect for control path (1→2)..."
set smc_ctrl [create_bd_cell -type ip \
    -vlnv xilinx.com:ip:smartconnect:1.0 axi_smc_ctrl]
set_property CONFIG.NUM_SI {1} $smc_ctrl
set_property CONFIG.NUM_MI {2} $smc_ctrl

# Clock and reset for SmartConnect
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
               [get_bd_pins axi_smc_ctrl/aclk]
connect_bd_net [get_bd_pins proc_sys_reset_0/interconnect_aresetn] \
               [get_bd_pins axi_smc_ctrl/aresetn]

# PS GP0 → SmartConnect input
connect_bd_intf_net [get_bd_intf_pins zynq_ultra_ps_e_0/M_AXI_HPM0_FPD] \
                    [get_bd_intf_pins axi_smc_ctrl/S00_AXI]

# SmartConnect outputs → BitLinear AXI-Lite slaves
connect_bd_intf_net [get_bd_intf_pins axi_smc_ctrl/M00_AXI] \
                    [get_bd_intf_pins bitlinear_hls_0/s_axi_CTRL]
connect_bd_intf_net [get_bd_intf_pins axi_smc_ctrl/M01_AXI] \
                    [get_bd_intf_pins bitlinear_hls_0/s_axi_control]

# =============================================================================
# Step 7 — AXI SmartConnects: BitLinear AXI4 masters → PS HP slaves
# =============================================================================
# Three dedicated SmartConnects (one per HP port) for independent data paths.
# Each connects one BitLinear AXI4 master to one PS HP slave.
#
#   MEM_IN  → axi_smc_hp0 → S_AXI_HP0_FPD  (activation read)
#   MEM_W   → axi_smc_hp1 → S_AXI_HP1_FPD  (packed weight read)
#   MEM_OUT → axi_smc_hp2 → S_AXI_HP2_FPD  (output write)

foreach {smc_name bl_port hp_port} {
    axi_smc_hp0  m_axi_MEM_IN   S_AXI_HP0_FPD
    axi_smc_hp1  m_axi_MEM_W    S_AXI_HP1_FPD
    axi_smc_hp2  m_axi_MEM_OUT  S_AXI_HP2_FPD
} {
    puts ">>> Adding $smc_name ($bl_port → $hp_port)..."
    set smc [create_bd_cell -type ip \
        -vlnv xilinx.com:ip:smartconnect:1.0 $smc_name]
    set_property CONFIG.NUM_SI {1} $smc
    set_property CONFIG.NUM_MI {1} $smc

    connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
                   [get_bd_pins ${smc_name}/aclk]
    connect_bd_net [get_bd_pins proc_sys_reset_0/interconnect_aresetn] \
                   [get_bd_pins ${smc_name}/aresetn]

    # BitLinear AXI4 master → SmartConnect slave input
    connect_bd_intf_net [get_bd_intf_pins bitlinear_hls_0/${bl_port}] \
                        [get_bd_intf_pins ${smc_name}/S00_AXI]

    # SmartConnect master output → PS HP slave
    connect_bd_intf_net [get_bd_intf_pins ${smc_name}/M00_AXI] \
                        [get_bd_intf_pins zynq_ultra_ps_e_0/${hp_port}]
}

# =============================================================================
# Step 8 — Clock connections for HP ports
# =============================================================================
# The PS HP slave clocks must be connected to the PL clock domain.

connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
               [get_bd_pins zynq_ultra_ps_e_0/saxihp0_fpd_aclk]
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
               [get_bd_pins zynq_ultra_ps_e_0/saxihp1_fpd_aclk]
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
               [get_bd_pins zynq_ultra_ps_e_0/saxihp2_fpd_aclk]

# GP0 master clock
connect_bd_net [get_bd_pins zynq_ultra_ps_e_0/pl_clk0] \
               [get_bd_pins zynq_ultra_ps_e_0/maxihpm0_fpd_aclk]

# =============================================================================
# Step 9 — Address assignment
# =============================================================================

set_msg_config -id {BD 41-237} -new_severity {WARNING}
puts ">>> Assigning addresses..."
assign_bd_address

# Override AXI-Lite addresses to match run_bitlinear.cpp defines
# If the segment names differ in your Vivado version, open the Address Editor
# GUI and update CTRL_PHYS_BASE / CONTROL_PHYS_BASE in run_bitlinear.cpp.

puts ">>> s_axi_CTRL    → 0xA0000000  64 KB  (auto-assigned)"
puts ">>> s_axi_control → 0xA0010000  64 KB  (auto-assigned)"

# =============================================================================
# Step 10 — Validate block design
# =============================================================================

puts ">>> Validating block design..."
validate_bd_design
puts ">>> Block design valid."

save_bd_design

# =============================================================================
# Step 11 — Generate output products and create HDL wrapper
# =============================================================================

puts ">>> Generating output products..."
generate_target all [get_files ${BD_NAME}.bd]

make_wrapper -files [get_files ${BD_NAME}.bd] -top
add_files -norecurse \
    ./${PROJ_NAME}/${PROJ_NAME}.srcs/sources_1/bd/${BD_NAME}/hdl/${BD_NAME}_wrapper.v

set_property top ${BD_NAME}_wrapper [current_fileset]

puts ">>> HDL wrapper created."

# =============================================================================
# Step 12 — Synthesis
# =============================================================================

puts ">>> Launching synthesis..."
launch_runs synth_1 -jobs 4
wait_on_run synth_1

if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    puts "ERROR: Synthesis did not complete."
    exit 1
}
puts ">>> Synthesis complete."

# =============================================================================
# Step 13 — Implementation
# =============================================================================

puts ">>> Launching implementation..."
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1

if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    puts "ERROR: Implementation did not complete."
    exit 1
}
puts ">>> Implementation complete."

# =============================================================================
# Step 14 — Export hardware (XSA)
# =============================================================================

set xsa_path "./${PROJ_NAME}/${PROJ_NAME}.xsa"
write_hw_platform -fixed -include_bit -force -file $xsa_path

puts ""
puts "==================================================================="
puts "  Block design complete."
puts "==================================================================="
puts "  Bitstream : ./${PROJ_NAME}/${PROJ_NAME}.runs/impl_1/${BD_NAME}_wrapper.bit"
puts "  XSA       : $xsa_path"
puts ""
puts "  To program the ZCU106:"
puts "    1. Copy the .bit to SD card or use JTAG:"
puts "       open_hw_manager"
puts "       connect_hw_server"
puts "       open_hw_target"
puts "       program_hw_devices -bitstream <path>.bit [lindex [get_hw_devices] 0]"
puts ""
puts "  After programming, confirm addresses in Vivado Address Editor:"
puts "    s_axi_CTRL    should be 0x[format %08X $CTRL_BASEADDR]"
puts "    s_axi_control should be 0x[format %08X $CONTROL_BASEADDR]"
puts "  Update run_bitlinear.cpp if Vivado chose different values."
puts "==================================================================="

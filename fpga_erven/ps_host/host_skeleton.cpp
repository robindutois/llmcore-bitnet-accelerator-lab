// =============================================================================
// host_skeleton.cpp — Week 5
// PS Host Program Skeleton — BitLinear-FPGA Alpha
// =============================================================================
// Full execution deferred to Week 6 (board and bitstream required).
// This skeleton documents the intended flow and register map.
//
// AXI addresses will be filled from Vivado address editor after
// block design is complete in Week 6.
// =============================================================================

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ----------------------------------------------------------------------------
// AXI base addresses — to be filled after Vivado block design (Week 6)
// ----------------------------------------------------------------------------
#define BITLINEAR_CTRL_BASEADDR    0x00000000  // s_axi_CTRL
#define BITLINEAR_CONTROL_BASEADDR 0x00000000  // s_axi_control

// ap_ctrl_hs register offsets (standard HLS AXI-Lite control)
#define AP_CTRL_OFFSET   0x00  // bit0=ap_start, bit1=ap_done, bit2=ap_idle
#define AP_M_OFFSET      0x10  // scalar M
#define AP_K_OFFSET      0x18  // scalar K
#define AP_X_OFFSET      0x20  // base address of x[] in PS DDR
#define AP_W_OFFSET      0x2C  // base address of W_packed[] in PS DDR
#define AP_Y_OFFSET      0x38  // base address of y[] in PS DDR

// ----------------------------------------------------------------------------
// Sizes for test case: M=64, K=128
// ----------------------------------------------------------------------------
#define TEST_M 64
#define TEST_K 128
#define TEST_PACKED_SIZE ((TEST_M * TEST_K + 3) / 4)

// ----------------------------------------------------------------------------
// Prototype: run_bitlinear()
// Full implementation in Week 6 after bitstream is available.
// ----------------------------------------------------------------------------
int run_bitlinear(
    volatile uint32_t* ctrl_base,
    volatile uint32_t* control_base,
    int8_t*  x,
    uint8_t* W_packed,
    int32_t* y,
    int M, int K
) {
    // Step 1: Write base addresses of DDR buffers to AXI-Lite control regs
    // Step 2: Write M and K scalars
    // Step 3: Write ap_start = 1
    // Step 4: Poll ap_done
    // Step 5: Read y[] from DDR
    // Step 6: Return 0 on success
    printf("[host] run_bitlinear: not yet implemented (Week 6)\n");
    return -1;
}

int main() {
    printf("BitLinear PS Host — skeleton (Week 5)\n");
    printf("Full execution requires ZCU106 bitstream (Week 6).\n");
    return 0;
}

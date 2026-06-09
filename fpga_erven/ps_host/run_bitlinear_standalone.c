/*
 * run_bitlinear_standalone.c - Week 6
 * BitLinear PS Standalone Host - ZCU106
 * Erven LE BIVIC - Seoul National University - BitLinear-FPGA Alpha
 *
 * Build : Vitis 2025.1 standalone, Cortex-A53 #0, ZCU106 XSA
 * Run   : xsct run_jtag.tcl
 *
 * No UART needed: results are in g_passed / g_total / g_done (XSCT: stop + print)
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "xil_types.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "xparameters.h"

#include "test_vectors_data.h"

/* ARM generic timer - no BSP header needed, works in SDT and legacy flows */
typedef uint64_t XTime;

static inline XTime arch_timer_read(void)
{
    XTime v;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v) :: "memory");
    return v;
}

static inline uint64_t arch_timer_freq(void)
{
    uint64_t f;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f) :: "memory");
    return f;
}

static void arch_usleep(uint32_t us)
{
    uint64_t freq  = arch_timer_freq();
    XTime    start = arch_timer_read();
    XTime    end   = start + (XTime)us * (freq / 1000000ULL);
    while (arch_timer_read() < end);
}

/* AXI base addresses - must match Vivado Address Editor */
#define CTRL_BASEADDR     0xA0000000UL
#define CONTROL_BASEADDR  0xA0010000UL

/* Register offsets - from xbitlinear_hls_hw.h */
#define REG_AP_CTRL  0x00u
#define REG_M_DATA   0x10u
#define REG_K_DATA   0x18u

#define AP_START  (1U << 0)
#define AP_DONE   (1U << 1)
#define AP_IDLE   (1U << 2)

#define REG_X_LO  0x10u
#define REG_X_HI  0x14u
#define REG_W_LO  0x1Cu
#define REG_W_HI  0x20u
#define REG_Y_LO  0x28u
#define REG_Y_HI  0x2Cu

/* DMA buffers - global, cache-line aligned. VA == PA in standalone. */
#define MAX_M         512
#define MAX_K         1024
#define MAX_PACKED_SZ (MAX_M * MAX_K / 4)

static s8  dma_x[MAX_K]         __attribute__((aligned(64)));
static u8  dma_W[MAX_PACKED_SZ] __attribute__((aligned(64)));
static s32 dma_y[MAX_M]         __attribute__((aligned(64)));

/* Result sentinels - read via XSCT: stop / print g_passed / print g_total */
volatile int g_passed = 0;
volatile int g_total  = 0;
volatile int g_done   = 0;
volatile unsigned int g_total_us = 0;
volatile unsigned int g_avg_us   = 0;

#define POLL_TIMEOUT_US  5000000

/* Register helpers */
static inline void reg_wr(u32 base, u32 off, u32 val)
{
    Xil_Out32(base + off, val);
    __asm__ volatile("dsb sy" ::: "memory");
}

static inline u32 reg_rd(u32 base, u32 off)
{
    __asm__ volatile("dsb sy" ::: "memory");
    return Xil_In32(base + off);
}

static inline void reg_wr64(u32 base, u32 lo, u32 hi, u64 addr)
{
    reg_wr(base, lo, (u32)(addr & 0xFFFFFFFFULL));
    reg_wr(base, hi, (u32)(addr >> 32));
}

/* Inline CPU reference */
static inline s8 decode_w(u8 byte, int pos)
{
    u8 code = (byte >> (pos * 2)) & 0x3U;
    if (code == 1U) return  1;
    if (code == 2U) return -1;
    return 0;
}

static void cpu_bitlinear(const s8 *x, const u8 *W, s32 *y, int M, int K)
{
    int m, k;
    for (m = 0; m < M; m++) {
        s32 acc = 0;
        for (k = 0; k < K; k++) {
            int flat = m * K + k;
            acc += (s32)decode_w(W[flat / 4], flat % 4) * (s32)x[k];
        }
        y[m] = acc;
    }
}

/* Run IP, return elapsed ticks (0 on timeout) */
static XTime ip_run(int M, int K)
{
    XTime    t0, t1;
    uint64_t freq;
    XTime    timeout;

    reg_wr(CTRL_BASEADDR, REG_M_DATA, (u32)M);
    reg_wr(CTRL_BASEADDR, REG_K_DATA, (u32)K);
    reg_wr64(CONTROL_BASEADDR, REG_X_LO, REG_X_HI, (u64)(uintptr_t)dma_x);
    reg_wr64(CONTROL_BASEADDR, REG_W_LO, REG_W_HI, (u64)(uintptr_t)dma_W);
    reg_wr64(CONTROL_BASEADDR, REG_Y_LO, REG_Y_HI, (u64)(uintptr_t)dma_y);

    Xil_DCacheFlushRange((UINTPTR)dma_x, (u32)K);
    Xil_DCacheFlushRange((UINTPTR)dma_W, (u32)((M * K + 3) / 4));
    Xil_DCacheFlushRange((UINTPTR)dma_y, (u32)(M * (int)sizeof(s32)));

    freq    = arch_timer_freq();
    timeout = (XTime)POLL_TIMEOUT_US * (freq / 1000000ULL);
    t0      = arch_timer_read();

    reg_wr(CTRL_BASEADDR, REG_AP_CTRL, AP_START);

    while (1) {
        t1 = arch_timer_read();
        if (reg_rd(CTRL_BASEADDR, REG_AP_CTRL) & AP_DONE)
            break;
        if ((t1 - t0) > timeout) {
            xil_printf("[TIMEOUT] ap_done not set\r\n");
            return 0;
        }
    }

    Xil_DCacheInvalidateRange((UINTPTR)dma_y, (u32)(M * (int)sizeof(s32)));
    return (t1 - t0);
}

/* Test result struct */
typedef struct { int passed; int mm; XTime ticks; int M; int K; } TR;

static TR run_one(int idx)
{
    TR r;
    s32 y_cpu[MAX_M];
    int m;

    r.passed = 0; r.mm = 0; r.ticks = 0;
    r.M = TV_M[idx]; r.K = TV_K[idx];

    memcpy(dma_x, TV_X[idx], (size_t)r.K);
    memcpy(dma_W, TV_W[idx], (size_t)TV_W_BYTES[idx]);
    memset(dma_y, 0,         (size_t)(r.M * (int)sizeof(s32)));

    cpu_bitlinear(dma_x, dma_W, y_cpu, r.M, r.K);
    for (m = 0; m < r.M; m++) {
        if (y_cpu[m] != TV_YREF[idx][m])
            xil_printf("[WARN] CPU vs golden m=%d: %d != %d\r\n",
                       m, (int)y_cpu[m], (int)TV_YREF[idx][m]);
    }

    r.ticks = ip_run(r.M, r.K);
    if (r.ticks == 0) return r;

    for (m = 0; m < r.M; m++) {
        if (dma_y[m] != TV_YREF[idx][m]) {
            if (r.mm < 4)
                xil_printf("  [MM] m=%d fpga=%d ref=%d\r\n",
                           m, (int)dma_y[m], (int)TV_YREF[idx][m]);
            r.mm++;
        }
    }
    r.passed = (r.mm == 0);
    return r;
}

int main(void)
{
    int   i, total, passed;
    XTime total_ticks;
    uint64_t freq;

    arch_usleep(200000);

    xil_printf("\r\n=== BitLinear-FPGA Alpha - ZCU106 Standalone - Week 6 ===\r\n");
    xil_printf("CTRL=0x%08X  CTRL2=0x%08X\r\n",
               (unsigned)CTRL_BASEADDR, (unsigned)CONTROL_BASEADDR);

    {
        u32 ap = reg_rd(CTRL_BASEADDR, REG_AP_CTRL);
        if (ap & AP_IDLE)
            xil_printf("[ok] IP idle (ap_ctrl=0x%08X)\r\n", ap);
        else
            xil_printf("[WARN] IP not idle (ap_ctrl=0x%08X)\r\n", ap);
    }

    xil_printf("%-22s %5s %5s %10s %s\r\n",
               "Test","M","K","Lat(us)","Result");

    total       = 0;
    passed      = 0;
    total_ticks = 0;
    freq        = arch_timer_freq();

    for (i = 0; i < TV_COUNT; i++) {
        TR r   = run_one(i);
        u32 us = (freq > 0) ? (u32)(r.ticks / (freq / 1000000ULL)) : 0;
        total++;
        if (r.passed) passed++;
        total_ticks += r.ticks;
        xil_printf("%-22s %5d %5d %10u %s\r\n",
                   TV_NAMES[i], r.M, r.K, us,
                   r.passed       ? "PASS" :
                   (r.ticks == 0) ? "TIMEOUT" : "FAIL");
        if (!r.passed && r.mm > 0)
            xil_printf("  -> %d mismatch(es)\r\n", r.mm);
    }

       u32 tot_us = (freq > 0)
        ? (u32)(total_ticks / (freq / 1000000ULL)) : 0;
    u32 avg_us = (total > 0 && freq > 0)
        ? (u32)(total_ticks / ((XTime)total * (freq / 1000000ULL))) : 0;
    xil_printf("=== RESULT: %d/%d PASS  total=%uus  avg=%uus ===\r\n",
               passed, total, tot_us, avg_us);
    
    g_total_us = tot_us;
    g_avg_us   = avg_us;
    g_passed   = passed;
    g_total    = total;
    g_done     = (int) 0xDEADBEEF;

    while (1)
        __asm__ volatile("nop");

    return 0;
}
/*
 * run_bitlinear_linux.c - Week 6
 * BitLinear PS Linux Host - ZCU106
 * Erven LE BIVIC - Seoul National University - BitLinear-FPGA Alpha
 *
 * Build (on target or via SDK):
 *   gcc -O2 -o run-bitlinear run_bitlinear_linux.c -lrt
 *
 * Run (root required for /dev/mem):
 *   sudo ./run-bitlinear
 *
 * Requires udmabuf nodes in device tree (see system-user.dtsi).
 * Requires bitstream loaded (via petalinux-boot --jtag --fpga or XSCT).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>

#include "test_vectors_data.h"

/* ------------------------------------------------------------------ */
/* AXI base addresses — must match Vivado Address Editor              */
/* ------------------------------------------------------------------ */
#define CTRL_BASEADDR     0xA0000000UL   /* s_axi_CTRL  (M/K params) */
#define CONTROL_BASEADDR  0xA0010000UL   /* s_axi_CONTROL (buf ptrs) */
#define MAP_SIZE          0x10000UL

/* Register offsets (from xbitlinear_hls_hw.h) */
#define REG_AP_CTRL  0x00u
#define REG_M_DATA   0x10u
#define REG_K_DATA   0x18u
#define AP_START     (1U << 0)
#define AP_DONE      (1U << 1)
#define AP_IDLE      (1U << 2)
#define REG_X_LO     0x10u
#define REG_X_HI     0x14u
#define REG_W_LO     0x1Cu
#define REG_W_HI     0x20u
#define REG_Y_LO     0x28u
#define REG_Y_HI     0x2Cu

/* Buffer sizes */
#define MAX_M          512
#define MAX_K          1024
#define MAX_PACKED_SZ  (MAX_M * MAX_K / 4)   /* 131072 bytes */
#define POLL_TIMEOUT_US 5000000ULL

/* ------------------------------------------------------------------ */
/* Globals                                                             */
/* ------------------------------------------------------------------ */
static volatile uint32_t *ctrl_map  = NULL;   /* mmap of CTRL_BASEADDR    */
static volatile uint32_t *ctrl2_map = NULL;   /* mmap of CONTROL_BASEADDR */

static int8_t  *dma_x = NULL;   /* udmabuf0 virtual address */
static uint8_t *dma_W = NULL;   /* udmabuf1 virtual address */
static int32_t *dma_y = NULL;   /* udmabuf2 virtual address */
static uint64_t phys_x, phys_W, phys_y;

static int devmem_fd = -1;

/* ------------------------------------------------------------------ */
/* Register helpers                                                    */
/* ------------------------------------------------------------------ */
static inline void reg_wr(volatile uint32_t *base, uint32_t off, uint32_t val)
{
    base[off / 4] = val;
    __sync_synchronize();
}

static inline uint32_t reg_rd(volatile uint32_t *base, uint32_t off)
{
    __sync_synchronize();
    return base[off / 4];
}

static inline void reg_wr64(volatile uint32_t *base,
                             uint32_t lo, uint32_t hi, uint64_t addr)
{
    reg_wr(base, lo, (uint32_t)(addr & 0xFFFFFFFFULL));
    reg_wr(base, hi, (uint32_t)(addr >> 32));
}

/* ------------------------------------------------------------------ */
/* Timing                                                              */
/* ------------------------------------------------------------------ */
static uint64_t get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL
         + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ------------------------------------------------------------------ */
/* /dev/mem helper                                                     */
/* ------------------------------------------------------------------ */
static volatile uint32_t *map_phys(int fd, uint64_t phys, size_t size)
{
    void *ptr = mmap(NULL, size,
                     PROT_READ | PROT_WRITE, MAP_SHARED,
                     fd, (off_t)phys);
    if (ptr == MAP_FAILED) {
        perror("mmap /dev/mem");
        return NULL;
    }
    return (volatile uint32_t *)ptr;
}

/* ------------------------------------------------------------------ */
/* udmabuf helpers                                                     */
/* (driver: github.com/ikwzm/udmabuf, included in PetaLinux)          */
/* ------------------------------------------------------------------ */
static uint64_t udmabuf_phys_addr(int idx)
{
    char path[80];
    FILE *f;
    uint64_t addr = 0;
    snprintf(path, sizeof(path),
             "/sys/class/u-dma-buf/udmabuf%d/phys_addr", idx);
    f = fopen(path, "r");
    if (!f) { perror(path); return 0; }
    if (fscanf(f, "0x%lx", &addr) != 1)
        fprintf(stderr, "[WARN] could not parse phys_addr for udmabuf%d\n", idx);
    fclose(f);
    return addr;
}

/* Explicit DMA cache management for cacheable udmabuf mappings.
 * direction: 1 = sync_for_device (flush CPU writes to DDR before the IP reads)
 *            0 = sync_for_cpu    (invalidate CPU cache after the IP writes DDR)
 * The ikwzm udmabuf driver exposes these as sysfs attributes. Writing the
 * byte length to sync_size then triggering sync_for_* performs the op. */
static void udmabuf_sync(int idx, int for_device, size_t len)
{
    char path[96];
    FILE *f;

    snprintf(path, sizeof(path), "/sys/class/u-dma-buf/udmabuf%d/sync_offset", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "0"); fclose(f); }

    snprintf(path, sizeof(path), "/sys/class/u-dma-buf/udmabuf%d/sync_size", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "%zu", len); fclose(f); }

    snprintf(path, sizeof(path), "/sys/class/u-dma-buf/udmabuf%d/sync_direction", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "%d", for_device ? 1 : 2); fclose(f); }

    snprintf(path, sizeof(path),
             for_device ? "/sys/class/u-dma-buf/udmabuf%d/sync_for_device"
                        : "/sys/class/u-dma-buf/udmabuf%d/sync_for_cpu", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "1"); fclose(f); }
}

/* Keep the fds open for the lifetime of the process. Closing the fd right
 * after mmap can release the udmabuf mapping on some driver versions, which
 * surfaces as a SIGBUS the first time a page beyond the initially-faulted
 * one is touched. Small buffers (one page) never hit it; large buffers do. */
static int udmabuf_fd[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

static void *udmabuf_mmap(int idx, size_t size)
{
    char path[32];
    int fd;
    void *ptr;
    snprintf(path, sizeof(path), "/dev/udmabuf%d", idx);
    /* NOTE: do NOT pass O_SYNC here. With O_SYNC the udmabuf mapping becomes
     * non-cacheable device memory; glibc memset() then uses the "DC ZVA"
     * cache-zero instruction which faults (SIGBUS) on device memory. Mapping
     * it cacheable lets memset/memcpy work; DMA coherency is handled explicitly
     * via the udmabuf sync_for_device / sync_for_cpu sysfs controls. */
    fd = open(path, O_RDWR);
    if (fd < 0) { perror(path); return NULL; }
    ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) { perror("mmap udmabuf"); close(fd); return NULL; }
    if (idx >= 0 && idx < 8)
        udmabuf_fd[idx] = fd;   /* keep open; do not close */
    else
        close(fd);
    return ptr;
}

/* ------------------------------------------------------------------ */
/* CPU reference (identical to standalone)                             */
/* ------------------------------------------------------------------ */
static inline int8_t decode_w(uint8_t byte, int pos)
{
    uint8_t code = (byte >> (pos * 2)) & 0x3U;
    if (code == 1U) return  1;
    if (code == 2U) return -1;
    return 0;
}

static void cpu_bitlinear(const int8_t *x, const uint8_t *W,
                          int32_t *y, int M, int K)
{
    int m, k;
    for (m = 0; m < M; m++) {
        int32_t acc = 0;
        for (k = 0; k < K; k++) {
            int flat = m * K + k;
            acc += (int32_t)decode_w(W[flat / 4], flat % 4) * (int32_t)x[k];
        }
        y[m] = acc;
    }
}

/* ------------------------------------------------------------------ */
/* Per-test run                                                        */
/* ------------------------------------------------------------------ */
typedef struct { int passed; int mm; uint32_t us; int M; int K; } TR;

static TR run_one(int idx)
{
    TR r = {0, 0, 0, TV_M[idx], TV_K[idx]};
    int32_t y_cpu[MAX_M];
    uint64_t t0, t1;
    int m;

    memcpy(dma_x, TV_X[idx], (size_t)r.K);
    memcpy(dma_W, TV_W[idx], (size_t)TV_W_BYTES[idx]);
    memset(dma_y, 0,         (size_t)(r.M * (int)sizeof(int32_t)));

    /* CPU reference sanity check */
    cpu_bitlinear(dma_x, dma_W, y_cpu, r.M, r.K);
    for (m = 0; m < r.M; m++)
        if (y_cpu[m] != TV_YREF[idx][m])
            printf("[WARN] CPU vs golden m=%d: %d != %d\n",
                   m, y_cpu[m], TV_YREF[idx][m]);

    /* Write params and physical buffer addresses to IP */
    reg_wr(ctrl_map,  REG_M_DATA, (uint32_t)r.M);
    reg_wr(ctrl_map,  REG_K_DATA, (uint32_t)r.K);
    reg_wr64(ctrl2_map, REG_X_LO, REG_X_HI, phys_x);
    reg_wr64(ctrl2_map, REG_W_LO, REG_W_HI, phys_W);
    reg_wr64(ctrl2_map, REG_Y_LO, REG_Y_HI, phys_y);

    __sync_synchronize();   /* memory barrier before ap_start */

    /* Flush CPU-written inputs (x, W) out to DDR so the IP sees fresh data. */
    udmabuf_sync(0, 1, (size_t)r.K);
    udmabuf_sync(1, 1, (size_t)TV_W_BYTES[idx]);
    udmabuf_sync(2, 1, (size_t)(r.M * (int)sizeof(int32_t)));  /* push zeroed y */

    /* Start IP and poll ap_done */
    t0 = get_time_us();
    reg_wr(ctrl_map, REG_AP_CTRL, AP_START);

    while (1) {
        t1 = get_time_us();
        if (reg_rd(ctrl_map, REG_AP_CTRL) & AP_DONE) break;
        if ((t1 - t0) > POLL_TIMEOUT_US) {
            printf("[TIMEOUT] ap_done not set\n");
            return r;
        }
    }

    __sync_synchronize();   /* ensure DMA writes are visible */

    /* Invalidate CPU cache for y so we read what the IP wrote to DDR. */
    udmabuf_sync(2, 0, (size_t)(r.M * (int)sizeof(int32_t)));

    r.us = (uint32_t)(t1 - t0);

    /* Compare FPGA output vs golden reference */
    for (m = 0; m < r.M; m++) {
        if (dma_y[m] != TV_YREF[idx][m]) {
            if (r.mm < 4)
                printf("  [MM] m=%d  fpga=%d  ref=%d\n",
                       m, dma_y[m], TV_YREF[idx][m]);
            r.mm++;
        }
    }
    r.passed = (r.mm == 0);
    return r;
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void)
{
    int i, total = 0, passed = 0;
    uint64_t total_us = 0;
    uint32_t ap;

    printf("\n=== BitLinear-FPGA Alpha - ZCU106 Linux - Week 6 ===\n");
    printf("CTRL=0x%08X  CTRL2=0x%08X\n",
           (unsigned)CTRL_BASEADDR, (unsigned)CONTROL_BASEADDR);

    /* Open /dev/mem */
    devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem_fd < 0) { perror("/dev/mem"); return 1; }

    /* Map AXI control register spaces */
    ctrl_map  = map_phys(devmem_fd, CTRL_BASEADDR,    MAP_SIZE);
    ctrl2_map = map_phys(devmem_fd, CONTROL_BASEADDR, MAP_SIZE);
    if (!ctrl_map || !ctrl2_map) return 1;

    /* Check IP is idle before starting */
    ap = reg_rd(ctrl_map, REG_AP_CTRL);
    printf("[%s] IP ap_ctrl=0x%08X\n",
           (ap & AP_IDLE) ? "ok" : "WARN", ap);

    /* Map udmabuf DMA buffers. Map the FULL declared udmabuf size (page-aligned)
     * rather than the minimal logical size, so large transfers never touch an
     * unmapped page. */
    dma_x = udmabuf_mmap(0, (size_t)0x1000);    /* 4 KB, >= MAX_K */
    dma_W = udmabuf_mmap(1, (size_t)0x20000);   /* 128 KB = MAX_PACKED_SZ */
    dma_y = udmabuf_mmap(2, (size_t)0x1000);    /* 4 KB, >= MAX_M*4 */
    if (!dma_x || !dma_W || !dma_y) {
        fprintf(stderr,
            "[ERROR] udmabuf not available.\n"
            "        Add udmabuf nodes to system-user.dtsi and rebuild.\n");
        return 1;
    }

    phys_x = udmabuf_phys_addr(0);
    phys_W = udmabuf_phys_addr(1);
    phys_y = udmabuf_phys_addr(2);
    printf("DMA phys: x=0x%lx  W=0x%lx  y=0x%lx\n",
           (unsigned long)phys_x,
           (unsigned long)phys_W,
           (unsigned long)phys_y);

    /* Run all test vectors */
    printf("%-22s %5s %5s %10s %s\n",
           "Test", "M", "K", "Lat(us)", "Result");

    for (i = 0; i < TV_COUNT; i++) {
        TR r = run_one(i);
        total++;
        if (r.passed) passed++;
        total_us += r.us;
        printf("%-22s %5d %5d %10u %s\n",
               TV_NAMES[i], r.M, r.K, r.us,
               r.passed       ? "PASS"    :
               (r.us == 0)    ? "TIMEOUT" : "FAIL");
        if (!r.passed && r.mm > 0)
            printf("  -> %d mismatch(es)\n", r.mm);
    }

    printf("=== RESULT: %d/%d PASS  total=%llu us  avg=%llu us ===\n",
           passed, total,
           (unsigned long long)total_us,
           total > 0 ? (unsigned long long)(total_us / (uint64_t)total) : 0ULL);

    close(devmem_fd);
    return (passed == total) ? 0 : 1;
}

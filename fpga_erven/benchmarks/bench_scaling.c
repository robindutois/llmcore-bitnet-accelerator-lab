/* =============================================================================
 * bench_scaling.c — Week 7
 * Matrix-scaling benchmark — BitLinear-FPGA Alpha (PetaLinux / u-dma-buf flow)
 * Student : Erven LE BIVIC — Seoul National University
 * =============================================================================
 *
 * Mirrors the proven hardware/DMA path of run_bitlinear_linux.c:
 *   - AXI-Lite control registers mapped via /dev/mem
 *   - DMA buffers via the ikwzm u-dma-buf driver (/dev/udmabuf0/1/2),
 *     physical addresses read from sysfs, cache coherency handled with the
 *     sync_for_device / sync_for_cpu sysfs controls.
 *
 * Per size: generates random activations/weights, runs WARMUP_RUNS untimed
 * then BENCH_RUNS timed iterations, records the MEDIAN kernel latency (robust
 * against the first-call cost), throughput (GOPS), and a bit-exact verify
 * against an inline CPU reference.
 *
 * Required Week 7 sizes: M=64/K=128, M=128/K=256, M=256/K=512. Extra sizes are
 * included to trace the scaling curve.
 *
 * Build (on the PC — cross-compile for the board's ARM cores):
 *   aarch64-linux-gnu-gcc -O2 -static -o bench_scaling bench_scaling.c
 *
 * Run ON THE BOARD, in this order (this is the critical part):
 *   load-bitstream            # wait until fpga_manager state = 'operating'
 *   modprobe u-dma-buf
 *   sudo ./bench_scaling results_week7.csv
 *
 * Requires udmabuf0/1/2 in the device tree (system-user.dtsi):
 *   udmabuf0 >= 1024 B (x) , udmabuf1 >= 131072 B (W) , udmabuf2 >= 2048 B (y)
 * ============================================================================= */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

/* ---- AXI-Lite address map (identical to run_bitlinear_linux.c) ---- */
#define CTRL_BASEADDR     0xA0000000UL   /* s_axi_CTRL    (M, K, ap_ctrl)   */
#define CONTROL_BASEADDR  0xA0010000UL   /* s_axi_CONTROL (x / W / y ptrs)  */
#define MAP_SIZE          0x10000UL

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

/* ---- Buffer sizes (match the udmabuf nodes in system-user.dtsi) ---- */
#define MAX_M           512
#define MAX_K           1024
#define MAX_PACKED_SZ   (MAX_M * MAX_K / 4)        /* 131072 bytes */
#define POLL_TIMEOUT_NS 5000000000ULL             /* 5 s */

/* ---- Benchmark parameters ---- */
#define WARMUP_RUNS    2
#define BENCH_RUNS     11      /* odd -> true median at index BENCH_RUNS/2 */
#define RNG_SEED       42u

/* Swept sizes, ascending in 2*M*K. The three Week 7 CDC-required sizes are
 * 64x128, 128x256, 256x512. Comment out the others to run only those three. */
typedef struct { int M; int K; } Size;
static const Size SIZES[] = {
    {  64,   64 },
    {  64,  128 },   /* *** CDC required *** */
    { 128,  128 },
    { 128,  256 },   /* *** CDC required *** */
    { 256,  256 },
    { 256,  512 },   /* *** CDC required *** */
    { 512,  512 },
    { 512, 1024 },
};
static const int NUM_SIZES = (int)(sizeof(SIZES) / sizeof(SIZES[0]));

/* ---- Globals ---- */
static volatile uint32_t *ctrl_map  = NULL;
static volatile uint32_t *ctrl2_map = NULL;
static int8_t  *dma_x = NULL;   /* udmabuf0 */
static uint8_t *dma_W = NULL;   /* udmabuf1 */
static int32_t *dma_y = NULL;   /* udmabuf2 */
static uint64_t phys_x, phys_W, phys_y;
static int devmem_fd = -1;

/* ---- Register helpers ---- */
static inline void reg_wr(volatile uint32_t *base, uint32_t off, uint32_t val) {
    base[off / 4] = val;
    __sync_synchronize();
}
static inline uint32_t reg_rd(volatile uint32_t *base, uint32_t off) {
    __sync_synchronize();
    return base[off / 4];
}
static inline void reg_wr64(volatile uint32_t *base, uint32_t lo, uint32_t hi, uint64_t a) {
    reg_wr(base, lo, (uint32_t)(a & 0xFFFFFFFFULL));
    reg_wr(base, hi, (uint32_t)(a >> 32));
}

static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ---- /dev/mem ---- */
static volatile uint32_t *map_phys(int fd, uint64_t phys, size_t size) {
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)phys);
    if (p == MAP_FAILED) { perror("mmap /dev/mem"); return NULL; }
    return (volatile uint32_t *)p;
}

/* ---- u-dma-buf helpers (ikwzm driver) ---- */
static uint64_t udmabuf_phys_addr(int idx) {
    char path[80]; FILE *f; uint64_t a = 0;
    snprintf(path, sizeof(path), "/sys/class/u-dma-buf/udmabuf%d/phys_addr", idx);
    f = fopen(path, "r");
    if (!f) { perror(path); return 0; }
    if (fscanf(f, "0x%lx", &a) != 1)
        fprintf(stderr, "[WARN] could not parse phys_addr for udmabuf%d\n", idx);
    fclose(f);
    return a;
}

/* for_device=1 : flush CPU writes to DDR before the IP reads (sync_for_device)
 * for_device=0 : invalidate CPU cache after the IP writes DDR (sync_for_cpu)  */
static void udmabuf_sync(int idx, int for_device, size_t len) {
    char path[96]; FILE *f;
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

static int udmabuf_fd[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };

/* Keep the fd open for the process lifetime (closing right after mmap can
 * release the mapping on some driver versions -> SIGBUS on later pages). */
static void *udmabuf_mmap(int idx, size_t size) {
    char path[32]; int fd; void *p;
    snprintf(path, sizeof(path), "/dev/udmabuf%d", idx);
    /* No O_SYNC: keep the mapping cacheable so memset/memcpy work; coherency
     * is handled explicitly via the sync_for_* sysfs controls above. */
    fd = open(path, O_RDWR);
    if (fd < 0) { perror(path); return NULL; }
    p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap udmabuf"); close(fd); return NULL; }
    if (idx >= 0 && idx < 8) udmabuf_fd[idx] = fd; else close(fd);
    return p;
}

/* ---- Ternary weight encode/decode + CPU reference ---- */
static inline uint8_t encode_w(int8_t w) { return (w == 1) ? 1u : (w == -1) ? 2u : 0u; }
static inline int8_t  decode_w(uint8_t b, int pos) {
    uint8_t c = (b >> (pos * 2)) & 0x3u;
    return (c == 1) ? 1 : (c == 2) ? -1 : 0;
}
static void cpu_bitlinear(const int8_t *x, const uint8_t *W, int32_t *y, int M, int K) {
    for (int m = 0; m < M; m++) {
        int32_t acc = 0;
        for (int k = 0; k < K; k++) {
            int flat = m * K + k;
            acc += (int32_t)decode_w(W[flat / 4], flat % 4) * (int32_t)x[k];
        }
        y[m] = acc;
    }
}

static int cmp_u64(const void *a, const void *b) {
    uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b;
    return (x > y) - (x < y);
}

/* ---- Per-size benchmark ---- */
typedef struct {
    int M, K; uint64_t median_ns, min_ns; double gops; long long ops; int verify; int valid;
} SR;

static SR bench_one(int M, int K) {
    SR r; memset(&r, 0, sizeof(r));
    r.M = M; r.K = K; r.ops = 2LL * M * K;
    static int32_t y_ref[MAX_M];
    size_t packed = (size_t)M * K / 4;

    /* Random activations x in [-127, 127] */
    for (int k = 0; k < K; k++)
        dma_x[k] = (int8_t)((rand() % 255) - 127);

    /* Random ternary W, packed directly into dma_W */
    memset(dma_W, 0, packed);
    for (int flat = 0; flat < M * K; flat++) {
        int rv = rand() % 3;                       /* 0,1,2 -> 0,+1,-1 */
        int8_t w = (rv == 0) ? 0 : (rv == 1) ? 1 : -1;
        dma_W[flat / 4] |= (uint8_t)(encode_w(w) << ((flat % 4) * 2));
    }

    /* Golden reference (host CPU) */
    cpu_bitlinear(dma_x, dma_W, y_ref, M, K);

    /* Program IP: scalars + physical buffer addresses */
    reg_wr(ctrl_map, REG_M_DATA, (uint32_t)M);
    reg_wr(ctrl_map, REG_K_DATA, (uint32_t)K);
    reg_wr64(ctrl2_map, REG_X_LO, REG_X_HI, phys_x);
    reg_wr64(ctrl2_map, REG_W_LO, REG_W_HI, phys_W);
    reg_wr64(ctrl2_map, REG_Y_LO, REG_Y_HI, phys_y);
    __sync_synchronize();

    /* Push inputs to DDR so the IP sees fresh x, W */
    udmabuf_sync(0, 1, (size_t)K);
    udmabuf_sync(1, 1, packed);

    /* Warmup (untimed) */
    for (int w = 0; w < WARMUP_RUNS; w++) {
        uint64_t t0 = now_ns();
        reg_wr(ctrl_map, REG_AP_CTRL, AP_START);
        while (!(reg_rd(ctrl_map, REG_AP_CTRL) & AP_DONE))
            if (now_ns() - t0 > POLL_TIMEOUT_NS) { r.valid = 0; return r; }
    }

    /* Timed runs — measure ap_start -> ap_done (kernel latency) */
    uint64_t lat[BENCH_RUNS];
    for (int run = 0; run < BENCH_RUNS; run++) {
        uint64_t t0 = now_ns(), t1;
        reg_wr(ctrl_map, REG_AP_CTRL, AP_START);
        while (1) {
            t1 = now_ns();
            if (reg_rd(ctrl_map, REG_AP_CTRL) & AP_DONE) break;
            if (t1 - t0 > POLL_TIMEOUT_NS) { r.valid = 0; return r; }
        }
        lat[run] = t1 - t0;
    }
    __sync_synchronize();

    /* Invalidate y so the CPU reads what the IP wrote, then verify */
    udmabuf_sync(2, 0, (size_t)(M * (int)sizeof(int32_t)));
    int mm = 0;
    for (int m = 0; m < M; m++) if (dma_y[m] != y_ref[m]) mm++;
    r.verify = (mm == 0);

    qsort(lat, BENCH_RUNS, sizeof(uint64_t), cmp_u64);
    r.min_ns    = lat[0];
    r.median_ns = lat[BENCH_RUNS / 2];
    r.gops      = (double)r.ops / (double)r.median_ns;   /* ops/ns = GOP/s */
    r.valid     = 1;
    return r;
}

int main(int argc, char **argv) {
    const char *csv_path = (argc > 1) ? argv[1] : "results_week7.csv";

    printf("\n=== BitLinear-FPGA Alpha - Week 7 Matrix-Scaling Benchmark ===\n");
    printf("CTRL=0x%08X  CTRL2=0x%08X   runs/size: %d warmup + %d timed (median)\n",
           (unsigned)CTRL_BASEADDR, (unsigned)CONTROL_BASEADDR, WARMUP_RUNS, BENCH_RUNS);

    srand(RNG_SEED);

    devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem_fd < 0) { perror("/dev/mem"); return 1; }

    ctrl_map  = map_phys(devmem_fd, CTRL_BASEADDR,    MAP_SIZE);
    ctrl2_map = map_phys(devmem_fd, CONTROL_BASEADDR, MAP_SIZE);
    if (!ctrl_map || !ctrl2_map) return 1;

    uint32_t ap = reg_rd(ctrl_map, REG_AP_CTRL);
    printf("[%s] IP ap_ctrl=0x%08X\n", (ap & AP_IDLE) ? "ok" : "WARN", ap);

    dma_x = udmabuf_mmap(0, (size_t)0x1000);    /* >= MAX_K            */
    dma_W = udmabuf_mmap(1, (size_t)0x20000);   /* = MAX_PACKED_SZ     */
    dma_y = udmabuf_mmap(2, (size_t)0x1000);    /* >= MAX_M*4          */
    if (!dma_x || !dma_W || !dma_y) {
        fprintf(stderr,
            "[ERROR] udmabuf not available.\n"
            "        Run 'modprobe u-dma-buf' first, and make sure udmabuf0/1/2\n"
            "        are declared in system-user.dtsi.\n");
        return 1;
    }

    phys_x = udmabuf_phys_addr(0);
    phys_W = udmabuf_phys_addr(1);
    phys_y = udmabuf_phys_addr(2);
    printf("DMA phys: x=0x%lx  W=0x%lx  y=0x%lx\n",
           (unsigned long)phys_x, (unsigned long)phys_W, (unsigned long)phys_y);

    FILE *csv = fopen(csv_path, "w");
    if (!csv) { perror("fopen csv"); return 1; }
    fprintf(csv, "M,K,median_us,min_us,gops,ops,verify\n");

    printf("\n%6s %6s %12s %12s %10s %s\n",
           "M", "K", "median(us)", "min(us)", "GOPS", "verify");

    int passed = 0, valid = 0;
    for (int i = 0; i < NUM_SIZES; i++) {
        SR r = bench_one(SIZES[i].M, SIZES[i].K);
        if (!r.valid) {
            printf("%6d %6d  %s\n", SIZES[i].M, SIZES[i].K, "TIMEOUT");
            continue;
        }
        valid++;
        if (r.verify) passed++;

        unsigned long long med_us = (r.median_ns + 500) / 1000ULL;
        unsigned long long min_us = (r.min_ns    + 500) / 1000ULL;

        printf("%6d %6d %12llu %12llu %10.6f %s\n",
               r.M, r.K, med_us, min_us, r.gops, r.verify ? "PASS" : "FAIL");
        fprintf(csv, "%d,%d,%llu,%llu,%.6f,%lld,%s\n",
                r.M, r.K, med_us, min_us, r.gops, r.ops, r.verify ? "PASS" : "FAIL");
        printf("CSV,%d,%d,%llu,%llu,%.6f,%lld,%s\n",
               r.M, r.K, med_us, min_us, r.gops, r.ops, r.verify ? "PASS" : "FAIL");
    }
    fclose(csv);

    printf("\n=== %d/%d sizes verified bit-exact ; CSV -> %s ===\n",
           passed, valid, csv_path);

    close(devmem_fd);
    return (passed == valid && valid == NUM_SIZES) ? 0 : 1;
}

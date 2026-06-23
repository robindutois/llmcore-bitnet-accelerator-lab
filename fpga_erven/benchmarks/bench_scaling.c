/* =============================================================================
 * bench_scaling.c — Week 8
 * Matrix-scaling benchmark + timing decomposition — BitLinear-FPGA Alpha
 * Student : Erven LE BIVIC — Seoul National University
 * =============================================================================
 *
 * Extends the Week 7 bench_scaling.c with:
 *   1. Timing decomposition: separates each benchmark iteration into three
 *      measurable phases:
 *        T_setup   — CPU register writes + sync_for_device (DMA flush)
 *        T_compute — ap_start -> ap_done poll (pure kernel time)
 *        T_readback — sync_for_cpu (cache invalidate) + output verify
 *   2. Phase medians reported per size, written to CSV alongside total latency.
 *   3. Same correctness guarantee as Week 7 (bit-exact vs. CPU reference).
 *
 * This decomposition is the key analytical argument for Week 8: it shows
 * concretely where time is spent and confirms T_compute is the bottleneck
 * (not DMA setup), which motivates the HLS unroll exploration.
 *
 * Build (cross-compile on PC, conda deactivated):
 *   aarch64-linux-gnu-gcc -O2 -static -o bench_scaling bench_scaling.c
 *
 * Run ON THE BOARD via run_benchmark.sh, or manually:
 *   sudo ./bench_scaling results_week8.csv
 *
 * Required board state before running:
 *   load-bitstream   # wait for fpga_manager state = 'operating'
 *   modprobe u-dma-buf
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
#define CTRL_BASEADDR     0xA0000000UL
#define CONTROL_BASEADDR  0xA0010000UL
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

/* ---- Buffer sizes (match udmabuf nodes in system-user.dtsi) ---- */
#define MAX_M            512
#define MAX_K            1024
#define MAX_PACKED_SZ    (MAX_M * MAX_K / 4)  /* 131 072 bytes */
#define POLL_TIMEOUT_NS  5000000000ULL         /* 5 s */

/* ---- Benchmark parameters ---- */
#define WARMUP_RUNS  2
#define BENCH_RUNS   11   /* odd -> true median at BENCH_RUNS/2 */
#define RNG_SEED     42u

/* --- Sizes swept: the 3 CDC-required sizes + 5 additional scaling points --- */
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
static int8_t  *dma_x = NULL;
static uint8_t *dma_W = NULL;
static int32_t *dma_y = NULL;
static uint64_t phys_x, phys_W, phys_y;
static int devmem_fd  = -1;
static int udmabuf_fd[8];

/* ---- Register helpers ---- */
static inline void reg_wr(volatile uint32_t *b, uint32_t off, uint32_t v) {
    b[off / 4] = v; __sync_synchronize();
}
static inline uint32_t reg_rd(volatile uint32_t *b, uint32_t off) {
    __sync_synchronize(); return b[off / 4];
}
static inline void reg_wr64(volatile uint32_t *b, uint32_t lo, uint32_t hi, uint64_t a) {
    reg_wr(b, lo, (uint32_t)(a & 0xFFFFFFFFULL));
    reg_wr(b, hi, (uint32_t)(a >> 32));
}
static uint64_t now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* ---- /dev/mem mapper ---- */
static volatile uint32_t *map_phys(int fd, uint64_t phys, size_t sz) {
    void *p = mmap(NULL, sz, PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)phys);
    if (p == MAP_FAILED) { perror("mmap /dev/mem"); return NULL; }
    return (volatile uint32_t *)p;
}

/* ---- u-dma-buf helpers (ikwzm driver) ---- */
static uint64_t udmabuf_phys_addr(int idx) {
    char path[80]; FILE *f; uint64_t a = 0;
    snprintf(path, sizeof(path),
             "/sys/class/u-dma-buf/udmabuf%d/phys_addr", idx);
    f = fopen(path, "r");
    if (!f) { perror(path); return 0; }
    if (fscanf(f, "0x%lx", &a) != 1)
        fprintf(stderr, "[WARN] cannot parse phys_addr for udmabuf%d\n", idx);
    fclose(f); return a;
}

static void udmabuf_sync(int idx, int for_device, size_t len) {
    char path[96]; FILE *f;
    snprintf(path, sizeof(path),
             "/sys/class/u-dma-buf/udmabuf%d/sync_offset", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "0"); fclose(f); }
    snprintf(path, sizeof(path),
             "/sys/class/u-dma-buf/udmabuf%d/sync_size", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "%zu", len); fclose(f); }
    snprintf(path, sizeof(path),
             "/sys/class/u-dma-buf/udmabuf%d/sync_direction", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "%d", for_device ? 1 : 2); fclose(f); }
    snprintf(path, sizeof(path),
             for_device ? "/sys/class/u-dma-buf/udmabuf%d/sync_for_device"
                        : "/sys/class/u-dma-buf/udmabuf%d/sync_for_cpu", idx);
    f = fopen(path, "w"); if (f) { fprintf(f, "1"); fclose(f); }
}

static void *udmabuf_mmap(int idx, size_t size) {
    char path[32]; int fd; void *p;
    snprintf(path, sizeof(path), "/dev/udmabuf%d", idx);
    /* No O_SYNC: keep cacheable; coherency via sync_for_device/cpu sysfs. */
    fd = open(path, O_RDWR);
    if (fd < 0) { perror(path); return NULL; }
    p = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap udmabuf"); close(fd); return NULL; }
    if (idx >= 0 && idx < 8) udmabuf_fd[idx] = fd; else close(fd);
    return p;
}

/* ---- Ternary encode/decode + CPU reference ---- */
static inline uint8_t encode_w(int8_t w) {
    return (w == 1) ? 1u : (w == -1) ? 2u : 0u;
}
static inline int8_t decode_w(uint8_t b, int pos) {
    uint8_t c = (b >> (pos * 2)) & 0x3u;
    return (c == 1) ? 1 : (c == 2) ? -1 : 0;
}
static void cpu_bitlinear(const int8_t *x, const uint8_t *W,
                          int32_t *y, int M, int K) {
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

/* ---- Result struct carrying phase breakdown ---- */
typedef struct {
    int M, K;
    /* total (ap_start -> ap_done, same metric as Week 7) */
    uint64_t median_ns, min_ns;
    /* phase decomposition medians */
    uint64_t setup_ns;    /* reg writes + sync_for_device */
    uint64_t compute_ns;  /* ap_start -> ap_done */
    uint64_t readback_ns; /* sync_for_cpu + memcmp */
    double gops;
    long long ops;
    int verify;
    int valid;
} SR;

static SR bench_one(int M, int K) {
    SR r; memset(&r, 0, sizeof(r));
    r.M = M; r.K = K; r.ops = 2LL * M * K;

    static int32_t y_ref[MAX_M];
    size_t packed = (size_t)M * K / 4;
    size_t x_bytes = (size_t)K;
    size_t y_bytes = (size_t)M * sizeof(int32_t);

    /* Random activations */
    for (int k = 0; k < K; k++)
        dma_x[k] = (int8_t)((rand() % 255) - 127);

    /* Random ternary weights, packed */
    memset(dma_W, 0, packed);
    for (int flat = 0; flat < M * K; flat++) {
        int rv = rand() % 3;
        int8_t w = (rv == 0) ? 0 : (rv == 1) ? 1 : -1;
        dma_W[flat / 4] |= (uint8_t)(encode_w(w) << ((flat % 4) * 2));
    }

    /* Golden reference */
    cpu_bitlinear(dma_x, dma_W, y_ref, M, K);

    /* ---- Phase latency arrays ---- */
    uint64_t lat_setup[BENCH_RUNS];
    uint64_t lat_compute[BENCH_RUNS];
    uint64_t lat_readback[BENCH_RUNS];
    uint64_t lat_total[BENCH_RUNS];

    /* ---- Warmup (untimed, absorbs first-call costs) ---- */
    /* Pre-load registers once before warmup */
    reg_wr(ctrl_map, REG_M_DATA, (uint32_t)M);
    reg_wr(ctrl_map, REG_K_DATA, (uint32_t)K);
    reg_wr64(ctrl2_map, REG_X_LO, REG_X_HI, phys_x);
    reg_wr64(ctrl2_map, REG_W_LO, REG_W_HI, phys_W);
    reg_wr64(ctrl2_map, REG_Y_LO, REG_Y_HI, phys_y);
    __sync_synchronize();

    for (int w = 0; w < WARMUP_RUNS; w++) {
        udmabuf_sync(0, 1, x_bytes);
        udmabuf_sync(1, 1, packed);
        uint64_t t0 = now_ns();
        reg_wr(ctrl_map, REG_AP_CTRL, AP_START);
        while (!(reg_rd(ctrl_map, REG_AP_CTRL) & AP_DONE))
            if (now_ns() - t0 > POLL_TIMEOUT_NS) { r.valid = 0; return r; }
        udmabuf_sync(2, 0, y_bytes);
    }

    /* ---- Timed iterations with phase decomposition ---- */
    for (int run = 0; run < BENCH_RUNS; run++) {
        uint64_t t0, t1, t2, t3;

        /* Phase 1: setup */
        t0 = now_ns();
        reg_wr(ctrl_map, REG_M_DATA, (uint32_t)M);
        reg_wr(ctrl_map, REG_K_DATA, (uint32_t)K);
        reg_wr64(ctrl2_map, REG_X_LO, REG_X_HI, phys_x);
        reg_wr64(ctrl2_map, REG_W_LO, REG_W_HI, phys_W);
        reg_wr64(ctrl2_map, REG_Y_LO, REG_Y_HI, phys_y);
        __sync_synchronize();
        udmabuf_sync(0, 1, x_bytes);
        udmabuf_sync(1, 1, packed);
        t1 = now_ns();

        /* Phase 2: compute (ap_start -> ap_done) */
        reg_wr(ctrl_map, REG_AP_CTRL, AP_START);
        while (1) {
            t2 = now_ns();
            if (reg_rd(ctrl_map, REG_AP_CTRL) & AP_DONE) break;
            if (t2 - t1 > POLL_TIMEOUT_NS) { r.valid = 0; return r; }
        }

        /* Phase 3: readback (cache invalidate + verify prep) */
        udmabuf_sync(2, 0, y_bytes);
        t3 = now_ns();

        lat_setup[run]    = t1 - t0;
        lat_compute[run]  = t2 - t1;
        lat_readback[run] = t3 - t2;
        lat_total[run]    = t3 - t0;
    }

    /* Final output verify (on last run's result) */
    int mm = 0;
    for (int m = 0; m < M; m++) if (dma_y[m] != y_ref[m]) mm++;
    r.verify = (mm == 0);

    /* Sort each phase independently to get individual medians */
    qsort(lat_setup,    BENCH_RUNS, sizeof(uint64_t), cmp_u64);
    qsort(lat_compute,  BENCH_RUNS, sizeof(uint64_t), cmp_u64);
    qsort(lat_readback, BENCH_RUNS, sizeof(uint64_t), cmp_u64);
    qsort(lat_total,    BENCH_RUNS, sizeof(uint64_t), cmp_u64);

    r.setup_ns    = lat_setup[BENCH_RUNS / 2];
    r.compute_ns  = lat_compute[BENCH_RUNS / 2];
    r.readback_ns = lat_readback[BENCH_RUNS / 2];
    r.median_ns   = lat_total[BENCH_RUNS / 2];
    r.min_ns      = lat_total[0];

    /* GOPS based on total median (consistent with Week 7 definition) */
    r.gops  = (double)r.ops / (double)r.median_ns;
    r.valid = 1;
    return r;
}

int main(int argc, char **argv) {
    const char *csv_path = (argc > 1) ? argv[1] : "results_week8.csv";

    printf("\n=== BitLinear-FPGA Alpha - Week 8 Benchmark "
           "(timing decomposition) ===\n");
    printf("CTRL=0x%08X  CTRL2=0x%08X  runs/size: %d warmup + %d timed\n",
           (unsigned)CTRL_BASEADDR, (unsigned)CONTROL_BASEADDR,
           WARMUP_RUNS, BENCH_RUNS);

    srand(RNG_SEED);
    for (int i = 0; i < 8; i++) udmabuf_fd[i] = -1;

    devmem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (devmem_fd < 0) { perror("/dev/mem"); return 1; }

    ctrl_map  = map_phys(devmem_fd, CTRL_BASEADDR,    MAP_SIZE);
    ctrl2_map = map_phys(devmem_fd, CONTROL_BASEADDR, MAP_SIZE);
    if (!ctrl_map || !ctrl2_map) return 1;

    uint32_t ap = reg_rd(ctrl_map, REG_AP_CTRL);
    printf("[%s] IP ap_ctrl=0x%08X\n", (ap & AP_IDLE) ? "ok" : "WARN", ap);

    dma_x = udmabuf_mmap(0, (size_t)0x1000);
    dma_W = udmabuf_mmap(1, (size_t)0x20000);
    dma_y = udmabuf_mmap(2, (size_t)0x1000);
    if (!dma_x || !dma_W || !dma_y) {
        fprintf(stderr,
            "[ERROR] udmabuf not available.\n"
            "        Run 'modprobe u-dma-buf' and check system-user.dtsi.\n");
        return 1;
    }

    phys_x = udmabuf_phys_addr(0);
    phys_W = udmabuf_phys_addr(1);
    phys_y = udmabuf_phys_addr(2);
    printf("DMA phys: x=0x%lx  W=0x%lx  y=0x%lx\n",
           (unsigned long)phys_x, (unsigned long)phys_W, (unsigned long)phys_y);

    FILE *csv = fopen(csv_path, "w");
    if (!csv) { perror("fopen csv"); return 1; }

    /* CSV header — extended with phase columns */
    fprintf(csv,
        "M,K,median_us,min_us,setup_us,compute_us,readback_us,"
        "compute_pct,gops,ops,verify\n");

    printf("\n%-6s %-6s %11s %11s %9s %11s %11s %8s %9s %s\n",
           "M", "K", "total(us)", "min(us)",
           "setup(us)", "compute(us)", "readbk(us)", "cmp%", "GOPS", "verify");
    printf("%s\n",
           "------  ------  ----------- -----------  --------- "
           "----------- -----------  --------  --------- ------");

    int passed = 0, valid = 0;
    for (int i = 0; i < NUM_SIZES; i++) {
        SR r = bench_one(SIZES[i].M, SIZES[i].K);
        if (!r.valid) {
            printf("%-6d %-6d  TIMEOUT\n", SIZES[i].M, SIZES[i].K);
            continue;
        }
        valid++;
        if (r.verify) passed++;

        unsigned long long med_us  = (r.median_ns   + 500) / 1000ULL;
        unsigned long long min_us  = (r.min_ns      + 500) / 1000ULL;
        unsigned long long set_us  = (r.setup_ns    + 500) / 1000ULL;
        unsigned long long cmp_us  = (r.compute_ns  + 500) / 1000ULL;
        unsigned long long rbk_us  = (r.readback_ns + 500) / 1000ULL;
        double cmp_pct = (r.median_ns > 0)
                         ? 100.0 * (double)r.compute_ns / (double)r.median_ns
                         : 0.0;

        printf("%-6d %-6d %11llu %11llu %9llu %11llu %11llu %7.1f%% %9.6f %s\n",
               r.M, r.K,
               med_us, min_us, set_us, cmp_us, rbk_us,
               cmp_pct, r.gops,
               r.verify ? "PASS" : "FAIL");

        fprintf(csv,
            "%d,%d,%llu,%llu,%llu,%llu,%llu,%.1f,%.6f,%lld,%s\n",
            r.M, r.K,
            med_us, min_us, set_us, cmp_us, rbk_us,
            cmp_pct, r.gops, r.ops,
            r.verify ? "PASS" : "FAIL");

        /* Echo CSV line to stdout (parseable by nc-pipe capture) */
        printf("CSV,%d,%d,%llu,%llu,%llu,%llu,%llu,%.1f,%.6f,%lld,%s\n",
               r.M, r.K,
               med_us, min_us, set_us, cmp_us, rbk_us,
               cmp_pct, r.gops, r.ops,
               r.verify ? "PASS" : "FAIL");
    }
    fclose(csv);

    printf("\n=== %d/%d sizes verified bit-exact ; CSV -> %s ===\n",
           passed, valid, csv_path);

    close(devmem_fd);
    return (passed == valid && valid == NUM_SIZES) ? 0 : 1;
}

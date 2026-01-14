/**
 * @file test_benchmark.c
 * @brief Performance benchmarks for critical UFT operations
 * 
 * Measures throughput and latency of:
 * - CRC calculations
 * - PLL decoding
 * - Format detection
 * - Memory operations
 * 
 * Part of INDUSTRIAL_UPGRADE_PLAN.
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Timing Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

static double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

#define BENCHMARK_START() double _bm_start = get_time_ms()
#define BENCHMARK_END() (get_time_ms() - _bm_start)

typedef struct {
    const char *name;
    double time_ms;
    size_t operations;
    double ops_per_sec;
    double mb_per_sec;
} benchmark_result_t;

static void print_result(const benchmark_result_t *r) {
    printf("  %-35s %8.2f ms  %12.0f ops/s", r->name, r->time_ms, r->ops_per_sec);
    if (r->mb_per_sec > 0) {
        printf("  %8.2f MB/s", r->mb_per_sec);
    }
    printf("\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CRC Implementations for Benchmarking
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t bench_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

static uint16_t bench_crc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : crc << 1;
        }
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Simple PLL for Benchmarking
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int bench_pll_decode(const uint32_t *flux_times, size_t count, 
                            uint8_t *bits, size_t max_bits) {
    double cell_ns = 2000.0;  /* 2µs MFM DD */
    double window = 0;
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < count && bit_pos < max_bits; i++) {
        double delta = flux_times[i] - window;
        int cells = (int)(delta / cell_ns + 0.5);
        if (cells < 1) cells = 1;
        if (cells > 8) cells = 8;
        
        for (int c = 0; c < cells - 1 && bit_pos < max_bits; c++) {
            bits[bit_pos++] = 0;
        }
        if (bit_pos < max_bits) {
            bits[bit_pos++] = 1;
        }
        
        window = flux_times[i];
        cell_ns += (delta - cells * cell_ns) * 0.04 / cells;
        if (cell_ns < 1600) cell_ns = 1600;
        if (cell_ns > 2400) cell_ns = 2400;
    }
    
    return (int)bit_pos;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Memory Operations for Benchmarking
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void bench_memset_pattern(uint8_t *data, size_t size, uint8_t pattern) {
    memset(data, pattern, size);
}

static void bench_memcpy_data(uint8_t *dst, const uint8_t *src, size_t size) {
    memcpy(dst, src, size);
}

static int bench_memcmp_data(const uint8_t *a, const uint8_t *b, size_t size) {
    return memcmp(a, b, size);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Benchmark: CRC-32
 * ═══════════════════════════════════════════════════════════════════════════════ */

static benchmark_result_t benchmark_crc32(void) {
    benchmark_result_t r = {"CRC-32 (1MB blocks)", 0, 0, 0, 0};
    
    #define CRC_SIZE (1024 * 1024)
    #define CRC_ITERATIONS 100
    
    uint8_t *data = malloc(CRC_SIZE);
    if (!data) return r;
    
    /* Fill with pseudo-random data */
    for (size_t i = 0; i < CRC_SIZE; i++) {
        data[i] = (uint8_t)((i * 17) ^ (i >> 8));
    }
    
    volatile uint32_t result = 0;
    
    BENCHMARK_START();
    for (int i = 0; i < CRC_ITERATIONS; i++) {
        result ^= bench_crc32(data, CRC_SIZE);
    }
    r.time_ms = BENCHMARK_END();
    
    (void)result;  /* Prevent optimization */
    
    free(data);
    
    r.operations = CRC_ITERATIONS;
    r.ops_per_sec = r.operations / (r.time_ms / 1000.0);
    r.mb_per_sec = (CRC_SIZE * CRC_ITERATIONS / (1024.0 * 1024.0)) / (r.time_ms / 1000.0);
    
    #undef CRC_SIZE
    #undef CRC_ITERATIONS
    
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Benchmark: CRC-16
 * ═══════════════════════════════════════════════════════════════════════════════ */

static benchmark_result_t benchmark_crc16(void) {
    benchmark_result_t r = {"CRC-16 CCITT (sector blocks)", 0, 0, 0, 0};
    
    #define SECTOR_SIZE 512
    #define SECTOR_COUNT 10000
    
    uint8_t sector[SECTOR_SIZE];
    for (size_t i = 0; i < SECTOR_SIZE; i++) {
        sector[i] = (uint8_t)i;
    }
    
    volatile uint16_t result = 0;
    
    BENCHMARK_START();
    for (int i = 0; i < SECTOR_COUNT; i++) {
        result ^= bench_crc16(sector, SECTOR_SIZE);
    }
    r.time_ms = BENCHMARK_END();
    
    (void)result;
    
    r.operations = SECTOR_COUNT;
    r.ops_per_sec = r.operations / (r.time_ms / 1000.0);
    r.mb_per_sec = (SECTOR_SIZE * SECTOR_COUNT / (1024.0 * 1024.0)) / (r.time_ms / 1000.0);
    
    #undef SECTOR_SIZE
    #undef SECTOR_COUNT
    
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Benchmark: PLL Decode
 * ═══════════════════════════════════════════════════════════════════════════════ */

static benchmark_result_t benchmark_pll(void) {
    benchmark_result_t r = {"PLL Decode (track simulation)", 0, 0, 0, 0};
    
    #define FLUX_COUNT 100000
    #define PLL_ITERATIONS 100
    
    uint32_t *flux = malloc(FLUX_COUNT * sizeof(uint32_t));
    uint8_t *bits = malloc(FLUX_COUNT * 2);
    
    if (!flux || !bits) {
        free(flux);
        free(bits);
        return r;
    }
    
    /* Generate simulated flux timing (2µs cells with jitter) */
    uint32_t time = 0;
    for (size_t i = 0; i < FLUX_COUNT; i++) {
        time += 2000 + (i % 200) - 100;  /* 2µs ± jitter */
        flux[i] = time;
    }
    
    volatile int total_bits = 0;
    
    BENCHMARK_START();
    for (int i = 0; i < PLL_ITERATIONS; i++) {
        total_bits += bench_pll_decode(flux, FLUX_COUNT, bits, FLUX_COUNT * 2);
    }
    r.time_ms = BENCHMARK_END();
    
    (void)total_bits;
    
    free(flux);
    free(bits);
    
    r.operations = PLL_ITERATIONS;
    r.ops_per_sec = r.operations / (r.time_ms / 1000.0);
    r.mb_per_sec = 0;  /* Not meaningful for PLL */
    
    #undef FLUX_COUNT
    #undef PLL_ITERATIONS
    
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Benchmark: Memory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

static benchmark_result_t benchmark_memcpy(void) {
    benchmark_result_t r = {"memcpy (10MB blocks)", 0, 0, 0, 0};
    
    #define MEM_SIZE (10 * 1024 * 1024)
    #define MEM_ITERATIONS 50
    
    uint8_t *src = malloc(MEM_SIZE);
    uint8_t *dst = malloc(MEM_SIZE);
    
    if (!src || !dst) {
        free(src);
        free(dst);
        return r;
    }
    
    memset(src, 0xAA, MEM_SIZE);
    
    BENCHMARK_START();
    for (int i = 0; i < MEM_ITERATIONS; i++) {
        bench_memcpy_data(dst, src, MEM_SIZE);
    }
    r.time_ms = BENCHMARK_END();
    
    free(src);
    free(dst);
    
    r.operations = MEM_ITERATIONS;
    r.ops_per_sec = r.operations / (r.time_ms / 1000.0);
    r.mb_per_sec = (MEM_SIZE * MEM_ITERATIONS / (1024.0 * 1024.0)) / (r.time_ms / 1000.0);
    
    #undef MEM_SIZE
    #undef MEM_ITERATIONS
    
    return r;
}

static benchmark_result_t benchmark_memcmp(void) {
    benchmark_result_t r = {"memcmp (1MB blocks)", 0, 0, 0, 0};
    
    #define CMP_SIZE (1024 * 1024)
    #define CMP_ITERATIONS 500
    
    uint8_t *a = malloc(CMP_SIZE);
    uint8_t *b = malloc(CMP_SIZE);
    
    if (!a || !b) {
        free(a);
        free(b);
        return r;
    }
    
    memset(a, 0x55, CMP_SIZE);
    memset(b, 0x55, CMP_SIZE);
    
    volatile int result = 0;
    
    BENCHMARK_START();
    for (int i = 0; i < CMP_ITERATIONS; i++) {
        result += bench_memcmp_data(a, b, CMP_SIZE);
    }
    r.time_ms = BENCHMARK_END();
    
    (void)result;
    
    free(a);
    free(b);
    
    r.operations = CMP_ITERATIONS;
    r.ops_per_sec = r.operations / (r.time_ms / 1000.0);
    r.mb_per_sec = (CMP_SIZE * CMP_ITERATIONS / (1024.0 * 1024.0)) / (r.time_ms / 1000.0);
    
    #undef CMP_SIZE
    #undef CMP_ITERATIONS
    
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Benchmark: Format Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int mock_detect(const uint8_t *data, size_t size) {
    if (size < 4) return -1;
    
    /* Check various magic bytes */
    if (memcmp(data, "WOZ", 3) == 0) return 1;
    if (memcmp(data, "SCP", 3) == 0) return 2;
    if (memcmp(data, "A2R", 3) == 0) return 3;
    if (data[0] == 'T' && data[1] == 'D') return 4;
    
    /* Size-based detection */
    if (size == 174848) return 10;  /* D64 */
    if (size == 901120) return 11;  /* ADF */
    if (size == 737280) return 12;  /* ST/IMG */
    
    return 0;
}

static benchmark_result_t benchmark_detect(void) {
    benchmark_result_t r = {"Format Detection", 0, 0, 0, 0};
    
    #define DETECT_ITERATIONS 1000000
    
    uint8_t samples[][8] = {
        {'W', 'O', 'Z', '2', 0xFF, 0x0A, 0x0D, 0x0A},
        {'S', 'C', 'P', 0x00, 0x00, 0x00, 0x00, 0x00},
        {'A', '2', 'R', '2', 0xFF, 0x0A, 0x0D, 0x0A},
        {'T', 'D', 0x00, 0x00, 0x15, 0x00, 0x00, 0x00},
        {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    };
    
    volatile int result = 0;
    
    BENCHMARK_START();
    for (int i = 0; i < DETECT_ITERATIONS; i++) {
        result += mock_detect(samples[i % 5], 8);
    }
    r.time_ms = BENCHMARK_END();
    
    (void)result;
    
    r.operations = DETECT_ITERATIONS;
    r.ops_per_sec = r.operations / (r.time_ms / 1000.0);
    r.mb_per_sec = 0;
    
    #undef DETECT_ITERATIONS
    
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Benchmark: Disk Image Processing Simulation
 * ═══════════════════════════════════════════════════════════════════════════════ */

static benchmark_result_t benchmark_disk_processing(void) {
    benchmark_result_t r = {"Disk Processing (D64 simulation)", 0, 0, 0, 0};
    
    #define D64_SIZE 174848
    #define D64_ITERATIONS 100
    
    uint8_t *disk = malloc(D64_SIZE);
    if (!disk) return r;
    
    /* Fill with pattern */
    for (size_t i = 0; i < D64_SIZE; i++) {
        disk[i] = (uint8_t)((i * 7) ^ (i >> 4));
    }
    
    volatile uint32_t checksum = 0;
    
    BENCHMARK_START();
    for (int iter = 0; iter < D64_ITERATIONS; iter++) {
        /* Simulate: read all sectors, calculate CRC for each */
        size_t offset = 0;
        for (int track = 1; track <= 35; track++) {
            int sectors = (track <= 17) ? 21 : (track <= 24) ? 19 : (track <= 30) ? 18 : 17;
            for (int s = 0; s < sectors && offset + 256 <= D64_SIZE; s++) {
                checksum ^= bench_crc32(disk + offset, 256);
                offset += 256;
            }
        }
    }
    r.time_ms = BENCHMARK_END();
    
    (void)checksum;
    
    free(disk);
    
    r.operations = D64_ITERATIONS;
    r.ops_per_sec = r.operations / (r.time_ms / 1000.0);
    r.mb_per_sec = (D64_SIZE * D64_ITERATIONS / (1024.0 * 1024.0)) / (r.time_ms / 1000.0);
    
    #undef D64_SIZE
    #undef D64_ITERATIONS
    
    return r;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    printf("  UFT Performance Benchmarks\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n\n");
    
    benchmark_result_t results[8];
    int n = 0;
    
    printf("Running benchmarks...\n\n");
    
    printf("CRC Operations:\n");
    results[n++] = benchmark_crc32();
    print_result(&results[n-1]);
    results[n++] = benchmark_crc16();
    print_result(&results[n-1]);
    
    printf("\nPLL Operations:\n");
    results[n++] = benchmark_pll();
    print_result(&results[n-1]);
    
    printf("\nMemory Operations:\n");
    results[n++] = benchmark_memcpy();
    print_result(&results[n-1]);
    results[n++] = benchmark_memcmp();
    print_result(&results[n-1]);
    
    printf("\nFormat Operations:\n");
    results[n++] = benchmark_detect();
    print_result(&results[n-1]);
    results[n++] = benchmark_disk_processing();
    print_result(&results[n-1]);
    
    printf("\n═══════════════════════════════════════════════════════════════════════════\n");
    printf("  Summary:\n");
    printf("═══════════════════════════════════════════════════════════════════════════\n");
    
    double total_time = 0;
    for (int i = 0; i < n; i++) {
        total_time += results[i].time_ms;
    }
    printf("  Total benchmark time: %.2f ms\n", total_time);
    printf("  Benchmarks completed: %d\n", n);
    
    printf("\n");
    
    return 0;
}

/**
 * @file uft_benchmark.c
 * @brief UFT Performance Benchmark Suite
 * 
 * Measures decode/encode performance for various formats.
 * 
 * Build: gcc -O3 -o uft_benchmark uft_benchmark.c -I../../include -L../../build -luft_core
 * Run:   ./uft_benchmark [iterations]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

/*============================================================================
 * TIMING HELPERS
 *============================================================================*/

typedef struct {
    double start;
    double elapsed;
} uft_timer_t;

static double get_time_us(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000000.0 / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000000.0 + (double)tv.tv_usec;
#endif
}

static void timer_start(uft_timer_t *t) {
    t->start = get_time_us();
}

static void timer_stop(uft_timer_t *t) {
    t->elapsed = get_time_us() - t->start;
}

/*============================================================================
 * TEST DATA GENERATORS
 *============================================================================*/

/* Generate synthetic MFM-encoded data */
static uint8_t* generate_mfm_data(size_t bits, size_t *byte_count) {
    *byte_count = (bits + 7) / 8;
    uint8_t *data = malloc(*byte_count);
    if (!data) return NULL;
    
    /* Pattern: alternating clock/data bits */
    for (size_t i = 0; i < *byte_count; i++) {
        data[i] = 0xAA;  /* 10101010 - valid MFM pattern */
    }
    return data;
}

/* Generate synthetic GCR-encoded data */
static uint8_t* generate_gcr_data(size_t bytes, size_t *out_count) {
    *out_count = bytes;
    uint8_t *data = malloc(bytes);
    if (!data) return NULL;
    
    /* Use valid GCR patterns */
    static const uint8_t gcr_patterns[] = {
        0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
        0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
    };
    
    for (size_t i = 0; i < bytes; i++) {
        data[i] = gcr_patterns[i % 16];
    }
    return data;
}

/* Generate synthetic flux timing data */
static uint32_t* generate_flux_data(size_t transitions, double cell_ns) {
    uint32_t *flux = malloc(transitions * sizeof(uint32_t));
    if (!flux) return NULL;
    
    /* Simulate flux transitions at regular intervals with jitter */
    uint32_t base = (uint32_t)(cell_ns / 25.0);  /* 25ns resolution */
    for (size_t i = 0; i < transitions; i++) {
        /* Add ±10% jitter */
        int jitter = (rand() % (base / 5)) - (base / 10);
        flux[i] = base + jitter;
    }
    return flux;
}

/*============================================================================
 * BENCHMARK FUNCTIONS
 *============================================================================*/

typedef struct {
    const char *name;
    size_t data_size;
    size_t iterations;
    double total_us;
    double avg_us;
    double throughput_mbps;
} bench_result_t;

/* Benchmark: MFM bit extraction */
static bench_result_t bench_mfm_extract(size_t iterations) {
    bench_result_t result = {"MFM Bit Extract", 0, iterations, 0, 0, 0};
    
    size_t byte_count;
    uint8_t *mfm_data = generate_mfm_data(100000, &byte_count);
    if (!mfm_data) return result;
    
    result.data_size = byte_count;
    uint8_t *output = malloc(byte_count / 2);
    if (!output) { free(mfm_data); return result; }
    
    uft_timer_t timer;
    timer_start(&timer);
    
    for (size_t iter = 0; iter < iterations; iter++) {
        /* Simple MFM decode: extract every other bit */
        for (size_t i = 0; i < byte_count; i++) {
            uint8_t in = mfm_data[i];
            output[i/2] = (uint8_t)(((in >> 6) & 1) << 3 |
                                    ((in >> 4) & 1) << 2 |
                                    ((in >> 2) & 1) << 1 |
                                    ((in >> 0) & 1) << 0);
        }
    }
    
    timer_stop(&timer);
    
    result.total_us = timer.elapsed;
    result.avg_us = timer.elapsed / iterations;
    result.throughput_mbps = (double)(byte_count * iterations) / timer.elapsed;
    
    free(mfm_data);
    free(output);
    return result;
}

/* Benchmark: GCR 5-to-4 decode */
static bench_result_t bench_gcr_decode(size_t iterations) {
    bench_result_t result = {"GCR 5-to-4 Decode", 0, iterations, 0, 0, 0};
    
    size_t byte_count;
    uint8_t *gcr_data = generate_gcr_data(10000, &byte_count);
    if (!gcr_data) return result;
    
    result.data_size = byte_count;
    uint8_t *output = malloc(byte_count);
    if (!output) { free(gcr_data); return result; }
    
    /* GCR decode table */
    static const uint8_t gcr_to_nybble[32] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
        0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
        0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
    };
    
    uft_timer_t timer;
    timer_start(&timer);
    
    for (size_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < byte_count; i++) {
            output[i] = gcr_to_nybble[gcr_data[i] & 0x1F];
        }
    }
    
    timer_stop(&timer);
    
    result.total_us = timer.elapsed;
    result.avg_us = timer.elapsed / iterations;
    result.throughput_mbps = (double)(byte_count * iterations) / timer.elapsed;
    
    free(gcr_data);
    free(output);
    return result;
}

/* Benchmark: CRC-16 CCITT */
static bench_result_t bench_crc16(size_t iterations) {
    bench_result_t result = {"CRC-16 CCITT", 0, iterations, 0, 0, 0};
    
    size_t data_size = 512;  /* Typical sector size */
    uint8_t *data = malloc(data_size);
    if (!data) return result;
    
    result.data_size = data_size;
    
    /* Fill with random data */
    for (size_t i = 0; i < data_size; i++) {
        data[i] = (uint8_t)(rand() & 0xFF);
    }
    
    volatile uint16_t crc = 0;
    
    uft_timer_t timer;
    timer_start(&timer);
    
    for (size_t iter = 0; iter < iterations; iter++) {
        uint16_t c = 0xFFFF;
        for (size_t i = 0; i < data_size; i++) {
            c = (uint16_t)((c >> 8) ^ ((c ^ data[i]) << 8));
            for (int j = 0; j < 8; j++) {
                c = (c & 1) ? (c >> 1) ^ 0x8408 : c >> 1;
            }
        }
        crc = c;
    }
    
    timer_stop(&timer);
    (void)crc;
    
    result.total_us = timer.elapsed;
    result.avg_us = timer.elapsed / iterations;
    result.throughput_mbps = (double)(data_size * iterations) / timer.elapsed;
    
    free(data);
    return result;
}

/* Benchmark: Flux to timing conversion */
static bench_result_t bench_flux_convert(size_t iterations) {
    bench_result_t result = {"Flux→Timing", 0, iterations, 0, 0, 0};
    
    size_t flux_count = 50000;  /* ~1 track worth */
    uint32_t *flux = generate_flux_data(flux_count, 2000.0);  /* 2µs cells */
    if (!flux) return result;
    
    result.data_size = flux_count * sizeof(uint32_t);
    double *timing = malloc(flux_count * sizeof(double));
    if (!timing) { free(flux); return result; }
    
    uft_timer_t timer;
    timer_start(&timer);
    
    for (size_t iter = 0; iter < iterations; iter++) {
        for (size_t i = 0; i < flux_count; i++) {
            timing[i] = (double)flux[i] * 25.0;  /* Convert to ns */
        }
    }
    
    timer_stop(&timer);
    
    result.total_us = timer.elapsed;
    result.avg_us = timer.elapsed / iterations;
    result.throughput_mbps = (double)(flux_count * sizeof(uint32_t) * iterations) / timer.elapsed;
    
    free(flux);
    free(timing);
    return result;
}

/*============================================================================
 * MAIN
 *============================================================================*/

static void print_result(const bench_result_t *r) {
    printf("  %-20s %8zu bytes × %6zu = %10.2f µs avg, %8.2f MB/s\n",
           r->name, r->data_size, r->iterations, r->avg_us, r->throughput_mbps);
}

int main(int argc, char **argv) {
    size_t iterations = 1000;
    
    if (argc > 1) {
        iterations = (size_t)atoi(argv[1]);
        if (iterations < 1) iterations = 1;
        if (iterations > 100000) iterations = 100000;
    }
    
    printf("════════════════════════════════════════════════════════════════════════\n");
    printf("  UFT PERFORMANCE BENCHMARK\n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("  Iterations: %zu\n", iterations);
    printf("\n");
    printf("  Results:\n");
    
    srand(42);  /* Reproducible */
    
    bench_result_t results[4];
    results[0] = bench_mfm_extract(iterations);
    results[1] = bench_gcr_decode(iterations);
    results[2] = bench_crc16(iterations);
    results[3] = bench_flux_convert(iterations);
    
    for (int i = 0; i < 4; i++) {
        print_result(&results[i]);
    }
    
    printf("\n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    
    return 0;
}

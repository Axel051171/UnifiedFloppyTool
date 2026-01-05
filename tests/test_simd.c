/**
 * @file test_simd.c
 * @brief Test für UFT SIMD Detection und Basic Functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "uft/uft_simd.h"

#define TEST_SIZE 10000
#define BENCHMARK_ITERATIONS 1000

/* Test result tracking */
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    printf("  Testing: %-40s ", name); \
    fflush(stdout)

#define PASS() \
    do { printf("[PASS]\n"); tests_passed++; } while(0)

#define FAIL(msg) \
    do { printf("[FAIL] %s\n", msg); tests_failed++; } while(0)

/*============================================================================
 * TEST: CPU Detection
 *============================================================================*/

static void test_cpu_detection(void)
{
    printf("\n=== CPU Detection Tests ===\n");
    
    TEST("uft_cpu_detect() returns valid info");
    uft_cpu_info_t info = uft_cpu_detect();
    if (info.vendor[0] != '\0') {
        PASS();
    } else {
        FAIL("Vendor string is empty");
    }
    
    TEST("uft_cpu_get_info() returns cached pointer");
    const uft_cpu_info_t* cached = uft_cpu_get_info();
    if (cached != NULL && strcmp(cached->vendor, info.vendor) == 0) {
        PASS();
    } else {
        FAIL("Cached info doesn't match");
    }
    
    TEST("uft_cpu_impl_name() returns valid string");
    const char* impl = uft_cpu_impl_name();
    if (impl != NULL && strlen(impl) > 0) {
        PASS();
        printf("    Selected implementation: %s\n", impl);
    } else {
        FAIL("Implementation name is NULL/empty");
    }
    
    TEST("Feature detection is consistent");
    bool has_sse2_direct = uft_cpu_has_feature(UFT_CPU_SSE2);
    bool has_sse2_cached = (uft_cpu_get_info()->features & UFT_CPU_SSE2) != 0;
    if (has_sse2_direct == has_sse2_cached) {
        PASS();
    } else {
        FAIL("Feature flags inconsistent");
    }
    
    /* Print detected features */
    printf("\n  Detected Features:\n");
    printf("    SSE2:     %s\n", uft_cpu_has_feature(UFT_CPU_SSE2) ? "Yes" : "No");
    printf("    AVX2:     %s\n", uft_cpu_has_feature(UFT_CPU_AVX2) ? "Yes" : "No");
    printf("    AVX-512:  %s\n", uft_cpu_has_feature(UFT_CPU_AVX512F) ? "Yes" : "No");
    printf("    POPCNT:   %s\n", uft_cpu_has_feature(UFT_CPU_POPCNT) ? "Yes" : "No");
}

/*============================================================================
 * TEST: Bit Operations
 *============================================================================*/

static void test_bit_operations(void)
{
    printf("\n=== Bit Operation Tests ===\n");
    
    TEST("uft_find_first_set(0) returns -1");
    if (uft_find_first_set(0) == -1) {
        PASS();
    } else {
        FAIL("Should return -1 for zero");
    }
    
    TEST("uft_find_first_set(1) returns 0");
    if (uft_find_first_set(1) == 0) {
        PASS();
    } else {
        FAIL("Should return 0");
    }
    
    TEST("uft_find_first_set(0x80) returns 7");
    if (uft_find_first_set(0x80) == 7) {
        PASS();
    } else {
        FAIL("Should return 7");
    }
    
    TEST("uft_find_first_set(0x100) returns 8");
    if (uft_find_first_set(0x100) == 8) {
        PASS();
    } else {
        FAIL("Should return 8");
    }
    
    TEST("uft_find_last_set(0) returns -1");
    if (uft_find_last_set(0) == -1) {
        PASS();
    } else {
        FAIL("Should return -1 for zero");
    }
    
    TEST("uft_find_last_set(0xFF) returns 7");
    if (uft_find_last_set(0xFF) == 7) {
        PASS();
    } else {
        FAIL("Should return 7");
    }
    
    TEST("uft_popcount_array works correctly");
    uint8_t test_data[] = {0xFF, 0x00, 0xAA, 0x55};  /* 8 + 0 + 4 + 4 = 16 bits */
    size_t count = uft_popcount_array(test_data, sizeof(test_data));
    if (count == 16) {
        PASS();
    } else {
        char msg[64];
        snprintf(msg, sizeof(msg), "Expected 16, got %zu", count);
        FAIL(msg);
    }
}

/*============================================================================
 * TEST: MFM Decoder
 *============================================================================*/

static void test_mfm_decoder(void)
{
    printf("\n=== MFM Decoder Tests ===\n");
    
    TEST("Scalar decoder handles NULL input");
    size_t result = uft_mfm_decode_flux_scalar(NULL, 0, NULL);
    if (result == 0) {
        PASS();
    } else {
        FAIL("Should return 0 for NULL input");
    }
    
    TEST("Scalar decoder handles empty input");
    uint64_t empty[2] = {0, 0};
    uint8_t output[256];
    result = uft_mfm_decode_flux_scalar(empty, 1, output);
    if (result == 0) {
        PASS();
    } else {
        FAIL("Should return 0 for count < 2");
    }
    
    TEST("Scalar decoder produces output");
    /* Generate synthetic flux data (DD timing: ~2µs cells) */
    uint64_t flux[100];
    flux[0] = 0;
    for (int i = 1; i < 100; i++) {
        /* Alternate between 2000ns and 4000ns intervals */
        flux[i] = flux[i-1] + (i % 2 ? 2000 : 4000);
    }
    
    memset(output, 0, sizeof(output));
    result = uft_mfm_decode_flux_scalar(flux, 100, output);
    if (result > 0) {
        PASS();
        printf("    Decoded %zu bytes from 100 transitions\n", result);
    } else {
        FAIL("Should produce output");
    }
    
    TEST("Dispatcher selects implementation");
    result = uft_mfm_decode_flux(flux, 100, output);
    if (result > 0) {
        PASS();
        printf("    Dispatcher used: %s\n", uft_cpu_impl_name());
    } else {
        FAIL("Dispatcher failed");
    }
}

/*============================================================================
 * TEST: Aligned Memory
 *============================================================================*/

static void test_aligned_memory(void)
{
    printf("\n=== Aligned Memory Tests ===\n");
    
    TEST("uft_simd_alloc(1024, 32) returns aligned ptr");
    void* ptr = uft_simd_alloc(1024, 32);
    if (ptr != NULL && uft_is_aligned(ptr, 32)) {
        PASS();
        uft_simd_free(ptr);
    } else {
        FAIL("Pointer not aligned or NULL");
    }
    
    TEST("uft_simd_alloc(0, 32) returns NULL");
    ptr = uft_simd_alloc(0, 32);
    if (ptr == NULL) {
        PASS();
    } else {
        FAIL("Should return NULL for size 0");
        uft_simd_free(ptr);
    }
    
    TEST("uft_simd_alloc(1024, 3) returns NULL (non-power-of-2)");
    ptr = uft_simd_alloc(1024, 3);
    if (ptr == NULL) {
        PASS();
    } else {
        FAIL("Should return NULL for non-power-of-2 alignment");
        uft_simd_free(ptr);
    }
    
    TEST("64-byte alignment for AVX-512");
    ptr = uft_simd_alloc(4096, 64);
    if (ptr != NULL && uft_is_aligned(ptr, 64)) {
        PASS();
        uft_simd_free(ptr);
    } else {
        FAIL("64-byte alignment failed");
    }
}

/*============================================================================
 * BENCHMARK
 *============================================================================*/

static void run_benchmark(void)
{
    printf("\n=== Performance Benchmark ===\n");
    
    /* Allocate test data */
    uint64_t* flux = uft_simd_alloc(TEST_SIZE * sizeof(uint64_t), 32);
    uint8_t* output = uft_simd_alloc(TEST_SIZE * 2, 32);
    
    if (!flux || !output) {
        printf("  Failed to allocate benchmark buffers\n");
        return;
    }
    
    /* Generate synthetic flux data */
    flux[0] = 0;
    for (size_t i = 1; i < TEST_SIZE; i++) {
        flux[i] = flux[i-1] + 2000 + (rand() % 500) - 250;
    }
    
    /* Benchmark scalar */
    clock_t start = clock();
    for (int iter = 0; iter < BENCHMARK_ITERATIONS; iter++) {
        uft_mfm_decode_flux_scalar(flux, TEST_SIZE, output);
    }
    clock_t end = clock();
    
    double scalar_time = (double)(end - start) / CLOCKS_PER_SEC;
    double scalar_mbps = (TEST_SIZE * sizeof(uint64_t) * BENCHMARK_ITERATIONS) 
                        / (scalar_time * 1024 * 1024);
    
    printf("  Scalar:     %.2f MB/s (%.3f sec)\n", scalar_mbps, scalar_time);
    
    /* Benchmark dispatcher (uses best available) */
    start = clock();
    for (int iter = 0; iter < BENCHMARK_ITERATIONS; iter++) {
        uft_mfm_decode_flux(flux, TEST_SIZE, output);
    }
    end = clock();
    
    double opt_time = (double)(end - start) / CLOCKS_PER_SEC;
    double opt_mbps = (TEST_SIZE * sizeof(uint64_t) * BENCHMARK_ITERATIONS) 
                     / (opt_time * 1024 * 1024);
    
    printf("  Optimized:  %.2f MB/s (%.3f sec) [%s]\n", 
           opt_mbps, opt_time, uft_cpu_impl_name());
    
    if (opt_time < scalar_time) {
        printf("  Speedup:    %.2fx\n", scalar_time / opt_time);
    }
    
    uft_simd_free(flux);
    uft_simd_free(output);
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  UnifiedFloppyTool v1.6.1 - SIMD Test Suite               ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    /* Print CPU info */
    uft_cpu_print_info();
    
    /* Run tests */
    test_cpu_detection();
    test_bit_operations();
    test_mfm_decoder();
    test_aligned_memory();
    
    /* Run benchmark */
    run_benchmark();
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("═══════════════════════════════════════════════════════════\n");
    
    return (tests_failed > 0) ? 1 : 0;
}

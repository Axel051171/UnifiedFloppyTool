/**
 * @file test_safe_io_alloc.c
 * @brief Tests for Safe I/O and Memory Allocation (W-P0-001, W-P0-002)
 * 
 * Tests cover:
 * - Safe file operations
 * - Safe memory allocation
 * - Allocation tracking
 * - Error handling patterns
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/core/uft_safe_io.h"
#include "uft/core/uft_safe_alloc.h"

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NULL(x) ASSERT((x) == NULL)

/*============================================================================
 * TESTS: SAFE ALLOCATION
 *============================================================================*/

TEST(alloc_basic_malloc) {
    void *ptr = uft_malloc(1024);
    ASSERT_NOT_NULL(ptr);
    uft_free(ptr);
}

TEST(alloc_zero_size) {
    void *ptr = uft_malloc(0);
    ASSERT_NULL(ptr);  /* Zero size should return NULL */
}

TEST(alloc_calloc_basic) {
    int *arr = (int *)uft_calloc(10, sizeof(int));
    ASSERT_NOT_NULL(arr);
    
    /* Should be zero-initialized */
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(arr[i], 0);
    }
    
    uft_free(arr);
}

TEST(alloc_calloc_zero) {
    void *ptr = uft_calloc(0, sizeof(int));
    ASSERT_NULL(ptr);
    
    ptr = uft_calloc(10, 0);
    ASSERT_NULL(ptr);
}

TEST(alloc_realloc_grow) {
    char *ptr = (char *)uft_malloc(10);
    ASSERT_NOT_NULL(ptr);
    
    strcpy(ptr, "hello");
    
    ptr = (char *)uft_realloc(ptr, 100);
    ASSERT_NOT_NULL(ptr);
    ASSERT_EQ(strcmp(ptr, "hello"), 0);  /* Data preserved */
    
    uft_free(ptr);
}

TEST(alloc_realloc_null) {
    /* realloc(NULL, size) should work like malloc */
    void *ptr = uft_realloc(NULL, 50);
    ASSERT_NOT_NULL(ptr);
    uft_free(ptr);
}

TEST(alloc_realloc_zero) {
    void *ptr = uft_malloc(100);
    ASSERT_NOT_NULL(ptr);
    
    /* realloc(ptr, 0) should free */
    void *result = uft_realloc(ptr, 0);
    ASSERT_NULL(result);
    /* ptr is now freed, don't use it */
}

TEST(alloc_free_null) {
    /* Should not crash */
    uft_free(NULL);
}

TEST(alloc_strdup) {
    char *dup = uft_strdup("test string");
    ASSERT_NOT_NULL(dup);
    ASSERT_EQ(strcmp(dup, "test string"), 0);
    uft_free(dup);
}

TEST(alloc_strdup_null) {
    char *dup = uft_strdup(NULL);
    ASSERT_NULL(dup);
}

/*============================================================================
 * TESTS: ALLOCATION TRACKING
 *============================================================================*/

TEST(alloc_tracking_basic) {
    uft_alloc_reset_stats();
    uft_alloc_set_tracking(true);
    
    void *ptr1 = uft_malloc(100);
    void *ptr2 = uft_malloc(200);
    void *ptr3 = uft_calloc(10, sizeof(int));
    
    const uft_alloc_stats_t *stats = uft_alloc_get_stats();
    ASSERT_EQ(stats->total_allocations, 3);
    
    uft_free(ptr1);
    uft_free(ptr2);
    uft_free(ptr3);
    
    stats = uft_alloc_get_stats();
    ASSERT_EQ(stats->total_frees, 3);
    
    uft_alloc_set_tracking(false);
}

TEST(alloc_tracking_disabled) {
    uft_alloc_reset_stats();
    uft_alloc_set_tracking(false);
    
    void *ptr = uft_malloc(100);
    uft_free(ptr);
    
    const uft_alloc_stats_t *stats = uft_alloc_get_stats();
    ASSERT_EQ(stats->total_allocations, 0);  /* Not tracked */
}

/*============================================================================
 * TESTS: SAFE I/O
 *============================================================================*/

TEST(io_fopen_null_path) {
    FILE *fp = uft_fopen(NULL, "rb");
    ASSERT_NULL(fp);
}

TEST(io_fopen_null_mode) {
    FILE *fp = uft_fopen("test.txt", NULL);
    ASSERT_NULL(fp);
}

TEST(io_fopen_nonexistent) {
    FILE *fp = uft_fopen("/nonexistent/path/file.txt", "rb");
    ASSERT_NULL(fp);
    
    /* Error message should be set */
    const char *err = uft_io_get_error();
    ASSERT_NOT_NULL(err);
    ASSERT_TRUE(strlen(err) > 0);
}

TEST(io_roundtrip) {
    const char *path = "/tmp/uft_test_io.bin";
    uint8_t write_data[256];
    
    /* Fill with pattern */
    for (int i = 0; i < 256; i++) {
        write_data[i] = (uint8_t)i;
    }
    
    /* Write */
    ASSERT_TRUE(uft_write_file(path, write_data, sizeof(write_data)));
    
    /* Read back */
    size_t read_size;
    uint8_t *read_data = uft_read_file(path, &read_size);
    ASSERT_NOT_NULL(read_data);
    ASSERT_EQ(read_size, sizeof(write_data));
    
    /* Verify */
    ASSERT_EQ(memcmp(write_data, read_data, sizeof(write_data)), 0);
    
    free(read_data);
    remove(path);
}

TEST(io_read_u16_le) {
    const char *path = "/tmp/uft_test_u16.bin";
    uint8_t data[2] = {0x34, 0x12};  /* LE: 0x1234 */
    
    uft_write_file(path, data, 2);
    
    FILE *fp = uft_fopen(path, "rb");
    ASSERT_NOT_NULL(fp);
    
    uint16_t val;
    ASSERT_TRUE(uft_read_u16_le(fp, &val));
    ASSERT_EQ(val, 0x1234);
    
    fclose(fp);
    remove(path);
}

TEST(io_read_u32_be) {
    const char *path = "/tmp/uft_test_u32.bin";
    uint8_t data[4] = {0x12, 0x34, 0x56, 0x78};  /* BE: 0x12345678 */
    
    uft_write_file(path, data, 4);
    
    FILE *fp = uft_fopen(path, "rb");
    ASSERT_NOT_NULL(fp);
    
    uint32_t val;
    ASSERT_TRUE(uft_read_u32_be(fp, &val));
    ASSERT_EQ(val, 0x12345678);
    
    fclose(fp);
    remove(path);
}

TEST(io_fseek_ftell) {
    const char *path = "/tmp/uft_test_seek.bin";
    uint8_t data[100] = {0};
    
    uft_write_file(path, data, sizeof(data));
    
    FILE *fp = uft_fopen(path, "rb");
    ASSERT_NOT_NULL(fp);
    
    ASSERT_TRUE(uft_fseek(fp, 50, SEEK_SET));
    ASSERT_EQ(uft_ftell(fp), 50);
    
    ASSERT_TRUE(uft_fseek(fp, 10, SEEK_CUR));
    ASSERT_EQ(uft_ftell(fp), 60);
    
    fclose(fp);
    remove(path);
}

TEST(io_file_size) {
    const char *path = "/tmp/uft_test_size.bin";
    uint8_t data[12345];
    memset(data, 0xAB, sizeof(data));
    
    uft_write_file(path, data, sizeof(data));
    
    FILE *fp = uft_fopen(path, "rb");
    ASSERT_NOT_NULL(fp);
    
    long size = uft_file_size(fp);
    ASSERT_EQ(size, sizeof(data));
    
    /* Position should be restored */
    ASSERT_EQ(uft_ftell(fp), 0);
    
    fclose(fp);
    remove(path);
}

/*============================================================================
 * TESTS: ERROR HANDLING MACROS
 *============================================================================*/

TEST(macro_free_null) {
    int *ptr = (int *)uft_malloc(sizeof(int));
    ASSERT_NOT_NULL(ptr);
    
    UFT_FREE_NULL(ptr);
    ASSERT_NULL(ptr);
    
    /* Should be safe to call again */
    UFT_FREE_NULL(ptr);
}

TEST(macro_free_array) {
    char **arr = (char **)uft_calloc(3, sizeof(char *));
    ASSERT_NOT_NULL(arr);
    
    arr[0] = uft_strdup("first");
    arr[1] = uft_strdup("second");
    arr[2] = uft_strdup("third");
    
    /* Free all */
    uft_free_array((void **)arr, 3);
    /* arr is now invalid, don't use it */
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Safe I/O and Allocation Tests (W-P0-001, W-P0-002)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Safe Allocation */
    printf("[SUITE] Safe Allocation\n");
    RUN_TEST(alloc_basic_malloc);
    RUN_TEST(alloc_zero_size);
    RUN_TEST(alloc_calloc_basic);
    RUN_TEST(alloc_calloc_zero);
    RUN_TEST(alloc_realloc_grow);
    RUN_TEST(alloc_realloc_null);
    RUN_TEST(alloc_realloc_zero);
    RUN_TEST(alloc_free_null);
    RUN_TEST(alloc_strdup);
    RUN_TEST(alloc_strdup_null);
    
    /* Allocation Tracking */
    printf("\n[SUITE] Allocation Tracking\n");
    RUN_TEST(alloc_tracking_basic);
    RUN_TEST(alloc_tracking_disabled);
    
    /* Safe I/O */
    printf("\n[SUITE] Safe I/O\n");
    RUN_TEST(io_fopen_null_path);
    RUN_TEST(io_fopen_null_mode);
    RUN_TEST(io_fopen_nonexistent);
    RUN_TEST(io_roundtrip);
    RUN_TEST(io_read_u16_le);
    RUN_TEST(io_read_u32_be);
    RUN_TEST(io_fseek_ftell);
    RUN_TEST(io_file_size);
    
    /* Error Handling Macros */
    printf("\n[SUITE] Error Handling Macros\n");
    RUN_TEST(macro_free_null);
    RUN_TEST(macro_free_array);
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

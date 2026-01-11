/**
 * @file test_format_convert.c
 * @brief Tests for Format Converter
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/convert/uft_format_convert.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  Testing: %s... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASS\n"); \
        tests_passed++; \
    } else { \
        printf("FAIL\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion Path Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_conv_path_count(void) {
    int count = 0;
    for (int i = 0; UFT_CONV_PATHS[i].notes != NULL; i++) {
        count++;
    }
    return count >= 20;  /* At least 20 paths */
}

static int test_conv_find_path(void) {
    const uft_conv_path_t* path = uft_conv_find_path(UFT_FORMAT_ADF, UFT_FORMAT_HFE);
    return path != NULL && path->lossless == true;
}

static int test_conv_can_convert(void) {
    return uft_conv_can_convert(UFT_FORMAT_ADF, UFT_FORMAT_HFE) &&
           !uft_conv_can_convert(UFT_FORMAT_HFE, UFT_FORMAT_IPF);  /* No reverse path defined */
}

static int test_conv_is_lossless(void) {
    return uft_conv_is_lossless(UFT_FORMAT_ADF, UFT_FORMAT_HFE) &&
           !uft_conv_is_lossless(UFT_FORMAT_IPF, UFT_FORMAT_ADF);
}

static int test_conv_get_level(void) {
    uft_conv_level_t level = uft_conv_get_level(UFT_FORMAT_ADF, UFT_FORMAT_HFE);
    return level == UFT_CONV_LEVEL_BITSTREAM;
}

static int test_conv_get_targets(void) {
    uft_format_type_t targets[10];
    int count = uft_conv_get_targets(UFT_FORMAT_ADF, targets, 10);
    return count >= 2;  /* ADF can convert to HFE, SCP */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Status and Options Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_conv_status_str(void) {
    return strcmp(uft_conv_status_str(UFT_CONV_OK), "Success") == 0 &&
           strcmp(uft_conv_status_str(UFT_CONV_ERR_NULL_PTR), "Null pointer") == 0;
}

static int test_conv_options_init(void) {
    uft_conv_options_t opts;
    uft_conv_options_init(&opts);
    return opts.level == UFT_CONV_LEVEL_AUTO &&
           opts.flags == UFT_CONV_FLAG_NONE &&
           opts.revolutions == 1;
}

static int test_conv_disk_init(void) {
    uft_conv_disk_t disk;
    uft_conv_disk_init(&disk);
    return disk.source_format == UFT_FORMAT_UNKNOWN &&
           disk.rpm == 300 &&
           disk.data_rate == 250000;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Size Code Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_conv_size_codes(void) {
    return uft_conv_size_code_to_bytes(0) == 128 &&
           uft_conv_size_code_to_bytes(1) == 256 &&
           uft_conv_size_code_to_bytes(2) == 512 &&
           uft_conv_size_code_to_bytes(3) == 1024;
}

static int test_conv_size_code_reverse(void) {
    return uft_conv_bytes_to_size_code(128) == 0 &&
           uft_conv_bytes_to_size_code(512) == 2 &&
           uft_conv_bytes_to_size_code(1024) == 3;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== Format Converter Tests ===\n\n");
    
    printf("[Conversion Paths]\n");
    TEST(conv_path_count);
    TEST(conv_find_path);
    TEST(conv_can_convert);
    TEST(conv_is_lossless);
    TEST(conv_get_level);
    TEST(conv_get_targets);
    
    printf("\n[Status & Options]\n");
    TEST(conv_status_str);
    TEST(conv_options_init);
    TEST(conv_disk_init);
    
    printf("\n[Size Codes]\n");
    TEST(conv_size_codes);
    TEST(conv_size_code_reverse);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}

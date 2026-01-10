/**
 * @file test_scl.c
 * @brief SCL Format Unit Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/formats/scl/uft_scl.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    printf("  TEST: %s ... ", #name); \
    tests_run++; \
    if (test_##name()) { \
        printf("PASSED\n"); \
        tests_passed++; \
    } else { \
        printf("FAILED\n"); \
    } \
} while(0)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Test Data
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Minimal valid SCL: SINCLAIR + 0 files */
static const uint8_t scl_empty[] = {
    'S', 'I', 'N', 'C', 'L', 'A', 'I', 'R',  /* Magic */
    0x00                                       /* File count = 0 */
};

/* SCL with 1 file (1 sector = 256 bytes) */
static uint8_t scl_one_file[9 + 14 + 256];

static void init_one_file(void) {
    memset(scl_one_file, 0, sizeof(scl_one_file));
    
    /* Magic */
    memcpy(scl_one_file, "SINCLAIR", 8);
    scl_one_file[8] = 1;  /* 1 file */
    
    /* Directory entry at offset 9 (14 bytes) */
    uint8_t *entry = scl_one_file + 9;
    memcpy(entry, "TEST    ", 8);  /* Filename, space-padded */
    entry[8] = 'B';                /* Type = BASIC */
    entry[9] = 0x00;               /* Param 0 */
    entry[10] = 0x80;              /* Param 1 */
    entry[11] = 0x00;              /* Param 2 */
    entry[12] = 0x00;              /* Start sector (unused in SCL) */
    entry[13] = 0x01;              /* Length = 1 sector */
    
    /* Data at offset 23 (256 bytes) */
    uint8_t *data = scl_one_file + 23;
    for (int i = 0; i < 256; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int test_probe_valid(void) {
    return uft_scl_probe(scl_empty, sizeof(scl_empty)) == true;
}

static int test_probe_invalid(void) {
    uint8_t bad[] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    return uft_scl_probe(bad, sizeof(bad)) == false;
}

static int test_probe_null(void) {
    return uft_scl_probe(NULL, 0) == false;
}

static int test_probe_short(void) {
    return uft_scl_probe(scl_empty, 5) == false;
}

static int test_validate_empty(void) {
    return uft_scl_validate(scl_empty, sizeof(scl_empty)) == UFT_OK;
}

static int test_validate_one_file(void) {
    return uft_scl_validate(scl_one_file, sizeof(scl_one_file)) == UFT_OK;
}

static int test_validate_null(void) {
    return uft_scl_validate(NULL, 0) == UFT_ERR_INVALID_ARG;
}

static int test_validate_truncated(void) {
    /* Header says 1 file but no data */
    uint8_t bad[] = {'S','I','N','C','L','A','I','R', 1};
    return uft_scl_validate(bad, sizeof(bad)) == UFT_ERR_BUFFER_TOO_SMALL;
}

static int test_parse_empty(void) {
    uft_scl_t scl;
    int rc = uft_scl_parse(scl_empty, sizeof(scl_empty), &scl);
    if (rc != UFT_OK) return 0;
    if (scl.file_count != 0) return 0;
    uft_scl_free(&scl);
    return 1;
}

static int test_parse_one_file(void) {
    uft_scl_t scl;
    int rc = uft_scl_parse(scl_one_file, sizeof(scl_one_file), &scl);
    if (rc != UFT_OK) return 0;
    if (scl.file_count != 1) { uft_scl_free(&scl); return 0; }
    if (scl.entries == NULL) { uft_scl_free(&scl); return 0; }
    if (strcmp(scl.entries[0].name, "TEST") != 0) { uft_scl_free(&scl); return 0; }
    if (scl.entries[0].type != 'B') { uft_scl_free(&scl); return 0; }
    if (scl.entries[0].length_sectors != 1) { uft_scl_free(&scl); return 0; }
    uft_scl_free(&scl);
    return 1;
}

static int test_get_file(void) {
    uft_scl_entry_t meta;
    const uint8_t *data = NULL;
    size_t len = 0;
    
    int rc = uft_scl_get_file(scl_one_file, sizeof(scl_one_file), 0, &meta, &data, &len);
    if (rc != UFT_OK) return 0;
    if (len != 256) return 0;
    if (data == NULL) return 0;
    if (strcmp(meta.name, "TEST") != 0) return 0;
    
    /* Verify data content */
    for (int i = 0; i < 256; i++) {
        if (data[i] != (uint8_t)(i & 0xFF)) return 0;
    }
    
    return 1;
}

static int test_get_file_invalid_index(void) {
    const uint8_t *data = NULL;
    size_t len = 0;
    
    int rc = uft_scl_get_file(scl_one_file, sizeof(scl_one_file), 99, NULL, &data, &len);
    return rc == UFT_ERR_INVALID_ARG;
}

static int test_build_empty(void) {
    uint8_t *buf = NULL;
    size_t len = 0;
    
    int rc = uft_scl_build(NULL, NULL, NULL, 0, &buf, &len);
    if (rc != UFT_OK) return 0;
    if (len != 9) { free(buf); return 0; }
    if (memcmp(buf, "SINCLAIR", 8) != 0) { free(buf); return 0; }
    if (buf[8] != 0) { free(buf); return 0; }
    
    free(buf);
    return 1;
}

static int test_build_one_file(void) {
    uft_scl_entry_t entry = {
        .name = "HELLO",
        .type = 'C',
        .param = {0, 0, 0},
        .length_sectors = 1
    };
    
    uint8_t file_data[256];
    memset(file_data, 0xAA, 256);
    
    const uint8_t *data_ptrs[] = {file_data};
    size_t sizes[] = {256};
    
    uint8_t *buf = NULL;
    size_t len = 0;
    
    int rc = uft_scl_build(&entry, data_ptrs, sizes, 1, &buf, &len);
    if (rc != UFT_OK) return 0;
    if (len != 9 + 14 + 256) { free(buf); return 0; }
    
    /* Verify by parsing back */
    uft_scl_t scl;
    rc = uft_scl_parse(buf, len, &scl);
    if (rc != UFT_OK) { free(buf); return 0; }
    if (scl.file_count != 1) { uft_scl_free(&scl); free(buf); return 0; }
    if (strcmp(scl.entries[0].name, "HELLO") != 0) { uft_scl_free(&scl); free(buf); return 0; }
    
    uft_scl_free(&scl);
    free(buf);
    return 1;
}

static int test_build_invalid_size(void) {
    uft_scl_entry_t entry = {.name = "BAD", .type = 'B', .length_sectors = 1};
    uint8_t file_data[100];  /* Not 256 aligned! */
    const uint8_t *data_ptrs[] = {file_data};
    size_t sizes[] = {100};
    
    uint8_t *buf = NULL;
    size_t len = 0;
    
    int rc = uft_scl_build(&entry, data_ptrs, sizes, 1, &buf, &len);
    return rc == UFT_ERR_FORMAT;
}

static int test_roundtrip(void) {
    /* Parse original */
    uft_scl_t scl;
    int rc = uft_scl_parse(scl_one_file, sizeof(scl_one_file), &scl);
    if (rc != UFT_OK) return 0;
    
    /* Get file data */
    const uint8_t *orig_data = NULL;
    size_t orig_len = 0;
    rc = uft_scl_get_file(scl_one_file, sizeof(scl_one_file), 0, NULL, &orig_data, &orig_len);
    if (rc != UFT_OK) { uft_scl_free(&scl); return 0; }
    
    /* Build new */
    const uint8_t *data_ptrs[] = {orig_data};
    size_t sizes[] = {orig_len};
    
    uint8_t *new_buf = NULL;
    size_t new_len = 0;
    rc = uft_scl_build(scl.entries, data_ptrs, sizes, scl.file_count, &new_buf, &new_len);
    uft_scl_free(&scl);
    
    if (rc != UFT_OK) return 0;
    
    /* Parse rebuilt */
    uft_scl_t scl2;
    rc = uft_scl_parse(new_buf, new_len, &scl2);
    if (rc != UFT_OK) { free(new_buf); return 0; }
    
    /* Verify content matches */
    const uint8_t *new_data = NULL;
    size_t new_data_len = 0;
    rc = uft_scl_get_file(new_buf, new_len, 0, NULL, &new_data, &new_data_len);
    if (rc != UFT_OK) { uft_scl_free(&scl2); free(new_buf); return 0; }
    
    int match = (new_data_len == orig_len && memcmp(new_data, orig_data, orig_len) == 0);
    
    uft_scl_free(&scl2);
    free(new_buf);
    return match;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("SCL Format Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    /* Initialize test data */
    init_one_file();
    
    printf("Probe Tests:\n");
    TEST(probe_valid);
    TEST(probe_invalid);
    TEST(probe_null);
    TEST(probe_short);
    
    printf("\nValidate Tests:\n");
    TEST(validate_empty);
    TEST(validate_one_file);
    TEST(validate_null);
    TEST(validate_truncated);
    
    printf("\nParse Tests:\n");
    TEST(parse_empty);
    TEST(parse_one_file);
    
    printf("\nGet File Tests:\n");
    TEST(get_file);
    TEST(get_file_invalid_index);
    
    printf("\nBuild Tests:\n");
    TEST(build_empty);
    TEST(build_one_file);
    TEST(build_invalid_size);
    
    printf("\nRoundtrip Tests:\n");
    TEST(roundtrip);
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

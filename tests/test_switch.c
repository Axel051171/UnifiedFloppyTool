/**
 * @file test_switch.c
 * @brief Unit tests for Nintendo Switch XCI/NSP Format
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/nintendo/uft_switch.h"

/* Test counters */
static int tests_run = 0;
static int tests_passed = 0;

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(condition) do { \
    if (!(condition)) { \
        printf("FAILED at line %d: %s\n", __LINE__, #condition); \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/**
 * @brief Create minimal XCI data
 */
static uint8_t *create_test_xci(size_t *size)
{
    size_t total = 0x400;  /* Minimum for header */
    uint8_t *data = calloc(1, total);
    
    /* XCI magic at offset 0x100 */
    data[0x100] = 'H';
    data[0x101] = 'E';
    data[0x102] = 'A';
    data[0x103] = 'D';
    
    /* Cartridge type */
    data[0x10D] = XCI_CART_8GB;
    
    *size = total;
    return data;
}

/**
 * @brief Create minimal NSP/PFS0 data
 */
static uint8_t *create_test_nsp(size_t *size)
{
    /* PFS0 header + 2 file entries + string table + data */
    size_t header_size = 16;
    size_t entries_size = 2 * 24;  /* 2 entries */
    size_t string_table = 32;      /* Filenames */
    size_t data_size = 256;
    size_t total = header_size + entries_size + string_table + data_size;
    
    uint8_t *data = calloc(1, total);
    
    /* PFS0 magic */
    data[0] = 'P';
    data[1] = 'F';
    data[2] = 'S';
    data[3] = '0';
    
    /* Number of files */
    data[4] = 2;
    data[5] = 0;
    data[6] = 0;
    data[7] = 0;
    
    /* String table size */
    data[8] = string_table;
    data[9] = 0;
    data[10] = 0;
    data[11] = 0;
    
    /* File entry 1 */
    uint8_t *entry1 = data + 16;
    entry1[0] = 0;      /* offset low */
    entry1[8] = 100;    /* size low */
    entry1[16] = 0;     /* string offset */
    
    /* File entry 2 */
    uint8_t *entry2 = data + 16 + 24;
    entry2[0] = 100;    /* offset low */
    entry2[8] = 156;    /* size low */
    entry2[16] = 10;    /* string offset */
    
    /* String table */
    uint8_t *strings = data + 16 + 48;
    strcpy((char *)strings, "test1.nca");
    strcpy((char *)strings + 10, "test2.nca");
    
    *size = total;
    return data;
}

/* ============================================================================
 * Unit Tests - Detection
 * ============================================================================ */

TEST(detect_xci)
{
    size_t size;
    uint8_t *data = create_test_xci(&size);
    
    ASSERT_TRUE(xci_detect(data, size));
    ASSERT_FALSE(nsp_detect(data, size));
    
    free(data);
}

TEST(detect_nsp)
{
    size_t size;
    uint8_t *data = create_test_nsp(&size);
    
    ASSERT_TRUE(nsp_detect(data, size));
    ASSERT_FALSE(xci_detect(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[100] = {0};
    
    ASSERT_FALSE(xci_detect(data, 100));
    ASSERT_FALSE(nsp_detect(data, 100));
    ASSERT_FALSE(xci_detect(NULL, 100));
    ASSERT_FALSE(nsp_detect(NULL, 100));
}

TEST(cart_size_name)
{
    ASSERT_STR_EQ(xci_cart_size_name(XCI_CART_1GB), "1GB");
    ASSERT_STR_EQ(xci_cart_size_name(XCI_CART_8GB), "8GB");
    ASSERT_STR_EQ(xci_cart_size_name(XCI_CART_32GB), "32GB");
}

TEST(content_type_name)
{
    ASSERT_STR_EQ(nca_content_type_name(NCA_TYPE_PROGRAM), "Program");
    ASSERT_STR_EQ(nca_content_type_name(NCA_TYPE_META), "Meta");
    ASSERT_STR_EQ(nca_content_type_name(NCA_TYPE_CONTROL), "Control");
}

/* ============================================================================
 * Unit Tests - Container Operations
 * ============================================================================ */

TEST(open_xci)
{
    size_t size;
    uint8_t *data = create_test_xci(&size);
    
    switch_ctx_t ctx;
    int ret = switch_open(data, size, &ctx);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(ctx.is_xci);
    ASSERT_NOT_NULL(ctx.data);
    
    switch_close(&ctx);
    free(data);
}

TEST(open_nsp)
{
    size_t size;
    uint8_t *data = create_test_nsp(&size);
    
    switch_ctx_t ctx;
    int ret = switch_open(data, size, &ctx);
    
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(ctx.is_xci);
    ASSERT_NOT_NULL(ctx.data);
    
    switch_close(&ctx);
    free(data);
}

TEST(close_ctx)
{
    size_t size;
    uint8_t *data = create_test_nsp(&size);
    
    switch_ctx_t ctx;
    switch_open(data, size, &ctx);
    switch_close(&ctx);
    
    ASSERT_EQ(ctx.data, NULL);
    
    free(data);
}

/* ============================================================================
 * Unit Tests - NSP Operations
 * ============================================================================ */

TEST(nsp_get_info)
{
    size_t size;
    uint8_t *data = create_test_nsp(&size);
    
    switch_ctx_t ctx;
    switch_open(data, size, &ctx);
    
    nsp_info_t info;
    int ret = nsp_get_info(&ctx, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_EQ(info.num_files, 2);
    
    switch_close(&ctx);
    free(data);
}

TEST(nsp_get_file_count)
{
    size_t size;
    uint8_t *data = create_test_nsp(&size);
    
    switch_ctx_t ctx;
    switch_open(data, size, &ctx);
    
    int count = nsp_get_file_count(&ctx);
    ASSERT_EQ(count, 2);
    
    switch_close(&ctx);
    free(data);
}

TEST(nsp_get_file)
{
    size_t size;
    uint8_t *data = create_test_nsp(&size);
    
    switch_ctx_t ctx;
    switch_open(data, size, &ctx);
    
    switch_file_entry_t entry;
    int ret = nsp_get_file(&ctx, 0, &entry);
    
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(entry.name, "test1.nca");
    ASSERT_EQ(entry.size, 100);
    
    ret = nsp_get_file(&ctx, 1, &entry);
    ASSERT_EQ(ret, 0);
    ASSERT_STR_EQ(entry.name, "test2.nca");
    
    switch_close(&ctx);
    free(data);
}

/* ============================================================================
 * Unit Tests - Utilities
 * ============================================================================ */

TEST(title_id_str)
{
    char buffer[20];
    
    switch_title_id_str(0x01004F8006A78000ULL, buffer);
    ASSERT_STR_EQ(buffer, "01004F8006A78000");
    
    switch_title_id_str(0, buffer);
    ASSERT_STR_EQ(buffer, "0000000000000000");
}

/* ============================================================================
 * Main Test Runner
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Nintendo Switch XCI/NSP Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_xci);
    RUN_TEST(detect_nsp);
    RUN_TEST(detect_invalid);
    RUN_TEST(cart_size_name);
    RUN_TEST(content_type_name);
    
    printf("\nContainer Operations:\n");
    RUN_TEST(open_xci);
    RUN_TEST(open_nsp);
    RUN_TEST(close_ctx);
    
    printf("\nNSP Operations:\n");
    RUN_TEST(nsp_get_info);
    RUN_TEST(nsp_get_file_count);
    RUN_TEST(nsp_get_file);
    
    printf("\nUtilities:\n");
    RUN_TEST(title_id_str);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}

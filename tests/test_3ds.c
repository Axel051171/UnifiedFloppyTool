/**
 * @file test_3ds.c
 * @brief Unit tests for Nintendo 3DS Container Formats
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/formats/nintendo/uft_3ds.h"

static int tests_run = 0;
static int tests_passed = 0;

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
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_TRUE(x) ASSERT((x))
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(p) ASSERT((p) != NULL)

/* Create minimal CCI/3DS data */
static uint8_t *create_test_cci(size_t *size)
{
    size_t total = 0x4000;  /* Minimal CCI */
    uint8_t *data = calloc(1, total);
    
    /* NCSD header at offset 0x100 */
    data[0x100] = 'N';
    data[0x101] = 'C';
    data[0x102] = 'S';
    data[0x103] = 'D';
    
    /* Size in media units */
    data[0x104] = (total / 0x200) & 0xFF;
    
    /* First partition entry at offset 0x120 */
    data[0x120] = 0x01;  /* Offset: 1 media unit */
    data[0x124] = 0x10;  /* Size: 16 media units */
    
    /* NCCH header at offset 0x200 (partition 0) */
    data[0x300] = 'N';
    data[0x301] = 'C';
    data[0x302] = 'C';
    data[0x303] = 'H';
    
    /* Product code */
    memcpy(data + 0x350, "CTR-TEST-0001", 13);
    
    /* Program ID */
    data[0x318] = 0x01;
    data[0x31F] = 0x00;
    
    *size = total;
    return data;
}

/* Create minimal CIA data */
static uint8_t *create_test_cia(size_t *size)
{
    size_t total = 0x4000;
    uint8_t *data = calloc(1, total);
    
    /* CIA header */
    data[0] = 0x20;
    data[1] = 0x20;  /* Header size 0x2020 */
    
    *size = total;
    return data;
}

/* Create minimal NCCH data */
static uint8_t *create_test_ncch(size_t *size)
{
    size_t total = 0x1000;
    uint8_t *data = calloc(1, total);
    
    /* NCCH header at offset 0x100 */
    data[0x100] = 'N';
    data[0x101] = 'C';
    data[0x102] = 'C';
    data[0x103] = 'H';
    
    /* Product code */
    memcpy(data + 0x150, "CTR-NCCH-TEST", 13);
    
    /* Flags - no crypto flag */
    data[0x18F] = 0x04;  /* No crypto */
    
    *size = total;
    return data;
}

/* Tests */
TEST(detect_cci)
{
    size_t size;
    uint8_t *data = create_test_cci(&size);
    
    ASSERT_TRUE(n3ds_detect_cci(data, size));
    ASSERT_FALSE(n3ds_detect_cia(data, size));
    
    free(data);
}

TEST(detect_cia)
{
    size_t size;
    uint8_t *data = create_test_cia(&size);
    
    ASSERT_TRUE(n3ds_detect_cia(data, size));
    ASSERT_FALSE(n3ds_detect_cci(data, size));
    
    free(data);
}

TEST(detect_ncch)
{
    size_t size;
    uint8_t *data = create_test_ncch(&size);
    
    ASSERT_TRUE(n3ds_detect_ncch(data, size));
    
    free(data);
}

TEST(detect_invalid)
{
    uint8_t data[256] = {0};
    
    ASSERT_FALSE(n3ds_detect_cci(data, 256));
    ASSERT_FALSE(n3ds_detect_cia(data, 256));
    ASSERT_FALSE(n3ds_detect_ncch(data, 256));
}

TEST(open_cci)
{
    size_t size;
    uint8_t *data = create_test_cci(&size);
    
    n3ds_ctx_t ctx;
    int ret = n3ds_open(data, size, &ctx);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(ctx.is_cci);
    ASSERT_NOT_NULL(ctx.data);
    ASSERT_NOT_NULL(ctx.ncsd);
    
    n3ds_close(&ctx);
    free(data);
}

TEST(open_ncch)
{
    size_t size;
    uint8_t *data = create_test_ncch(&size);
    
    n3ds_ctx_t ctx;
    int ret = n3ds_open(data, size, &ctx);
    
    ASSERT_EQ(ret, 0);
    ASSERT_FALSE(ctx.is_cci);
    ASSERT_NOT_NULL(ctx.ncch);
    
    n3ds_close(&ctx);
    free(data);
}

TEST(get_info)
{
    size_t size;
    uint8_t *data = create_test_cci(&size);
    
    n3ds_ctx_t ctx;
    n3ds_open(data, size, &ctx);
    
    n3ds_info_t info;
    int ret = n3ds_get_info(&ctx, &info);
    
    ASSERT_EQ(ret, 0);
    ASSERT_TRUE(info.is_cci);
    
    n3ds_close(&ctx);
    free(data);
}

TEST(is_encrypted)
{
    size_t size;
    uint8_t *data = create_test_ncch(&size);
    
    n3ds_ctx_t ctx;
    n3ds_open(data, size, &ctx);
    
    /* Our test NCCH has no-crypto flag set */
    ASSERT_FALSE(n3ds_is_encrypted(ctx.ncch));
    
    n3ds_close(&ctx);
    free(data);
}

TEST(partition_count)
{
    size_t size;
    uint8_t *data = create_test_cci(&size);
    
    n3ds_ctx_t ctx;
    n3ds_open(data, size, &ctx);
    
    int count = n3ds_get_partition_count(&ctx);
    ASSERT(count >= 1);
    
    n3ds_close(&ctx);
    free(data);
}

TEST(title_id_str)
{
    char buffer[20];
    n3ds_title_id_str(0x0004000000001234ULL, buffer);
    ASSERT_STR_EQ(buffer, "0004000000001234");
}

TEST(close_ctx)
{
    size_t size;
    uint8_t *data = create_test_cci(&size);
    
    n3ds_ctx_t ctx;
    n3ds_open(data, size, &ctx);
    n3ds_close(&ctx);
    
    ASSERT_EQ(ctx.data, NULL);
    
    free(data);
}

int main(int argc, char *argv[])
{
    (void)argc; (void)argv;
    
    printf("\n=== Nintendo 3DS Format Tests ===\n\n");
    
    printf("Detection:\n");
    RUN_TEST(detect_cci);
    RUN_TEST(detect_cia);
    RUN_TEST(detect_ncch);
    RUN_TEST(detect_invalid);
    
    printf("\nContainer Operations:\n");
    RUN_TEST(open_cci);
    RUN_TEST(open_ncch);
    RUN_TEST(get_info);
    RUN_TEST(is_encrypted);
    RUN_TEST(partition_count);
    RUN_TEST(close_ctx);
    
    printf("\nUtilities:\n");
    RUN_TEST(title_id_str);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}

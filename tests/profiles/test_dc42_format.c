/**
 * @file test_dc42_format.c
 * @brief Test suite for Apple DiskCopy 4.2 DC42 format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/profiles/uft_dc42_format.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { printf("  Testing %s... ", #name); test_##name(); printf("OK\n"); tests_passed++; } while(0)

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("FAIL: %d != %d\n", (int)(a), (int)(b)); tests_failed++; return; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_FALSE(x) do { if (x) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_STR_EQ(a, b) do { if (strcmp(a, b) != 0) { printf("FAIL: %s != %s\n", a, b); tests_failed++; return; } } while(0)

TEST(header_size) { ASSERT_EQ(sizeof(uft_dc42_header_t), 84); }
TEST(constants) {
    ASSERT_EQ(UFT_DC42_MAGIC, 0x0100);
    ASSERT_EQ(UFT_DC42_HEADER_SIZE, 84);
    ASSERT_EQ(UFT_DC42_MAX_NAME_LEN, 63);
    ASSERT_EQ(UFT_DC42_TAG_SIZE, 12);
}

TEST(format_constants) {
    ASSERT_EQ(UFT_DC42_FORMAT_400K_SS, 0x00);
    ASSERT_EQ(UFT_DC42_FORMAT_800K_DS, 0x01);
    ASSERT_EQ(UFT_DC42_FORMAT_1440K_HD, 0x02);
}

TEST(encoding_constants) {
    ASSERT_EQ(UFT_DC42_ENCODING_GCR, 0x12);
    ASSERT_EQ(UFT_DC42_ENCODING_MFM, 0x22);
}

TEST(size_constants) {
    ASSERT_EQ(UFT_DC42_400K_SIZE, 409600);
    ASSERT_EQ(UFT_DC42_800K_SIZE, 819200);
    ASSERT_EQ(UFT_DC42_1440K_SIZE, 1474560);
}

TEST(be16_helpers) {
    uint8_t buf[2];
    uft_dc42_write_be16(buf, 0x1234);
    ASSERT_EQ(uft_dc42_read_be16(buf), 0x1234);
}

TEST(be32_helpers) {
    uint8_t buf[4];
    uft_dc42_write_be32(buf, 0x12345678);
    ASSERT_EQ(uft_dc42_read_be32(buf), 0x12345678);
}

TEST(validate_null) { ASSERT_FALSE(uft_dc42_validate_header(NULL)); }

TEST(validate_valid) {
    uint8_t header[84] = {0};
    header[0] = 8;
    memcpy(header + 1, "TestDisk", 8);
    uft_dc42_write_be32(header + 64, UFT_DC42_400K_SIZE);
    uft_dc42_write_be16(header + 82, UFT_DC42_MAGIC);
    ASSERT_TRUE(uft_dc42_validate_header(header));
}

TEST(validate_invalid_magic) {
    uint8_t header[84] = {0};
    header[0] = 8;
    uft_dc42_write_be16(header + 82, 0xBEEF);
    ASSERT_FALSE(uft_dc42_validate_header(header));
}

TEST(parse_400k) {
    uint8_t header[84] = {0};
    header[0] = 4;
    memcpy(header + 1, "Test", 4);
    uft_dc42_write_be32(header + 64, UFT_DC42_400K_SIZE);
    uft_dc42_write_be32(header + 68, 0);
    header[80] = UFT_DC42_FORMAT_400K_SS;
    header[81] = UFT_DC42_ENCODING_GCR;
    uft_dc42_write_be16(header + 82, UFT_DC42_MAGIC);
    
    uft_dc42_info_t info;
    ASSERT_TRUE(uft_dc42_parse(header, sizeof(header), &info));
    ASSERT_STR_EQ(info.disk_name, "Test");
    ASSERT_EQ(info.data_size, UFT_DC42_400K_SIZE);
    ASSERT_TRUE(info.is_gcr);
}

TEST(parse_null) {
    uft_dc42_info_t info;
    ASSERT_FALSE(uft_dc42_parse(NULL, 100, &info));
    uint8_t header[84] = {0};
    ASSERT_FALSE(uft_dc42_parse(header, 100, NULL));
}

TEST(format_names) {
    ASSERT_STR_EQ(uft_dc42_format_name(UFT_DC42_FORMAT_400K_SS), "Mac 400K (SS)");
    ASSERT_STR_EQ(uft_dc42_format_name(UFT_DC42_FORMAT_800K_DS), "Mac 800K (DS)");
    ASSERT_STR_EQ(uft_dc42_format_name(UFT_DC42_FORMAT_1440K_HD), "Mac 1.44MB (HD)");
}

TEST(encoding_names) {
    ASSERT_STR_EQ(uft_dc42_encoding_name(UFT_DC42_ENCODING_GCR), "GCR (Sony)");
    ASSERT_STR_EQ(uft_dc42_encoding_name(UFT_DC42_ENCODING_MFM), "MFM");
}

TEST(gcr_zones) {
    ASSERT_EQ(uft_dc42_gcr_sectors_per_track(0), 12);
    ASSERT_EQ(uft_dc42_gcr_sectors_per_track(16), 11);
    ASSERT_EQ(uft_dc42_gcr_sectors_per_track(32), 10);
    ASSERT_EQ(uft_dc42_gcr_sectors_per_track(48), 9);
    ASSERT_EQ(uft_dc42_gcr_sectors_per_track(64), 8);
}

TEST(probe_valid) {
    uint8_t data[UFT_DC42_HEADER_SIZE + UFT_DC42_400K_SIZE] = {0};
    data[0] = 4;
    memcpy(data + 1, "Test", 4);
    uft_dc42_write_be32(data + 64, UFT_DC42_400K_SIZE);
    uft_dc42_write_be16(data + 82, UFT_DC42_MAGIC);
    ASSERT_TRUE(uft_dc42_probe(data, UFT_DC42_HEADER_SIZE + UFT_DC42_400K_SIZE) >= 0.7);
}

TEST(probe_invalid) {
    uint8_t data[84] = {0};
    uft_dc42_write_be16(data + 82, 0xBEEF);
    ASSERT_EQ(uft_dc42_probe(data, sizeof(data)), 0.0);
}

TEST(probe_null) { ASSERT_EQ(uft_dc42_probe(NULL, 100), 0.0); }

TEST(crc_empty) { ASSERT_EQ(uft_dc42_crc32(NULL, 0), 0); }

TEST(crc_data) {
    uint8_t data[] = {1, 2, 3, 4};
    ASSERT_TRUE(uft_dc42_crc32(data, 4) != 0);
}

TEST(create_header) {
    uint8_t header[84];
    ASSERT_TRUE(uft_dc42_create_header(header, "Test", UFT_DC42_400K_SIZE, 0, UFT_DC42_FORMAT_400K_SS));
    ASSERT_EQ(header[0], 4);
    ASSERT_EQ(uft_dc42_read_be16(header + 82), UFT_DC42_MAGIC);
}

TEST(create_header_null) { ASSERT_FALSE(uft_dc42_create_header(NULL, "Test", 0, 0, 0)); }

int main(void) {
    printf("=== DC42 Format Tests ===\n");
    RUN_TEST(header_size);
    RUN_TEST(constants);
    RUN_TEST(format_constants);
    RUN_TEST(encoding_constants);
    RUN_TEST(size_constants);
    RUN_TEST(be16_helpers);
    RUN_TEST(be32_helpers);
    RUN_TEST(validate_null);
    RUN_TEST(validate_valid);
    RUN_TEST(validate_invalid_magic);
    RUN_TEST(parse_400k);
    RUN_TEST(parse_null);
    RUN_TEST(format_names);
    RUN_TEST(encoding_names);
    RUN_TEST(gcr_zones);
    RUN_TEST(probe_valid);
    RUN_TEST(probe_invalid);
    RUN_TEST(probe_null);
    RUN_TEST(crc_empty);
    RUN_TEST(crc_data);
    RUN_TEST(create_header);
    RUN_TEST(create_header_null);
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

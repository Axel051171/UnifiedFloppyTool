/**
 * @file test_d88_format.c
 * @brief Test suite for NEC PC-88/PC-98 D88 format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/profiles/uft_d88_format.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { printf("  Testing %s... ", #name); test_##name(); printf("OK\n"); tests_passed++; } while(0)

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("FAIL: %d != %d\n", (int)(a), (int)(b)); tests_failed++; return; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_FALSE(x) do { if (x) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_STR_EQ(a, b) do { if (strcmp(a, b) != 0) { printf("FAIL: %s != %s\n", a, b); tests_failed++; return; } } while(0)

TEST(header_size) { ASSERT_EQ(sizeof(uft_d88_header_t), 688); ASSERT_EQ(UFT_D88_HEADER_SIZE, 0x2B0); }
TEST(sector_header_size) { ASSERT_EQ(sizeof(uft_d88_sector_header_t), 16); }
TEST(constants) {
    ASSERT_EQ(UFT_D88_NAME_SIZE, 17);
    ASSERT_EQ(UFT_D88_MAX_TRACKS, 164);
}

TEST(disk_types) {
    ASSERT_EQ(UFT_D88_TYPE_2D, 0x00);
    ASSERT_EQ(UFT_D88_TYPE_2DD, 0x10);
    ASSERT_EQ(UFT_D88_TYPE_2HD, 0x20);
    ASSERT_EQ(UFT_D88_TYPE_1D, 0x30);
    ASSERT_EQ(UFT_D88_TYPE_1DD, 0x40);
}

TEST(type_names) {
    ASSERT_STR_EQ(uft_d88_type_name(UFT_D88_TYPE_2D), "2D (320KB)");
    ASSERT_STR_EQ(uft_d88_type_name(UFT_D88_TYPE_2DD), "2DD (640KB)");
    ASSERT_STR_EQ(uft_d88_type_name(UFT_D88_TYPE_2HD), "2HD (1.2MB)");
    ASSERT_STR_EQ(uft_d88_type_name(0xFF), "Unknown");
}

TEST(density_names) {
    ASSERT_STR_EQ(uft_d88_density_name(UFT_D88_DENSITY_MFM), "MFM");
    ASSERT_STR_EQ(uft_d88_density_name(UFT_D88_DENSITY_FM), "FM");
}

TEST(status_names) {
    ASSERT_STR_EQ(uft_d88_status_name(UFT_D88_STATUS_NORMAL), "Normal");
    ASSERT_STR_EQ(uft_d88_status_name(UFT_D88_STATUS_DELETED), "Deleted");
    ASSERT_STR_EQ(uft_d88_status_name(UFT_D88_STATUS_CRC_ERR_DAT), "CRC Error (Data)");
}

TEST(size_code_to_bytes) {
    ASSERT_EQ(uft_d88_size_code_to_bytes(0), 128);
    ASSERT_EQ(uft_d88_size_code_to_bytes(1), 256);
    ASSERT_EQ(uft_d88_size_code_to_bytes(2), 512);
    ASSERT_EQ(uft_d88_size_code_to_bytes(3), 1024);
    ASSERT_EQ(uft_d88_size_code_to_bytes(7), 0);
}

TEST(bytes_to_size_code) {
    ASSERT_EQ(uft_d88_bytes_to_size_code(128), 0);
    ASSERT_EQ(uft_d88_bytes_to_size_code(256), 1);
    ASSERT_EQ(uft_d88_bytes_to_size_code(512), 2);
    ASSERT_EQ(uft_d88_bytes_to_size_code(1024), 3);
    ASSERT_EQ(uft_d88_bytes_to_size_code(999), 0xFF);
}

TEST(geometry_pc98_2hd) {
    ASSERT_EQ(UFT_D88_PC98_2HD_TRACKS, 77);
    ASSERT_EQ(UFT_D88_PC98_2HD_HEADS, 2);
    ASSERT_EQ(UFT_D88_PC98_2HD_SECTORS, 8);
    ASSERT_EQ(UFT_D88_PC98_2HD_SECSIZE, 1024);
}

TEST(validate_null) { ASSERT_FALSE(uft_d88_validate_header(NULL, 1000)); }
TEST(validate_too_small) { uint8_t h[100] = {0}; ASSERT_FALSE(uft_d88_validate_header(h, 100)); }

TEST(validate_valid) {
    uint8_t header[UFT_D88_HEADER_SIZE] = {0};
    header[0x1B] = UFT_D88_TYPE_2D;
    uint32_t size = UFT_D88_HEADER_SIZE + 1000;
    header[0x1C] = size & 0xFF;
    header[0x1D] = (size >> 8) & 0xFF;
    header[0x1E] = (size >> 16) & 0xFF;
    header[0x1F] = (size >> 24) & 0xFF;
    header[0x20] = UFT_D88_HEADER_SIZE & 0xFF;
    header[0x21] = (UFT_D88_HEADER_SIZE >> 8) & 0xFF;
    ASSERT_TRUE(uft_d88_validate_header(header, size));
}

TEST(validate_invalid_type) {
    uint8_t header[UFT_D88_HEADER_SIZE] = {0};
    header[0x1B] = 0xFF;
    ASSERT_FALSE(uft_d88_validate_header(header, UFT_D88_HEADER_SIZE + 100));
}

TEST(parse_2hd) {
    uint8_t header[UFT_D88_HEADER_SIZE] = {0};
    strcpy((char*)header, "PC98DISK");
    header[0x1B] = UFT_D88_TYPE_2HD;
    uint32_t size = UFT_D88_HEADER_SIZE + 5000;
    header[0x1C] = size & 0xFF;
    header[0x1D] = (size >> 8) & 0xFF;
    header[0x1E] = (size >> 16) & 0xFF;
    header[0x1F] = (size >> 24) & 0xFF;
    header[0x20] = UFT_D88_HEADER_SIZE & 0xFF;
    header[0x21] = (UFT_D88_HEADER_SIZE >> 8) & 0xFF;
    
    uft_d88_info_t info;
    ASSERT_TRUE(uft_d88_parse(header, size, &info));
    ASSERT_STR_EQ(info.name, "PC98DISK");
    ASSERT_EQ(info.disk_type, UFT_D88_TYPE_2HD);
    ASSERT_EQ(info.tracks, 77);
    ASSERT_EQ(info.sector_size, 1024);
}

TEST(parse_null) {
    uft_d88_info_t info;
    ASSERT_FALSE(uft_d88_parse(NULL, 1000, &info));
    uint8_t header[UFT_D88_HEADER_SIZE] = {0};
    ASSERT_FALSE(uft_d88_parse(header, 1000, NULL));
}

TEST(probe_valid) {
    uint8_t data[UFT_D88_HEADER_SIZE + 100] = {0};
    data[0x1B] = UFT_D88_TYPE_2HD;
    uint32_t size = sizeof(data);
    data[0x1C] = size & 0xFF;
    data[0x1D] = (size >> 8) & 0xFF;
    data[0x1E] = (size >> 16) & 0xFF;
    data[0x1F] = (size >> 24) & 0xFF;
    data[0x20] = UFT_D88_HEADER_SIZE & 0xFF;
    data[0x21] = (UFT_D88_HEADER_SIZE >> 8) & 0xFF;
    ASSERT_TRUE(uft_d88_probe(data, sizeof(data)) >= 0.6);
}

TEST(probe_invalid) {
    uint8_t data[UFT_D88_HEADER_SIZE] = {0};
    data[0x1B] = 0xFF;
    ASSERT_EQ(uft_d88_probe(data, sizeof(data)), 0.0);
}

TEST(probe_null) { ASSERT_EQ(uft_d88_probe(NULL, 1000), 0.0); }
TEST(probe_too_small) { uint8_t data[100] = {0}; ASSERT_EQ(uft_d88_probe(data, 100), 0.0); }

TEST(track_offset) {
    uft_d88_info_t info = {0};
    info.track_offsets[0] = 0x2B0;
    info.track_offsets[1] = 0x1000;
    ASSERT_EQ(uft_d88_track_offset(&info, 0, 0), 0x2B0);
    ASSERT_EQ(uft_d88_track_offset(&info, 0, 1), 0x1000);
}

TEST(track_offset_null) { ASSERT_EQ(uft_d88_track_offset(NULL, 0, 0), 0); }

TEST(create_header) {
    uint8_t header[UFT_D88_HEADER_SIZE];
    ASSERT_TRUE(uft_d88_create_header(header, "Test", UFT_D88_TYPE_2HD));
    ASSERT_EQ(header[0x1B], UFT_D88_TYPE_2HD);
}

TEST(create_header_null) { ASSERT_FALSE(uft_d88_create_header(NULL, "Test", 0)); }

int main(void) {
    printf("=== D88 Format Tests ===\n");
    RUN_TEST(header_size);
    RUN_TEST(sector_header_size);
    RUN_TEST(constants);
    RUN_TEST(disk_types);
    RUN_TEST(type_names);
    RUN_TEST(density_names);
    RUN_TEST(status_names);
    RUN_TEST(size_code_to_bytes);
    RUN_TEST(bytes_to_size_code);
    RUN_TEST(geometry_pc98_2hd);
    RUN_TEST(validate_null);
    RUN_TEST(validate_too_small);
    RUN_TEST(validate_valid);
    RUN_TEST(validate_invalid_type);
    RUN_TEST(parse_2hd);
    RUN_TEST(parse_null);
    RUN_TEST(probe_valid);
    RUN_TEST(probe_invalid);
    RUN_TEST(probe_null);
    RUN_TEST(probe_too_small);
    RUN_TEST(track_offset);
    RUN_TEST(track_offset_null);
    RUN_TEST(create_header);
    RUN_TEST(create_header_null);
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

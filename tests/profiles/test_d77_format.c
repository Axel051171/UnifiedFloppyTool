/**
 * @file test_d77_format.c
 * @brief Test suite for Fujitsu FM-7/FM-77 D77 format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/profiles/uft_d77_format.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { printf("  Testing %s... ", #name); test_##name(); printf("OK\n"); tests_passed++; } while(0)

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("FAIL: %d != %d\n", (int)(a), (int)(b)); tests_failed++; return; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_FALSE(x) do { if (x) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_STR_EQ(a, b) do { if (strcmp(a, b) != 0) { printf("FAIL: %s != %s\n", a, b); tests_failed++; return; } } while(0)

TEST(header_size) { ASSERT_EQ(sizeof(uft_d77_header_t), 688); ASSERT_EQ(UFT_D77_HEADER_SIZE, 0x2B0); }
TEST(sector_header_size) { ASSERT_EQ(sizeof(uft_d77_sector_header_t), 16); }
TEST(constants) { ASSERT_EQ(UFT_D77_NAME_SIZE, 17); ASSERT_EQ(UFT_D77_MAX_TRACKS, 164); }

TEST(disk_types) {
    ASSERT_EQ(UFT_D77_TYPE_2D, 0x00);
    ASSERT_EQ(UFT_D77_TYPE_2DD, 0x10);
    ASSERT_EQ(UFT_D77_TYPE_2HD, 0x20);
}

TEST(type_names) {
    ASSERT_STR_EQ(uft_d77_type_name(UFT_D77_TYPE_2D), "2D (320KB)");
    ASSERT_STR_EQ(uft_d77_type_name(UFT_D77_TYPE_2DD), "2DD (640KB)");
    ASSERT_STR_EQ(uft_d77_type_name(UFT_D77_TYPE_2HD), "2HD (1.2MB)");
    ASSERT_STR_EQ(uft_d77_type_name(0xFF), "Unknown");
}

TEST(fm7_2d_geometry) {
    ASSERT_EQ(UFT_D77_FM7_2D_TRACKS, 40);
    ASSERT_EQ(UFT_D77_FM7_2D_HEADS, 2);
    ASSERT_EQ(UFT_D77_FM7_2D_SECTORS, 16);
    ASSERT_EQ(UFT_D77_FM7_2D_SECSIZE, 256);
    ASSERT_EQ(UFT_D77_FM7_2D_TRACK_SIZE, 16 * 256);
    ASSERT_EQ(UFT_D77_FM7_2D_TOTAL_SIZE, 40 * 2 * 16 * 256);
}

TEST(fm77_2dd_geometry) {
    ASSERT_EQ(UFT_D77_FM77_2DD_TRACKS, 80);
    ASSERT_EQ(UFT_D77_FM77_2DD_HEADS, 2);
    ASSERT_EQ(UFT_D77_FM77_2DD_SECTORS, 8);
    ASSERT_EQ(UFT_D77_FM77_2DD_SECSIZE, 512);
    ASSERT_EQ(UFT_D77_FM77_2DD_TRACK_SIZE, 8 * 512);
    ASSERT_EQ(UFT_D77_FM77_2DD_TOTAL_SIZE, 80 * 2 * 8 * 512);
}

TEST(model_names) {
    ASSERT_STR_EQ(uft_d77_model_name(40, 16, 256), "FM-7/FM-77 (2D)");
    ASSERT_STR_EQ(uft_d77_model_name(80, 8, 512), "FM-77AV (2DD)");
    ASSERT_STR_EQ(uft_d77_model_name(77, 8, 1024), "Unknown Model");
}

TEST(size_code_conversion) {
    ASSERT_EQ(uft_d77_size_code_to_bytes(0), 128);
    ASSERT_EQ(uft_d77_size_code_to_bytes(1), 256);
    ASSERT_EQ(uft_d77_size_code_to_bytes(2), 512);
    ASSERT_EQ(uft_d77_size_code_to_bytes(7), 0);
}

TEST(validate_null) { ASSERT_FALSE(uft_d77_validate_header(NULL, 1000)); }
TEST(validate_too_small) { uint8_t h[100] = {0}; ASSERT_FALSE(uft_d77_validate_header(h, 100)); }

TEST(validate_valid) {
    uint8_t header[UFT_D77_HEADER_SIZE] = {0};
    header[0x1B] = UFT_D77_TYPE_2D;
    uint32_t size = UFT_D77_HEADER_SIZE + 1000;
    header[0x1C] = size & 0xFF;
    header[0x1D] = (size >> 8) & 0xFF;
    header[0x1E] = (size >> 16) & 0xFF;
    header[0x1F] = (size >> 24) & 0xFF;
    ASSERT_TRUE(uft_d77_validate_header(header, size));
}

TEST(validate_invalid_type) {
    uint8_t header[UFT_D77_HEADER_SIZE] = {0};
    header[0x1B] = 0xFF;
    ASSERT_FALSE(uft_d77_validate_header(header, UFT_D77_HEADER_SIZE + 100));
}

TEST(parse_fm7_2d) {
    uint8_t header[UFT_D77_HEADER_SIZE] = {0};
    strcpy((char*)header, "FM7 DISK");
    header[0x1B] = UFT_D77_TYPE_2D;
    uint32_t size = UFT_D77_HEADER_SIZE + 5000;
    header[0x1C] = size & 0xFF;
    header[0x1D] = (size >> 8) & 0xFF;
    header[0x1E] = (size >> 16) & 0xFF;
    header[0x1F] = (size >> 24) & 0xFF;
    
    uft_d77_info_t info;
    ASSERT_TRUE(uft_d77_parse(header, size, &info));
    ASSERT_STR_EQ(info.name, "FM7 DISK");
    ASSERT_EQ(info.disk_type, UFT_D77_TYPE_2D);
    ASSERT_EQ(info.tracks, 40);
    ASSERT_EQ(info.heads, 2);
    ASSERT_EQ(info.sectors_per_track, 16);
    ASSERT_EQ(info.sector_size, 256);
    ASSERT_TRUE(info.is_fm7_format);
    ASSERT_FALSE(info.is_fm77_format);
}

TEST(parse_fm77_2dd) {
    uint8_t header[UFT_D77_HEADER_SIZE] = {0};
    strcpy((char*)header, "FM77AV");
    header[0x1B] = UFT_D77_TYPE_2DD;
    uint32_t size = UFT_D77_HEADER_SIZE + 10000;
    header[0x1C] = size & 0xFF;
    header[0x1D] = (size >> 8) & 0xFF;
    header[0x1E] = (size >> 16) & 0xFF;
    header[0x1F] = (size >> 24) & 0xFF;
    
    uft_d77_info_t info;
    ASSERT_TRUE(uft_d77_parse(header, size, &info));
    ASSERT_EQ(info.disk_type, UFT_D77_TYPE_2DD);
    ASSERT_EQ(info.tracks, 80);
    ASSERT_EQ(info.sectors_per_track, 8);
    ASSERT_EQ(info.sector_size, 512);
    ASSERT_FALSE(info.is_fm7_format);
    ASSERT_TRUE(info.is_fm77_format);
}

TEST(parse_null) {
    uft_d77_info_t info;
    ASSERT_FALSE(uft_d77_parse(NULL, 1000, &info));
    uint8_t header[UFT_D77_HEADER_SIZE] = {0};
    ASSERT_FALSE(uft_d77_parse(header, 1000, NULL));
    ASSERT_FALSE(uft_d77_parse(header, 100, &info));
}

TEST(probe_valid) {
    uint8_t data[UFT_D77_HEADER_SIZE + 100] = {0};
    data[0x1B] = UFT_D77_TYPE_2D;
    uint32_t size = sizeof(data);
    data[0x1C] = size & 0xFF;
    data[0x1D] = (size >> 8) & 0xFF;
    data[0x1E] = (size >> 16) & 0xFF;
    data[0x1F] = (size >> 24) & 0xFF;
    ASSERT_TRUE(uft_d77_probe(data, sizeof(data)) >= 0.6);
}

TEST(probe_invalid) {
    uint8_t data[UFT_D77_HEADER_SIZE] = {0};
    data[0x1B] = 0xFF;
    ASSERT_EQ(uft_d77_probe(data, sizeof(data)), 0.0);
}

TEST(probe_null) { ASSERT_EQ(uft_d77_probe(NULL, 1000), 0.0); }
TEST(probe_too_small) { uint8_t data[100] = {0}; ASSERT_EQ(uft_d77_probe(data, 100), 0.0); }

TEST(track_offset) {
    uft_d77_info_t info = {0};
    info.track_offsets[0] = 0x2B0;
    info.track_offsets[1] = 0x1000;
    ASSERT_EQ(uft_d77_track_offset(&info, 0, 0), 0x2B0);
    ASSERT_EQ(uft_d77_track_offset(&info, 0, 1), 0x1000);
}

TEST(track_offset_null) { ASSERT_EQ(uft_d77_track_offset(NULL, 0, 0), 0); }

TEST(fm7_compatible) {
    uft_d77_info_t info = {0};
    info.is_valid = true;
    info.tracks = 40; info.heads = 2;
    info.sectors_per_track = 16; info.sector_size = 256;
    ASSERT_TRUE(uft_d77_is_fm7_compatible(&info));
    ASSERT_FALSE(uft_d77_is_fm77_compatible(&info));
}

TEST(fm77_compatible) {
    uft_d77_info_t info = {0};
    info.is_valid = true;
    info.tracks = 80; info.heads = 2;
    info.sectors_per_track = 8; info.sector_size = 512;
    ASSERT_FALSE(uft_d77_is_fm7_compatible(&info));
    ASSERT_TRUE(uft_d77_is_fm77_compatible(&info));
}

TEST(compatibility_null) {
    ASSERT_FALSE(uft_d77_is_fm7_compatible(NULL));
    ASSERT_FALSE(uft_d77_is_fm77_compatible(NULL));
}

TEST(create_fm7_2d) {
    uint8_t header[UFT_D77_HEADER_SIZE];
    ASSERT_TRUE(uft_d77_create_fm7_2d(header, "TestDisk"));
    ASSERT_STR_EQ((char*)header, "TestDisk");
    ASSERT_EQ(header[0x1B], UFT_D77_TYPE_2D);
}

TEST(create_fm77_2dd) {
    uint8_t header[UFT_D77_HEADER_SIZE];
    ASSERT_TRUE(uft_d77_create_fm77_2dd(header, "FM77AV"));
    ASSERT_EQ(header[0x1B], UFT_D77_TYPE_2DD);
}

TEST(create_header_null) {
    ASSERT_FALSE(uft_d77_create_fm7_2d(NULL, "Test"));
    ASSERT_FALSE(uft_d77_create_fm77_2dd(NULL, "Test"));
}

int main(void) {
    printf("=== D77 Format Tests ===\n");
    RUN_TEST(header_size);
    RUN_TEST(sector_header_size);
    RUN_TEST(constants);
    RUN_TEST(disk_types);
    RUN_TEST(type_names);
    RUN_TEST(fm7_2d_geometry);
    RUN_TEST(fm77_2dd_geometry);
    RUN_TEST(model_names);
    RUN_TEST(size_code_conversion);
    RUN_TEST(validate_null);
    RUN_TEST(validate_too_small);
    RUN_TEST(validate_valid);
    RUN_TEST(validate_invalid_type);
    RUN_TEST(parse_fm7_2d);
    RUN_TEST(parse_fm77_2dd);
    RUN_TEST(parse_null);
    RUN_TEST(probe_valid);
    RUN_TEST(probe_invalid);
    RUN_TEST(probe_null);
    RUN_TEST(probe_too_small);
    RUN_TEST(track_offset);
    RUN_TEST(track_offset_null);
    RUN_TEST(fm7_compatible);
    RUN_TEST(fm77_compatible);
    RUN_TEST(compatibility_null);
    RUN_TEST(create_fm7_2d);
    RUN_TEST(create_fm77_2dd);
    RUN_TEST(create_header_null);
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

/**
 * @file test_woz_format.c
 * @brief Test suite for Apple II WOZ format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/profiles/uft_woz_format.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { printf("  Testing %s... ", #name); test_##name(); printf("OK\n"); tests_passed++; } while(0)

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("FAIL: 0x%X != 0x%X\n", (unsigned)(a), (unsigned)(b)); tests_failed++; return; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_FALSE(x) do { if (x) { printf("FAIL\n"); tests_failed++; return; } } while(0)
#define ASSERT_STR_EQ(a, b) do { if (strcmp(a, b) != 0) { printf("FAIL: %s != %s\n", a, b); tests_failed++; return; } } while(0)

TEST(header_size) { ASSERT_EQ(sizeof(uft_woz_header_t), 12); }
TEST(chunk_header_size) { ASSERT_EQ(sizeof(uft_woz_chunk_header_t), 8); }
TEST(info_chunk_size) { ASSERT_EQ(sizeof(uft_woz_info_t), 60); }
TEST(v1_track_size) { ASSERT_EQ(sizeof(uft_woz_v1_track_t), 6656); }

TEST(signature_constants) {
    ASSERT_EQ(UFT_WOZ_SIGNATURE_WOZ1, 0x315A4F57);
    ASSERT_EQ(UFT_WOZ_SIGNATURE_WOZ2, 0x325A4F57);
    ASSERT_EQ(UFT_WOZ_MAGIC, 0x0A0D0AFF);
}

TEST(detect_version_woz1) {
    uint8_t data[32] = {0};
    data[0] = 'W'; data[1] = 'O'; data[2] = 'Z'; data[3] = '1';
    data[4] = 0xFF; data[5] = 0x0A; data[6] = 0x0D; data[7] = 0x0A;
    ASSERT_EQ(uft_woz_detect_version(data, sizeof(data)), 1);
}

TEST(detect_version_woz2) {
    uint8_t data[32] = {0};
    data[0] = 'W'; data[1] = 'O'; data[2] = 'Z'; data[3] = '2';
    data[4] = 0xFF; data[5] = 0x0A; data[6] = 0x0D; data[7] = 0x0A;
    ASSERT_EQ(uft_woz_detect_version(data, sizeof(data)), 2);
}

TEST(detect_version_invalid) {
    uint8_t data[32] = {0};
    data[0] = 'X'; data[1] = 'Y'; data[2] = 'Z'; data[3] = '1';
    data[4] = 0xFF; data[5] = 0x0A; data[6] = 0x0D; data[7] = 0x0A;
    ASSERT_EQ(uft_woz_detect_version(data, sizeof(data)), 0);
}

TEST(detect_version_null) { ASSERT_EQ(uft_woz_detect_version(NULL, 100), 0); }

TEST(chunk_ids) {
    ASSERT_EQ(UFT_WOZ_CHUNK_INFO, 0x4F464E49);
    ASSERT_EQ(UFT_WOZ_CHUNK_TMAP, 0x50414D54);
    ASSERT_EQ(UFT_WOZ_CHUNK_TRKS, 0x534B5254);
}

TEST(disk_type_names) {
    ASSERT_STR_EQ(uft_woz_disk_type_name(UFT_WOZ_DISK_525), "5.25\"");
    ASSERT_STR_EQ(uft_woz_disk_type_name(UFT_WOZ_DISK_35), "3.5\"");
    ASSERT_STR_EQ(uft_woz_disk_type_name(0), "Unknown");
}

TEST(boot_format_names) {
    ASSERT_STR_EQ(uft_woz_boot_format_name(UFT_WOZ_BOOT_DOS32), "DOS 3.2 (13-sector)");
    ASSERT_STR_EQ(uft_woz_boot_format_name(UFT_WOZ_BOOT_DOS33), "DOS 3.3 (16-sector)");
    ASSERT_STR_EQ(uft_woz_boot_format_name(UFT_WOZ_BOOT_PRODOS), "ProDOS");
}

TEST(quarter_track) {
    ASSERT_TRUE(uft_woz_quarter_track_to_track(4) - 1.0f < 0.01f);
    ASSERT_TRUE(uft_woz_quarter_track_to_track(0) < 0.01f);
}

TEST(valid_nibble) {
    ASSERT_TRUE(uft_woz_is_valid_nibble(0x96));
    ASSERT_TRUE(uft_woz_is_valid_nibble(0xFF));
    ASSERT_FALSE(uft_woz_is_valid_nibble(0xAA));
    ASSERT_FALSE(uft_woz_is_valid_nibble(0xD5));
    ASSERT_FALSE(uft_woz_is_valid_nibble(0x00));
}

TEST(gcr_constants) {
    ASSERT_EQ(UFT_WOZ_GCR_ADDR_PROLOGUE_1, 0xD5);
    ASSERT_EQ(UFT_WOZ_GCR_ADDR_PROLOGUE_2, 0xAA);
    ASSERT_EQ(UFT_WOZ_TIMING_525, 32);
}

TEST(hardware_flags) {
    ASSERT_EQ(UFT_WOZ_HW_APPLE2, 0x0001);
    ASSERT_EQ(UFT_WOZ_HW_APPLE2GS, 0x0020);
}

TEST(probe_valid) {
    uint8_t data[64] = {0};
    data[0] = 'W'; data[1] = 'O'; data[2] = 'Z'; data[3] = '1';
    data[4] = 0xFF; data[5] = 0x0A; data[6] = 0x0D; data[7] = 0x0A;
    data[12] = 'I'; data[13] = 'N'; data[14] = 'F'; data[15] = 'O';
    data[16] = 8; data[17] = 0; data[18] = 0; data[19] = 0;
    ASSERT_TRUE(uft_woz_probe(data, sizeof(data)) >= 0.6);
}

TEST(probe_invalid) {
    uint8_t data[32] = {0};
    memcpy(data, "INVALID!", 8);
    ASSERT_EQ(uft_woz_probe(data, sizeof(data)), 0.0);
}

TEST(probe_null) { ASSERT_EQ(uft_woz_probe(NULL, 100), 0.0); }

int main(void) {
    printf("=== WOZ Format Tests ===\n");
    RUN_TEST(header_size);
    RUN_TEST(chunk_header_size);
    RUN_TEST(info_chunk_size);
    RUN_TEST(v1_track_size);
    RUN_TEST(signature_constants);
    RUN_TEST(detect_version_woz1);
    RUN_TEST(detect_version_woz2);
    RUN_TEST(detect_version_invalid);
    RUN_TEST(detect_version_null);
    RUN_TEST(chunk_ids);
    RUN_TEST(disk_type_names);
    RUN_TEST(boot_format_names);
    RUN_TEST(quarter_track);
    RUN_TEST(valid_nibble);
    RUN_TEST(gcr_constants);
    RUN_TEST(hardware_flags);
    RUN_TEST(probe_valid);
    RUN_TEST(probe_invalid);
    RUN_TEST(probe_null);
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

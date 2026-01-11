/**
 * @file test_hfe_format.c
 * @brief Test suite for HxC Floppy Emulator HFE format
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/profiles/uft_hfe_format.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
    tests_passed++; \
} while(0)

#define ASSERT_EQ(a, b) do { if ((a) != (b)) { printf("FAIL: %d != %d\n", (int)(a), (int)(b)); tests_failed++; return; } } while(0)
#define ASSERT_TRUE(x) do { if (!(x)) { printf("FAIL: false\n"); tests_failed++; return; } } while(0)
#define ASSERT_FALSE(x) do { if (x) { printf("FAIL: true\n"); tests_failed++; return; } } while(0)
#define ASSERT_STR_EQ(a, b) do { if (strcmp(a, b) != 0) { printf("FAIL: %s != %s\n", a, b); tests_failed++; return; } } while(0)

TEST(header_size) { ASSERT_EQ(sizeof(uft_hfe_header_t), 512); }
TEST(track_entry_size) { ASSERT_EQ(sizeof(uft_hfe_track_entry_t), 4); }
TEST(signature_v1) { ASSERT_STR_EQ(UFT_HFE_SIGNATURE_V1, "HXCPICFE"); }
TEST(signature_v3) { ASSERT_STR_EQ(UFT_HFE_SIGNATURE_V3, "HXCHFEV3"); }

TEST(detect_version_v1) {
    uint8_t data[512] = {0};
    memcpy(data, UFT_HFE_SIGNATURE_V1, 8);
    ASSERT_EQ(uft_hfe_detect_version(data, sizeof(data)), 1);
}

TEST(detect_version_v3) {
    uint8_t data[512] = {0};
    memcpy(data, UFT_HFE_SIGNATURE_V3, 8);
    ASSERT_EQ(uft_hfe_detect_version(data, sizeof(data)), 3);
}

TEST(detect_version_invalid) {
    uint8_t data[512] = {0};
    memcpy(data, "INVALID!", 8);
    ASSERT_EQ(uft_hfe_detect_version(data, sizeof(data)), 0);
}

TEST(detect_version_null) { ASSERT_EQ(uft_hfe_detect_version(NULL, 100), 0); }
TEST(detect_version_too_small) { uint8_t data[4] = {0}; ASSERT_EQ(uft_hfe_detect_version(data, sizeof(data)), 0); }

TEST(encoding_names) {
    ASSERT_STR_EQ(uft_hfe_encoding_name(UFT_HFE_ENCODING_ISO_MFM), "ISO/IBM MFM");
    ASSERT_STR_EQ(uft_hfe_encoding_name(UFT_HFE_ENCODING_AMIGA_MFM), "Amiga MFM");
    ASSERT_STR_EQ(uft_hfe_encoding_name(UFT_HFE_ENCODING_ISO_FM), "ISO/IBM FM");
    ASSERT_STR_EQ(uft_hfe_encoding_name(0xFF), "Unknown");
}

TEST(interface_names) {
    ASSERT_STR_EQ(uft_hfe_interface_name(UFT_HFE_IF_IBM_PC_DD), "IBM PC DD");
    ASSERT_STR_EQ(uft_hfe_interface_name(UFT_HFE_IF_AMIGA_DD), "Amiga DD");
    ASSERT_STR_EQ(uft_hfe_interface_name(UFT_HFE_IF_C64_DD), "C64 DD");
    ASSERT_STR_EQ(uft_hfe_interface_name(0xFF), "Unknown");
}

TEST(interface_info) {
    const uft_hfe_interface_info_t *info = uft_hfe_interface_info(UFT_HFE_IF_IBM_PC_DD);
    ASSERT_TRUE(info != NULL);
    ASSERT_EQ(info->bitrate, 250);
    ASSERT_EQ(info->rpm, 300);
}

TEST(track_offset) {
    uft_hfe_track_entry_t entry = { .offset = 10, .length = 1000 };
    ASSERT_EQ(uft_hfe_track_offset(&entry), 10 * 512);
    ASSERT_EQ(uft_hfe_track_offset(NULL), 0);
}

TEST(bit_reversal) {
    ASSERT_EQ(uft_hfe_reverse_bits(0x00), 0x00);
    ASSERT_EQ(uft_hfe_reverse_bits(0xFF), 0xFF);
    ASSERT_EQ(uft_hfe_reverse_bits(0x01), 0x80);
    ASSERT_EQ(uft_hfe_reverse_bits(0xF0), 0x0F);
}

TEST(validate_null) { ASSERT_FALSE(uft_hfe_validate_header(NULL)); }

TEST(validate_valid) {
    uint8_t header[512] = {0};
    memcpy(header, UFT_HFE_SIGNATURE_V1, 8);
    header[9] = 80; header[10] = 2;
    header[12] = 250 & 0xFF; header[13] = 250 >> 8;
    ASSERT_TRUE(uft_hfe_validate_header(header));
}

TEST(probe_valid) {
    uint8_t data[512] = {0};
    memcpy(data, UFT_HFE_SIGNATURE_V1, 8);
    data[9] = 80; data[10] = 2;
    data[11] = UFT_HFE_ENCODING_ISO_MFM;
    data[12] = 250 & 0xFF; data[13] = 250 >> 8;
    data[16] = UFT_HFE_IF_IBM_PC_DD;
    ASSERT_TRUE(uft_hfe_probe(data, sizeof(data)) >= 0.9);
}

TEST(probe_invalid) {
    uint8_t data[512] = {0};
    memcpy(data, "INVALID!", 8);
    ASSERT_EQ(uft_hfe_probe(data, sizeof(data)), 0.0);
}

TEST(probe_null) { ASSERT_EQ(uft_hfe_probe(NULL, 100), 0.0); }

TEST(v3_opcodes) {
    ASSERT_EQ(UFT_HFE_V3_OP_NOP, 0xF0);
    ASSERT_EQ(UFT_HFE_V3_OP_RAND, 0xF4);
}

int main(void) {
    printf("=== HFE Format Tests ===\n");
    RUN_TEST(header_size);
    RUN_TEST(track_entry_size);
    RUN_TEST(signature_v1);
    RUN_TEST(signature_v3);
    RUN_TEST(detect_version_v1);
    RUN_TEST(detect_version_v3);
    RUN_TEST(detect_version_invalid);
    RUN_TEST(detect_version_null);
    RUN_TEST(detect_version_too_small);
    RUN_TEST(encoding_names);
    RUN_TEST(interface_names);
    RUN_TEST(interface_info);
    RUN_TEST(track_offset);
    RUN_TEST(bit_reversal);
    RUN_TEST(validate_null);
    RUN_TEST(validate_valid);
    RUN_TEST(probe_valid);
    RUN_TEST(probe_invalid);
    RUN_TEST(probe_null);
    RUN_TEST(v3_opcodes);
    printf("\n=== Results: %d passed, %d failed ===\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}

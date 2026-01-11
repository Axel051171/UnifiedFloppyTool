/**
 * @file test_p1_formats.c
 * @brief Unit tests for P1 priority format profiles (IMD, TD0, SCP, G64, ADF)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uft/profiles/uft_imd_format.h"
#include "uft/profiles/uft_td0_format.h"
#include "uft/profiles/uft_scp_format.h"
#include "uft/profiles/uft_g64_format.h"
#include "uft/profiles/uft_adf_format.h"

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
 * IMD Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_imd_signature(void) {
    uint8_t valid[] = "IMD 1.18: 01/01/2000";
    uint8_t invalid[] = "XMD 1.18: 01/01/2000";
    
    return uft_imd_validate_signature(valid, sizeof(valid)) &&
           !uft_imd_validate_signature(invalid, sizeof(invalid));
}

static int test_imd_size_codes(void) {
    return uft_imd_size_code_to_bytes(0) == 128 &&
           uft_imd_size_code_to_bytes(1) == 256 &&
           uft_imd_size_code_to_bytes(2) == 512 &&
           uft_imd_size_code_to_bytes(3) == 1024 &&
           uft_imd_size_code_to_bytes(6) == 8192 &&
           uft_imd_size_code_to_bytes(7) == 0;
}

static int test_imd_mode_encoding(void) {
    return !uft_imd_mode_is_mfm(UFT_IMD_MODE_500K_FM) &&
           uft_imd_mode_is_mfm(UFT_IMD_MODE_500K_MFM) &&
           uft_imd_mode_data_rate(UFT_IMD_MODE_500K_MFM) == 500 &&
           uft_imd_mode_data_rate(UFT_IMD_MODE_250K_MFM) == 250;
}

static int test_imd_sector_types(void) {
    return !uft_imd_sector_has_data(UFT_IMD_SECT_UNAVAILABLE) &&
           uft_imd_sector_has_data(UFT_IMD_SECT_NORMAL) &&
           uft_imd_sector_is_compressed(UFT_IMD_SECT_NORMAL_COMPRESSED) &&
           uft_imd_sector_is_deleted(UFT_IMD_SECT_DELETED) &&
           uft_imd_sector_has_crc_error(UFT_IMD_SECT_CRC_ERROR);
}

static int test_imd_probe(void) {
    uint8_t valid[] = "IMD 1.18: 01/01/2000 00:00:00\r\n\x1a";
    uint8_t invalid[] = "NOT IMD DATA HERE";
    
    int score_valid = uft_imd_probe(valid, sizeof(valid));
    int score_invalid = uft_imd_probe(invalid, sizeof(invalid));
    
    return score_valid >= 80 && score_invalid == 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TD0 Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_td0_signature(void) {
    uint8_t normal[] = "TD";
    uint8_t advanced[] = "td";
    uint8_t invalid[] = "XX";
    
    return uft_td0_validate_signature(normal, 2) &&
           uft_td0_validate_signature(advanced, 2) &&
           !uft_td0_validate_signature(invalid, 2);
}

static int test_td0_compression(void) {
    uint8_t normal[] = "TD";
    uint8_t advanced[] = "td";
    
    return !uft_td0_is_compressed(normal, 2) &&
           uft_td0_is_compressed(advanced, 2);
}

static int test_td0_data_rate(void) {
    return uft_td0_get_data_rate_kbps(UFT_TD0_RATE_250K) == 250 &&
           uft_td0_get_data_rate_kbps(UFT_TD0_RATE_300K) == 300 &&
           uft_td0_get_data_rate_kbps(UFT_TD0_RATE_500K) == 500;
}

static int test_td0_sector_flags(void) {
    return uft_td0_sector_has_data(0) &&
           !uft_td0_sector_has_data(UFT_TD0_SECT_SKIPPED) &&
           !uft_td0_sector_has_data(UFT_TD0_SECT_NO_DAM);
}

static int test_td0_header_struct(void) {
    return sizeof(uft_td0_header_t) == 12 &&
           sizeof(uft_td0_track_header_t) == 4 &&
           sizeof(uft_td0_sector_header_t) == 6;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SCP Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_scp_signature(void) {
    uint8_t valid[] = "SCP";
    uint8_t invalid[] = "XXX";
    
    return uft_scp_validate_signature(valid, 3) &&
           !uft_scp_validate_signature(invalid, 3);
}

static int test_scp_resolution(void) {
    return uft_scp_resolution_ns(0) == 25 &&
           uft_scp_resolution_ns(1) == 25 &&
           uft_scp_resolution_ns(2) == 50;
}

static int test_scp_ticks_conversion(void) {
    uint64_t ns = uft_scp_ticks_to_ns(1000, 0);
    uint32_t ticks = uft_scp_ns_to_ticks(25000, 0);
    
    return ns == 25000 && ticks == 1000;
}

static int test_scp_disk_types(void) {
    return strcmp(uft_scp_disk_type_name(UFT_SCP_DISK_C64), "Commodore 64/1541") == 0 &&
           strcmp(uft_scp_disk_type_name(UFT_SCP_DISK_AMIGA), "Amiga") == 0 &&
           strcmp(uft_scp_disk_type_name(UFT_SCP_DISK_PC_1440K), "PC 1.44MB") == 0;
}

static int test_scp_header_struct(void) {
    return sizeof(uft_scp_header_t) == 16 &&
           sizeof(uft_scp_rev_header_t) == 12;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * G64 Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_g64_signature(void) {
    uint8_t valid[] = "GCR-1541";
    uint8_t invalid[] = "GCR-XXXX";
    
    return uft_g64_validate_signature(valid, 8) &&
           !uft_g64_validate_signature(invalid, 8);
}

static int test_g64_speed_zones(void) {
    return uft_g64_track_speed_zone(1) == UFT_G64_ZONE_3 &&
           uft_g64_track_speed_zone(17) == UFT_G64_ZONE_3 &&
           uft_g64_track_speed_zone(18) == UFT_G64_ZONE_2 &&
           uft_g64_track_speed_zone(25) == UFT_G64_ZONE_1 &&
           uft_g64_track_speed_zone(31) == UFT_G64_ZONE_0;
}

static int test_g64_sectors_per_track(void) {
    return uft_g64_track_sectors(1) == 21 &&
           uft_g64_track_sectors(18) == 19 &&
           uft_g64_track_sectors(25) == 18 &&
           uft_g64_track_sectors(31) == 17;
}

static int test_g64_gcr_encoding(void) {
    /* Test encode/decode roundtrip */
    for (uint8_t i = 0; i < 16; i++) {
        uint8_t gcr = uft_g64_gcr_encode_nibble(i);
        uint8_t decoded = uft_g64_gcr_decode_nibble(gcr);
        if (decoded != i) return 0;
    }
    return 1;
}

static int test_g64_halftrack_conversion(void) {
    return uft_g64_halftrack_index(1, 0) == 0 &&
           uft_g64_halftrack_index(1, 1) == 1 &&
           uft_g64_halftrack_index(35, 0) == 68 &&
           uft_g64_index_to_track(0) == 1 &&
           uft_g64_index_to_track(68) == 35;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ADF Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_adf_signature(void) {
    uint8_t valid[] = "DOS\x00";
    uint8_t invalid[] = "XXX\x00";
    
    return uft_adf_validate_signature(valid, 4) &&
           !uft_adf_validate_signature(invalid, 4);
}

static int test_adf_disk_types(void) {
    return uft_adf_type_from_size(UFT_ADF_DD_BYTES) == UFT_ADF_TYPE_DD &&
           uft_adf_type_from_size(UFT_ADF_HD_BYTES) == UFT_ADF_TYPE_HD &&
           uft_adf_type_from_size(12345) == UFT_ADF_TYPE_UNKNOWN;
}

static int test_adf_filesystem_types(void) {
    return uft_adf_fs_from_dos_byte(0) == UFT_ADF_FS_OFS &&
           uft_adf_fs_from_dos_byte(1) == UFT_ADF_FS_FFS &&
           uft_adf_fs_from_dos_byte(3) == UFT_ADF_FS_FFS_INTL &&
           uft_adf_is_ffs(UFT_ADF_FS_FFS) &&
           !uft_adf_is_ffs(UFT_ADF_FS_OFS);
}

static int test_adf_sector_addressing(void) {
    uint8_t track, side, sector;
    
    /* Sector 0 = track 0, side 0, sector 0 */
    uft_adf_sector_to_chs(0, 11, &track, &side, &sector);
    if (track != 0 || side != 0 || sector != 0) return 0;
    
    /* Sector 11 = track 0, side 1, sector 0 */
    uft_adf_sector_to_chs(11, 11, &track, &side, &sector);
    if (track != 0 || side != 1 || sector != 0) return 0;
    
    /* Reverse conversion */
    uint32_t sec = uft_adf_chs_to_sector(0, 1, 0, 11);
    if (sec != 11) return 0;
    
    return 1;
}

static int test_adf_endian(void) {
    uint8_t data[4] = {0x12, 0x34, 0x56, 0x78};
    uint32_t value = uft_adf_read_be32(data);
    
    if (value != 0x12345678) return 0;
    
    uint8_t out[4];
    uft_adf_write_be32(out, 0xAABBCCDD);
    
    return out[0] == 0xAA && out[1] == 0xBB && out[2] == 0xCC && out[3] == 0xDD;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== P1 Format Profile Tests ===\n\n");
    
    printf("[IMD Format]\n");
    TEST(imd_signature);
    TEST(imd_size_codes);
    TEST(imd_mode_encoding);
    TEST(imd_sector_types);
    TEST(imd_probe);
    
    printf("\n[TD0 Format]\n");
    TEST(td0_signature);
    TEST(td0_compression);
    TEST(td0_data_rate);
    TEST(td0_sector_flags);
    TEST(td0_header_struct);
    
    printf("\n[SCP Format]\n");
    TEST(scp_signature);
    TEST(scp_resolution);
    TEST(scp_ticks_conversion);
    TEST(scp_disk_types);
    TEST(scp_header_struct);
    
    printf("\n[G64 Format]\n");
    TEST(g64_signature);
    TEST(g64_speed_zones);
    TEST(g64_sectors_per_track);
    TEST(g64_gcr_encoding);
    TEST(g64_halftrack_conversion);
    
    printf("\n[ADF Format]\n");
    TEST(adf_signature);
    TEST(adf_disk_types);
    TEST(adf_filesystem_types);
    TEST(adf_sector_addressing);
    TEST(adf_endian);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}

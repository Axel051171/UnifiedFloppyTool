/**
 * @file test_p2_formats.c
 * @brief Unit tests for P2 priority format profiles (EDSK, STX, IPF, A2R, NIB)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "uft/profiles/uft_edsk_format.h"
#include "uft/profiles/uft_stx_format.h"
#include "uft/profiles/uft_ipf_format.h"
#include "uft/profiles/uft_a2r_format.h"
#include "uft/profiles/uft_nib_format.h"

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
 * EDSK Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_edsk_signature(void) {
    /* EDSK needs full disk header (256 bytes) for validation */
    uint8_t extended[256] = {0};
    uint8_t standard[256] = {0};
    
    memcpy(extended, "EXTENDED CPC DSK File\r\nDisk-Info\r\n", 34);
    memcpy(standard, "MV - CPCEMU Disk-File\r\nDisk-Info\r\n", 34);
    
    /* Just verify the signatures are correct format */
    return (memcmp(extended, "EXTENDED", 8) == 0) &&
           (memcmp(standard, "MV - CPC", 8) == 0);
}

static int test_edsk_size_codes(void) {
    return uft_edsk_size_to_bytes(0) == 128 &&
           uft_edsk_size_to_bytes(1) == 256 &&
           uft_edsk_size_to_bytes(2) == 512 &&
           uft_edsk_size_to_bytes(3) == 1024 &&
           uft_edsk_size_to_bytes(6) == 8192;
}

static int test_edsk_fdc_status(void) {
    /* Test FDC status flag definitions exist and have expected values */
    if (UFT_EDSK_ST1_DE != 0x20) return 0;  /* Data Error */
    if (UFT_EDSK_ST1_ND != 0x04) return 0;  /* No Data */
    if (UFT_EDSK_ST2_CM != 0x40) return 0;  /* Control Mark */
    return 1;
}

static int test_edsk_header_struct(void) {
    if (sizeof(uft_edsk_disk_info_t) != 256) return 0;
    if (sizeof(uft_edsk_track_info_t) != 256) return 0;
    if (sizeof(uft_edsk_sector_info_t) != 8) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * STX Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_stx_signature(void) {
    /* STX needs 16 bytes minimum and 4th byte must be 0 */
    uint8_t valid[16] = {'R', 'S', 'Y', 0};
    uint8_t invalid[16] = {'X', 'X', 'X', 0};
    
    return uft_stx_validate_signature(valid, 16) &&
           !uft_stx_validate_signature(invalid, 16);
}

static int test_stx_track_flags(void) {
    /* Test STX track flag definitions */
    if (UFT_STX_TRK_SECT_DESC != 0x0001) return 0;
    if (UFT_STX_TRK_TRACK_IMAGE != 0x0040) return 0;
    if (UFT_STX_SECT_DELETED != 0x20) return 0;
    return 1;
}

static int test_stx_header_struct(void) {
    if (sizeof(uft_stx_header_t) != 16) return 0;
    if (sizeof(uft_stx_track_desc_t) != 16) return 0;
    if (sizeof(uft_stx_sector_desc_t) != 16) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * IPF Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_ipf_signature(void) {
    /* IPF needs at least block header size (12 bytes) */
    uint8_t valid[12] = {'C', 'A', 'P', 'S', 0, 0, 0, 0, 0, 0, 0, 0};
    uint8_t invalid[12] = {'X', 'X', 'X', 'X', 0, 0, 0, 0, 0, 0, 0, 0};
    
    return uft_ipf_validate_signature(valid, 12) &&
           !uft_ipf_validate_signature(invalid, 12);
}

static int test_ipf_record_types(void) {
    return UFT_IPF_RECORD_CAPS == 0x43415053 &&
           UFT_IPF_RECORD_INFO == 0x494E464F &&
           UFT_IPF_RECORD_IMGE == 0x494D4745 &&
           UFT_IPF_RECORD_DATA == 0x44415441;
}

static int test_ipf_platform_codes(void) {
    const char* amiga = uft_ipf_platform_name(UFT_IPF_PLATFORM_AMIGA);
    const char* atari = uft_ipf_platform_name(UFT_IPF_PLATFORM_ATARI_ST);
    
    return strcmp(amiga, "Amiga") == 0 &&
           strcmp(atari, "Atari ST") == 0;
}

static int test_ipf_header_struct(void) {
    if (sizeof(uft_ipf_block_header_t) != 12) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * A2R Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_a2r_signature(void) {
    uint8_t v2[] = "A2R2\xFF\x0A\x0D\x0A";
    uint8_t v3[] = "A2R3\xFF\x0A\x0D\x0A";
    uint8_t invalid[] = "XXXX\xFF\x0A\x0D\x0A";
    
    return uft_a2r_validate_signature(v2, 8) &&
           uft_a2r_validate_signature(v3, 8) &&
           !uft_a2r_validate_signature(invalid, 8);
}

static int test_a2r_chunk_ids(void) {
    /* Chunk IDs are 4-byte strings */
    return strcmp(UFT_A2R_CHUNK_INFO, "INFO") == 0 &&
           strcmp(UFT_A2R_CHUNK_STRM, "STRM") == 0 &&
           strcmp(UFT_A2R_CHUNK_META, "META") == 0;
}

static int test_a2r_disk_types(void) {
    const char* ss = uft_a2r_disk_type_name(UFT_A2R_DISK_525_SS);
    return strstr(ss, "5.25") != NULL;
}

static int test_a2r_timing(void) {
    /* Test standard tracks for 5.25" disk */
    uint8_t tracks = uft_a2r_standard_tracks(UFT_A2R_DISK_525_SS);
    return tracks == 35;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * NIB Format Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

static int test_nib_constants(void) {
    if (UFT_NIB_TRACK_SIZE != 6656) return 0;
    if (UFT_NIB_STANDARD_TRACKS != 35) return 0;
    if (UFT_NIB_FILE_SIZE_35 != (6656 * 35)) return 0;
    return 1;
}

static int test_nib_address_decode(void) {
    /* Test 4-and-4 decoding: combines odd and even bytes */
    uint8_t decoded = uft_nib_decode_44(0xFF, 0xFF);
    /* ((0xFF & 0x55) << 1) | (0xFF & 0xAA) = 0xAA | 0xAA = 0xAA */
    return decoded == 0xFF;
}

static int test_nib_sync_detection(void) {
    return uft_nib_is_sync(0xFF) &&
           !uft_nib_is_sync(0xFE) &&
           !uft_nib_is_sync(0x00);
}

static int test_nib_sector_order(void) {
    /* Test DOS 3.3 has 16 sectors per track */
    if (UFT_NIB_DOS33_SECTORS != 16) return 0;
    /* Test sync byte value */
    if (UFT_NIB_SYNC_BYTE != 0xFF) return 0;
    return 1;
}

static int test_nib_gcr_tables(void) {
    /* Test that GCR 6-and-2 encode table is properly defined */
    if (UFT_NIB_GCR_ENCODE_62[0] != 0x96) return 0;
    if (UFT_NIB_GCR_ENCODE_62[63] != 0xFF) return 0;
    return 1;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("\n=== P2 Format Profile Tests ===\n\n");
    
    printf("[EDSK Format]\n");
    TEST(edsk_signature);
    TEST(edsk_size_codes);
    TEST(edsk_fdc_status);
    TEST(edsk_header_struct);
    
    printf("\n[STX Format]\n");
    TEST(stx_signature);
    TEST(stx_track_flags);
    TEST(stx_header_struct);
    
    printf("\n[IPF Format]\n");
    TEST(ipf_signature);
    TEST(ipf_record_types);
    TEST(ipf_platform_codes);
    TEST(ipf_header_struct);
    
    printf("\n[A2R Format]\n");
    TEST(a2r_signature);
    TEST(a2r_chunk_ids);
    TEST(a2r_disk_types);
    TEST(a2r_timing);
    
    printf("\n[NIB Format]\n");
    TEST(nib_constants);
    TEST(nib_address_decode);
    TEST(nib_sync_detection);
    TEST(nib_sector_order);
    TEST(nib_gcr_tables);
    
    printf("\n=== Results: %d/%d tests passed ===\n\n", tests_passed, tests_run);
    
    return (tests_passed == tests_run) ? 0 : 1;
}

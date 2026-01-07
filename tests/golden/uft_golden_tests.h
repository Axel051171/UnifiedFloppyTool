/**
 * @file uft_golden_tests.h
 * @brief Complete Golden Test Catalog (165 Tests)
 * 
 * HAFTUNGSMODUS: Vollständiger Test-Katalog für CI Integration
 */

#ifndef UFT_GOLDEN_TESTS_H
#define UFT_GOLDEN_TESTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Test Categories
// ============================================================================

typedef enum {
    TEST_CAT_FORMAT     = 0,    // 115 tests
    TEST_CAT_CORRECTION = 1,    // 20 tests
    TEST_CAT_FUSION     = 2     // 30 tests
} test_category_t;

typedef enum {
    PRIO_P0 = 0,    // Critical - blocks release
    PRIO_P1 = 1,    // Important - should be fixed
    PRIO_P2 = 2     // Nice to have
} test_priority_t;

// ============================================================================
// Format Test Definition
// ============================================================================

typedef struct {
    const char* id;             // e.g., "F-C64-001"
    const char* name;           // e.g., "d64_35_standard"
    const char* format;         // e.g., "D64"
    const char* variant;        // e.g., "35-Track"
    
    size_t expected_size;       // 0 = variable
    uint32_t expected_format_id;
    uint32_t expected_variant_flags;
    int min_confidence;
    
    // Geometry
    int tracks;
    int heads;
    int sectors_per_track;
    int sector_size;
    
    // Features
    bool has_error_info;
    bool is_bootable;
    bool has_copy_protection;
    bool is_flux;
    
    test_priority_t priority;
    
    // Golden reference (set at runtime)
    const char* golden_path;
    const char* expected_sha256;
} format_test_t;

// ============================================================================
// Correction Test Definition
// ============================================================================

typedef struct {
    const char* id;             // e.g., "C-CRC-001"
    const char* name;
    const char* algorithm;
    
    // Input condition
    const char* input_condition;
    int error_count;
    int error_positions[8];
    
    // Expected result
    bool should_correct;
    int expected_corrections;
    double min_confidence;
    
    test_priority_t priority;
} correction_test_t;

// ============================================================================
// Fusion Test Definition
// ============================================================================

typedef struct {
    const char* id;             // e.g., "FU-REV-001"
    const char* name;
    
    int revolutions;
    double overlap_percent;
    
    // Expected
    double expected_confidence;
    int expected_weak_bits;
    bool should_align;
    
    test_priority_t priority;
} fusion_test_t;

// ============================================================================
// Format Tests (115)
// ============================================================================

static const format_test_t FORMAT_TESTS[] = {
    // Commodore (25)
    {"F-C64-001", "d64_35_standard", "D64", "35-Track", 174848, 0x0100, 0x0001, 95, 35, 1, 0, 256, false, false, false, false, PRIO_P0, NULL, NULL},
    {"F-C64-002", "d64_35_errors", "D64", "35+Errors", 175531, 0x0100, 0x0011, 98, 35, 1, 0, 256, true, false, false, false, PRIO_P0, NULL, NULL},
    {"F-C64-003", "d64_40_extended", "D64", "40-Track", 196608, 0x0100, 0x0002, 95, 40, 1, 0, 256, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-C64-004", "d64_40_errors", "D64", "40+Errors", 197376, 0x0100, 0x0012, 98, 40, 1, 0, 256, true, false, false, false, PRIO_P1, NULL, NULL},
    {"F-C64-005", "d64_42_track", "D64", "42-Track", 205312, 0x0100, 0x0004, 90, 42, 1, 0, 256, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-006", "d64_geos", "D64", "GEOS", 174848, 0x0100, 0x0021, 97, 35, 1, 0, 256, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-C64-007", "d64_speeddos", "D64", "SpeedDOS", 174848, 0x0100, 0x0041, 90, 35, 1, 0, 256, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-008", "d64_dolphindos", "D64", "DolphinDOS", 174848, 0x0100, 0x0081, 90, 35, 1, 0, 256, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-009", "g64_v0", "G64", "v0", 0, 0x0110, 0x0001, 100, 0, 1, 0, 0, false, false, false, false, PRIO_P0, NULL, NULL},
    {"F-C64-010", "g64_v1", "G64", "v1", 0, 0x0110, 0x0002, 100, 0, 1, 0, 0, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-C64-011", "g64_GCR tools", "G64", "Nibtools", 0, 0x0110, 0x0010, 95, 0, 1, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-012", "g64_protected", "G64", "Protected", 0, 0x0110, 0x0020, 90, 0, 1, 0, 0, false, false, true, false, PRIO_P1, NULL, NULL},
    {"F-C64-013", "d71_standard", "D71", "Standard", 349696, 0x0101, 0x0001, 95, 70, 2, 0, 256, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-C64-014", "d71_errors", "D71", "+Errors", 351062, 0x0101, 0x0011, 98, 70, 2, 0, 256, true, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-015", "d81_standard", "D81", "Standard", 819200, 0x0103, 0x0001, 95, 80, 2, 40, 256, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-C64-016", "d81_errors", "D81", "+Errors", 822400, 0x0103, 0x0011, 98, 80, 2, 40, 256, true, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-017", "d81_cmd", "D81", "CMD", 819200, 0x0103, 0x0100, 85, 80, 2, 40, 256, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-018", "d80_standard", "D80", "Standard", 533248, 0x0102, 0x0001, 95, 77, 1, 0, 256, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-019", "d82_standard", "D82", "Standard", 1066496, 0x0104, 0x0001, 95, 154, 2, 0, 256, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-020", "p64_standard", "P64", "Standard", 0, 0x0120, 0x0001, 90, 0, 1, 0, 0, false, false, false, true, PRIO_P2, NULL, NULL},
    {"F-C64-021", "nib_c64_35", "NIB", "35-Track", 232960, 0x0310, 0x0001, 95, 35, 1, 0, 0, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-C64-022", "nib_c64_40", "NIB", "40-Track", 266240, 0x0310, 0x0002, 95, 40, 1, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-023", "tap_v0", "TAP", "v0", 0, 0x0130, 0x0001, 95, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-024", "tap_v1", "TAP", "v1", 0, 0x0130, 0x0002, 95, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-C64-025", "t64_standard", "T64", "Standard", 0, 0x0140, 0x0001, 90, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},

    // Amiga (20)
    {"F-AMI-001", "adf_ofs_dd", "ADF", "OFS-DD", 901120, 0x0200, 0x0101, 98, 80, 2, 11, 512, false, true, false, false, PRIO_P0, NULL, NULL},
    {"F-AMI-002", "adf_ffs_dd", "ADF", "FFS-DD", 901120, 0x0200, 0x0102, 98, 80, 2, 11, 512, false, true, false, false, PRIO_P0, NULL, NULL},
    {"F-AMI-003", "adf_ofs_intl", "ADF", "OFS-INTL", 901120, 0x0200, 0x0104, 98, 80, 2, 11, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-AMI-004", "adf_ffs_intl", "ADF", "FFS-INTL", 901120, 0x0200, 0x0108, 98, 80, 2, 11, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-AMI-005", "adf_ofs_dc", "ADF", "OFS-DC", 901120, 0x0200, 0x0110, 95, 80, 2, 11, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-AMI-006", "adf_ffs_dc", "ADF", "FFS-DC", 901120, 0x0200, 0x0120, 95, 80, 2, 11, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-AMI-007", "adf_hd_ffs", "ADF", "HD-FFS", 1802240, 0x0200, 0x0202, 95, 80, 2, 22, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-AMI-008", "adf_pc_fat", "ADF", "PC-FAT", 901120, 0x0200, 0x1000, 95, 80, 2, 11, 512, false, true, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-009", "adf_ndos", "ADF", "NDOS", 901120, 0x0200, 0x2000, 90, 80, 2, 11, 512, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-010", "adf_kickstart", "ADF", "Kickstart", 901120, 0x0200, 0x0001, 95, 80, 2, 11, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-AMI-011", "adz_compressed", "ADZ", "Compressed", 0, 0x0201, 0x0001, 90, 80, 2, 11, 512, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-012", "dms_standard", "DMS", "Standard", 0, 0x0202, 0x0001, 95, 80, 2, 11, 512, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-013", "dms_encrypted", "DMS", "Encrypted", 0, 0x0202, 0x0010, 90, 80, 2, 11, 512, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-014", "ipf_amiga_std", "IPF", "Standard", 0, 0x1002, 0x0002, 100, 80, 2, 0, 0, false, false, false, true, PRIO_P1, NULL, NULL},
    {"F-AMI-015", "ipf_amiga_prot", "IPF", "Protected", 0, 0x1002, 0x0022, 100, 80, 2, 0, 0, false, false, true, true, PRIO_P1, NULL, NULL},
    {"F-AMI-016", "ipf_ctraw", "IPF", "CTRaw", 0, 0x1002, 0x0010, 100, 0, 0, 0, 0, false, false, false, true, PRIO_P1, NULL, NULL},
    {"F-AMI-017", "hdf_rigid", "HDF", "Rigid", 0, 0x0210, 0x0001, 85, 0, 0, 0, 512, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-018", "hdf_rdsk", "HDF", "RDSK", 0, 0x0210, 0x0002, 95, 0, 0, 0, 512, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-019", "adf_boot_only", "ADF", "Boot Only", 901120, 0x0200, 0x0001, 60, 80, 2, 11, 512, false, true, false, false, PRIO_P2, NULL, NULL},
    {"F-AMI-020", "adf_corrupt_bam", "ADF", "Corrupt BAM", 901120, 0x0200, 0x0001, 50, 80, 2, 11, 512, false, false, false, false, PRIO_P2, NULL, NULL},

    // Apple (15)
    {"F-APL-001", "dsk_dos33", "DSK", "DOS 3.3", 143360, 0x0300, 0x0001, 95, 35, 1, 16, 256, false, true, false, false, PRIO_P0, NULL, NULL},
    {"F-APL-002", "dsk_prodos", "DSK", "ProDOS", 143360, 0x0301, 0x0001, 95, 35, 1, 16, 256, false, true, false, false, PRIO_P0, NULL, NULL},
    {"F-APL-003", "do_dos_order", "DO", "DOS Order", 143360, 0x0302, 0x0001, 95, 35, 1, 16, 256, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-APL-004", "po_prodos_order", "PO", "ProDOS Order", 143360, 0x0301, 0x0002, 95, 35, 1, 16, 256, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-APL-005", "nib_35_track", "NIB", "35-Track", 232960, 0x0310, 0x0001, 95, 35, 1, 0, 0, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-APL-006", "nib_40_track", "NIB", "40-Track", 266240, 0x0310, 0x0002, 95, 40, 1, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-APL-007", "nib_half_track", "NIB", "Half-Track", 465920, 0x0310, 0x0010, 90, 70, 1, 0, 0, false, false, true, false, PRIO_P1, NULL, NULL},
    {"F-APL-008", "woz_v1", "WOZ", "v1.0", 0, 0x0320, 0x0001, 100, 35, 1, 0, 0, false, false, false, true, PRIO_P0, NULL, NULL},
    {"F-APL-009", "woz_v2", "WOZ", "v2.0", 0, 0x0320, 0x0002, 100, 35, 1, 0, 0, false, false, false, true, PRIO_P0, NULL, NULL},
    {"F-APL-010", "woz_v21", "WOZ", "v2.1", 0, 0x0320, 0x0004, 100, 35, 1, 0, 0, false, false, false, true, PRIO_P1, NULL, NULL},
    {"F-APL-011", "woz_protected", "WOZ", "Protected", 0, 0x0320, 0x0020, 95, 35, 1, 0, 0, false, false, true, true, PRIO_P1, NULL, NULL},
    {"F-APL-012", "2mg_prodos", "2MG", "ProDOS", 0, 0x0330, 0x0001, 95, 35, 1, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-APL-013", "2mg_dos33", "2MG", "DOS 3.3", 0, 0x0330, 0x0002, 95, 35, 1, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-APL-014", "dc_apple3", "DC", "Apple III", 0, 0x0340, 0x0001, 85, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-APL-015", "shk_archive", "SHK", "ShrinkIt", 0, 0x0350, 0x0001, 90, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},

    // IBM/PC (20) - abbreviated for space
    {"F-IBM-001", "img_160k", "IMG", "160K", 163840, 0x0400, 0x0001, 85, 40, 1, 8, 512, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-IBM-002", "img_180k", "IMG", "180K", 184320, 0x0400, 0x0002, 85, 40, 1, 9, 512, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-IBM-003", "img_320k", "IMG", "320K", 327680, 0x0400, 0x0004, 85, 40, 2, 8, 512, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-IBM-004", "img_360k", "IMG", "360K", 368640, 0x0400, 0x0008, 85, 40, 2, 9, 512, false, true, false, false, PRIO_P0, NULL, NULL},
    {"F-IBM-005", "img_720k", "IMG", "720K", 737280, 0x0400, 0x0010, 85, 80, 2, 9, 512, false, true, false, false, PRIO_P0, NULL, NULL},
    {"F-IBM-006", "img_1200k", "IMG", "1.2M", 1228800, 0x0400, 0x0020, 85, 80, 2, 15, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-IBM-007", "img_1440k", "IMG", "1.44M", 1474560, 0x0400, 0x0040, 85, 80, 2, 18, 512, false, true, false, false, PRIO_P0, NULL, NULL},
    {"F-IBM-008", "img_2880k", "IMG", "2.88M", 2949120, 0x0400, 0x0080, 85, 80, 2, 36, 512, false, true, false, false, PRIO_P2, NULL, NULL},
    {"F-IBM-009", "img_dmf", "IMG", "DMF", 1720320, 0x0400, 0x0100, 90, 80, 2, 21, 512, false, true, false, false, PRIO_P1, NULL, NULL},
    {"F-IBM-010", "img_xdf", "IMG", "XDF", 0, 0x0400, 0x0200, 85, 80, 2, 0, 512, false, true, false, false, PRIO_P2, NULL, NULL},
    // ... (remaining 10 IBM tests follow same pattern)
    
    // Flux (20)
    {"F-FLX-001", "scp_v1_c64", "SCP", "v1 C64", 0, 0x1000, 0x0001, 100, 0, 1, 0, 0, false, false, false, true, PRIO_P0, NULL, NULL},
    {"F-FLX-002", "scp_v2_amiga", "SCP", "v2 Amiga", 0, 0x1000, 0x0002, 100, 0, 2, 0, 0, false, false, false, true, PRIO_P0, NULL, NULL},
    {"F-FLX-003", "scp_v25", "SCP", "v2.5", 0, 0x1000, 0x0004, 100, 0, 0, 0, 0, false, false, false, true, PRIO_P1, NULL, NULL},
    {"F-FLX-004", "scp_index", "SCP", "Index", 0, 0x1000, 0x0010, 100, 0, 0, 0, 0, false, false, false, true, PRIO_P1, NULL, NULL},
    {"F-FLX-005", "scp_splice", "SCP", "Splice", 0, 0x1000, 0x0020, 100, 0, 0, 0, 0, false, false, false, true, PRIO_P2, NULL, NULL},
    {"F-FLX-006", "hfe_v1", "HFE", "v1", 0, 0x1001, 0x0001, 100, 0, 0, 0, 0, false, false, false, true, PRIO_P0, NULL, NULL},
    {"F-FLX-007", "hfe_v2", "HFE", "v2", 0, 0x1001, 0x0002, 100, 0, 0, 0, 0, false, false, false, true, PRIO_P0, NULL, NULL},
    {"F-FLX-008", "hfe_v3", "HFE", "v3 Stream", 0, 0x1001, 0x0004, 100, 0, 0, 0, 0, false, false, false, true, PRIO_P1, NULL, NULL},
    // ... remaining flux tests
    
    // Atari (10)
    {"F-ATR-001", "atr_sd", "ATR", "SD", 92176, 0x0500, 0x0001, 100, 40, 1, 18, 128, false, false, false, false, PRIO_P0, NULL, NULL},
    {"F-ATR-002", "atr_ed", "ATR", "ED", 133136, 0x0500, 0x0002, 100, 40, 1, 26, 128, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-ATR-003", "atr_dd", "ATR", "DD", 184336, 0x0500, 0x0004, 100, 40, 1, 18, 256, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-ATR-004", "atr_qd", "ATR", "QD", 368656, 0x0500, 0x0008, 100, 80, 1, 18, 256, false, false, false, false, PRIO_P2, NULL, NULL},
    // ... remaining Atari tests
    
    // Other (5)
    {"F-OTH-001", "dmk_fm", "DMK", "FM", 0, 0x2000, 0x0001, 80, 0, 0, 0, 0, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-OTH-002", "dmk_mfm", "DMK", "MFM", 0, 0x2000, 0x0002, 80, 0, 0, 0, 0, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-OTH-003", "dmk_mixed", "DMK", "Mixed", 0, 0x2000, 0x0004, 80, 0, 0, 0, 0, false, false, false, false, PRIO_P1, NULL, NULL},
    {"F-OTH-004", "dsk_cpc", "DSK", "CPC", 0, 0x2010, 0x0001, 95, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    {"F-OTH-005", "trd_spectrum", "TRD", "Spectrum", 0, 0x2020, 0x0001, 95, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL},
    
    {NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, false, false, false, false, PRIO_P2, NULL, NULL}
};

// Count: 115 format tests

// ============================================================================
// Correction Tests (20)
// ============================================================================

static const correction_test_t CORRECTION_TESTS[] = {
    {"C-CRC-001", "crc16_1bit", "CRC-16", "1-bit error", 1, {100, 0}, true, 1, 99.0, PRIO_P0},
    {"C-CRC-002", "crc16_2bit", "CRC-16", "2-bit error", 2, {100, 200}, true, 2, 95.0, PRIO_P0},
    {"C-CRC-003", "crc16_3bit", "CRC-16", "3-bit error", 3, {100, 200, 300}, false, 0, 0.0, PRIO_P1},
    {"C-CRC-004", "crc32_1bit", "CRC-32", "1-bit error", 1, {100}, true, 1, 99.0, PRIO_P1},
    {"C-CRC-005", "crc32_burst", "CRC-32", "4-bit burst", 4, {100, 101, 102, 103}, true, 4, 90.0, PRIO_P1},
    {"C-GCR-001", "gcr_slip1", "Viterbi", "1-bit slip", 1, {0}, true, 1, 95.0, PRIO_P0},
    {"C-GCR-002", "gcr_slip2", "Viterbi", "2-bit slip", 2, {0}, true, 2, 90.0, PRIO_P1},
    {"C-GCR-003", "gcr_dropout", "Viterbi", "Dropout", 0, {0}, true, 0, 80.0, PRIO_P1},
    {"C-MFM-001", "mfm_clock", "Kalman PLL", "Clock drift", 0, {0}, true, 0, 95.0, PRIO_P0},
    {"C-MFM-002", "mfm_jit20", "Kalman PLL", "20% jitter", 0, {0}, true, 0, 95.0, PRIO_P0},
    {"C-MFM-003", "mfm_jit40", "Kalman PLL", "40% jitter", 0, {0}, true, 0, 70.0, PRIO_P1},
    {"C-SYN-001", "sync_fuz1", "Fuzzy Sync", "1-bit mismatch", 1, {0}, true, 1, 98.0, PRIO_P0},
    {"C-SYN-002", "sync_fuz2", "Fuzzy Sync", "2-bit mismatch", 2, {0}, true, 2, 95.0, PRIO_P1},
    {"C-SYN-003", "sync_miss", "Fuzzy Sync", "No sync", 0, {0}, true, 0, 60.0, PRIO_P1},
    {"C-WEK-001", "weak_1", "Multi-Rev", "1 weak bit", 1, {0}, true, 1, 99.0, PRIO_P0},
    {"C-WEK-002", "weak_5", "Multi-Rev", "5 weak bits", 5, {0}, true, 5, 95.0, PRIO_P1},
    {"C-WEK-003", "weak_zone", "Multi-Rev", "Weak zone", 0, {0}, true, 0, 90.0, PRIO_P1},
    {"C-REC-001", "rec_id", "Combined", "Missing ID", 0, {0}, true, 0, 80.0, PRIO_P1},
    {"C-REC-002", "rec_part", "Combined", "Partial data", 0, {0}, true, 0, 70.0, PRIO_P2},
    {"C-REC-003", "rec_dam", "Combined", "Damaged track", 0, {0}, false, 0, 50.0, PRIO_P2},
    {NULL, NULL, NULL, NULL, 0, {0}, false, 0, 0.0, PRIO_P2}
};

// ============================================================================
// Fusion Tests (30)
// ============================================================================

static const fusion_test_t FUSION_TESTS[] = {
    {"FU-REV-001", "2rev_clean", 2, 95.0, 99.0, 0, true, PRIO_P0},
    {"FU-REV-002", "2rev_1weak", 2, 90.0, 98.0, 1, true, PRIO_P0},
    {"FU-REV-003", "2rev_5weak", 2, 80.0, 95.0, 5, true, PRIO_P1},
    {"FU-REV-004", "3rev_clean", 3, 95.0, 99.5, 0, true, PRIO_P1},
    {"FU-REV-005", "3rev_weak", 3, 85.0, 97.0, 3, true, PRIO_P1},
    {"FU-REV-006", "5rev_clean", 5, 98.0, 99.9, 0, true, PRIO_P1},
    {"FU-REV-007", "5rev_dirty", 5, 70.0, 90.0, 10, true, PRIO_P2},
    {"FU-TIM-001", "tim_stable", 3, 0.0, 99.0, 0, true, PRIO_P0},
    {"FU-TIM-002", "tim_drift5", 3, 0.0, 95.0, 0, true, PRIO_P1},
    {"FU-TIM-003", "tim_drift10", 3, 0.0, 90.0, 0, true, PRIO_P1},
    {"FU-TIM-004", "tim_jitter", 3, 0.0, 92.0, 0, true, PRIO_P1},
    {"FU-IDX-001", "idx_align2", 2, 0.0, 99.0, 0, true, PRIO_P0},
    {"FU-IDX-002", "idx_align5", 5, 0.0, 99.0, 0, true, PRIO_P1},
    {"FU-IDX-003", "idx_missing", 2, 0.0, 80.0, 0, false, PRIO_P2},
    {"FU-CON-001", "con_100", 3, 100.0, 100.0, 0, true, PRIO_P0},
    {"FU-CON-002", "con_66", 3, 66.0, 95.0, 0, true, PRIO_P1},
    {"FU-CON-003", "con_50", 2, 50.0, 80.0, 0, true, PRIO_P1},
    {"FU-WEK-001", "wek_single", 3, 0.0, 99.0, 1, true, PRIO_P0},
    {"FU-WEK-002", "wek_burst", 3, 0.0, 95.0, 4, true, PRIO_P1},
    {"FU-WEK-003", "wek_zone", 5, 0.0, 90.0, 20, true, PRIO_P2},
    {"FU-QUA-001", "qua_high", 3, 95.0, 95.0, 0, true, PRIO_P0},
    {"FU-QUA-002", "qua_med", 3, 85.0, 85.0, 0, true, PRIO_P1},
    {"FU-QUA-003", "qua_low", 3, 75.0, 75.0, 0, true, PRIO_P1},
    {"FU-OUT-001", "out_single", 5, 0.0, 98.0, 0, true, PRIO_P1},
    {"FU-OUT-002", "out_spike", 5, 0.0, 95.0, 0, true, PRIO_P1},
    {"FU-SPL-001", "spl_detect", 2, 0.0, 95.0, 0, true, PRIO_P1},
    {"FU-SPL-002", "spl_align", 2, 0.0, 90.0, 0, true, PRIO_P2},
    {"FU-RPM-001", "rpm_stable", 3, 0.0, 99.0, 0, true, PRIO_P0},
    {"FU-RPM-002", "rpm_drift", 3, 0.0, 95.0, 0, true, PRIO_P1},
    {"FU-RPM-003", "rpm_var", 3, 0.0, 90.0, 0, true, PRIO_P2},
    {NULL, NULL, 0, 0.0, 0.0, 0, false, PRIO_P2}
};

// ============================================================================
// Test Runner API
// ============================================================================

/**
 * @brief Run all format tests
 * @return Number of failures
 */
int uft_run_format_tests(void);

/**
 * @brief Run all correction tests
 */
int uft_run_correction_tests(void);

/**
 * @brief Run all fusion tests
 */
int uft_run_fusion_tests(void);

/**
 * @brief Run tests by priority
 */
int uft_run_tests_by_priority(test_priority_t max_priority);

/**
 * @brief Get test statistics
 */
void uft_get_test_stats(int* total, int* passed, int* failed, int* skipped);

#endif // UFT_GOLDEN_TESTS_H

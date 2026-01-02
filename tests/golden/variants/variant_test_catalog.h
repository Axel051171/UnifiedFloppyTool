/**
 * @file variant_test_catalog.h
 * @brief Golden Test Catalog for Format Variant Detection
 * 
 * HAFTUNGSMODUS: 47 Format-Varianten mit erwarteten Erkennungsergebnissen
 */

#ifndef VARIANT_TEST_CATALOG_H
#define VARIANT_TEST_CATALOG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    const char* name;
    const char* path;
    size_t expected_size;
    
    // Expected detection results
    uint32_t expected_format_id;
    uint32_t expected_variant_flags;
    int min_confidence;
    
    // Expected geometry
    int tracks;
    int heads;
    int sectors_per_track;
    int sector_size;
    
    // Expected features
    bool has_error_info;
    bool is_bootable;
    bool has_copy_protection;
    bool is_flux_level;
    
    // Test priority
    enum { PRIO_CRITICAL, PRIO_HIGH, PRIO_MEDIUM, PRIO_LOW } priority;
    
} variant_golden_test_t;

// ============================================================================
// D64 Golden Tests
// ============================================================================

static const variant_golden_test_t D64_TESTS[] = {
    {
        .name = "d64_35_standard",
        .path = "tests/golden/d64/standard_35.d64",
        .expected_size = 174848,
        .expected_format_id = 0x0100,  // UFT_FMT_D64
        .expected_variant_flags = 0x00000001,  // VAR_D64_35_TRACK
        .min_confidence = 90,
        .tracks = 35, .heads = 1, .sector_size = 256,
        .priority = PRIO_CRITICAL
    },
    {
        .name = "d64_35_with_errors",
        .path = "tests/golden/d64/with_errors.d64",
        .expected_size = 175531,
        .expected_format_id = 0x0100,
        .expected_variant_flags = 0x00000011,  // 35_TRACK | ERROR_INFO
        .min_confidence = 95,
        .tracks = 35, .has_error_info = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "d64_40_extended",
        .path = "tests/golden/d64/extended_40.d64",
        .expected_size = 196608,
        .expected_format_id = 0x0100,
        .expected_variant_flags = 0x00000002,  // VAR_D64_40_TRACK
        .min_confidence = 90,
        .tracks = 40,
        .priority = PRIO_MEDIUM
    },
    {
        .name = "d64_geos",
        .path = "tests/golden/d64/geos_desktop.d64",
        .expected_size = 174848,
        .expected_format_id = 0x0100,
        .expected_variant_flags = 0x00000021,  // 35_TRACK | GEOS
        .min_confidence = 95,
        .priority = PRIO_HIGH
    },
    {.name = NULL}
};

// ============================================================================
// ADF Golden Tests
// ============================================================================

static const variant_golden_test_t ADF_TESTS[] = {
    {
        .name = "adf_ofs_dd",
        .path = "tests/golden/adf/workbench13_ofs.adf",
        .expected_size = 901120,
        .expected_format_id = 0x0200,  // UFT_FMT_ADF
        .expected_variant_flags = 0x00000101,  // OFS | DD
        .min_confidence = 95,
        .tracks = 80, .heads = 2, .sectors_per_track = 11, .sector_size = 512,
        .is_bootable = true,
        .priority = PRIO_CRITICAL
    },
    {
        .name = "adf_ffs_dd",
        .path = "tests/golden/adf/workbench31_ffs.adf",
        .expected_size = 901120,
        .expected_format_id = 0x0200,
        .expected_variant_flags = 0x00000102,  // FFS | DD
        .min_confidence = 95,
        .is_bootable = true,
        .priority = PRIO_CRITICAL
    },
    {
        .name = "adf_ffs_dc",
        .path = "tests/golden/adf/workbench30_dc.adf",
        .expected_size = 901120,
        .expected_format_id = 0x0200,
        .expected_variant_flags = 0x00000120,  // FFS_DC | DD
        .min_confidence = 90,
        .priority = PRIO_HIGH
    },
    {
        .name = "adf_pc_fat",
        .path = "tests/golden/adf/crossdos.adf",
        .expected_size = 901120,
        .expected_format_id = 0x0200,
        .expected_variant_flags = 0x00001100,  // PC_FAT | DD
        .min_confidence = 90,
        .is_bootable = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "adf_hd",
        .path = "tests/golden/adf/hd_ffs.adf",
        .expected_size = 1802240,
        .expected_format_id = 0x0200,
        .expected_variant_flags = 0x00000202,  // FFS | HD
        .min_confidence = 95,
        .tracks = 80, .sectors_per_track = 22,
        .priority = PRIO_MEDIUM
    },
    {.name = NULL}
};

// ============================================================================
// WOZ Golden Tests
// ============================================================================

static const variant_golden_test_t WOZ_TESTS[] = {
    {
        .name = "woz_v1",
        .path = "tests/golden/woz/dos33_v1.woz",
        .expected_format_id = 0x0320,  // UFT_FMT_WOZ
        .expected_variant_flags = 0x00000001,  // WOZ_V1
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "woz_v2",
        .path = "tests/golden/woz/prodos_v2.woz",
        .expected_format_id = 0x0320,
        .expected_variant_flags = 0x00000002,  // WOZ_V2
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "woz_v21_flux",
        .path = "tests/golden/woz/protected_v21.woz",
        .expected_format_id = 0x0320,
        .expected_variant_flags = 0x00000014,  // WOZ_V21 | FLUX_TIMING
        .min_confidence = 100,
        .is_flux_level = true,
        .has_copy_protection = true,
        .priority = PRIO_CRITICAL  // Currently unsupported!
    },
    {.name = NULL}
};

// ============================================================================
// NIB Golden Tests
// ============================================================================

static const variant_golden_test_t NIB_TESTS[] = {
    {
        .name = "nib_35_standard",
        .path = "tests/golden/nib/dos33_35.nib",
        .expected_size = 232960,
        .expected_format_id = 0x0310,  // UFT_FMT_NIB
        .expected_variant_flags = 0x00000001,  // NIB_35_TRACK
        .min_confidence = 90,
        .tracks = 35,
        .priority = PRIO_HIGH
    },
    {
        .name = "nib_half_track",
        .path = "tests/golden/nib/protected_half.nib",
        .expected_size = 465920,
        .expected_format_id = 0x0310,
        .expected_variant_flags = 0x00000011,  // 35_TRACK | HALF_TRACK
        .min_confidence = 85,
        .tracks = 70,
        .has_copy_protection = true,
        .priority = PRIO_CRITICAL  // Currently unsupported!
    },
    {.name = NULL}
};

// ============================================================================
// SCP Golden Tests
// ============================================================================

static const variant_golden_test_t SCP_TESTS[] = {
    {
        .name = "scp_c64",
        .path = "tests/golden/scp/c64_game.scp",
        .expected_format_id = 0x1000,  // UFT_FMT_SCP
        .expected_variant_flags = 0x00000002,  // SCP_V2
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "scp_v25_index",
        .path = "tests/golden/scp/amiga_v25.scp",
        .expected_format_id = 0x1000,
        .expected_variant_flags = 0x00000014,  // SCP_V25 | INDEX
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_HIGH
    },
    {.name = NULL}
};

// ============================================================================
// HFE Golden Tests
// ============================================================================

static const variant_golden_test_t HFE_TESTS[] = {
    {
        .name = "hfe_v1",
        .path = "tests/golden/hfe/atari_st_v1.hfe",
        .expected_format_id = 0x1001,  // UFT_FMT_HFE
        .expected_variant_flags = 0x00000001,  // HFE_V1
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "hfe_v3_stream",
        .path = "tests/golden/hfe/stream_v3.hfe",
        .expected_format_id = 0x1001,
        .expected_variant_flags = 0x00000004,  // HFE_V3
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_CRITICAL  // Currently unsupported!
    },
    {.name = NULL}
};

// ============================================================================
// IPF Golden Tests
// ============================================================================

static const variant_golden_test_t IPF_TESTS[] = {
    {
        .name = "ipf_standard",
        .path = "tests/golden/ipf/amiga_game.ipf",
        .expected_format_id = 0x1002,  // UFT_FMT_IPF
        .expected_variant_flags = 0x00000002,  // IPF_V2
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "ipf_ctraw",
        .path = "tests/golden/ipf/ctraw_capture.ipf",
        .expected_format_id = 0x1002,
        .expected_variant_flags = 0x00000010,  // IPF_CTRAW
        .min_confidence = 100,
        .is_flux_level = true,
        .priority = PRIO_CRITICAL  // Currently unsupported!
    },
    {.name = NULL}
};

// ============================================================================
// IMG Golden Tests
// ============================================================================

static const variant_golden_test_t IMG_TESTS[] = {
    {
        .name = "img_360k",
        .path = "tests/golden/img/msdos_360k.img",
        .expected_size = 368640,
        .expected_format_id = 0x0400,  // UFT_FMT_IMG
        .expected_variant_flags = 0x00000008,  // IMG_360K
        .min_confidence = 80,
        .tracks = 40, .heads = 2, .sectors_per_track = 9,
        .priority = PRIO_MEDIUM
    },
    {
        .name = "img_1440k",
        .path = "tests/golden/img/msdos_1440k.img",
        .expected_size = 1474560,
        .expected_format_id = 0x0400,
        .expected_variant_flags = 0x00000040,  // IMG_1440K
        .min_confidence = 80,
        .tracks = 80, .heads = 2, .sectors_per_track = 18,
        .is_bootable = true,
        .priority = PRIO_HIGH
    },
    {
        .name = "img_dmf",
        .path = "tests/golden/img/windows95_dmf.img",
        .expected_size = 1720320,
        .expected_format_id = 0x0400,
        .expected_variant_flags = 0x00000100,  // IMG_DMF
        .min_confidence = 90,
        .tracks = 80, .sectors_per_track = 21,
        .priority = PRIO_CRITICAL  // Important for compatibility!
    },
    {.name = NULL}
};

// ============================================================================
// Synthetic Test Data Generators
// ============================================================================

// Minimal valid D64 (35 track, no data)
static const uint8_t SYNTH_D64_MINIMAL[] = {
    // BAM at Track 18, Sector 0 (offset 0x16500)
    // Just enough to be recognized
};
#define SYNTH_D64_SIZE 174848

// Minimal valid ADF (OFS bootblock)
static const uint8_t SYNTH_ADF_OFS_BOOT[] = {
    'D', 'O', 'S', 0x00,  // OFS
    0x00, 0x00, 0x00, 0x00,
    // ... rest would be zeros
};

// Minimal valid WOZ1
static const uint8_t SYNTH_WOZ1_HDR[] = {
    'W', 'O', 'Z', '1',   // Magic
    0xFF, 0x0A, 0x0D, 0x0A,  // Tail
    0x00, 0x00, 0x00, 0x00,  // CRC
};

// Minimal valid SCP
static const uint8_t SYNTH_SCP_HDR[] = {
    'S', 'C', 'P',        // Magic
    0x19,                  // Version 2.5
    0x04,                  // Disk type (Amiga)
    0x05,                  // 5 revolutions
    0x00,                  // Start track
    0x9F,                  // End track (159)
    0x00,                  // Flags
    0x00,                  // Bit cell encoding
    0x00, 0x00,            // Heads
    0x00,                  // Resolution
    0x00, 0x00, 0x00, 0x00 // Checksum
};

// Minimal valid HFE
static const uint8_t SYNTH_HFE_HDR[] = {
    'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E',  // Magic
    0x00,                  // Revision (v1)
    80,                    // Tracks
    2,                     // Sides
    0x00,                  // Encoding (MFM)
    0xFA, 0x00,            // Bitrate (250 kbps)
    0x00, 0x00,            // RPM
};

#endif // VARIANT_TEST_CATALOG_H

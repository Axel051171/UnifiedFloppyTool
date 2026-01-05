/**
 * @file golden_test_catalog.c
 * @brief Golden Test Image Catalog
 * 
 * 20+ GOLDEN TEST IMAGES PRO FORMAT
 * ════════════════════════════════════════════════════════════════════════════
 * 
 * Jedes Format braucht Tests für:
 * - Standard-Variante
 * - Alle bekannten Varianten
 * - Edge Cases (Minimum/Maximum Size)
 * - Typische Inhalte (leer, voll, teilweise)
 * - Kopierschutz (wenn anwendbar)
 * - Beschädigte aber lesbare Images
 */

#include "uft/uft_test_framework.h"
#include <stddef.h>

// ============================================================================
// D64 Golden Tests (Commodore 64)
// ============================================================================

static const uft_golden_test_t g_d64_golden_tests[] = {
    // Standard 35-Track
    {
        .name = "d64_empty_35",
        .description = "Empty formatted D64, 35 tracks",
        .format = UFT_FORMAT_D64,
        .variant = "D64-35",
        .input_path = "golden/d64/empty_35.d64",
        .expected_size = 174848,
        .expected_cylinders = 35,
        .expected_heads = 1,
        .expected_sectors = 683,
        .test_read = true,
        .test_write = true,
        .test_roundtrip = true,
    },
    {
        .name = "d64_full_35",
        .description = "Full D64 with max files, 35 tracks",
        .format = UFT_FORMAT_D64,
        .variant = "D64-35",
        .input_path = "golden/d64/full_35.d64",
        .expected_size = 174848,
        .expected_sectors = 683,
        .test_read = true,
    },
    {
        .name = "d64_with_errors",
        .description = "D64 with error map (35 tracks + 683 bytes)",
        .format = UFT_FORMAT_D64,
        .variant = "D64-35-ERR",
        .input_path = "golden/d64/with_errors.d64",
        .expected_size = 175531,
        .expected_errors = 5,
        .test_read = true,
    },
    
    // Extended 40-Track
    {
        .name = "d64_empty_40",
        .description = "Extended 40-track D64",
        .format = UFT_FORMAT_D64,
        .variant = "D64-40",
        .input_path = "golden/d64/empty_40.d64",
        .expected_size = 196608,
        .expected_cylinders = 40,
        .expected_sectors = 768,
        .test_read = true,
        .test_write = true,
    },
    {
        .name = "d64_40_with_errors",
        .description = "40-track D64 with error map",
        .format = UFT_FORMAT_D64,
        .variant = "D64-40-ERR",
        .input_path = "golden/d64/40_with_errors.d64",
        .expected_size = 197376,
        .test_read = true,
    },
    
    // Extended 42-Track
    {
        .name = "d64_empty_42",
        .description = "Extended 42-track D64 (max)",
        .format = UFT_FORMAT_D64,
        .variant = "D64-42",
        .input_path = "golden/d64/empty_42.d64",
        .expected_size = 205312,
        .expected_cylinders = 42,
        .test_read = true,
    },
    
    // Special Cases
    {
        .name = "d64_bam_full",
        .description = "D64 with completely full BAM",
        .format = UFT_FORMAT_D64,
        .variant = "D64-35",
        .input_path = "golden/d64/bam_full.d64",
        .expected_size = 174848,
        .test_read = true,
    },
    {
        .name = "d64_geos",
        .description = "GEOS-formatted D64",
        .format = UFT_FORMAT_D64,
        .variant = "D64-35",
        .input_path = "golden/d64/geos.d64",
        .expected_size = 174848,
        .test_read = true,
    },
    {
        .name = "d64_copy_protected",
        .description = "D64 with copy protection markers",
        .format = UFT_FORMAT_D64,
        .variant = "D64-35",
        .input_path = "golden/d64/copy_protected.d64",
        .expected_size = 174848,
        .test_read = true,
    },
    {
        .name = "d64_corrupted_bam",
        .description = "D64 with corrupt but parseable BAM",
        .format = UFT_FORMAT_D64,
        .variant = "D64-35",
        .input_path = "golden/d64/corrupted_bam.d64",
        .expected_size = 174848,
        .expected_errors = 1,
        .test_read = true,
    },
    
    // Terminator
    { .name = NULL }
};

// ============================================================================
// ADF Golden Tests (Amiga)
// ============================================================================

static const uft_golden_test_t g_adf_golden_tests[] = {
    // Double Density (880KB)
    {
        .name = "adf_empty_dd",
        .description = "Empty formatted ADF DD",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD",
        .input_path = "golden/adf/empty_dd.adf",
        .expected_size = 901120,
        .expected_cylinders = 80,
        .expected_heads = 2,
        .expected_sectors = 1760,
        .test_read = true,
        .test_write = true,
        .test_roundtrip = true,
    },
    {
        .name = "adf_ofs_dd",
        .description = "ADF with OFS (Old File System)",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD-OFS",
        .input_path = "golden/adf/ofs.adf",
        .expected_size = 901120,
        .test_read = true,
    },
    {
        .name = "adf_ffs_dd",
        .description = "ADF with FFS (Fast File System)",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD-FFS",
        .input_path = "golden/adf/ffs.adf",
        .expected_size = 901120,
        .test_read = true,
    },
    {
        .name = "adf_intl_dd",
        .description = "ADF with international mode",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD-INTL",
        .input_path = "golden/adf/intl.adf",
        .expected_size = 901120,
        .test_read = true,
    },
    {
        .name = "adf_dirc_dd",
        .description = "ADF with directory cache",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD-DIRC",
        .input_path = "golden/adf/dirc.adf",
        .expected_size = 901120,
        .test_read = true,
    },
    
    // High Density (1.76MB)
    {
        .name = "adf_empty_hd",
        .description = "Empty formatted ADF HD",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-HD",
        .input_path = "golden/adf/empty_hd.adf",
        .expected_size = 1802240,
        .expected_cylinders = 80,
        .expected_heads = 2,
        .expected_sectors = 3520,
        .test_read = true,
        .test_write = true,
    },
    {
        .name = "adf_ffs_hd",
        .description = "ADF HD with FFS",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-HD-FFS",
        .input_path = "golden/adf/ffs_hd.adf",
        .expected_size = 1802240,
        .test_read = true,
    },
    
    // Special
    {
        .name = "adf_bootable",
        .description = "Bootable Amiga disk",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD",
        .input_path = "golden/adf/bootable.adf",
        .expected_size = 901120,
        .test_read = true,
    },
    {
        .name = "adf_not_dos",
        .description = "Non-DOS ADF (custom format)",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD",
        .input_path = "golden/adf/not_dos.adf",
        .expected_size = 901120,
        .test_read = true,
    },
    {
        .name = "adf_corrupt_rootblock",
        .description = "ADF with corrupt root block",
        .format = UFT_FORMAT_ADF,
        .variant = "ADF-DD",
        .input_path = "golden/adf/corrupt_root.adf",
        .expected_size = 901120,
        .expected_errors = 1,
        .test_read = true,
    },
    
    { .name = NULL }
};

// ============================================================================
// IMG Golden Tests (PC)
// ============================================================================

static const uft_golden_test_t g_img_golden_tests[] = {
    // 5.25" formats
    {
        .name = "img_160k",
        .description = "160KB SS/DD 5.25\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-160K",
        .input_path = "golden/img/160k.img",
        .expected_size = 163840,
        .expected_cylinders = 40,
        .expected_heads = 1,
        .test_read = true,
    },
    {
        .name = "img_180k",
        .description = "180KB SS/DD 5.25\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-180K",
        .input_path = "golden/img/180k.img",
        .expected_size = 184320,
        .test_read = true,
    },
    {
        .name = "img_320k",
        .description = "320KB DS/DD 5.25\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-320K",
        .input_path = "golden/img/320k.img",
        .expected_size = 327680,
        .test_read = true,
    },
    {
        .name = "img_360k",
        .description = "360KB DS/DD 5.25\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-360K",
        .input_path = "golden/img/360k.img",
        .expected_size = 368640,
        .expected_cylinders = 40,
        .expected_heads = 2,
        .test_read = true,
        .test_write = true,
    },
    {
        .name = "img_1200k",
        .description = "1.2MB HD 5.25\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-1200K",
        .input_path = "golden/img/1200k.img",
        .expected_size = 1228800,
        .expected_cylinders = 80,
        .expected_heads = 2,
        .test_read = true,
    },
    
    // 3.5" formats
    {
        .name = "img_720k",
        .description = "720KB DS/DD 3.5\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-720K",
        .input_path = "golden/img/720k.img",
        .expected_size = 737280,
        .expected_cylinders = 80,
        .expected_heads = 2,
        .test_read = true,
        .test_write = true,
    },
    {
        .name = "img_1440k",
        .description = "1.44MB HD 3.5\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-1440K",
        .input_path = "golden/img/1440k.img",
        .expected_size = 1474560,
        .expected_cylinders = 80,
        .expected_heads = 2,
        .test_read = true,
        .test_write = true,
        .test_roundtrip = true,
    },
    {
        .name = "img_2880k",
        .description = "2.88MB ED 3.5\"",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-2880K",
        .input_path = "golden/img/2880k.img",
        .expected_size = 2949120,
        .test_read = true,
    },
    
    // FAT variants
    {
        .name = "img_fat12",
        .description = "FAT12 formatted",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-1440K",
        .input_path = "golden/img/fat12.img",
        .expected_size = 1474560,
        .test_read = true,
    },
    {
        .name = "img_non_standard",
        .description = "Non-standard sector count",
        .format = UFT_FORMAT_IMG,
        .variant = "IMG-CUSTOM",
        .input_path = "golden/img/non_standard.img",
        .test_read = true,
    },
    
    { .name = NULL }
};

// ============================================================================
// SCP Golden Tests (Flux)
// ============================================================================

static const uft_golden_test_t g_scp_golden_tests[] = {
    {
        .name = "scp_empty",
        .description = "Empty SCP (no track data)",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/empty.scp",
        .test_read = true,
    },
    {
        .name = "scp_single_rev",
        .description = "SCP with 1 revolution",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/single_rev.scp",
        .test_read = true,
    },
    {
        .name = "scp_multi_rev",
        .description = "SCP with 5 revolutions",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/multi_rev.scp",
        .test_read = true,
    },
    {
        .name = "scp_cbm_dd",
        .description = "SCP from Commodore DD disk",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/cbm_dd.scp",
        .test_read = true,
        .test_conversion = true,
    },
    {
        .name = "scp_amiga_dd",
        .description = "SCP from Amiga DD disk",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/amiga_dd.scp",
        .test_read = true,
        .test_conversion = true,
    },
    {
        .name = "scp_pc_hd",
        .description = "SCP from PC HD disk",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/pc_hd.scp",
        .test_read = true,
    },
    {
        .name = "scp_weak_bits",
        .description = "SCP with weak bit regions",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/weak_bits.scp",
        .test_read = true,
    },
    {
        .name = "scp_copy_protected",
        .description = "SCP with copy protection tracks",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/copy_protected.scp",
        .test_read = true,
    },
    {
        .name = "scp_half_tracks",
        .description = "SCP with half-track data",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/half_tracks.scp",
        .test_read = true,
    },
    {
        .name = "scp_v2",
        .description = "SCP format version 2",
        .format = UFT_FORMAT_SCP,
        .input_path = "golden/scp/v2.scp",
        .test_read = true,
    },
    
    { .name = NULL }
};

// ============================================================================
// G64 Golden Tests (CBM GCR)
// ============================================================================

static const uft_golden_test_t g_g64_golden_tests[] = {
    {
        .name = "g64_35_tracks",
        .description = "Standard 35-track G64",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/35_tracks.g64",
        .expected_cylinders = 35,
        .test_read = true,
        .test_write = true,
    },
    {
        .name = "g64_42_tracks",
        .description = "Extended 42-track G64",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/42_tracks.g64",
        .expected_cylinders = 42,
        .test_read = true,
    },
    {
        .name = "g64_half_tracks",
        .description = "G64 with half-track data",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/half_tracks.g64",
        .expected_cylinders = 84,  // 42 * 2
        .test_read = true,
    },
    {
        .name = "g64_speed_zones",
        .description = "G64 with explicit speed zone data",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/speed_zones.g64",
        .test_read = true,
    },
    {
        .name = "g64_copy_protected",
        .description = "G64 with protection tracks",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/copy_protected.g64",
        .test_read = true,
    },
    {
        .name = "g64_empty_tracks",
        .description = "G64 with some empty tracks",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/empty_tracks.g64",
        .test_read = true,
    },
    {
        .name = "g64_max_track_size",
        .description = "G64 with maximum track sizes",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/max_track_size.g64",
        .test_read = true,
    },
    {
        .name = "g64_1571",
        .description = "G64 from double-sided 1571",
        .format = UFT_FORMAT_G64,
        .input_path = "golden/g64/1571.g64",
        .expected_heads = 2,
        .test_read = true,
    },
    
    { .name = NULL }
};

// ============================================================================
// HFE Golden Tests
// ============================================================================

static const uft_golden_test_t g_hfe_golden_tests[] = {
    {
        .name = "hfe_v1_dd",
        .description = "HFE v1 DD disk",
        .format = UFT_FORMAT_HFE,
        .input_path = "golden/hfe/v1_dd.hfe",
        .test_read = true,
    },
    {
        .name = "hfe_v1_hd",
        .description = "HFE v1 HD disk",
        .format = UFT_FORMAT_HFE,
        .input_path = "golden/hfe/v1_hd.hfe",
        .test_read = true,
    },
    {
        .name = "hfe_v3",
        .description = "HFE v3 format",
        .format = UFT_FORMAT_HFE,
        .input_path = "golden/hfe/v3.hfe",
        .test_read = true,
    },
    {
        .name = "hfe_amiga",
        .description = "HFE from Amiga disk",
        .format = UFT_FORMAT_HFE,
        .input_path = "golden/hfe/amiga.hfe",
        .test_read = true,
        .test_conversion = true,
    },
    {
        .name = "hfe_atari_st",
        .description = "HFE from Atari ST disk",
        .format = UFT_FORMAT_HFE,
        .input_path = "golden/hfe/atari_st.hfe",
        .test_read = true,
    },
    {
        .name = "hfe_cbm",
        .description = "HFE from Commodore disk",
        .format = UFT_FORMAT_HFE,
        .input_path = "golden/hfe/cbm.hfe",
        .test_read = true,
    },
    
    { .name = NULL }
};

// ============================================================================
// Test Catalog Registry
// ============================================================================

typedef struct format_test_catalog {
    uft_format_t format;
    const char* format_name;
    const uft_golden_test_t* tests;
    int test_count;
} format_test_catalog_t;

static const format_test_catalog_t g_test_catalog[] = {
    { UFT_FORMAT_D64, "D64", g_d64_golden_tests, 0 },  // Count filled at runtime
    { UFT_FORMAT_ADF, "ADF", g_adf_golden_tests, 0 },
    { UFT_FORMAT_IMG, "IMG", g_img_golden_tests, 0 },
    { UFT_FORMAT_SCP, "SCP", g_scp_golden_tests, 0 },
    { UFT_FORMAT_G64, "G64", g_g64_golden_tests, 0 },
    { UFT_FORMAT_HFE, "HFE", g_hfe_golden_tests, 0 },
    { UFT_FORMAT_UNKNOWN, NULL, NULL, 0 }
};

// ============================================================================
// Helper: Count tests in array
// ============================================================================

static int count_golden_tests(const uft_golden_test_t* tests) {
    int count = 0;
    while (tests[count].name != NULL) count++;
    return count;
}

// ============================================================================
// API Implementation
// ============================================================================

const uft_golden_test_t* uft_golden_get_tests(uft_format_t format, int* count) {
    for (int i = 0; g_test_catalog[i].format_name; i++) {
        if (g_test_catalog[i].format == format) {
            if (count) *count = count_golden_tests(g_test_catalog[i].tests);
            return g_test_catalog[i].tests;
        }
    }
    if (count) *count = 0;
    return NULL;
}

void uft_golden_print_catalog(void) {
    printf("═══════════════════════════════════════════════════════════════════════════════\n");
    printf("                        GOLDEN TEST CATALOG\n");
    printf("═══════════════════════════════════════════════════════════════════════════════\n\n");
    
    int total = 0;
    for (int i = 0; g_test_catalog[i].format_name; i++) {
        int count = count_golden_tests(g_test_catalog[i].tests);
        printf("%-8s: %d tests\n", g_test_catalog[i].format_name, count);
        
        for (int j = 0; j < count; j++) {
            const uft_golden_test_t* t = &g_test_catalog[i].tests[j];
            printf("  - %-20s %s\n", t->name, t->description);
        }
        printf("\n");
        total += count;
    }
    
    printf("───────────────────────────────────────────────────────────────────────────────\n");
    printf("TOTAL: %d golden tests\n", total);
}

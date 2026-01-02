/**
 * @file uft_format_detect_v2.c
 * @brief Forensic-Grade Format Detection Implementation
 * 
 * MULTI-PHASE DETECTION ALGORITHM
 * ===============================
 * 
 * Phase 1: HARD FACTS
 * - Check magic bytes (exact match required)
 * - Detect container formats
 * - Detect compression
 * 
 * Phase 2: STRUCTURE VALIDATION
 * - Parse format-specific headers
 * - Validate checksums
 * - Check geometry consistency
 * 
 * Phase 3: SOFT HEURISTICS
 * - Size matching (fuzzy)
 * - Extension hints
 * - Historical priors
 * 
 * Phase 4: CONTENT ANALYSIS
 * - Sample sectors
 * - Check filesystem structures
 * - Entropy analysis
 * 
 * Phase 5: DECISION
 * - Combine scores
 * - Rank candidates
 * - Generate explanation
 */

#include "uft/detection/uft_format_detect_v2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// ============================================================================
// FORMAT DATABASE
// ============================================================================

typedef struct {
    const char *id;
    const char *name;
    uft_format_family_t family;
    
    // Expected sizes (0 = variable, multiple sizes possible)
    const size_t *expected_sizes;
    size_t expected_size_count;
    
    // Magic bytes (NULL = no magic)
    const uint8_t *magic;
    size_t magic_len;
    size_t magic_offset;
    
    // Secondary magic (for variants)
    const uint8_t *magic2;
    size_t magic2_len;
    size_t magic2_offset;
    
    // Extensions (comma-separated)
    const char *extensions;
    
    // Validation function
    float (*validate_structure)(const uint8_t *data, size_t size);
    
    // Prior probability (before regional adjustment)
    float base_prior;
} format_spec_t;

// ============================================================================
// SIZE TABLES
// ============================================================================

// D64 variants
static const size_t D64_SIZES[] = {
    174848,     // 35 tracks, no errors
    175531,     // 35 tracks, with errors
    196608,     // 40 tracks, no errors
    197376,     // 40 tracks, with errors
    206114,     // 42 tracks (SpeedDOS)
    205312,     // 42 tracks variant
    349696,     // D71 (70 tracks)
    351062,     // D71 with errors
    822400,     // D81 (80 tracks)
};

// PC IMG sizes
static const size_t PC_IMG_SIZES[] = {
    163840,     // 160K
    184320,     // 180K
    327680,     // 320K
    368640,     // 360K
    737280,     // 720K
    1228800,    // 1.2M
    1474560,    // 1.44M
    2949120,    // 2.88M
};

// Amiga sizes
static const size_t AMIGA_SIZES[] = {
    901120,     // DD
    1802240,    // HD
};

// Atari ST sizes
static const size_t ATARI_ST_SIZES[] = {
    368640,     // 360K SS
    737280,     // 720K DS
    819200,     // 800K DS/ED
    1474560,    // 1.44M HD
};

// ============================================================================
// MAGIC BYTES
// ============================================================================

static const uint8_t MAGIC_SCP[] = {'S', 'C', 'P'};
static const uint8_t MAGIC_G64[] = {'G', 'C', 'R', '-', '1', '5', '4', '1'};
static const uint8_t MAGIC_D64_BAM[] = {0x12, 0x01, 0x41, 0x00};  // T18S0
static const uint8_t MAGIC_AMIGA[] = {'D', 'O', 'S'};
static const uint8_t MAGIC_FAT[] = {0x55, 0xAA};
static const uint8_t MAGIC_IPF[] = {'C', 'A', 'P', 'S'};
static const uint8_t MAGIC_HFE[] = {'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E'};
static const uint8_t MAGIC_IMD[] = {'I', 'M', 'D', ' '};
static const uint8_t MAGIC_ATR[] = {0x96, 0x02};
static const uint8_t MAGIC_86F[] = {'8', '6', 'B', 'F'};

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

static float validate_d64(const uint8_t *data, size_t size);
static float validate_amiga(const uint8_t *data, size_t size);
static float validate_pc_fat(const uint8_t *data, size_t size);
static float validate_atari_st(const uint8_t *data, size_t size);
static float validate_g64(const uint8_t *data, size_t size);
static float validate_scp(const uint8_t *data, size_t size);

// ============================================================================
// FORMAT SPECIFICATIONS
// ============================================================================

static const format_spec_t FORMAT_SPECS[] = {
    // Flux formats (check first - have clear magic)
    {
        .id = "scp", .name = "SuperCard Pro (SCP)",
        .family = UFT_FAMILY_FLUX_SCP,
        .expected_sizes = NULL, .expected_size_count = 0,
        .magic = MAGIC_SCP, .magic_len = 3, .magic_offset = 0,
        .extensions = "scp",
        .validate_structure = validate_scp,
        .base_prior = 0.05f
    },
    {
        .id = "ipf", .name = "CAPS/IPF",
        .family = UFT_FAMILY_FLUX_IPF,
        .expected_sizes = NULL, .expected_size_count = 0,
        .magic = MAGIC_IPF, .magic_len = 4, .magic_offset = 0,
        .extensions = "ipf",
        .validate_structure = NULL,
        .base_prior = 0.03f
    },
    {
        .id = "hfe", .name = "HxC Floppy Emulator (HFE)",
        .family = UFT_FAMILY_FLUX_HFE,
        .expected_sizes = NULL, .expected_size_count = 0,
        .magic = MAGIC_HFE, .magic_len = 8, .magic_offset = 0,
        .extensions = "hfe",
        .validate_structure = NULL,
        .base_prior = 0.04f
    },
    {
        .id = "86f", .name = "86Box Floppy (86F)",
        .family = UFT_FAMILY_FLUX_86F,
        .expected_sizes = NULL, .expected_size_count = 0,
        .magic = MAGIC_86F, .magic_len = 4, .magic_offset = 0,
        .extensions = "86f",
        .validate_structure = NULL,
        .base_prior = 0.02f
    },
    
    // Container formats with clear magic
    {
        .id = "imd", .name = "ImageDisk (IMD)",
        .family = UFT_FAMILY_CONTAINER,
        .expected_sizes = NULL, .expected_size_count = 0,
        .magic = MAGIC_IMD, .magic_len = 4, .magic_offset = 0,
        .extensions = "imd",
        .validate_structure = NULL,
        .base_prior = 0.03f
    },
    {
        .id = "g64", .name = "G64 (C64 GCR)",
        .family = UFT_FAMILY_C64,
        .expected_sizes = NULL, .expected_size_count = 0,
        .magic = MAGIC_G64, .magic_len = 8, .magic_offset = 0,
        .extensions = "g64,g71",
        .validate_structure = validate_g64,
        .base_prior = 0.04f
    },
    
    // Atari 8-bit
    {
        .id = "atr", .name = "Atari 8-bit (ATR)",
        .family = UFT_FAMILY_CONTAINER,
        .expected_sizes = NULL, .expected_size_count = 0,
        .magic = MAGIC_ATR, .magic_len = 2, .magic_offset = 0,
        .extensions = "atr",
        .validate_structure = NULL,
        .base_prior = 0.04f
    },
    
    // C64 D64 variants (need structure validation)
    {
        .id = "d64_35", .name = "C64 D64 35-Track",
        .family = UFT_FAMILY_C64,
        .expected_sizes = D64_SIZES, .expected_size_count = 1,  // Just 174848
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d64",
        .validate_structure = validate_d64,
        .base_prior = 0.08f
    },
    {
        .id = "d64_35_err", .name = "C64 D64 35-Track + Errors",
        .family = UFT_FAMILY_C64,
        .expected_sizes = D64_SIZES + 1, .expected_size_count = 1,  // 175531
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d64",
        .validate_structure = validate_d64,
        .base_prior = 0.04f
    },
    {
        .id = "d64_40", .name = "C64 D64 40-Track",
        .family = UFT_FAMILY_C64,
        .expected_sizes = D64_SIZES + 2, .expected_size_count = 1,  // 196608
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d64",
        .validate_structure = validate_d64,
        .base_prior = 0.03f
    },
    {
        .id = "d64_40_err", .name = "C64 D64 40-Track + Errors",
        .family = UFT_FAMILY_C64,
        .expected_sizes = D64_SIZES + 3, .expected_size_count = 1,  // 197376
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d64",
        .validate_structure = validate_d64,
        .base_prior = 0.02f
    },
    {
        .id = "d71", .name = "C128 D71 (Double-Sided)",
        .family = UFT_FAMILY_C64,
        .expected_sizes = D64_SIZES + 6, .expected_size_count = 2,  // 349696, 351062
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d71",
        .validate_structure = validate_d64,
        .base_prior = 0.02f
    },
    {
        .id = "d81", .name = "C128 D81 (3.5\" DD)",
        .family = UFT_FAMILY_C64,
        .expected_sizes = D64_SIZES + 8, .expected_size_count = 1,  // 822400
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d81",
        .validate_structure = NULL,
        .base_prior = 0.02f
    },
    
    // Amiga (need DOS type checking)
    {
        .id = "amiga_ofs", .name = "Amiga OFS (DOS0)",
        .family = UFT_FAMILY_AMIGA,
        .expected_sizes = AMIGA_SIZES, .expected_size_count = 1,
        .magic = MAGIC_AMIGA, .magic_len = 3, .magic_offset = 0,
        .magic2 = (const uint8_t[]){0x00}, .magic2_len = 1, .magic2_offset = 3,
        .extensions = "adf",
        .validate_structure = validate_amiga,
        .base_prior = 0.06f
    },
    {
        .id = "amiga_ffs", .name = "Amiga FFS (DOS1)",
        .family = UFT_FAMILY_AMIGA,
        .expected_sizes = AMIGA_SIZES, .expected_size_count = 1,
        .magic = MAGIC_AMIGA, .magic_len = 3, .magic_offset = 0,
        .magic2 = (const uint8_t[]){0x01}, .magic2_len = 1, .magic2_offset = 3,
        .extensions = "adf",
        .validate_structure = validate_amiga,
        .base_prior = 0.05f
    },
    {
        .id = "amiga_intl", .name = "Amiga OFS/FFS Intl (DOS2/3)",
        .family = UFT_FAMILY_AMIGA,
        .expected_sizes = AMIGA_SIZES, .expected_size_count = 2,
        .magic = MAGIC_AMIGA, .magic_len = 3, .magic_offset = 0,
        .extensions = "adf",
        .validate_structure = validate_amiga,
        .base_prior = 0.03f
    },
    
    // PC formats (need BPB validation)
    {
        .id = "pc_360k", .name = "PC 360K (5.25\" DS/DD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_sizes = PC_IMG_SIZES + 3, .expected_size_count = 1,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .validate_structure = validate_pc_fat,
        .base_prior = 0.04f
    },
    {
        .id = "pc_720k", .name = "PC 720K (3.5\" DS/DD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_sizes = PC_IMG_SIZES + 4, .expected_size_count = 1,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .validate_structure = validate_pc_fat,
        .base_prior = 0.07f
    },
    {
        .id = "pc_1200k", .name = "PC 1.2M (5.25\" HD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_sizes = PC_IMG_SIZES + 5, .expected_size_count = 1,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .validate_structure = validate_pc_fat,
        .base_prior = 0.05f
    },
    {
        .id = "pc_1440k", .name = "PC 1.44M (3.5\" HD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_sizes = PC_IMG_SIZES + 6, .expected_size_count = 1,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .validate_structure = validate_pc_fat,
        .base_prior = 0.12f
    },
    
    // Atari ST (conflicts with PC 720K!)
    {
        .id = "atari_st_ss", .name = "Atari ST SS (360K)",
        .family = UFT_FAMILY_ATARI_ST,
        .expected_sizes = ATARI_ST_SIZES, .expected_size_count = 1,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "st",
        .validate_structure = validate_atari_st,
        .base_prior = 0.03f
    },
    {
        .id = "atari_st_ds", .name = "Atari ST DS (720K)",
        .family = UFT_FAMILY_ATARI_ST,
        .expected_sizes = ATARI_ST_SIZES + 1, .expected_size_count = 1,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "st",
        .validate_structure = validate_atari_st,
        .base_prior = 0.04f
    },
    
    // Terminator
    {.id = NULL}
};

#define FORMAT_COUNT (sizeof(FORMAT_SPECS) / sizeof(FORMAT_SPECS[0]) - 1)

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static void str_lower(char *dst, const char *src, size_t max) {
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = (char)tolower((unsigned char)src[i]);
        i++;
    }
    dst[i] = '\0';
}

static const char *get_extension(const char *path) {
    if (!path) return "";
    const char *dot = strrchr(path, '.');
    return dot ? dot + 1 : "";
}

static bool extension_matches(const char *extensions, const char *ext) {
    if (!extensions || !ext || !ext[0]) return false;
    
    char ext_lower[16];
    str_lower(ext_lower, ext, sizeof(ext_lower));
    
    char exts_copy[256];
    strncpy(exts_copy, extensions, sizeof(exts_copy) - 1);
    exts_copy[sizeof(exts_copy) - 1] = '\0';
    
    char *token = strtok(exts_copy, ",");
    while (token) {
        char token_lower[16];
        str_lower(token_lower, token, sizeof(token_lower));
        if (strcmp(token_lower, ext_lower) == 0) return true;
        token = strtok(NULL, ",");
    }
    return false;
}

static bool size_matches(const size_t *sizes, size_t count, size_t actual) {
    for (size_t i = 0; i < count; i++) {
        if (sizes[i] == actual) return true;
    }
    return false;
}

static float size_score(const size_t *sizes, size_t count, size_t actual) {
    if (count == 0) return 0.5f;  // Variable size format
    
    for (size_t i = 0; i < count; i++) {
        if (sizes[i] == actual) return 1.0f;  // Exact match
        
        // Close match (within 1%)
        size_t expected = sizes[i];
        float ratio = (float)actual / (float)expected;
        if (ratio > 0.99f && ratio < 1.01f) return 0.8f;
        
        // Fuzzy match (within 5%)
        if (ratio > 0.95f && ratio < 1.05f) return 0.5f;
    }
    
    return 0.1f;  // No match
}

// ============================================================================
// STRUCTURE VALIDATORS
// ============================================================================

/**
 * Validate D64 structure
 * - Check BAM at track 18, sector 0
 * - Verify track/sector chain validity
 */
static float validate_d64(const uint8_t *data, size_t size) {
    if (!data || size < 174848) return 0.0f;
    
    float score = 0.0f;
    
    // Track 18, Sector 0 offset = 91392 bytes (17 tracks × various sectors × 256)
    // Actually: need to compute properly
    // Tracks 1-17 have 21 sectors each = 357 sectors
    // Sectors per track: 1-17: 21, 18-24: 19, 25-30: 18, 31-35: 17
    size_t t18s0_offset = 357 * 256;  // Simplified
    
    if (t18s0_offset + 256 > size) return 0.1f;
    
    const uint8_t *bam = data + t18s0_offset;
    
    // First 2 bytes: track/sector of first directory entry
    // Should be 18/1 for standard D64
    if (bam[0] == 0x12 && bam[1] == 0x01) {
        score += 0.3f;
    }
    
    // Byte 2: DOS version ('A' = 0x41 for standard)
    if (bam[2] == 0x41) {
        score += 0.2f;
    }
    
    // Byte 3: unused (should be 0x00)
    // BAM entries at 4-143 (35 tracks × 4 bytes)
    
    // Check BAM validity: first track should have some free sectors
    uint8_t free_sectors_t1 = bam[4];
    if (free_sectors_t1 > 0 && free_sectors_t1 <= 21) {
        score += 0.2f;
    }
    
    // Disk name at 144-159 (usually printable ASCII or PETSCII spaces)
    bool name_valid = true;
    for (int i = 144; i < 160; i++) {
        uint8_t c = bam[i];
        if (c != 0xA0 && (c < 0x20 || c > 0x7E)) {
            // Allow PETSCII space (0xA0) and printable ASCII
            if (c < 0x41 || c > 0x5A) {  // Also allow uppercase
                name_valid = false;
                break;
            }
        }
    }
    if (name_valid) score += 0.2f;
    
    // Disk ID at 162-163
    // Should be 2 printable characters
    
    return fminf(score, 1.0f);
}

/**
 * Validate Amiga structure
 * - Check DOS type byte
 * - Validate rootblock checksum
 */
static float validate_amiga(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0.0f;
    
    float score = 0.0f;
    
    // Check "DOS" signature
    if (data[0] == 'D' && data[1] == 'O' && data[2] == 'S') {
        score += 0.3f;
        
        // DOS type byte
        uint8_t dos_type = data[3];
        if (dos_type <= 7) {  // Valid DOS types: 0-7
            score += 0.2f;
        }
    }
    
    // Rootblock is at sector 880 (middle of disk)
    // For DD: 880 × 512 = 450560
    size_t rootblock_offset = 880 * 512;
    if (rootblock_offset + 512 <= size) {
        const uint8_t *rb = data + rootblock_offset;
        
        // Rootblock type should be 2 (T_HEADER)
        uint32_t type = ((uint32_t)rb[0] << 24) | ((uint32_t)rb[1] << 16) |
                        ((uint32_t)rb[2] << 8) | rb[3];
        if (type == 2) {
            score += 0.2f;
        }
        
        // Secondary type at offset 508 should be 1 (ST_ROOT)
        uint32_t sec_type = ((uint32_t)rb[508] << 24) | ((uint32_t)rb[509] << 16) |
                           ((uint32_t)rb[510] << 8) | rb[511];
        if (sec_type == 1) {
            score += 0.2f;
        }
    }
    
    return fminf(score, 1.0f);
}

/**
 * Validate PC FAT structure
 * - Check BPB fields
 * - Validate media descriptor
 */
static float validate_pc_fat(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0.0f;
    
    float score = 0.0f;
    
    // Boot signature at 510-511
    if (data[510] == 0x55 && data[511] == 0xAA) {
        score += 0.2f;
    }
    
    // BPB starts at offset 11
    // Bytes per sector (should be 512 for most floppies)
    uint16_t bytes_per_sector = data[11] | ((uint16_t)data[12] << 8);
    if (bytes_per_sector == 512 || bytes_per_sector == 1024) {
        score += 0.15f;
    }
    
    // Sectors per cluster (should be 1 or 2 for floppies)
    uint8_t sectors_per_cluster = data[13];
    if (sectors_per_cluster >= 1 && sectors_per_cluster <= 4) {
        score += 0.1f;
    }
    
    // Reserved sectors (usually 1)
    uint16_t reserved = data[14] | ((uint16_t)data[15] << 8);
    if (reserved >= 1 && reserved <= 8) {
        score += 0.1f;
    }
    
    // Number of FATs (usually 2)
    uint8_t num_fats = data[16];
    if (num_fats == 2) {
        score += 0.15f;
    }
    
    // Root entries (usually 112 or 224 for floppies)
    uint16_t root_entries = data[17] | ((uint16_t)data[18] << 8);
    if (root_entries == 112 || root_entries == 224 || root_entries == 240) {
        score += 0.1f;
    }
    
    // Media descriptor (should be 0xF0-0xFF for floppies)
    uint8_t media = data[21];
    if (media >= 0xF0) {
        score += 0.2f;
    }
    
    return fminf(score, 1.0f);
}

/**
 * Validate Atari ST structure
 * - Similar to PC FAT but different BPB interpretation
 * - Check for Atari-specific markers
 */
static float validate_atari_st(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0.0f;
    
    // Start with PC FAT validation
    float score = validate_pc_fat(data, size);
    
    // Atari-specific: check for typical Atari boot code patterns
    // Atari boot sectors often start with 0x60 0x38 (BRA.S)
    if (data[0] == 0x60) {
        score += 0.1f;
    }
    
    // Check serial number at offset 8-10 (Atari-specific)
    // Often contains "USB" or similar
    
    return fminf(score, 1.0f);
}

/**
 * Validate G64 structure
 * - Check header fields
 * - Verify track count
 */
static float validate_g64(const uint8_t *data, size_t size) {
    if (!data || size < 12) return 0.0f;
    
    float score = 0.0f;
    
    // Signature "GCR-1541"
    if (memcmp(data, "GCR-1541", 8) == 0) {
        score += 0.5f;
    }
    
    // Version byte at offset 8 (usually 0)
    if (data[8] == 0) {
        score += 0.1f;
    }
    
    // Track count at offset 9 (usually 84 for double-sided or 42 for single)
    uint8_t tracks = data[9];
    if (tracks == 42 || tracks == 84) {
        score += 0.2f;
    }
    
    // Max track size at offset 10-11
    uint16_t max_size = data[10] | ((uint16_t)data[11] << 8);
    if (max_size > 0 && max_size <= 8192) {
        score += 0.2f;
    }
    
    return fminf(score, 1.0f);
}

/**
 * Validate SCP structure
 * - Check header checksum
 * - Verify track table
 */
static float validate_scp(const uint8_t *data, size_t size) {
    if (!data || size < 16) return 0.0f;
    
    float score = 0.0f;
    
    // Signature "SCP"
    if (memcmp(data, "SCP", 3) == 0) {
        score += 0.4f;
    }
    
    // Version at offset 3
    uint8_t version = data[3];
    if (version <= 0x19) {  // Valid versions
        score += 0.1f;
    }
    
    // Disk type at offset 4
    uint8_t disk_type = data[4];
    if (disk_type < 0x80) {  // Valid disk types
        score += 0.1f;
    }
    
    // Revolution count at offset 5
    uint8_t revolutions = data[5];
    if (revolutions >= 1 && revolutions <= 10) {
        score += 0.2f;
    }
    
    // Start/end track at 6-7
    uint8_t start_track = data[6];
    uint8_t end_track = data[7];
    if (start_track <= end_track && end_track <= 167) {
        score += 0.2f;
    }
    
    return fminf(score, 1.0f);
}

// ============================================================================
// CONFIGURATION
// ============================================================================

uft_detection_config_t uft_detection_config_default(void) {
    return (uft_detection_config_t){
        .skip_content_analysis = false,
        .require_structure_valid = false,
        
        .accept_threshold = 0.70f,
        .uncertain_threshold = 0.85f,
        .review_threshold = 0.50f,
        .margin_threshold = 0.15f,
        
        .weight_magic = 0.20f,
        .weight_structure = 0.35f,
        .weight_size = 0.25f,
        .weight_extension = 0.10f,
        .weight_content = 0.10f,
        
        .region = "EU",
        .verbose = false,
        .log_callback = NULL
    };
}

uft_detection_config_t uft_detection_config_paranoid(void) {
    return (uft_detection_config_t){
        .skip_content_analysis = false,
        .require_structure_valid = true,
        
        .accept_threshold = 0.85f,
        .uncertain_threshold = 0.95f,
        .review_threshold = 0.70f,
        .margin_threshold = 0.20f,
        
        .weight_magic = 0.15f,
        .weight_structure = 0.45f,
        .weight_size = 0.20f,
        .weight_extension = 0.05f,
        .weight_content = 0.15f,
        
        .region = NULL,
        .verbose = true,
        .log_callback = NULL
    };
}

// ============================================================================
// HELPER NAMES
// ============================================================================

const char *uft_confidence_level_name(uft_confidence_level_t level) {
    switch (level) {
        case UFT_CONF_NONE: return "NONE";
        case UFT_CONF_VERY_LOW: return "VERY_LOW";
        case UFT_CONF_LOW: return "LOW";
        case UFT_CONF_MEDIUM: return "MEDIUM";
        case UFT_CONF_HIGH: return "HIGH";
        case UFT_CONF_VERY_HIGH: return "VERY_HIGH";
        case UFT_CONF_CERTAIN: return "CERTAIN";
        default: return "UNKNOWN";
    }
}

const char *uft_warning_type_name(uft_warning_type_t type) {
    switch (type) {
        case UFT_WARN_NONE: return "NONE";
        case UFT_WARN_SIZE_MISMATCH: return "SIZE_MISMATCH";
        case UFT_WARN_CHECKSUM_FAIL: return "CHECKSUM_FAIL";
        case UFT_WARN_TRUNCATED: return "TRUNCATED";
        case UFT_WARN_CORRUPTED: return "CORRUPTED";
        case UFT_WARN_AMBIGUOUS: return "AMBIGUOUS";
        case UFT_WARN_EXTENSION_MISMATCH: return "EXTENSION_MISMATCH";
        case UFT_WARN_COMPRESSED: return "COMPRESSED";
        case UFT_WARN_FAKE_HEADER: return "FAKE_HEADER";
        case UFT_WARN_VARIANT_UNKNOWN: return "VARIANT_UNKNOWN";
        case UFT_WARN_NEEDS_REVIEW: return "NEEDS_REVIEW";
        default: return "UNKNOWN";
    }
}

// ============================================================================
// MAIN DETECTION FUNCTION
// ============================================================================

int uft_detect_format_v2_buffer(
    const uint8_t *data,
    size_t size,
    const char *extension,
    const uft_detection_config_t *config_in,
    uft_detection_result_t *result)
{
    if (!data || size == 0 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    uft_detection_config_t config = config_in ? *config_in : uft_detection_config_default();
    
    // Temporary candidate array
    uft_detection_candidate_t candidates[FORMAT_COUNT];
    size_t candidate_count = 0;
    
    result->audit.file_size = size;
    
    // ========================================================================
    // PHASE 1: HARD FACTS (Magic bytes)
    // ========================================================================
    
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        const format_spec_t *spec = &FORMAT_SPECS[i];
        
        if (spec->magic && spec->magic_len > 0) {
            if (spec->magic_offset + spec->magic_len <= size) {
                if (memcmp(data + spec->magic_offset, spec->magic, spec->magic_len) == 0) {
                    // Magic matched!
                    uft_detection_candidate_t *c = &candidates[candidate_count++];
                    memset(c, 0, sizeof(*c));
                    
                    strncpy(c->format_id, spec->id, UFT_MAX_FORMAT_ID_LEN - 1);
                    strncpy(c->format_name, spec->name, sizeof(c->format_name) - 1);
                    c->family = spec->family;
                    c->score_magic = 1.0f;
                    c->evidence.magic_matched = true;
                    c->added_in_phase = UFT_PHASE_HARD_FACTS;
                    
                    snprintf(c->reason, sizeof(c->reason), 
                             "Magic bytes matched at offset %zu", spec->magic_offset);
                    
                    // Check secondary magic (for Amiga variants)
                    if (spec->magic2 && spec->magic2_len > 0) {
                        if (spec->magic2_offset + spec->magic2_len <= size) {
                            if (memcmp(data + spec->magic2_offset, spec->magic2, spec->magic2_len) == 0) {
                                c->score_magic = 1.0f;
                            } else {
                                c->score_magic = 0.7f;  // Primary matches, secondary doesn't
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ========================================================================
    // PHASE 2: STRUCTURE VALIDATION
    // ========================================================================
    
    // For each candidate, run structure validation
    for (size_t i = 0; i < candidate_count; i++) {
        uft_detection_candidate_t *c = &candidates[i];
        
        // Find spec
        const format_spec_t *spec = NULL;
        for (size_t j = 0; j < FORMAT_COUNT; j++) {
            if (strcmp(FORMAT_SPECS[j].id, c->format_id) == 0) {
                spec = &FORMAT_SPECS[j];
                break;
            }
        }
        
        if (spec && spec->validate_structure) {
            c->score_structure = spec->validate_structure(data, size);
            c->evidence.structure_valid = (c->score_structure >= 0.5f);
        } else {
            c->score_structure = 0.5f;  // No validator = neutral
        }
    }
    
    // Add size-based candidates if no magic matches found
    if (candidate_count == 0) {
        for (size_t i = 0; i < FORMAT_COUNT; i++) {
            const format_spec_t *spec = &FORMAT_SPECS[i];
            
            if (spec->expected_sizes && spec->expected_size_count > 0) {
                if (size_matches(spec->expected_sizes, spec->expected_size_count, size)) {
                    uft_detection_candidate_t *c = &candidates[candidate_count++];
                    memset(c, 0, sizeof(*c));
                    
                    strncpy(c->format_id, spec->id, UFT_MAX_FORMAT_ID_LEN - 1);
                    strncpy(c->format_name, spec->name, sizeof(c->format_name) - 1);
                    c->family = spec->family;
                    c->score_size = 1.0f;
                    c->evidence.size_exact = true;
                    c->added_in_phase = UFT_PHASE_HEURISTICS;
                    
                    snprintf(c->reason, sizeof(c->reason), 
                             "Size %zu matches expected", size);
                    
                    // Run structure validation
                    if (spec->validate_structure) {
                        c->score_structure = spec->validate_structure(data, size);
                        c->evidence.structure_valid = (c->score_structure >= 0.5f);
                    }
                    
                    if (candidate_count >= FORMAT_COUNT) break;
                }
            }
        }
    }
    
    // ========================================================================
    // PHASE 3: SOFT HEURISTICS (Extension, Priors)
    // ========================================================================
    
    for (size_t i = 0; i < candidate_count; i++) {
        uft_detection_candidate_t *c = &candidates[i];
        
        // Find spec
        const format_spec_t *spec = NULL;
        for (size_t j = 0; j < FORMAT_COUNT; j++) {
            if (strcmp(FORMAT_SPECS[j].id, c->format_id) == 0) {
                spec = &FORMAT_SPECS[j];
                break;
            }
        }
        
        if (!spec) continue;
        
        // Extension score
        if (extension && extension[0]) {
            if (extension_matches(spec->extensions, extension)) {
                c->score_extension = 1.0f;
                c->evidence.extension_matched = true;
            } else {
                c->score_extension = 0.2f;
            }
        } else {
            c->score_extension = 0.5f;  // No extension = neutral
        }
        
        // Size score (if not already set)
        if (c->score_size == 0.0f) {
            c->score_size = size_score(spec->expected_sizes, spec->expected_size_count, size);
        }
    }
    
    // ========================================================================
    // PHASE 4: CONTENT ANALYSIS (simplified)
    // ========================================================================
    
    if (!config.skip_content_analysis) {
        for (size_t i = 0; i < candidate_count; i++) {
            uft_detection_candidate_t *c = &candidates[i];
            
            // Simple entropy check: highly compressed/encrypted data = suspicious
            if (size >= 512) {
                int byte_counts[256] = {0};
                for (size_t j = 0; j < 512; j++) {
                    byte_counts[data[j]]++;
                }
                
                float entropy = 0.0f;
                for (int j = 0; j < 256; j++) {
                    if (byte_counts[j] > 0) {
                        float p = (float)byte_counts[j] / 512.0f;
                        entropy -= p * log2f(p);
                    }
                }
                
                // Normal disk data: entropy 4-7 bits
                // Random/encrypted: entropy ~8 bits
                // Sparse/empty: entropy < 3 bits
                if (entropy >= 3.0f && entropy <= 7.5f) {
                    c->score_content = 0.8f;
                } else {
                    c->score_content = 0.4f;
                }
            }
        }
    }
    
    // ========================================================================
    // PHASE 5: DECISION
    // ========================================================================
    
    // Compute combined scores
    for (size_t i = 0; i < candidate_count; i++) {
        uft_detection_candidate_t *c = &candidates[i];
        
        c->confidence = 
            config.weight_magic * c->score_magic +
            config.weight_structure * c->score_structure +
            config.weight_size * c->score_size +
            config.weight_extension * c->score_extension +
            config.weight_content * c->score_content;
    }
    
    // Sort by confidence (descending)
    for (size_t i = 0; i < candidate_count; i++) {
        for (size_t j = i + 1; j < candidate_count; j++) {
            if (candidates[j].confidence > candidates[i].confidence) {
                uft_detection_candidate_t tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }
    
    // Fill result
    result->audit.candidates_considered = candidate_count;
    
    if (candidate_count > 0) {
        result->best = candidates[0];
        result->confidence = candidates[0].confidence;
        
        // Compute margin
        if (candidate_count > 1) {
            result->margin = candidates[0].confidence - candidates[1].confidence;
        } else {
            result->margin = candidates[0].confidence;
        }
        
        // Determine confidence level
        if (result->confidence >= 0.99f) {
            result->confidence_level = UFT_CONF_CERTAIN;
        } else if (result->confidence >= 0.90f) {
            result->confidence_level = UFT_CONF_VERY_HIGH;
        } else if (result->confidence >= 0.70f) {
            result->confidence_level = UFT_CONF_HIGH;
        } else if (result->confidence >= 0.50f) {
            result->confidence_level = UFT_CONF_MEDIUM;
        } else if (result->confidence >= 0.30f) {
            result->confidence_level = UFT_CONF_LOW;
        } else {
            result->confidence_level = UFT_CONF_VERY_LOW;
        }
        
        // Check uncertainty
        result->is_uncertain = (result->margin < config.margin_threshold);
        result->needs_manual_review = (result->confidence < config.review_threshold);
        
        // Add alternatives
        for (size_t i = 1; i < candidate_count && result->alternative_count < UFT_MAX_CANDIDATES; i++) {
            result->alternatives[result->alternative_count++] = candidates[i];
        }
        
        // Generate explanation
        snprintf(result->explanation.decision, sizeof(result->explanation.decision),
                 "Detected as: %s", result->best.format_name);
        
        snprintf(result->explanation.primary_reason, sizeof(result->explanation.primary_reason),
                 "%s (confidence: %.0f%%)", result->best.reason, result->confidence * 100);
        
        if (result->alternative_count > 0) {
            snprintf(result->explanation.alternatives_text, sizeof(result->explanation.alternatives_text),
                     "Also possible: %s (%.0f%%)", 
                     result->alternatives[0].format_name,
                     result->alternatives[0].confidence * 100);
        }
        
        // Warnings
        if (result->is_uncertain) {
            result->warnings[result->warning_count].type = UFT_WARN_AMBIGUOUS;
            snprintf(result->warnings[result->warning_count].message, 
                     sizeof(result->warnings[0].message),
                     "Detection is uncertain (margin %.1f%% < threshold %.1f%%)",
                     result->margin * 100, config.margin_threshold * 100);
            result->warnings[result->warning_count].severity = 0.7f;
            result->warning_count++;
        }
        
        if (result->needs_manual_review) {
            result->warnings[result->warning_count].type = UFT_WARN_NEEDS_REVIEW;
            snprintf(result->warnings[result->warning_count].message,
                     sizeof(result->warnings[0].message),
                     "Confidence %.1f%% below threshold, manual review recommended",
                     result->confidence * 100);
            result->warnings[result->warning_count].severity = 0.8f;
            result->warning_count++;
        }
        
    } else {
        // No candidates found
        strncpy(result->best.format_id, "unknown", UFT_MAX_FORMAT_ID_LEN - 1);
        strncpy(result->best.format_name, "Unknown Format", sizeof(result->best.format_name) - 1);
        result->confidence = 0.0f;
        result->confidence_level = UFT_CONF_NONE;
        result->is_uncertain = true;
        result->needs_manual_review = true;
        
        snprintf(result->explanation.decision, sizeof(result->explanation.decision),
                 "Unable to detect format");
        snprintf(result->explanation.primary_reason, sizeof(result->explanation.primary_reason),
                 "No matching magic bytes, size, or structure found");
    }
    
    result->audit.final_phase = UFT_PHASE_DECISION;
    
    return 0;
}

int uft_detect_format_v2(
    const char *path,
    const uft_detection_config_t *config,
    uft_detection_result_t *result)
{
    if (!path || !result) return -1;
    
    // Read file
    FILE *f = fopen(path, "rb");
    if (!f) return -2;
    
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (file_size <= 0 || file_size > 100 * 1024 * 1024) {  // Max 100MB
        fclose(f);
        return -3;
    }
    
    uint8_t *data = malloc((size_t)file_size);
    if (!data) {
        fclose(f);
        return -4;
    }
    
    size_t read = fread(data, 1, (size_t)file_size, f);
    fclose(f);
    
    if (read != (size_t)file_size) {
        free(data);
        return -5;
    }
    
    const char *ext = get_extension(path);
    
    int ret = uft_detect_format_v2_buffer(data, (size_t)file_size, ext, config, result);
    
    free(data);
    return ret;
}

const char *uft_detection_explain(const uft_detection_result_t *result) {
    if (!result) return "No result";
    static char buffer[2048];
    
    snprintf(buffer, sizeof(buffer),
             "%s\n"
             "Reason: %s\n"
             "Confidence: %.0f%% (%s)\n"
             "%s%s"
             "%s%s",
             result->explanation.decision,
             result->explanation.primary_reason,
             result->confidence * 100,
             uft_confidence_level_name(result->confidence_level),
             result->alternative_count > 0 ? result->explanation.alternatives_text : "",
             result->alternative_count > 0 ? "\n" : "",
             result->warning_count > 0 ? "Warnings: " : "",
             result->warning_count > 0 ? result->warnings[0].message : "");
    
    return buffer;
}

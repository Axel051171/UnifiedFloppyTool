/**
 * @file uft_format_detect.c
 * @brief 7-Phase Format Detection with Confidence Scoring
 * 
 * DETECTION PHASES:
 * 1. Magic Bytes
 * 2. Header Structure
 * 3. Size Analysis
 * 4. Content Analysis
 * 5. Extension Hint
 * 6. Disambiguation
 * 7. Variant Detection
 */

#include "uft/params/uft_canonical_params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// MAGIC BYTES DATABASE
// ============================================================================

typedef struct {
    const uint8_t *magic;
    size_t magic_len;
    size_t offset;
    uft_format_e format;
    float confidence;
    const char *description;
} magic_entry_t;

static const uint8_t MAGIC_SCP[] = {'S', 'C', 'P'};
static const uint8_t MAGIC_WOZ1[] = {'W', 'O', 'Z', '1'};
static const uint8_t MAGIC_WOZ2[] = {'W', 'O', 'Z', '2'};
static const uint8_t MAGIC_IPF[] = {'C', 'A', 'P', 'S'};
static const uint8_t MAGIC_IMD[] = {'I', 'M', 'D', ' '};
static const uint8_t MAGIC_EDSK[] = {'E', 'X', 'T', 'E', 'N', 'D', 'E', 'D'};
static const uint8_t MAGIC_DSK[] = {'M', 'V', ' ', '-'};
static const uint8_t MAGIC_D88[] = {0x00};  // Special handling
static const uint8_t MAGIC_HFE[] = {'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E'};
static const uint8_t MAGIC_G64[] = {'G', 'C', 'R', '-', '1', '5', '4', '1'};
static const uint8_t MAGIC_P64[] = {'P', '6', '4', '-', '1', '5', '4', '1'};
static const uint8_t MAGIC_UFF[] = {'U', 'F', 'F', '1'};
static const uint8_t MAGIC_2IMG[] = {'2', 'I', 'M', 'G'};
static const uint8_t MAGIC_ATR[] = {0x96, 0x02};
static const uint8_t MAGIC_ATX[] = {'A', 'T', '8', 'X'};
static const uint8_t MAGIC_A2R[] = {'A', '2', 'R', '2'};
static const uint8_t MAGIC_STX[] = {'R', 'S', 'Y'};
static const uint8_t MAGIC_MSA[] = {0x0E, 0x0F};
static const uint8_t MAGIC_TD0[] = {'T', 'D'};
static const uint8_t MAGIC_TD0A[] = {'t', 'd'};
static const uint8_t MAGIC_DC42[] = {0x00, 0x00, 0x01, 0x00};
static const uint8_t MAGIC_FDI[] = {'F', 'o', 'r', 'm', 'a', 't', 't', 'e', 'd'};

static const magic_entry_t MAGIC_TABLE[] = {
    {MAGIC_SCP,  3, 0, UFT_FORMAT_SCP, 0.95f, "SuperCard Pro"},
    {MAGIC_WOZ1, 4, 0, UFT_FORMAT_WOZ, 0.95f, "WOZ 1.0"},
    {MAGIC_WOZ2, 4, 0, UFT_FORMAT_WOZ, 0.95f, "WOZ 2.0"},
    {MAGIC_IPF,  4, 0, UFT_FORMAT_IPF, 0.95f, "CAPS/IPF"},
    {MAGIC_IMD,  4, 0, UFT_FORMAT_IMD, 0.95f, "ImageDisk"},
    {MAGIC_EDSK, 8, 0, UFT_FORMAT_EDSK, 0.95f, "Extended DSK"},
    {MAGIC_DSK,  4, 0, UFT_FORMAT_DSK_CPC, 0.90f, "Amstrad DSK"},
    {MAGIC_HFE,  8, 0, UFT_FORMAT_HFE, 0.95f, "HxC HFE"},
    {MAGIC_G64,  8, 0, UFT_FORMAT_G64, 0.95f, "G64 GCR"},
    {MAGIC_P64,  8, 0, UFT_FORMAT_G64, 0.95f, "P64 GCR"},  // P64 is G64 variant
    {MAGIC_UFF,  4, 0, UFT_FORMAT_RAW, 0.95f, "UFF Universal"},  // Will need new enum
    {MAGIC_2IMG, 4, 0, UFT_FORMAT_2MG, 0.95f, "Apple 2IMG"},
    {MAGIC_ATR,  2, 0, UFT_FORMAT_ATR, 0.85f, "Atari ATR"},
    {MAGIC_ATX,  4, 0, UFT_FORMAT_ATR, 0.95f, "Atari ATX"},  // ATX variant
    {MAGIC_A2R,  4, 0, UFT_FORMAT_WOZ, 0.95f, "Applesauce A2R"},
    {MAGIC_STX,  3, 0, UFT_FORMAT_STX, 0.90f, "Atari STX"},
    {MAGIC_MSA,  2, 0, UFT_FORMAT_MSA, 0.85f, "Atari MSA"},
    {MAGIC_TD0,  2, 0, UFT_FORMAT_IMD, 0.85f, "Teledisk"},
    {MAGIC_TD0A, 2, 0, UFT_FORMAT_IMD, 0.85f, "Teledisk Advanced"},
    {MAGIC_FDI,  9, 0, UFT_FORMAT_FDI, 0.90f, "FDI"},
    {NULL, 0, 0, UFT_FORMAT_AUTO, 0.0f, NULL}  // Sentinel
};

// ============================================================================
// SIZE DATABASE
// ============================================================================

typedef struct {
    uint32_t size;
    uft_format_e formats[5];
    int format_count;
    uint16_t variants[5];
    const char *description;
} size_entry_t;

// Variant codes
#define VAR_D64_35      0x0001
#define VAR_D64_35_ERR  0x0002
#define VAR_D64_40      0x0003
#define VAR_D64_40_ERR  0x0004
#define VAR_D64_42      0x0005
#define VAR_D64_42_ERR  0x0006
#define VAR_ADF_DD      0x0010
#define VAR_ADF_HD      0x0011
#define VAR_SSD_40      0x0020
#define VAR_SSD_80      0x0021
#define VAR_DSD_40      0x0022
#define VAR_DSD_80      0x0023

static const size_entry_t SIZE_TABLE[] = {
    // Commodore
    {174848,   {UFT_FORMAT_D64}, 1, {VAR_D64_35}, "D64 35-track"},
    {175531,   {UFT_FORMAT_D64}, 1, {VAR_D64_35_ERR}, "D64 35-track + errors"},
    {196608,   {UFT_FORMAT_D64}, 1, {VAR_D64_40}, "D64 40-track"},
    {197376,   {UFT_FORMAT_D64}, 1, {VAR_D64_40_ERR}, "D64 40-track + errors"},
    {205312,   {UFT_FORMAT_D64}, 1, {VAR_D64_42}, "D64 42-track"},
    {206114,   {UFT_FORMAT_D64}, 1, {VAR_D64_42_ERR}, "D64 42-track + errors"},
    {349696,   {UFT_FORMAT_D71}, 1, {0}, "D71"},
    {819200,   {UFT_FORMAT_D81}, 1, {0}, "D81"},
    
    // Amiga
    {901120,   {UFT_FORMAT_ADF}, 1, {VAR_ADF_DD}, "ADF DD (880K)"},
    {1802240,  {UFT_FORMAT_ADF}, 1, {VAR_ADF_HD}, "ADF HD (1.76M)"},
    
    // PC - Note: multiple formats per size!
    {163840,   {UFT_FORMAT_IMG}, 1, {0}, "PC 160K"},
    {184320,   {UFT_FORMAT_IMG}, 1, {0}, "PC 180K"},
    {327680,   {UFT_FORMAT_IMG, UFT_FORMAT_D88}, 2, {0, 0}, "PC 320K / PC-88"},
    {368640,   {UFT_FORMAT_IMG, UFT_FORMAT_ST}, 2, {0, 0}, "PC 360K / Atari ST SS"},
    {737280,   {UFT_FORMAT_IMG, UFT_FORMAT_ST, UFT_FORMAT_MSX_DSK}, 3, {0, 0, 0}, "PC/ST/MSX 720K"},
    {1228800,  {UFT_FORMAT_IMG}, 1, {0}, "PC 1.2M"},
    {1474560,  {UFT_FORMAT_IMG}, 1, {0}, "PC 1.44M"},
    {2949120,  {UFT_FORMAT_IMG}, 1, {0}, "PC 2.88M"},
    {1720320,  {UFT_FORMAT_IMG}, 1, {0}, "PC DMF 1.68M"},
    
    // BBC / Acorn - AMBIGUOUS!
    {102400,   {UFT_FORMAT_SSD}, 1, {VAR_SSD_40}, "BBC SSD 40T"},
    {204800,   {UFT_FORMAT_SSD, UFT_FORMAT_DSD}, 2, {VAR_SSD_80, VAR_DSD_40}, "BBC SSD 80T / DSD 40T"},
    {409600,   {UFT_FORMAT_DSD}, 1, {VAR_DSD_80}, "BBC DSD 80T"},
    
    // Apple
    {116480,   {UFT_FORMAT_DO}, 1, {0}, "Apple II 13-sector"},
    {143360,   {UFT_FORMAT_DO, UFT_FORMAT_PO}, 2, {0, 0}, "Apple II DOS/ProDOS"},
    {232960,   {UFT_FORMAT_NIB}, 1, {0}, "Apple II NIB"},
    
    // Atari 8-bit
    {92176,    {UFT_FORMAT_ATR}, 1, {0}, "Atari ATR 90K"},
    {133136,   {UFT_FORMAT_ATR}, 1, {0}, "Atari ATR 130K"},
    {183952,   {UFT_FORMAT_ATR}, 1, {0}, "Atari ATR 180K"},
    
    // Spectrum
    {655360,   {UFT_FORMAT_TRD}, 1, {0}, "TR-DOS"},
    
    // PC-98
    {1261568,  {UFT_FORMAT_D88}, 1, {0}, "PC-98 2HD"},
    
    {0, {UFT_FORMAT_AUTO}, 0, {0}, NULL}  // Sentinel
};

// ============================================================================
// DETECTION RESULT STRUCTURE
// ============================================================================

typedef struct {
    uft_format_e format;
    uint16_t variant;
    float confidence;
    uint32_t flags;
    char reason[128];
} detection_candidate_t;

typedef struct {
    detection_candidate_t candidates[10];
    int candidate_count;
    
    detection_candidate_t *best;
    float confidence_gap;
    
    bool magic_found;
    bool header_valid;
    bool size_match;
    bool content_match;
    bool extension_match;
    
    bool has_error_map;
    bool is_extended;
    bool is_compressed;
    const char *tool_origin;
    
    char warnings[512];
    int warning_count;
} detection_result_t;

// ============================================================================
// PHASE 1: MAGIC BYTE DETECTION
// ============================================================================

static int detect_magic(const uint8_t *data, size_t size, detection_result_t *result) {
    for (const magic_entry_t *m = MAGIC_TABLE; m->magic != NULL; m++) {
        if (size < m->offset + m->magic_len) continue;
        
        if (memcmp(data + m->offset, m->magic, m->magic_len) == 0) {
            result->magic_found = true;
            
            detection_candidate_t *c = &result->candidates[result->candidate_count++];
            c->format = m->format;
            c->confidence = m->confidence;
            snprintf(c->reason, sizeof(c->reason), "Magic: %s", m->description);
            
            return 1;
        }
    }
    return 0;
}

// ============================================================================
// PHASE 3: SIZE ANALYSIS
// ============================================================================

static int detect_by_size(size_t size, detection_result_t *result) {
    for (const size_entry_t *s = SIZE_TABLE; s->size != 0; s++) {
        if ((size_t)s->size == size) {
            result->size_match = true;
            
            for (int i = 0; i < s->format_count && result->candidate_count < 10; i++) {
                // Check if already added by magic
                bool already_added = false;
                for (int j = 0; j < result->candidate_count; j++) {
                    if (result->candidates[j].format == s->formats[i]) {
                        // Boost confidence
                        result->candidates[j].confidence += 0.15f;
                        if (result->candidates[j].confidence > 1.0f)
                            result->candidates[j].confidence = 1.0f;
                        already_added = true;
                        break;
                    }
                }
                
                if (!already_added) {
                    detection_candidate_t *c = &result->candidates[result->candidate_count++];
                    c->format = s->formats[i];
                    c->variant = s->variants[i];
                    c->confidence = (s->format_count == 1) ? 0.70f : 0.40f;  // Lower if ambiguous
                    snprintf(c->reason, sizeof(c->reason), "Size: %s", s->description);
                }
            }
            
            return s->format_count;
        }
    }
    return 0;
}

// ============================================================================
// PHASE 4: CONTENT ANALYSIS
// ============================================================================

// Check for C64 BAM structure
static bool detect_c64_bam(const uint8_t *data, size_t size) {
    if (size < 256 * 19) return false;  // Track 18 offset
    
    const uint8_t *bam = data + (17 * 21 * 256);  // Track 18, sector 0
    
    // BAM signature checks
    if (bam[0] != 18 || bam[1] != 1) return false;  // Directory track/sector
    if (bam[2] != 'A') return false;  // DOS type
    
    return true;
}

// Check for Amiga bootblock
static bool detect_amiga_bootblock(const uint8_t *data, size_t size) {
    if (size < 4) return false;
    
    // DOS\0 or DOS\1 magic
    if (data[0] == 'D' && data[1] == 'O' && data[2] == 'S' &&
        (data[3] == 0 || data[3] == 1)) {
        return true;
    }
    
    return false;
}

// Check for FAT bootblock
static bool detect_fat_bootblock(const uint8_t *data, size_t size) {
    if (size < 512) return false;
    
    // Jump instruction
    if (data[0] != 0xEB && data[0] != 0xE9) return false;
    
    // Boot signature
    if (data[510] != 0x55 || data[511] != 0xAA) return false;
    
    return true;
}

// Check for Apple DOS 3.3
static bool detect_apple_dos33(const uint8_t *data, size_t size) {
    if (size < 256 * 17) return false;
    
    // VTOC at track 17, sector 0
    const uint8_t *vtoc = data + (17 * 16 * 256);
    
    // Check for valid DOS version
    if (vtoc[3] != 3) return false;  // DOS 3.x
    
    return true;
}

static int detect_by_content(const uint8_t *data, size_t size, detection_result_t *result) {
    int matches = 0;
    
    // Check various content signatures
    if (detect_c64_bam(data, size)) {
        for (int i = 0; i < result->candidate_count; i++) {
            if (result->candidates[i].format == UFT_FORMAT_D64) {
                result->candidates[i].confidence += 0.20f;
                result->content_match = true;
                matches++;
            }
        }
    }
    
    if (detect_amiga_bootblock(data, size)) {
        for (int i = 0; i < result->candidate_count; i++) {
            if (result->candidates[i].format == UFT_FORMAT_ADF) {
                result->candidates[i].confidence += 0.20f;
                result->content_match = true;
                matches++;
            }
        }
    }
    
    if (detect_fat_bootblock(data, size)) {
        for (int i = 0; i < result->candidate_count; i++) {
            if (result->candidates[i].format == UFT_FORMAT_IMG ||
                result->candidates[i].format == UFT_FORMAT_ST) {
                result->candidates[i].confidence += 0.15f;
                result->content_match = true;
                matches++;
            }
        }
    }
    
    if (detect_apple_dos33(data, size)) {
        for (int i = 0; i < result->candidate_count; i++) {
            if (result->candidates[i].format == UFT_FORMAT_DO) {
                result->candidates[i].confidence += 0.25f;
                result->content_match = true;
                matches++;
            }
        }
    }
    
    return matches;
}

// ============================================================================
// PHASE 5: EXTENSION HINT
// ============================================================================

static const struct {
    const char *ext;
    uft_format_e format;
} EXT_TABLE[] = {
    {".d64", UFT_FORMAT_D64},
    {".d71", UFT_FORMAT_D71},
    {".d81", UFT_FORMAT_D81},
    {".g64", UFT_FORMAT_G64},
    {".adf", UFT_FORMAT_ADF},
    {".adz", UFT_FORMAT_ADZ},
    {".dms", UFT_FORMAT_DMS},
    {".img", UFT_FORMAT_IMG},
    {".ima", UFT_FORMAT_IMA},
    {".imd", UFT_FORMAT_IMD},
    {".st", UFT_FORMAT_ST},
    {".msa", UFT_FORMAT_MSA},
    {".stx", UFT_FORMAT_STX},
    {".atr", UFT_FORMAT_ATR},
    {".xfd", UFT_FORMAT_XFD},
    {".do", UFT_FORMAT_DO},
    {".po", UFT_FORMAT_PO},
    {".nib", UFT_FORMAT_NIB},
    {".woz", UFT_FORMAT_WOZ},
    {".2mg", UFT_FORMAT_2MG},
    {".ssd", UFT_FORMAT_SSD},
    {".dsd", UFT_FORMAT_DSD},
    {".dmk", UFT_FORMAT_DMK},
    {".jv1", UFT_FORMAT_JV1},
    {".jv3", UFT_FORMAT_JV3},
    {".dsk", UFT_FORMAT_DSK},  // Ambiguous!
    {".edsk", UFT_FORMAT_EDSK},
    {".trd", UFT_FORMAT_TRD},
    {".scl", UFT_FORMAT_SCL},
    {".d88", UFT_FORMAT_D88},
    {".scp", UFT_FORMAT_SCP},
    {".hfe", UFT_FORMAT_HFE},
    {".ipf", UFT_FORMAT_IPF},
    {".uff", UFT_FORMAT_RAW},  // UFF
    {NULL, UFT_FORMAT_AUTO}
};

static int detect_by_extension(const char *filename, detection_result_t *result) {
    if (!filename) return 0;
    
    const char *ext = strrchr(filename, '.');
    if (!ext) return 0;
    
    // Convert to lowercase
    char ext_lower[16];
    size_t i;
    for (i = 0; ext[i] && i < sizeof(ext_lower) - 1; i++) {
        ext_lower[i] = tolower((unsigned char)ext[i]);
    }
    ext_lower[i] = '\0';
    
    for (const void *e = EXT_TABLE; ; e = (const char *)e + sizeof(EXT_TABLE[0])) {
        const char *table_ext = *(const char **)e;
        if (!table_ext) break;
        
        uft_format_e table_fmt = *((uft_format_e *)((const char *)e + sizeof(const char *)));
        
        if (strcmp(ext_lower, table_ext) == 0) {
            result->extension_match = true;
            
            // Boost matching candidate
            for (int j = 0; j < result->candidate_count; j++) {
                if (result->candidates[j].format == table_fmt) {
                    result->candidates[j].confidence += 0.05f;
                    return 1;
                }
            }
            
            // Add as weak candidate if not found
            if (result->candidate_count < 10) {
                detection_candidate_t *c = &result->candidates[result->candidate_count++];
                c->format = table_fmt;
                c->confidence = 0.30f;  // Extension-only is weak
                snprintf(c->reason, sizeof(c->reason), "Extension: %s", ext_lower);
            }
            return 1;
        }
    }
    
    return 0;
}

// ============================================================================
// PHASE 6: DISAMBIGUATION
// ============================================================================

static void disambiguate(detection_result_t *result) {
    if (result->candidate_count <= 1) return;
    
    // Sort by confidence (descending)
    for (int i = 0; i < result->candidate_count - 1; i++) {
        for (int j = i + 1; j < result->candidate_count; j++) {
            if (result->candidates[j].confidence > result->candidates[i].confidence) {
                detection_candidate_t tmp = result->candidates[i];
                result->candidates[i] = result->candidates[j];
                result->candidates[j] = tmp;
            }
        }
    }
    
    // Calculate confidence gap
    if (result->candidate_count >= 2) {
        result->confidence_gap = result->candidates[0].confidence - 
                                 result->candidates[1].confidence;
    }
    
    result->best = &result->candidates[0];
    
    // Warn if gap is small
    if (result->confidence_gap < 0.20f && result->candidate_count > 1) {
        snprintf(result->warnings + strlen(result->warnings),
                 sizeof(result->warnings) - strlen(result->warnings),
                 "WARN: Ambiguous detection (gap=%.2f). Consider manual verification.\n",
                 result->confidence_gap);
        result->warning_count++;
    }
}

// ============================================================================
// PHASE 7: VARIANT DETECTION
// ============================================================================

static void detect_variants(const uint8_t *data, size_t size, detection_result_t *result) {
    if (!result->best) return;
    
    switch (result->best->format) {
        case UFT_FORMAT_D64:
            // Check for error map
            if (size == 175531 || size == 197376 || size == 206114) {
                result->has_error_map = true;
            }
            break;
            
        case UFT_FORMAT_ADF:
            // Check filesystem type (OFS vs FFS)
            if (size >= 4) {
                if (data[3] == 0) {
                    result->tool_origin = "OFS";
                } else if (data[3] == 1) {
                    result->tool_origin = "FFS";
                }
            }
            break;
            
        default:
            break;
    }
}

// ============================================================================
// MAIN DETECTION FUNCTION
// ============================================================================

int uft_detect_format(
    const uint8_t *data,
    size_t size,
    const char *filename,
    const char *platform_hint,
    detection_result_t *result
) {
    if (!data || size == 0 || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // Phase 1: Magic bytes
    detect_magic(data, size, result);
    
    // Phase 2: Header structure (integrated with magic for structured formats)
    // Already done in magic detection for formats with headers
    
    // Phase 3: Size analysis
    detect_by_size(size, result);
    
    // Phase 4: Content analysis
    detect_by_content(data, size, result);
    
    // Phase 5: Extension hint
    detect_by_extension(filename, result);
    
    // Phase 6: Disambiguation
    disambiguate(result);
    
    // Phase 7: Variant detection
    detect_variants(data, size, result);
    
    // Final confidence check
    if (result->best && result->best->confidence < 0.50f) {
        snprintf(result->warnings + strlen(result->warnings),
                 sizeof(result->warnings) - strlen(result->warnings),
                 "WARN: Low confidence (%.2f). Format detection uncertain.\n",
                 result->best->confidence);
        result->warning_count++;
    }
    
    (void)platform_hint;  // TODO: Use for disambiguation
    
    return result->candidate_count;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

const char *uft_format_name(uft_format_e format) {
    static const char *names[] = {
        [UFT_FORMAT_AUTO] = "AUTO",
        [UFT_FORMAT_RAW] = "RAW",
        [UFT_FORMAT_D64] = "D64",
        [UFT_FORMAT_D71] = "D71",
        [UFT_FORMAT_D81] = "D81",
        [UFT_FORMAT_G64] = "G64",
        [UFT_FORMAT_G71] = "G71",
        [UFT_FORMAT_ADF] = "ADF",
        [UFT_FORMAT_ADZ] = "ADZ",
        [UFT_FORMAT_DMS] = "DMS",
        [UFT_FORMAT_IMG] = "IMG",
        [UFT_FORMAT_IMA] = "IMA",
        [UFT_FORMAT_DSK] = "DSK",
        [UFT_FORMAT_IMD] = "IMD",
        [UFT_FORMAT_ST] = "ST",
        [UFT_FORMAT_MSA] = "MSA",
        [UFT_FORMAT_STX] = "STX",
        [UFT_FORMAT_ATR] = "ATR",
        [UFT_FORMAT_XFD] = "XFD",
        [UFT_FORMAT_DO] = "DO",
        [UFT_FORMAT_PO] = "PO",
        [UFT_FORMAT_NIB] = "NIB",
        [UFT_FORMAT_WOZ] = "WOZ",
        [UFT_FORMAT_2MG] = "2MG",
        [UFT_FORMAT_SSD] = "SSD",
        [UFT_FORMAT_DSD] = "DSD",
        [UFT_FORMAT_ADF_ACORN] = "ADF-ACORN",
        [UFT_FORMAT_ADL] = "ADL",
        [UFT_FORMAT_DMK] = "DMK",
        [UFT_FORMAT_JV1] = "JV1",
        [UFT_FORMAT_JV3] = "JV3",
        [UFT_FORMAT_MSX_DSK] = "MSX-DSK",
        [UFT_FORMAT_DSK_CPC] = "DSK-CPC",
        [UFT_FORMAT_EDSK] = "EDSK",
        [UFT_FORMAT_TRD] = "TRD",
        [UFT_FORMAT_SCL] = "SCL",
        [UFT_FORMAT_FDI] = "FDI",
        [UFT_FORMAT_D88] = "D88",
        [UFT_FORMAT_NFD] = "NFD",
        [UFT_FORMAT_HDM] = "HDM",
        [UFT_FORMAT_SCP] = "SCP",
        [UFT_FORMAT_HFE] = "HFE",
        [UFT_FORMAT_IPF] = "IPF",
        [UFT_FORMAT_KF_RAW] = "KF-RAW",
        [UFT_FORMAT_KF_STREAM] = "KF-STREAM",
        [UFT_FORMAT_A2R] = "A2R",
        [UFT_FORMAT_FLUX] = "FLUX"
    };
    
    if (format < sizeof(names)/sizeof(names[0]) && names[format]) {
        return names[format];
    }
    return "UNKNOWN";
}

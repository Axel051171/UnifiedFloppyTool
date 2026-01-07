/**
 * @file uft_bayesian_detect.c
 * @brief GOD MODE: Bayesian Format Detection
 * 
 * Uses probabilistic inference to identify disk image formats.
 * Combines multiple evidence sources with prior probabilities.
 * 
 * Advantages over simple magic-byte detection:
 * - Handles ambiguous formats (D64 vs ADF vs raw)
 * - Provides confidence scores
 * - Explains detection reasoning
 * - Gracefully handles partial/corrupted data
 * 
 * @author Superman QA - GOD MODE
 * @date 2026
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// Format Database
// ============================================================================

#define MAX_FORMATS 13
#define MAX_MAGIC_LEN 16
#define MAX_SIZES 8

typedef struct {
    const char* name;
    const char* description;
    
    // Size evidence
    size_t valid_sizes[MAX_SIZES];
    int num_sizes;
    
    // Magic bytes evidence
    uint8_t magic[MAX_MAGIC_LEN];
    size_t magic_offset;
    size_t magic_len;
    
    // Structure evidence
    int sector_size;            // 0 = variable
    int sectors_per_track;      // 0 = variable
    int num_tracks;             // 0 = variable
    int num_sides;
    
    // Encoding evidence
    enum { ENC_UNKNOWN, ENC_RAW, ENC_MFM, ENC_FM, ENC_GCR } encoding;
    
    // Filesystem evidence
    enum { FS_NONE, FS_FAT12, FS_FAT16, FS_OFS, FS_FFS, FS_CBM_DOS } filesystem;
    
    // Platform association
    enum { PLAT_GENERIC, PLAT_COMMODORE, PLAT_AMIGA, PLAT_APPLE, 
           PLAT_ATARI, PLAT_IBM_PC, PLAT_BBC } platform;
    
    // Prior probability (how common is this format)
    float prior;
    
} format_spec_t;

// Known formats database
static const format_spec_t FORMAT_DB[] = {
    // Commodore
    {
        .name = "D64",
        .description = "Commodore 64 Disk Image",
        .valid_sizes = {174848, 175531, 196608, 197376, 205312, 206114},
        .num_sizes = 6,
        .magic = {0},
        .magic_len = 0,
        .sector_size = 256,
        .num_tracks = 35,
        .encoding = ENC_GCR,
        .filesystem = FS_CBM_DOS,
        .platform = PLAT_COMMODORE,
        .prior = 0.15f
    },
    {
        .name = "G64",
        .description = "Commodore 64 GCR Image",
        .magic = {'G', 'C', 'R', '-', '1', '5', '4', '1'},
        .magic_offset = 0,
        .magic_len = 8,
        .encoding = ENC_GCR,
        .platform = PLAT_COMMODORE,
        .prior = 0.05f
    },
    {
        .name = "D71",
        .description = "Commodore 128 Disk Image",
        .valid_sizes = {349696, 351062},
        .num_sizes = 2,
        .sector_size = 256,
        .num_tracks = 70,
        .num_sides = 2,
        .encoding = ENC_GCR,
        .filesystem = FS_CBM_DOS,
        .platform = PLAT_COMMODORE,
        .prior = 0.03f
    },
    {
        .name = "D81",
        .description = "Commodore 1581 Disk Image",
        .valid_sizes = {819200, 822400},
        .num_sizes = 2,
        .sector_size = 512,
        .num_tracks = 80,
        .encoding = ENC_MFM,
        .filesystem = FS_CBM_DOS,
        .platform = PLAT_COMMODORE,
        .prior = 0.03f
    },
    
    // Amiga
    {
        .name = "ADF",
        .description = "Amiga Disk File",
        .valid_sizes = {901120, 1802240},
        .num_sizes = 2,
        .sector_size = 512,
        .sectors_per_track = 11,
        .num_tracks = 80,
        .num_sides = 2,
        .encoding = ENC_MFM,
        .filesystem = FS_OFS,  // or FFS
        .platform = PLAT_AMIGA,
        .prior = 0.12f
    },
    
    // Atari
    {
        .name = "ATR",
        .description = "Atari 8-bit Disk Image",
        .magic = {0x96, 0x02},
        .magic_offset = 0,
        .magic_len = 2,
        .sector_size = 128,
        .platform = PLAT_ATARI,
        .prior = 0.05f
    },
    {
        .name = "ST",
        .description = "Atari ST Disk Image",
        .valid_sizes = {368640, 737280, 819200},
        .num_sizes = 3,
        .sector_size = 512,
        .encoding = ENC_MFM,
        .filesystem = FS_FAT12,
        .platform = PLAT_ATARI,
        .prior = 0.05f
    },
    
    // Apple
    {
        .name = "DSK",
        .description = "Apple II DOS 3.3 Disk",
        .valid_sizes = {143360},
        .num_sizes = 1,
        .sector_size = 256,
        .sectors_per_track = 16,
        .num_tracks = 35,
        .encoding = ENC_GCR,
        .platform = PLAT_APPLE,
        .prior = 0.05f
    },
    {
        .name = "WOZ",
        .description = "Apple II Flux Image",
        .magic = {'W', 'O', 'Z', '1'},
        .magic_offset = 0,
        .magic_len = 4,
        .encoding = ENC_GCR,
        .platform = PLAT_APPLE,
        .prior = 0.03f
    },
    
    // IBM PC
    {
        .name = "IMG",
        .description = "Raw Sector Image",
        .valid_sizes = {163840, 184320, 327680, 368640, 737280, 1228800, 1474560},
        .num_sizes = 7,
        .sector_size = 512,
        .encoding = ENC_MFM,
        .filesystem = FS_FAT12,
        .platform = PLAT_IBM_PC,
        .prior = 0.15f
    },
    
    // Flux formats
    {
        .name = "SCP",
        .description = "SuperCard Pro Flux",
        .magic = {'S', 'C', 'P'},
        .magic_offset = 0,
        .magic_len = 3,
        .prior = 0.08f
    },
    {
        .name = "HFE",
        .description = "UFT HFE Format",
        .magic = {'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E'},
        .magic_offset = 0,
        .magic_len = 8,
        .prior = 0.05f
    },
    {
        .name = "IPF",
        .description = "Interchangeable Preservation Format",
        .magic = {'C', 'A', 'P', 'S'},
        .magic_offset = 0,
        .magic_len = 4,
        .prior = 0.04f
    },
    
    // End marker
    {.name = NULL}
};

// ============================================================================
// Evidence Types
// ============================================================================

typedef struct {
    // File evidence
    size_t file_size;
    bool has_file_size;
    
    // Magic evidence
    uint8_t header[64];
    size_t header_len;
    bool has_header;
    
    // Structure evidence
    int detected_sector_size;
    int detected_sectors_per_track;
    bool has_structure;
    
    // Filesystem evidence
    bool has_boot_sector;
    uint8_t boot_sector[512];
    char oem_name[9];
    int fat_type;               // 0 = none, 12, 16, 32
    
    // Content evidence
    bool has_cbm_dos_header;    // Track 18, sector 0 signature
    bool has_amiga_bootblock;   // DOS\0 or DOS\1
    
} format_evidence_t;

// ============================================================================
// Bayesian Inference
// ============================================================================

typedef struct {
    const format_spec_t* format;
    float prior;
    float likelihood;
    float posterior;
    
    // Explanation
    char evidence_summary[256];
    float size_match;
    float magic_match;
    float structure_match;
    float content_match;
} format_hypothesis_t;

typedef struct {
    format_hypothesis_t hypotheses[MAX_FORMATS];
    int num_hypotheses;
    
    const format_spec_t* best_match;
    float best_confidence;
    
    const format_spec_t* second_match;
    float second_confidence;
    
    bool ambiguous;             // Multiple high-confidence matches
    char explanation[512];
} detection_result_t;

// ============================================================================
// Evidence Gathering
// ============================================================================

/**
 * @brief Gather evidence from file data
 */
void gather_evidence(const uint8_t* data, size_t size, format_evidence_t* ev) {
    memset(ev, 0, sizeof(*ev));
    
    // File size
    ev->file_size = size;
    ev->has_file_size = true;
    
    // Header
    ev->header_len = (size < 64) ? size : 64;
    memcpy(ev->header, data, ev->header_len);
    ev->has_header = (ev->header_len >= 4);
    
    // Boot sector analysis (if large enough)
    if (size >= 512) {
        memcpy(ev->boot_sector, data, 512);
        ev->has_boot_sector = true;
        
        // Check for FAT boot sector
        if (data[510] == 0x55 && data[511] == 0xAA) {
            memcpy(ev->oem_name, data + 3, 8);
            ev->oem_name[8] = '\0';
            
            // FAT type detection
            uint16_t bytes_per_sector = data[11] | (data[12] << 8);
            if (bytes_per_sector == 512) {
                ev->fat_type = 12;  // Assume FAT12 for floppy
            }
        }
        
        // Check for Amiga bootblock
        if (memcmp(data, "DOS", 3) == 0) {
            ev->has_amiga_bootblock = true;
        }
    }
    
    // CBM DOS header check (at typical location)
    if (size >= 174848) {
        // Track 18, sector 0 is at offset 0x16500 in D64
        size_t t18s0 = 0x16500;
        if (size > t18s0 + 2) {
            // First byte should be 0x12 (track) for directory
            if (data[t18s0] == 0x12) {
                ev->has_cbm_dos_header = true;
            }
        }
    }
}

// ============================================================================
// Likelihood Calculation
// ============================================================================

/**
 * @brief Calculate P(evidence | format)
 */
float calculate_likelihood(const format_spec_t* fmt, const format_evidence_t* ev) {
    float likelihood = 1.0f;
    
    // Size evidence
    if (ev->has_file_size && fmt->num_sizes > 0) {
        bool size_match = false;
        for (int i = 0; i < fmt->num_sizes; i++) {
            if (ev->file_size == fmt->valid_sizes[i]) {
                size_match = true;
                break;
            }
        }
        likelihood *= size_match ? 0.9f : 0.1f;
    }
    
    // Magic evidence
    if (ev->has_header && fmt->magic_len > 0) {
        if (fmt->magic_offset + fmt->magic_len <= ev->header_len) {
            bool magic_match = (memcmp(ev->header + fmt->magic_offset, 
                                       fmt->magic, fmt->magic_len) == 0);
            likelihood *= magic_match ? 0.95f : 0.01f;
        }
    }
    
    // Filesystem evidence
    if (ev->has_boot_sector) {
        if (fmt->filesystem == FS_FAT12 && ev->fat_type == 12) {
            likelihood *= 0.8f;
        } else if (fmt->filesystem == FS_OFS && ev->has_amiga_bootblock) {
            likelihood *= 0.85f;
        } else if (fmt->filesystem == FS_CBM_DOS && ev->has_cbm_dos_header) {
            likelihood *= 0.85f;
        }
    }
    
    return likelihood;
}

// ============================================================================
// Bayesian Detection
// ============================================================================

/**
 * @brief Detect format using Bayesian inference
 */
bool bayesian_detect(const uint8_t* data, size_t size, detection_result_t* result) {
    if (!data || size == 0 || !result) return false;
    
    memset(result, 0, sizeof(*result));
    
    // Gather evidence
    format_evidence_t evidence;
    gather_evidence(data, size, &evidence);
    
    // Build hypotheses
    float total_posterior = 0;
    
    for (int i = 0; FORMAT_DB[i].name != NULL && i < MAX_FORMATS; i++) {
        const format_spec_t* fmt = &FORMAT_DB[i];
        format_hypothesis_t* hyp = &result->hypotheses[result->num_hypotheses];
        
        hyp->format = fmt;
        hyp->prior = fmt->prior;
        hyp->likelihood = calculate_likelihood(fmt, &evidence);
        hyp->posterior = hyp->prior * hyp->likelihood;
        
        total_posterior += hyp->posterior;
        result->num_hypotheses++;
    }
    
    // Normalize posteriors
    if (total_posterior > 0) {
        for (int i = 0; i < result->num_hypotheses; i++) {
            result->hypotheses[i].posterior /= total_posterior;
        }
    }
    
    // Sort by posterior (simple bubble sort)
    for (int i = 0; i < result->num_hypotheses - 1; i++) {
        for (int j = 0; j < result->num_hypotheses - 1 - i; j++) {
            if (result->hypotheses[j].posterior < result->hypotheses[j+1].posterior) {
                format_hypothesis_t tmp = result->hypotheses[j];
                result->hypotheses[j] = result->hypotheses[j+1];
                result->hypotheses[j+1] = tmp;
            }
        }
    }
    
    // Best match
    if (result->num_hypotheses > 0) {
        result->best_match = result->hypotheses[0].format;
        result->best_confidence = result->hypotheses[0].posterior * 100;
    }
    
    // Second best
    if (result->num_hypotheses > 1) {
        result->second_match = result->hypotheses[1].format;
        result->second_confidence = result->hypotheses[1].posterior * 100;
        
        // Check for ambiguity
        if (result->second_confidence > result->best_confidence * 0.7f) {
            result->ambiguous = true;
        }
    }
    
    // Generate explanation
    snprintf(result->explanation, sizeof(result->explanation),
             "Best: %s (%.1f%%), Second: %s (%.1f%%)%s",
             result->best_match ? result->best_match->name : "Unknown",
             result->best_confidence,
             result->second_match ? result->second_match->name : "None",
             result->second_confidence,
             result->ambiguous ? " [AMBIGUOUS]" : "");
    
    return (result->best_match != NULL);
}

// ============================================================================
// Convenience API
// ============================================================================

/**
 * @brief Quick format detection
 */
const char* detect_format_quick(const uint8_t* data, size_t size, float* confidence) {
    detection_result_t result;
    
    if (!bayesian_detect(data, size, &result)) {
        if (confidence) *confidence = 0;
        return "Unknown";
    }
    
    if (confidence) *confidence = result.best_confidence;
    return result.best_match->name;
}

/**
 * @brief Print detection results
 */
void print_detection_results(const detection_result_t* result, FILE* out) {
    fprintf(out, "\n");
    fprintf(out, "╔══════════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║  BAYESIAN FORMAT DETECTION                                   ║\n");
    fprintf(out, "╠══════════════════════════════════════════════════════════════╣\n");
    
    fprintf(out, "║  Best Match: %-15s  Confidence: %5.1f%%            ║\n",
            result->best_match ? result->best_match->name : "Unknown",
            result->best_confidence);
    
    if (result->second_match) {
        fprintf(out, "║  2nd Match:  %-15s  Confidence: %5.1f%%            ║\n",
                result->second_match->name, result->second_confidence);
    }
    
    if (result->ambiguous) {
        fprintf(out, "║  ⚠️  AMBIGUOUS: Multiple formats match                       ║\n");
    }
    
    fprintf(out, "╠══════════════════════════════════════════════════════════════╣\n");
    fprintf(out, "║  Top Candidates:                                             ║\n");
    
    for (int i = 0; i < result->num_hypotheses && i < 5; i++) {
        const format_hypothesis_t* h = &result->hypotheses[i];
        fprintf(out, "║    %d. %-12s  P=%.1f%%  (prior=%.2f, L=%.3f)          ║\n",
                i + 1, h->format->name, h->posterior * 100, h->prior, h->likelihood);
    }
    
    fprintf(out, "╚══════════════════════════════════════════════════════════════╝\n");
}

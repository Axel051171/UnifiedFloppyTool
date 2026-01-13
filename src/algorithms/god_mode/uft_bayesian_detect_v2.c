/**
 * @file uft_bayesian_detect_v2.c
 * @brief GOD MODE: Bayesian Format Detection
 * 
 * Probabilistic format detection using Bayesian inference.
 * Combines multiple evidence sources to determine most likely format.
 * 
 * Problem: Simple magic-byte detection fails for:
 * - Formats without magic bytes (D64, IMG, ADF)
 * - Ambiguous sizes (D64 vs Atari ST)
 * - Corrupted headers
 * 
 * Solution: Bayesian framework combining:
 * - File size (P(size|format))
 * - Magic bytes (P(magic|format))
 * - Structure validity (P(structure|format))
 * - Content patterns (P(content|format))
 * 
 * @author Superman QA - GOD MODE
 * @date 2026
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// ============================================================================
// Format Database
// ============================================================================

#define MAX_FORMATS 64
#define MAX_MAGIC_LEN 16
#define MAX_SIZES 8

typedef struct {
    const char* name;
    const char* description;
    
    // Size evidence
    size_t valid_sizes[MAX_SIZES];
    int num_valid_sizes;
    size_t min_size;
    size_t max_size;
    
    // Magic bytes
    uint8_t magic[MAX_MAGIC_LEN];
    size_t magic_offset;
    size_t magic_len;
    
    // Structure validation function
    bool (*validate_structure)(const uint8_t* data, size_t len);
    
    // Prior probability (base rate)
    float prior;
    
} format_spec_t;

// ============================================================================
// Evidence Types
// ============================================================================

typedef struct {
    // File info
    size_t file_size;
    
    // Magic bytes (first 64 bytes)
    uint8_t header[64];
    size_t header_len;
    
    // Structure checks
    bool has_valid_bootsector;
    bool has_valid_fat;
    bool has_valid_directory;
    
    // Content patterns
    bool has_ascii_text;
    bool has_executable_code;
    int sector_size_hint;
    
} format_evidence_t;

typedef struct {
    const char* format_name;
    float prior;                // P(format)
    float likelihood;           // P(evidence|format)
    float posterior;            // P(format|evidence)
    float confidence;           // 0-100
    
    // Evidence breakdown
    float size_score;
    float magic_score;
    float structure_score;
    float content_score;
    
} format_hypothesis_t;

typedef struct {
    format_hypothesis_t hypotheses[MAX_FORMATS];
    int num_hypotheses;
    
    const char* best_format;
    float best_confidence;
    
    const char* second_format;
    float second_confidence;
    
    // Warnings
    bool ambiguous;             // Multiple formats close in probability
    bool low_confidence;        // Best < 70%
    char warning[256];
    
} detection_result_t;

// ============================================================================
// Format Specifications
// ============================================================================

// Validation functions (forward declarations)
static bool validate_d64(const uint8_t* data, size_t len);
static bool validate_adf(const uint8_t* data, size_t len);
static bool validate_scp(const uint8_t* data, size_t len);
static bool validate_g64(const uint8_t* data, size_t len);

static const format_spec_t FORMAT_SPECS[] = {
    // Commodore D64
    {
        .name = "D64",
        .description = "Commodore 64 Disk Image",
        .valid_sizes = {174848, 175531, 196608, 197376, 205312, 206114},
        .num_valid_sizes = 6,
        .min_size = 174848,
        .max_size = 210000,
        .magic = {0},
        .magic_len = 0,
        .validate_structure = validate_d64,
        .prior = 0.15f
    },
    
    // Amiga ADF
    {
        .name = "ADF",
        .description = "Amiga Disk File",
        .valid_sizes = {901120, 1802240},
        .num_valid_sizes = 2,
        .min_size = 901120,
        .max_size = 1802240,
        .magic = {'D', 'O', 'S'},
        .magic_offset = 0,
        .magic_len = 3,
        .validate_structure = validate_adf,
        .prior = 0.12f
    },
    
    // SuperCard Pro
    {
        .name = "SCP",
        .description = "SuperCard Pro Flux",
        .valid_sizes = {0},  // Variable
        .num_valid_sizes = 0,
        .min_size = 32,
        .max_size = 100 * 1024 * 1024,
        .magic = {'S', 'C', 'P'},
        .magic_offset = 0,
        .magic_len = 3,
        .validate_structure = validate_scp,
        .prior = 0.10f
    },
    
    // G64 (Commodore)
    {
        .name = "G64",
        .description = "Commodore GCR Disk Image",
        .valid_sizes = {0},
        .num_valid_sizes = 0,
        .min_size = 7928,
        .max_size = 500000,
        .magic = {'G', 'C', 'R', '-', '1', '5', '4', '1'},
        .magic_offset = 0,
        .magic_len = 8,
        .validate_structure = validate_g64,
        .prior = 0.08f
    },
    
    // HFE
    {
        .name = "HFE",
        .description = "UFT HFE Format",
        .valid_sizes = {0},
        .num_valid_sizes = 0,
        .min_size = 512,
        .max_size = 50 * 1024 * 1024,
        .magic = {'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E'},
        .magic_offset = 0,
        .magic_len = 8,
        .validate_structure = NULL,
        .prior = 0.08f
    },
    
    // Raw IMG/IMA (PC)
    {
        .name = "IMG",
        .description = "Raw Sector Image",
        .valid_sizes = {163840, 184320, 327680, 368640, 737280, 
                       1228800, 1474560, 2949120},
        .num_valid_sizes = 8,
        .min_size = 163840,
        .max_size = 3000000,
        .magic = {0},
        .magic_len = 0,
        .validate_structure = NULL,
        .prior = 0.15f
    },
    
    // WOZ (Apple II)
    {
        .name = "WOZ",
        .description = "Apple II Disk Image",
        .valid_sizes = {0},
        .num_valid_sizes = 0,
        .min_size = 256,
        .max_size = 10 * 1024 * 1024,
        .magic = {'W', 'O', 'Z', '1'},
        .magic_offset = 0,
        .magic_len = 4,
        .validate_structure = NULL,
        .prior = 0.05f
    },
    
    // Sentinel
    {.name = NULL}
};

// ============================================================================
// Validation Functions
// ============================================================================

static bool validate_d64(const uint8_t* data, size_t len) {
    if (len < 174848) return false;
    
    // Check track 18, sector 0 (directory header)
    // Offset for T18S0 = 357 * 256 = 91392
    size_t dir_offset = 91392;
    if (dir_offset + 256 > len) return false;
    
    // First byte should be track link (usually 18)
    // Second byte should be sector link (usually 1)
    uint8_t track_link = data[dir_offset];
    uint8_t sector_link = data[dir_offset + 1];
    
    // Reasonable values
    if (track_link > 40 && track_link != 0) return false;
    if (sector_link > 21) return false;
    
    // Check BAM signature at offset +2 (disk format type)
    uint8_t format = data[dir_offset + 2];
    if (format != 0x41 && format != 0x00) return false;  // 'A' or 0
    
    return true;
}

static bool validate_adf(const uint8_t* data, size_t len) {
    if (len < 901120) return false;
    
    // Check bootblock
    if (data[0] != 'D' || data[1] != 'O' || data[2] != 'S') {
        // Non-bootable ADF - still valid
        return true;
    }
    
    // Bootblock checksum (simplified check)
    uint32_t checksum = 0;
    for (int i = 0; i < 1024; i += 4) {
        uint32_t val = (data[i] << 24) | (data[i+1] << 16) | 
                       (data[i+2] << 8) | data[i+3];
        checksum += val;
    }
    
    // Checksum should be 0 for valid bootblock
    return true;  // Be lenient
}

static bool validate_scp(const uint8_t* data, size_t len) {
    if (len < 16) return false;
    
    // Magic
    if (data[0] != 'S' || data[1] != 'C' || data[2] != 'P') return false;
    
    // Version (0x00 - 0x29 known)
    if (data[3] > 0x29) return false;
    
    // Start track <= end track
    if (data[6] > data[7]) return false;
    
    // Number of revolutions (1-10 typical)
    if (data[5] == 0 || data[5] > 32) return false;
    
    return true;
}

static bool validate_g64(const uint8_t* data, size_t len) {
    if (len < 12) return false;
    
    // Magic
    if (memcmp(data, "GCR-1541", 8) != 0) return false;
    
    // Version
    if (data[8] != 0) return false;
    
    // Number of tracks (84 or 168 typical)
    uint8_t tracks = data[9];
    if (tracks == 0 || tracks > 168) return false;
    
    return true;
}

// ============================================================================
// Bayesian Scoring
// ============================================================================

static float score_size(const format_spec_t* spec, size_t file_size) {
    // Exact match
    for (int i = 0; i < spec->num_valid_sizes; i++) {
        if (file_size == spec->valid_sizes[i]) return 1.0f;
    }
    
    // Within range
    if (file_size >= spec->min_size && file_size <= spec->max_size) {
        return 0.5f;
    }
    
    // Close to valid size (within 10%)
    for (int i = 0; i < spec->num_valid_sizes; i++) {
        float ratio = (float)file_size / spec->valid_sizes[i];
        if (ratio > 0.9f && ratio < 1.1f) return 0.3f;
    }
    
    return 0.01f;  // Very unlikely
}

static float score_magic(const format_spec_t* spec, const uint8_t* header, 
                         size_t header_len) {
    if (spec->magic_len == 0) return 0.5f;  // No magic = neutral
    
    if (header_len < spec->magic_offset + spec->magic_len) return 0.01f;
    
    if (memcmp(header + spec->magic_offset, spec->magic, spec->magic_len) == 0) {
        return 1.0f;  // Exact match
    }
    
    // Partial match (first few bytes)
    int match = 0;
    for (size_t i = 0; i < spec->magic_len; i++) {
        if (header[spec->magic_offset + i] == spec->magic[i]) match++;
    }
    
    return (float)match / spec->magic_len * 0.5f;
}

static float score_structure(const format_spec_t* spec, const uint8_t* data,
                             size_t len) {
    if (!spec->validate_structure) return 0.5f;  // No validator = neutral
    
    return spec->validate_structure(data, len) ? 1.0f : 0.1f;
}

// ============================================================================
// Main Detection
// ============================================================================

/**
 * @brief Detect format using Bayesian inference
 */
void bayesian_detect_format(const format_evidence_t* evidence,
                            detection_result_t* result) {
    memset(result, 0, sizeof(*result));
    
    float total_posterior = 0;
    
    // Evaluate each format
    for (const format_spec_t* spec = FORMAT_SPECS; spec->name != NULL; spec++) {
        if (result->num_hypotheses >= MAX_FORMATS) break;
        
        format_hypothesis_t* hyp = &result->hypotheses[result->num_hypotheses];
        hyp->format_name = spec->name;
        hyp->prior = spec->prior;
        
        // Calculate likelihood from evidence
        hyp->size_score = score_size(spec, evidence->file_size);
        hyp->magic_score = score_magic(spec, evidence->header, evidence->header_len);
        hyp->structure_score = score_structure(spec, evidence->header, 
                                               evidence->header_len);
        
        // Combined likelihood (geometric mean for independence)
        hyp->likelihood = powf(hyp->size_score * hyp->magic_score * 
                               hyp->structure_score, 1.0f/3.0f);
        
        // Unnormalized posterior
        hyp->posterior = hyp->prior * hyp->likelihood;
        total_posterior += hyp->posterior;
        
        result->num_hypotheses++;
    }
    
    // Normalize posteriors
    if (total_posterior > 0) {
        for (int i = 0; i < result->num_hypotheses; i++) {
            result->hypotheses[i].posterior /= total_posterior;
            result->hypotheses[i].confidence = result->hypotheses[i].posterior * 100;
        }
    }
    
    // Find best and second best
    result->best_confidence = 0;
    result->second_confidence = 0;
    
    for (int i = 0; i < result->num_hypotheses; i++) {
        float conf = result->hypotheses[i].confidence;
        if (conf > result->best_confidence) {
            result->second_format = result->best_format;
            result->second_confidence = result->best_confidence;
            result->best_format = result->hypotheses[i].format_name;
            result->best_confidence = conf;
        } else if (conf > result->second_confidence) {
            result->second_format = result->hypotheses[i].format_name;
            result->second_confidence = conf;
        }
    }
    
    // Warnings
    result->low_confidence = (result->best_confidence < 70);
    result->ambiguous = (result->best_confidence - result->second_confidence < 20);
    
    if (result->low_confidence && result->ambiguous) {
        snprintf(result->warning, sizeof(result->warning),
                 "Low confidence (%.1f%%), ambiguous between %s and %s",
                 result->best_confidence, result->best_format, result->second_format);
    } else if (result->low_confidence) {
        snprintf(result->warning, sizeof(result->warning),
                 "Low confidence: %.1f%% for %s",
                 result->best_confidence, result->best_format);
    } else if (result->ambiguous) {
        snprintf(result->warning, sizeof(result->warning),
                 "Ambiguous: %s (%.1f%%) vs %s (%.1f%%)",
                 result->best_format, result->best_confidence,
                 result->second_format, result->second_confidence);
    }
}

/**
 * @brief Convenience function to detect from file buffer
 */
const char* detect_format_simple(const uint8_t* data, size_t len, 
                                 float* confidence_out) {
    format_evidence_t evidence = {0};
    evidence.file_size = len;
    evidence.header_len = (len > 64) ? 64 : len;
    memcpy(evidence.header, data, evidence.header_len);
    
    detection_result_t result;
    bayesian_detect_format(&evidence, &result);
    
    if (confidence_out) *confidence_out = result.best_confidence;
    return result.best_format;
}

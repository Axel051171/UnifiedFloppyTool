/**
 * @file uft_bayesian_format.c
 * @brief Bayesian Format Classifier Implementation
 * 
 * BAYESIAN CLASSIFICATION MODEL
 * =============================
 * 
 * We compute P(Format | Evidence) for each format F:
 * 
 *   P(F | E) = P(E | F) × P(F) / P(E)
 * 
 * Since P(E) is constant across formats, we compute:
 * 
 *   P(F | E) ∝ P(E | F) × P(F)
 * 
 * Then normalize so probabilities sum to 1.
 * 
 * LIKELIHOOD MODEL P(E | F)
 * =========================
 * 
 * For each evidence type, we model likelihood:
 * 
 * 1. File Size:
 *    - Exact match: P = 0.95
 *    - Close match (±5%): P = 0.7
 *    - Far match: P = 0.1
 *    
 * 2. Magic Bytes:
 *    - Full match: P = 0.99
 *    - Partial match: P = 0.5
 *    - No match: P = 0.1
 *    
 * 3. Geometry:
 *    - Each matching field multiplies likelihood
 *    
 * 4. Extension:
 *    - Known extension for format: P = 0.8
 *    - Generic extension: P = 0.5
 *    
 * PRIOR MODEL P(F)
 * ================
 * 
 * Default priors based on historical prevalence:
 * - PC formats: 0.35 (most common)
 * - Amiga: 0.15
 * - C64: 0.12
 * - Atari ST: 0.10
 * - Apple II: 0.08
 * - Others: distributed
 * 
 * Regional adjustments available.
 */

#include "uft/algorithms/uft_bayesian_format.h"
#include <string.h>
#include <math.h>

// ============================================================================
// FORMAT DATABASE
// ============================================================================

typedef struct {
    const char *id;
    const char *name;
    uft_format_family_t family;
    
    size_t expected_size;           // 0 = variable
    uint32_t tracks;
    uint32_t heads;
    uint32_t sectors;
    uint32_t sector_size;
    
    const uint8_t *magic;
    size_t magic_len;
    size_t magic_offset;
    
    const char *extensions;         // comma-separated
    
    float base_prior;               // Before regional adjustment
} format_definition_t;

// Magic bytes
static const uint8_t MAGIC_FAT[] = {0x55, 0xAA};
static const uint8_t MAGIC_AMIGA[] = {'D', 'O', 'S'};
static const uint8_t MAGIC_D64[] = {0x12, 0x01, 0x41};  // Track 18 sector 0

static const format_definition_t FORMAT_DB[] = {
    // PC Formats
    {
        .id = "pc_160k", .name = "PC 160K (5.25\" SS/DD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_size = 163840,
        .tracks = 40, .heads = 1, .sectors = 8, .sector_size = 512,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .base_prior = 0.02f
    },
    {
        .id = "pc_180k", .name = "PC 180K (5.25\" SS/DD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_size = 184320,
        .tracks = 40, .heads = 1, .sectors = 9, .sector_size = 512,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .base_prior = 0.02f
    },
    {
        .id = "pc_320k", .name = "PC 320K (5.25\" DS/DD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_size = 327680,
        .tracks = 40, .heads = 2, .sectors = 8, .sector_size = 512,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .base_prior = 0.03f
    },
    {
        .id = "pc_360k", .name = "PC 360K (5.25\" DS/DD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_size = 368640,
        .tracks = 40, .heads = 2, .sectors = 9, .sector_size = 512,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .base_prior = 0.05f
    },
    {
        .id = "pc_720k", .name = "PC 720K (3.5\" DS/DD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_size = 737280,
        .tracks = 80, .heads = 2, .sectors = 9, .sector_size = 512,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .base_prior = 0.08f
    },
    {
        .id = "pc_1200k", .name = "PC 1.2M (5.25\" HD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_size = 1228800,
        .tracks = 80, .heads = 2, .sectors = 15, .sector_size = 512,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .base_prior = 0.06f
    },
    {
        .id = "pc_1440k", .name = "PC 1.44M (3.5\" HD)",
        .family = UFT_FAMILY_PC_FAT,
        .expected_size = 1474560,
        .tracks = 80, .heads = 2, .sectors = 18, .sector_size = 512,
        .magic = MAGIC_FAT, .magic_len = 2, .magic_offset = 510,
        .extensions = "img,ima,dsk",
        .base_prior = 0.12f
    },
    
    // Amiga Formats
    {
        .id = "amiga_dd", .name = "Amiga DD (880K)",
        .family = UFT_FAMILY_AMIGA,
        .expected_size = 901120,
        .tracks = 80, .heads = 2, .sectors = 11, .sector_size = 512,
        .magic = MAGIC_AMIGA, .magic_len = 3, .magic_offset = 0,
        .extensions = "adf",
        .base_prior = 0.10f
    },
    {
        .id = "amiga_hd", .name = "Amiga HD (1.76M)",
        .family = UFT_FAMILY_AMIGA,
        .expected_size = 1802240,
        .tracks = 80, .heads = 2, .sectors = 22, .sector_size = 512,
        .magic = MAGIC_AMIGA, .magic_len = 3, .magic_offset = 0,
        .extensions = "adf",
        .base_prior = 0.03f
    },
    
    // C64 Formats
    {
        .id = "c64_d64", .name = "C64 D64 (170K)",
        .family = UFT_FAMILY_C64,
        .expected_size = 174848,
        .tracks = 35, .heads = 1, .sectors = 21, .sector_size = 256,
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d64",
        .base_prior = 0.08f
    },
    {
        .id = "c64_d64_errors", .name = "C64 D64 with errors (175K)",
        .family = UFT_FAMILY_C64,
        .expected_size = 175531,
        .tracks = 35, .heads = 1, .sectors = 21, .sector_size = 256,
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d64",
        .base_prior = 0.04f
    },
    {
        .id = "c64_d71", .name = "C64 D71 (340K)",
        .family = UFT_FAMILY_C64,
        .expected_size = 349696,
        .tracks = 70, .heads = 1, .sectors = 21, .sector_size = 256,
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d71",
        .base_prior = 0.03f
    },
    {
        .id = "c64_d81", .name = "C64 D81 (800K)",
        .family = UFT_FAMILY_C64,
        .expected_size = 819200,
        .tracks = 80, .heads = 2, .sectors = 10, .sector_size = 512,
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "d81",
        .base_prior = 0.02f
    },
    
    // Atari ST
    {
        .id = "atari_st_ss", .name = "Atari ST SS (360K)",
        .family = UFT_FAMILY_ATARI_ST,
        .expected_size = 368640,
        .tracks = 80, .heads = 1, .sectors = 9, .sector_size = 512,
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "st",
        .base_prior = 0.03f
    },
    {
        .id = "atari_st_ds", .name = "Atari ST DS (720K)",
        .family = UFT_FAMILY_ATARI_ST,
        .expected_size = 737280,
        .tracks = 80, .heads = 2, .sectors = 9, .sector_size = 512,
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "st",
        .base_prior = 0.05f
    },
    
    // Apple II
    {
        .id = "apple2_140k", .name = "Apple II (140K)",
        .family = UFT_FAMILY_APPLE_II,
        .expected_size = 143360,
        .tracks = 35, .heads = 1, .sectors = 16, .sector_size = 256,
        .magic = NULL, .magic_len = 0, .magic_offset = 0,
        .extensions = "dsk,do,po",
        .base_prior = 0.06f
    },
    
    // Sentinel
    {.id = NULL}
};

#define FORMAT_COUNT (sizeof(FORMAT_DB) / sizeof(FORMAT_DB[0]) - 1)

// ============================================================================
// REGIONAL PRIORS
// ============================================================================

static float g_region_multiplier[UFT_FAMILY_COUNT] = {
    1.0f,   // UNKNOWN
    1.0f,   // PC_FAT
    1.0f,   // AMIGA
    1.0f,   // ATARI_ST
    1.0f,   // C64
    1.0f,   // APPLE_II
    1.0f,   // MAC
    1.0f,   // SPECTRUM
    1.0f,   // AMSTRAD
    1.0f,   // MSX
    1.0f,   // PC98
    1.0f,   // CPM
    1.0f    // CUSTOM
};

void uft_format_set_region_priors(const char *region) {
    if (!region) return;
    
    // Reset to defaults
    for (int i = 0; i < UFT_FAMILY_COUNT; i++) {
        g_region_multiplier[i] = 1.0f;
    }
    
    if (strcmp(region, "eu") == 0) {
        // Europe: more Amiga, Atari ST, Spectrum, Amstrad
        g_region_multiplier[UFT_FAMILY_AMIGA] = 2.0f;
        g_region_multiplier[UFT_FAMILY_ATARI_ST] = 1.5f;
        g_region_multiplier[UFT_FAMILY_SPECTRUM] = 2.0f;
        g_region_multiplier[UFT_FAMILY_AMSTRAD] = 1.5f;
        g_region_multiplier[UFT_FAMILY_APPLE_II] = 0.7f;
    } else if (strcmp(region, "us") == 0) {
        // USA: more Apple II, less Spectrum/Amstrad
        g_region_multiplier[UFT_FAMILY_APPLE_II] = 2.0f;
        g_region_multiplier[UFT_FAMILY_C64] = 1.3f;
        g_region_multiplier[UFT_FAMILY_SPECTRUM] = 0.3f;
        g_region_multiplier[UFT_FAMILY_AMSTRAD] = 0.5f;
    } else if (strcmp(region, "jp") == 0) {
        // Japan: more PC-98, MSX
        g_region_multiplier[UFT_FAMILY_PC98] = 3.0f;
        g_region_multiplier[UFT_FAMILY_MSX] = 2.0f;
        g_region_multiplier[UFT_FAMILY_AMIGA] = 0.3f;
        g_region_multiplier[UFT_FAMILY_C64] = 0.5f;
    }
}

// ============================================================================
// EVIDENCE INITIALIZATION
// ============================================================================

void uft_format_evidence_init(uft_format_evidence_t *evidence) {
    if (!evidence) return;
    memset(evidence, 0, sizeof(*evidence));
    evidence->family_hint = UFT_FAMILY_UNKNOWN;
}

// ============================================================================
// LIKELIHOOD COMPUTATION
// ============================================================================

static float compute_size_likelihood(size_t actual, size_t expected) {
    if (expected == 0) return 0.5f;  // Variable size format
    
    if (actual == expected) return 0.95f;
    
    float ratio = (float)actual / (float)expected;
    if (ratio > 0.95f && ratio < 1.05f) return 0.7f;  // ±5%
    if (ratio > 0.9f && ratio < 1.1f) return 0.4f;    // ±10%
    
    return 0.05f;  // Poor match
}

static float compute_magic_likelihood(
    const uint8_t *actual, size_t actual_size,
    const uint8_t *expected, size_t expected_len, size_t offset)
{
    if (!expected || expected_len == 0) return 0.5f;  // No magic defined
    if (!actual || actual_size < offset + expected_len) return 0.3f;
    
    if (memcmp(actual + offset, expected, expected_len) == 0) {
        return 0.99f;  // Perfect match
    }
    
    // Partial match check
    size_t matches = 0;
    for (size_t i = 0; i < expected_len; i++) {
        if (actual[offset + i] == expected[i]) matches++;
    }
    
    if (matches > expected_len / 2) return 0.5f;
    
    return 0.1f;  // Poor match
}

static float compute_geometry_likelihood(
    const uft_format_evidence_t *evidence,
    const format_definition_t *fmt)
{
    if (!evidence->geometry_known) return 0.5f;
    
    float likelihood = 1.0f;
    
    if (evidence->tracks == fmt->tracks) {
        likelihood *= 0.9f;
    } else if (evidence->tracks != 0) {
        likelihood *= 0.2f;
    }
    
    if (evidence->heads == fmt->heads) {
        likelihood *= 0.9f;
    } else if (evidence->heads != 0) {
        likelihood *= 0.3f;
    }
    
    if (evidence->sectors_per_track == fmt->sectors) {
        likelihood *= 0.85f;
    } else if (evidence->sectors_per_track != 0) {
        likelihood *= 0.2f;
    }
    
    return likelihood;
}

static float compute_extension_likelihood(
    const char *actual_ext,
    const char *expected_exts)
{
    if (!actual_ext || !expected_exts) return 0.5f;
    
    // Check if actual extension is in comma-separated list
    char ext_copy[256];
    strncpy(ext_copy, expected_exts, sizeof(ext_copy) - 1);
    ext_copy[sizeof(ext_copy) - 1] = '\0';
    
    char *token = strtok(ext_copy, ",");
    while (token) {
        if (strcasecmp(actual_ext, token) == 0) {
            return 0.9f;  // Extension match
        }
        token = strtok(NULL, ",");
    }
    
    return 0.3f;  // No match
}

// ============================================================================
// MAIN CLASSIFICATION
// ============================================================================

int uft_format_classify(
    const uft_format_evidence_t *evidence,
    uft_format_classification_t *result)
{
    if (!evidence || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    
    // ========================================================================
    // COMPUTE POSTERIORS
    // ========================================================================
    
    float posteriors[FORMAT_COUNT];
    float max_posterior = 0.0f;
    size_t max_idx = 0;
    
    float total = 0.0f;
    
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        const format_definition_t *fmt = &FORMAT_DB[i];
        
        // Prior (with regional adjustment)
        float prior = fmt->base_prior * g_region_multiplier[fmt->family];
        
        // Family hint adjustment
        if (evidence->family_hint != UFT_FAMILY_UNKNOWN) {
            if (fmt->family == evidence->family_hint) {
                prior *= 3.0f;
            } else {
                prior *= 0.5f;
            }
        }
        
        // Likelihood computation
        float likelihood = 1.0f;
        
        // File size
        if (evidence->file_size_known) {
            likelihood *= compute_size_likelihood(evidence->file_size, fmt->expected_size);
        }
        
        // Magic bytes
        if (evidence->boot_sector_available) {
            likelihood *= compute_magic_likelihood(
                evidence->boot_sector, evidence->boot_sector_size,
                fmt->magic, fmt->magic_len, fmt->magic_offset);
        }
        
        // Geometry
        likelihood *= compute_geometry_likelihood(evidence, fmt);
        
        // Extension
        if (evidence->file_extension) {
            likelihood *= compute_extension_likelihood(evidence->file_extension, fmt->extensions);
        }
        
        // Posterior (unnormalized)
        posteriors[i] = likelihood * prior;
        total += posteriors[i];
        
        if (posteriors[i] > max_posterior) {
            max_posterior = posteriors[i];
            max_idx = i;
        }
    }
    
    // ========================================================================
    // NORMALIZE AND RANK
    // ========================================================================
    
    if (total > 0.0f) {
        for (size_t i = 0; i < FORMAT_COUNT; i++) {
            posteriors[i] /= total;
        }
    }
    
    // Find top candidates
    size_t sorted_idx[FORMAT_COUNT];
    for (size_t i = 0; i < FORMAT_COUNT; i++) sorted_idx[i] = i;
    
    // Simple selection sort for top 10
    for (size_t i = 0; i < 10 && i < FORMAT_COUNT; i++) {
        size_t max_j = i;
        for (size_t j = i + 1; j < FORMAT_COUNT; j++) {
            if (posteriors[sorted_idx[j]] > posteriors[sorted_idx[max_j]]) {
                max_j = j;
            }
        }
        size_t tmp = sorted_idx[i];
        sorted_idx[i] = sorted_idx[max_j];
        sorted_idx[max_j] = tmp;
    }
    
    // Fill result
    size_t count = (FORMAT_COUNT < 10) ? FORMAT_COUNT : 10;
    for (size_t i = 0; i < count; i++) {
        size_t idx = sorted_idx[i];
        const format_definition_t *fmt = &FORMAT_DB[idx];
        
        result->candidates[i].format_id = fmt->id;
        result->candidates[i].format_name = fmt->name;
        result->candidates[i].family = fmt->family;
        result->candidates[i].posterior = posteriors[idx];
        result->candidates[i].prior = fmt->base_prior * g_region_multiplier[fmt->family];
        result->candidates[i].likelihood = (result->candidates[i].prior > 0.0f) ?
            posteriors[idx] / result->candidates[i].prior : 0.0f;
        result->candidates[i].expected_tracks = fmt->tracks;
        result->candidates[i].expected_heads = fmt->heads;
        result->candidates[i].expected_sectors = fmt->sectors;
        result->candidates[i].expected_sector_size = fmt->sector_size;
    }
    result->candidate_count = count;
    
    // ========================================================================
    // CONFIDENCE METRICS
    // ========================================================================
    
    result->confidence = posteriors[sorted_idx[0]];
    result->margin = (count > 1) ? 
        posteriors[sorted_idx[0]] - posteriors[sorted_idx[1]] : 1.0f;
    result->is_uncertain = (result->margin < 0.1f);
    
    result->most_likely_family = FORMAT_DB[sorted_idx[0]].family;
    result->family_posterior = result->confidence;
    
    // Decision reason
    if (result->confidence > 0.8f) {
        result->decision_reason = "High confidence match";
    } else if (result->confidence > 0.5f) {
        result->decision_reason = "Moderate confidence";
    } else if (result->is_uncertain) {
        result->decision_reason = "Uncertain - multiple formats possible";
    } else {
        result->decision_reason = "Low confidence - unusual format";
    }
    
    return 0;
}

// ============================================================================
// QUICK CLASSIFICATION BY SIZE
// ============================================================================

int uft_format_classify_by_size(
    size_t file_size,
    uft_format_classification_t *result)
{
    uft_format_evidence_t evidence;
    uft_format_evidence_init(&evidence);
    
    evidence.file_size = file_size;
    evidence.file_size_known = true;
    
    return uft_format_classify(&evidence, result);
}

// ============================================================================
// FAMILY PRIOR GETTER
// ============================================================================

float uft_format_get_family_prior(uft_format_family_t family) {
    if (family >= UFT_FAMILY_COUNT) return 0.0f;
    
    // Sum base priors for this family
    float total = 0.0f;
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (FORMAT_DB[i].family == family) {
            total += FORMAT_DB[i].base_prior;
        }
    }
    return total * g_region_multiplier[family];
}

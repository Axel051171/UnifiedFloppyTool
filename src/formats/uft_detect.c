/**
 * @file uft_detect.c
 * @brief Bayesian Format Detection Implementation
 * 
 * ROADMAP F2.4 - Priority P1
 */

#include "uft/formats/uft_detect.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

// ============================================================================
// Format Database
// ============================================================================

typedef struct {
    uint32_t format_id;
    const char* name;
    const char* extension;
    size_t sizes[4];          // Known sizes (0 = end)
    uint32_t magic;
    int magic_offset;
    int magic_len;
    double prior;             // Prior probability
    bool is_flux;
} format_info_t;

static const format_info_t FORMAT_DB[] = {
    // Commodore
    {UFT_FMT_D64, "D64", ".d64", {174848, 175531, 196608, 197376}, 0, -1, 0, 0.15, false},
    {UFT_FMT_G64, "G64", ".g64", {0}, 0x34365447, 0, 4, 0.05, false},
    {UFT_FMT_D71, "D71", ".d71", {349696, 351062}, 0, -1, 0, 0.03, false},
    {UFT_FMT_D81, "D81", ".d81", {819200, 822400}, 0, -1, 0, 0.03, false},
    
    // Amiga
    {UFT_FMT_ADF, "ADF", ".adf", {901120, 1802240}, 0x534F44, 0, 3, 0.10, false},  // "DOS"
    
    // Apple
    {UFT_FMT_DSK_APPLE, "DSK", ".dsk", {143360}, 0, -1, 0, 0.08, false},
    {UFT_FMT_WOZ, "WOZ", ".woz", {0}, 0x315A4F57, 0, 4, 0.05, true},
    
    // PC
    {UFT_FMT_IMG_PC, "IMG", ".img", {163840, 184320, 327680, 368640}, 0, -1, 0, 0.12, false},
    
    // Atari
    {UFT_FMT_ATR, "ATR", ".atr", {92176, 133136, 184336}, 0x0296, 0, 2, 0.05, false},
    
    // Flux
    {UFT_FMT_SCP, "SCP", ".scp", {0}, 0x50435343, 0, 4, 0.10, true},
    {UFT_FMT_HFE, "HFE", ".hfe", {0}, 0x45464350, 0, 4, 0.08, true},
    {UFT_FMT_IPF, "IPF", ".ipf", {0}, 0x53504143, 0, 4, 0.06, true},
    
    {0, NULL, NULL, {0}, 0, -1, 0, 0, false}
};

static const int FORMAT_COUNT = sizeof(FORMAT_DB) / sizeof(FORMAT_DB[0]) - 1;

// ============================================================================
// Options
// ============================================================================

void uft_detect_options_default(uft_detect_options_t* opts) {
    if (!opts) return;
    
    opts->use_extension = true;
    opts->use_magic = true;
    opts->use_size = true;
    opts->use_content = true;
    opts->min_confidence = 50;
    opts->max_candidates = 5;
}

// ============================================================================
// Helpers
// ============================================================================

static const char* get_extension(const char* filename) {
    if (!filename) return NULL;
    const char* dot = strrchr(filename, '.');
    return dot ? dot : NULL;
}

static double size_likelihood(size_t actual, const size_t* expected) {
    if (!expected) return 0.1;
    
    for (int i = 0; expected[i] != 0 && i < 4; i++) {
        if (actual == expected[i]) return 0.95;
        
        // Close match
        double diff = (double)abs((int)actual - (int)expected[i]) / expected[i];
        if (diff < 0.01) return 0.8;
        if (diff < 0.05) return 0.5;
    }
    
    return 0.1;  // No match
}

static double magic_likelihood(const uint8_t* data, size_t size,
                               uint32_t magic, int offset, int len) {
    if (offset < 0 || len <= 0) return 0.5;  // No magic defined
    if ((size_t)(offset + len) > size) return 0.1;
    
    uint32_t found = 0;
    for (int i = 0; i < len && i < 4; i++) {
        found |= ((uint32_t)data[offset + i]) << (8 * i);
    }
    
    if (found == magic) return 0.99;
    return 0.01;
}

static double extension_likelihood(const char* ext, const char* expected) {
    if (!ext || !expected) return 0.5;
    if (strcasecmp(ext, expected) == 0) return 0.9;
    return 0.2;
}

// ============================================================================
// Detection
// ============================================================================

int uft_detect_format(const uint8_t* data, size_t size,
                      const char* filename,
                      const uft_detect_options_t* opts,
                      uft_detect_result_t* result) {
    if (!data || !result) return -1;
    
    uft_detect_options_t local_opts;
    if (!opts) {
        uft_detect_options_default(&local_opts);
        opts = &local_opts;
    }
    
    memset(result, 0, sizeof(*result));
    
    // Allocate candidates
    result->candidates = calloc(FORMAT_COUNT, sizeof(uft_detect_candidate_t));
    if (!result->candidates) return -1;
    
    const char* ext = get_extension(filename);
    
    // Calculate posteriors
    double posteriors[FORMAT_COUNT];
    double evidence = 0;
    
    for (int i = 0; i < FORMAT_COUNT; i++) {
        const format_info_t* fmt = &FORMAT_DB[i];
        
        double likelihood = 1.0;
        
        // Size likelihood
        if (opts->use_size) {
            likelihood *= size_likelihood(size, fmt->sizes);
        }
        
        // Magic likelihood
        if (opts->use_magic && fmt->magic_len > 0) {
            likelihood *= magic_likelihood(data, size, fmt->magic,
                                           fmt->magic_offset, fmt->magic_len);
        }
        
        // Extension likelihood
        if (opts->use_extension && ext) {
            likelihood *= extension_likelihood(ext, fmt->extension);
        }
        
        // Posterior ∝ likelihood × prior
        posteriors[i] = likelihood * fmt->prior;
        evidence += posteriors[i];
    }
    
    // Normalize
    if (evidence > 0) {
        for (int i = 0; i < FORMAT_COUNT; i++) {
            posteriors[i] /= evidence;
        }
    }
    
    // Build result
    int count = 0;
    double best_prob = 0;
    double second_prob = 0;
    
    for (int i = 0; i < FORMAT_COUNT && count < opts->max_candidates; i++) {
        if (posteriors[i] < opts->min_confidence / 100.0) continue;
        
        uft_detect_candidate_t* c = &result->candidates[count];
        c->format_id = FORMAT_DB[i].format_id;
        c->probability = posteriors[i];
        c->confidence = (int)(posteriors[i] * 100);
        strncpy(c->format_name, FORMAT_DB[i].name, sizeof(c->format_name) - 1);
        
        // Generate explanation
        snprintf(c->explanation, sizeof(c->explanation),
                 "%s detected (%.1f%% confidence)%s%s",
                 FORMAT_DB[i].name,
                 posteriors[i] * 100,
                 FORMAT_DB[i].is_flux ? " [Flux]" : "",
                 (FORMAT_DB[i].sizes[0] == size) ? " [Size match]" : "");
        
        if (posteriors[i] > best_prob) {
            second_prob = best_prob;
            best_prob = posteriors[i];
            result->best_index = count;
        } else if (posteriors[i] > second_prob) {
            second_prob = posteriors[i];
        }
        
        count++;
    }
    
    result->candidate_count = count;
    
    // Check ambiguity
    if (second_prob > 0 && best_prob > 0) {
        result->ambiguity_ratio = second_prob / best_prob;
        result->is_ambiguous = (result->ambiguity_ratio > 0.7);
        
        if (result->is_ambiguous) {
            snprintf(result->warning, sizeof(result->warning),
                     "Ambiguous detection: multiple formats match");
        }
    }
    
    // Sort by probability (simple bubble sort)
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (result->candidates[j].probability < result->candidates[j+1].probability) {
                uft_detect_candidate_t tmp = result->candidates[j];
                result->candidates[j] = result->candidates[j+1];
                result->candidates[j+1] = tmp;
            }
        }
    }
    result->best_index = 0;
    
    return 0;
}

// ============================================================================
// Query
// ============================================================================

const uft_detect_candidate_t* uft_detect_best(const uft_detect_result_t* result) {
    if (!result || result->candidate_count == 0) return NULL;
    return &result->candidates[result->best_index];
}

int uft_detect_top_n(const uft_detect_result_t* result,
                     uft_detect_candidate_t* top, int n) {
    if (!result || !top || n <= 0) return 0;
    
    int count = result->candidate_count < n ? result->candidate_count : n;
    memcpy(top, result->candidates, count * sizeof(uft_detect_candidate_t));
    
    return count;
}

void uft_detect_result_free(uft_detect_result_t* result) {
    if (!result) return;
    free(result->candidates);
    memset(result, 0, sizeof(*result));
}

const char* uft_detect_explain(const uft_detect_candidate_t* candidate) {
    return candidate ? candidate->explanation : "";
}

// ============================================================================
// Format Info
// ============================================================================

const char* uft_format_name(uint32_t format_id) {
    for (int i = 0; i < FORMAT_COUNT; i++) {
        if (FORMAT_DB[i].format_id == format_id) {
            return FORMAT_DB[i].name;
        }
    }
    return "Unknown";
}

bool uft_format_is_flux(uint32_t format_id) {
    for (int i = 0; i < FORMAT_COUNT; i++) {
        if (FORMAT_DB[i].format_id == format_id) {
            return FORMAT_DB[i].is_flux;
        }
    }
    return false;
}

int uft_format_sizes(uint32_t format_id, size_t* sizes, int max_sizes) {
    for (int i = 0; i < FORMAT_COUNT; i++) {
        if (FORMAT_DB[i].format_id == format_id) {
            int count = 0;
            for (int j = 0; j < 4 && j < max_sizes; j++) {
                if (FORMAT_DB[i].sizes[j] == 0) break;
                sizes[count++] = FORMAT_DB[i].sizes[j];
            }
            return count;
        }
    }
    return 0;
}

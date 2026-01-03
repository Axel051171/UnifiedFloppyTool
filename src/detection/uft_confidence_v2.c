/**
 * @file uft_confidence_v2.c
 * @brief Format Detection Confidence Scoring - GOD MODE OPTIMIZED
 * @version 5.3.1-GOD
 *
 * CHANGES from v1:
 * - FIX: strcpy → snprintf (Buffer Overflow Prevention)
 * - FIX: Bounds checking on all string operations
 * - NEW: SIMD-optimized byte comparison
 * - NEW: Confidence fusion from multiple sources
 * - NEW: GUI-compatible parameter export
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

#define UFT_MAX_FORMAT_NAME     64
#define UFT_MAX_EXTENSION       16
#define UFT_MAX_CANDIDATES      32
#define UFT_CONFIDENCE_MAGIC    100
#define UFT_CONFIDENCE_SIZE     50
#define UFT_CONFIDENCE_HEADER   80
#define UFT_CONFIDENCE_LAYOUT   70

/*============================================================================
 * TYPES
 *============================================================================*/

typedef enum {
    UFT_EVIDENCE_NONE       = 0,
    UFT_EVIDENCE_MAGIC      = (1 << 0),
    UFT_EVIDENCE_EXTENSION  = (1 << 1),
    UFT_EVIDENCE_SIZE       = (1 << 2),
    UFT_EVIDENCE_HEADER     = (1 << 3),
    UFT_EVIDENCE_LAYOUT     = (1 << 4),
    UFT_EVIDENCE_CHECKSUM   = (1 << 5),
    UFT_EVIDENCE_CONTENT    = (1 << 6)
} uft_evidence_type_t;

typedef struct {
    char name[UFT_MAX_FORMAT_NAME];
    char extension[UFT_MAX_EXTENSION];
    float confidence;           // 0.0 - 1.0
    uint32_t evidence_mask;     // Bitmap of evidence types
    float scores[8];            // Individual scores per evidence type
} uft_format_candidate_t;

typedef struct {
    uft_format_candidate_t candidates[UFT_MAX_CANDIDATES];
    int count;
    int best_index;
    float best_confidence;
    bool is_ambiguous;          // Multiple high-confidence matches
} uft_detection_result_t;

/*============================================================================
 * SIMD-OPTIMIZED BYTE COMPARISON
 *============================================================================*/

#ifdef __SSE2__
/**
 * @brief SIMD-optimized memory comparison (16 bytes at a time)
 * @return Number of matching bytes
 */
static inline int simd_memcmp_count(const uint8_t* a, const uint8_t* b, size_t len) {
    int matches = 0;
    size_t i = 0;
    
    // Process 16 bytes at a time with SSE2
    for (; i + 16 <= len; i += 16) {
        __m128i va = _mm_loadu_si128((const __m128i*)(a + i));
        __m128i vb = _mm_loadu_si128((const __m128i*)(b + i));
        __m128i cmp = _mm_cmpeq_epi8(va, vb);
        int mask = _mm_movemask_epi8(cmp);
        matches += __builtin_popcount(mask);
    }
    
    // Remainder
    for (; i < len; i++) {
        if (a[i] == b[i]) matches++;
    }
    
    return matches;
}
#else
static inline int simd_memcmp_count(const uint8_t* a, const uint8_t* b, size_t len) {
    int matches = 0;
    for (size_t i = 0; i < len; i++) {
        if (a[i] == b[i]) matches++;
    }
    return matches;
}
#endif

/*============================================================================
 * CONFIDENCE SCORING - SAFE STRING OPERATIONS
 *============================================================================*/

/**
 * @brief Build format description string - SAFE VERSION
 * @note Uses snprintf instead of strcpy to prevent buffer overflow
 */
static int build_format_description(char* buf, size_t buf_size,
                                    const uft_format_candidate_t* candidates,
                                    int count) {
    if (!buf || buf_size == 0 || !candidates || count <= 0) {
        return -1;
    }
    
    size_t pos = 0;
    
    // Header
    int written = snprintf(buf + pos, buf_size - pos, "Detected formats (%d):\n", count);
    if (written < 0 || (size_t)written >= buf_size - pos) {
        return -1;  // Buffer too small
    }
    pos += written;
    
    // Each candidate - SAFE string copy
    for (int i = 0; i < count && pos < buf_size - 1; i++) {
        const char* name = candidates[i].name;
        size_t name_len = strlen(name);
        
        // Check available space: "  - <name> (<confidence>%)\n"
        size_t needed = 6 + name_len + 10;  // "  - " + name + " (100.0%)\n"
        
        if (pos + needed >= buf_size) {
            // Truncate gracefully
            snprintf(buf + pos, buf_size - pos, "  ... (%d more)\n", count - i);
            break;
        }
        
        // SAFE: Use snprintf instead of strcpy
        written = snprintf(buf + pos, buf_size - pos, 
                          "  - %s (%.1f%%)\n", 
                          name, 
                          candidates[i].confidence * 100.0f);
        if (written > 0 && (size_t)written < buf_size - pos) {
            pos += written;
        }
    }
    
    return (int)pos;
}

/*============================================================================
 * CONFIDENCE FUSION - BAYESIAN COMBINATION
 *============================================================================*/

/**
 * @brief Combine multiple confidence scores using Bayesian fusion
 * @param scores Array of individual confidence scores (0.0 - 1.0)
 * @param weights Array of weights for each score
 * @param count Number of scores
 * @return Combined confidence (0.0 - 1.0)
 */
static float bayesian_confidence_fusion(const float* scores, const float* weights, int count) {
    if (!scores || !weights || count <= 0) {
        return 0.0f;
    }
    
    // Normalize weights
    float weight_sum = 0.0f;
    for (int i = 0; i < count; i++) {
        weight_sum += weights[i];
    }
    
    if (weight_sum < 0.0001f) {
        return 0.0f;  // Avoid division by zero
    }
    
    // Weighted geometric mean (better for confidence fusion)
    float log_sum = 0.0f;
    for (int i = 0; i < count; i++) {
        float s = scores[i];
        if (s < 0.001f) s = 0.001f;  // Avoid log(0)
        if (s > 0.999f) s = 0.999f;  // Avoid log(1) issues
        
        float normalized_weight = weights[i] / weight_sum;
        log_sum += normalized_weight * logf(s);
    }
    
    return expf(log_sum);
}

/*============================================================================
 * MAIN DETECTION FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize detection result
 */
void uft_detection_result_init(uft_detection_result_t* result) {
    if (!result) return;
    
    memset(result, 0, sizeof(*result));
    result->best_index = -1;
}

/**
 * @brief Add a candidate to detection result
 */
int uft_detection_add_candidate(uft_detection_result_t* result,
                                const char* name,
                                const char* extension,
                                float confidence,
                                uint32_t evidence_mask) {
    if (!result || !name || result->count >= UFT_MAX_CANDIDATES) {
        return -1;
    }
    
    uft_format_candidate_t* cand = &result->candidates[result->count];
    
    // SAFE: Use snprintf for string copy
    snprintf(cand->name, sizeof(cand->name), "%s", name);
    if (extension) {
        snprintf(cand->extension, sizeof(cand->extension), "%s", extension);
    }
    
    cand->confidence = confidence;
    cand->evidence_mask = evidence_mask;
    
    // Update best
    if (confidence > result->best_confidence) {
        result->best_confidence = confidence;
        result->best_index = result->count;
    }
    
    result->count++;
    
    // Check for ambiguity (multiple candidates > 0.8)
    int high_count = 0;
    for (int i = 0; i < result->count; i++) {
        if (result->candidates[i].confidence > 0.8f) {
            high_count++;
        }
    }
    result->is_ambiguous = (high_count > 1);
    
    return result->count - 1;
}

/**
 * @brief Calculate confidence from magic bytes
 */
float uft_confidence_from_magic(const uint8_t* data, size_t data_size,
                                const uint8_t* magic, size_t magic_size,
                                size_t offset) {
    if (!data || !magic || data_size < offset + magic_size) {
        return 0.0f;
    }
    
    int matches = simd_memcmp_count(data + offset, magic, magic_size);
    return (float)matches / (float)magic_size;
}

/**
 * @brief Calculate confidence from file size
 */
float uft_confidence_from_size(size_t actual_size, 
                               size_t expected_size,
                               size_t tolerance) {
    if (expected_size == 0) {
        return 0.0f;
    }
    
    if (actual_size == expected_size) {
        return 1.0f;
    }
    
    size_t diff;
    if (actual_size > expected_size) {
        diff = actual_size - expected_size;
    } else {
        diff = expected_size - actual_size;
    }
    
    if (diff <= tolerance) {
        return 1.0f - ((float)diff / (float)(tolerance + 1));
    }
    
    return 0.0f;
}

/**
 * @brief Get final confidence with fusion
 */
float uft_detection_get_final_confidence(const uft_detection_result_t* result) {
    if (!result || result->best_index < 0) {
        return 0.0f;
    }
    
    const uft_format_candidate_t* best = &result->candidates[result->best_index];
    
    // Evidence weights
    static const float weights[] = {
        1.0f,   // MAGIC
        0.3f,   // EXTENSION
        0.5f,   // SIZE
        0.8f,   // HEADER
        0.7f,   // LAYOUT
        0.9f,   // CHECKSUM
        0.6f    // CONTENT
    };
    
    float scores[7];
    int score_count = 0;
    float active_weights[7];
    
    for (int i = 0; i < 7; i++) {
        if (best->evidence_mask & (1 << i)) {
            scores[score_count] = best->scores[i];
            active_weights[score_count] = weights[i];
            score_count++;
        }
    }
    
    if (score_count == 0) {
        return best->confidence;
    }
    
    return bayesian_confidence_fusion(scores, active_weights, score_count);
}

/*============================================================================
 * GUI PARAMETER EXPORT
 *============================================================================*/

typedef struct {
    float magic_weight;         // 0.0 - 2.0, default 1.0
    float size_tolerance_pct;   // 0.0 - 10.0%, default 1.0
    float ambiguity_threshold;  // 0.5 - 0.95, default 0.8
    bool enable_content_scan;   // default true
    bool enable_checksum;       // default true
} uft_detection_params_gui_t;

/**
 * @brief Get default GUI parameters
 */
void uft_detection_params_get_defaults(uft_detection_params_gui_t* params) {
    if (!params) return;
    
    params->magic_weight = 1.0f;
    params->size_tolerance_pct = 1.0f;
    params->ambiguity_threshold = 0.8f;
    params->enable_content_scan = true;
    params->enable_checksum = true;
}

/**
 * @brief Validate GUI parameters
 */
bool uft_detection_params_validate(const uft_detection_params_gui_t* params) {
    if (!params) return false;
    
    if (params->magic_weight < 0.0f || params->magic_weight > 2.0f) return false;
    if (params->size_tolerance_pct < 0.0f || params->size_tolerance_pct > 10.0f) return false;
    if (params->ambiguity_threshold < 0.5f || params->ambiguity_threshold > 0.95f) return false;
    
    return true;
}

#ifdef UFT_CONFIDENCE_TEST
/*============================================================================
 * UNIT TEST
 *============================================================================*/

#include <assert.h>

int main(void) {
    printf("=== uft_confidence_v2 Unit Tests ===\n");
    
    // Test 1: Safe string operations
    {
        char buf[256];
        uft_format_candidate_t candidates[3] = {
            { .name = "D64", .confidence = 0.95f },
            { .name = "G64", .confidence = 0.82f },
            { .name = "TAP", .confidence = 0.45f }
        };
        
        int len = build_format_description(buf, sizeof(buf), candidates, 3);
        assert(len > 0);
        assert(strlen(buf) < sizeof(buf));
        printf("✓ Safe string operations: %d bytes\n", len);
    }
    
    // Test 2: Buffer overflow prevention
    {
        char small_buf[32];
        uft_format_candidate_t candidates[10] = {0};
        for (int i = 0; i < 10; i++) {
            snprintf(candidates[i].name, sizeof(candidates[i].name), "Format%d", i);
            candidates[i].confidence = 0.5f + i * 0.05f;
        }
        
        int len = build_format_description(small_buf, sizeof(small_buf), candidates, 10);
        assert(len > 0 || len == -1);  // Either truncated or error
        assert(strlen(small_buf) < sizeof(small_buf));
        printf("✓ Buffer overflow prevention\n");
    }
    
    // Test 3: Bayesian fusion
    {
        float scores[] = { 0.9f, 0.8f, 0.7f };
        float weights[] = { 1.0f, 0.5f, 0.3f };
        float fused = bayesian_confidence_fusion(scores, weights, 3);
        assert(fused > 0.7f && fused < 0.95f);
        printf("✓ Bayesian fusion: %.3f\n", fused);
    }
    
    // Test 4: Magic byte comparison
    {
        uint8_t data[] = { 0x00, 0x00, 0x12, 0x01, 0x41, 0x00 };
        uint8_t magic[] = { 0x12, 0x01, 0x41 };
        float conf = uft_confidence_from_magic(data, sizeof(data), magic, sizeof(magic), 2);
        assert(conf == 1.0f);
        printf("✓ Magic byte detection: %.1f%%\n", conf * 100.0f);
    }
    
    // Test 5: Size confidence
    {
        float conf1 = uft_confidence_from_size(174848, 174848, 0);
        float conf2 = uft_confidence_from_size(174849, 174848, 1);
        float conf3 = uft_confidence_from_size(175000, 174848, 100);
        assert(conf1 == 1.0f);
        assert(conf2 > 0.4f && conf2 < 0.6f);
        assert(conf3 < conf2);
        printf("✓ Size confidence: exact=%.0f%%, +1=%.0f%%, +152=%.0f%%\n",
               conf1*100, conf2*100, conf3*100);
    }
    
    // Test 6: GUI parameter validation
    {
        uft_detection_params_gui_t params;
        uft_detection_params_get_defaults(&params);
        assert(uft_detection_params_validate(&params));
        
        params.magic_weight = 3.0f;  // Invalid
        assert(!uft_detection_params_validate(&params));
        printf("✓ GUI parameter validation\n");
    }
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}
#endif

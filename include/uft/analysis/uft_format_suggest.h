/**
 * @file uft_format_suggest.h
 * @brief Intelligent Format Suggestion Engine
 *
 * Given an input disk image, recommends the best output formats ranked by
 * suitability.  Takes into account:
 *   - Whether the input contains flux, bitstream, or sector data
 *   - Presence of copy protection and weak bits
 *   - Archival vs. emulator use-case
 *
 * @author UFT Project
 * @date 2026
 */

#ifndef UFT_FORMAT_SUGGEST_H
#define UFT_FORMAT_SUGGEST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Suggestion Entry
 * ============================================================================ */

typedef struct {
    int   format_id;              /**< uft_format_t enum value */
    char  format_name[32];        /**< Human-readable name */
    float suitability;            /**< 0.0 - 1.0 overall score */
    bool  preserves_flux;
    bool  preserves_timing;
    bool  preserves_protection;
    bool  preserves_weak_bits;
    bool  emulator_compatible;
    char  reason[128];            /**< Why recommended / not */
    char  warning[128];           /**< What gets lost on conversion */
} uft_format_suggestion_t;

#define UFT_SUGGEST_MAX 10

/* ============================================================================
 * Result Set
 * ============================================================================ */

typedef struct {
    uft_format_suggestion_t suggestions[UFT_SUGGEST_MAX];
    int  count;                   /**< Number of valid entries */
    int  recommended_index;       /**< Best overall choice */
    int  archival_index;          /**< Best for long-term archive */
    int  emulator_index;          /**< Best for emulator use */
    char summary[256];            /**< Human-readable summary */
} uft_suggest_result_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Suggest output formats for a given disk image
 *
 * @param input_path      Path to the source image / flux file
 * @param has_protection  true if copy protection was detected
 * @param has_weak_bits   true if weak / fuzzy bits are present
 * @param for_archival    true to boost archival formats, false for emulator use
 * @param result          Output: suggestions sorted by suitability (descending)
 * @return 0 on success, negative error code on failure
 */
int uft_format_suggest(const char *input_path,
                       bool has_protection, bool has_weak_bits,
                       bool for_archival,
                       uft_suggest_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_SUGGEST_H */

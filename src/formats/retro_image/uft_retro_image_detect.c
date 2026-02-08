/**
 * @file uft_retro_image_detect.c
 * @brief Retro Image Format Detection Implementation
 *
 * Multi-factor detection engine for 400 retro image formats.
 * Combines magic byte matching, file extension, and file size
 * for reliable format identification.
 *
 * Integrates with UFT's forensic carving pipeline for recovery
 * of retro image files from disk images.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "uft/formats/uft_retro_image_detect.h"
#include "uft/formats/uft_retro_image_sigs.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static int score_entry(const ri_sig_entry_t *s,
                       const uint8_t *data, size_t data_len,
                       uint32_t file_size, const char *ext)
{
    int score = 0;

    /* Magic bytes: strongest signal, weighted by length */
    if (s->magic && s->magic_len > 0 && (size_t)s->magic_len <= data_len) {
        if (memcmp(data, s->magic, s->magic_len) == 0)
            score += 15 + s->magic_len * 8;  /* 2B=31, 4B=47, 8B=79 */
    }

    /* Extension match */
    if (ext && ext[0] != '\0' && strcmp(s->ext, ext) == 0)
        score += 25;

    /* File size: exact match on fixed-size format is very strong */
    if (file_size > 0) {
        if (s->fixed_size && file_size == s->min_size)
            score += 30;
        else if (!s->fixed_size && file_size >= s->min_size && file_size <= s->max_size)
            score += 12;
        /* Penalize if outside known range */
        else if (file_size < s->min_size || (s->max_size > 0 && file_size > s->max_size * 2))
            score -= 10;
    }

    return score;
}

static void fill_result(ri_detect_result_t *r, const ri_sig_entry_t *s, int confidence)
{
    r->ext = s->ext;
    r->name = s->name;
    r->platform_id = (int)s->platform;
    r->platform_name = (s->platform < RI_PLAT_COUNT) ? ri_platform_names[s->platform] : "Unknown";
    r->confidence = (confidence > 100) ? 100 : confidence;
    r->min_size = s->min_size;
    r->max_size = s->max_size;
    r->fixed_size = s->fixed_size;
}

/* ============================================================================
 * Core Detection
 * ============================================================================ */

int rid_detect(const uint8_t *data, size_t data_len,
               uint32_t file_size, const char *ext,
               ri_detect_results_t *results)
{
    if (!results) return 0;
    memset(results, 0, sizeof(*results));
    results->best_idx = -1;

    if (!data || data_len < 2) return 0;

    /* Score all entries */
    typedef struct { uint16_t idx; int score; } scored_t;
    scored_t scored[RI_SIG_COUNT];
    uint16_t scored_count = 0;

    for (uint16_t i = 0; i < RI_SIG_COUNT; i++) {
        int s = score_entry(&ri_signatures[i], data, data_len, file_size, ext);
        if (s >= 20) {  /* minimum threshold */
            scored[scored_count].idx = i;
            scored[scored_count].score = s;
            scored_count++;
        }
    }

    /* Sort by score descending (simple insertion sort, small N) */
    for (uint16_t i = 1; i < scored_count; i++) {
        scored_t tmp = scored[i];
        int j = (int)i - 1;
        while (j >= 0 && scored[j].score < tmp.score) {
            scored[j + 1] = scored[j];
            j--;
        }
        scored[j + 1] = tmp;
    }

    /* Fill results (top N) */
    uint16_t n = (scored_count < RI_DETECT_MAX_CANDIDATES) ? scored_count : RI_DETECT_MAX_CANDIDATES;
    for (uint16_t i = 0; i < n; i++) {
        fill_result(&results->candidates[i],
                    &ri_signatures[scored[i].idx],
                    scored[i].score);
    }
    results->count = n;
    if (n > 0) results->best_idx = 0;

    return (int)n;
}

int rid_detect_quick(const uint8_t *data, size_t data_len,
                     uint32_t file_size, const char *ext,
                     const char **out_name, const char **out_platform)
{
    if (!data || data_len < 2) return 0;

    const ri_sig_entry_t *best = NULL;
    int best_score = 0;

    for (uint16_t i = 0; i < RI_SIG_COUNT; i++) {
        int s = score_entry(&ri_signatures[i], data, data_len, file_size, ext);
        if (s > best_score) {
            best_score = s;
            best = &ri_signatures[i];
        }
    }

    if (best_score < 20 || !best) return 0;

    if (out_name) *out_name = best->name;
    if (out_platform) *out_platform = (best->platform < RI_PLAT_COUNT)
                                      ? ri_platform_names[best->platform] : "Unknown";

    return (best_score > 100) ? 100 : best_score;
}

/* ============================================================================
 * Platform Listing
 * ============================================================================ */

uint16_t rid_list_platform(int platform_id,
                           ri_detect_result_t *results,
                           uint16_t max_results)
{
    uint16_t count = 0;
    for (uint16_t i = 0; i < RI_SIG_COUNT && count < max_results; i++) {
        if ((int)ri_signatures[i].platform == platform_id) {
            ri_detect_result_t *r = &results[count];
            r->ext = ri_signatures[i].ext;
            r->name = ri_signatures[i].name;
            r->platform_id = platform_id;
            r->platform_name = (platform_id >= 0 && platform_id < (int)RI_PLAT_COUNT)
                               ? ri_platform_names[platform_id] : "Unknown";
            r->confidence = 0;
            r->min_size = ri_signatures[i].min_size;
            r->max_size = ri_signatures[i].max_size;
            r->fixed_size = ri_signatures[i].fixed_size;
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void rid_get_stats(ri_db_stats_t *stats)
{
    if (!stats) return;
    memset(stats, 0, sizeof(*stats));

    stats->total_formats = RI_SIG_COUNT;
    uint16_t plat_seen[16] = {0};

    for (uint16_t i = 0; i < RI_SIG_COUNT; i++) {
        const ri_sig_entry_t *s = &ri_signatures[i];
        if (s->magic_len >= 2) stats->with_magic++;
        if (s->magic_len >= 4) stats->strong_magic++;
        if (s->fixed_size)     stats->fixed_size++;
        if (s->platform < 16) {
            stats->per_platform[s->platform]++;
            plat_seen[s->platform] = 1;
        }
    }

    for (int i = 0; i < 16; i++)
        stats->platforms += plat_seen[i];
}

const char *rid_platform_name(int platform_id)
{
    if (platform_id >= 0 && platform_id < (int)RI_PLAT_COUNT)
        return ri_platform_names[platform_id];
    return "Unknown";
}

/* ============================================================================
 * Debug Output
 * ============================================================================ */

void rid_print_results(const ri_detect_results_t *results)
{
    if (!results) return;
    fprintf(stderr, "[retro-image] %u candidate(s), best=%d\n",
            results->count, results->best_idx);
    for (uint16_t i = 0; i < results->count; i++) {
        const ri_detect_result_t *r = &results->candidates[i];
        fprintf(stderr, "  [%u] .%-6s %-30s %-15s conf=%d%%  size=%u-%u%s\n",
                i, r->ext, r->name, r->platform_name, r->confidence,
                r->min_size, r->max_size,
                r->fixed_size ? " (fixed)" : "");
    }
}

/* ============================================================================
 * Forensic Carving Scanner
 * ============================================================================ */

uint32_t rid_carve_scan(const uint8_t *data, size_t data_len,
                        rid_carve_callback_t callback, void *user_data)
{
    if (!data || data_len < 4 || !callback) return 0;

    uint32_t found = 0;

    /* Build quick-match table: group signatures by first 2 bytes */
    /* For efficiency, only scan signatures with ≥3 byte magic */
    for (size_t offset = 0; offset + 16 <= data_len; offset++) {
        const uint8_t *p = data + offset;

        for (uint16_t i = 0; i < RI_SIG_COUNT; i++) {
            const ri_sig_entry_t *s = &ri_signatures[i];
            if (!s->magic || s->magic_len < 3)
                continue;
            if ((size_t)s->magic_len > data_len - offset)
                continue;

            /* Quick first-byte pre-check */
            if (p[0] != s->magic[0] || p[1] != s->magic[1])
                continue;

            /* Full magic match */
            if (memcmp(p, s->magic, s->magic_len) == 0) {
                callback(offset, s, user_data);
                found++;
                break;  /* one match per offset */
            }
        }
    }

    return found;
}

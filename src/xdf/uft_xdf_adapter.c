/**
 * @file uft_xdf_adapter.c
 * @brief XDF Format Adapter Implementation
 * @version 1.0.0
 * 
 * Central adapter registry connecting format parsers to XDF API.
 */

#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/core/uft_error_codes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Registry
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define MAX_ADAPTERS 64

static const uft_format_adapter_t *g_adapters[MAX_ADAPTERS];
static size_t g_adapter_count = 0;

uft_error_t uft_adapter_register(const uft_format_adapter_t *adapter) {
    if (!adapter) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (g_adapter_count >= MAX_ADAPTERS) {
        return UFT_ERR_OVERFLOW;
    }
    
    /* Check for duplicate */
    for (size_t i = 0; i < g_adapter_count; i++) {
        if (g_adapters[i]->format_id == adapter->format_id) {
            return UFT_ERR_ALREADY_EXISTS;
        }
    }
    
    g_adapters[g_adapter_count++] = adapter;
    return UFT_SUCCESS;
}

const uft_format_adapter_t *uft_adapter_find_by_id(uint32_t format_id) {
    for (size_t i = 0; i < g_adapter_count; i++) {
        if (g_adapters[i]->format_id == format_id) {
            return g_adapters[i];
        }
    }
    return NULL;
}

const uft_format_adapter_t *uft_adapter_find_by_extension(const char *extension) {
    if (!extension) return NULL;
    
    for (size_t i = 0; i < g_adapter_count; i++) {
        if (!g_adapters[i]->extensions) continue;
        
        /* Check if extension is in comma-separated list */
        const char *exts = g_adapters[i]->extensions;
        size_t ext_len = strlen(extension);
        
        while (*exts) {
            /* Skip whitespace and commas */
            while (*exts == ',' || *exts == ' ') exts++;
            if (!*exts) break;
            
            /* Compare */
            const char *end = exts;
            while (*end && *end != ',' && *end != ' ') end++;
            
            size_t len = (size_t)(end - exts);
            if (len == ext_len) {
                bool match = true;
                for (size_t j = 0; j < len; j++) {
                    char c1 = exts[j];
                    char c2 = extension[j];
                    /* Case-insensitive */
                    if (c1 >= 'A' && c1 <= 'Z') c1 += 32;
                    if (c2 >= 'A' && c2 <= 'Z') c2 += 32;
                    if (c1 != c2) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    return g_adapters[i];
                }
            }
            
            exts = end;
        }
    }
    return NULL;
}

size_t uft_adapter_probe_all(
    const uint8_t *data,
    size_t size,
    const char *filename,
    uft_format_score_t *scores,
    size_t max_scores
) {
    if (!data || !scores || max_scores == 0) return 0;
    
    size_t found = 0;
    
    for (size_t i = 0; i < g_adapter_count && found < max_scores; i++) {
        if (!g_adapters[i]->probe) continue;
        
        uft_format_score_t score = g_adapters[i]->probe(data, size, filename);
        
        if (score.overall > 0.0f) {
            score.format_name = g_adapters[i]->name;
            score.format_id = g_adapters[i]->format_id;
            scores[found++] = score;
        }
    }
    
    /* Sort by score (descending) */
    for (size_t i = 0; i < found; i++) {
        for (size_t j = i + 1; j < found; j++) {
            if (scores[j].overall > scores[i].overall) {
                uft_format_score_t tmp = scores[i];
                scores[i] = scores[j];
                scores[j] = tmp;
            }
        }
    }
    
    return found;
}

const uft_format_adapter_t *uft_adapter_detect(
    const uint8_t *data,
    size_t size,
    const char *filename,
    uft_format_score_t *score
) {
    uft_format_score_t scores[MAX_ADAPTERS];
    size_t found = uft_adapter_probe_all(data, size, filename, scores, MAX_ADAPTERS);
    
    if (found == 0) return NULL;
    
    /* Return best match */
    if (score) {
        *score = scores[0];
    }
    
    return uft_adapter_find_by_id(scores[0].format_id);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Track/Sector Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_track_data_init(uft_track_data_t *track) {
    if (!track) return;
    memset(track, 0, sizeof(*track));
}

void uft_track_data_free(uft_track_data_t *track) {
    if (!track) return;
    
    if (track->raw_data) {
        free(track->raw_data);
        track->raw_data = NULL;
    }
    
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            if (track->sectors[i].data) {
                free(track->sectors[i].data);
            }
        }
        free(track->sectors);
        track->sectors = NULL;
    }
    
    if (track->bit_times) {
        free(track->bit_times);
        track->bit_times = NULL;
    }
    
    track->sector_count = 0;
}

void uft_sector_data_init(uft_sector_data_t *sector) {
    if (!sector) return;
    memset(sector, 0, sizeof(*sector));
}

uft_error_t uft_track_alloc_sectors(uft_track_data_t *track, size_t count) {
    if (!track) return UFT_ERR_INVALID_PARAM;
    
    if (track->sectors) {
        uft_track_data_free(track);
    }
    
    track->sectors = calloc(count, sizeof(uft_sector_data_t));
    if (!track->sectors) {
        return UFT_ERR_NOMEM;
    }
    
    track->sector_count = count;
    return UFT_SUCCESS;
}

uft_sector_data_t *uft_track_find_sector(
    uft_track_data_t *track,
    uint8_t sector_id
) {
    if (!track || !track->sectors) return NULL;
    
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].sector_id == sector_id) {
            return &track->sectors[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Diagnostics
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get adapter count
 */
size_t uft_adapter_get_count(void) {
    return g_adapter_count;
}

/**
 * @brief Get adapter by index
 */
const uft_format_adapter_t *uft_adapter_get_by_index(size_t index) {
    if (index >= g_adapter_count) return NULL;
    return g_adapters[index];
}

/**
 * @brief Print registered adapters (debug)
 */
void uft_adapter_print_all(void) {
    printf("Registered Format Adapters: %zu\n", g_adapter_count);
    for (size_t i = 0; i < g_adapter_count; i++) {
        printf("  [%02zu] %-8s (0x%04X) - %s\n",
               i,
               g_adapters[i]->name,
               g_adapters[i]->format_id,
               g_adapters[i]->description ? g_adapters[i]->description : "");
    }
}

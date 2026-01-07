/**
 * @file uft_format_hints.c
 * @brief Format-Guided Decoding Implementation
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#include "uft/decoder/uft_format_hints.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>  /* For strcasecmp */
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * C64 GCR Zone Tables
 * ============================================================================ */

static const uint8_t c64_zone_tracks[] = { 17, 24, 30, 35, 40 };
static const double c64_zone_cell_ns[] = { 3250, 3500, 3750, 4000 };
static const uint8_t c64_zone_sectors[] = { 21, 19, 18, 17 };

/* ============================================================================
 * Apple GCR Zone Tables
 * ============================================================================ */

static const uint8_t apple_zone_tracks[] = { 16, 32, 48, 64, 80 };
static const double apple_zone_cell_ns[] = { 3840, 3520, 3200, 2880, 2560 };
static const uint8_t apple_zone_sectors[] = { 12, 11, 10, 9, 8 };

/* ============================================================================
 * Predefined Format Hints
 * ============================================================================ */

static const uft_format_hint_t format_hints[] = {
    /* FM Generic */
    {
        .format_id = UFT_FMT_FM_GENERIC,
        .name = "FM",
        .description = "Generic FM sector image",
        .tracks_min = 40, .tracks_max = 80, .tracks_default = 40,
        .sides = 1, .rpm = 300, .is_clv = false,
        .encoding = UFT_ENC_FM, .bitrate_bps = 125000, .cell_time_ns = 4000,
        .sectors_per_track = 10, .sector_size = 256, .interleave = 1, .skew = 0,
        .sync_pattern = 0xF57E, .sync_bits = 16, .gap_bytes = 10,
        .timing_tolerance = 0.15, .max_retries = 5
    },
    
    /* MFM DD */
    {
        .format_id = UFT_FMT_MFM_GENERIC,
        .name = "MFM_DD",
        .description = "Generic MFM sector image (DD)",
        .tracks_min = 40, .tracks_max = 80, .tracks_default = 80,
        .sides = 2, .rpm = 300, .is_clv = false,
        .encoding = UFT_ENC_MFM, .bitrate_bps = 250000, .cell_time_ns = 2000,
        .sectors_per_track = 9, .sector_size = 512, .interleave = 1, .skew = 0,
        .sync_pattern = 0x4489, .sync_bits = 16, .gap_bytes = 22,
        .timing_tolerance = 0.10, .max_retries = 5
    },
    
    /* Amiga ADF */
    {
        .format_id = UFT_FMT_AMIGA_ADF,
        .name = "ADF",
        .description = "Amiga ADF sector image",
        .tracks_min = 80, .tracks_max = 84, .tracks_default = 80,
        .sides = 2, .rpm = 300, .is_clv = false,
        .encoding = UFT_ENC_AMIGA, .bitrate_bps = 250000, .cell_time_ns = 2000,
        .sectors_per_track = 11, .sector_size = 512, .interleave = 1, .skew = 0,
        .sync_pattern = 0x4489, .sync_bits = 16, .gap_bytes = 0,
        .timing_tolerance = 0.10, .max_retries = 5
    },
    
    /* Commodore D64 */
    {
        .format_id = UFT_FMT_CBM_D64,
        .name = "D64",
        .description = "Commodore 64 D64 image",
        .tracks_min = 35, .tracks_max = 42, .tracks_default = 35,
        .sides = 1, .rpm = 300, .is_clv = true,
        .encoding = UFT_ENC_GCR_CBM, .bitrate_bps = 307700, .cell_time_ns = 3250,
        .sectors_per_track = 21, .sector_size = 256, .interleave = 4, .skew = 0,
        .sync_pattern = 0x3FF, .sync_bits = 10, .gap_bytes = 9,
        .num_zones = 4, .zone_tracks = c64_zone_tracks,
        .zone_cell_ns = c64_zone_cell_ns, .zone_sectors = c64_zone_sectors,
        .timing_tolerance = 0.12, .max_retries = 5,
        .has_error_map = false, .has_copy_protection = false
    },
    
    /* Commodore D64 with Error Map */
    {
        .format_id = UFT_FMT_CBM_D64_ERRMAP,
        .name = "D64_ERR",
        .description = "Commodore 64 D64 with error map",
        .tracks_min = 35, .tracks_max = 42, .tracks_default = 35,
        .sides = 1, .rpm = 300, .is_clv = true,
        .encoding = UFT_ENC_GCR_CBM, .bitrate_bps = 307700, .cell_time_ns = 3250,
        .sectors_per_track = 21, .sector_size = 256, .interleave = 4, .skew = 0,
        .sync_pattern = 0x3FF, .sync_bits = 10, .gap_bytes = 9,
        .num_zones = 4, .zone_tracks = c64_zone_tracks,
        .zone_cell_ns = c64_zone_cell_ns, .zone_sectors = c64_zone_sectors,
        .timing_tolerance = 0.12, .max_retries = 5,
        .has_error_map = true, .has_copy_protection = false
    },
    
    /* Apple DOS 3.3 */
    {
        .format_id = UFT_FMT_APPLE_DOS33,
        .name = "DSK",
        .description = "Apple DOS 3.3 sector image",
        .tracks_min = 35, .tracks_max = 40, .tracks_default = 35,
        .sides = 1, .rpm = 300, .is_clv = false,
        .encoding = UFT_ENC_GCR_APPLE, .bitrate_bps = 250000, .cell_time_ns = 4000,
        .sectors_per_track = 16, .sector_size = 256, .interleave = 1, .skew = 0,
        .sync_pattern = 0xD5AA96, .sync_bits = 24, .gap_bytes = 5,
        .timing_tolerance = 0.12, .max_retries = 5
    },
    
    /* Apple 400K/800K */
    {
        .format_id = UFT_FMT_APPLE_800K,
        .name = "DC42",
        .description = "Apple 400K/800K Macintosh",
        .tracks_min = 80, .tracks_max = 80, .tracks_default = 80,
        .sides = 2, .rpm = 300, .is_clv = true,
        .encoding = UFT_ENC_GCR_MAC, .bitrate_bps = 500000, .cell_time_ns = 2000,
        .sectors_per_track = 12, .sector_size = 512, .interleave = 2, .skew = 0,
        .sync_pattern = 0xD5AA96, .sync_bits = 24, .gap_bytes = 5,
        .num_zones = 5, .zone_tracks = apple_zone_tracks,
        .zone_cell_ns = apple_zone_cell_ns, .zone_sectors = apple_zone_sectors,
        .timing_tolerance = 0.10, .max_retries = 5
    },
    
    /* IBM 1.44MB */
    {
        .format_id = UFT_FMT_IBM_1440K,
        .name = "IMG",
        .description = "IBM PC 1.44MB HD",
        .tracks_min = 80, .tracks_max = 80, .tracks_default = 80,
        .sides = 2, .rpm = 300, .is_clv = false,
        .encoding = UFT_ENC_MFM, .bitrate_bps = 500000, .cell_time_ns = 1000,
        .sectors_per_track = 18, .sector_size = 512, .interleave = 1, .skew = 0,
        .sync_pattern = 0x4489, .sync_bits = 16, .gap_bytes = 22,
        .timing_tolerance = 0.08, .max_retries = 5
    },
    
    /* Atari ST */
    {
        .format_id = UFT_FMT_ATARI_ST,
        .name = "ST",
        .description = "Atari ST disk image",
        .tracks_min = 80, .tracks_max = 86, .tracks_default = 80,
        .sides = 2, .rpm = 300, .is_clv = false,
        .encoding = UFT_ENC_MFM, .bitrate_bps = 250000, .cell_time_ns = 2000,
        .sectors_per_track = 9, .sector_size = 512, .interleave = 1, .skew = 0,
        .sync_pattern = 0x4489, .sync_bits = 16, .gap_bytes = 22,
        .timing_tolerance = 0.10, .max_retries = 5
    }
};

#define NUM_FORMAT_HINTS (sizeof(format_hints) / sizeof(format_hints[0]))

/* ============================================================================
 * API Implementation
 * ============================================================================ */

const uft_format_hint_t* uft_format_get_hint(uft_format_id_t format_id) {
    for (size_t i = 0; i < NUM_FORMAT_HINTS; i++) {
        if (format_hints[i].format_id == format_id) {
            return &format_hints[i];
        }
    }
    return NULL;
}

const uft_format_hint_t* uft_format_get_hint_by_name(const char *name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < NUM_FORMAT_HINTS; i++) {
        if (strcasecmp(format_hints[i].name, name) == 0) {
            return &format_hints[i];
        }
    }
    return NULL;
}

size_t uft_format_get_all_hints(
    const uft_format_hint_t **hints,
    size_t max
) {
    size_t count = (max < NUM_FORMAT_HINTS) ? max : NUM_FORMAT_HINTS;
    
    for (size_t i = 0; i < count; i++) {
        hints[i] = &format_hints[i];
    }
    
    return count;
}

/* ============================================================================
 * Decode Context Functions
 * ============================================================================ */

void uft_decode_context_init(
    uft_decode_context_t *ctx,
    const uft_format_hint_t *hint
) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->hint = hint;
    
    if (hint) {
        ctx->effective_cell_ns = hint->cell_time_ns;
        ctx->effective_sectors = hint->sectors_per_track;
    }
}

void uft_decode_context_set_track(
    uft_decode_context_t *ctx,
    uint8_t track,
    uint8_t head
) {
    if (!ctx) return;
    
    ctx->current_track = track;
    ctx->current_head = head;
    
    if (ctx->hint && ctx->hint->is_clv && ctx->hint->num_zones > 0) {
        /* Update zone */
        ctx->current_zone = uft_format_get_zone(ctx->hint, track);
        ctx->effective_cell_ns = uft_format_get_zone_cell_ns(
            ctx->hint, ctx->current_zone
        );
        ctx->effective_sectors = uft_format_get_zone_sectors(
            ctx->hint, ctx->current_zone
        );
    }
}

double uft_decode_context_get_cell_ns(const uft_decode_context_t *ctx) {
    if (!ctx) return 2000;  /* Default MFM DD */
    return ctx->effective_cell_ns;
}

uint8_t uft_decode_context_get_sectors(const uft_decode_context_t *ctx) {
    if (!ctx) return 9;  /* Default */
    return ctx->effective_sectors;
}

void uft_decode_context_reset_stats(uft_decode_context_t *ctx) {
    if (!ctx) return;
    
    ctx->sectors_decoded = 0;
    ctx->sectors_failed = 0;
    ctx->sync_found = 0;
    ctx->sync_missed = 0;
}

/* ============================================================================
 * Zone Functions
 * ============================================================================ */

uint8_t uft_format_get_zone(
    const uft_format_hint_t *hint,
    uint8_t track
) {
    if (!hint || hint->num_zones == 0 || !hint->zone_tracks) {
        return 0;
    }
    
    for (uint8_t z = 0; z < hint->num_zones; z++) {
        if (track < hint->zone_tracks[z]) {
            return z;
        }
    }
    
    return hint->num_zones - 1;
}

double uft_format_get_zone_cell_ns(
    const uft_format_hint_t *hint,
    uint8_t zone
) {
    if (!hint || !hint->zone_cell_ns || zone >= hint->num_zones) {
        return hint ? hint->cell_time_ns : 2000;
    }
    
    return hint->zone_cell_ns[zone];
}

uint8_t uft_format_get_zone_sectors(
    const uft_format_hint_t *hint,
    uint8_t zone
) {
    if (!hint || !hint->zone_sectors || zone >= hint->num_zones) {
        return hint ? hint->sectors_per_track : 9;
    }
    
    return hint->zone_sectors[zone];
}

/* ============================================================================
 * Format Detection
 * ============================================================================ */

size_t uft_format_detect(
    const uint32_t *flux_data,
    size_t flux_count,
    double sample_rate,
    uft_format_candidate_t *candidates,
    size_t max_candidates
) {
    if (!flux_data || !candidates || flux_count == 0) {
        return 0;
    }
    
    /* Calculate average cell time */
    uint64_t sum = 0;
    for (size_t i = 0; i < flux_count && i < 10000; i++) {
        sum += flux_data[i];
    }
    
    size_t sample_count = (flux_count < 10000) ? flux_count : 10000;
    double avg_samples = (double)sum / sample_count;
    double avg_ns = avg_samples / sample_rate * 1e9;
    
    /* Cell time is typically half the average flux interval (for MFM) */
    double est_cell_ns = avg_ns / 2.0;
    
    size_t found = 0;
    
    /* Check each format */
    for (size_t i = 0; i < NUM_FORMAT_HINTS && found < max_candidates; i++) {
        double diff = (format_hints[i].cell_time_ns - est_cell_ns) / 
                     format_hints[i].cell_time_ns;
        
        if (fabs(diff) < format_hints[i].timing_tolerance) {
            candidates[found].format_id = format_hints[i].format_id;
            candidates[found].confidence = (uint8_t)(100 * (1.0 - fabs(diff)));
            candidates[found].reason = "Cell time match";
            found++;
        }
    }
    
    /* Sort by confidence (simple bubble sort) */
    for (size_t i = 0; i < found; i++) {
        for (size_t j = i + 1; j < found; j++) {
            if (candidates[j].confidence > candidates[i].confidence) {
                uft_format_candidate_t tmp = candidates[i];
                candidates[i] = candidates[j];
                candidates[j] = tmp;
            }
        }
    }
    
    return found;
}

size_t uft_format_detect_from_sector(
    const uint8_t *sector_data,
    size_t sector_size,
    uft_format_candidate_t *candidates,
    size_t max_candidates
) {
    if (!sector_data || !candidates || sector_size == 0) {
        return 0;
    }
    
    size_t found = 0;
    
    /* Check for known boot sector signatures */
    
    /* Amiga: "DOS" at offset 0 */
    if (sector_size >= 4 && 
        sector_data[0] == 'D' && sector_data[1] == 'O' && sector_data[2] == 'S') {
        if (found < max_candidates) {
            candidates[found].format_id = UFT_FMT_AMIGA_ADF;
            candidates[found].confidence = 95;
            candidates[found].reason = "Amiga DOS signature";
            found++;
        }
    }
    
    /* FAT boot sector: 0x55 0xAA at end */
    if (sector_size >= 512 && 
        sector_data[510] == 0x55 && sector_data[511] == 0xAA) {
        if (found < max_candidates) {
            candidates[found].format_id = UFT_FMT_IBM_1440K;
            candidates[found].confidence = 80;
            candidates[found].reason = "FAT boot signature";
            found++;
        }
    }
    
    /* C64: BAM at track 18 */
    if (sector_size == 256 && sector_data[0] == 0x12) {
        if (found < max_candidates) {
            candidates[found].format_id = UFT_FMT_CBM_D64;
            candidates[found].confidence = 70;
            candidates[found].reason = "C64 directory track hint";
            found++;
        }
    }
    
    return found;
}

/* ============================================================================
 * Guided Decoding
 * ============================================================================ */

int uft_format_guided_decode(
    const uint32_t *flux_data,
    size_t flux_count,
    double sample_rate,
    uft_decode_context_t *ctx,
    uint8_t *output,
    size_t max_output,
    size_t *actual_output
) {
    if (!flux_data || !ctx || !output || !actual_output) {
        return -1;
    }
    
    *actual_output = 0;
    
    if (!ctx->hint) {
        return -1;  /* Need hint for guided decode */
    }
    
    /* Get cell time in samples */
    double cell_ns = uft_decode_context_get_cell_ns(ctx);
    double cell_samples = cell_ns * sample_rate / 1e9;
    
    /* Simple PLL decode */
    double phase = 0;
    size_t bit_pos = 0;
    uint8_t current_byte = 0;
    size_t byte_pos = 0;
    
    for (size_t i = 0; i < flux_count && byte_pos < max_output; i++) {
        double interval = flux_data[i];
        
        /* How many cells fit in this interval? */
        int cells = (int)((interval + phase) / cell_samples + 0.5);
        if (cells < 1) cells = 1;
        if (cells > 4) cells = 4;  /* Sanity limit */
        
        /* Phase adjustment */
        double expected = cells * cell_samples;
        double error = interval - expected;
        phase += error * 0.1;  /* PLL gain */
        
        /* Output bits */
        for (int c = 0; c < cells; c++) {
            uint8_t bit = (c == cells - 1) ? 1 : 0;
            
            current_byte = (current_byte << 1) | bit;
            bit_pos++;
            
            if ((bit_pos % 8) == 0) {
                output[byte_pos++] = current_byte;
                current_byte = 0;
            }
        }
    }
    
    *actual_output = byte_pos;
    ctx->sectors_decoded++;
    
    return 0;
}

int64_t uft_format_find_sync(
    const uint8_t *data,
    size_t bit_count,
    const uft_format_hint_t *hint,
    size_t start_bit
) {
    if (!data || !hint || bit_count == 0) {
        return -1;
    }
    
    uint64_t pattern = hint->sync_pattern;
    uint8_t pattern_bits = hint->sync_bits;
    uint64_t mask = (1ULL << pattern_bits) - 1;
    
    uint64_t window = 0;
    
    for (size_t pos = start_bit; pos < bit_count; pos++) {
        size_t byte_idx = pos / 8;
        size_t bit_idx = 7 - (pos % 8);
        uint8_t bit = (data[byte_idx] >> bit_idx) & 1;
        
        window = ((window << 1) | bit) & mask;
        
        if (window == pattern) {
            return (int64_t)(pos - pattern_bits + 1);
        }
    }
    
    return -1;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* uft_encoding_name(uft_encoding_type_t encoding) {
    switch (encoding) {
        case UFT_ENC_FM: return "FM";
        case UFT_ENC_MFM: return "MFM";
        case UFT_ENC_GCR_CBM: return "GCR (Commodore)";
        case UFT_ENC_GCR_APPLE: return "GCR (Apple 6&2)";
        case UFT_ENC_GCR_APPLE32: return "GCR (Apple 5&3)";
        case UFT_ENC_GCR_MAC: return "GCR (Macintosh)";
        case UFT_ENC_DMMFM: return "DMMFM";
        case UFT_ENC_AMIGA: return "Amiga MFM";
        default: return "Unknown";
    }
}

const char* uft_format_name(uft_format_id_t format_id) {
    const uft_format_hint_t *hint = uft_format_get_hint(format_id);
    return hint ? hint->name : "Unknown";
}

uft_format_hint_t* uft_format_hint_create(void) {
    uft_format_hint_t *hint = (uft_format_hint_t *)calloc(1, sizeof(*hint));
    if (hint) {
        /* Set safe defaults */
        hint->format_id = UFT_FMT_CUSTOM;
        hint->tracks_default = 80;
        hint->sides = 2;
        hint->rpm = 300;
        hint->encoding = UFT_ENC_MFM;
        hint->cell_time_ns = 2000;
        hint->sectors_per_track = 9;
        hint->sector_size = 512;
        hint->timing_tolerance = 0.15;
        hint->max_retries = 5;
    }
    return hint;
}

void uft_format_hint_free(uft_format_hint_t *hint) {
    if (hint && hint->format_id >= UFT_FMT_CUSTOM) {
        free(hint);
    }
    /* Don't free static hints */
}

uft_format_hint_t* uft_format_hint_copy(const uft_format_hint_t *hint) {
    if (!hint) return NULL;
    
    uft_format_hint_t *copy = (uft_format_hint_t *)malloc(sizeof(*copy));
    if (copy) {
        memcpy(copy, hint, sizeof(*copy));
        copy->format_id = UFT_FMT_CUSTOM;  /* Mark as custom copy */
    }
    return copy;
}

size_t uft_format_hint_to_json(
    const uft_format_hint_t *hint,
    char *buffer,
    size_t size
) {
    if (!hint) return 0;
    
    size_t offset = 0;
    
    #define APPEND(...) do { \
        int n = snprintf(buffer ? buffer + offset : NULL, \
                        buffer ? size - offset : 0, __VA_ARGS__); \
        if (n > 0) offset += n; \
    } while(0)
    
    APPEND("{\n");
    APPEND("  \"format_id\": %d,\n", hint->format_id);
    APPEND("  \"name\": \"%s\",\n", hint->name ? hint->name : "");
    APPEND("  \"description\": \"%s\",\n", hint->description ? hint->description : "");
    APPEND("  \"tracks\": { \"min\": %u, \"max\": %u, \"default\": %u },\n",
           hint->tracks_min, hint->tracks_max, hint->tracks_default);
    APPEND("  \"sides\": %u,\n", hint->sides);
    APPEND("  \"rpm\": %u,\n", hint->rpm);
    APPEND("  \"encoding\": \"%s\",\n", uft_encoding_name(hint->encoding));
    APPEND("  \"bitrate_bps\": %u,\n", hint->bitrate_bps);
    APPEND("  \"cell_time_ns\": %.1f,\n", hint->cell_time_ns);
    APPEND("  \"sectors_per_track\": %u,\n", hint->sectors_per_track);
    APPEND("  \"sector_size\": %u,\n", hint->sector_size);
    APPEND("  \"timing_tolerance\": %.2f\n", hint->timing_tolerance);
    APPEND("}\n");
    
    #undef APPEND
    
    return offset;
}

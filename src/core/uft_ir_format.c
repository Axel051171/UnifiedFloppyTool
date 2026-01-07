/**
 * @file uft_ir_format.c
 * @brief UFT Intermediate Representation (UFT-IR) Format Implementation
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_ir_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Safe calloc with overflow check */
static void* safe_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    if (count > SIZE_MAX / size) return NULL;  /* Overflow check */
    return calloc(count, size);
}

/** Safe realloc */
static void* safe_realloc(void* ptr, size_t size) {
    if (size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, size);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * COMPRESSION HELPERS (P2-IMPL-003)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Simple RLE compression
 * Format: [count-1][byte] for runs of 3+, or raw bytes prefixed with 0x00
 * @return Compressed size, or 0 on failure
 */
static size_t ir_compress_rle(const uint8_t* input, size_t in_size,
                               uint8_t* output, size_t out_max) {
    if (!input || !output || in_size == 0 || out_max < 2) return 0;
    
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < in_size && out_pos + 2 < out_max) {
        uint8_t current = input[in_pos];
        size_t run_len = 1;
        
        /* Count run length */
        while (in_pos + run_len < in_size && 
               input[in_pos + run_len] == current && 
               run_len < 257) {
            run_len++;
        }
        
        if (run_len >= 3) {
            /* Encode run: [0xFF][count-1][byte] */
            if (out_pos + 3 > out_max) break;
            output[out_pos++] = 0xFF;
            output[out_pos++] = (uint8_t)(run_len - 1);
            output[out_pos++] = current;
            in_pos += run_len;
        } else {
            /* Raw byte (escape 0xFF) */
            if (current == 0xFF) {
                if (out_pos + 2 > out_max) break;
                output[out_pos++] = 0xFF;
                output[out_pos++] = 0x00;
            } else {
                output[out_pos++] = current;
            }
            in_pos++;
        }
    }
    
    /* Only use compression if it actually saves space */
    if (out_pos >= in_size || in_pos < in_size) {
        return 0;  /* Compression didn't help */
    }
    
    return out_pos;
}

/**
 * @brief Simple RLE decompression
 */
static size_t ir_decompress_rle(const uint8_t* input, size_t in_size,
                                 uint8_t* output, size_t out_max) {
    if (!input || !output || in_size == 0) return 0;
    
    size_t in_pos = 0;
    size_t out_pos = 0;
    
    while (in_pos < in_size && out_pos < out_max) {
        uint8_t byte = input[in_pos++];
        
        if (byte == 0xFF && in_pos < in_size) {
            uint8_t count = input[in_pos++];
            if (count == 0x00) {
                /* Escaped 0xFF */
                output[out_pos++] = 0xFF;
            } else if (in_pos < in_size) {
                /* Run of bytes */
                uint8_t val = input[in_pos++];
                size_t run_len = count + 1;
                if (out_pos + run_len > out_max) run_len = out_max - out_pos;
                memset(output + out_pos, val, run_len);
                out_pos += run_len;
            }
        } else {
            output[out_pos++] = byte;
        }
    }
    
    return out_pos;
}

/**
 * @brief Delta encoding for flux data
 * Stores differences between consecutive values
 */
static size_t ir_compress_delta(const uint8_t* input, size_t in_size,
                                 uint8_t* output, size_t out_max) {
    if (!input || !output || in_size < 2 || out_max < in_size) return 0;
    
    /* First byte stored as-is */
    output[0] = input[0];
    
    /* Store deltas */
    for (size_t i = 1; i < in_size; i++) {
        int16_t delta = (int16_t)input[i] - (int16_t)input[i - 1];
        /* Store as signed byte (wraps around) */
        output[i] = (uint8_t)(delta & 0xFF);
    }
    
    /* Delta encoding always same size - apply RLE on top */
    uint8_t* rle_buf = (uint8_t*)malloc(out_max);
    if (!rle_buf) return in_size;  /* Return delta-only */
    
    size_t rle_size = ir_compress_rle(output, in_size, rle_buf, out_max);
    if (rle_size > 0 && rle_size < in_size) {
        memcpy(output, rle_buf, rle_size);
        free(rle_buf);
        return rle_size;
    }
    
    free(rle_buf);
    return in_size;  /* Delta only, no RLE benefit */
}

/**
 * @brief Delta decoding
 */
static size_t ir_decompress_delta(const uint8_t* input, size_t in_size,
                                   uint8_t* output, size_t out_max,
                                   bool with_rle) {
    if (!input || !output || in_size == 0) return 0;
    
    const uint8_t* delta_data = input;
    size_t delta_size = in_size;
    uint8_t* temp = NULL;
    
    /* First decompress RLE if needed */
    if (with_rle) {
        temp = (uint8_t*)malloc(out_max);
        if (!temp) return 0;
        delta_size = ir_decompress_rle(input, in_size, temp, out_max);
        if (delta_size == 0) {
            free(temp);
            return 0;
        }
        delta_data = temp;
    }
    
    /* Reconstruct from deltas */
    if (delta_size > 0 && delta_size <= out_max) {
        output[0] = delta_data[0];
        for (size_t i = 1; i < delta_size; i++) {
            output[i] = (uint8_t)(output[i - 1] + (int8_t)delta_data[i]);
        }
    }
    
    free(temp);
    return delta_size;
}

/** CRC32 implementation (IEEE polynomial) */
static uint32_t crc32_table[256];
static int crc32_table_init = 0;

static void init_crc32_table(void) {
    if (crc32_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_init = 1;
}

static uint32_t calc_crc32(const void* data, size_t len) {
    init_crc32_table();
    const uint8_t* p = (const uint8_t*)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * LIFECYCLE: CREATION & DESTRUCTION
 * ═══════════════════════════════════════════════════════════════════════════ */

uft_ir_disk_t* uft_ir_disk_create(uint8_t cylinders, uint8_t heads) {
    if (cylinders > UFT_IR_MAX_CYLINDERS || heads > UFT_IR_MAX_HEADS) {
        return NULL;
    }
    
    uft_ir_disk_t* disk = (uft_ir_disk_t*)safe_calloc(1, sizeof(uft_ir_disk_t));
    if (!disk) return NULL;
    
    /* Initialize header */
    disk->magic = UFT_IR_MAGIC;
    disk->version = UFT_IR_VERSION;
    disk->header_size = sizeof(uft_ir_disk_t);
    
    /* Initialize geometry */
    disk->geometry.cylinders = cylinders;
    disk->geometry.heads = heads;
    disk->geometry.total_sectors = 0;  /* Will be computed */
    
    /* Allocate track pointer array */
    uint16_t max_tracks = (uint16_t)(cylinders * heads);
    disk->tracks = (uft_ir_track_t**)safe_calloc(max_tracks, sizeof(uft_ir_track_t*));
    if (!disk->tracks) {
        free(disk);
        return NULL;
    }
    
    /* Set timestamps */
    disk->metadata.creation_time = (uint64_t)time(NULL);
    disk->metadata.modification_time = disk->metadata.creation_time;
    
    return disk;
}

void uft_ir_disk_free(uft_ir_disk_t* disk) {
    if (!disk) return;
    
    /* Free all tracks */
    if (disk->tracks) {
        for (uint16_t i = 0; i < disk->track_count; i++) {
            uft_ir_track_free(disk->tracks[i]);
        }
        free(disk->tracks);
    }
    
    /* Free custom metadata */
    if (disk->metadata.custom_data) {
        free(disk->metadata.custom_data);
    }
    
    free(disk);
}

uft_ir_track_t* uft_ir_track_create(uint8_t cylinder, uint8_t head) {
    if (cylinder > UFT_IR_MAX_CYLINDERS || head > UFT_IR_MAX_HEADS) {
        return NULL;
    }
    
    uft_ir_track_t* track = (uft_ir_track_t*)safe_calloc(1, sizeof(uft_ir_track_t));
    if (!track) return NULL;
    
    track->cylinder = cylinder;
    track->head = head;
    track->encoding = UFT_IR_ENC_UNKNOWN;
    track->quality = UFT_IR_QUALITY_UNKNOWN;
    track->capture_timestamp = (uint64_t)time(NULL);
    
    return track;
}

void uft_ir_track_free(uft_ir_track_t* track) {
    if (!track) return;
    
    /* Free revolutions */
    for (int i = 0; i < UFT_IR_MAX_REVOLUTIONS; i++) {
        if (track->revolutions[i]) {
            uft_ir_revolution_free(track->revolutions[i]);
        }
    }
    
    /* Free weak regions */
    if (track->weak_regions) {
        free(track->weak_regions);
    }
    
    /* Free protections */
    if (track->protections) {
        free(track->protections);
    }
    
    /* Free decoded data */
    if (track->decoded_data) {
        free(track->decoded_data);
    }
    
    free(track);
}

uft_ir_revolution_t* uft_ir_revolution_create(uint32_t flux_count) {
    if (flux_count > UFT_IR_MAX_FLUX_PER_REV) {
        return NULL;
    }
    
    uft_ir_revolution_t* rev = (uft_ir_revolution_t*)safe_calloc(1, sizeof(uft_ir_revolution_t));
    if (!rev) return NULL;
    
    rev->data_type = UFT_IR_DATA_FLUX_DELTA;
    
    if (flux_count > 0) {
        rev->flux_deltas = (uint32_t*)safe_calloc(flux_count, sizeof(uint32_t));
        if (!rev->flux_deltas) {
            free(rev);
            return NULL;
        }
        rev->flux_count = flux_count;
        rev->data_size = flux_count * sizeof(uint32_t);
    }
    
    return rev;
}

void uft_ir_revolution_free(uft_ir_revolution_t* rev) {
    if (!rev) return;
    
    if (rev->flux_deltas) {
        free(rev->flux_deltas);
    }
    if (rev->flux_confidence) {
        free(rev->flux_confidence);
    }
    
    free(rev);
}

uft_ir_track_t* uft_ir_track_clone(const uft_ir_track_t* src) {
    if (!src) return NULL;
    
    uft_ir_track_t* dst = uft_ir_track_create(src->cylinder, src->head);
    if (!dst) return NULL;
    
    /* Copy scalar fields */
    dst->flags = src->flags;
    dst->cyl_offset_quarters = src->cyl_offset_quarters;
    dst->encoding = src->encoding;
    dst->sectors_expected = src->sectors_expected;
    dst->sectors_found = src->sectors_found;
    dst->sectors_good = src->sectors_good;
    dst->bitcell_ns = src->bitcell_ns;
    dst->rpm_measured = src->rpm_measured;
    dst->write_splice_ns = src->write_splice_ns;
    dst->revolution_count = 0;  /* Will be set by add */
    dst->best_revolution = src->best_revolution;
    dst->quality = src->quality;
    dst->quality_score = src->quality_score;
    dst->capture_timestamp = src->capture_timestamp;
    dst->capture_duration_ms = src->capture_duration_ms;
    memcpy(dst->comment, src->comment, UFT_IR_MAX_COMMENT_LEN);
    
    /* Clone revolutions */
    for (int i = 0; i < src->revolution_count && i < UFT_IR_MAX_REVOLUTIONS; i++) {
        if (src->revolutions[i]) {
            uft_ir_revolution_t* rev_clone = uft_ir_revolution_clone(src->revolutions[i]);
            if (rev_clone) {
                uft_ir_track_add_revolution(dst, rev_clone);
            }
        }
    }
    
    /* Clone weak regions */
    if (src->weak_region_count > 0 && src->weak_regions) {
        dst->weak_regions = (uft_ir_weak_region_t*)safe_calloc(
            src->weak_region_count, sizeof(uft_ir_weak_region_t));
        if (dst->weak_regions) {
            memcpy(dst->weak_regions, src->weak_regions,
                   src->weak_region_count * sizeof(uft_ir_weak_region_t));
            dst->weak_region_count = src->weak_region_count;
        }
    }
    
    /* Clone protections */
    if (src->protection_count > 0 && src->protections) {
        dst->protections = (uft_ir_protection_t*)safe_calloc(
            src->protection_count, sizeof(uft_ir_protection_t));
        if (dst->protections) {
            memcpy(dst->protections, src->protections,
                   src->protection_count * sizeof(uft_ir_protection_t));
            dst->protection_count = src->protection_count;
        }
    }
    
    /* Clone decoded data */
    if (src->decoded_size > 0 && src->decoded_data) {
        dst->decoded_data = (uint8_t*)safe_calloc(1, src->decoded_size);
        if (dst->decoded_data) {
            memcpy(dst->decoded_data, src->decoded_data, src->decoded_size);
            dst->decoded_size = src->decoded_size;
        }
    }
    
    return dst;
}

uft_ir_revolution_t* uft_ir_revolution_clone(const uft_ir_revolution_t* src) {
    if (!src) return NULL;
    
    uft_ir_revolution_t* dst = uft_ir_revolution_create(src->flux_count);
    if (!dst) return NULL;
    
    /* Copy scalar fields */
    dst->rev_index = src->rev_index;
    dst->flags = src->flags;
    dst->duration_ns = src->duration_ns;
    dst->index_offset_ns = src->index_offset_ns;
    dst->data_type = src->data_type;
    dst->quality_score = src->quality_score;
    memcpy(&dst->stats, &src->stats, sizeof(uft_ir_flux_stats_t));
    
    /* Copy flux data */
    if (src->flux_count > 0 && src->flux_deltas && dst->flux_deltas) {
        memcpy(dst->flux_deltas, src->flux_deltas, 
               src->flux_count * sizeof(uint32_t));
    }
    
    /* Clone confidence data if present */
    if (src->flux_confidence && src->flux_count > 0) {
        dst->flux_confidence = (uint8_t*)safe_calloc(src->flux_count, sizeof(uint8_t));
        if (dst->flux_confidence) {
            memcpy(dst->flux_confidence, src->flux_confidence, src->flux_count);
        }
    }
    
    return dst;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_ir_disk_add_track(uft_ir_disk_t* disk, uft_ir_track_t* track) {
    if (!disk || !track) return UFT_IR_ERR_INVALID;
    
    /* Check for duplicate */
    for (uint16_t i = 0; i < disk->track_count; i++) {
        if (disk->tracks[i] &&
            disk->tracks[i]->cylinder == track->cylinder &&
            disk->tracks[i]->head == track->head) {
            return UFT_IR_ERR_DUPLICATE;
        }
    }
    
    /* Check capacity */
    uint16_t max_tracks = (uint16_t)(disk->geometry.cylinders * disk->geometry.heads);
    if (disk->track_count >= max_tracks) {
        return UFT_IR_ERR_OVERFLOW;
    }
    
    /* Add track */
    disk->tracks[disk->track_count++] = track;
    
    /* Update disk statistics */
    disk->metadata.modification_time = (uint64_t)time(NULL);
    
    /* Update quality counts */
    switch (track->quality) {
        case UFT_IR_QUALITY_PERFECT:
            disk->tracks_perfect++;
            break;
        case UFT_IR_QUALITY_GOOD:
            disk->tracks_good++;
            break;
        case UFT_IR_QUALITY_DEGRADED:
        case UFT_IR_QUALITY_MARGINAL:
            disk->tracks_degraded++;
            break;
        default:
            disk->tracks_bad++;
            break;
    }
    
    /* Aggregate flags */
    disk->disk_flags |= track->flags;
    
    return UFT_IR_OK;
}

uft_ir_track_t* uft_ir_disk_get_track(const uft_ir_disk_t* disk, 
                                       uint8_t cylinder, uint8_t head) {
    if (!disk) return NULL;
    
    for (uint16_t i = 0; i < disk->track_count; i++) {
        if (disk->tracks[i] &&
            disk->tracks[i]->cylinder == cylinder &&
            disk->tracks[i]->head == head) {
            return disk->tracks[i];
        }
    }
    return NULL;
}

uft_ir_track_t* uft_ir_disk_remove_track(uft_ir_disk_t* disk,
                                          uint8_t cylinder, uint8_t head) {
    if (!disk) return NULL;
    
    for (uint16_t i = 0; i < disk->track_count; i++) {
        if (disk->tracks[i] &&
            disk->tracks[i]->cylinder == cylinder &&
            disk->tracks[i]->head == head) {
            uft_ir_track_t* removed = disk->tracks[i];
            
            /* Shift remaining tracks */
            for (uint16_t j = i; j < disk->track_count - 1; j++) {
                disk->tracks[j] = disk->tracks[j + 1];
            }
            disk->tracks[--disk->track_count] = NULL;
            
            disk->metadata.modification_time = (uint64_t)time(NULL);
            return removed;
        }
    }
    return NULL;
}

int uft_ir_track_add_revolution(uft_ir_track_t* track, uft_ir_revolution_t* rev) {
    if (!track || !rev) return -1;
    
    if (track->revolution_count >= UFT_IR_MAX_REVOLUTIONS) {
        return -1;
    }
    
    int idx = track->revolution_count;
    rev->rev_index = (uint32_t)idx;
    track->revolutions[idx] = rev;
    track->revolution_count++;
    
    return idx;
}

int uft_ir_revolution_set_flux(uft_ir_revolution_t* rev,
                                const uint32_t* deltas, uint32_t count,
                                uft_ir_data_type_t data_type) {
    if (!rev || !deltas || count == 0) return UFT_IR_ERR_INVALID;
    if (count > UFT_IR_MAX_FLUX_PER_REV) return UFT_IR_ERR_OVERFLOW;
    
    /* Allocate or reallocate flux array */
    uint32_t* new_deltas = (uint32_t*)safe_realloc(
        rev->flux_deltas, count * sizeof(uint32_t));
    if (!new_deltas) return UFT_IR_ERR_NOMEM;
    
    rev->flux_deltas = new_deltas;
    memcpy(rev->flux_deltas, deltas, count * sizeof(uint32_t));
    rev->flux_count = count;
    rev->data_size = count * sizeof(uint32_t);
    rev->data_type = data_type;
    
    /* Calculate duration */
    uint64_t total = 0;
    for (uint32_t i = 0; i < count; i++) {
        total += deltas[i];
    }
    rev->duration_ns = (uint32_t)(total & 0xFFFFFFFF);
    
    return UFT_IR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ANALYSIS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_ir_revolution_calc_stats(uft_ir_revolution_t* rev) {
    if (!rev || !rev->flux_deltas || rev->flux_count == 0) {
        return UFT_IR_ERR_INVALID;
    }
    
    uft_ir_flux_stats_t* s = &rev->stats;
    memset(s, 0, sizeof(uft_ir_flux_stats_t));
    
    s->total_transitions = rev->flux_count;
    s->min_delta_ns = UINT32_MAX;
    s->max_delta_ns = 0;
    
    /* First pass: sum, min, max, histogram */
    uint64_t sum = 0;
    memset(s->histogram_1us, 0, sizeof(s->histogram_1us));
    
    for (uint32_t i = 0; i < rev->flux_count; i++) {
        uint32_t d = rev->flux_deltas[i];
        sum += d;
        
        if (d < s->min_delta_ns) s->min_delta_ns = d;
        if (d > s->max_delta_ns) s->max_delta_ns = d;
        
        /* Histogram: bucket by microsecond (0-63µs) */
        uint32_t bucket = d / 1000;
        if (bucket < 64) {
            if (s->histogram_1us[bucket] < UINT16_MAX) {
                s->histogram_1us[bucket]++;
            }
        }
    }
    
    s->mean_delta_ns = (uint32_t)(sum / rev->flux_count);
    s->index_to_index_ns = (uint32_t)(sum & 0xFFFFFFFF);
    
    /* Second pass: standard deviation */
    uint64_t var_sum = 0;
    for (uint32_t i = 0; i < rev->flux_count; i++) {
        int64_t diff = (int64_t)rev->flux_deltas[i] - (int64_t)s->mean_delta_ns;
        var_sum += (uint64_t)(diff * diff);
    }
    double variance = (double)var_sum / (double)rev->flux_count;
    s->stddev_delta_ns = (uint32_t)sqrt(variance);
    
    /* Detect clock from histogram peaks */
    /* Find the most populated bucket (likely 1T) */
    uint16_t max_count = 0;
    uint32_t peak_bucket = 0;
    for (int i = 1; i < 64; i++) {  /* Start at 1 to skip noise */
        if (s->histogram_1us[i] > max_count) {
            max_count = s->histogram_1us[i];
            peak_bucket = (uint32_t)i;
        }
    }
    s->clock_period_ns = peak_bucket * 1000 + 500;  /* Center of bucket */
    
    return UFT_IR_OK;
}

uft_ir_encoding_t uft_ir_detect_encoding(const uft_ir_revolution_t* rev,
                                          uint8_t* confidence) {
    if (!rev || !rev->flux_deltas || rev->flux_count < 100) {
        if (confidence) *confidence = 0;
        return UFT_IR_ENC_UNKNOWN;
    }
    
    /* Build timing histogram */
    uint32_t buckets[8] = {0};  /* 2-8µs range */
    for (uint32_t i = 0; i < rev->flux_count; i++) {
        uint32_t us = rev->flux_deltas[i] / 1000;
        if (us >= 2 && us < 10) {
            buckets[us - 2]++;
        }
    }
    
    /* Analyze pattern */
    uint32_t total = 0;
    for (int i = 0; i < 8; i++) total += buckets[i];
    if (total == 0) {
        if (confidence) *confidence = 0;
        return UFT_IR_ENC_UNKNOWN;
    }
    
    /* MFM typically has peaks at 4µs, 6µs, 8µs (2T, 3T, 4T at 2µs clock) */
    /* GCR has different patterns depending on variant */
    
    uint32_t pct_4us = (buckets[2] * 100) / total;  /* 4µs = index 2 */
    uint32_t pct_6us = (buckets[4] * 100) / total;  /* 6µs = index 4 */
    uint32_t pct_8us = (buckets[6] * 100) / total;  /* 8µs = index 6 */
    
    /* Simple heuristic */
    if (pct_4us > 20 && pct_6us > 15 && pct_8us > 5) {
        /* Looks like MFM */
        if (confidence) *confidence = 80;
        return UFT_IR_ENC_MFM;
    }
    
    /* Check for GCR Commodore pattern */
    uint32_t pct_3us = (buckets[1] * 100) / total;  /* 3.25µs area */
    if (pct_3us > 30 || pct_4us > 40) {
        if (confidence) *confidence = 60;
        return UFT_IR_ENC_GCR_COMMODORE;
    }
    
    /* Check for FM (single density - longer pulses) */
    if (pct_8us > 30) {
        if (confidence) *confidence = 50;
        return UFT_IR_ENC_FM;
    }
    
    if (confidence) *confidence = 20;
    return UFT_IR_ENC_UNKNOWN;
}

int uft_ir_detect_weak_bits(uft_ir_track_t* track) {
    if (!track || track->revolution_count < 2) {
        return 0;
    }
    
    /* Need at least 2 revolutions to detect weak bits */
    /* Compare bit positions across revolutions */
    
    /* For now, placeholder - real implementation would:
     * 1. Decode each revolution to bits
     * 2. Compare bit-by-bit across revolutions
     * 3. Mark positions with variations as weak */
    
    track->weak_region_count = 0;
    return 0;
}

uint8_t uft_ir_calc_quality(uft_ir_track_t* track) {
    if (!track) return 0;
    
    uint8_t score = 100;
    
    /* Deduct for missing/bad sectors */
    if (track->sectors_expected > 0 && track->sectors_found < track->sectors_expected) {
        int missing = track->sectors_expected - track->sectors_found;
        score -= (uint8_t)(missing * 10);
    }
    
    /* Deduct for CRC errors */
    if (track->sectors_found > 0 && track->sectors_good < track->sectors_found) {
        int bad = track->sectors_found - track->sectors_good;
        score -= (uint8_t)(bad * 15);
    }
    
    /* Deduct for flags indicating problems */
    if (track->flags & UFT_IR_TF_WEAK_BITS) score -= 5;
    if (track->flags & UFT_IR_TF_INCOMPLETE) score -= 20;
    
    /* Bonus for multi-rev fusion */
    if (track->flags & UFT_IR_TF_MULTI_REV_FUSED) {
        score = (uint8_t)((score < 95) ? score + 5 : 100);
    }
    
    track->quality_score = score;
    
    /* Set quality enum based on score */
    if (score >= 95) {
        track->quality = UFT_IR_QUALITY_PERFECT;
    } else if (score >= 80) {
        track->quality = UFT_IR_QUALITY_GOOD;
    } else if (score >= 60) {
        track->quality = UFT_IR_QUALITY_DEGRADED;
    } else if (score >= 40) {
        track->quality = UFT_IR_QUALITY_MARGINAL;
    } else if (score > 0) {
        track->quality = UFT_IR_QUALITY_BAD;
    } else {
        track->quality = UFT_IR_QUALITY_UNREADABLE;
    }
    
    return score;
}

int uft_ir_find_best_revolution(const uft_ir_track_t* track) {
    if (!track || track->revolution_count == 0) {
        return -1;
    }
    
    int best_idx = 0;
    uint8_t best_score = 0;
    
    for (int i = 0; i < track->revolution_count; i++) {
        if (track->revolutions[i] && 
            track->revolutions[i]->quality_score > best_score) {
            best_score = track->revolutions[i]->quality_score;
            best_idx = i;
        }
    }
    
    return best_idx;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SERIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_ir_disk_save(const uft_ir_disk_t* disk, const char* path,
                      uft_ir_compression_t compression) {
    if (!disk || !path) return UFT_IR_ERR_INVALID;
    
    FILE* fp = fopen(path, "wb");
    if (!fp) return UFT_IR_ERR_IO;
    
    /* Write file header */
    uft_ir_file_header_t header;
    memset(&header, 0, sizeof(header));
    memcpy(header.magic, UFT_IR_MAGIC_BYTES, 8);
    header.version = UFT_IR_VERSION;
    header.header_size = sizeof(uft_ir_file_header_t);
    header.compression = compression;
    header.track_count = disk->track_count;
    
    /* Calculate sizes (uncompressed for now) */
    header.uncompressed_size = sizeof(uft_ir_file_header_t);
    header.uncompressed_size += disk->track_count * sizeof(uft_ir_track_header_t);
    
    /* Header CRC */
    header.crc32 = calc_crc32(&header, sizeof(header) - sizeof(uint32_t));
    
    if (fwrite(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return UFT_IR_ERR_IO;
    }
    
    /* Write geometry */
    if (fwrite(&disk->geometry, sizeof(uft_ir_geometry_t), 1, fp) != 1) {
        fclose(fp);
        return UFT_IR_ERR_IO;
    }
    
    /* Write track headers and data */
    uint32_t data_offset = sizeof(uft_ir_file_header_t) + 
                           sizeof(uft_ir_geometry_t) +
                           disk->track_count * sizeof(uft_ir_track_header_t);
    
    for (uint16_t i = 0; i < disk->track_count; i++) {
        const uft_ir_track_t* track = disk->tracks[i];
        if (!track) continue;
        
        uft_ir_track_header_t thdr;
        memset(&thdr, 0, sizeof(thdr));
        thdr.cylinder = track->cylinder;
        thdr.head = track->head;
        thdr.flags = track->flags;
        thdr.revolution_count = track->revolution_count;
        thdr.encoding = (uint8_t)track->encoding;
        thdr.quality = (uint8_t)track->quality;
        thdr.data_offset = data_offset;
        
        /* Calculate track data size */
        uint32_t track_size = 0;
        for (int r = 0; r < track->revolution_count; r++) {
            if (track->revolutions[r]) {
                track_size += track->revolutions[r]->data_size;
            }
        }
        thdr.data_size = track_size;
        thdr.uncompressed_size = track_size;
        thdr.crc32 = 0;  /* TODO: Calculate */
        
        if (fwrite(&thdr, sizeof(thdr), 1, fp) != 1) {
            fclose(fp);
            return UFT_IR_ERR_IO;
        }
        
        data_offset += track_size;
    }
    
    /* Write track data */
    for (uint16_t i = 0; i < disk->track_count; i++) {
        const uft_ir_track_t* track = disk->tracks[i];
        if (!track) continue;
        
        for (int r = 0; r < track->revolution_count; r++) {
            const uft_ir_revolution_t* rev = track->revolutions[r];
            if (!rev || !rev->flux_deltas) continue;
            
            if (fwrite(rev->flux_deltas, sizeof(uint32_t), 
                       rev->flux_count, fp) != rev->flux_count) {
                fclose(fp);
                return UFT_IR_ERR_IO;
            }
        }
    }
    
    fclose(fp);
    return UFT_IR_OK;
}

int uft_ir_disk_load(const char* path, uft_ir_disk_t** disk_out) {
    if (!path || !disk_out) return UFT_IR_ERR_INVALID;
    
    *disk_out = NULL;
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_IR_ERR_IO;
    
    /* Read and validate header */
    uft_ir_file_header_t header;
    if (fread(&header, sizeof(header), 1, fp) != 1) {
        fclose(fp);
        return UFT_IR_ERR_IO;
    }
    
    if (memcmp(header.magic, UFT_IR_MAGIC_BYTES, 8) != 0) {
        fclose(fp);
        return UFT_IR_ERR_FORMAT;
    }
    
    if ((header.version >> 16) != UFT_IR_VERSION_MAJOR) {
        fclose(fp);
        return UFT_IR_ERR_VERSION;
    }
    
    /* Read geometry */
    uft_ir_geometry_t geo;
    if (fread(&geo, sizeof(geo), 1, fp) != 1) {
        fclose(fp);
        return UFT_IR_ERR_IO;
    }
    
    /* Create disk */
    uft_ir_disk_t* disk = uft_ir_disk_create(geo.cylinders, geo.heads);
    if (!disk) {
        fclose(fp);
        return UFT_IR_ERR_NOMEM;
    }
    
    memcpy(&disk->geometry, &geo, sizeof(geo));
    
    /* Read track headers */
    uft_ir_track_header_t* thdrs = (uft_ir_track_header_t*)safe_calloc(
        header.track_count, sizeof(uft_ir_track_header_t));
    if (!thdrs) {
        uft_ir_disk_free(disk);
        fclose(fp);
        return UFT_IR_ERR_NOMEM;
    }
    
    for (uint32_t i = 0; i < header.track_count; i++) {
        if (fread(&thdrs[i], sizeof(uft_ir_track_header_t), 1, fp) != 1) {
            free(thdrs);
            uft_ir_disk_free(disk);
            fclose(fp);
            return UFT_IR_ERR_IO;
        }
    }
    
    /* Read track data */
    for (uint32_t i = 0; i < header.track_count; i++) {
        uft_ir_track_t* track = uft_ir_track_create(thdrs[i].cylinder, thdrs[i].head);
        if (!track) continue;
        
        track->flags = thdrs[i].flags;
        track->encoding = (uft_ir_encoding_t)thdrs[i].encoding;
        track->quality = (uft_ir_quality_t)thdrs[i].quality;
        
        /* Seek to data offset */
        if (fseek(fp, (long)thdrs[i].data_offset, SEEK_SET) != 0) {
            uft_ir_track_free(track);
            continue;
        }
        
        /* Read flux data for each revolution */
        uint32_t remaining = thdrs[i].data_size;
        for (int r = 0; r < thdrs[i].revolution_count && remaining > 0; r++) {
            /* Estimate flux count (data_size / sizeof(uint32_t) / rev_count) */
            uint32_t flux_count = remaining / sizeof(uint32_t) / 
                                  (thdrs[i].revolution_count - r);
            if (flux_count == 0) flux_count = 1000;  /* Fallback */
            
            uft_ir_revolution_t* rev = uft_ir_revolution_create(flux_count);
            if (!rev) break;
            
            size_t read = fread(rev->flux_deltas, sizeof(uint32_t), 
                               flux_count, fp);
            rev->flux_count = (uint32_t)read;
            rev->data_size = (uint32_t)(read * sizeof(uint32_t));
            remaining -= rev->data_size;
            
            uft_ir_track_add_revolution(track, rev);
        }
        
        uft_ir_disk_add_track(disk, track);
    }
    
    free(thdrs);
    fclose(fp);
    
    *disk_out = disk;
    return UFT_IR_OK;
}

int uft_ir_track_serialize(const uft_ir_track_t* track,
                            uint8_t** buffer, size_t* size,
                            uft_ir_compression_t compression) {
    if (!track || !buffer || !size) return UFT_IR_ERR_INVALID;
    
    /* Calculate uncompressed data size */
    size_t data_size = 0;
    for (int r = 0; r < track->revolution_count; r++) {
        if (track->revolutions[r]) {
            data_size += track->revolutions[r]->data_size;
        }
    }
    
    /* Prepare uncompressed data */
    uint8_t* raw_data = NULL;
    if (data_size > 0) {
        raw_data = (uint8_t*)malloc(data_size);
        if (!raw_data) return UFT_IR_ERR_NOMEM;
        
        uint8_t* ptr = raw_data;
        for (int r = 0; r < track->revolution_count; r++) {
            const uft_ir_revolution_t* rev = track->revolutions[r];
            if (!rev || !rev->flux_deltas) continue;
            
            memcpy(ptr, rev->flux_deltas, rev->data_size);
            ptr += rev->data_size;
        }
    }
    
    /* Apply compression */
    uint8_t* comp_data = NULL;
    size_t comp_size = 0;
    uft_ir_compression_t used_comp = UFT_IR_COMP_NONE;
    
    if (compression != UFT_IR_COMP_NONE && data_size > 32) {
        size_t max_comp = data_size + 256;  /* Some overhead */
        comp_data = (uint8_t*)malloc(max_comp);
        
        if (comp_data) {
            switch (compression) {
                case UFT_IR_COMP_RLE:
                    comp_size = ir_compress_rle(raw_data, data_size, comp_data, max_comp);
                    if (comp_size > 0) used_comp = UFT_IR_COMP_RLE;
                    break;
                    
                case UFT_IR_COMP_DELTA:
                    comp_size = ir_compress_delta(raw_data, data_size, comp_data, max_comp);
                    if (comp_size > 0 && comp_size < data_size) {
                        used_comp = UFT_IR_COMP_DELTA;
                    } else {
                        comp_size = 0;  /* No benefit */
                    }
                    break;
                    
                case UFT_IR_COMP_ZLIB:
                case UFT_IR_COMP_LZ4:
                case UFT_IR_COMP_ZSTD:
                    /* External libraries not implemented - fall through to none */
                    comp_size = 0;
                    break;
                    
                default:
                    break;
            }
            
            if (comp_size == 0) {
                free(comp_data);
                comp_data = NULL;
            }
        }
    }
    
    /* Calculate total size */
    size_t final_data_size = (comp_size > 0) ? comp_size : data_size;
    size_t total_size = sizeof(uft_ir_track_header_t) + final_data_size;
    
    uint8_t* buf = (uint8_t*)safe_calloc(1, total_size);
    if (!buf) {
        free(raw_data);
        free(comp_data);
        return UFT_IR_ERR_NOMEM;
    }
    
    /* Write header */
    uft_ir_track_header_t* hdr = (uft_ir_track_header_t*)buf;
    hdr->cylinder = track->cylinder;
    hdr->head = track->head;
    hdr->flags = track->flags;
    hdr->revolution_count = track->revolution_count;
    hdr->encoding = (uint8_t)track->encoding;
    hdr->quality = (uint8_t)track->quality;
    hdr->data_offset = sizeof(uft_ir_track_header_t);
    hdr->data_size = (uint32_t)final_data_size;
    hdr->compression = (uint8_t)used_comp;
    hdr->uncompressed_size = (uint32_t)data_size;
    
    /* Write data */
    if (comp_size > 0) {
        memcpy(buf + sizeof(uft_ir_track_header_t), comp_data, comp_size);
    } else if (data_size > 0) {
        memcpy(buf + sizeof(uft_ir_track_header_t), raw_data, data_size);
    }
    
    free(raw_data);
    free(comp_data);
    
    *buffer = buf;
    *size = total_size;
    return UFT_IR_OK;
}

int uft_ir_track_deserialize(const uint8_t* buffer, size_t size,
                              uft_ir_track_t** track_out) {
    if (!buffer || size < sizeof(uft_ir_track_header_t) || !track_out) {
        return UFT_IR_ERR_INVALID;
    }
    
    const uft_ir_track_header_t* hdr = (const uft_ir_track_header_t*)buffer;
    
    uft_ir_track_t* track = uft_ir_track_create(hdr->cylinder, hdr->head);
    if (!track) return UFT_IR_ERR_NOMEM;
    
    track->flags = hdr->flags;
    track->encoding = (uft_ir_encoding_t)hdr->encoding;
    track->quality = (uft_ir_quality_t)hdr->quality;
    
    /* Get compressed data */
    const uint8_t* comp_data = buffer + sizeof(uft_ir_track_header_t);
    size_t comp_size = size - sizeof(uft_ir_track_header_t);
    
    /* Decompress if needed */
    uint8_t* flux_data = NULL;
    size_t flux_size = 0;
    bool need_free = false;
    
    uft_ir_compression_t compression = (uft_ir_compression_t)hdr->compression;
    size_t uncompressed_size = hdr->uncompressed_size > 0 ? 
                               hdr->uncompressed_size : comp_size;
    
    if (compression != UFT_IR_COMP_NONE && uncompressed_size > 0) {
        flux_data = (uint8_t*)malloc(uncompressed_size);
        if (!flux_data) {
            uft_ir_track_free(track);
            return UFT_IR_ERR_NOMEM;
        }
        need_free = true;
        
        switch (compression) {
            case UFT_IR_COMP_RLE:
                flux_size = ir_decompress_rle(comp_data, comp_size, 
                                              flux_data, uncompressed_size);
                break;
                
            case UFT_IR_COMP_DELTA:
                flux_size = ir_decompress_delta(comp_data, comp_size,
                                                flux_data, uncompressed_size, true);
                break;
                
            default:
                /* Unknown compression - try raw */
                flux_size = comp_size < uncompressed_size ? comp_size : uncompressed_size;
                memcpy(flux_data, comp_data, flux_size);
                break;
        }
        
        if (flux_size == 0) {
            free(flux_data);
            uft_ir_track_free(track);
            return UFT_IR_ERR_CORRUPT;
        }
    } else {
        flux_data = (uint8_t*)comp_data;
        flux_size = comp_size;
    }
    
    /* Read flux data into revolutions */
    const uint8_t* ptr = flux_data;
    size_t remaining = flux_size;
    
    for (int r = 0; r < hdr->revolution_count && remaining >= sizeof(uint32_t); r++) {
        uint32_t flux_count = (uint32_t)(remaining / sizeof(uint32_t) / 
                              (hdr->revolution_count - r));
        if (flux_count == 0) break;
        
        uft_ir_revolution_t* rev = uft_ir_revolution_create(flux_count);
        if (!rev) break;
        
        memcpy(rev->flux_deltas, ptr, flux_count * sizeof(uint32_t));
        rev->flux_count = flux_count;
        rev->data_size = flux_count * sizeof(uint32_t);
        
        ptr += rev->data_size;
        remaining -= rev->data_size;
        
        uft_ir_track_add_revolution(track, rev);
    }
    
    if (need_free) {
        free(flux_data);
    }
    
    *track_out = track;
    return UFT_IR_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t uft_ir_get_nominal_bitcell(uft_ir_encoding_t encoding, uint32_t rpm) {
    /* Standard bitcell times for 300 RPM */
    uint32_t bitcell_300 = 0;
    
    switch (encoding) {
        case UFT_IR_ENC_FM:
            bitcell_300 = 4000;  /* 4µs = 125kbps */
            break;
        case UFT_IR_ENC_MFM:
        case UFT_IR_ENC_AMIGA_MFM:
            bitcell_300 = 2000;  /* 2µs = 250kbps DD */
            break;
        case UFT_IR_ENC_GCR_COMMODORE:
            /* Variable by zone, use average */
            bitcell_300 = 3200;  /* ~3.25µs average */
            break;
        case UFT_IR_ENC_GCR_APPLE:
            bitcell_300 = 4000;  /* Apple II: 4µs */
            break;
        case UFT_IR_ENC_GCR_APPLE_35:
            /* Variable by zone */
            bitcell_300 = 2000;  /* Innermost tracks */
            break;
        case UFT_IR_ENC_GCR_VICTOR:
            bitcell_300 = 1667;  /* Victor 9000: ~600kbps */
            break;
        case UFT_IR_ENC_M2FM:
            bitcell_300 = 2000;
            break;
        default:
            bitcell_300 = 2000;  /* Default to DD MFM */
            break;
    }
    
    /* Adjust for RPM (higher RPM = shorter bitcell) */
    if (rpm == 0) rpm = 300;
    return (bitcell_300 * 300) / rpm;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VALIDATION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_ir_disk_validate(const uft_ir_disk_t* disk, 
                          char** errors, int max_errors) {
    int err_count = 0;
    
    if (!disk) {
        if (errors && max_errors > 0) {
            errors[err_count] = "Null disk pointer";
        }
        return 1;
    }
    
    /* Check magic */
    if (disk->magic != UFT_IR_MAGIC) {
        if (errors && err_count < max_errors) {
            errors[err_count] = "Invalid magic number";
        }
        err_count++;
    }
    
    /* Check geometry */
    if (disk->geometry.cylinders > UFT_IR_MAX_CYLINDERS) {
        if (errors && err_count < max_errors) {
            errors[err_count] = "Cylinder count exceeds maximum";
        }
        err_count++;
    }
    
    if (disk->geometry.heads > UFT_IR_MAX_HEADS) {
        if (errors && err_count < max_errors) {
            errors[err_count] = "Head count exceeds maximum";
        }
        err_count++;
    }
    
    /* Validate each track */
    for (uint16_t i = 0; i < disk->track_count; i++) {
        int track_err = uft_ir_track_validate(disk->tracks[i]);
        if (track_err != UFT_IR_OK) {
            if (errors && err_count < max_errors) {
                errors[err_count] = "Track validation failed";
            }
            err_count++;
        }
    }
    
    return err_count;
}

int uft_ir_track_validate(const uft_ir_track_t* track) {
    if (!track) return UFT_IR_ERR_INVALID;
    
    /* Check cylinder/head bounds */
    if (track->cylinder > UFT_IR_MAX_CYLINDERS) {
        return UFT_IR_ERR_INVALID;
    }
    if (track->head > UFT_IR_MAX_HEADS) {
        return UFT_IR_ERR_INVALID;
    }
    
    /* Check revolution count */
    if (track->revolution_count > UFT_IR_MAX_REVOLUTIONS) {
        return UFT_IR_ERR_OVERFLOW;
    }
    
    /* Validate revolutions */
    for (int i = 0; i < track->revolution_count; i++) {
        const uft_ir_revolution_t* rev = track->revolutions[i];
        if (!rev) continue;
        
        if (rev->flux_count > UFT_IR_MAX_FLUX_PER_REV) {
            return UFT_IR_ERR_OVERFLOW;
        }
        
        if (rev->flux_count > 0 && !rev->flux_deltas) {
            return UFT_IR_ERR_INVALID;
        }
    }
    
    return UFT_IR_OK;
}

int uft_ir_is_uft_ir_file(const char* path) {
    if (!path) return -1;
    
    FILE* fp = fopen(path, "rb");
    if (!fp) return -1;
    
    uint8_t magic[8];
    size_t read = fread(magic, 1, 8, fp);
    fclose(fp);
    
    if (read != 8) return -1;
    
    return (memcmp(magic, UFT_IR_MAGIC_BYTES, 8) == 0) ? 1 : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * EXPORT/REPORT
 * ═══════════════════════════════════════════════════════════════════════════ */

/** String buffer helper */
typedef struct {
    char* data;
    size_t len;
    size_t cap;
} strbuf_t;

static void strbuf_init(strbuf_t* sb) {
    sb->cap = 4096;
    sb->data = (char*)safe_calloc(1, sb->cap);
    sb->len = 0;
    if (sb->data) sb->data[0] = '\0';
}

static void strbuf_append(strbuf_t* sb, const char* fmt, ...) {
    if (!sb || !sb->data) return;
    
    va_list args;
    va_start(args, fmt);
    
    size_t remain = sb->cap - sb->len;
    int needed = vsnprintf(sb->data + sb->len, remain, fmt, args);
    
    va_end(args);
    
    if (needed >= (int)remain) {
        /* Need to grow */
        size_t new_cap = sb->cap * 2 + (size_t)needed;
        char* new_data = (char*)safe_realloc(sb->data, new_cap);
        if (!new_data) return;
        
        sb->data = new_data;
        sb->cap = new_cap;
        
        va_start(args, fmt);
        vsnprintf(sb->data + sb->len, sb->cap - sb->len, fmt, args);
        va_end(args);
    }
    
    sb->len += (size_t)needed;
}

char* uft_ir_disk_to_json(const uft_ir_disk_t* disk, bool include_flux) {
    if (!disk) return NULL;
    
    strbuf_t sb;
    strbuf_init(&sb);
    if (!sb.data) return NULL;
    
    strbuf_append(&sb, "{\n");
    strbuf_append(&sb, "  \"format\": \"UFT-IR\",\n");
    strbuf_append(&sb, "  \"version\": \"%d.%d.%d\",\n",
                  UFT_IR_VERSION_MAJOR, UFT_IR_VERSION_MINOR, UFT_IR_VERSION_PATCH);
    
    /* Geometry */
    strbuf_append(&sb, "  \"geometry\": {\n");
    strbuf_append(&sb, "    \"cylinders\": %d,\n", disk->geometry.cylinders);
    strbuf_append(&sb, "    \"heads\": %d,\n", disk->geometry.heads);
    strbuf_append(&sb, "    \"sectors_per_track\": %d,\n", disk->geometry.sectors_per_track);
    strbuf_append(&sb, "    \"rpm\": %u\n", disk->geometry.rpm);
    strbuf_append(&sb, "  },\n");
    
    /* Metadata */
    strbuf_append(&sb, "  \"metadata\": {\n");
    strbuf_append(&sb, "    \"source\": \"%s\",\n", disk->metadata.source_name);
    strbuf_append(&sb, "    \"title\": \"%s\",\n", disk->metadata.title);
    strbuf_append(&sb, "    \"platform\": \"%s\"\n", disk->metadata.platform);
    strbuf_append(&sb, "  },\n");
    
    /* Quality summary */
    strbuf_append(&sb, "  \"quality\": {\n");
    strbuf_append(&sb, "    \"tracks_perfect\": %u,\n", disk->tracks_perfect);
    strbuf_append(&sb, "    \"tracks_good\": %u,\n", disk->tracks_good);
    strbuf_append(&sb, "    \"tracks_degraded\": %u,\n", disk->tracks_degraded);
    strbuf_append(&sb, "    \"tracks_bad\": %u\n", disk->tracks_bad);
    strbuf_append(&sb, "  },\n");
    
    /* Tracks */
    strbuf_append(&sb, "  \"track_count\": %u,\n", disk->track_count);
    strbuf_append(&sb, "  \"tracks\": [\n");
    
    for (uint16_t i = 0; i < disk->track_count; i++) {
        const uft_ir_track_t* t = disk->tracks[i];
        if (!t) continue;
        
        strbuf_append(&sb, "    {\n");
        strbuf_append(&sb, "      \"cylinder\": %d,\n", t->cylinder);
        strbuf_append(&sb, "      \"head\": %d,\n", t->head);
        strbuf_append(&sb, "      \"encoding\": %d,\n", t->encoding);
        strbuf_append(&sb, "      \"quality_score\": %d,\n", t->quality_score);
        strbuf_append(&sb, "      \"revolutions\": %d", t->revolution_count);
        
        if (include_flux && t->revolution_count > 0 && t->revolutions[0]) {
            strbuf_append(&sb, ",\n      \"flux_count\": %u",
                         t->revolutions[0]->flux_count);
        }
        
        strbuf_append(&sb, "\n    }%s\n", (i < disk->track_count - 1) ? "," : "");
    }
    
    strbuf_append(&sb, "  ]\n");
    strbuf_append(&sb, "}\n");
    
    return sb.data;
}

char* uft_ir_track_to_json(const uft_ir_track_t* track, bool include_flux) {
    if (!track) return NULL;
    
    strbuf_t sb;
    strbuf_init(&sb);
    if (!sb.data) return NULL;
    
    strbuf_append(&sb, "{\n");
    strbuf_append(&sb, "  \"cylinder\": %d,\n", track->cylinder);
    strbuf_append(&sb, "  \"head\": %d,\n", track->head);
    strbuf_append(&sb, "  \"encoding\": %d,\n", track->encoding);
    strbuf_append(&sb, "  \"flags\": %u,\n", track->flags);
    strbuf_append(&sb, "  \"quality\": %d,\n", track->quality);
    strbuf_append(&sb, "  \"quality_score\": %d,\n", track->quality_score);
    strbuf_append(&sb, "  \"sectors_expected\": %d,\n", track->sectors_expected);
    strbuf_append(&sb, "  \"sectors_found\": %d,\n", track->sectors_found);
    strbuf_append(&sb, "  \"sectors_good\": %d,\n", track->sectors_good);
    strbuf_append(&sb, "  \"bitcell_ns\": %u,\n", track->bitcell_ns);
    strbuf_append(&sb, "  \"rpm_measured\": %u,\n", track->rpm_measured);
    strbuf_append(&sb, "  \"revolution_count\": %d,\n", track->revolution_count);
    
    /* Revolutions */
    strbuf_append(&sb, "  \"revolutions\": [\n");
    for (int i = 0; i < track->revolution_count; i++) {
        const uft_ir_revolution_t* r = track->revolutions[i];
        if (!r) continue;
        
        strbuf_append(&sb, "    {\n");
        strbuf_append(&sb, "      \"index\": %d,\n", i);
        strbuf_append(&sb, "      \"flux_count\": %u,\n", r->flux_count);
        strbuf_append(&sb, "      \"duration_ns\": %u,\n", r->duration_ns);
        strbuf_append(&sb, "      \"quality_score\": %d", r->quality_score);
        
        if (include_flux && r->flux_count > 0 && r->flux_deltas) {
            strbuf_append(&sb, ",\n      \"flux_sample\": [");
            int show = (r->flux_count < 10) ? (int)r->flux_count : 10;
            for (int j = 0; j < show; j++) {
                strbuf_append(&sb, "%u%s", r->flux_deltas[j], 
                             (j < show - 1) ? "," : "");
            }
            strbuf_append(&sb, "]");
        }
        
        strbuf_append(&sb, "\n    }%s\n", (i < track->revolution_count - 1) ? "," : "");
    }
    strbuf_append(&sb, "  ],\n");
    
    /* Weak regions */
    strbuf_append(&sb, "  \"weak_region_count\": %u,\n", track->weak_region_count);
    strbuf_append(&sb, "  \"protection_count\": %u\n", track->protection_count);
    
    strbuf_append(&sb, "}\n");
    
    return sb.data;
}

char* uft_ir_disk_summary(const uft_ir_disk_t* disk) {
    if (!disk) return NULL;
    
    strbuf_t sb;
    strbuf_init(&sb);
    if (!sb.data) return NULL;
    
    strbuf_append(&sb, "═══════════════════════════════════════════════════════════════\n");
    strbuf_append(&sb, "                    UFT-IR DISK SUMMARY\n");
    strbuf_append(&sb, "═══════════════════════════════════════════════════════════════\n\n");
    
    strbuf_append(&sb, "FORMAT:     UFT-IR v%d.%d.%d\n",
                  UFT_IR_VERSION_MAJOR, UFT_IR_VERSION_MINOR, UFT_IR_VERSION_PATCH);
    
    if (disk->metadata.title[0]) {
        strbuf_append(&sb, "TITLE:      %s\n", disk->metadata.title);
    }
    if (disk->metadata.platform[0]) {
        strbuf_append(&sb, "PLATFORM:   %s\n", disk->metadata.platform);
    }
    if (disk->metadata.source_name[0]) {
    }
    
    strbuf_append(&sb, "\n── GEOMETRY ──────────────────────────────────────────────────\n");
    strbuf_append(&sb, "Cylinders:  %d\n", disk->geometry.cylinders);
    strbuf_append(&sb, "Heads:      %d\n", disk->geometry.heads);
    strbuf_append(&sb, "RPM:        %u\n", disk->geometry.rpm);
    strbuf_append(&sb, "Density:    %s\n", 
                  disk->geometry.density == 0 ? "SD" :
                  disk->geometry.density == 1 ? "DD" :
                  disk->geometry.density == 2 ? "HD" : "ED");
    
    strbuf_append(&sb, "\n── QUALITY ───────────────────────────────────────────────────\n");
    strbuf_append(&sb, "Total Tracks: %u\n", disk->track_count);
    strbuf_append(&sb, "  Perfect:    %u\n", disk->tracks_perfect);
    strbuf_append(&sb, "  Good:       %u\n", disk->tracks_good);
    strbuf_append(&sb, "  Degraded:   %u\n", disk->tracks_degraded);
    strbuf_append(&sb, "  Bad:        %u\n", disk->tracks_bad);
    
    uint32_t quality_pct = 0;
    if (disk->track_count > 0) {
        quality_pct = ((disk->tracks_perfect + disk->tracks_good) * 100) / 
                      disk->track_count;
    }
    strbuf_append(&sb, "  Overall:    %u%% readable\n", quality_pct);
    
    /* Flags */
    if (disk->disk_flags) {
        strbuf_append(&sb, "\n── FLAGS ─────────────────────────────────────────────────────\n");
        if (disk->disk_flags & UFT_IR_TF_WEAK_BITS)
            strbuf_append(&sb, "  • Weak bits detected\n");
        if (disk->disk_flags & UFT_IR_TF_PROTECTED)
            strbuf_append(&sb, "  • Copy protection detected\n");
        if (disk->disk_flags & UFT_IR_TF_LONG_TRACK)
            strbuf_append(&sb, "  • Long tracks present\n");
        if (disk->disk_flags & UFT_IR_TF_HALF_TRACK)
            strbuf_append(&sb, "  • Half-tracks present\n");
    }
    
    strbuf_append(&sb, "\n═══════════════════════════════════════════════════════════════\n");
    
    return sb.data;
}

char* uft_ir_track_summary(const uft_ir_track_t* track) {
    if (!track) return NULL;
    
    strbuf_t sb;
    strbuf_init(&sb);
    if (!sb.data) return NULL;
    
    strbuf_append(&sb, "Track C%02d.H%d: ", track->cylinder, track->head);
    
    /* Encoding */
    const char* enc_names[] = {
        "Unknown", "FM", "MFM", "M2FM", "GCR-C64", "GCR-Apple",
        "GCR-Apple3.5", "GCR-Victor", "Amiga-MFM", "RLL", "Mixed"
    };
    int enc_idx = (int)track->encoding;
    if (enc_idx < 0 || enc_idx > 10) enc_idx = 0;
    strbuf_append(&sb, "%s, ", enc_names[enc_idx]);
    
    /* Quality */
    const char* qual_names[] = {
        "?", "PERFECT", "GOOD", "DEGRADED", "MARGINAL", 
        "BAD", "UNREADABLE", "EMPTY", "PROTECTED"
    };
    int qual_idx = (int)track->quality;
    if (qual_idx < 0 || qual_idx > 8) qual_idx = 0;
    strbuf_append(&sb, "%s (%d%%), ", qual_names[qual_idx], track->quality_score);
    
    /* Sectors */
    strbuf_append(&sb, "Sectors: %d/%d OK, ", 
                  track->sectors_good, track->sectors_expected);
    
    /* Revolutions */
    strbuf_append(&sb, "%d revs", track->revolution_count);
    
    /* Flags */
    if (track->flags & UFT_IR_TF_WEAK_BITS) strbuf_append(&sb, " [WEAK]");
    if (track->flags & UFT_IR_TF_PROTECTED) strbuf_append(&sb, " [PROT]");
    if (track->flags & UFT_IR_TF_CRC_CORRECTED) strbuf_append(&sb, " [CORR]");
    
    strbuf_append(&sb, "\n");
    
    return sb.data;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR MESSAGES
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_ir_strerror(int err) {
    switch (err) {
        case UFT_IR_OK:             return "Success";
        case UFT_IR_ERR_NOMEM:      return "Out of memory";
        case UFT_IR_ERR_INVALID:    return "Invalid parameter";
        case UFT_IR_ERR_OVERFLOW:   return "Buffer overflow";
        case UFT_IR_ERR_IO:         return "I/O error";
        case UFT_IR_ERR_FORMAT:     return "Invalid format";
        case UFT_IR_ERR_VERSION:    return "Unsupported version";
        case UFT_IR_ERR_CHECKSUM:   return "Checksum mismatch";
        case UFT_IR_ERR_COMPRESSION:return "Compression error";
        case UFT_IR_ERR_NOT_FOUND:  return "Not found";
        case UFT_IR_ERR_DUPLICATE:  return "Duplicate entry";
        default:                    return "Unknown error";
    }
}

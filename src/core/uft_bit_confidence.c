/**
 * @file uft_bit_confidence.c
 * @brief Per-Bit Confidence System Implementation
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_bit_confidence.h"
#include "uft/uft_flux_statistics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Initialization
 *===========================================================================*/

void uft_bitconf_init(uft_bit_confidence_t *conf) {
    if (!conf) return;
    memset(conf, 0, sizeof(*conf));
    conf->confidence = UFT_BITCONF_NONE;
}

/*===========================================================================
 * Confidence Calculation
 *===========================================================================*/

uint8_t uft_bitconf_from_timing(uint16_t timing_ns, uint16_t expected_ns,
                                 uint8_t tolerance_pct) {
    if (expected_ns == 0) return UFT_BITCONF_NONE;
    
    /* Calculate percentage difference */
    int32_t diff = (int32_t)timing_ns - (int32_t)expected_ns;
    if (diff < 0) diff = -diff;
    
    uint32_t pct_diff = (diff * 100) / expected_ns;
    
    /* Perfect timing = 100% confidence */
    if (pct_diff == 0) return UFT_BITCONF_CERTAIN;
    
    /* Within tolerance = high confidence */
    if (pct_diff <= tolerance_pct) {
        return UFT_BITCONF_CERTAIN - (pct_diff * (UFT_BITCONF_CERTAIN - UFT_BITCONF_HIGH) / tolerance_pct);
    }
    
    /* Beyond tolerance = degraded confidence */
    if (pct_diff <= tolerance_pct * 2) {
        return UFT_BITCONF_HIGH - ((pct_diff - tolerance_pct) * (UFT_BITCONF_HIGH - UFT_BITCONF_GOOD) / tolerance_pct);
    }
    
    if (pct_diff <= tolerance_pct * 4) {
        return UFT_BITCONF_GOOD - ((pct_diff - tolerance_pct * 2) * (UFT_BITCONF_GOOD - UFT_BITCONF_MARGINAL) / (tolerance_pct * 2));
    }
    
    if (pct_diff <= tolerance_pct * 8) {
        return UFT_BITCONF_MARGINAL - ((pct_diff - tolerance_pct * 4) * (UFT_BITCONF_MARGINAL - UFT_BITCONF_LOW) / (tolerance_pct * 4));
    }
    
    return UFT_BITCONF_LOW;
}

uint8_t uft_bitconf_from_multirev(uint8_t ones_count, uint8_t zeros_count,
                                   uint8_t *value) {
    uint8_t total = ones_count + zeros_count;
    
    if (total == 0) {
        if (value) *value = 0;
        return UFT_BITCONF_NONE;
    }
    
    /* Determine best value */
    uint8_t best;
    uint8_t best_count;
    
    if (ones_count >= zeros_count) {
        best = 1;
        best_count = ones_count;
    } else {
        best = 0;
        best_count = zeros_count;
    }
    
    if (value) *value = best;
    
    /* Calculate consistency */
    /* 100% = all reads agree */
    /* 50% = perfect split */
    uint8_t consistency = (best_count * 100) / total;
    
    /* Scale to confidence */
    if (consistency == 100) {
        /* All reads agree */
        if (total >= 3) return UFT_BITCONF_CERTAIN;
        if (total == 2) return UFT_BITCONF_HIGH;
        return UFT_BITCONF_GOOD;
    }
    
    if (consistency >= 80) {
        return UFT_BITCONF_HIGH - (100 - consistency);
    }
    
    if (consistency >= 60) {
        return UFT_BITCONF_GOOD - (80 - consistency);
    }
    
    /* Near 50/50 split = very low confidence */
    return UFT_BITCONF_LOW + (consistency - 50) / 2;
}

uint8_t uft_bitconf_from_pll(uint8_t pll_phase, uint8_t pll_lock_quality,
                              uint8_t pll_status) {
    /* Start with lock quality as base */
    uint8_t confidence = pll_lock_quality;
    
    /* Penalize for status issues */
    if (pll_status & UFT_PLL_STATUS_SLIP) {
        confidence = (confidence > 20) ? confidence - 20 : 0;
    }
    if (pll_status & UFT_PLL_STATUS_LOST) {
        confidence = (confidence > 40) ? confidence - 40 : 0;
    }
    if (pll_status & UFT_PLL_STATUS_REACQUIRE) {
        confidence = (confidence > 10) ? confidence - 10 : 0;
    }
    
    /* Phase near 0 or 255 is best (centered) */
    /* Phase near 128 is worst (edge of window) */
    uint8_t phase_penalty = 0;
    if (pll_phase > 64 && pll_phase < 192) {
        /* In middle range - calculate penalty */
        uint8_t dist_from_edge = (pll_phase < 128) ? 
                                  (pll_phase - 64) : (192 - pll_phase);
        phase_penalty = dist_from_edge / 4; /* Max 16 penalty */
    }
    
    if (confidence > phase_penalty) {
        confidence -= phase_penalty;
    } else {
        confidence = 0;
    }
    
    return confidence;
}

uint8_t uft_bitconf_fuse(const uft_confidence_source_t *sources, uint8_t count,
                          const uft_confidence_params_t *params) {
    if (!sources || count == 0) return UFT_BITCONF_NONE;
    
    const uft_confidence_params_t *p = params ? params : &UFT_CONFIDENCE_PARAMS_DEFAULT;
    (void)p; /* Used for future weighted fusion */
    
    /* Weighted average of all sources */
    uint32_t weighted_sum = 0;
    uint32_t weight_sum = 0;
    
    for (uint8_t i = 0; i < count; i++) {
        if (sources[i].weight > 0) {
            weighted_sum += (uint32_t)sources[i].confidence * sources[i].weight;
            weight_sum += sources[i].weight;
        }
    }
    
    if (weight_sum == 0) return UFT_BITCONF_NONE;
    
    return (uint8_t)(weighted_sum / weight_sum);
}

void uft_bitconf_update(uft_bit_confidence_t *conf,
                         const uft_confidence_source_t *source,
                         const uft_confidence_params_t *params) {
    if (!conf || !source) return;
    
    /* Add source to list if space */
    if (conf->source_count < 4) {
        conf->sources[conf->source_count++] = *source;
    } else {
        /* Replace lowest weight source */
        uint8_t min_idx = 0;
        uint8_t min_weight = conf->sources[0].weight;
        for (uint8_t i = 1; i < 4; i++) {
            if (conf->sources[i].weight < min_weight) {
                min_weight = conf->sources[i].weight;
                min_idx = i;
            }
        }
        if (source->weight > min_weight) {
            conf->sources[min_idx] = *source;
        }
    }
    
    /* Update source flags */
    conf->source_flags |= source->source_flags;
    
    /* Recalculate overall confidence */
    conf->confidence = uft_bitconf_fuse(conf->sources, conf->source_count, params);
}

bool uft_bitconf_add_alternative(uft_bit_confidence_t *conf,
                                  uint8_t value,
                                  uint8_t confidence,
                                  uint16_t source_flags) {
    if (!conf || conf->alt_count >= UFT_BITCONF_MAX_ALTERNATIVES) return false;
    
    /* Don't add if same as primary value with lower confidence */
    if (value == conf->value && confidence <= conf->confidence) return false;
    
    /* Add alternative */
    conf->alternatives[conf->alt_count].value = value;
    conf->alternatives[conf->alt_count].confidence = confidence;
    conf->alternatives[conf->alt_count].source_flags = source_flags;
    conf->alt_count++;
    
    /* Set ambiguous flag if we have alternatives */
    conf->flags |= UFT_CONFLAG_AMBIGUOUS;
    
    return true;
}

/*===========================================================================
 * Track Functions
 *===========================================================================*/

uft_track_confidence_t* uft_trackconf_alloc(uint8_t track, uint8_t head,
                                             uint32_t bit_count) {
    uft_track_confidence_t *tconf = calloc(1, sizeof(*tconf));
    if (!tconf) return NULL;
    
    tconf->bits = calloc(bit_count, sizeof(uft_bit_confidence_packed_t));
    if (!tconf->bits) {
        free(tconf);
        return NULL;
    }
    
    tconf->track = track;
    tconf->head = head;
    tconf->bit_count = bit_count;
    
    return tconf;
}

void uft_trackconf_free(uft_track_confidence_t *tconf) {
    if (tconf) {
        free(tconf->bits);
        free(tconf);
    }
}

int uft_trackconf_set_bit(uft_track_confidence_t *tconf,
                           uint32_t bit_index,
                           const uft_bit_confidence_packed_t *conf) {
    if (!tconf || !conf || bit_index >= tconf->bit_count) return -1;
    tconf->bits[bit_index] = *conf;
    return 0;
}

int uft_trackconf_get_bit(const uft_track_confidence_t *tconf,
                           uint32_t bit_index,
                           uft_bit_confidence_packed_t *conf) {
    if (!tconf || !conf || bit_index >= tconf->bit_count) return -1;
    *conf = tconf->bits[bit_index];
    return 0;
}

void uft_trackconf_calc_stats(uft_track_confidence_t *tconf) {
    if (!tconf || !tconf->bits || tconf->bit_count == 0) return;
    
    uint32_t sum = 0;
    uint8_t min_conf = 100, max_conf = 0;
    uint32_t weak_count = 0, corrected_count = 0, ambiguous_count = 0;
    
    /* Histogram for median */
    uint32_t histogram[101] = {0};
    
    for (uint32_t i = 0; i < tconf->bit_count; i++) {
        uint8_t conf = tconf->bits[i].confidence;
        sum += conf;
        if (conf < min_conf) min_conf = conf;
        if (conf > max_conf) max_conf = conf;
        histogram[conf]++;
        
        if (tconf->bits[i].weak) weak_count++;
        if (tconf->bits[i].corrected) corrected_count++;
        if (tconf->bits[i].ambiguous) ambiguous_count++;
    }
    
    tconf->min_confidence = min_conf;
    tconf->max_confidence = max_conf;
    tconf->avg_confidence = (uint8_t)(sum / tconf->bit_count);
    tconf->weak_bit_count = weak_count;
    tconf->corrected_bit_count = corrected_count;
    tconf->ambiguous_bit_count = ambiguous_count;
    
    /* Calculate median */
    uint32_t median_pos = tconf->bit_count / 2;
    uint32_t count = 0;
    for (int i = 0; i <= 100; i++) {
        count += histogram[i];
        if (count >= median_pos) {
            tconf->median_confidence = i;
            break;
        }
    }
}

void uft_trackconf_find_regions(uft_track_confidence_t *tconf,
                                 uint8_t threshold,
                                 uint32_t min_length) {
    if (!tconf || !tconf->bits) return;
    
    tconf->low_conf_region_count = 0;
    
    bool in_region = false;
    uint32_t region_start = 0;
    uint8_t region_min = 100;
    
    for (uint32_t i = 0; i < tconf->bit_count; i++) {
        bool low = (tconf->bits[i].confidence < threshold);
        
        if (low && !in_region) {
            /* Start new region */
            in_region = true;
            region_start = i;
            region_min = tconf->bits[i].confidence;
        } else if (low && in_region) {
            /* Continue region */
            if (tconf->bits[i].confidence < region_min) {
                region_min = tconf->bits[i].confidence;
            }
        } else if (!low && in_region) {
            /* End region */
            uint32_t length = i - region_start;
            if (length >= min_length && tconf->low_conf_region_count < 64) {
                tconf->low_conf_regions[tconf->low_conf_region_count].start_bit = region_start;
                tconf->low_conf_regions[tconf->low_conf_region_count].end_bit = i - 1;
                tconf->low_conf_regions[tconf->low_conf_region_count].min_confidence = region_min;
                tconf->low_conf_region_count++;
            }
            in_region = false;
        }
    }
    
    /* Handle region at end of track */
    if (in_region) {
        uint32_t length = tconf->bit_count - region_start;
        if (length >= min_length && tconf->low_conf_region_count < 64) {
            tconf->low_conf_regions[tconf->low_conf_region_count].start_bit = region_start;
            tconf->low_conf_regions[tconf->low_conf_region_count].end_bit = tconf->bit_count - 1;
            tconf->low_conf_regions[tconf->low_conf_region_count].min_confidence = region_min;
            tconf->low_conf_region_count++;
        }
    }
}

int uft_trackconf_get_full(const uft_track_confidence_t *tconf,
                            uint32_t bit_index,
                            uft_bit_confidence_t *conf) {
    if (!tconf || !conf || bit_index >= tconf->bit_count) return -1;
    
    uft_bitconf_unpack(&tconf->bits[bit_index], conf);
    conf->bit_index = bit_index;
    conf->byte_index = bit_index / 8;
    conf->bit_in_byte = bit_index % 8;
    
    return 0;
}

/*===========================================================================
 * Sector Functions
 *===========================================================================*/

void uft_sectorconf_calc(const uft_track_confidence_t *tconf,
                          uint32_t start_bit,
                          uint32_t bit_count,
                          uint32_t header_bits,
                          uint8_t sector,
                          bool crc_valid,
                          uft_sector_confidence_t *sconf) {
    if (!tconf || !sconf) return;
    
    memset(sconf, 0, sizeof(*sconf));
    sconf->track = tconf->track;
    sconf->head = tconf->head;
    sconf->sector = sector;
    sconf->first_bit = start_bit;
    sconf->bit_count = bit_count;
    sconf->crc_valid = crc_valid;
    
    if (!tconf->bits || start_bit + bit_count > tconf->bit_count) return;
    
    /* Calculate header confidence */
    uint32_t header_sum = 0;
    for (uint32_t i = 0; i < header_bits && start_bit + i < tconf->bit_count; i++) {
        header_sum += tconf->bits[start_bit + i].confidence;
    }
    sconf->header_confidence = (header_bits > 0) ? (uint8_t)(header_sum / header_bits) : 0;
    
    /* Calculate data confidence */
    uint32_t data_sum = 0;
    uint32_t data_start = start_bit + header_bits;
    uint32_t data_count = bit_count - header_bits;
    
    for (uint32_t i = 0; i < data_count && data_start + i < tconf->bit_count; i++) {
        data_sum += tconf->bits[data_start + i].confidence;
        if (tconf->bits[data_start + i].weak) sconf->weak_bit_count++;
        if (tconf->bits[data_start + i].confidence < UFT_BITCONF_GOOD) {
            sconf->low_conf_bit_count++;
        }
    }
    sconf->data_confidence = (data_count > 0) ? (uint8_t)(data_sum / data_count) : 0;
    
    /* CRC confidence */
    sconf->crc_confidence = crc_valid ? UFT_BITCONF_CERTAIN : UFT_BITCONF_NONE;
    
    /* Overall confidence */
    if (crc_valid) {
        sconf->overall_confidence = (sconf->header_confidence + sconf->data_confidence * 2 + 100) / 4;
    } else {
        sconf->overall_confidence = (sconf->header_confidence + sconf->data_confidence) / 2;
    }
    
    /* Flags */
    sconf->has_weak_bits = (sconf->weak_bit_count > 0);
    sconf->was_corrected = false; /* Set by caller if correction applied */
}

/*===========================================================================
 * Reporting
 *===========================================================================*/

const char* uft_bitconf_level_name(uint8_t confidence) {
    if (confidence >= UFT_BITCONF_CERTAIN) return "CERTAIN";
    if (confidence >= UFT_BITCONF_HIGH) return "HIGH";
    if (confidence >= UFT_BITCONF_GOOD) return "GOOD";
    if (confidence >= UFT_BITCONF_MARGINAL) return "MARGINAL";
    if (confidence >= UFT_BITCONF_LOW) return "LOW";
    return "NONE";
}

char* uft_bitconf_source_names(uint16_t flags, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return buffer;
    
    buffer[0] = '\0';
    size_t pos = 0;
    
    struct { uint16_t flag; const char *name; } sources[] = {
        { UFT_CONFSRC_TIMING, "TIMING" },
        { UFT_CONFSRC_AMPLITUDE, "AMPLITUDE" },
        { UFT_CONFSRC_MULTIREV, "MULTIREV" },
        { UFT_CONFSRC_PLL, "PLL" },
        { UFT_CONFSRC_CRC, "CRC" },
        { UFT_CONFSRC_CHECKSUM, "CHECKSUM" },
        { UFT_CONFSRC_CONTEXT, "CONTEXT" },
        { UFT_CONFSRC_PATTERN, "PATTERN" },
        { UFT_CONFSRC_CORRECTION, "CORRECTION" },
        { UFT_CONFSRC_INFERRED, "INFERRED" },
        { UFT_CONFSRC_MANUAL, "MANUAL" },
        { 0, NULL }
    };
    
    for (int i = 0; sources[i].name; i++) {
        if (flags & sources[i].flag) {
            if (pos > 0 && pos < buffer_size - 1) {
                buffer[pos++] = '|';
            }
            size_t len = strlen(sources[i].name);
            if (pos + len < buffer_size) {
                memcpy(buffer + pos, sources[i].name, len);
                pos += len;
            }
        }
    }
    
    buffer[pos] = '\0';
    return buffer;
}

char* uft_bitconf_flag_names(uint16_t flags, char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return buffer;
    
    buffer[0] = '\0';
    size_t pos = 0;
    
    struct { uint16_t flag; const char *name; } flag_list[] = {
        { UFT_CONFLAG_WEAK, "WEAK" },
        { UFT_CONFLAG_UNSTABLE, "UNSTABLE" },
        { UFT_CONFLAG_CORRECTED, "CORRECTED" },
        { UFT_CONFLAG_INTERPOLATED, "INTERPOLATED" },
        { UFT_CONFLAG_AMBIGUOUS, "AMBIGUOUS" },
        { UFT_CONFLAG_PROTECTED, "PROTECTED" },
        { UFT_CONFLAG_NO_FLUX, "NO_FLUX" },
        { UFT_CONFLAG_TIMING_ANOMALY, "TIMING_ANOMALY" },
        { UFT_CONFLAG_PLL_SLIP, "PLL_SLIP" },
        { UFT_CONFLAG_BOUNDARY, "BOUNDARY" },
        { 0, NULL }
    };
    
    for (int i = 0; flag_list[i].name; i++) {
        if (flags & flag_list[i].flag) {
            if (pos > 0 && pos < buffer_size - 1) {
                buffer[pos++] = '|';
            }
            size_t len = strlen(flag_list[i].name);
            if (pos + len < buffer_size) {
                memcpy(buffer + pos, flag_list[i].name, len);
                pos += len;
            }
        }
    }
    
    buffer[pos] = '\0';
    return buffer;
}

size_t uft_bitconf_report(const uft_bit_confidence_t *conf,
                           char *buffer, size_t buffer_size) {
    if (!conf || !buffer || buffer_size == 0) return 0;
    
    char src_buf[128], flag_buf[128];
    uft_bitconf_source_names(conf->source_flags, src_buf, sizeof(src_buf));
    uft_bitconf_flag_names(conf->flags, flag_buf, sizeof(flag_buf));
    
    size_t w = snprintf(buffer, buffer_size,
        "Bit %u: value=%d, confidence=%d%% (%s)\n"
        "  Position: byte %u, bit %d\n"
        "  Timing: %u ns (expected %u ns, error %d ns)\n"
        "  Multi-rev: %d reads (%d ones, %d zeros), consistency=%d%%\n"
        "  PLL: phase=%d, quality=%d%%, status=0x%02X\n"
        "  Sources: %s\n"
        "  Flags: %s\n"
        "  Alternatives: %d\n",
        conf->bit_index, conf->value, conf->confidence,
        uft_bitconf_level_name(conf->confidence),
        conf->byte_index, conf->bit_in_byte,
        conf->timing_ns, conf->expected_ns, conf->timing_error_ns,
        conf->revolutions_read, conf->ones_count, conf->zeros_count,
        conf->consistency,
        conf->pll_phase, conf->pll_lock_quality, conf->pll_status,
        src_buf[0] ? src_buf : "NONE",
        flag_buf[0] ? flag_buf : "NONE",
        conf->alt_count);
    
    return w;
}

size_t uft_trackconf_report(const uft_track_confidence_t *tconf,
                             char *buffer, size_t buffer_size) {
    if (!tconf || !buffer || buffer_size == 0) return 0;
    
    size_t w = snprintf(buffer, buffer_size,
        "Track %d/%d Confidence Report\n"
        "================================\n"
        "Bits: %u\n"
        "Confidence: min=%d%%, max=%d%%, avg=%d%%, median=%d%%\n"
        "Weak bits: %u\n"
        "Corrected bits: %u\n"
        "Ambiguous bits: %u\n"
        "Low confidence regions: %d\n",
        tconf->track, tconf->head,
        tconf->bit_count,
        tconf->min_confidence, tconf->max_confidence,
        tconf->avg_confidence, tconf->median_confidence,
        tconf->weak_bit_count,
        tconf->corrected_bit_count,
        tconf->ambiguous_bit_count,
        tconf->low_conf_region_count);
    
    if (w < buffer_size && tconf->low_conf_region_count > 0) {
        w += snprintf(buffer + w, buffer_size - w, "\nLow Confidence Regions:\n");
        for (uint16_t i = 0; i < tconf->low_conf_region_count && w < buffer_size; i++) {
            w += snprintf(buffer + w, buffer_size - w,
                "  [%d] bits %u-%u (min %d%%)\n",
                i,
                tconf->low_conf_regions[i].start_bit,
                tconf->low_conf_regions[i].end_bit,
                tconf->low_conf_regions[i].min_confidence);
        }
    }
    
    return w;
}

size_t uft_trackconf_export_json(const uft_track_confidence_t *tconf,
                                  char *buffer, size_t buffer_size) {
    if (!tconf || !buffer || buffer_size == 0) return 0;
    
    size_t w = snprintf(buffer, buffer_size,
        "{\n"
        "  \"track\": %d,\n"
        "  \"head\": %d,\n"
        "  \"bit_count\": %u,\n"
        "  \"confidence\": {\n"
        "    \"min\": %d,\n"
        "    \"max\": %d,\n"
        "    \"avg\": %d,\n"
        "    \"median\": %d\n"
        "  },\n"
        "  \"weak_bits\": %u,\n"
        "  \"corrected_bits\": %u,\n"
        "  \"ambiguous_bits\": %u,\n"
        "  \"low_conf_regions\": [\n",
        tconf->track, tconf->head,
        tconf->bit_count,
        tconf->min_confidence, tconf->max_confidence,
        tconf->avg_confidence, tconf->median_confidence,
        tconf->weak_bit_count,
        tconf->corrected_bit_count,
        tconf->ambiguous_bit_count);
    
    for (uint16_t i = 0; i < tconf->low_conf_region_count && w < buffer_size; i++) {
        w += snprintf(buffer + w, buffer_size - w,
            "    {\"start\": %u, \"end\": %u, \"min\": %d}%s\n",
            tconf->low_conf_regions[i].start_bit,
            tconf->low_conf_regions[i].end_bit,
            tconf->low_conf_regions[i].min_confidence,
            (i < tconf->low_conf_region_count - 1) ? "," : "");
    }
    
    if (w < buffer_size) {
        w += snprintf(buffer + w, buffer_size - w,
            "  ]\n}\n");
    }
    
    return w;
}

void uft_trackconf_heatmap(const uft_track_confidence_t *tconf,
                            uint32_t width,
                            uint8_t *heatmap) {
    if (!tconf || !heatmap || width == 0 || tconf->bit_count == 0) return;
    
    memset(heatmap, 0, width);
    
    uint32_t bits_per_sample = tconf->bit_count / width;
    if (bits_per_sample == 0) bits_per_sample = 1;
    
    for (uint32_t i = 0; i < width; i++) {
        uint32_t start_bit = i * bits_per_sample;
        uint32_t end_bit = start_bit + bits_per_sample;
        if (end_bit > tconf->bit_count) end_bit = tconf->bit_count;
        
        /* Average confidence for this segment */
        uint32_t sum = 0;
        uint32_t count = 0;
        for (uint32_t j = start_bit; j < end_bit; j++) {
            sum += tconf->bits[j].confidence;
            count++;
        }
        
        if (count > 0) {
            /* Scale to 0-255 for heatmap */
            heatmap[i] = (uint8_t)((sum * 255) / (count * 100));
        }
    }
}

/*===========================================================================
 * Pack/Unpack
 *===========================================================================*/

void uft_bitconf_pack(const uft_bit_confidence_t *full,
                       uft_bit_confidence_packed_t *packed) {
    if (!full || !packed) return;
    
    packed->value = full->value & 1;
    packed->weak = (full->flags & UFT_CONFLAG_WEAK) ? 1 : 0;
    packed->corrected = (full->flags & UFT_CONFLAG_CORRECTED) ? 1 : 0;
    packed->ambiguous = (full->flags & UFT_CONFLAG_AMBIGUOUS) ? 1 : 0;
    packed->protected = (full->flags & UFT_CONFLAG_PROTECTED) ? 1 : 0;
    packed->reserved = 0;
    packed->confidence = full->confidence;
    packed->consistency = full->consistency;
    packed->pll_quality = full->pll_lock_quality;
    packed->timing_ns = full->timing_ns;
    packed->source_flags = full->source_flags;
}

void uft_bitconf_unpack(const uft_bit_confidence_packed_t *packed,
                         uft_bit_confidence_t *full) {
    if (!packed || !full) return;
    
    memset(full, 0, sizeof(*full));
    
    full->value = packed->value;
    full->confidence = packed->confidence;
    full->consistency = packed->consistency;
    full->pll_lock_quality = packed->pll_quality;
    full->timing_ns = packed->timing_ns;
    full->source_flags = packed->source_flags;
    
    full->flags = 0;
    if (packed->weak) full->flags |= UFT_CONFLAG_WEAK;
    if (packed->corrected) full->flags |= UFT_CONFLAG_CORRECTED;
    if (packed->ambiguous) full->flags |= UFT_CONFLAG_AMBIGUOUS;
    if (packed->protected) full->flags |= UFT_CONFLAG_PROTECTED;
}

/*===========================================================================
 * Serialization
 *===========================================================================*/

#define UFT_TRACKCONF_MAGIC  0x55544643  /* "UTFC" */
#define UFT_TRACKCONF_VER    1

int32_t uft_trackconf_serialize(const uft_track_confidence_t *tconf,
                                 uint8_t *buffer, size_t buffer_size) {
    if (!tconf || !buffer) return -1;
    
    /* Calculate required size */
    size_t required = 16 + /* header */
                      tconf->bit_count * sizeof(uft_bit_confidence_packed_t) +
                      12 + /* stats */
                      tconf->low_conf_region_count * 12; /* regions */
    
    if (buffer_size < required) return -2;
    
    uint8_t *p = buffer;
    
    /* Header */
    *(uint32_t*)p = UFT_TRACKCONF_MAGIC; p += 4;
    *(uint32_t*)p = UFT_TRACKCONF_VER; p += 4;
    *p++ = tconf->track;
    *p++ = tconf->head;
    p += 2; /* padding */
    *(uint32_t*)p = tconf->bit_count; p += 4;
    
    /* Bit data */
    size_t bits_size = tconf->bit_count * sizeof(uft_bit_confidence_packed_t);
    memcpy(p, tconf->bits, bits_size);
    p += bits_size;
    
    /* Stats */
    *p++ = tconf->min_confidence;
    *p++ = tconf->max_confidence;
    *p++ = tconf->avg_confidence;
    *p++ = tconf->median_confidence;
    *(uint32_t*)p = tconf->weak_bit_count; p += 4;
    *(uint16_t*)p = tconf->low_conf_region_count; p += 2;
    p += 2; /* padding */
    
    /* Regions */
    for (uint16_t i = 0; i < tconf->low_conf_region_count; i++) {
        *(uint32_t*)p = tconf->low_conf_regions[i].start_bit; p += 4;
        *(uint32_t*)p = tconf->low_conf_regions[i].end_bit; p += 4;
        *p++ = tconf->low_conf_regions[i].min_confidence;
        p += 3; /* padding */
    }
    
    return (int32_t)(p - buffer);
}

int32_t uft_trackconf_deserialize(const uint8_t *buffer, size_t buffer_size,
                                   uft_track_confidence_t **tconf) {
    if (!buffer || !tconf || buffer_size < 16) return -1;
    
    const uint8_t *p = buffer;
    
    /* Check header */
    if (*(uint32_t*)p != UFT_TRACKCONF_MAGIC) return -3;
    p += 4;
    if (*(uint32_t*)p != UFT_TRACKCONF_VER) return -4;
    p += 4;
    
    uint8_t track = *p++;
    uint8_t head = *p++;
    p += 2; /* padding */
    uint32_t bit_count = *(uint32_t*)p; p += 4;
    
    /* Allocate */
    *tconf = uft_trackconf_alloc(track, head, bit_count);
    if (!*tconf) return -5;
    
    /* Read bit data */
    size_t bits_size = bit_count * sizeof(uft_bit_confidence_packed_t);
    if (p + bits_size > buffer + buffer_size) {
        uft_trackconf_free(*tconf);
        *tconf = NULL;
        return -6;
    }
    memcpy((*tconf)->bits, p, bits_size);
    p += bits_size;
    
    /* Stats */
    (*tconf)->min_confidence = *p++;
    (*tconf)->max_confidence = *p++;
    (*tconf)->avg_confidence = *p++;
    (*tconf)->median_confidence = *p++;
    (*tconf)->weak_bit_count = *(uint32_t*)p; p += 4;
    (*tconf)->low_conf_region_count = *(uint16_t*)p; p += 2;
    p += 2; /* padding */
    
    /* Regions */
    for (uint16_t i = 0; i < (*tconf)->low_conf_region_count && i < 64; i++) {
        (*tconf)->low_conf_regions[i].start_bit = *(uint32_t*)p; p += 4;
        (*tconf)->low_conf_regions[i].end_bit = *(uint32_t*)p; p += 4;
        (*tconf)->low_conf_regions[i].min_confidence = *p++;
        p += 3; /* padding */
    }
    
    return (int32_t)(p - buffer);
}

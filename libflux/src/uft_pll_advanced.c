/*
 * uft_pll_advanced.c
 * 
 * UnifiedFloppyTool - Advanced PLL Implementation
 * 
 * Based on FluxFox PLL concepts (MIT licensed) by Daniel Balsom
 * 
 * Copyright (c) 2025 UFT Project
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_pll.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Q16 Fixed-Point Helpers
 *============================================================================*/

#define Q16_ONE     65536
#define Q16_HALF    32768

static inline uint32_t float_to_q16(double f) {
    return (uint32_t)(f * 65536.0 + 0.5);
}

static inline double q16_to_float(uint32_t q) {
    return q / 65536.0;
}

static inline int64_t q16_mul(int64_t a, int64_t b) {
    return (a * b) >> 16;
}

/*============================================================================
 * Configuration Functions
 *============================================================================*/

void uft_pll_config_init(uft_pll_config_t *config)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    /* Default: MFM DD at 250kbps -> 500kHz clock -> 2µs cell */
    config->clock_rate_hz = UFT_PLL_RATE_500K;
    config->cell_time_ns = 2000;  /* 2µs */
    
    /* FluxFox-style gains */
    config->clock_gain_q16 = UFT_PLL_CLOCK_GAIN_Q16;  /* 0.05 */
    config->phase_gain_q16 = UFT_PLL_PHASE_GAIN_Q16;  /* 0.65 */
    config->max_adjust_q16 = UFT_PLL_MAX_ADJUST_Q16;  /* 0.20 */
    
    /* Bounds: ±20% of nominal */
    config->cell_ns_min = 1600;   /* 2000 - 20% */
    config->cell_ns_max = 2400;   /* 2000 + 20% */
    config->max_run_cells = 16;   /* Max zeros before forcing sync */
    
    config->encoding = UFT_PLL_ENC_MFM;
    config->flags = UFT_PLL_FLAG_NONE;
}

void uft_pll_config_from_preset(uft_pll_config_t *config, uft_pll_preset_t preset)
{
    uft_pll_config_init(config);
    
    switch (preset) {
        case UFT_PLL_PRESET_AGGRESSIVE:
            config->clock_gain_q16 = float_to_q16(0.10);
            config->phase_gain_q16 = float_to_q16(0.80);
            config->max_adjust_q16 = float_to_q16(0.30);
            break;
            
        case UFT_PLL_PRESET_CONSERVATIVE:
            config->clock_gain_q16 = float_to_q16(0.02);
            config->phase_gain_q16 = float_to_q16(0.40);
            config->max_adjust_q16 = float_to_q16(0.10);
            break;
            
        case UFT_PLL_PRESET_WEAK_DISK:
            config->clock_gain_q16 = float_to_q16(0.03);
            config->phase_gain_q16 = float_to_q16(0.50);
            config->max_adjust_q16 = float_to_q16(0.25);
            config->max_run_cells = 32;
            config->flags |= UFT_PLL_FLAG_DETECT_WEAK;
            break;
            
        case UFT_PLL_PRESET_COPY_PROTECT:
            config->clock_gain_q16 = float_to_q16(0.04);
            config->phase_gain_q16 = float_to_q16(0.55);
            config->max_adjust_q16 = float_to_q16(0.15);
            config->flags |= UFT_PLL_FLAG_DETECT_MARKERS | UFT_PLL_FLAG_DETECT_WEAK;
            break;
            
        default:
            break;
    }
}

void uft_pll_config_set_clock(uft_pll_config_t *config, uint32_t rate_hz)
{
    if (!config || rate_hz == 0) return;
    
    config->clock_rate_hz = rate_hz;
    config->cell_time_ns = (uint32_t)(1000000000ULL / rate_hz);
    
    /* Update bounds */
    double max_adj = q16_to_float(config->max_adjust_q16);
    config->cell_ns_min = (uint32_t)(config->cell_time_ns * (1.0 - max_adj));
    config->cell_ns_max = (uint32_t)(config->cell_time_ns * (1.0 + max_adj));
}

void uft_pll_config_set_gains(uft_pll_config_t *config, 
                              double clock_gain, double phase_gain)
{
    if (!config) return;
    
    if (clock_gain >= 0.0 && clock_gain <= 1.0) {
        config->clock_gain_q16 = float_to_q16(clock_gain);
    }
    if (phase_gain >= 0.0 && phase_gain <= 1.0) {
        config->phase_gain_q16 = float_to_q16(phase_gain);
    }
}

void uft_pll_config_set_max_adjust(uft_pll_config_t *config, double max_adj)
{
    if (!config || max_adj < 0.0 || max_adj > 1.0) return;
    
    config->max_adjust_q16 = float_to_q16(max_adj);
    config->cell_ns_min = (uint32_t)(config->cell_time_ns * (1.0 - max_adj));
    config->cell_ns_max = (uint32_t)(config->cell_time_ns * (1.0 + max_adj));
}

/*============================================================================
 * Result Management
 *============================================================================*/

void uft_pll_result_init(uft_pll_result_t *result)
{
    if (!result) return;
    memset(result, 0, sizeof(*result));
}

void uft_pll_result_free(uft_pll_result_t *result)
{
    if (!result) return;
    
    free(result->bitstream);
    free(result->error_mask);
    free(result->flux_types);
    free(result->markers);
    free(result->weak_regions);
    
    memset(result, 0, sizeof(*result));
}

/*============================================================================
 * Classification
 *============================================================================*/

uft_flux_type_t uft_pll_classify_flux(uint32_t cell_ns, uint64_t duration_ns)
{
    /* Calculate number of cells */
    double cells = (double)duration_ns / (double)cell_ns;
    
    if (cells < 1.5) return UFT_FLUX_TOO_SHORT;
    if (cells < 2.5) return UFT_FLUX_SHORT;    /* 2T */
    if (cells < 3.5) return UFT_FLUX_MEDIUM;   /* 3T */
    if (cells < 4.5) return UFT_FLUX_LONG;     /* 4T */
    return UFT_FLUX_TOO_LONG;
}

const char* uft_pll_flux_type_name(uft_flux_type_t type)
{
    switch (type) {
        case UFT_FLUX_TOO_SHORT: return "TooShort";
        case UFT_FLUX_SHORT:     return "Short(2T)";
        case UFT_FLUX_MEDIUM:    return "Medium(3T)";
        case UFT_FLUX_LONG:      return "Long(4T)";
        case UFT_FLUX_TOO_LONG:  return "TooLong";
        default:                 return "Unknown";
    }
}

const char* uft_pll_encoding_name(uft_pll_encoding_t encoding)
{
    switch (encoding) {
        case UFT_PLL_ENC_MFM:       return "MFM";
        case UFT_PLL_ENC_FM:        return "FM";
        case UFT_PLL_ENC_GCR_CBM:   return "GCR (Commodore)";
        case UFT_PLL_ENC_GCR_APPLE: return "GCR (Apple)";
        case UFT_PLL_ENC_RAW:       return "Raw";
        default:                    return "Unknown";
    }
}

/*============================================================================
 * Core PLL Decode - MFM
 *============================================================================*/

static int decode_mfm(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_pll_config_t *config,
    uft_pll_result_t *result)
{
    if (count < 2) return -1;
    
    /* Allocate output buffers */
    size_t max_bits = count * 4;  /* Generous estimate */
    result->bitstream = calloc((max_bits + 7) / 8, 1);
    result->error_mask = calloc((max_bits + 7) / 8, 1);
    if (!result->bitstream || !result->error_mask) {
        return -1;
    }
    
    /* Allocate flux types if requested */
    if (config->flags & UFT_PLL_FLAG_COLLECT_TYPES) {
        result->flux_types = malloc(count * sizeof(uft_flux_type_t));
        result->flux_type_count = 0;
    }
    
    /* Allocate markers if requested */
    if (config->flags & UFT_PLL_FLAG_DETECT_MARKERS) {
        result->marker_capacity = 256;
        result->markers = malloc(result->marker_capacity * sizeof(uft_pll_marker_t));
        result->marker_count = 0;
    }
    
    /* PLL state */
    uint32_t cell_ns = config->cell_time_ns;
    int64_t phase_error = 0;
    int64_t phase_adjust = 0;
    
    /* Statistics */
    result->stats.total = (uint32_t)count;
    result->stats.shortest_ns = UINT64_MAX;
    result->stats.longest_ns = 0;
    
    /* Decode state */
    size_t bit_pos = 0;
    uint64_t shift_reg = 0;
    int zero_count = 0;
    bool last_bit = false;
    
    /* Process each flux transition */
    for (size_t i = 1; i < count; i++) {
        uint64_t delta_ns = timestamps_ns[i] - timestamps_ns[i-1];
        
        /* Update statistics */
        if (delta_ns < result->stats.shortest_ns) {
            result->stats.shortest_ns = delta_ns;
        }
        if (delta_ns > result->stats.longest_ns) {
            result->stats.longest_ns = delta_ns;
        }
        
        /* Calculate flux length in cells */
        int64_t adjusted_delta = (int64_t)delta_ns + phase_adjust;
        if (adjusted_delta < 0) adjusted_delta = 0;
        
        uint32_t flux_cells = (adjusted_delta + cell_ns / 2) / cell_ns;
        
        /* Classify transition */
        uft_flux_type_t ftype;
        if (flux_cells < 2) {
            ftype = UFT_FLUX_TOO_SHORT;
            result->stats.too_short++;
            flux_cells = 2;  /* Clamp to minimum */
        } else if (flux_cells > 4) {
            ftype = UFT_FLUX_TOO_LONG;
            result->stats.too_long++;
            if (flux_cells > config->max_run_cells) {
                flux_cells = config->max_run_cells;
            }
        } else {
            switch (flux_cells) {
                case 2: ftype = UFT_FLUX_SHORT;  result->stats.short_count++;  break;
                case 3: ftype = UFT_FLUX_MEDIUM; result->stats.medium_count++; break;
                case 4: ftype = UFT_FLUX_LONG;   result->stats.long_count++;   break;
                default: ftype = UFT_FLUX_UNKNOWN; break;
            }
        }
        
        /* Store flux type if requested */
        if (result->flux_types && result->flux_type_count < count) {
            result->flux_types[result->flux_type_count++] = ftype;
        }
        
        /* Emit bits: (flux_cells - 1) zeros followed by a one */
        for (uint32_t j = 0; j < flux_cells - 1 && bit_pos < max_bits; j++) {
            /* Emit zero */
            /* (bit already 0 from calloc) */
            
            zero_count++;
            shift_reg <<= 1;
            
            /* More than 3 zeros in MFM is an error */
            if (zero_count > 3) {
                result->error_mask[bit_pos / 8] |= (0x80 >> (bit_pos % 8));
                result->stats.mfm_errors++;
            }
            
            bit_pos++;
            last_bit = false;
        }
        
        /* Emit one (the transition) */
        if (bit_pos < max_bits) {
            result->bitstream[bit_pos / 8] |= (0x80 >> (bit_pos % 8));
            
            /* Two ones in a row is an MFM error */
            if (last_bit) {
                result->error_mask[bit_pos / 8] |= (0x80 >> (bit_pos % 8));
                result->stats.mfm_errors++;
            }
            
            shift_reg <<= 1;
            shift_reg |= 1;
            zero_count = 0;
            last_bit = true;
            bit_pos++;
        }
        
        /* Check for MFM sync markers */
        if ((config->flags & UFT_PLL_FLAG_DETECT_MARKERS) && 
            result->markers && 
            result->marker_count < result->marker_capacity) {
            /* Check for A1 sync (0x4489) or AAAA4489 pattern */
            if ((shift_reg & 0xFFFF) == UFT_PLL_MFM_SYNC_MARK) {
                uft_pll_marker_t *m = &result->markers[result->marker_count++];
                m->time_ns = timestamps_ns[i];
                m->bit_offset = bit_pos > 16 ? bit_pos - 16 : 0;
                m->pattern = UFT_PLL_MFM_SYNC_MARK;
                result->stats.markers_found++;
            }
        }
        
        /* Calculate phase error */
        /* The transition should ideally be in the center of the last cell */
        int64_t expected_delta = (int64_t)flux_cells * cell_ns;
        phase_error = (int64_t)delta_ns - expected_delta;
        
        /* Apply phase adjustment (FluxFox-style) */
        /* Use minimum of current and previous error for stability */
        int64_t phase_term = q16_mul(phase_error, config->phase_gain_q16);
        phase_adjust = phase_term;
        
        /* Adjust clock rate based on error */
        if (phase_error != 0) {
            int64_t clock_adj = q16_mul(phase_error, config->clock_gain_q16);
            int64_t new_cell = (int64_t)cell_ns + (clock_adj / (int64_t)flux_cells);
            
            /* Clamp to bounds */
            if (new_cell < (int64_t)config->cell_ns_min) {
                new_cell = config->cell_ns_min;
            } else if (new_cell > (int64_t)config->cell_ns_max) {
                new_cell = config->cell_ns_max;
            }
            
            cell_ns = (uint32_t)new_cell;
        }
    }
    
    /* Finalize result */
    result->bit_count = bit_pos;
    result->bitstream_len = (bit_pos + 7) / 8;
    result->final_cell_ns = cell_ns;
    
    return (int)bit_pos;
}

/*============================================================================
 * Public Decode Functions
 *============================================================================*/

int uft_pll_decode(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_pll_config_t *config,
    uft_pll_result_t *result)
{
    if (!timestamps_ns || !config || !result || count < 2) {
        return -1;
    }
    
    uft_pll_result_init(result);
    
    switch (config->encoding) {
        case UFT_PLL_ENC_MFM:
            return decode_mfm(timestamps_ns, count, config, result);
            
        case UFT_PLL_ENC_FM:
            /* FM is similar but with different cell timing */
            /* TODO: Implement FM decode */
            return decode_mfm(timestamps_ns, count, config, result);
            
        default:
            return -1;
    }
}

int uft_pll_decode_deltas(
    const uint32_t *delta_ns,
    size_t count,
    const uft_pll_config_t *config,
    uft_pll_result_t *result)
{
    if (!delta_ns || count == 0) return -1;
    
    /* Convert deltas to timestamps */
    uint64_t *timestamps = malloc((count + 1) * sizeof(uint64_t));
    if (!timestamps) return -1;
    
    timestamps[0] = 0;
    for (size_t i = 0; i < count; i++) {
        timestamps[i + 1] = timestamps[i] + delta_ns[i];
    }
    
    int ret = uft_pll_decode(timestamps, count + 1, config, result);
    free(timestamps);
    return ret;
}

int uft_pll_decode_ticks(
    const uint32_t *ticks,
    size_t count,
    uint32_t tick_rate_hz,
    const uft_pll_config_t *config,
    uft_pll_result_t *result)
{
    if (!ticks || count == 0 || tick_rate_hz == 0) return -1;
    
    /* Convert ticks to nanosecond deltas */
    double ns_per_tick = 1000000000.0 / tick_rate_hz;
    
    uint64_t *timestamps = malloc((count + 1) * sizeof(uint64_t));
    if (!timestamps) return -1;
    
    timestamps[0] = 0;
    for (size_t i = 0; i < count; i++) {
        timestamps[i + 1] = timestamps[i] + (uint64_t)(ticks[i] * ns_per_tick);
    }
    
    int ret = uft_pll_decode(timestamps, count + 1, config, result);
    free(timestamps);
    return ret;
}

/*============================================================================
 * MFM Helpers
 *============================================================================*/

int uft_pll_mfm_decode(const uint8_t *mfm, size_t mfm_bits,
                       uint8_t *data, size_t data_max)
{
    if (!mfm || !data || mfm_bits < 2) return -1;
    
    size_t data_bits = 0;
    size_t data_bytes = 0;
    
    /* MFM: every other bit is data, starting from bit 1 */
    for (size_t i = 1; i < mfm_bits && data_bytes < data_max; i += 2) {
        size_t byte_idx = i / 8;
        size_t bit_idx = 7 - (i % 8);
        
        bool bit = (mfm[byte_idx] >> bit_idx) & 1;
        
        size_t out_byte = data_bits / 8;
        size_t out_bit = 7 - (data_bits % 8);
        
        if (bit) {
            data[out_byte] |= (1 << out_bit);
        }
        
        data_bits++;
        data_bytes = (data_bits + 7) / 8;
    }
    
    return (int)data_bits;
}

int uft_pll_mfm_encode(const uint8_t *data, size_t data_bits,
                       uint8_t *mfm, size_t mfm_max)
{
    if (!data || !mfm || data_bits == 0) return -1;
    
    size_t mfm_bits = 0;
    bool last_data_bit = false;
    
    memset(mfm, 0, mfm_max);
    
    for (size_t i = 0; i < data_bits && (mfm_bits + 2) / 8 < mfm_max; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = 7 - (i % 8);
        bool data_bit = (data[byte_idx] >> bit_idx) & 1;
        
        /* Clock bit: 1 if both adjacent data bits are 0 */
        bool clock_bit = !last_data_bit && !data_bit;
        
        /* Write clock bit */
        if (clock_bit) {
            size_t out_byte = mfm_bits / 8;
            size_t out_bit = 7 - (mfm_bits % 8);
            mfm[out_byte] |= (1 << out_bit);
        }
        mfm_bits++;
        
        /* Write data bit */
        if (data_bit) {
            size_t out_byte = mfm_bits / 8;
            size_t out_bit = 7 - (mfm_bits % 8);
            mfm[out_byte] |= (1 << out_bit);
        }
        mfm_bits++;
        
        last_data_bit = data_bit;
    }
    
    return (int)mfm_bits;
}

int uft_pll_mfm_find_marker(const uint8_t *bits, size_t bit_count,
                            size_t start, uint16_t marker)
{
    if (!bits || bit_count < 16) return -1;
    
    uint16_t shift = 0;
    
    for (size_t i = start; i < bit_count; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = 7 - (i % 8);
        bool bit = (bits[byte_idx] >> bit_idx) & 1;
        
        shift = (shift << 1) | (bit ? 1 : 0);
        
        if (i >= start + 15 && shift == marker) {
            return (int)(i - 15);
        }
    }
    
    return -1;
}

/*============================================================================
 * Utilities
 *============================================================================*/

uint32_t uft_pll_estimate_rate(const uint64_t *timestamps_ns, size_t count)
{
    if (!timestamps_ns || count < 100) return UFT_PLL_RATE_500K;
    
    /* Sample middle of track for stability */
    size_t start = count / 4;
    size_t end = count * 3 / 4;
    
    uint64_t sum = 0;
    size_t samples = 0;
    
    for (size_t i = start + 1; i < end; i++) {
        uint64_t delta = timestamps_ns[i] - timestamps_ns[i-1];
        /* Only count reasonable deltas (2-8 µs for MFM) */
        if (delta >= 2000 && delta <= 10000) {
            sum += delta;
            samples++;
        }
    }
    
    if (samples == 0) return UFT_PLL_RATE_500K;
    
    uint64_t avg_delta = sum / samples;
    
    /* Estimate cell time (most common is 2T = short) */
    /* Assume average is around 3T (mix of 2T, 3T, 4T) */
    uint64_t cell_ns = avg_delta / 3;
    
    /* Convert to rate */
    if (cell_ns == 0) return UFT_PLL_RATE_500K;
    return (uint32_t)(1000000000ULL / cell_ns);
}

void uft_pll_print_stats(const uft_pll_stats_t *stats, FILE *stream)
{
    if (!stats) return;
    if (!stream) stream = stdout;
    
    fprintf(stream, "PLL Decode Statistics:\n");
    fprintf(stream, "  Total transitions: %u\n", stats->total);
    fprintf(stream, "  Short (2T):  %u (%.1f%%)\n", 
            stats->short_count, 100.0 * stats->short_count / stats->total);
    fprintf(stream, "  Medium (3T): %u (%.1f%%)\n",
            stats->medium_count, 100.0 * stats->medium_count / stats->total);
    fprintf(stream, "  Long (4T):   %u (%.1f%%)\n",
            stats->long_count, 100.0 * stats->long_count / stats->total);
    fprintf(stream, "  Too short:   %u\n", stats->too_short);
    fprintf(stream, "  Too long:    %u\n", stats->too_long);
    fprintf(stream, "  MFM errors:  %u\n", stats->mfm_errors);
    fprintf(stream, "  Markers:     %u\n", stats->markers_found);
    fprintf(stream, "  Shortest flux: %.2f µs\n", stats->shortest_ns / 1000.0);
    fprintf(stream, "  Longest flux:  %.2f µs\n", stats->longest_ns / 1000.0);
}

/*============================================================================
 * Legacy API Wrappers
 *============================================================================*/

uft_pll_cfg_t uft_pll_cfg_default_mfm_dd(void)
{
    return (uft_pll_cfg_t){
        .cell_ns = 2000,
        .cell_ns_min = 1600,
        .cell_ns_max = 2400,
        .alpha_q16 = UFT_PLL_CLOCK_GAIN_Q16,
        .max_run_cells = 16
    };
}

uft_pll_cfg_t uft_pll_cfg_default_mfm_hd(void)
{
    return (uft_pll_cfg_t){
        .cell_ns = 1000,
        .cell_ns_min = 800,
        .cell_ns_max = 1200,
        .alpha_q16 = UFT_PLL_CLOCK_GAIN_Q16,
        .max_run_cells = 16
    };
}

size_t uft_flux_to_bits_pll(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_pll_cfg_t *cfg,
    uint8_t *out_bits,
    size_t out_bits_capacity_bits,
    uint32_t *out_final_cell_ns,
    size_t *out_dropped_transitions)
{
    if (!timestamps_ns || !cfg || !out_bits || count < 2) {
        return 0;
    }
    
    /* Convert legacy config to new format */
    uft_pll_config_t config;
    uft_pll_config_init(&config);
    config.cell_time_ns = cfg->cell_ns;
    config.cell_ns_min = cfg->cell_ns_min;
    config.cell_ns_max = cfg->cell_ns_max;
    config.clock_gain_q16 = cfg->alpha_q16;
    config.max_run_cells = cfg->max_run_cells;
    
    /* Decode */
    uft_pll_result_t result;
    int ret = uft_pll_decode(timestamps_ns, count, &config, &result);
    
    if (ret < 0) {
        return 0;
    }
    
    /* Copy output */
    size_t bits_to_copy = result.bit_count;
    if (bits_to_copy > out_bits_capacity_bits) {
        bits_to_copy = out_bits_capacity_bits;
    }
    
    size_t bytes = (bits_to_copy + 7) / 8;
    memcpy(out_bits, result.bitstream, bytes);
    
    if (out_final_cell_ns) {
        *out_final_cell_ns = result.final_cell_ns;
    }
    if (out_dropped_transitions) {
        *out_dropped_transitions = result.dropped_transitions;
    }
    
    uft_pll_result_free(&result);
    return bits_to_copy;
}

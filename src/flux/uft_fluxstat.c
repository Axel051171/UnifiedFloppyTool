/**
 * @file uft_fluxstat.c
 * @brief Statistical Flux Recovery Implementation
 * @version 1.0.0
 * @date 2026-01-06
 *
 * Multi-pass flux analysis for recovering marginal sectors.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "uft/flux/uft_fluxstat.h"

/*============================================================================
 * Internal Structures
 *============================================================================*/

/**
 * @brief Internal context structure
 */
struct uft_fluxstat_ctx {
    uft_fluxstat_config_t config;
    
    /* Pass data */
    int pass_count;
    struct {
        uint32_t* flux_data;
        size_t    flux_count;
        uint32_t  index_time_ns;
        bool      allocated;
    } passes[UFT_FLUXSTAT_MAX_PASSES];
    
    /* Correlation data */
    bool correlated;
    size_t bit_count;
    uft_fluxstat_bit_t* bits;
    
    /* Histogram */
    uint32_t histogram[UFT_FLUXSTAT_HIST_BINS];
    bool histogram_valid;
};

/*============================================================================
 * Lifecycle Functions
 *============================================================================*/

uft_fluxstat_ctx_t* uft_fluxstat_create(void)
{
    uft_fluxstat_ctx_t* ctx = calloc(1, sizeof(uft_fluxstat_ctx_t));
    if (!ctx) return NULL;
    
    /* Set defaults */
    ctx->config = uft_fluxstat_default_config();
    
    return ctx;
}

void uft_fluxstat_destroy(uft_fluxstat_ctx_t* ctx)
{
    if (!ctx) return;
    
    /* Free pass data */
    for (int i = 0; i < ctx->pass_count; i++) {
        if (ctx->passes[i].allocated && ctx->passes[i].flux_data) {
            free(ctx->passes[i].flux_data);
        }
    }
    
    /* Free correlation data */
    if (ctx->bits) {
        free(ctx->bits);
    }
    
    free(ctx);
}

int uft_fluxstat_configure(uft_fluxstat_ctx_t* ctx, 
                           const uft_fluxstat_config_t* config)
{
    if (!ctx || !config) return UFT_FLUXSTAT_ERR_NULLPTR;
    
    if (config->pass_count < UFT_FLUXSTAT_MIN_PASSES ||
        config->pass_count > UFT_FLUXSTAT_MAX_PASSES) {
        return UFT_FLUXSTAT_ERR_INVALID;
    }
    
    ctx->config = *config;
    return UFT_FLUXSTAT_OK;
}

int uft_fluxstat_get_config(uft_fluxstat_ctx_t* ctx,
                            uft_fluxstat_config_t* config)
{
    if (!ctx || !config) return UFT_FLUXSTAT_ERR_NULLPTR;
    *config = ctx->config;
    return UFT_FLUXSTAT_OK;
}

uft_fluxstat_config_t uft_fluxstat_default_config(void)
{
    uft_fluxstat_config_t config = {
        .pass_count = UFT_FLUXSTAT_DEFAULT_PASSES,
        .confidence_threshold = 75,
        .max_correction_bits = 3,
        .encoding = UFT_FLUXSTAT_ENC_MFM,
        .data_rate = 250000,
        .use_crc_correction = true,
        .preserve_weak_bits = true
    };
    return config;
}

/*============================================================================
 * Multi-Pass Management
 *============================================================================*/

int uft_fluxstat_add_pass(uft_fluxstat_ctx_t* ctx,
                          const uint32_t* flux_data,
                          size_t flux_count,
                          uint32_t index_time_ns)
{
    if (!ctx || !flux_data) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (flux_count == 0) return UFT_FLUXSTAT_ERR_INVALID;
    if (ctx->pass_count >= UFT_FLUXSTAT_MAX_PASSES) return UFT_FLUXSTAT_ERR_OVERFLOW;
    
    int idx = ctx->pass_count;
    
    /* Allocate and copy flux data */
    ctx->passes[idx].flux_data = malloc(flux_count * sizeof(uint32_t));
    if (!ctx->passes[idx].flux_data) return UFT_FLUXSTAT_ERR_MEMORY;
    
    memcpy(ctx->passes[idx].flux_data, flux_data, flux_count * sizeof(uint32_t));
    ctx->passes[idx].flux_count = flux_count;
    ctx->passes[idx].index_time_ns = index_time_ns;
    ctx->passes[idx].allocated = true;
    
    ctx->pass_count++;
    ctx->correlated = false;
    ctx->histogram_valid = false;
    
    return idx;
}

void uft_fluxstat_clear_passes(uft_fluxstat_ctx_t* ctx)
{
    if (!ctx) return;
    
    for (int i = 0; i < ctx->pass_count; i++) {
        if (ctx->passes[i].allocated && ctx->passes[i].flux_data) {
            free(ctx->passes[i].flux_data);
            ctx->passes[i].flux_data = NULL;
        }
        ctx->passes[i].flux_count = 0;
        ctx->passes[i].allocated = false;
    }
    
    ctx->pass_count = 0;
    ctx->correlated = false;
    ctx->histogram_valid = false;
    
    if (ctx->bits) {
        free(ctx->bits);
        ctx->bits = NULL;
    }
    ctx->bit_count = 0;
}

int uft_fluxstat_pass_count(uft_fluxstat_ctx_t* ctx)
{
    return ctx ? ctx->pass_count : 0;
}

int uft_fluxstat_get_capture(uft_fluxstat_ctx_t* ctx,
                              uft_fluxstat_capture_t* capture)
{
    if (!ctx || !capture) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (ctx->pass_count == 0) return UFT_FLUXSTAT_ERR_NO_DATA;
    
    memset(capture, 0, sizeof(*capture));
    capture->pass_count = ctx->pass_count;
    
    uint32_t total_flux = 0;
    uint32_t min_flux = UINT32_MAX;
    uint32_t max_flux = 0;
    uint64_t total_rpm = 0;
    
    for (int i = 0; i < ctx->pass_count; i++) {
        capture->passes[i].flux_count = ctx->passes[i].flux_count;
        capture->passes[i].index_time_ns = ctx->passes[i].index_time_ns;
        capture->passes[i].flux_data = ctx->passes[i].flux_data;
        
        total_flux += ctx->passes[i].flux_count;
        if (ctx->passes[i].flux_count < min_flux) min_flux = ctx->passes[i].flux_count;
        if (ctx->passes[i].flux_count > max_flux) max_flux = ctx->passes[i].flux_count;
        
        if (ctx->passes[i].index_time_ns > 0) {
            total_rpm += uft_fluxstat_calculate_rpm(ctx->passes[i].index_time_ns);
        }
    }
    
    capture->total_flux = total_flux;
    capture->min_flux = min_flux;
    capture->max_flux = max_flux;
    capture->avg_rpm = (uint32_t)(total_rpm / ctx->pass_count);
    
    return UFT_FLUXSTAT_OK;
}

/*============================================================================
 * Histogram Analysis
 *============================================================================*/

int uft_fluxstat_compute_histogram(uft_fluxstat_ctx_t* ctx,
                                    uft_fluxstat_histogram_t* hist)
{
    if (!ctx || !hist) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (ctx->pass_count == 0) return UFT_FLUXSTAT_ERR_NO_DATA;
    
    memset(hist, 0, sizeof(*hist));
    memset(ctx->histogram, 0, sizeof(ctx->histogram));
    
    hist->interval_min = UINT16_MAX;
    hist->interval_max = 0;
    
    /* Accumulate histogram from all passes */
    for (int p = 0; p < ctx->pass_count; p++) {
        for (size_t i = 0; i < ctx->passes[p].flux_count; i++) {
            uint32_t val = ctx->passes[p].flux_data[i];
            
            /* Scale to histogram bins (assume ~4us max → 256 bins) */
            uint32_t bin = val >> 4;  /* Divide by 16 for binning */
            if (bin >= UFT_FLUXSTAT_HIST_BINS) {
                hist->overflow_count++;
                bin = UFT_FLUXSTAT_HIST_BINS - 1;
            }
            
            ctx->histogram[bin]++;
            hist->bins[bin]++;
            hist->total_count++;
            
            if (val < hist->interval_min) hist->interval_min = val;
            if (val > hist->interval_max) hist->interval_max = val;
        }
    }
    
    /* Find peak */
    uint32_t peak_count = 0;
    for (int i = 0; i < UFT_FLUXSTAT_HIST_BINS; i++) {
        if (hist->bins[i] > peak_count) {
            peak_count = hist->bins[i];
            hist->peak_bin = i;
        }
    }
    hist->peak_count = peak_count;
    
    /* Calculate mean */
    uint64_t sum = 0;
    for (int i = 0; i < UFT_FLUXSTAT_HIST_BINS; i++) {
        sum += (uint64_t)i * hist->bins[i];
    }
    if (hist->total_count > 0) {
        hist->mean_interval = (uint16_t)((sum << 4) / hist->total_count);
    }
    
    ctx->histogram_valid = true;
    return UFT_FLUXSTAT_OK;
}

int uft_fluxstat_estimate_rate(uft_fluxstat_ctx_t* ctx,
                                uint32_t* rate_bps)
{
    if (!ctx || !rate_bps) return UFT_FLUXSTAT_ERR_NULLPTR;
    
    uft_fluxstat_histogram_t hist;
    if (!ctx->histogram_valid) {
        int err = uft_fluxstat_compute_histogram(ctx, &hist);
        if (err != UFT_FLUXSTAT_OK) return err;
    }
    
    /* Peak bin gives nominal cell time */
    /* Assume 24MHz tick frequency */
    uint32_t cell_ticks = (ctx->histogram[0] > 0) ? 
                          (hist.peak_bin << 4) : 48;  /* Default 2us */
    
    if (cell_ticks > 0) {
        *rate_bps = 24000000 / cell_ticks;
    } else {
        *rate_bps = 250000;  /* Default */
    }
    
    return UFT_FLUXSTAT_OK;
}

int uft_fluxstat_detect_encoding(uft_fluxstat_ctx_t* ctx,
                                  uft_fluxstat_encoding_t* encoding)
{
    if (!ctx || !encoding) return UFT_FLUXSTAT_ERR_NULLPTR;
    
    uint32_t rate;
    int err = uft_fluxstat_estimate_rate(ctx, &rate);
    if (err != UFT_FLUXSTAT_OK) return err;
    
    /* Simple heuristic based on data rate */
    if (rate >= 450000) {
        *encoding = UFT_FLUXSTAT_ENC_MFM;  /* HD MFM */
    } else if (rate >= 280000) {
        *encoding = UFT_FLUXSTAT_ENC_AMIGA;  /* Amiga or Atari */
    } else if (rate >= 220000) {
        *encoding = UFT_FLUXSTAT_ENC_MFM;  /* DD MFM */
    } else {
        *encoding = UFT_FLUXSTAT_ENC_GCR;  /* Likely GCR */
    }
    
    return UFT_FLUXSTAT_OK;
}

/*============================================================================
 * Correlation Analysis
 *============================================================================*/

/**
 * @brief Simple PLL for flux-to-bits conversion
 */
static size_t flux_to_bits(const uint32_t* flux, size_t count,
                           uint8_t* bits, size_t max_bits,
                           uint32_t cell_time)
{
    if (!flux || !bits || count == 0 || cell_time == 0) return 0;
    
    size_t bit_pos = 0;
    int32_t phase = 0;
    int32_t pump = cell_time * 16;
    
    for (size_t i = 0; i < count && bit_pos < max_bits; i++) {
        int32_t pulse = flux[i] * 16;
        int32_t pos = phase + pulse;
        
        /* Count cells until pulse */
        while (pos > pump && bit_pos < max_bits) {
            bits[bit_pos++] = 0;
            phase += pump;
            pos -= pump;
        }
        
        if (bit_pos < max_bits) {
            bits[bit_pos++] = 1;
        }
        
        /* Phase correction */
        int32_t error = pos - (pump / 2);
        pump += error / 4;
        
        /* Clamp pump */
        int32_t min_pump = (cell_time * 16 * 82) / 100;
        int32_t max_pump = (cell_time * 16 * 118) / 100;
        if (pump < min_pump) pump = min_pump;
        if (pump > max_pump) pump = max_pump;
        
        phase = pos;
    }
    
    return bit_pos;
}

int uft_fluxstat_correlate(uft_fluxstat_ctx_t* ctx)
{
    if (!ctx) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (ctx->pass_count < UFT_FLUXSTAT_MIN_PASSES) return UFT_FLUXSTAT_ERR_NO_DATA;
    
    /* Estimate cell time from histogram */
    uint32_t rate;
    uft_fluxstat_estimate_rate(ctx, &rate);
    uint32_t cell_time = (rate > 0) ? (24000000 / rate) : 48;
    
    /* Estimate bit count from first pass */
    size_t est_bits = ctx->passes[0].flux_count * 2;  /* Rough estimate */
    
    /* Allocate temporary bit arrays */
    uint8_t** pass_bits = malloc(ctx->pass_count * sizeof(uint8_t*));
    size_t* pass_bit_counts = malloc(ctx->pass_count * sizeof(size_t));
    if (!pass_bits || !pass_bit_counts) {
        free(pass_bits);
        free(pass_bit_counts);
        return UFT_FLUXSTAT_ERR_MEMORY;
    }
    
    /* Convert each pass to bits */
    size_t min_bits = SIZE_MAX;
    for (int p = 0; p < ctx->pass_count; p++) {
        pass_bits[p] = malloc(est_bits);
        if (!pass_bits[p]) {
            for (int j = 0; j < p; j++) free(pass_bits[j]);
            free(pass_bits);
            free(pass_bit_counts);
            return UFT_FLUXSTAT_ERR_MEMORY;
        }
        
        pass_bit_counts[p] = flux_to_bits(
            ctx->passes[p].flux_data, ctx->passes[p].flux_count,
            pass_bits[p], est_bits, cell_time);
        
        if (pass_bit_counts[p] < min_bits) {
            min_bits = pass_bit_counts[p];
        }
    }
    
    /* Allocate correlation results */
    if (ctx->bits) free(ctx->bits);
    ctx->bits = calloc(min_bits, sizeof(uft_fluxstat_bit_t));
    if (!ctx->bits) {
        for (int p = 0; p < ctx->pass_count; p++) free(pass_bits[p]);
        free(pass_bits);
        free(pass_bit_counts);
        return UFT_FLUXSTAT_ERR_MEMORY;
    }
    ctx->bit_count = min_bits;
    
    /* Correlate bits across passes */
    for (size_t i = 0; i < min_bits; i++) {
        int ones = 0;
        for (int p = 0; p < ctx->pass_count; p++) {
            if (pass_bits[p][i]) ones++;
        }
        
        int confidence = (ones * 100) / ctx->pass_count;
        if (confidence < 50) {
            confidence = 100 - confidence;
            ctx->bits[i].value = 0;
        } else {
            ctx->bits[i].value = 1;
        }
        
        ctx->bits[i].confidence = confidence;
        ctx->bits[i].transition_count = ones;
        
        /* Classify */
        if (confidence >= UFT_CONF_STRONG) {
            ctx->bits[i].classification = ctx->bits[i].value ? 
                UFT_BITCELL_STRONG_1 : UFT_BITCELL_STRONG_0;
        } else if (confidence >= UFT_CONF_WEAK) {
            ctx->bits[i].classification = ctx->bits[i].value ?
                UFT_BITCELL_WEAK_1 : UFT_BITCELL_WEAK_0;
        } else {
            ctx->bits[i].classification = UFT_BITCELL_AMBIGUOUS;
        }
    }
    
    /* Cleanup */
    for (int p = 0; p < ctx->pass_count; p++) {
        free(pass_bits[p]);
    }
    free(pass_bits);
    free(pass_bit_counts);
    
    ctx->correlated = true;
    return UFT_FLUXSTAT_OK;
}

int uft_fluxstat_get_bits(uft_fluxstat_ctx_t* ctx,
                          uint32_t bit_offset,
                          uint32_t count,
                          uft_fluxstat_bit_t* bits)
{
    if (!ctx || !bits) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (!ctx->correlated) return UFT_FLUXSTAT_ERR_NO_DATA;
    if (bit_offset + count > ctx->bit_count) return UFT_FLUXSTAT_ERR_INVALID;
    
    memcpy(bits, &ctx->bits[bit_offset], count * sizeof(uft_fluxstat_bit_t));
    return UFT_FLUXSTAT_OK;
}

/*============================================================================
 * Track/Sector Recovery
 *============================================================================*/

int uft_fluxstat_recover_track(uft_fluxstat_ctx_t* ctx,
                                uft_fluxstat_track_t* track)
{
    if (!ctx || !track) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (!ctx->correlated) {
        int err = uft_fluxstat_correlate(ctx);
        if (err != UFT_FLUXSTAT_OK) return err;
    }
    
    memset(track, 0, sizeof(*track));
    
    /* Simple sector detection - find sync patterns */
    /* This is a simplified implementation */
    uint8_t min_conf = 100;
    uint64_t total_conf = 0;
    
    for (size_t i = 0; i < ctx->bit_count; i++) {
        if (ctx->bits[i].confidence < min_conf) {
            min_conf = ctx->bits[i].confidence;
        }
        total_conf += ctx->bits[i].confidence;
    }
    
    track->overall_confidence = (uint8_t)(total_conf / ctx->bit_count);
    
    /* Count weak bits */
    int weak_count = 0;
    for (size_t i = 0; i < ctx->bit_count; i++) {
        if (ctx->bits[i].classification == UFT_BITCELL_WEAK_0 ||
            ctx->bits[i].classification == UFT_BITCELL_WEAK_1 ||
            ctx->bits[i].classification == UFT_BITCELL_AMBIGUOUS) {
            weak_count++;
        }
    }
    
    /* Estimate sectors based on bit count and encoding */
    /* ~100000 bits/track for MFM DD → 9 sectors */
    if (ctx->config.encoding == UFT_FLUXSTAT_ENC_MFM) {
        track->sector_count = (ctx->bit_count > 80000) ? 9 : 
                              (ctx->bit_count > 40000) ? 5 : 1;
    } else {
        track->sector_count = (ctx->bit_count > 50000) ? 11 : 5;
    }
    
    /* For now, mark all as recovered if confidence is high enough */
    if (track->overall_confidence >= ctx->config.confidence_threshold) {
        track->sectors_recovered = track->sector_count;
    } else if (track->overall_confidence >= 50) {
        track->sectors_partial = track->sector_count;
    } else {
        track->sectors_failed = track->sector_count;
    }
    
    return UFT_FLUXSTAT_OK;
}

int uft_fluxstat_recover_sector(uft_fluxstat_ctx_t* ctx,
                                 uint8_t sector_num,
                                 uft_fluxstat_sector_t* sector)
{
    if (!ctx || !sector) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (!ctx->correlated) return UFT_FLUXSTAT_ERR_NO_DATA;
    
    memset(sector, 0, sizeof(*sector));
    sector->sector_num = sector_num;
    
    /* Calculate sector boundaries (simplified) */
    size_t bits_per_sector = ctx->bit_count / 9;  /* Assume 9 sectors */
    size_t start_bit = sector_num * bits_per_sector;
    size_t end_bit = start_bit + bits_per_sector;
    
    if (end_bit > ctx->bit_count) {
        end_bit = ctx->bit_count;
    }
    
    /* Extract sector data and statistics */
    uint8_t min_conf = 100;
    uint64_t total_conf = 0;
    int weak_count = 0;
    
    size_t byte_idx = 0;
    uint8_t current_byte = 0;
    int bit_in_byte = 0;
    
    for (size_t i = start_bit; i < end_bit && byte_idx < UFT_FLUXSTAT_MAX_SECTOR; i++) {
        /* Build bytes from bits */
        current_byte = (current_byte << 1) | ctx->bits[i].value;
        bit_in_byte++;
        
        if (bit_in_byte == 8) {
            sector->data[byte_idx++] = current_byte;
            current_byte = 0;
            bit_in_byte = 0;
        }
        
        /* Statistics */
        if (ctx->bits[i].confidence < min_conf) {
            min_conf = ctx->bits[i].confidence;
        }
        total_conf += ctx->bits[i].confidence;
        
        if (ctx->bits[i].classification == UFT_BITCELL_WEAK_0 ||
            ctx->bits[i].classification == UFT_BITCELL_WEAK_1 ||
            ctx->bits[i].classification == UFT_BITCELL_AMBIGUOUS) {
            if (weak_count < UFT_FLUXSTAT_MAX_WEAK_POS) {
                sector->weak_positions[weak_count] = (uint16_t)(i - start_bit);
            }
            weak_count++;
        }
    }
    
    sector->size = byte_idx;
    sector->confidence_min = min_conf;
    sector->confidence_avg = (uint8_t)(total_conf / (end_bit - start_bit));
    sector->weak_bit_count = weak_count;
    sector->recovered = (sector->confidence_avg >= ctx->config.confidence_threshold);
    
    return UFT_FLUXSTAT_OK;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

int uft_fluxstat_calculate_confidence(uft_fluxstat_ctx_t* ctx,
                                       const uint8_t* data,
                                       size_t length,
                                       uint8_t* min_conf,
                                       uint8_t* avg_conf)
{
    if (!ctx || !data || !min_conf || !avg_conf) return UFT_FLUXSTAT_ERR_NULLPTR;
    if (!ctx->correlated) return UFT_FLUXSTAT_ERR_NO_DATA;
    
    /* This would need bit-to-byte mapping */
    /* For now, return overall stats */
    *min_conf = 100;
    uint64_t total = 0;
    
    size_t check_bits = length * 8;
    if (check_bits > ctx->bit_count) check_bits = ctx->bit_count;
    
    for (size_t i = 0; i < check_bits; i++) {
        if (ctx->bits[i].confidence < *min_conf) {
            *min_conf = ctx->bits[i].confidence;
        }
        total += ctx->bits[i].confidence;
    }
    
    *avg_conf = (uint8_t)(total / check_bits);
    return UFT_FLUXSTAT_OK;
}

const char* uft_fluxstat_class_name(uft_bitcell_class_t classification)
{
    switch (classification) {
        case UFT_BITCELL_STRONG_1:   return "STRONG_1";
        case UFT_BITCELL_WEAK_1:     return "WEAK_1";
        case UFT_BITCELL_STRONG_0:   return "STRONG_0";
        case UFT_BITCELL_WEAK_0:     return "WEAK_0";
        case UFT_BITCELL_AMBIGUOUS:  return "AMBIGUOUS";
        default:                     return "UNKNOWN";
    }
}

uint32_t uft_fluxstat_calculate_rpm(uint32_t index_time_ns)
{
    if (index_time_ns == 0) return 0;
    /* RPM = 60 seconds / (index_time in seconds) */
    /* = 60,000,000,000 ns / index_time_ns */
    return (uint32_t)(60000000000ULL / index_time_ns);
}

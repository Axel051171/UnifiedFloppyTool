/**
 * @file uft_pll_unified.c
 * @brief Unified PLL Controller Implementation (W-P1-005)
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_pll_unified.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * PRESET CONFIGURATIONS
 *===========================================================================*/

static const uft_pll_config_t PRESET_CONFIGS[] = {
    /* AUTO - same as IBM DD */
    [UFT_PLL_PRESET_AUTO] = {
        .base = { .bitcell_ns = 4000, .clock_min_ns = 3400, .clock_max_ns = 4600,
                  .clock_centre_ns = 4000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2 },
        .algorithm = UFT_PLL_ALGO_ADAPTIVE, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 100, .max_zeros = 32, .track_quality = true
    },
    /* IBM DD */
    [UFT_PLL_PRESET_IBM_DD] = {
        .base = { .bitcell_ns = 4000, .clock_min_ns = 3400, .clock_max_ns = 4600,
                  .clock_centre_ns = 4000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2 },
        .algorithm = UFT_PLL_ALGO_DPLL, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 100, .max_zeros = 32, .track_quality = true
    },
    /* IBM HD */
    [UFT_PLL_PRESET_IBM_HD] = {
        .base = { .bitcell_ns = 2000, .clock_min_ns = 1700, .clock_max_ns = 2300,
                  .clock_centre_ns = 2000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2 },
        .algorithm = UFT_PLL_ALGO_DPLL, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 50, .max_zeros = 32, .track_quality = true
    },
    /* Amiga DD */
    [UFT_PLL_PRESET_AMIGA_DD] = {
        .base = { .bitcell_ns = 2000, .clock_min_ns = 1700, .clock_max_ns = 2300,
                  .clock_centre_ns = 2000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2 },
        .algorithm = UFT_PLL_ALGO_DPLL, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 50, .max_zeros = 32, .track_quality = true
    },
    /* Amiga HD */
    [UFT_PLL_PRESET_AMIGA_HD] = {
        .base = { .bitcell_ns = 1000, .clock_min_ns = 850, .clock_max_ns = 1150,
                  .clock_centre_ns = 1000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2 },
        .algorithm = UFT_PLL_ALGO_DPLL, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 25, .max_zeros = 32, .track_quality = true
    },
    /* C64/1541 */
    [UFT_PLL_PRESET_C64] = {
        .base = { .bitcell_ns = 3200, .clock_min_ns = 2700, .clock_max_ns = 4600,
                  .clock_centre_ns = 3600, .pll_adjust_percent = 20, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 3,
                  .use_gcr = true },
        .algorithm = UFT_PLL_ALGO_ADAPTIVE, .gain_p = 0.5f, .gain_i = 0.15f,
        .noise_filter_ns = 100, .max_zeros = 10, .track_quality = true, .adaptive_gain = true
    },
    /* Apple II */
    [UFT_PLL_PRESET_APPLE2] = {
        .base = { .bitcell_ns = 4000, .clock_min_ns = 3400, .clock_max_ns = 4600,
                  .clock_centre_ns = 4000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2,
                  .use_gcr = true },
        .algorithm = UFT_PLL_ALGO_DPLL, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 100, .max_zeros = 10, .track_quality = true
    },
    /* Mac 400K */
    [UFT_PLL_PRESET_MAC_400K] = {
        .base = { .bitcell_ns = 2000, .clock_min_ns = 1600, .clock_max_ns = 2600,
                  .clock_centre_ns = 2000, .pll_adjust_percent = 20, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 3,
                  .use_gcr = true },
        .algorithm = UFT_PLL_ALGO_ADAPTIVE, .gain_p = 0.5f, .gain_i = 0.15f,
        .noise_filter_ns = 50, .max_zeros = 10, .track_quality = true, .adaptive_gain = true
    },
    /* Mac 800K */
    [UFT_PLL_PRESET_MAC_800K] = {
        .base = { .bitcell_ns = 2000, .clock_min_ns = 1600, .clock_max_ns = 2600,
                  .clock_centre_ns = 2000, .pll_adjust_percent = 20, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 3,
                  .use_gcr = true },
        .algorithm = UFT_PLL_ALGO_ADAPTIVE, .gain_p = 0.5f, .gain_i = 0.15f,
        .noise_filter_ns = 50, .max_zeros = 10, .track_quality = true, .adaptive_gain = true
    },
    /* Atari ST */
    [UFT_PLL_PRESET_ATARI_ST] = {
        .base = { .bitcell_ns = 4000, .clock_min_ns = 3400, .clock_max_ns = 4600,
                  .clock_centre_ns = 4000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2 },
        .algorithm = UFT_PLL_ALGO_DPLL, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 100, .max_zeros = 32, .track_quality = true
    },
    /* FM Single Density */
    [UFT_PLL_PRESET_FM_SD] = {
        .base = { .bitcell_ns = 8000, .clock_min_ns = 6800, .clock_max_ns = 9200,
                  .clock_centre_ns = 8000, .pll_adjust_percent = 15, .pll_phase_percent = 60,
                  .flux_scale_percent = 100, .sync_bits_required = 256, .jitter_percent = 2,
                  .use_fm = true },
        .algorithm = UFT_PLL_ALGO_DPLL, .gain_p = 0.6f, .gain_i = 0.1f,
        .noise_filter_ns = 200, .max_zeros = 64, .track_quality = true
    }
};

/*===========================================================================
 * INTERNAL STATE
 *===========================================================================*/

struct uft_pll_context {
    uft_pll_config_t config;
    uft_pll_stats_t stats;
    
    /* PLL state */
    int clock;                  /* Current clock period estimate */
    int phase;                  /* Phase accumulator */
    int zeros;                  /* Consecutive zeros */
    int good_bits;              /* Bits since last resync */
    bool synced;                /* Sync established */
    
    /* PI controller state */
    float integral;             /* Integral accumulator */
    float last_error;           /* Last phase error */
    
    /* Adaptive state */
    float avg_period;           /* Running average period */
    int period_samples;         /* Samples for average */
};

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

uft_pll_context_t* uft_pll_create(const uft_pll_config_t *config) {
    uft_pll_context_t *ctx = calloc(1, sizeof(uft_pll_context_t));
    if (!ctx) return NULL;
    
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = UFT_PLL_CONFIG_DEFAULT;
    }
    
    uft_pll_context_reset(ctx);
    return ctx;
}

uft_pll_context_t* uft_pll_create_preset(uft_pll_preset_t preset) {
    if (preset >= UFT_PLL_PRESET_COUNT) {
        preset = UFT_PLL_PRESET_IBM_DD;
    }
    return uft_pll_create(&PRESET_CONFIGS[preset]);
}

void uft_pll_destroy(uft_pll_context_t *ctx) {
    free(ctx);
}

void uft_pll_context_reset(uft_pll_context_t *ctx) {
    if (!ctx) return;
    
    ctx->clock = ctx->config.base.clock_centre_ns;
    ctx->phase = 0;
    ctx->zeros = 0;
    ctx->good_bits = 0;
    ctx->synced = false;
    ctx->integral = 0.0f;
    ctx->last_error = 0.0f;
    ctx->avg_period = (float)ctx->config.base.clock_centre_ns;
    ctx->period_samples = 0;
    
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.min_bitcell_ns = 1e9;
}

/*===========================================================================
 * CONFIGURATION
 *===========================================================================*/

const uft_pll_config_t* uft_pll_get_config(const uft_pll_context_t *ctx) {
    return ctx ? &ctx->config : NULL;
}

int uft_pll_set_config(uft_pll_context_t *ctx, const uft_pll_config_t *config) {
    if (!ctx || !config) return -1;
    ctx->config = *config;
    uft_pll_context_reset(ctx);
    return 0;
}

int uft_pll_apply_preset(uft_pll_context_t *ctx, uft_pll_preset_t preset) {
    if (!ctx || preset >= UFT_PLL_PRESET_COUNT) return -1;
    ctx->config = PRESET_CONFIGS[preset];
    uft_pll_context_reset(ctx);
    return 0;
}

int uft_pll_set_algorithm(uft_pll_context_t *ctx, uft_pll_algo_t algo) {
    if (!ctx || algo >= UFT_PLL_ALGO_COUNT) return -1;
    ctx->config.algorithm = algo;
    return 0;
}

int uft_pll_set_bitcell(uft_pll_context_t *ctx, int bitcell_ns) {
    if (!ctx || bitcell_ns <= 0) return -1;
    
    ctx->config.base.bitcell_ns = bitcell_ns;
    ctx->config.base.clock_centre_ns = bitcell_ns;
    
    int adjust = (bitcell_ns * ctx->config.base.pll_adjust_percent) / 100;
    ctx->config.base.clock_min_ns = bitcell_ns - adjust;
    ctx->config.base.clock_max_ns = bitcell_ns + adjust;
    
    uft_pll_context_reset(ctx);
    return 0;
}

/*===========================================================================
 * DECODING - DPLL ALGORITHM
 *===========================================================================*/

static int dpll_process(uft_pll_context_t *ctx, int flux_ns) {
    const uft_pll_params_t *p = &ctx->config.base;
    
    /* Accumulate flux time */
    ctx->phase += flux_ns;
    
    /* How many bit cells? */
    int bits = 0;
    int bit_value = -1;
    
    while (ctx->phase >= ctx->clock / 2) {
        if (ctx->phase >= ctx->clock * 3 / 2) {
            /* Zero bit (no transition in window) */
            bit_value = 0;
            ctx->zeros++;
        } else {
            /* One bit (transition in window) */
            bit_value = 1;
            ctx->zeros = 0;
            
            /* Phase adjustment */
            int error = ctx->phase - ctx->clock;
            
            /* Clock adjustment */
            int clock_adjust = (error * p->pll_adjust_percent) / 100;
            ctx->clock += clock_adjust;
            
            /* Clamp clock */
            if (ctx->clock < p->clock_min_ns) ctx->clock = p->clock_min_ns;
            if (ctx->clock > p->clock_max_ns) ctx->clock = p->clock_max_ns;
            
            /* Update stats */
            if (ctx->config.track_quality) {
                double abs_error = (error >= 0) ? error : -error;
                ctx->stats.phase_error_avg = 
                    ctx->stats.phase_error_avg * 0.99 + abs_error * 0.01;
                if (abs_error > ctx->stats.phase_error_max) {
                    ctx->stats.phase_error_max = abs_error;
                }
            }
        }
        
        ctx->phase -= ctx->clock;
        bits++;
        
        /* Sync check */
        if (ctx->zeros > ctx->config.max_zeros) {
            if (ctx->synced) {
                ctx->synced = false;
                ctx->stats.sync_losses++;
            }
            ctx->clock = p->clock_centre_ns;
        }
    }
    
    /* Update sync state */
    if (bit_value == 1) {
        ctx->good_bits++;
        if (!ctx->synced && ctx->good_bits >= p->sync_bits_required) {
            ctx->synced = true;
            ctx->stats.sync_recoveries++;
        }
    }
    
    return bit_value;
}

/*===========================================================================
 * DECODING - PI CONTROLLER
 *===========================================================================*/

static int pi_process(uft_pll_context_t *ctx, int flux_ns) {
    const uft_pll_params_t *p = &ctx->config.base;
    
    ctx->phase += flux_ns;
    int bit_value = -1;
    
    while (ctx->phase >= ctx->clock / 2) {
        if (ctx->phase >= ctx->clock * 3 / 2) {
            bit_value = 0;
            ctx->zeros++;
        } else {
            bit_value = 1;
            ctx->zeros = 0;
            
            /* PI controller */
            float error = (float)(ctx->phase - ctx->clock);
            ctx->integral += error * ctx->config.gain_i;
            
            /* Anti-windup */
            float max_integral = (float)p->clock_centre_ns * 0.5f;
            if (ctx->integral > max_integral) ctx->integral = max_integral;
            if (ctx->integral < -max_integral) ctx->integral = -max_integral;
            
            float correction = error * ctx->config.gain_p + ctx->integral;
            ctx->clock += (int)correction;
            
            /* Clamp */
            if (ctx->clock < p->clock_min_ns) ctx->clock = p->clock_min_ns;
            if (ctx->clock > p->clock_max_ns) ctx->clock = p->clock_max_ns;
            
            ctx->last_error = error;
        }
        
        ctx->phase -= ctx->clock;
        
        if (ctx->zeros > ctx->config.max_zeros) {
            ctx->synced = false;
            ctx->clock = p->clock_centre_ns;
            ctx->integral = 0;
        }
    }
    
    if (bit_value == 1) {
        ctx->good_bits++;
        if (!ctx->synced && ctx->good_bits >= p->sync_bits_required) {
            ctx->synced = true;
        }
    }
    
    return bit_value;
}

/*===========================================================================
 * DECODING - ADAPTIVE
 *===========================================================================*/

static int adaptive_process(uft_pll_context_t *ctx, int flux_ns) {
    /* Use DPLL with adaptive gain */
    int result = dpll_process(ctx, flux_ns);
    
    if (ctx->config.adaptive_gain && result >= 0) {
        /* Adjust gain based on signal quality */
        float quality = ctx->synced ? 1.0f : 0.5f;
        ctx->config.gain_p = 0.4f + 0.4f * quality;
        ctx->config.gain_i = 0.05f + 0.1f * quality;
    }
    
    return result;
}

/*===========================================================================
 * MAIN PROCESS FUNCTION
 *===========================================================================*/

int uft_pll_process(uft_pll_context_t *ctx, int flux_ns, int *bit_out) {
    if (!ctx || !bit_out) return -1;
    
    /* Noise filter */
    if (flux_ns < ctx->config.noise_filter_ns) {
        *bit_out = -1;
        return 0;
    }
    
    int bit;
    
    switch (ctx->config.algorithm) {
        case UFT_PLL_ALGO_PI:
            bit = pi_process(ctx, flux_ns);
            break;
        case UFT_PLL_ALGO_ADAPTIVE:
            bit = adaptive_process(ctx, flux_ns);
            break;
        case UFT_PLL_ALGO_DPLL:
        default:
            bit = dpll_process(ctx, flux_ns);
            break;
    }
    
    *bit_out = bit;
    
    /* Update statistics */
    if (bit >= 0 && ctx->config.track_quality) {
        ctx->stats.bits_decoded++;
        if (bit == 0) ctx->stats.zeros_decoded++;
        else ctx->stats.ones_decoded++;
        
        /* Track bitcell timing */
        double period = (double)flux_ns;
        if (period < ctx->stats.min_bitcell_ns) ctx->stats.min_bitcell_ns = period;
        if (period > ctx->stats.max_bitcell_ns) ctx->stats.max_bitcell_ns = period;
        ctx->stats.avg_bitcell_ns = ctx->stats.avg_bitcell_ns * 0.999 + period * 0.001;
    }
    
    return 0;
}

int uft_pll_decode_flux(
    uft_pll_context_t *ctx,
    const int *flux_ns,
    size_t flux_count,
    uint8_t *bits_out,
    size_t bits_capacity)
{
    if (!ctx || !flux_ns || !bits_out) return -1;
    
    size_t bit_count = 0;
    size_t byte_pos = 0;
    int bit_pos = 7;
    
    memset(bits_out, 0, bits_capacity);
    
    for (size_t i = 0; i < flux_count && byte_pos < bits_capacity; i++) {
        int bit;
        if (uft_pll_process(ctx, flux_ns[i], &bit) < 0) continue;
        if (bit < 0) continue;
        
        if (bit) {
            bits_out[byte_pos] |= (1 << bit_pos);
        }
        
        bit_pos--;
        if (bit_pos < 0) {
            bit_pos = 7;
            byte_pos++;
        }
        bit_count++;
    }
    
    return (int)bit_count;
}

void uft_pll_index(uft_pll_context_t *ctx) {
    if (!ctx) return;
    /* Reset phase on index for track alignment */
    ctx->phase = 0;
}

/*===========================================================================
 * QUALITY
 *===========================================================================*/

const uft_pll_stats_t* uft_pll_get_stats(const uft_pll_context_t *ctx) {
    return ctx ? &ctx->stats : NULL;
}

void uft_pll_reset_stats(uft_pll_context_t *ctx) {
    if (!ctx) return;
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    ctx->stats.min_bitcell_ns = 1e9;
}

bool uft_pll_is_synced(const uft_pll_context_t *ctx) {
    return ctx ? ctx->synced : false;
}

int uft_pll_get_quality(const uft_pll_context_t *ctx) {
    if (!ctx) return 0;
    
    /* Calculate quality from statistics */
    int quality = 50;  /* Base */
    
    if (ctx->synced) quality += 20;
    if (ctx->stats.sync_losses == 0) quality += 10;
    
    /* Phase error contribution */
    if (ctx->stats.phase_error_avg < ctx->config.base.clock_centre_ns * 0.1) {
        quality += 20;
    } else if (ctx->stats.phase_error_avg < ctx->config.base.clock_centre_ns * 0.2) {
        quality += 10;
    }
    
    if (quality > 100) quality = 100;
    return quality;
}

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

const uft_pll_config_t* uft_pll_get_preset_config(uft_pll_preset_t preset) {
    if (preset >= UFT_PLL_PRESET_COUNT) return NULL;
    return &PRESET_CONFIGS[preset];
}

uft_pll_preset_t uft_pll_detect_preset(const int *flux_ns, size_t count) {
    if (!flux_ns || count < 100) return UFT_PLL_PRESET_IBM_DD;
    
    /* Calculate average period */
    double avg = 0;
    for (size_t i = 0; i < count && i < 1000; i++) {
        avg += flux_ns[i];
    }
    avg /= (count < 1000 ? count : 1000);
    
    /* Match to preset */
    if (avg > 6000) return UFT_PLL_PRESET_FM_SD;
    if (avg > 3500) return UFT_PLL_PRESET_IBM_DD;
    if (avg > 2500) return UFT_PLL_PRESET_C64;
    if (avg > 1500) return UFT_PLL_PRESET_IBM_HD;
    return UFT_PLL_PRESET_AMIGA_HD;
}

const char* uft_pll_algo_name(uft_pll_algo_t algo) {
    if (algo >= UFT_PLL_ALGO_COUNT) return "Unknown";
    return UFT_PLL_ALGO_NAMES[algo];
}

const char* uft_pll_preset_name(uft_pll_preset_t preset) {
    if (preset >= UFT_PLL_PRESET_COUNT) return "Unknown";
    return UFT_PLL_PRESET_NAMES[preset];
}

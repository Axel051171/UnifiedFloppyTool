/**
 * @file uft_custom_encoder.c
 * @brief Custom Flux Encoder Implementation
 * 
 * EXT4-008: Programmable flux pattern encoder
 * 
 * Features:
 * - Custom encoding rules
 * - Protection pattern generation
 * - Timing manipulation
 * - Weak bit injection
 * - Format-specific encoding
 */

#include "uft/flux/uft_custom_encoder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define DEFAULT_CELL_NS     2000    /* 2µs for MFM @ 250kbps */
#define MAX_RULES           64
#define MAX_PATTERN_LEN     256

/*===========================================================================
 * Encoder Context
 *===========================================================================*/

int uft_encoder_init(uft_encoder_ctx_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    ctx->cell_time_ns = DEFAULT_CELL_NS;
    ctx->jitter_ns = 0;
    ctx->encoding = UFT_ENC_MFM;
    
    ctx->rules = calloc(MAX_RULES, sizeof(uft_encode_rule_t));
    if (!ctx->rules) return -1;
    
    ctx->max_rules = MAX_RULES;
    
    return 0;
}

void uft_encoder_free(uft_encoder_ctx_t *ctx)
{
    if (ctx) {
        if (ctx->rules) free(ctx->rules);
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Configuration
 *===========================================================================*/

int uft_encoder_set_timing(uft_encoder_ctx_t *ctx,
                           uint32_t cell_time_ns,
                           uint32_t jitter_ns)
{
    if (!ctx) return -1;
    
    ctx->cell_time_ns = cell_time_ns;
    ctx->jitter_ns = jitter_ns;
    
    return 0;
}

int uft_encoder_set_encoding(uft_encoder_ctx_t *ctx, uft_enc_type_t encoding)
{
    if (!ctx) return -1;
    
    ctx->encoding = encoding;
    
    /* Set default cell time for encoding */
    switch (encoding) {
        case UFT_ENC_FM:
            ctx->cell_time_ns = 4000;  /* 4µs */
            break;
        case UFT_ENC_MFM:
            ctx->cell_time_ns = 2000;  /* 2µs @ 250kbps */
            break;
        case UFT_ENC_MFM_HD:
            ctx->cell_time_ns = 1000;  /* 1µs @ 500kbps */
            break;
        case UFT_ENC_GCR:
            ctx->cell_time_ns = 2500;  /* Variable */
            break;
        default:
            break;
    }
    
    return 0;
}

/*===========================================================================
 * Encoding Rules
 *===========================================================================*/

int uft_encoder_add_rule(uft_encoder_ctx_t *ctx, const uft_encode_rule_t *rule)
{
    if (!ctx || !rule || ctx->rule_count >= ctx->max_rules) return -1;
    
    ctx->rules[ctx->rule_count] = *rule;
    ctx->rule_count++;
    
    return 0;
}

int uft_encoder_add_sync(uft_encoder_ctx_t *ctx, 
                         size_t position,
                         const uint8_t *pattern, size_t pattern_len)
{
    if (!ctx || !pattern || pattern_len == 0) return -1;
    if (pattern_len > MAX_PATTERN_LEN) return -1;
    
    uft_encode_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    rule.type = UFT_RULE_SYNC;
    rule.position = position;
    rule.pattern_len = pattern_len;
    memcpy(rule.pattern, pattern, pattern_len);
    
    return uft_encoder_add_rule(ctx, &rule);
}

int uft_encoder_add_timing_mod(uft_encoder_ctx_t *ctx,
                               size_t start, size_t end,
                               int32_t offset_ns)
{
    if (!ctx) return -1;
    
    uft_encode_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    rule.type = UFT_RULE_TIMING;
    rule.position = start;
    rule.end_position = end;
    rule.timing_offset = offset_ns;
    
    return uft_encoder_add_rule(ctx, &rule);
}

int uft_encoder_add_weak_bit(uft_encoder_ctx_t *ctx,
                             size_t position, size_t length)
{
    if (!ctx) return -1;
    
    uft_encode_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    rule.type = UFT_RULE_WEAK;
    rule.position = position;
    rule.end_position = position + length;
    
    return uft_encoder_add_rule(ctx, &rule);
}

int uft_encoder_add_missing_clock(uft_encoder_ctx_t *ctx, size_t position)
{
    if (!ctx) return -1;
    
    uft_encode_rule_t rule;
    memset(&rule, 0, sizeof(rule));
    
    rule.type = UFT_RULE_MISSING_CLOCK;
    rule.position = position;
    
    return uft_encoder_add_rule(ctx, &rule);
}

/*===========================================================================
 * MFM Encoding
 *===========================================================================*/

static int encode_mfm(const uint8_t *data, size_t data_size,
                      uint32_t cell_time, uint8_t *prev_bit,
                      uint32_t *flux, size_t *flux_count, size_t max_flux)
{
    size_t f_idx = 0;
    uint32_t current_time = 0;
    uint8_t last_bit = prev_bit ? *prev_bit : 0;
    
    for (size_t i = 0; i < data_size; i++) {
        uint8_t byte = data[i];
        
        for (int b = 7; b >= 0; b--) {
            uint8_t bit = (byte >> b) & 1;
            
            /* Clock bit */
            bool clock = (bit == 0 && last_bit == 0);
            
            if (clock && f_idx < max_flux) {
                current_time += cell_time / 2;
                flux[f_idx++] = current_time;
            }
            
            current_time += cell_time / 2;
            
            /* Data bit */
            if (bit && f_idx < max_flux) {
                flux[f_idx++] = current_time;
            }
            
            current_time += cell_time / 2;
            last_bit = bit;
        }
    }
    
    if (prev_bit) *prev_bit = last_bit;
    *flux_count = f_idx;
    
    return 0;
}

/*===========================================================================
 * FM Encoding
 *===========================================================================*/

static int encode_fm(const uint8_t *data, size_t data_size,
                     uint32_t cell_time,
                     uint32_t *flux, size_t *flux_count, size_t max_flux)
{
    size_t f_idx = 0;
    uint32_t current_time = 0;
    
    for (size_t i = 0; i < data_size; i++) {
        uint8_t byte = data[i];
        
        for (int b = 7; b >= 0; b--) {
            uint8_t bit = (byte >> b) & 1;
            
            /* FM always has clock */
            if (f_idx < max_flux) {
                current_time += cell_time / 2;
                flux[f_idx++] = current_time;
            }
            
            current_time += cell_time / 2;
            
            /* Data bit */
            if (bit && f_idx < max_flux) {
                flux[f_idx++] = current_time;
            }
            
            current_time += cell_time / 2;
        }
    }
    
    *flux_count = f_idx;
    return 0;
}

/*===========================================================================
 * Main Encoding Function
 *===========================================================================*/

int uft_encoder_encode(uft_encoder_ctx_t *ctx,
                       const uint8_t *data, size_t data_size,
                       uint32_t *flux, size_t *flux_count)
{
    if (!ctx || !data || !flux || !flux_count) return -1;
    
    size_t max_flux = *flux_count;
    *flux_count = 0;
    
    /* Encode based on type */
    uint8_t prev_bit = 0;
    
    switch (ctx->encoding) {
        case UFT_ENC_FM:
            encode_fm(data, data_size, ctx->cell_time_ns, flux, flux_count, max_flux);
            break;
            
        case UFT_ENC_MFM:
        case UFT_ENC_MFM_HD:
            encode_mfm(data, data_size, ctx->cell_time_ns, &prev_bit,
                      flux, flux_count, max_flux);
            break;
            
        default:
            /* Default to MFM */
            encode_mfm(data, data_size, ctx->cell_time_ns, &prev_bit,
                      flux, flux_count, max_flux);
    }
    
    /* Apply rules */
    for (size_t r = 0; r < ctx->rule_count; r++) {
        const uft_encode_rule_t *rule = &ctx->rules[r];
        
        switch (rule->type) {
            case UFT_RULE_TIMING:
                /* Apply timing offset to range */
                for (size_t i = rule->position; 
                     i < rule->end_position && i < *flux_count; i++) {
                    flux[i] += rule->timing_offset;
                }
                break;
                
            case UFT_RULE_MISSING_CLOCK:
                /* Remove flux at position (creates missing clock) */
                if (rule->position < *flux_count) {
                    memmove(&flux[rule->position], 
                            &flux[rule->position + 1],
                            (*flux_count - rule->position - 1) * sizeof(uint32_t));
                    (*flux_count)--;
                }
                break;
                
            case UFT_RULE_WEAK:
                /* Mark flux as weak (randomize timing slightly) */
                for (size_t i = rule->position; 
                     i < rule->end_position && i < *flux_count; i++) {
                    /* Add random jitter - placeholder, would need RNG */
                    int32_t jitter = ((i * 17) % 200) - 100;  /* Pseudo-random */
                    flux[i] += jitter;
                }
                break;
                
            default:
                break;
        }
    }
    
    /* Apply global jitter if configured */
    if (ctx->jitter_ns > 0) {
        for (size_t i = 0; i < *flux_count; i++) {
            /* Simple pseudo-random jitter */
            int32_t jitter = ((i * 31 + 7) % (ctx->jitter_ns * 2)) - ctx->jitter_ns;
            flux[i] += jitter;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Protection Pattern Generation
 *===========================================================================*/

int uft_encoder_gen_copylock(uft_encoder_ctx_t *ctx,
                             uint32_t seed,
                             uint32_t *flux, size_t *flux_count)
{
    if (!ctx || !flux || !flux_count) return -1;
    
    size_t max_flux = *flux_count;
    size_t f_idx = 0;
    
    /* Generate CopyLock timing pattern */
    /* CopyLock uses specific timing sequences verified by LFSR */
    
    uint32_t current_time = 0;
    uint32_t lfsr = seed;
    
    /* Generate sync area */
    for (int i = 0; i < 64 && f_idx < max_flux; i++) {
        current_time += ctx->cell_time_ns * 2;
        flux[f_idx++] = current_time;
    }
    
    /* Generate key area with LFSR-based timing */
    for (int i = 0; i < 256 && f_idx < max_flux; i++) {
        /* LFSR step */
        uint32_t bit = ((lfsr >> 31) ^ (lfsr >> 29) ^ (lfsr >> 25) ^ (lfsr >> 24)) & 1;
        lfsr = (lfsr << 1) | bit;
        
        /* Generate flux with timing based on LFSR */
        uint32_t cell = ctx->cell_time_ns;
        if (lfsr & 0x01) {
            cell += 200;  /* Slightly longer */
        }
        if (lfsr & 0x02) {
            cell -= 100;  /* Slightly shorter */
        }
        
        current_time += cell;
        flux[f_idx++] = current_time;
    }
    
    *flux_count = f_idx;
    return 0;
}

int uft_encoder_gen_longtrack(uft_encoder_ctx_t *ctx,
                              uint32_t track_length_us,
                              uint32_t *flux, size_t *flux_count)
{
    if (!ctx || !flux || !flux_count) return -1;
    
    size_t max_flux = *flux_count;
    size_t f_idx = 0;
    
    /* Generate long track pattern */
    /* Normal track is ~200ms, long track is ~206ms+ */
    
    uint32_t current_time = 0;
    uint32_t target_time = track_length_us * 1000;  /* Convert to ns */
    
    while (current_time < target_time && f_idx < max_flux) {
        current_time += ctx->cell_time_ns * 2;
        flux[f_idx++] = current_time;
        
        /* Add occasional longer gaps */
        if ((f_idx % 1000) == 0) {
            current_time += ctx->cell_time_ns;  /* Extra gap */
        }
    }
    
    *flux_count = f_idx;
    return 0;
}

/*===========================================================================
 * Preset Patterns
 *===========================================================================*/

int uft_encoder_preset_mfm_sync(uint8_t *pattern, size_t *len)
{
    if (!pattern || !len || *len < 6) return -1;
    
    /* MFM A1 sync with missing clock (3x) */
    pattern[0] = 0x44;
    pattern[1] = 0x89;
    pattern[2] = 0x44;
    pattern[3] = 0x89;
    pattern[4] = 0x44;
    pattern[5] = 0x89;
    
    *len = 6;
    return 0;
}

int uft_encoder_preset_amiga_sync(uint8_t *pattern, size_t *len)
{
    if (!pattern || !len || *len < 4) return -1;
    
    /* Amiga double A1 sync */
    pattern[0] = 0x44;
    pattern[1] = 0x89;
    pattern[2] = 0x44;
    pattern[3] = 0x89;
    
    *len = 4;
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_encoder_report_json(const uft_encoder_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    const char *enc_name;
    switch (ctx->encoding) {
        case UFT_ENC_FM:     enc_name = "FM"; break;
        case UFT_ENC_MFM:    enc_name = "MFM"; break;
        case UFT_ENC_MFM_HD: enc_name = "MFM_HD"; break;
        case UFT_ENC_GCR:    enc_name = "GCR"; break;
        default:             enc_name = "Unknown";
    }
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"encoding\": \"%s\",\n"
        "  \"cell_time_ns\": %u,\n"
        "  \"jitter_ns\": %u,\n"
        "  \"rule_count\": %zu\n"
        "}",
        enc_name,
        ctx->cell_time_ns,
        ctx->jitter_ns,
        ctx->rule_count
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

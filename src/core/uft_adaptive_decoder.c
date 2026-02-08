/**
 * @file uft_adaptive_decoder.c
 * @brief Adaptive Flux-to-MFM Decoder mit Entropy-Tracking
 * @version 3.1.4.001
 * 
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/uft_adaptive_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * KONSTANTEN
 *============================================================================*/

/* Amiga MFM Sync Marker: 0x4489 4489 = "01000100 10001001 01000100 10001001" */
const uint8_t UFT_AMIGA_SYNC_MARKER[32] = {
    0, 1, 0, 0, 0, 1, 0, 0,  /* 44 */
    1, 0, 0, 0, 1, 0, 0, 1,  /* 89 */
    0, 1, 0, 0, 0, 1, 0, 0,  /* 44 */
    1, 0, 0, 0, 1, 0, 0, 1   /* 89 */
};

/* DiskSpare Marker: 0x4489 4489 + 0x2AAA */
const uint8_t UFT_AMIGA_DS_MARKER[48] = {
    0, 1, 0, 0, 0, 1, 0, 0,  /* 44 */
    1, 0, 0, 0, 1, 0, 0, 1,  /* 89 */
    0, 1, 0, 0, 0, 1, 0, 0,  /* 44 */
    1, 0, 0, 0, 1, 0, 0, 1,  /* 89 */
    0, 0, 1, 0, 1, 0, 1, 0,  /* 2A */
    1, 0, 1, 0, 1, 0, 1, 0   /* AA */
};

/*============================================================================
 * KONFIGURATION
 *============================================================================*/

uft_adaptive_config_t uft_adaptive_config_dd_default(void)
{
    return (uft_adaptive_config_t){
        .timing_4us = UFT_ADAPT_DD_4US,
        .timing_6us = UFT_ADAPT_DD_6US,
        .timing_8us = UFT_ADAPT_DD_8US,
        .rate_of_change = 8.0f,
        .lowpass_radius = 100,
        .offset = 0,
        .high_density = false,
        .track_entropy = false,
        .add_noise = false,
        .noise_amount = 0,
        .noise_start = 0,
        .noise_end = 0
    };
}

uft_adaptive_config_t uft_adaptive_config_hd_default(void)
{
    return (uft_adaptive_config_t){
        .timing_4us = UFT_ADAPT_HD_4US,
        .timing_6us = UFT_ADAPT_HD_6US,
        .timing_8us = UFT_ADAPT_HD_8US,
        .rate_of_change = 8.0f,
        .lowpass_radius = 100,
        .offset = 0,
        .high_density = true,
        .track_entropy = false,
        .add_noise = false,
        .noise_amount = 0,
        .noise_start = 0,
        .noise_end = 0
    };
}

/*============================================================================
 * INITIALISIERUNG
 *============================================================================*/

bool uft_adaptive_init(uft_adaptive_state_t* state, const uft_adaptive_config_t* config)
{
    if (!state || !config) {
        return false;
    }
    
    memset(state, 0, sizeof(*state));
    state->config = *config;
    
    /* Lowpass-Filter allokieren falls aktiviert */
    if (config->lowpass_radius > 0) {
        int32_t radius = config->lowpass_radius;
        if (radius > UFT_ADAPT_MAX_LOWPASS) {
            radius = UFT_ADAPT_MAX_LOWPASS;
        }
        
        state->lowpass_4us = (float*)calloc((size_t)radius, sizeof(float));
        state->lowpass_6us = (float*)calloc((size_t)radius, sizeof(float));
        state->lowpass_8us = (float*)calloc((size_t)radius, sizeof(float));
        
        if (!state->lowpass_4us || !state->lowpass_6us || !state->lowpass_8us) {
            uft_adaptive_destroy(state);
            return false;
        }
        
        state->lowpass_size = radius;
    }
    
    uft_adaptive_reset(state);
    return true;
}

void uft_adaptive_destroy(uft_adaptive_state_t* state)
{
    if (!state) return;
    
    free(state->lowpass_4us);
    free(state->lowpass_6us);
    free(state->lowpass_8us);
    
    memset(state, 0, sizeof(*state));
}

void uft_adaptive_reset(uft_adaptive_state_t* state)
{
    if (!state) return;
    
    const uft_adaptive_config_t* cfg = &state->config;
    
    /* Thresholds auf Basis-Werte zurücksetzen */
    state->current_4us = (float)cfg->timing_4us;
    state->current_6us = (float)cfg->timing_6us;
    state->current_8us = (float)cfg->timing_8us;
    
    /* Lowpass-Filter initialisieren */
    if (state->lowpass_size > 0) {
        for (int32_t i = 0; i < state->lowpass_size; i++) {
            state->lowpass_4us[i] = (float)cfg->timing_4us;
            state->lowpass_6us[i] = (float)cfg->timing_6us;
            state->lowpass_8us[i] = (float)cfg->timing_8us;
        }
        
        state->sum_4us = (float)cfg->timing_4us * (float)state->lowpass_size;
        state->sum_6us = (float)cfg->timing_6us * (float)state->lowpass_size;
        state->sum_8us = (float)cfg->timing_8us * (float)state->lowpass_size;
        state->lowpass_index = 0;
    }
    
    /* Statistiken zurücksetzen */
    state->count_4us = 0;
    state->count_6us = 0;
    state->count_8us = 0;
    state->count_invalid = 0;
    state->resets = 0;
}

/*============================================================================
 * KERN-ALGORITHMUS
 *============================================================================*/

int uft_adaptive_decode_sample(uft_adaptive_state_t* state,
                                int32_t period_value,
                                uint8_t out_bits[4],
                                float* out_entropy)
{
    if (!state || !out_bits) {
        return 0;
    }
    
    const uft_adaptive_config_t* cfg = &state->config;
    
    /* HD-Modus: Werte verdoppeln */
    if (cfg->high_density) {
        period_value <<= 1;
    }
    
    /* Sehr kleine Werte ignorieren (Noise) */
    if (period_value < 4) {
        state->count_invalid++;
        return 0;
    }
    
    float val = (float)period_value;
    
    /* Threshold-Sanity-Check: Falls außer Kontrolle, zurücksetzen */
    if (state->current_4us >= state->current_6us || 
        state->current_6us >= state->current_8us) {
        state->current_4us = (float)cfg->timing_4us;
        state->current_6us = (float)cfg->timing_6us;
        state->current_8us = (float)cfg->timing_8us;
        state->resets++;
    }
    
    /* Dynamische Thresholds berechnen (Mitte zwischen Zellen) */
    float base_thresh_4_6 = (state->current_6us - state->current_4us) / 2.0f;
    float base_thresh_6_8 = (state->current_8us - state->current_6us) / 2.0f;
    
    float threshold_4us = state->current_4us + base_thresh_4_6 + (float)cfg->offset;
    float threshold_6us = state->current_6us + base_thresh_6_8 - (float)cfg->offset;
    
    int num_bits = 0;
    float entropy_val = 0.0f;
    
    /* ============================================ */
    /* Klassifizierung und MFM-Bit-Generierung     */
    /* ============================================ */
    
    if (val <= threshold_4us) {
        /* 4µs Zelle → "10" (2 Bits) */
        out_bits[0] = 1;
        out_bits[1] = 0;
        num_bits = 2;
        
        entropy_val = state->current_4us - val;
        state->count_4us++;
        
        /* Threshold adaptieren */
        if (state->lowpass_size > 0) {
            /* Lowpass-Filter */
            float old_val = state->lowpass_4us[state->lowpass_index % state->lowpass_size];
            state->sum_4us -= old_val;
            state->lowpass_4us[state->lowpass_index % state->lowpass_size] = val;
            state->sum_4us += val;
            state->current_4us = state->sum_4us / (float)state->lowpass_size;
        } else if (cfg->rate_of_change > 0.0f) {
            /* Einfache Adaption */
            state->current_4us += (val - state->current_4us) / cfg->rate_of_change;
        }
        
    } else if (val > threshold_4us && val < threshold_6us) {
        /* 6µs Zelle → "100" (3 Bits) */
        out_bits[0] = 1;
        out_bits[1] = 0;
        out_bits[2] = 0;
        num_bits = 3;
        
        entropy_val = state->current_6us - val;
        state->count_6us++;
        
        /* Threshold adaptieren */
        if (state->lowpass_size > 0) {
            float old_val = state->lowpass_6us[state->lowpass_index % state->lowpass_size];
            state->sum_6us -= old_val;
            state->lowpass_6us[state->lowpass_index % state->lowpass_size] = val;
            state->sum_6us += val;
            state->current_6us = state->sum_6us / (float)state->lowpass_size;
        } else if (cfg->rate_of_change > 0.0f) {
            state->current_6us += (val - state->current_6us) / cfg->rate_of_change;
        }
        
    } else {
        /* 8µs Zelle → "1000" (4 Bits) */
        out_bits[0] = 1;
        out_bits[1] = 0;
        out_bits[2] = 0;
        out_bits[3] = 0;
        num_bits = 4;
        
        entropy_val = state->current_8us - val;
        state->count_8us++;
        
        /* Threshold adaptieren */
        if (state->lowpass_size > 0) {
            float old_val = state->lowpass_8us[state->lowpass_index % state->lowpass_size];
            state->sum_8us -= old_val;
            state->lowpass_8us[state->lowpass_index % state->lowpass_size] = val;
            state->sum_8us += val;
            state->current_8us = state->sum_8us / (float)state->lowpass_size;
        } else if (cfg->rate_of_change > 0.0f) {
            state->current_8us += (val - state->current_8us) / cfg->rate_of_change;
        }
    }
    
    /* Lowpass-Index weiterzählen */
    if (state->lowpass_size > 0) {
        state->lowpass_index++;
    }
    
    /* Entropy-Wert zurückgeben falls gewünscht */
    if (out_entropy) {
        *out_entropy = entropy_val;
    }
    
    return num_bits;
}

/*============================================================================
 * BUFFER-DECODIERUNG
 *============================================================================*/

bool uft_adaptive_decode_buffer(const uint8_t* periods,
                                 size_t period_count,
                                 const uft_adaptive_config_t* config,
                                 uft_adaptive_result_t* result)
{
    if (!periods || period_count == 0 || !config || !result) {
        return false;
    }
    
    /* State initialisieren */
    uft_adaptive_state_t state;
    if (!uft_adaptive_init(&state, config)) {
        return false;
    }
    
    /* Output-Buffer vorbereiten (max 4 Bits pro Sample) */
    size_t max_mfm_bits = period_count * 4;
    uint8_t* mfm_bits = (uint8_t*)malloc(max_mfm_bits);
    float* entropy = config->track_entropy ? (float*)malloc(period_count * sizeof(float)) : NULL;
    
    if (!mfm_bits || (config->track_entropy && !entropy)) {
        free(mfm_bits);
        free(entropy);
        uft_adaptive_destroy(&state);
        return false;
    }
    
    size_t mfm_index = 0;
    uint8_t out_bits[4];
    
    /* Alle Samples decodieren */
    for (size_t i = 0; i < period_count; i++) {
        float ent_val = 0.0f;
        int num_bits = uft_adaptive_decode_sample(&state, periods[i], out_bits, 
                                                   entropy ? &ent_val : NULL);
        
        /* MFM-Bits kopieren */
        for (int j = 0; j < num_bits && mfm_index < max_mfm_bits; j++) {
            mfm_bits[mfm_index++] = out_bits[j];
        }
        
        /* Entropy speichern */
        if (entropy) {
            entropy[i] = ent_val;
        }
    }
    
    /* Ergebnis füllen */
    result->mfm_data = mfm_bits;
    result->mfm_length = mfm_index;
    result->entropy = entropy;
    result->entropy_length = entropy ? period_count : 0;
    result->cells_4us = state.count_4us;
    result->cells_6us = state.count_6us;
    result->cells_8us = state.count_8us;
    result->cells_invalid = state.count_invalid;
    result->threshold_resets = state.resets;
    
    uft_adaptive_destroy(&state);
    return true;
}

void uft_adaptive_get_stats(const uft_adaptive_state_t* state,
                             uint32_t* cells_4us,
                             uint32_t* cells_6us,
                             uint32_t* cells_8us,
                             uint32_t* cells_invalid)
{
    if (!state) return;
    
    if (cells_4us)     *cells_4us = state->count_4us;
    if (cells_6us)     *cells_6us = state->count_6us;
    if (cells_8us)     *cells_8us = state->count_8us;
    if (cells_invalid) *cells_invalid = state->count_invalid;
}

/*============================================================================
 * AMIGA MFM FUNKTIONEN
 *============================================================================*/

bool uft_amiga_mfm_decode_bytes(const uint8_t* mfm,
                                 size_t offset,
                                 size_t length,
                                 uint8_t* output)
{
    if (!mfm || !output || length == 0 || (length % 16) != 0) {
        return false;
    }
    
    size_t num_bytes = length / 16;
    
    /* Jedes Output-Byte besteht aus:
     * - 4 Odd-Bits (bei offset + i*8 + 1, 3, 5, 7)
     * - 4 Even-Bits (bei offset + i*8 + length/2 + 1, 3, 5, 7)
     */
    for (size_t i = 0; i < num_bytes; i++) {
        uint8_t b = 0;
        
        for (int j = 0; j < 8; j += 2) {
            /* Odd-Bit (obere Nibble) */
            b = (uint8_t)(b << 1);
            b |= mfm[offset + i * 8 + j + 1];
            
            /* Even-Bit (untere Nibble) */
            b = (uint8_t)(b << 1);
            b |= mfm[offset + i * 8 + j + (length / 2) + 1];
        }
        
        output[i] = b;
    }
    
    return true;
}

void uft_amiga_checksum(const uint8_t* mfm,
                         size_t offset,
                         size_t length,
                         uint8_t checksum[4])
{
    if (!mfm || !checksum || length == 0) {
        return;
    }
    
    memset(checksum, 0, 4);
    
    /* Checksum über 4-Byte-Blöcke (32 MFM-Bits = 128 Array-Einträge) */
    for (size_t i = 0; i < length / 16; i += 4) {
        size_t base = offset + (i * 8);
        
        for (int k = 0; k < 4; k++) {
            uint8_t odd = 0, even = 0;
            
            for (int j = 0; j < 8; j += 2) {
                size_t idx = base + k * 8 + j + 1;
                
                even = (uint8_t)(even << 2);
                even |= mfm[idx];
                
                odd = (uint8_t)(odd << 2);
                odd |= mfm[idx + (length / 2)];
            }
            
            checksum[k] ^= even;
            checksum[k] ^= odd;
        }
    }
}

size_t uft_amiga_mfm_encode_bytes(const uint8_t* data,
                                   size_t offset,
                                   size_t length,
                                   uint8_t* mfm_output)
{
    if (!data || !mfm_output || length == 0) {
        return 0;
    }
    
    size_t cnt = 0;
    uint8_t previous = 0;
    
    /* Odd-Bits encodieren */
    for (size_t i = offset; i < offset + length; i++) {
        for (int j = 0; j < 8; j += 2) {
            uint8_t bit = (data[i] >> (7 - j)) & 1;
            
            if (bit == 1) {
                mfm_output[cnt++] = 0;
                mfm_output[cnt++] = 1;
            } else if (previous == 0) {
                mfm_output[cnt++] = 1;
                mfm_output[cnt++] = 0;
            } else {
                mfm_output[cnt++] = 0;
                mfm_output[cnt++] = 0;
            }
            
            previous = bit;
        }
    }
    
    /* Even-Bits encodieren */
    for (size_t i = offset; i < offset + length; i++) {
        for (int j = 0; j < 8; j += 2) {
            uint8_t bit = (data[i] >> (6 - j)) & 1;
            
            if (bit == 1) {
                mfm_output[cnt++] = 0;
                mfm_output[cnt++] = 1;
            } else if (previous == 0) {
                mfm_output[cnt++] = 1;
                mfm_output[cnt++] = 0;
            } else {
                mfm_output[cnt++] = 0;
                mfm_output[cnt++] = 0;
            }
            
            previous = bit;
        }
    }
    
    return cnt;
}

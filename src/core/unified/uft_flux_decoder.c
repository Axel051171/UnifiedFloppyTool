/**
 * @file uft_flux_decoder.c
 * @brief Advanced Flux Decoder Implementation
 * 
 * PLL algorithm based on FluxEngine's proven approach.
 */

#include "uft_flux_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef _WIN32
typedef long ssize_t;
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * DEFAULT CONFIGURATIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_pll_config_t UFT_PLL_MFM_DD = {
    .clock_period_ns = 4000.0,   /* 4µs = 250kbps */
    .clock_min_ns = 3600.0,
    .clock_max_ns = 4400.0,
    .pll_phase = 0.65,
    .pll_adjust = 0.04,
    .bit_error_threshold = 0.2,
    .pulse_debounce_ns = 500.0,
    .clock_interval_bias = 0.0,
    .auto_clock = true,
};

const uft_pll_config_t UFT_PLL_MFM_HD = {
    .clock_period_ns = 2000.0,   /* 2µs = 500kbps */
    .clock_min_ns = 1800.0,
    .clock_max_ns = 2200.0,
    .pll_phase = 0.65,
    .pll_adjust = 0.04,
    .bit_error_threshold = 0.2,
    .pulse_debounce_ns = 250.0,
    .clock_interval_bias = 0.0,
    .auto_clock = true,
};

const uft_pll_config_t UFT_PLL_FM = {
    .clock_period_ns = 8000.0,   /* 8µs = 125kbps */
    .clock_min_ns = 7200.0,
    .clock_max_ns = 8800.0,
    .pll_phase = 0.65,
    .pll_adjust = 0.04,
    .bit_error_threshold = 0.25,
    .pulse_debounce_ns = 1000.0,
    .clock_interval_bias = 0.0,
    .auto_clock = true,
};

const uft_pll_config_t UFT_PLL_GCR_C64 = {
    .clock_period_ns = 4000.0,
    .clock_min_ns = 3200.0,
    .clock_max_ns = 4800.0,
    .pll_phase = 0.70,
    .pll_adjust = 0.05,
    .bit_error_threshold = 0.3,
    .pulse_debounce_ns = 500.0,
    .clock_interval_bias = 0.0,
    .auto_clock = true,
};

const uft_pll_config_t UFT_PLL_GCR_APPLE = {
    .clock_period_ns = 4000.0,
    .clock_min_ns = 3200.0,
    .clock_max_ns = 4800.0,
    .pll_phase = 0.70,
    .pll_adjust = 0.05,
    .bit_error_threshold = 0.3,
    .pulse_debounce_ns = 500.0,
    .clock_interval_bias = 0.0,
    .auto_clock = true,
};

const uft_pll_config_t UFT_PLL_GCR_MAC = {
    .clock_period_ns = 2000.0,
    .clock_min_ns = 1600.0,
    .clock_max_ns = 2400.0,
    .pll_phase = 0.70,
    .pll_adjust = 0.05,
    .bit_error_threshold = 0.3,
    .pulse_debounce_ns = 250.0,
    .clock_interval_bias = 0.0,
    .auto_clock = true,
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * INITIALIZATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_flux_decoder_init(uft_flux_decoder_t *dec, const uft_pll_config_t *config)
{
    if (!dec) return;
    
    memset(dec, 0, sizeof(*dec));
    
    if (config) {
        dec->config = *config;
    } else {
        dec->config = UFT_PLL_MFM_DD;
    }
    
    dec->clock = dec->config.clock_period_ns;
    dec->min_clock_seen = 1e9;
    dec->max_clock_seen = 0;
    
    /* Allocate output buffer (generous size) */
    dec->output_size = 1024 * 1024;
    dec->output = (uint8_t*)malloc(dec->output_size);
}

void uft_flux_decoder_reset(uft_flux_decoder_t *dec)
{
    if (!dec) return;
    
    dec->clock = dec->config.clock_period_ns;
    dec->flux_accumulator = 0;
    dec->clocked_zeros = 0;
    dec->good_bits = 0;
    dec->sync_lost = false;
    dec->total_bits = 0;
    dec->bad_bits = 0;
    dec->sync_losses = 0;
    dec->output_pos = 0;
    dec->bit_pos = 0;
    dec->current_byte = 0;
}

void uft_flux_decoder_free(uft_flux_decoder_t *dec)
{
    if (!dec) return;
    
    if (dec->output) {
        free(dec->output);
        dec->output = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * PLL CORE - Based on FluxEngine's Algorithm
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void output_bit(uft_flux_decoder_t *dec, int bit)
{
    dec->current_byte = (dec->current_byte << 1) | (bit & 1);
    dec->bit_pos++;
    
    if (dec->bit_pos == 8) {
        if (dec->output_pos < dec->output_size) {
            dec->output[dec->output_pos++] = dec->current_byte;
        }
        dec->bit_pos = 0;
        dec->current_byte = 0;
    }
    
    dec->total_bits++;
}

int uft_flux_process_interval(uft_flux_decoder_t *dec, double interval_ns)
{
    if (!dec) return 0;
    
    int bits_output = 0;
    
    /* Apply bias */
    interval_ns += dec->config.clock_interval_bias;
    
    /* Debounce: ignore very short pulses */
    if (interval_ns < dec->config.pulse_debounce_ns) {
        return 0;
    }
    
    /* Accumulate flux time */
    dec->flux_accumulator += interval_ns;
    
    /* Clock bits until we've consumed the flux */
    while (dec->flux_accumulator >= dec->clock * dec->config.pll_phase) {
        /* We have a pulse - output 1 */
        output_bit(dec, 1);
        bits_output++;
        
        /* PLL adjustment: move clock toward actual interval */
        double error = dec->flux_accumulator - dec->clock;
        double adjustment = error * dec->config.pll_adjust;
        
        /* Clamp adjustment */
        double new_clock = dec->clock + adjustment;
        if (new_clock >= dec->config.clock_min_ns && 
            new_clock <= dec->config.clock_max_ns) {
            dec->clock = new_clock;
        }
        
        /* Track statistics */
        if (dec->clock < dec->min_clock_seen) dec->min_clock_seen = dec->clock;
        if (dec->clock > dec->max_clock_seen) dec->max_clock_seen = dec->clock;
        
        /* Reset accumulator */
        dec->flux_accumulator = 0;
        dec->clocked_zeros = 0;
        dec->good_bits++;
    }
    
    return bits_output;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * HIGH-LEVEL DECODE
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool uft_flux_decode(uft_flux_decoder_t *dec,
                     const uint8_t *flux, size_t flux_len,
                     uft_decode_result_t *result)
{
    if (!dec || !flux || !result) return false;
    
    memset(result, 0, sizeof(*result));
    uft_flux_decoder_reset(dec);
    
    /* Process flux stream */
    size_t pos = 0;
    while (pos < flux_len) {
        uint8_t b = flux[pos++];
        
        /* Accumulate timing value */
        unsigned ticks = b & 0x3F;
        
        /* Check for events */
        if (b == 0 || (b & (UFT_FLUX_PULSE | UFT_FLUX_INDEX))) {
            /* Convert ticks to nanoseconds */
            double interval_ns = ticks * UFT_FLUX_NS_PER_TICK;
            
            if (b & UFT_FLUX_PULSE) {
                uft_flux_process_interval(dec, interval_ns);
            }
            /* INDEX events can be used for revolution tracking */
        }
    }
    
    /* Flush any remaining bits */
    if (dec->bit_pos > 0) {
        dec->current_byte <<= (8 - dec->bit_pos);
        if (dec->output_pos < dec->output_size) {
            dec->output[dec->output_pos++] = dec->current_byte;
        }
    }
    
    /* Fill result */
    result->data = dec->output;
    result->length = dec->output_pos;
    result->clock_ns = dec->clock;
    result->total_bits = dec->total_bits;
    result->bad_bits = dec->bad_bits;
    result->error_rate = dec->total_bits > 0 ? 
        (double)dec->bad_bits / dec->total_bits : 0.0;
    result->valid = (dec->output_pos > 0 && dec->sync_losses < 10);
    
    return result->valid;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * CLOCK DETECTION
 * ═══════════════════════════════════════════════════════════════════════════════ */

double uft_flux_detect_clock(const uint8_t *flux, size_t flux_len,
                             uint32_t sync_pattern, int sync_bits)
{
    (void)sync_pattern;
    (void)sync_bits;
    
    if (!flux || flux_len < 100) return 0.0;
    
    /* Build histogram of intervals */
    #define HIST_SIZE 256
    uint32_t histogram[HIST_SIZE] = {0};
    
    size_t pos = 0;
    unsigned accumulated = 0;
    
    while (pos < flux_len && pos < 50000) {
        uint8_t b = flux[pos++];
        accumulated += (b & 0x3F);
        
        if (b & UFT_FLUX_PULSE) {
            /* Bin the interval */
            unsigned bin = accumulated / 10;
            if (bin < HIST_SIZE) {
                histogram[bin]++;
            }
            accumulated = 0;
        }
    }
    
    /* Find peak (most common interval = clock) */
    uint32_t max_count = 0;
    unsigned peak_bin = 0;
    
    for (unsigned i = 10; i < HIST_SIZE - 10; i++) {
        /* Smooth with neighbors */
        uint32_t smoothed = histogram[i-1] + histogram[i] * 2 + histogram[i+1];
        if (smoothed > max_count) {
            max_count = smoothed;
            peak_bin = i;
        }
    }
    
    /* Convert bin to nanoseconds */
    double clock_ns = peak_bin * 10.0 * UFT_FLUX_NS_PER_TICK;
    
    return clock_ns;
    #undef HIST_SIZE
}

ssize_t uft_flux_find_sync(uft_flux_decoder_t *dec,
                           const uint8_t *flux, size_t flux_len,
                           uint32_t sync_pattern, int sync_bits)
{
    (void)dec;
    
    if (!flux || flux_len == 0 || sync_bits <= 0 || sync_bits > 32) return -1;
    
    /* Search for sync_pattern in the flux bitstream.
     * We shift bits in from the flux data and compare against the pattern.
     * The mask covers exactly sync_bits bits. */
    uint32_t mask = (sync_bits == 32) ? 0xFFFFFFFF : ((1u << sync_bits) - 1);
    uint32_t window = 0;
    size_t bit_pos = 0;
    
    for (size_t byte_idx = 0; byte_idx < flux_len; byte_idx++) {
        uint8_t byte_val = flux[byte_idx];
        for (int bit = 7; bit >= 0; bit--) {
            window = (window << 1) | ((byte_val >> bit) & 1);
            bit_pos++;
            
            if (bit_pos >= (size_t)sync_bits) {
                if ((window & mask) == (sync_pattern & mask)) {
                    /* Return bit position of sync start */
                    return (ssize_t)(bit_pos - sync_bits);
                }
            }
        }
    }
    
    return -1;  /* Sync pattern not found */
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * ENCODING-SPECIFIC DECODERS
 * ═══════════════════════════════════════════════════════════════════════════════ */

size_t uft_decode_mfm(const uint8_t *encoded, size_t len, uint8_t *decoded)
{
    if (!encoded || !decoded || len == 0) return 0;
    
    size_t out_pos = 0;
    
    /* MFM: every other bit is data, skip clock bits */
    for (size_t i = 0; i < len; i++) {
        uint8_t in = encoded[i];
        uint8_t out = 0;
        
        /* Extract bits 6,4,2,0 (data bits) */
        out |= ((in >> 6) & 1) << 3;
        out |= ((in >> 4) & 1) << 2;
        out |= ((in >> 2) & 1) << 1;
        out |= ((in >> 0) & 1) << 0;
        
        /* Combine two input bytes into one output byte */
        if (i & 1) {
            decoded[out_pos] = (decoded[out_pos] << 4) | out;
            out_pos++;
        } else {
            decoded[out_pos] = out;
        }
    }
    
    return out_pos;
}

size_t uft_decode_fm(const uint8_t *encoded, size_t len, uint8_t *decoded)
{
    /* FM is same as MFM for decoding purposes */
    return uft_decode_mfm(encoded, len, decoded);
}

/* Commodore GCR decode table */
static const uint8_t GCR_DECODE_C64[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

size_t uft_decode_gcr_c64(const uint8_t *encoded, size_t len, uint8_t *decoded)
{
    if (!encoded || !decoded || len < 5) return 0;
    
    size_t out_pos = 0;
    
    /* GCR: 5 bits encode 4 bits, so 5 encoded bytes = 4 decoded bytes */
    for (size_t i = 0; i + 4 < len; i += 5) {
        /* Extract 5 groups of 5 bits from 5 bytes */
        uint8_t g0 = (encoded[i] >> 3) & 0x1F;
        uint8_t g1 = ((encoded[i] << 2) | (encoded[i+1] >> 6)) & 0x1F;
        uint8_t g2 = (encoded[i+1] >> 1) & 0x1F;
        uint8_t g3 = ((encoded[i+1] << 4) | (encoded[i+2] >> 4)) & 0x1F;
        uint8_t g4 = ((encoded[i+2] << 1) | (encoded[i+3] >> 7)) & 0x1F;
        
        /* Decode each group */
        uint8_t d0 = GCR_DECODE_C64[g0];
        uint8_t d1 = GCR_DECODE_C64[g1];
        uint8_t d2 = GCR_DECODE_C64[g2];
        uint8_t d3 = GCR_DECODE_C64[g3];
        
        (void)g4; /* 5th group wraps to next set */
        
        /* Combine nibbles into bytes */
        if (d0 != 0xFF && d1 != 0xFF) {
            decoded[out_pos++] = (d0 << 4) | d1;
        }
        if (d2 != 0xFF && d3 != 0xFF) {
            decoded[out_pos++] = (d2 << 4) | d3;
        }
    }
    
    return out_pos;
}

/* Apple 6-and-2 decode table */
static const uint8_t GCR_DECODE_APPLE[128] = {
    /* 00-1F */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    /* 20-3F */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x01,
                0xFF,0xFF,0x02,0x03,0xFF,0x04,0x05,0x06,
    /* 40-5F */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x07,0x08,
                0xFF,0xFF,0xFF,0x09,0x0A,0x0B,0x0C,0x0D,
    /* 60-7F */ 0xFF,0xFF,0x0E,0x0F,0x10,0x11,0x12,0x13,
                0xFF,0x14,0x15,0x16,0x17,0x18,0x19,0x1A,
    /* 80-9F */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
                0xFF,0xFF,0xFF,0x1B,0xFF,0x1C,0x1D,0x1E,
    /* A0-BF */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x1F,0x20,
                0xFF,0xFF,0x21,0x22,0xFF,0x23,0x24,0x25,
    /* C0-DF */ 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x26,0x27,
                0xFF,0xFF,0xFF,0x28,0x29,0x2A,0x2B,0x2C,
    /* E0-FF */ 0xFF,0xFF,0x2D,0x2E,0x2F,0x30,0x31,0x32,
                0xFF,0x33,0x34,0x35,0x36,0x37,0x38,0x39,
};

size_t uft_decode_gcr_apple(const uint8_t *encoded, size_t len, uint8_t *decoded)
{
    if (!encoded || !decoded) return 0;
    
    size_t out_pos = 0;
    
    /* Apple 6-and-2: each byte encodes 6 bits */
    for (size_t i = 0; i < len; i++) {
        uint8_t val = encoded[i];
        if (val < 128 && GCR_DECODE_APPLE[val] != 0xFF) {
            decoded[out_pos++] = GCR_DECODE_APPLE[val];
        }
    }
    
    return out_pos;
}

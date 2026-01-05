/**
 * @file uft_custom_encoder.c
 * @brief Custom/Protection Encoder Implementation
 * 
 * EXT4-008: Custom encoding schemes and protection
 * 
 * Features:
 * - Custom bit patterns
 * - Weak bit generation
 * - Long track generation
 * - Protection scheme encoding
 * - Flux timing generation
 */

#include "uft/encode/uft_custom_encoder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard timing (microseconds) */
#define MFM_2T_US   2.0
#define MFM_3T_US   3.0
#define MFM_4T_US   4.0

/* Variation for weak bits */
#define WEAK_BIT_VARIANCE   0.5     /* ±0.5 µs */

/*===========================================================================
 * Random Number Generation (for weak bits)
 *===========================================================================*/

static uint32_t rng_state = 0x12345678;

static uint32_t xorshift32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
}

static double random_double(void)
{
    return (double)xorshift32() / (double)UINT32_MAX;
}

void uft_encoder_seed(uint32_t seed)
{
    rng_state = seed ? seed : 0x12345678;
}

/*===========================================================================
 * Flux Buffer Management
 *===========================================================================*/

int uft_flux_buffer_init(uft_flux_buffer_t *buf, size_t capacity)
{
    if (!buf) return -1;
    
    memset(buf, 0, sizeof(*buf));
    
    buf->times = malloc(capacity * sizeof(uint32_t));
    if (!buf->times) return -1;
    
    buf->capacity = capacity;
    buf->count = 0;
    buf->sample_clock = 24000000;  /* Default 24 MHz */
    
    return 0;
}

void uft_flux_buffer_free(uft_flux_buffer_t *buf)
{
    if (!buf) return;
    
    free(buf->times);
    memset(buf, 0, sizeof(*buf));
}

int uft_flux_buffer_add(uft_flux_buffer_t *buf, double time_us)
{
    if (!buf || !buf->times) return -1;
    
    if (buf->count >= buf->capacity) {
        /* Grow buffer */
        size_t new_cap = buf->capacity * 2;
        uint32_t *new_times = realloc(buf->times, new_cap * sizeof(uint32_t));
        if (!new_times) return -1;
        
        buf->times = new_times;
        buf->capacity = new_cap;
    }
    
    /* Convert µs to sample counts */
    uint32_t samples = (uint32_t)(time_us * buf->sample_clock / 1000000.0);
    
    if (buf->count > 0) {
        buf->times[buf->count] = buf->times[buf->count - 1] + samples;
    } else {
        buf->times[buf->count] = samples;
    }
    
    buf->count++;
    return 0;
}

/*===========================================================================
 * MFM Flux Generation
 *===========================================================================*/

int uft_encode_mfm_byte(uft_flux_buffer_t *buf, uint8_t byte, int *prev_bit)
{
    if (!buf || !prev_bit) return -1;
    
    for (int i = 7; i >= 0; i--) {
        int data_bit = (byte >> i) & 1;
        
        /* Clock bit: 1 if both previous and current are 0 */
        int clock_bit = (*prev_bit == 0 && data_bit == 0) ? 1 : 0;
        
        /* Generate flux for clock cell */
        if (clock_bit) {
            uft_flux_buffer_add(buf, MFM_2T_US);
        }
        
        /* Generate flux for data cell */
        if (data_bit) {
            if (clock_bit) {
                uft_flux_buffer_add(buf, MFM_2T_US);
            } else if (*prev_bit) {
                uft_flux_buffer_add(buf, MFM_2T_US);
            } else {
                uft_flux_buffer_add(buf, MFM_3T_US);
            }
        } else {
            if (!clock_bit && !*prev_bit) {
                /* No flux in this cell */
            }
        }
        
        *prev_bit = data_bit;
    }
    
    return 0;
}

int uft_encode_mfm_data(uft_flux_buffer_t *buf, const uint8_t *data, size_t size)
{
    if (!buf || !data) return -1;
    
    int prev_bit = 0;
    
    for (size_t i = 0; i < size; i++) {
        uft_encode_mfm_byte(buf, data[i], &prev_bit);
    }
    
    return 0;
}

/*===========================================================================
 * Sync Pattern Generation
 *===========================================================================*/

int uft_encode_sync_a1(uft_flux_buffer_t *buf, int count)
{
    if (!buf) return -1;
    
    /* A1 sync with missing clock: 0100010010001001 = 4489 */
    /* Normal A1 would be: 0100010010101001 */
    
    for (int c = 0; c < count; c++) {
        /* Generate the specific flux pattern for missing-clock A1 */
        uft_flux_buffer_add(buf, MFM_4T_US);  /* Leading zeros */
        uft_flux_buffer_add(buf, MFM_3T_US);
        uft_flux_buffer_add(buf, MFM_4T_US);  /* Missing clock */
        uft_flux_buffer_add(buf, MFM_3T_US);
        uft_flux_buffer_add(buf, MFM_2T_US);
    }
    
    return 0;
}

int uft_encode_gap(uft_flux_buffer_t *buf, int bytes)
{
    if (!buf) return -1;
    
    /* Gap byte 0x4E in MFM */
    for (int i = 0; i < bytes; i++) {
        int prev_bit = 0;
        uft_encode_mfm_byte(buf, 0x4E, &prev_bit);
    }
    
    return 0;
}

/*===========================================================================
 * Weak Bit Generation
 *===========================================================================*/

int uft_encode_weak_bits(uft_flux_buffer_t *buf, size_t bit_count,
                         double variance)
{
    if (!buf) return -1;
    
    for (size_t i = 0; i < bit_count; i++) {
        /* Random timing within variance */
        double base_time = MFM_2T_US + random_double() * 2.0;  /* 2-4 µs */
        double jitter = (random_double() - 0.5) * 2.0 * variance;
        
        uft_flux_buffer_add(buf, base_time + jitter);
    }
    
    return 0;
}

int uft_encode_weak_sector(uft_flux_buffer_t *buf, const uint8_t *data,
                           size_t size, const uint8_t *weak_mask)
{
    if (!buf || !data) return -1;
    
    int prev_bit = 0;
    
    for (size_t i = 0; i < size; i++) {
        uint8_t byte = data[i];
        uint8_t mask = weak_mask ? weak_mask[i] : 0;
        
        for (int bit = 7; bit >= 0; bit--) {
            int data_bit = (byte >> bit) & 1;
            int is_weak = mask & (1 << bit);
            
            if (is_weak) {
                /* Generate weak bit with random timing */
                double time = MFM_2T_US + (random_double() - 0.5) * WEAK_BIT_VARIANCE;
                uft_flux_buffer_add(buf, time);
            } else {
                /* Normal MFM encoding */
                int clock_bit = (prev_bit == 0 && data_bit == 0) ? 1 : 0;
                
                if (clock_bit || data_bit) {
                    double time;
                    if (clock_bit && data_bit) {
                        time = MFM_2T_US;
                    } else if (!prev_bit) {
                        time = MFM_3T_US;
                    } else {
                        time = MFM_2T_US;
                    }
                    uft_flux_buffer_add(buf, time);
                }
            }
            
            prev_bit = data_bit;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Long Track Generation
 *===========================================================================*/

int uft_encode_long_track(uft_flux_buffer_t *buf, 
                          double target_time_ms,
                          const uint8_t *data, size_t size)
{
    if (!buf || !data) return -1;
    
    /* Standard track is ~200ms at 300 RPM */
    /* Long track might be 210-220ms */
    
    double standard_time = 200.0;  /* ms */
    double extra_time = target_time_ms - standard_time;
    
    if (extra_time <= 0) {
        /* Just encode normally */
        return uft_encode_mfm_data(buf, data, size);
    }
    
    /* Calculate extra bits needed */
    /* At 250 kbps, 1ms = 250 bits */
    int extra_bits = (int)(extra_time * 250);
    
    /* Encode main data */
    uft_encode_mfm_data(buf, data, size);
    
    /* Add extra gap at end */
    int extra_bytes = (extra_bits + 7) / 8;
    uft_encode_gap(buf, extra_bytes);
    
    return 0;
}

/*===========================================================================
 * Protection Scheme Encoders
 *===========================================================================*/

int uft_encode_copylock(uft_flux_buffer_t *buf, const uft_copylock_params_t *params)
{
    if (!buf || !params) return -1;
    
    /* CopyLock uses weak bits at specific positions */
    /* and a signature track with LFSR-generated data */
    
    /* Generate LFSR data */
    uint16_t lfsr = params->seed;
    uint8_t lfsr_data[512];
    
    for (int i = 0; i < 512; i++) {
        lfsr_data[i] = lfsr & 0xFF;
        
        /* LFSR feedback */
        int bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5)) & 1;
        lfsr = (lfsr >> 1) | (bit << 15);
    }
    
    /* Encode with weak bits at signature positions */
    uint8_t weak_mask[512];
    memset(weak_mask, 0, sizeof(weak_mask));
    
    /* Set weak bit positions based on params */
    for (int i = 0; i < params->weak_count && i < 64; i++) {
        int pos = params->weak_positions[i];
        if (pos < 512 * 8) {
            weak_mask[pos / 8] |= (1 << (pos % 8));
        }
    }
    
    /* Encode sector header */
    uft_encode_gap(buf, 12);
    uft_encode_sync_a1(buf, 3);
    
    /* ID field */
    int prev_bit = 0;
    uft_encode_mfm_byte(buf, 0xFE, &prev_bit);  /* IDAM */
    uft_encode_mfm_byte(buf, params->track, &prev_bit);
    uft_encode_mfm_byte(buf, params->side, &prev_bit);
    uft_encode_mfm_byte(buf, params->sector, &prev_bit);
    uft_encode_mfm_byte(buf, 2, &prev_bit);  /* 512 bytes */
    
    /* CRC (simplified) */
    uft_encode_mfm_byte(buf, 0x00, &prev_bit);
    uft_encode_mfm_byte(buf, 0x00, &prev_bit);
    
    /* Gap 2 */
    uft_encode_gap(buf, 22);
    
    /* Data field with weak bits */
    uft_encode_sync_a1(buf, 3);
    prev_bit = 0;
    uft_encode_mfm_byte(buf, 0xFB, &prev_bit);  /* DAM */
    
    uft_encode_weak_sector(buf, lfsr_data, 512, weak_mask);
    
    /* CRC */
    uft_encode_mfm_byte(buf, 0x00, &prev_bit);
    uft_encode_mfm_byte(buf, 0x00, &prev_bit);
    
    /* Gap 3 */
    uft_encode_gap(buf, 80);
    
    return 0;
}

int uft_encode_speedlock(uft_flux_buffer_t *buf, 
                         const uft_speedlock_params_t *params)
{
    if (!buf || !params) return -1;
    
    /* Speedlock uses timing-based protection */
    /* Specific inter-sector gaps are measured by the loader */
    
    /* Generate normal sectors first */
    for (int s = 0; s < params->sector_count; s++) {
        /* Variable gap before sector */
        int gap_size = params->gap_sizes[s % 16];
        uft_encode_gap(buf, gap_size);
        
        /* Sector header */
        uft_encode_sync_a1(buf, 3);
        
        int prev_bit = 0;
        uft_encode_mfm_byte(buf, 0xFE, &prev_bit);
        uft_encode_mfm_byte(buf, params->track, &prev_bit);
        uft_encode_mfm_byte(buf, params->side, &prev_bit);
        uft_encode_mfm_byte(buf, s + 1, &prev_bit);
        uft_encode_mfm_byte(buf, params->size_code, &prev_bit);
        
        /* CRC */
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
        
        /* Gap 2 - this is the timing-critical part */
        for (int g = 0; g < params->timing_gaps[s % 16]; g++) {
            double time = MFM_2T_US;
            if (params->timing_variance > 0) {
                time += (random_double() - 0.5) * params->timing_variance;
            }
            uft_flux_buffer_add(buf, time);
        }
        
        /* Data */
        uft_encode_sync_a1(buf, 3);
        prev_bit = 0;
        uft_encode_mfm_byte(buf, 0xFB, &prev_bit);
        
        /* Sector data */
        for (int b = 0; b < (128 << params->size_code); b++) {
            uft_encode_mfm_byte(buf, 0xE5, &prev_bit);  /* Format fill */
        }
        
        /* CRC */
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
    }
    
    return 0;
}

/*===========================================================================
 * Track Generation
 *===========================================================================*/

int uft_encode_standard_track(uft_flux_buffer_t *buf,
                              int track, int side, int sectors,
                              int sector_size, const uint8_t *data)
{
    if (!buf) return -1;
    
    /* GAP 4a */
    uft_encode_gap(buf, 80);
    
    /* Index sync */
    uft_encode_sync_a1(buf, 3);
    int prev_bit = 0;
    uft_encode_mfm_byte(buf, 0xFC, &prev_bit);  /* IAM */
    
    /* GAP 1 */
    uft_encode_gap(buf, 50);
    
    /* Sectors */
    for (int s = 0; s < sectors; s++) {
        /* Sector ID */
        uft_encode_sync_a1(buf, 3);
        prev_bit = 0;
        uft_encode_mfm_byte(buf, 0xFE, &prev_bit);
        uft_encode_mfm_byte(buf, track, &prev_bit);
        uft_encode_mfm_byte(buf, side, &prev_bit);
        uft_encode_mfm_byte(buf, s + 1, &prev_bit);
        
        int size_code = 0;
        while ((128 << size_code) < sector_size && size_code < 7) {
            size_code++;
        }
        uft_encode_mfm_byte(buf, size_code, &prev_bit);
        
        /* ID CRC (placeholder) */
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
        
        /* GAP 2 */
        uft_encode_gap(buf, 22);
        
        /* Data field */
        uft_encode_sync_a1(buf, 3);
        prev_bit = 0;
        uft_encode_mfm_byte(buf, 0xFB, &prev_bit);
        
        /* Sector data */
        const uint8_t *sector_data = data ? data + s * sector_size : NULL;
        for (int b = 0; b < sector_size; b++) {
            uint8_t byte = sector_data ? sector_data[b] : 0xE5;
            uft_encode_mfm_byte(buf, byte, &prev_bit);
        }
        
        /* Data CRC */
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
        uft_encode_mfm_byte(buf, 0x00, &prev_bit);
        
        /* GAP 3 */
        uft_encode_gap(buf, 80);
    }
    
    /* GAP 4b - fill rest of track */
    uft_encode_gap(buf, 200);
    
    return 0;
}

/*===========================================================================
 * Export
 *===========================================================================*/

int uft_flux_buffer_export(const uft_flux_buffer_t *buf,
                           uint32_t **times, size_t *count)
{
    if (!buf || !times || !count) return -1;
    
    *times = malloc(buf->count * sizeof(uint32_t));
    if (!*times) return -1;
    
    memcpy(*times, buf->times, buf->count * sizeof(uint32_t));
    *count = buf->count;
    
    return 0;
}

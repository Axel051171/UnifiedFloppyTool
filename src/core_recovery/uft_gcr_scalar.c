/**
 * @file uft_gcr_scalar.c
 * @brief GCR Decode - Scalar Baseline Implementation
 * 
 * PERFORMANCE TARGET: 60 MB/s (baseline)
 * 
 * GCR (Group Code Recording) Decoding:
 * - Used in C64, Apple II
 * - 5-to-4 encoding (5 GCR bits → 4 data bits)
 * - Self-clocking (no separate clock track needed)
 * 
 * ALGORITHM:
 * 1. Read 5-bit groups from flux transitions
 * 2. Lookup in GCR decode table
 * 3. Combine 4-bit nibbles into bytes
 */

#include "uft/uft_simd.h"
#include <math.h>
#include <stdint.h>
#include <math.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <math.h>

/* =============================================================================
 * GCR 5-TO-4 DECODE TABLE
 * ============================================================================= */

/**
 * @brief GCR 5-to-4 decode lookup table
 * 
 * Only 16 out of 32 possible 5-bit patterns are valid GCR codes.
 * Invalid patterns map to 0xFF.
 * 
 * Valid GCR patterns guarantee no more than two consecutive zeros
 * (ensures reliable self-clocking).
 */
static const uint8_t GCR_DECODE_TABLE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF,  /* 00000-00011: Invalid */
    0xFF, 0xFF, 0xFF, 0xFF,  /* 00100-00111: Invalid */
    0xFF, 0x08, 0x00, 0x01,  /* 01000-01011: 0x9=8, 0xA=0, 0xB=1 */
    0xFF, 0x0C, 0x04, 0x05,  /* 01100-01111: 0xD=C, 0xE=4, 0xF=5 */
    
    0xFF, 0xFF, 0x02, 0x03,  /* 10000-10011: 0x12=2, 0x13=3 */
    0xFF, 0x0F, 0x06, 0x07,  /* 10100-10111: 0x15=F, 0x16=6, 0x17=7 */
    0xFF, 0x09, 0x0A, 0x0B,  /* 11000-11011: 0x19=9, 0x1A=A, 0x1B=B */
    0xFF, 0x0D, 0x0E, 0xFF   /* 11100-11111: 0x1D=D, 0x1E=E */
};

/**
 * @brief Check if GCR code is valid
 */
static inline bool is_valid_gcr(uint8_t gcr_code)
{
    return (gcr_code < 32) && (GCR_DECODE_TABLE[gcr_code] != 0xFF);
}

/**
 * @brief Decode single 5-bit GCR code to 4-bit nibble
 */
static inline uint8_t gcr_decode_nibble(uint8_t gcr_code)
{
    if (gcr_code >= 32) {
        return 0xFF;  /* Invalid */
    }
    return GCR_DECODE_TABLE[gcr_code];
}

/* =============================================================================
 * GCR TIMING CONSTANTS
 * ============================================================================= */

#define GCR_CELL_NS_C64    3200    /* C64 1541: 3.2µs per bit cell */
#define GCR_CELL_NS_APPLE  2000    /* Apple II: 2.0µs per bit cell */

#define GCR_WINDOW_MIN(cell)  ((cell) * 3 / 4)
#define GCR_WINDOW_MAX(cell)  ((cell) * 5 / 4)

/* =============================================================================
 * GCR FLUX TO BITSTREAM
 * ============================================================================= */

/**
 * @brief Convert GCR flux transitions to bitstream
 * 
 * GCR is self-clocking: each "1" bit is a flux transition
 * "0" bits are absences of transitions
 * 
 * @param flux_transitions Array of transition times (nanoseconds)
 * @param transition_count Number of transitions
 * @param output_bits Output bitstream
 * @return Number of bits decoded
 */
static size_t gcr_flux_to_bits(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits)
{
    if (transition_count < 2) {
        return 0;
    }
    
    /* Auto-detect cell time */
    uint64_t avg_delta = 0;
    size_t samples = (transition_count > 10) ? 10 : transition_count - 1;
    
    for (size_t i = 0; i < samples; i++) {
        avg_delta += flux_transitions[i + 1] - flux_transitions[i];
    }
    avg_delta /= samples;
    
    uint32_t cell_ns = (avg_delta > 2600) ? GCR_CELL_NS_C64 : GCR_CELL_NS_APPLE;
    uint32_t window_min = GCR_WINDOW_MIN(cell_ns);
    uint32_t window_max = GCR_WINDOW_MAX(cell_ns);
    
    /* Decode flux to bits */
    size_t bit_count = 0;
    
    for (size_t i = 0; i < transition_count - 1; i++) {
        uint64_t delta = flux_transitions[i + 1] - flux_transitions[i];
        
        /* Each transition is a "1" bit */
        /* Number of "0" bits = floor(delta / cell_ns) - 1 */
        
        if (delta < window_min) {
            continue;  /* Noise */
        }
        
        int zero_count = (delta / cell_ns) - 1;
        if (zero_count < 0) zero_count = 0;
        
        /* Add zeros */
        for (int j = 0; j < zero_count; j++) {
            size_t byte_idx = bit_count / 8;
            size_t bit_idx = 7 - (bit_count % 8);
            output_bits[byte_idx] &= ~(1 << bit_idx);  /* Clear bit = 0 */
            bit_count++;
        }
        
        /* Add one */
        size_t byte_idx = bit_count / 8;
        size_t bit_idx = 7 - (bit_count % 8);
        output_bits[byte_idx] |= (1 << bit_idx);  /* Set bit = 1 */
        bit_count++;
    }
    
    return bit_count;
}

/* =============================================================================
 * GCR 5-TO-4 DECODER (Scalar)
 * ============================================================================= */

/**
 * @brief Decode GCR bitstream using 5-to-4 decoding
 * 
 * @param flux_transitions Array of flux transition times
 * @param transition_count Number of transitions
 * @param output_bytes Output buffer for decoded bytes
 * @return Number of bytes decoded
 */
size_t uft_gcr_decode_5to4_scalar(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bytes)
{
    if (!flux_transitions || !output_bytes || transition_count < 2) {
        return 0;
    }
    
    /* Allocate temporary buffer for bitstream */
    size_t max_bits = transition_count * 4;  /* Worst case */
    size_t max_bytes = (max_bits + 7) / 8;
    uint8_t *bitstream = malloc(max_bytes);
    if (!bitstream) {
        return 0;
    }
    
    memset(bitstream, 0, max_bytes);
    
    /* Convert flux to bits */
    size_t bit_count = gcr_flux_to_bits(flux_transitions, transition_count, bitstream);
    
    /* Decode 5-to-4 */
    size_t byte_count = 0;
    size_t bit_pos = 0;
    
    while (bit_pos + 10 <= bit_count) {
        /* Read two 5-bit GCR codes (= 1 byte of data) */
        uint8_t gcr1 = 0;
        uint8_t gcr2 = 0;
        
        /* Extract first 5 bits */
        for (int i = 0; i < 5; i++) {
            size_t idx = bit_pos + i;
            size_t byte_idx = idx / 8;
            size_t bit_idx = 7 - (idx % 8);
            uint8_t bit = (bitstream[byte_idx] >> bit_idx) & 1;
            gcr1 = (gcr1 << 1) | bit;
        }
        
        /* Extract next 5 bits */
        for (int i = 0; i < 5; i++) {
            size_t idx = bit_pos + 5 + i;
            size_t byte_idx = idx / 8;
            size_t bit_idx = 7 - (idx % 8);
            uint8_t bit = (bitstream[byte_idx] >> bit_idx) & 1;
            gcr2 = (gcr2 << 1) | bit;
        }
        
        /* Decode nibbles */
        uint8_t nibble1 = gcr_decode_nibble(gcr1);
        uint8_t nibble2 = gcr_decode_nibble(gcr2);
        
        if (nibble1 == 0xFF || nibble2 == 0xFF) {
            /* Invalid GCR code - skip */
            bit_pos++;
            continue;
        }
        
        /* Combine nibbles into byte */
        output_bytes[byte_count++] = (nibble1 << 4) | nibble2;
        bit_pos += 10;
    }
    
    free(bitstream);
    return byte_count;
}

/* =============================================================================
 * C64-SPECIFIC GCR SECTOR DECODE
 * ============================================================================= */

/**
 * @brief Decode C64 GCR sector
 * 
 * C64 sector structure:
 * - Sync bytes (0xFF × 5+)
 * - Header block (8 bytes GCR = 10 bytes encoded)
 * - Data block (256 bytes GCR = 325 bytes encoded)
 * - Checksum
 * 
 * @param gcr_data GCR-encoded sector
 * @param gcr_length Length of GCR data
 * @param output_data Output buffer (256 bytes)
 * @return true if decode successful
 */
bool uft_gcr_decode_c64_sector(
    const uint8_t *gcr_data,
    size_t gcr_length,
    uint8_t *output_data)
{
    if (!gcr_data || !output_data || gcr_length < 325) {
        return false;
    }
    
    /* Find sync mark (5+ consecutive 0xFF bytes) */
    size_t sync_pos = 0;
    int sync_count = 0;
    
    for (size_t i = 0; i < gcr_length - 5; i++) {
        if (gcr_data[i] == 0xFF) {
            sync_count++;
            if (sync_count >= 5) {
                sync_pos = i + 1;
                break;
            }
        } else {
            sync_count = 0;
        }
    }
    
    if (sync_pos == 0) {
        return false;  /* No sync found */
    }
    
    /* Decode data block (256 bytes = 325 GCR bytes) */
    size_t gcr_pos = sync_pos;
    size_t data_pos = 0;
    
    while (data_pos < 256 && gcr_pos + 1 < gcr_length) {
        uint8_t gcr1 = gcr_data[gcr_pos];
        uint8_t gcr2 = gcr_data[gcr_pos + 1];
        
        /* Extract 5-bit codes (packed in bytes - simplified) */
        uint8_t code1 = (gcr1 >> 3) & 0x1F;
        uint8_t code2 = ((gcr1 & 0x07) << 2) | ((gcr2 >> 6) & 0x03);
        
        uint8_t nibble1 = gcr_decode_nibble(code1);
        uint8_t nibble2 = gcr_decode_nibble(code2);
        
        if (nibble1 == 0xFF || nibble2 == 0xFF) {
            return false;  /* Invalid GCR */
        }
        
        output_data[data_pos++] = (nibble1 << 4) | nibble2;
        gcr_pos += 2;
    }
    
    return (data_pos == 256);
}

/* =============================================================================
 * BENCHMARK
 * ============================================================================= */

#ifdef UFT_BENCHMARK

#include <time.h>
#include <math.h>
#include <stdio.h>
#include <math.h>

void uft_gcr_benchmark_scalar(const uint64_t *flux_data, size_t count, int iterations)
{
    uint8_t *output = malloc(count);
    
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        uft_gcr_decode_5to4_scalar(flux_data, count, output);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    double mb_per_sec = (count * sizeof(uint64_t) * iterations) / (elapsed * 1024 * 1024);
    
    printf("GCR Scalar: %.2f MB/s (%d iterations, %.3f sec)\n",
           mb_per_sec, iterations, elapsed);
    
    free(output);
}

#endif /* UFT_BENCHMARK */

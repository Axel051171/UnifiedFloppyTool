/**
 * @file uft_mfm_scalar.c
 * @brief MFM Decode - Scalar Baseline Implementation
 * 
 * PERFORMANCE TARGET: 80 MB/s (baseline)
 * 
 * MFM (Modified Frequency Modulation) Decoding:
 * - Used in IBM PC, Amiga, Atari ST
 * - Converts flux transitions to bit patterns
 * - Clock bits + data bits encoding
 * 
 * ALGORITHM:
 * 1. Measure time between flux transitions
 * 2. Short interval (< 1.5 cell) = "10" or "01"
 * 3. Long interval (> 1.5 cell) = "100" or "001"  
 * 4. Extract data bits (every other bit)
 */

#include "uft/uft_simd.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* =============================================================================
 * MFM TIMING CONSTANTS (Nanoseconds)
 * ============================================================================= */

#define MFM_CELL_NS_DD      2000    /* Double-Density: 2µs cell */
#define MFM_CELL_NS_HD      1000    /* High-Density: 1µs cell */

#define MFM_WINDOW_MIN(cell)  ((cell) * 3 / 4)   /* 75% of cell */
#define MFM_WINDOW_MAX(cell)  ((cell) * 5 / 4)   /* 125% of cell */

/* =============================================================================
 * MFM BITSTREAM DECODER (Scalar)
 * ============================================================================= */

/**
 * @brief Decode MFM flux transitions to bitstream (scalar)
 * 
 * @param flux_transitions Array of flux transition times (nanoseconds)
 * @param transition_count Number of transitions
 * @param output_bits Output buffer for decoded bits (bytes)
 * @return Number of bytes decoded
 * 
 * ALGORITHM:
 * For each pair of flux transitions:
 *   delta_t = transition[i+1] - transition[i]
 *   
 *   if delta_t < 1.5 * cell_time:
 *     // Short pulse = single bit
 *     output "1" bit
 *   else if delta_t < 2.5 * cell_time:
 *     // Medium pulse = two bits
 *     output "01" bits
 *   else:
 *     // Long pulse = error or sync
 *     output "001" bits
 */
size_t uft_mfm_decode_flux_scalar(
    const uint64_t *flux_transitions,
    size_t transition_count,
    uint8_t *output_bits)
{
    if (!flux_transitions || !output_bits || transition_count < 2) {
        return 0;
    }
    
    /* Auto-detect bitrate from first few transitions */
    uint64_t avg_delta = 0;
    size_t samples = (transition_count > 10) ? 10 : transition_count - 1;
    
    for (size_t i = 0; i < samples; i++) {
        avg_delta += flux_transitions[i + 1] - flux_transitions[i];
    }
    avg_delta /= samples;
    
    /* Determine cell time */
    uint32_t cell_ns;
    if (avg_delta < 1500) {
        cell_ns = MFM_CELL_NS_HD;  /* High-density */
    } else {
        cell_ns = MFM_CELL_NS_DD;  /* Double-density */
    }
    
    uint32_t window_min = MFM_WINDOW_MIN(cell_ns);
    uint32_t window_max = MFM_WINDOW_MAX(cell_ns);
    
    /* Decode loop */
    size_t bit_count = 0;
    uint8_t current_byte = 0;
    size_t byte_count = 0;
    
    for (size_t i = 0; i < transition_count - 1; i++) {
        uint64_t delta = flux_transitions[i + 1] - flux_transitions[i];
        
        /* Classify transition */
        if (delta < window_min) {
            /* Too short - skip (noise) */
            continue;
            
        } else if (delta < window_max) {
            /* 1 cell = single "1" bit */
            current_byte = (current_byte << 1) | 1;
            bit_count++;
            
        } else if (delta < window_max * 2) {
            /* 2 cells = "01" pattern */
            current_byte = (current_byte << 2) | 0x01;
            bit_count += 2;
            
        } else if (delta < window_max * 3) {
            /* 3 cells = "001" pattern */
            current_byte = (current_byte << 3) | 0x01;
            bit_count += 3;
            
        } else {
            /* 4+ cells = sync mark or error */
            current_byte = (current_byte << 4) | 0x0001;
            bit_count += 4;
        }
        
        /* Output complete bytes */
        while (bit_count >= 8) {
            output_bits[byte_count++] = (current_byte >> (bit_count - 8)) & 0xFF;
            bit_count -= 8;
            current_byte &= (1 << bit_count) - 1;  /* Keep remaining bits */
        }
    }
    
    /* Output final partial byte */
    if (bit_count > 0) {
        output_bits[byte_count++] = (current_byte << (8 - bit_count)) & 0xFF;
    }
    
    return byte_count;
}

/* =============================================================================
 * MFM DATA EXTRACTION
 * ============================================================================= */

/**
 * @brief Extract data bits from MFM bitstream
 * 
 * MFM encoding interleaves clock and data bits:
 *   C D C D C D C D ...
 *   
 * Data bits are at odd positions (1, 3, 5, 7...)
 * 
 * @param mfm_bits Raw MFM bitstream
 * @param bit_count Number of bits
 * @param output_data Output buffer for data bytes
 * @return Number of data bytes extracted
 */
size_t uft_mfm_extract_data(
    const uint8_t *mfm_bits,
    size_t bit_count,
    uint8_t *output_data)
{
    if (!mfm_bits || !output_data) {
        return 0;
    }
    
    size_t data_byte_count = 0;
    uint8_t current_byte = 0;
    size_t data_bit_count = 0;
    
    for (size_t i = 0; i < bit_count; i++) {
        /* Extract bit from byte array */
        size_t byte_idx  /* FIX: size_t to prevent overflow */ = i / 8;
        uint8_t bit_idx = 7 - (i % 8);
        uint8_t bit = (mfm_bits[byte_idx] >> bit_idx) & 1;
        
        /* Take every other bit (odd positions = data) */
        if (i % 2 == 1) {
            current_byte = (current_byte << 1) | bit;
            data_bit_count++;
            
            if (data_bit_count == 8) {
                output_data[data_byte_count++] = current_byte;
                current_byte = 0;
                data_bit_count = 0;
            }
        }
    }
    
    return data_byte_count;
}

/* =============================================================================
 * SYNC PATTERN DETECTION
 * ============================================================================= */

/**
 * @brief Find MFM sync pattern in bitstream
 * 
 * MFM sync patterns (violate normal encoding rules):
 * - 0x4489 (IBM format) = "0100 0100 1000 1001"
 * - 0xA1A1 (Amiga)      = "1010 0001 1010 0001"
 * 
 * @param mfm_bits MFM bitstream
 * @param bit_count Number of bits
 * @param sync_pattern Sync pattern to find (16 bits)
 * @return Bit offset of sync pattern, or -1 if not found
 */
int uft_mfm_find_sync(
    const uint8_t *mfm_bits,
    size_t bit_count,
    uint16_t sync_pattern)
{
    if (!mfm_bits || bit_count < 16) {
        return -1;
    }
    
    uint16_t window = 0;
    
    for (size_t i = 0; i < bit_count - 15; i++) {
        /* Build 16-bit window */
        size_t byte_idx  /* FIX: size_t to prevent overflow */ = i / 8;
        uint8_t bit_idx = 7 - (i % 8);
        uint8_t bit = (mfm_bits[byte_idx] >> bit_idx) & 1;
        
        window = (window << 1) | bit;
        
        if (i >= 15 && window == sync_pattern) {
            return (int)(i - 15);  /* Return start position */
        }
    }
    
    return -1;  /* Not found */
}

/* =============================================================================
 * BENCHMARK HELPERS
 * ============================================================================= */

#ifdef UFT_BENCHMARK

#include <time.h>
#include <stdio.h>

void uft_mfm_benchmark_scalar(const uint64_t *flux_data, size_t count, int iterations)
{
    uint8_t *output = uft_safe_malloc_array(count, 2);  /* Worst case: 2 bytes per transition */
    if (!output) {
        fprintf(stderr, "ERROR: malloc() failed in benchmark\n");
        return;  /* FIX: Check malloc() return value */
    }
    
    clock_t start = clock();
    
    for (int i = 0; i < iterations; i++) {
        uft_mfm_decode_flux_scalar(flux_data, count, output);
    }
    
    clock_t end = clock();
    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    
    double mb_per_sec = (count * sizeof(uint64_t) * iterations) / (elapsed * 1024 * 1024);
    
    printf("MFM Scalar: %.2f MB/s (%d iterations, %.3f sec)\n",
           mb_per_sec, iterations, elapsed);
    
    free(output);
}

#endif /* UFT_BENCHMARK */

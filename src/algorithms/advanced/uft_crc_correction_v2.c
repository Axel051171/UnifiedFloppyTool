/**
 * @file uft_crc_correction_v2.c
 * @brief GOD MODE: CRC-based Bit Error Correction
 * 
 * Attempts to correct 1-2 bit errors using CRC redundancy.
 * 
 * @author Superman QA - GOD MODE
 * @date 2026
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

// ============================================================================
// CRC Implementations
// ============================================================================

/**
 * @brief CRC-16 CCITT (used in MFM sectors)
 */
uint16_t crc16_ccitt(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    
    return crc;
}

/**
 * @brief CRC-16 for Commodore GCR
 */
uint8_t crc8_gcr(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
    }
    return crc;
}

// ============================================================================
// Correction Result
// ============================================================================

typedef struct {
    bool corrected;
    int bits_flipped;
    size_t flip_positions[4];   // Bit positions that were flipped
    uint8_t* corrected_data;    // Pointer to corrected data (caller owns)
    size_t data_len;
    
    // Audit info
    uint16_t original_crc;
    uint16_t expected_crc;
    uint16_t final_crc;
    int correction_attempts;
} crc_correction_result_t;

// ============================================================================
// 1-Bit Correction
// ============================================================================

/**
 * @brief Attempt 1-bit correction
 *
 * Forensic contract: returns true ONLY if exactly one bit-flip yields the
 * expected CRC. Multiple candidates → ambiguous → returns false (no silent
 * fabrication; caller keeps the original bytes flagged uncorrectable).
 *
 * Time complexity: O(n*8) where n = data length
 */
bool try_1bit_correction(const uint8_t* data, size_t len,
                          uint16_t expected_crc,
                          crc_correction_result_t* result) {
    result->correction_attempts = 0;
    result->original_crc = crc16_ccitt(data, len);
    result->expected_crc = expected_crc;

    // Already correct?
    if (result->original_crc == expected_crc) {
        result->corrected = true;
        result->bits_flipped = 0;
        result->final_crc = expected_crc;
        return true;
    }

    // Allocate work buffer
    uint8_t* work = malloc(len);
    if (!work) return false;
    memcpy(work, data, len);

    size_t hit_byte = 0;
    int    hit_bit  = 0;
    int    hits     = 0;

    for (size_t byte = 0; byte < len; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            result->correction_attempts++;

            work[byte] ^= (1 << bit);
            if (crc16_ccitt(work, len) == expected_crc) {
                hits++;
                if (hits == 1) {
                    hit_byte = byte;
                    hit_bit  = bit;
                }
            }
            work[byte] ^= (1 << bit);

            if (hits > 1) {
                /* Ambiguous: at least two distinct single-bit flips both
                 * satisfy the CRC. Refuse to invent data. */
                free(work);
                return false;
            }
        }
    }

    if (hits == 1) {
        work[hit_byte] ^= (1 << hit_bit);
        result->corrected = true;
        result->bits_flipped = 1;
        result->flip_positions[0] = hit_byte * 8 + (7 - hit_bit);
        result->corrected_data = work;
        result->data_len = len;
        result->final_crc = expected_crc;
        return true;
    }

    free(work);
    return false;
}

// ============================================================================
// 2-Bit Correction
// ============================================================================

/**
 * @brief Attempt 2-bit correction
 *
 * Forensic contract: returns true ONLY if exactly one 2-bit flip pair yields
 * the expected CRC. Multiple candidates → ambiguous → returns false. CRC-16
 * has many double-bit collisions; ambiguity is the rule, not the exception,
 * so refusing ambiguous corrections is essential to avoid fabricated data.
 *
 * Time complexity: O(n²*64) - only practical for small sectors
 * Recommended max size: 256 bytes (standard sector)
 */
bool try_2bit_correction(const uint8_t* data, size_t len,
                          uint16_t expected_crc,
                          crc_correction_result_t* result) {
    // Skip if 1-bit works (returns the unique 1-bit fix or refuses ambiguous)
    if (try_1bit_correction(data, len, expected_crc, result)) {
        return true;
    }

    // Limit to reasonable size
    if (len > 512) return false;

    uint8_t* work = malloc(len);
    if (!work) return false;
    memcpy(work, data, len);

    size_t total_bits = len * 8;
    size_t hit_b1 = 0, hit_b2 = 0;
    int    hits   = 0;

    for (size_t b1 = 0; b1 < total_bits; b1++) {
        work[b1 / 8] ^= (1 << (7 - (b1 % 8)));

        for (size_t b2 = b1 + 1; b2 < total_bits; b2++) {
            result->correction_attempts++;

            work[b2 / 8] ^= (1 << (7 - (b2 % 8)));
            if (crc16_ccitt(work, len) == expected_crc) {
                hits++;
                if (hits == 1) {
                    hit_b1 = b1;
                    hit_b2 = b2;
                }
            }
            work[b2 / 8] ^= (1 << (7 - (b2 % 8)));

            if (hits > 1) {
                /* Ambiguous: at least two distinct 2-bit pairs satisfy the
                 * CRC. Restore outer flip and refuse. */
                work[b1 / 8] ^= (1 << (7 - (b1 % 8)));
                free(work);
                return false;
            }
        }

        work[b1 / 8] ^= (1 << (7 - (b1 % 8)));
    }

    if (hits == 1) {
        work[hit_b1 / 8] ^= (1 << (7 - (hit_b1 % 8)));
        work[hit_b2 / 8] ^= (1 << (7 - (hit_b2 % 8)));
        result->corrected = true;
        result->bits_flipped = 2;
        result->flip_positions[0] = hit_b1;
        result->flip_positions[1] = hit_b2;
        result->corrected_data = work;
        result->data_len = len;
        result->final_crc = expected_crc;
        return true;
    }

    free(work);
    return false;
}

// ============================================================================
// Optimized Correction using Syndrome Tables
// ============================================================================

/**
 * @brief Fast 1-bit correction using precomputed syndrome table
 * 
 * The CRC syndrome uniquely identifies single-bit errors.
 */
typedef struct {
    uint16_t* syndrome_table;   // syndrome → bit position
    size_t max_bits;
    uint16_t poly;
} syndrome_table_t;

bool init_syndrome_table(syndrome_table_t* table, size_t max_bits) {
    // 64K entries for 16-bit syndrome
    table->syndrome_table = calloc(65536, sizeof(uint16_t));
    if (!table->syndrome_table) return false;
    
    table->max_bits = max_bits;
    table->poly = 0x1021;  // CCITT
    
    // Initialize to "not found"
    memset(table->syndrome_table, 0xFF, 65536 * sizeof(uint16_t));
    
    // Compute syndrome for each single-bit error position
    uint8_t* test = calloc((max_bits + 7) / 8, 1);
    if (!test) {
        free(table->syndrome_table);
        return false;
    }
    
    for (size_t bit = 0; bit < max_bits; bit++) {
        // Set single bit
        memset(test, 0, (max_bits + 7) / 8);
        test[bit / 8] = (1 << (7 - (bit % 8)));
        
        uint16_t syndrome = crc16_ccitt(test, (max_bits + 7) / 8);
        
        // Store if unique (first error at this syndrome)
        if (table->syndrome_table[syndrome] == 0xFFFF) {
            table->syndrome_table[syndrome] = (uint16_t)bit;
        }
    }
    
    free(test);
    return true;
}

bool fast_1bit_correction(const syndrome_table_t* table,
                           const uint8_t* data, size_t len,
                           uint16_t expected_crc,
                           crc_correction_result_t* result) {
    uint16_t actual_crc = crc16_ccitt(data, len);
    
    if (actual_crc == expected_crc) {
        result->corrected = true;
        result->bits_flipped = 0;
        return true;
    }
    
    // XOR of actual and expected gives syndrome
    uint16_t syndrome = actual_crc ^ expected_crc;
    
    uint16_t bit_pos = table->syndrome_table[syndrome];
    
    if (bit_pos == 0xFFFF || bit_pos >= len * 8) {
        // No single-bit solution
        return false;
    }
    
    // Allocate and correct
    uint8_t* work = malloc(len);
    if (!work) return false;
    memcpy(work, data, len);
    
    work[bit_pos / 8] ^= (1 << (7 - (bit_pos % 8)));
    
    // Verify
    if (crc16_ccitt(work, len) == expected_crc) {
        result->corrected = true;
        result->bits_flipped = 1;
        result->flip_positions[0] = bit_pos;
        result->corrected_data = work;
        result->data_len = len;
        result->final_crc = expected_crc;
        result->correction_attempts = 1;  // O(1)!
        return true;
    }
    
    free(work);
    return false;
}

void free_syndrome_table(syndrome_table_t* table) {
    free(table->syndrome_table);
    table->syndrome_table = NULL;
}

// ============================================================================
// Sector Correction Interface
// ============================================================================

typedef struct {
    int sectors_processed;
    int sectors_ok_original;
    int sectors_corrected_1bit;
    int sectors_corrected_2bit;
    int sectors_uncorrectable;
    
    double correction_rate;
    size_t total_bits_corrected;
} correction_stats_t;

bool correct_sector(const uint8_t* data, size_t len, uint16_t expected_crc,
                    uint8_t* output, correction_stats_t* stats) {
    crc_correction_result_t result = {0};
    
    stats->sectors_processed++;
    
    // Try 1-bit first (fast)
    if (try_1bit_correction(data, len, expected_crc, &result)) {
        if (result.bits_flipped == 0) {
            stats->sectors_ok_original++;
            memcpy(output, data, len);
        } else {
            stats->sectors_corrected_1bit++;
            stats->total_bits_corrected += result.bits_flipped;
            memcpy(output, result.corrected_data, len);
            free(result.corrected_data);
        }
        return true;
    }
    
    // Try 2-bit (slow, only for small sectors)
    if (len <= 256 && try_2bit_correction(data, len, expected_crc, &result)) {
        stats->sectors_corrected_2bit++;
        stats->total_bits_corrected += result.bits_flipped;
        memcpy(output, result.corrected_data, len);
        free(result.corrected_data);
        return true;
    }
    
    stats->sectors_uncorrectable++;
    memcpy(output, data, len);  // Return original
    return false;
}

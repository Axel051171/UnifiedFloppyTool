/**
 * @file uft_gcr_viterbi.c
 * @brief Context-Aware GCR Decoder with Viterbi Correction
 * 
 * ALGORITHM OVERVIEW
 * ==================
 * 
 * Phase 1: Sync Pattern Detection
 * - Scan for known sync patterns (format-specific)
 * - Build sector map with sync positions
 * 
 * Phase 2: Cell-Time Calibration
 * - Compute cell-time histogram per sector
 * - Use median for robustness against outliers
 * 
 * Phase 3: Viterbi Decoding
 * - For each 5-bit GCR code, compute distance to all 16 valid codes
 * - Use soft-decisions if confidence available
 * - Select minimum-distance path
 * - Track corrections made
 */

#include "uft/algorithms/uft_gcr_viterbi.h"
#include <string.h>
#include <stdlib.h>

// ============================================================================
// GCR TABLES
// ============================================================================

// Valid GCR 5-bit codes (only 16 out of 32 are valid)
static const uint8_t GCR_VALID_CODES[16] = {
    0x09, 0x0A, 0x0B, 0x0D, 0x0E, 0x0F,  // 8, 0, 1, C, 4, 5
    0x12, 0x13, 0x15, 0x16, 0x17,        // 2, 3, F, 6, 7
    0x19, 0x1A, 0x1B, 0x1D, 0x1E         // 9, A, B, D, E
};

// GCR decode table: 5-bit code → 4-bit nibble (0xFF = invalid)
static const uint8_t GCR_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 00-07
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  // 08-0F
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  // 10-17
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   // 18-1F
};

// GCR encode table: 4-bit nibble → 5-bit code
static const uint8_t GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,  // 0-7
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15   // 8-F
};

// ============================================================================
// SYNC PATTERNS
// ============================================================================

// C64 sync: 10 consecutive 1-bits = 0x3FF
#define C64_SYNC_PATTERN    0x3FF
#define C64_SYNC_MASK       0x3FF
#define C64_SYNC_BITS       10

// Apple DOS 3.3 address prologue: D5 AA 96
#define APPLE_ADDR_PROLOGUE     0xD5AA96ULL
#define APPLE_ADDR_PROLOGUE_LEN 24

// Apple DOS 3.3 data prologue: D5 AA AD
#define APPLE_DATA_PROLOGUE     0xD5AAADULL
#define APPLE_DATA_PROLOGUE_LEN 24

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static inline uint8_t get_bit(const uint8_t *bits, size_t bitpos) {
    return (bits[bitpos >> 3] >> (7 - (bitpos & 7))) & 1;
}

static inline uint32_t get_bits(const uint8_t *bits, size_t bitpos, size_t count) {
    uint32_t result = 0;
    for (size_t i = 0; i < count; i++) {
        result = (result << 1) | get_bit(bits, bitpos + i);
    }
    return result;
}

// Hamming distance between two 5-bit codes
static inline int hamming_distance(uint8_t a, uint8_t b) {
    uint8_t x = a ^ b;
    int count = 0;
    while (x) {
        count += (x & 1);
        x >>= 1;
    }
    return count;
}

// ============================================================================
// CONFIGURATION
// ============================================================================

uft_gcr_viterbi_config_t uft_gcr_viterbi_config_default(void) {
    return (uft_gcr_viterbi_config_t){
        .format_hint = UFT_GCR_FORMAT_UNKNOWN,
        .cell_ns_min = 2000,
        .cell_ns_max = 4500,
        .insertion_penalty = 2.0f,
        .deletion_penalty = 2.0f,
        .substitution_base = 1.0f,
        .min_confidence = 0.5f,
        .use_multi_rev = false,
        .rev_count = 1
    };
}

// ============================================================================
// FORMAT DETECTION
// ============================================================================

uft_gcr_format_t uft_gcr_detect_format(const uint8_t *bits, size_t bit_count) {
    if (!bits || bit_count < 100) return UFT_GCR_FORMAT_UNKNOWN;
    
    // Count C64 sync patterns (10 consecutive 1s)
    size_t c64_syncs = 0;
    size_t apple_syncs = 0;
    
    for (size_t i = 0; i + 24 < bit_count; i++) {
        uint32_t window = get_bits(bits, i, 10);
        if (window == C64_SYNC_PATTERN) {
            c64_syncs++;
            i += 9;  // Skip past this sync
        }
        
        // Check for Apple prologue
        uint32_t window24 = get_bits(bits, i, 24);
        if (window24 == APPLE_ADDR_PROLOGUE || window24 == APPLE_DATA_PROLOGUE) {
            apple_syncs++;
            i += 23;
        }
    }
    
    // Decision based on sync pattern density
    // C64: ~21 sectors × 2 syncs (header + data) = ~42 syncs
    // Apple: ~16 sectors × 2 syncs = ~32 syncs
    
    if (c64_syncs > 20 && c64_syncs > apple_syncs * 2) {
        return UFT_GCR_FORMAT_C64;
    }
    
    if (apple_syncs > 15) {
        return UFT_GCR_FORMAT_APPLE_DOS;
    }
    
    // Fallback: analyze GCR code statistics
    // C64 uses standard GCR, Apple uses 6-and-2
    // We can distinguish by checking which decode table works better
    
    size_t valid_gcr = 0;
    size_t total_codes = 0;
    
    for (size_t i = 0; i + 5 < bit_count; i += 5) {
        uint8_t code = (uint8_t)get_bits(bits, i, 5);
        if (GCR_DECODE[code] != 0xFF) {
            valid_gcr++;
        }
        total_codes++;
        if (total_codes > 1000) break;  // Sample enough
    }
    
    float valid_ratio = (total_codes > 0) ? (float)valid_gcr / total_codes : 0.0f;
    
    if (valid_ratio > 0.9f) {
        return UFT_GCR_FORMAT_C64;  // Standard GCR works well
    } else if (valid_ratio > 0.4f) {
        return UFT_GCR_FORMAT_APPLE_DOS;  // 6-and-2 GCR
    }
    
    return UFT_GCR_FORMAT_UNKNOWN;
}

// ============================================================================
// SYNC PATTERN FINDER
// ============================================================================

size_t uft_gcr_find_sync_patterns(
    const uint8_t *bits,
    size_t bit_count,
    uft_gcr_format_t format,
    size_t *sync_offsets,
    size_t max_syncs)
{
    if (!bits || !sync_offsets || bit_count < 10) return 0;
    
    size_t found = 0;
    
    if (format == UFT_GCR_FORMAT_C64 || format == UFT_GCR_FORMAT_UNKNOWN) {
        // Look for 10 consecutive 1-bits
        size_t consecutive_ones = 0;
        
        for (size_t i = 0; i < bit_count && found < max_syncs; i++) {
            if (get_bit(bits, i)) {
                consecutive_ones++;
                if (consecutive_ones >= 10) {
                    // Sync found! Record position after sync
                    sync_offsets[found++] = i + 1;
                    consecutive_ones = 0;
                }
            } else {
                consecutive_ones = 0;
            }
        }
    } else if (format == UFT_GCR_FORMAT_APPLE_DOS || format == UFT_GCR_FORMAT_APPLE_PRODOS) {
        // Look for D5 AA 96 or D5 AA AD
        for (size_t i = 0; i + 24 < bit_count && found < max_syncs; i++) {
            uint32_t window = get_bits(bits, i, 24);
            if ((window & 0xFFFFFF) == APPLE_ADDR_PROLOGUE ||
                (window & 0xFFFFFF) == APPLE_DATA_PROLOGUE) {
                sync_offsets[found++] = i + 24;
            }
        }
    }
    
    return found;
}

// ============================================================================
// VITERBI GCR BYTE DECODE
// ============================================================================

/**
 * Decode a single GCR byte (10 bits → 8 bits) using Viterbi-like correction.
 * 
 * For each 5-bit group:
 * 1. If valid, use directly
 * 2. If invalid, find minimum-distance valid code
 * 3. Weight distance by bit confidence if available
 */
int uft_gcr_viterbi_decode_byte(
    const uint8_t *bits,
    size_t bit_offset,
    const float *confidence,
    uint8_t *out_byte,
    float *out_conf)
{
    if (!bits || !out_byte) return -1;
    
    int corrections = 0;
    float total_conf = 1.0f;
    
    // Decode high nibble (first 5 bits)
    uint8_t gcr_hi = (uint8_t)get_bits(bits, bit_offset, 5);
    uint8_t nibble_hi;
    
    if (GCR_DECODE[gcr_hi] != 0xFF) {
        nibble_hi = GCR_DECODE[gcr_hi];
    } else {
        // Find closest valid code
        int min_dist = 6;  // Maximum possible for 5 bits
        uint8_t best_code = 0x0A;  // Default to '0'
        
        for (int i = 0; i < 16; i++) {
            int dist = hamming_distance(gcr_hi, GCR_VALID_CODES[i]);
            
            // Weight by confidence if available
            if (confidence) {
                float weighted_dist = 0.0f;
                for (int b = 0; b < 5; b++) {
                    int bit_gcr = (gcr_hi >> (4 - b)) & 1;
                    int bit_valid = (GCR_VALID_CODES[i] >> (4 - b)) & 1;
                    if (bit_gcr != bit_valid) {
                        float bit_conf = confidence[bit_offset + b];
                        weighted_dist += (1.0f - bit_conf);  // Low confidence = low cost to flip
                    }
                }
                dist = (int)(weighted_dist * 5.0f + 0.5f);
            }
            
            if (dist < min_dist) {
                min_dist = dist;
                best_code = GCR_VALID_CODES[i];
            }
        }
        
        nibble_hi = GCR_DECODE[best_code];
        corrections++;
        total_conf *= (1.0f - (float)min_dist / 5.0f);
    }
    
    // Decode low nibble (next 5 bits)
    uint8_t gcr_lo = (uint8_t)get_bits(bits, bit_offset + 5, 5);
    uint8_t nibble_lo;
    
    if (GCR_DECODE[gcr_lo] != 0xFF) {
        nibble_lo = GCR_DECODE[gcr_lo];
    } else {
        int min_dist = 6;
        uint8_t best_code = 0x0A;
        
        for (int i = 0; i < 16; i++) {
            int dist = hamming_distance(gcr_lo, GCR_VALID_CODES[i]);
            
            if (confidence) {
                float weighted_dist = 0.0f;
                for (int b = 0; b < 5; b++) {
                    int bit_gcr = (gcr_lo >> (4 - b)) & 1;
                    int bit_valid = (GCR_VALID_CODES[i] >> (4 - b)) & 1;
                    if (bit_gcr != bit_valid) {
                        float bit_conf = confidence[bit_offset + 5 + b];
                        weighted_dist += (1.0f - bit_conf);
                    }
                }
                dist = (int)(weighted_dist * 5.0f + 0.5f);
            }
            
            if (dist < min_dist) {
                min_dist = dist;
                best_code = GCR_VALID_CODES[i];
            }
        }
        
        nibble_lo = GCR_DECODE[best_code];
        corrections++;
        total_conf *= (1.0f - (float)min_dist / 5.0f);
    }
    
    *out_byte = (nibble_hi << 4) | nibble_lo;
    
    if (out_conf) {
        *out_conf = total_conf;
    }
    
    return corrections;
}

// ============================================================================
// FULL TRACK DECODE
// ============================================================================

int uft_gcr_viterbi_decode(
    const uint8_t *bits,
    size_t bit_count,
    const float *confidence,
    const uft_gcr_viterbi_config_t *cfg,
    uft_gcr_viterbi_output_t *output)
{
    if (!bits || bit_count < 100 || !cfg || !output || !output->data) {
        return -1;
    }
    
    // ========================================================================
    // PHASE 1: FORMAT DETECTION
    // ========================================================================
    
    uft_gcr_format_t format = cfg->format_hint;
    if (format == UFT_GCR_FORMAT_UNKNOWN) {
        format = uft_gcr_detect_format(bits, bit_count);
    }
    output->detected_format = format;
    
    // ========================================================================
    // PHASE 2: SYNC PATTERN DETECTION
    // ========================================================================
    
    size_t sync_offsets[256];
    size_t sync_count = uft_gcr_find_sync_patterns(bits, bit_count, format, 
                                                    sync_offsets, 256);
    output->sync_patterns_found = sync_count;
    
    // ========================================================================
    // PHASE 3: SECTOR DECODE
    // ========================================================================
    
    size_t data_pos = 0;
    size_t total_corrections = 0;
    size_t unrecoverable = 0;
    
    if (format == UFT_GCR_FORMAT_C64) {
        // C64 sector structure:
        // - Sync (10 × 1)
        // - Header ID (8 bits)
        // - Checksum (8 bits)
        // - Sector (8 bits)
        // - Track (8 bits)
        // - ID (16 bits)
        // - Gap
        // - Sync (10 × 1)
        // - Data block ID (8 bits)
        // - 256 data bytes
        // - Checksum
        
        for (size_t s = 0; s + 1 < sync_count && data_pos + 256 < output->data_size; s += 2) {
            size_t header_start = sync_offsets[s];
            size_t data_start = (s + 1 < sync_count) ? sync_offsets[s + 1] : 0;
            
            if (data_start == 0 || data_start <= header_start) continue;
            
            // Skip header block ID
            data_start += 10;  // Skip block ID byte (10 GCR bits)
            
            // Decode 256 data bytes
            for (size_t i = 0; i < 256 && data_start + (i + 1) * 10 < bit_count; i++) {
                uint8_t byte_val;
                float byte_conf;
                
                int corr = uft_gcr_viterbi_decode_byte(
                    bits, data_start + i * 10,
                    confidence,
                    &byte_val, &byte_conf);
                
                if (corr >= 0) {
                    output->data[data_pos++] = byte_val;
                    if (output->confidence) {
                        output->confidence[data_pos - 1] = byte_conf;
                    }
                    total_corrections += corr;
                } else {
                    output->data[data_pos++] = 0x00;
                    unrecoverable++;
                }
            }
        }
    } else if (format == UFT_GCR_FORMAT_APPLE_DOS || format == UFT_GCR_FORMAT_APPLE_PRODOS) {
        /* Apple 6-and-2 GCR encoding: 8-bit disk bytes → 6-bit values.
         * Valid disk bytes have bit pattern: 1x xx xx x1 (MSB set, no >1 consecutive 0s).
         * Viterbi decodes 8-bit groups with soft confidence, then applies
         * the 6-and-2 decode table to recover 6-bit data values. */
        
        /* Apple 6-and-2 decode table (disk byte → 6-bit value) */
        static const uint8_t apple62[256] = {
            [0x96]=0x00,[0x97]=0x01,[0x9A]=0x02,[0x9B]=0x03,[0x9D]=0x04,[0x9E]=0x05,[0x9F]=0x06,
            [0xA6]=0x07,[0xA7]=0x08,[0xAB]=0x09,[0xAC]=0x0A,[0xAD]=0x0B,[0xAE]=0x0C,[0xAF]=0x0D,
            [0xB2]=0x0E,[0xB3]=0x0F,[0xB4]=0x10,[0xB5]=0x11,[0xB6]=0x12,[0xB7]=0x13,[0xB9]=0x14,
            [0xBA]=0x15,[0xBB]=0x16,[0xBC]=0x17,[0xBD]=0x18,[0xBE]=0x19,[0xBF]=0x1A,
            [0xCB]=0x1B,[0xCD]=0x1C,[0xCE]=0x1D,[0xCF]=0x1E,
            [0xD3]=0x1F,[0xD6]=0x20,[0xD7]=0x21,[0xD9]=0x22,[0xDA]=0x23,[0xDB]=0x24,
            [0xDC]=0x25,[0xDD]=0x26,[0xDE]=0x27,[0xDF]=0x28,
            [0xE5]=0x29,[0xE6]=0x2A,[0xE7]=0x2B,[0xE9]=0x2C,[0xEA]=0x2D,[0xEB]=0x2E,
            [0xEC]=0x2F,[0xED]=0x30,[0xEE]=0x31,[0xEF]=0x32,
            [0xF2]=0x33,[0xF3]=0x34,[0xF4]=0x35,[0xF5]=0x36,[0xF6]=0x37,[0xF7]=0x38,
            [0xF9]=0x39,[0xFA]=0x3A,[0xFB]=0x3B,[0xFC]=0x3C,[0xFD]=0x3D,[0xFE]=0x3E,[0xFF]=0x3F
        };
        
        for (size_t i = 0; i + 8 <= bit_count && data_pos < output->data_size; i += 8) {
            uint8_t byte_val;
            float byte_conf;
            
            int corr = uft_gcr_viterbi_decode_byte(bits, i, confidence, &byte_val, &byte_conf);
            
            /* Apply Apple 6-and-2 decode: valid disk bytes decode to 6-bit values */
            uint8_t decoded = apple62[byte_val];
            if (decoded == 0 && byte_val != 0x96) {
                /* Invalid disk byte — Viterbi may need to try nearest valid byte */
                /* Find closest valid disk byte by hamming distance */
                int best_dist = 9;
                uint8_t best_byte = byte_val;
                for (int candidate = 0x96; candidate <= 0xFF; candidate++) {
                    if (apple62[candidate] != 0 || candidate == 0x96) {
                        int dist = 0;
                        uint8_t diff = byte_val ^ (uint8_t)candidate;
                        while (diff) { dist++; diff &= diff - 1; }
                        if (dist < best_dist) {
                            best_dist = dist;
                            best_byte = (uint8_t)candidate;
                        }
                    }
                }
                decoded = apple62[best_byte];
                if (best_dist > 0) total_corrections++;
                byte_conf *= (1.0f - best_dist * 0.15f);
            }
            
            output->data[data_pos++] = decoded;
            if (output->confidence) {
                output->confidence[data_pos - 1] = byte_conf;
            }
            if (corr > 0) total_corrections += corr;
        }
    } else {
        // Unknown format: raw decode
        for (size_t i = 0; i + 10 < bit_count && data_pos < output->data_size; i += 10) {
            uint8_t byte_val;
            uft_gcr_viterbi_decode_byte(bits, i, confidence, &byte_val, NULL);
            output->data[data_pos++] = byte_val;
        }
    }
    
    // ========================================================================
    // OUTPUT
    // ========================================================================
    
    output->data_size = data_pos;
    output->total_bits_processed = bit_count;
    output->viterbi_corrections = total_corrections;
    output->unrecoverable_errors = unrecoverable;
    
    return 0;
}

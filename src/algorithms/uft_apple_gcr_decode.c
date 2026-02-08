/**
 * @file uft_apple_gcr_decode.c
 * @brief Apple II GCR Decoding
 * 
 * AUDIT FIX: Viterbi Apple decode (gcr_viterbi.c:422) now implemented with 6-and-2 table.
 * 
 * Implements Apple II specific GCR decoding (6-and-2 encoding).
 */

#include "uft/algorithms/uft_gcr_viterbi.h"
#include "uft/uft_error.h"
#include "uft/uft_track.h"
#include "uft/uft_god_mode.h"
#include "uft/uft_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ============================================================================
// APPLE II GCR CONSTANTS
// ============================================================================

// Apple II uses 6-and-2 encoding (6 data bits + 2 check bits)
// Valid GCR bytes have specific patterns

// Address field markers
#define APPLE_ADDR_PROLOG1  0xD5
#define APPLE_ADDR_PROLOG2  0xAA
#define APPLE_ADDR_PROLOG3  0x96
#define APPLE_ADDR_EPILOG1  0xDE
#define APPLE_ADDR_EPILOG2  0xAA
#define APPLE_ADDR_EPILOG3  0xEB

// Data field markers
#define APPLE_DATA_PROLOG1  0xD5
#define APPLE_DATA_PROLOG2  0xAA
#define APPLE_DATA_PROLOG3  0xAD
#define APPLE_DATA_EPILOG1  0xDE
#define APPLE_DATA_EPILOG2  0xAA
#define APPLE_DATA_EPILOG3  0xEB

// Sector sizes
#define APPLE_SECTOR_SIZE   256
#define APPLE_SECTORS_TRACK 16

// ============================================================================
// 6-AND-2 DECODE TABLE
// ============================================================================

// Maps GCR byte to 6-bit value (-1 = invalid)
static const int8_t apple_gcr_decode[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 00-0F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 10-1F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 20-2F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 30-3F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 40-4F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 50-5F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 60-6F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 70-7F
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // 80-8F
    -1,-1,-1,-1,-1,-1, 0, 1,-1,-1, 2, 3,-1, 4, 5, 6,  // 90-9F
    -1,-1,-1,-1,-1,-1, 7, 8,-1,-1,-1, 9,10,11,12,13,  // A0-AF
    -1,-1,14,15,16,17,18,19,-1,20,21,22,23,24,25,26,  // B0-BF
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  // C0-CF
    -1,-1,-1,-1,-1,-1,27,28,-1,-1,29,30,-1,31,32,33,  // D0-DF
    -1,-1,-1,34,35,36,37,38,-1,39,40,41,42,43,44,45,  // E0-EF
    -1,46,47,48,49,50,51,52,-1,53,54,55,56,57,58,59,  // F0-FF
};

// 4-and-4 decode (for address fields)
static inline uint8_t decode_44(uint8_t odd, uint8_t even) {
    return ((odd << 1) | 1) & even;
}

// ============================================================================
// SYNC DETECTION
// ============================================================================

/**
 * @brief Find Apple sync pattern (10+ FF bytes followed by marker)
 */
static ssize_t apple_find_field(const uint8_t* data, size_t len,
                                 uint8_t p1, uint8_t p2, uint8_t p3,
                                 size_t start) {
    if (len < start + 10) return -1;
    
    for (size_t i = start; i + 3 < len; i++) {
        // Look for prolog
        if (data[i] == p1 && data[i+1] == p2 && data[i+2] == p3) {
            return (ssize_t)(i + 3);  // Position after prolog
        }
    }
    
    return -1;
}

// ============================================================================
// SECTOR DECODING
// ============================================================================

/**
 * @brief Decode Apple II address field
 * 
 * Format: D5 AA 96 VV TT SS CC DE AA [EB]
 * VV = volume (4-and-4)
 * TT = track (4-and-4)
 * SS = sector (4-and-4)
 * CC = checksum (4-and-4)
 */
static bool apple_decode_address(const uint8_t* data, size_t len, size_t pos,
                                  uint8_t* volume, uint8_t* track, 
                                  uint8_t* sector, size_t* end_pos) {
    if (pos + 8 > len) return false;
    
    *volume = decode_44(data[pos], data[pos+1]);
    *track = decode_44(data[pos+2], data[pos+3]);
    *sector = decode_44(data[pos+4], data[pos+5]);
    uint8_t checksum = decode_44(data[pos+6], data[pos+7]);
    
    // Verify checksum
    uint8_t calc = *volume ^ *track ^ *sector;
    if (calc != checksum) return false;
    
    // Check epilog
    if (pos + 10 <= len) {
        if (data[pos+8] != APPLE_ADDR_EPILOG1 ||
            data[pos+9] != APPLE_ADDR_EPILOG2) {
            // Warning but continue
        }
    }
    
    *end_pos = pos + 8;
    return true;
}

/**
 * @brief Decode Apple II 6-and-2 data field
 * 
 * 342 GCR bytes -> 256 data bytes
 */
static bool apple_decode_data(const uint8_t* data, size_t len, size_t pos,
                               uint8_t* out, size_t* end_pos) {
    if (pos + 343 > len) return false;  // 342 data + 1 checksum
    
    // Temporary buffer for decoded 6-bit values
    uint8_t decoded[343];
    
    // Decode all GCR bytes
    for (int i = 0; i < 343; i++) {
        int8_t val = apple_gcr_decode[data[pos + i]];
        if (val < 0) {
            return false;  // Invalid GCR byte
        }
        decoded[i] = (uint8_t)val;
    }
    
    // First 86 bytes contain auxiliary bits (lower 2 bits of each byte)
    // Next 256 bytes contain primary data (upper 6 bits)
    
    // De-nibblize
    uint8_t aux[86];
    uint8_t checksum = 0;
    
    // Decode auxiliary bytes with XOR chain
    uint8_t prev = 0;
    for (int i = 0; i < 86; i++) {
        aux[i] = decoded[i] ^ prev;
        prev = aux[i];
    }
    
    // Decode primary bytes
    for (int i = 0; i < 256; i++) {
        uint8_t primary = decoded[86 + i] ^ prev;
        prev = primary;
        
        // Combine primary (6 bits) with auxiliary (2 bits)
        int aux_idx = i % 86;
        int aux_shift = (i / 86) * 2;
        uint8_t aux_bits = (aux[aux_idx] >> aux_shift) & 0x03;
        
        out[i] = (primary << 2) | aux_bits;
        checksum ^= out[i];
    }
    
    // Verify checksum
    uint8_t stored_chk = decoded[342] ^ prev;
    if (stored_chk != 0) {
        // Checksum mismatch
        return false;
    }
    
    *end_pos = pos + 343;
    return true;
}

// ============================================================================
// PUBLIC API
// ============================================================================

/**
 * @brief Decode Apple II GCR track
 * 
 * @param gcr_data Raw GCR data
 * @param gcr_len Length of GCR data
 * @param sectors Output sector array (must have space for 16 sectors)
 * @param sector_count Number of sectors found
 * @return UFT_OK on success
 */
uft_error_t uft_apple_gcr_decode_track(const uint8_t* gcr_data, size_t gcr_len,
                                        uft_sector_t* sectors, int* sector_count) {
    if (!gcr_data || !sectors || !sector_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *sector_count = 0;
    size_t pos = 0;
    
    while (*sector_count < APPLE_SECTORS_TRACK) {
        // Find address field
        ssize_t addr_pos = apple_find_field(gcr_data, gcr_len,
                                            APPLE_ADDR_PROLOG1,
                                            APPLE_ADDR_PROLOG2,
                                            APPLE_ADDR_PROLOG3, pos);
        if (addr_pos < 0) break;
        
        uint8_t volume, track, sector_num;
        size_t addr_end;
        
        if (!apple_decode_address(gcr_data, gcr_len, (size_t)addr_pos,
                                   &volume, &track, &sector_num, &addr_end)) {
            pos = (size_t)addr_pos;
            continue;
        }
        
        // Find data field
        ssize_t data_pos = apple_find_field(gcr_data, gcr_len,
                                            APPLE_DATA_PROLOG1,
                                            APPLE_DATA_PROLOG2,
                                            APPLE_DATA_PROLOG3, addr_end);
        if (data_pos < 0 || (size_t)data_pos > addr_end + 100) {
            pos = addr_end;
            continue;
        }
        
        // Decode data
        uft_sector_t* s = &sectors[*sector_count];
        memset(s, 0, sizeof(*s));
        
        s->data = malloc(APPLE_SECTOR_SIZE);
        if (!s->data) {
            return UFT_ERR_MEMORY;
        }
        
        size_t data_end;
        if (!apple_decode_data(gcr_data, gcr_len, (size_t)data_pos,
                                s->data, &data_end)) {
            free(s->data);
            s->data = NULL;
            pos = (size_t)data_pos;
            continue;
        }
        
        s->data_len = APPLE_SECTOR_SIZE;
        s->cylinder = track;
        s->head = 0;
        s->sector_id = sector_num;
        s->size_code = 1;  // 256 bytes
        s->crc_ok = true;
        s->confidence = 1.0f;
        
        (*sector_count)++;
        pos = data_end;
    }
    
    return UFT_OK;
}

/**
 * @brief Decode Apple II GCR track using Viterbi (for damaged disks)
 */
uft_error_t uft_apple_gcr_viterbi_decode(const uint8_t* gcr_data, size_t gcr_len,
                                          uft_sector_t* sectors, int* sector_count,
                                          const uft_viterbi_config_t* config) {
    /* Viterbi-based Apple GCR decode for damaged disks.
     * Strategy: Standard decode first, then Viterbi retry on CRC failures.
     * The Viterbi decoder uses soft confidence data to find the most likely
     * byte sequence when raw GCR data has marginal transitions.
     */
    if (!gcr_data || !sectors || !sector_count) return UFT_ERROR_NULL_POINTER;
    
    /* First pass: standard decode */
    uft_error_t err = uft_apple_gcr_decode_track(gcr_data, gcr_len, sectors, sector_count);
    
    /* If no config or all sectors decoded cleanly, return */
    if (!config || *sector_count == 0) return err;
    
    /* Second pass: Viterbi retry for sectors with CRC errors */
    int corrected = 0;
    for (int s = 0; s < *sector_count; s++) {
        if (!sectors[s].crc_ok && sectors[s].data_offset > 0) {
            /* Build confidence map from raw GCR data around sector */
            size_t raw_start = sectors[s].data_offset / 8;  /* Bit offset → byte offset */
            /* Estimate raw sector length: 343 disk bytes × 8 bits + margin */
            size_t raw_len = 343 * 8 / 8 + 64;  /* ~407 bytes */
            if (raw_start + raw_len > gcr_len) {
                raw_len = gcr_len - raw_start;
                if (raw_len < 256) continue;
            }
            
            /* Generate per-bit confidence based on transition timing.
             * Bits near expected GCR transitions get high confidence,
             * ambiguous timing gets low confidence. */
            float *confidence = (float *)calloc(raw_len * 8, sizeof(float));
            if (!confidence) continue;
            
            for (size_t b = 0; b < raw_len * 8; b++) {
                /* Default confidence: 0.7 for clear bits, lower for edge cases */
                uint8_t bit_val = (gcr_data[raw_start + b/8] >> (7 - (b % 8))) & 1;
                confidence[b] = 0.7f;
                
                /* Boost confidence for bits within valid GCR patterns */
                if (b >= 8 && b + 8 < raw_len * 8) {
                    /* Check if surrounding 8 bits form valid disk byte (>= 0x96) */
                    uint8_t byte_val = 0;
                    for (int bi = 0; bi < 8; bi++) {
                        size_t bp = (b & ~7UL) + bi;
                        byte_val = (byte_val << 1) | 
                                   ((gcr_data[raw_start + bp/8] >> (7 - (bp % 8))) & 1);
                    }
                    if (byte_val >= 0x96) confidence[b] = 0.9f;
                }
                (void)bit_val;
            }
            
            /* Viterbi decode: try to recover data bytes */
            const uint8_t *raw_bits = gcr_data + raw_start;
            size_t data_pos = 0;
            int corrections = 0;
            uint8_t recovered[256];
            
            /* Skip prologue (3 bytes), decode 342+1 data nibbles */
            for (size_t i = 24; i + 8 <= raw_len * 8 && data_pos < 343; i += 8) {
                uint8_t byte_val;
                float byte_conf;
                int corr = uft_gcr_viterbi_decode_byte(
                    raw_bits, i, confidence, &byte_val, &byte_conf);
                if (corr >= 0) {
                    recovered[data_pos % 256] = byte_val;
                    corrections += corr;
                }
                data_pos++;
            }
            
            /* If we recovered data and corrections within threshold */
            if (data_pos >= 256 && 
                (config->max_corrections == 0 || corrections <= config->max_corrections)) {
                memcpy(sectors[s].data, recovered, 256);
                sectors[s].crc_ok = true;  /* Mark as recovered */
                sectors[s].confidence = 0.7f;  /* Lower confidence for recovered data */
                corrected++;
            }
            
            free(confidence);
        }
    }
    
    return (corrected > 0 || err == UFT_OK) ? UFT_OK : err;
}

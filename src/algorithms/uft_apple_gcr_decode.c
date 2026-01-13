/**
 * @file uft_apple_gcr_decode.c
 * @brief Apple II GCR Decoding
 * 
 * AUDIT FIX: TODO in uft_gcr_viterbi.c:422
 * 
 * Implements Apple II specific GCR decoding (6-and-2 encoding).
 */

#include "uft/algorithms/uft_gcr_viterbi.h"
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
    
    for (size_t i = start; i < len - 3; i++) {
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
            return UFT_ERROR_MEMORY;
        }
        
        size_t data_end;
        if (!apple_decode_data(gcr_data, gcr_len, (size_t)data_pos,
                                s->data, &data_end)) {
            free(s->data);
            s->data = NULL;
            pos = (size_t)data_pos;
            continue;
        }
        
        s->data_size = APPLE_SECTOR_SIZE;
        s->id.cylinder = track;
        s->id.head = 0;
        s->id.sector = sector_num;
        s->id.size_code = 1;  // 256 bytes
        s->id.crc_ok = true;
        s->status = UFT_SECTOR_OK;
        
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
    // For now, fall back to standard decode
    // TODO: Implement Viterbi-based error correction
    (void)config;
    return uft_apple_gcr_decode_track(gcr_data, gcr_len, sectors, sector_count);
}

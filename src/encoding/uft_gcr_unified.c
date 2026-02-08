/**
 * @file uft_gcr_unified.c
 * @brief Unified GCR encoding/decoding implementation
 * 
 * P2-002: Consolidated GCR module
 */

#include "uft/encoding/uft_gcr_unified.h"
#include <string.h>

/* ============================================================================
 * Commodore 64 GCR Tables
 * ============================================================================ */

/* 4-bit to 5-bit encoding */
const uint8_t gcr_encode_c64[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* 5-bit to 4-bit decoding (0xFF = invalid) */
const uint8_t gcr_decode_c64[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* ============================================================================
 * Apple II 6&2 GCR Tables
 * ============================================================================ */

const uint8_t gcr_encode_apple_62[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

const uint8_t gcr_decode_apple_62[256] = {
    /* 0x00-0x0F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x10-0x1F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x20-0x2F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x30-0x3F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x40-0x4F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x50-0x5F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x60-0x6F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x70-0x7F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x80-0x8F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    /* 0x90-0x9F */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01,
                    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x04, 0x05, 0x06,
    /* 0xA0-0xAF */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x07, 0x08,
                    0xFF, 0xFF, 0xFF, 0x09, 0x0A, 0x0B, 0x0C, 0x0D,
    /* 0xB0-0xBF */ 0xFF, 0xFF, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
                    0xFF, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
    /* 0xC0-0xCF */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0x1B, 0xFF, 0x1C, 0x1D, 0x1E,
    /* 0xD0-0xDF */ 0xFF, 0xFF, 0xFF, 0x1F, 0xFF, 0xFF, 0x20, 0x21,
                    0xFF, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    /* 0xE0-0xEF */ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x29, 0x2A, 0x2B,
                    0xFF, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32,
    /* 0xF0-0xFF */ 0xFF, 0xFF, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38,
                    0xFF, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
};

/* ============================================================================
 * Apple II 5&3 GCR Tables
 * ============================================================================ */

const uint8_t gcr_encode_apple_53[32] = {
    0xAB, 0xAD, 0xAE, 0xAF, 0xB5, 0xB6, 0xB7, 0xBA,
    0xBB, 0xBD, 0xBE, 0xBF, 0xD6, 0xD7, 0xDA, 0xDB,
    0xDD, 0xDE, 0xDF, 0xEA, 0xEB, 0xED, 0xEE, 0xEF,
    0xF5, 0xF6, 0xF7, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF
};

const uint8_t gcr_decode_apple_53[256] = {
    [0xAB] = 0x00, [0xAD] = 0x01, [0xAE] = 0x02, [0xAF] = 0x03,
    [0xB5] = 0x04, [0xB6] = 0x05, [0xB7] = 0x06, [0xBA] = 0x07,
    [0xBB] = 0x08, [0xBD] = 0x09, [0xBE] = 0x0A, [0xBF] = 0x0B,
    [0xD6] = 0x0C, [0xD7] = 0x0D, [0xDA] = 0x0E, [0xDB] = 0x0F,
    [0xDD] = 0x10, [0xDE] = 0x11, [0xDF] = 0x12, [0xEA] = 0x13,
    [0xEB] = 0x14, [0xED] = 0x15, [0xEE] = 0x16, [0xEF] = 0x17,
    [0xF5] = 0x18, [0xF6] = 0x19, [0xF7] = 0x1A, [0xFA] = 0x1B,
    [0xFB] = 0x1C, [0xFD] = 0x1D, [0xFE] = 0x1E, [0xFF] = 0x1F,
    /* All other entries default to 0 (invalid) */
};

/* ============================================================================
 * Victor 9000 GCR Tables
 * ============================================================================ */

const uint8_t gcr_encode_victor[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

const uint8_t gcr_decode_victor[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* ============================================================================
 * Commodore GCR Functions
 * ============================================================================ */

void gcr_encode_c64_4to5(const uint8_t *src, uint8_t *dst) {
    /* 4 bytes (32 bits) -> 5 bytes (40 bits) */
    uint64_t input = ((uint64_t)src[0] << 24) | ((uint64_t)src[1] << 16) |
                     ((uint64_t)src[2] << 8) | src[3];
    
    uint64_t output = 0;
    for (int i = 0; i < 8; i++) {
        int nibble = (input >> (28 - i * 4)) & 0x0F;
        output = (output << 5) | gcr_encode_c64[nibble];
    }
    
    dst[0] = (output >> 32) & 0xFF;
    dst[1] = (output >> 24) & 0xFF;
    dst[2] = (output >> 16) & 0xFF;
    dst[3] = (output >> 8) & 0xFF;
    dst[4] = output & 0xFF;
}

int gcr_decode_c64_5to4(const uint8_t *src, uint8_t *dst) {
    /* 5 bytes (40 bits) -> 4 bytes (32 bits) */
    uint64_t input = ((uint64_t)src[0] << 32) | ((uint64_t)src[1] << 24) |
                     ((uint64_t)src[2] << 16) | ((uint64_t)src[3] << 8) | src[4];
    
    uint32_t output = 0;
    for (int i = 0; i < 8; i++) {
        int gcr = (input >> (35 - i * 5)) & 0x1F;
        int nibble = gcr_decode_c64[gcr];
        
        if (nibble == 0xFF) {
            return -1;  /* Invalid GCR */
        }
        
        output = (output << 4) | nibble;
    }
    
    dst[0] = (output >> 24) & 0xFF;
    dst[1] = (output >> 16) & 0xFF;
    dst[2] = (output >> 8) & 0xFF;
    dst[3] = output & 0xFF;
    
    return 0;
}

size_t gcr_encode_c64_sector(const uint8_t *data, size_t data_len,
                             uint8_t *gcr, size_t gcr_capacity) {
    if (data_len != 256 || gcr_capacity < 325) {
        return 0;
    }
    
    /* Encode 256 bytes -> 325 GCR bytes */
    /* (256 bytes = 64 groups of 4 bytes = 64 * 5 = 320 GCR bytes) */
    /* Plus 5 bytes for data block ID */
    
    size_t out_pos = 0;
    
    /* Encode in groups of 4 */
    for (size_t i = 0; i < 256; i += 4) {
        gcr_encode_c64_4to5(&data[i], &gcr[out_pos]);
        out_pos += 5;
    }
    
    return out_pos;
}

int gcr_decode_c64_sector(const uint8_t *gcr, size_t gcr_len,
                          uint8_t *data, size_t *data_len) {
    if (gcr_len < 320 || !data || !data_len) {
        return -1;
    }
    
    size_t out_pos = 0;
    
    /* Decode in groups of 5 */
    for (size_t i = 0; i < 320; i += 5) {
        if (gcr_decode_c64_5to4(&gcr[i], &data[out_pos]) < 0) {
            return -1;
        }
        out_pos += 4;
    }
    
    *data_len = out_pos;
    return 0;
}

bool gcr_valid_c64(uint8_t nibble) {
    return gcr_decode_c64[nibble & 0x1F] != 0xFF;
}

int gcr_count_illegal_c64(const uint8_t *gcr, size_t len) {
    int count = 0;
    
    /* Check 5-bit groups */
    for (size_t i = 0; i + 4 < len; i += 5) {
        uint64_t val = ((uint64_t)gcr[i] << 32) | ((uint64_t)gcr[i+1] << 24) |
                       ((uint64_t)gcr[i+2] << 16) | ((uint64_t)gcr[i+3] << 8) | gcr[i+4];
        
        for (int j = 0; j < 8; j++) {
            int nibble = (val >> (35 - j * 5)) & 0x1F;
            if (gcr_decode_c64[nibble] == 0xFF) {
                count++;
            }
        }
    }
    
    return count;
}

/* ============================================================================
 * Apple GCR Functions
 * ============================================================================ */

uint8_t gcr_encode_apple_62_byte(uint8_t val) {
    if (val >= 64) return 0xFF;
    return gcr_encode_apple_62[val];
}

uint8_t gcr_decode_apple_62_byte(uint8_t gcr) {
    return gcr_decode_apple_62[gcr];
}

bool gcr_valid_apple_62(uint8_t byte) {
    return gcr_decode_apple_62[byte] != 0xFF;
}

void gcr_encode_apple_62_sector(const uint8_t *data, uint8_t *gcr) {
    /* 
     * 6&2 encoding: 256 bytes -> 342 bytes
     * First 86 bytes: 2-bit remainders
     * Next 256 bytes: 6-bit values
     */
    
    uint8_t buffer[342];
    memset(buffer, 0, sizeof(buffer));
    
    /* Extract 2-bit remainders */
    for (int i = 0; i < 86; i++) {
        uint8_t val = 0;
        
        if (i < 86) {
            val = (data[i] & 0x03);
        }
        if (i + 86 < 256) {
            val |= (data[i + 86] & 0x03) << 2;
        }
        if (i + 172 < 256) {
            val |= (data[i + 172] & 0x03) << 4;
        }
        
        buffer[i] = val;
    }
    
    /* Store 6-bit values */
    for (int i = 0; i < 256; i++) {
        buffer[86 + i] = data[i] >> 2;
    }
    
    /* XOR encoding */
    uint8_t prev = 0;
    for (int i = 341; i >= 0; i--) {
        uint8_t curr = buffer[i];
        buffer[i] = curr ^ prev;
        prev = curr;
    }
    
    /* GCR encode */
    for (int i = 0; i < 342; i++) {
        gcr[i] = gcr_encode_apple_62[buffer[i]];
    }
}

int gcr_decode_apple_62_sector(const uint8_t *gcr, uint8_t *data) {
    uint8_t buffer[342];
    
    /* GCR decode */
    for (int i = 0; i < 342; i++) {
        uint8_t decoded = gcr_decode_apple_62[gcr[i]];
        if (decoded == 0xFF) {
            return -1;
        }
        buffer[i] = decoded;
    }
    
    /* XOR decode */
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) {
        buffer[i] ^= prev;
        prev = buffer[i];
    }
    
    /* Verify checksum */
    if (buffer[341] != 0) {
        return -1;
    }
    
    /* Reconstruct 256 bytes */
    for (int i = 0; i < 256; i++) {
        uint8_t hi = buffer[86 + i];
        uint8_t lo = 0;
        
        int idx = i % 86;
        int shift = (i / 86) * 2;
        
        lo = (buffer[idx] >> shift) & 0x03;
        
        data[i] = (hi << 2) | lo;
    }
    
    return 0;
}

uint8_t gcr_encode_apple_53_byte(uint8_t val) {
    if (val >= 32) return 0xFF;
    return gcr_encode_apple_53[val];
}

uint8_t gcr_decode_apple_53_byte(uint8_t gcr) {
    return gcr_decode_apple_53[gcr];
}

/* ============================================================================
 * Generic GCR Operations
 * ============================================================================ */

void gcr_init(gcr_context_t *ctx, gcr_type_t type) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->type = type;
    
    switch (type) {
        case GCR_TYPE_C64:
            ctx->encode_table = gcr_encode_c64;
            ctx->decode_table = gcr_decode_c64;
            ctx->encode_bits = 4;
            ctx->decode_bits = 5;
            break;
            
        case GCR_TYPE_APPLE_62:
            ctx->encode_table = gcr_encode_apple_62;
            ctx->decode_table = gcr_decode_apple_62;
            ctx->encode_bits = 6;
            ctx->decode_bits = 8;
            break;
            
        case GCR_TYPE_APPLE_53:
            ctx->encode_table = gcr_encode_apple_53;
            ctx->decode_table = gcr_decode_apple_53;
            ctx->encode_bits = 5;
            ctx->decode_bits = 8;
            break;
            
        case GCR_TYPE_VICTOR:
            ctx->encode_table = gcr_encode_victor;
            ctx->decode_table = gcr_decode_victor;
            ctx->encode_bits = 4;
            ctx->decode_bits = 5;
            break;
    }
}

/* ============================================================================
 * Sync Pattern Detection
 * ============================================================================ */

int gcr_find_sync(const uint8_t *bits, size_t bit_count,
                  uint32_t sync_pattern, int sync_len,
                  size_t start_bit) {
    if (!bits || sync_len > 32) return -1;
    
    uint32_t mask = (1 << sync_len) - 1;
    uint32_t window = 0;
    
    for (size_t i = start_bit; i < bit_count; i++) {
        size_t byte_idx = i / 8;
        int bit_idx = 7 - (i % 8);
        int bit = (bits[byte_idx] >> bit_idx) & 1;
        
        window = ((window << 1) | bit) & mask;
        
        if (window == sync_pattern && i >= (size_t)(sync_len - 1)) {
            return (int)(i - sync_len + 1);
        }
    }
    
    return -1;
}

int gcr_count_syncs(const uint8_t *bits, size_t bit_count, gcr_type_t type) {
    int count = 0;
    uint32_t sync_pattern;
    int sync_len;
    
    switch (type) {
        case GCR_TYPE_C64:
            sync_pattern = 0x3FF;  /* 10 ones */
            sync_len = 10;
            break;
        case GCR_TYPE_APPLE_62:
        case GCR_TYPE_APPLE_53:
            sync_pattern = 0xFF;   /* 8 ones */
            sync_len = 8;
            break;
        default:
            return 0;
    }
    
    size_t pos = 0;
    while (pos < bit_count) {
        int found = gcr_find_sync(bits, bit_count, sync_pattern, sync_len, pos);
        if (found < 0) break;
        count++;
        pos = found + sync_len;
    }
    
    return count;
}

/* ============================================================================
 * Track Building
 * ============================================================================ */

size_t gcr_build_c64_sector(uint8_t track, uint8_t sector,
                            uint8_t id1, uint8_t id2,
                            const uint8_t *data,
                            uint8_t *output, size_t output_capacity) {
    if (!data || !output || output_capacity < 400) {
        return 0;
    }
    
    size_t pos = 0;
    
    /* Sync bytes */
    for (int i = 0; i < 5; i++) {
        output[pos++] = 0xFF;
    }
    
    /* Header block */
    uint8_t header[8];
    header[0] = C64_HEADER_ID;
    header[1] = sector ^ track ^ id2 ^ id1;  /* Checksum */
    header[2] = sector;
    header[3] = track;
    header[4] = id2;
    header[5] = id1;
    header[6] = 0x0F;
    header[7] = 0x0F;
    
    /* Encode header (8 bytes -> 10 GCR bytes) */
    gcr_encode_c64_4to5(&header[0], &output[pos]);
    pos += 5;
    gcr_encode_c64_4to5(&header[4], &output[pos]);
    pos += 5;
    
    /* Header gap */
    for (int i = 0; i < 9; i++) {
        output[pos++] = 0x55;
    }
    
    /* Sync bytes */
    for (int i = 0; i < 5; i++) {
        output[pos++] = 0xFF;
    }
    
    /* Data block ID */
    uint8_t data_hdr[4] = {C64_DATA_ID, 0, 0, 0};
    gcr_encode_c64_4to5(data_hdr, &output[pos]);
    pos += 5;
    
    /* Calculate data checksum */
    uint8_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        checksum ^= data[i];
    }
    
    /* Encode sector data */
    for (int i = 0; i < 256; i += 4) {
        uint8_t block[4] = {data[i], data[i+1], data[i+2], data[i+3]};
        gcr_encode_c64_4to5(block, &output[pos]);
        pos += 5;
    }
    
    /* Checksum */
    uint8_t chk_block[4] = {checksum, 0, 0, 0};
    gcr_encode_c64_4to5(chk_block, &output[pos]);
    pos += 5;
    
    /* Tail gap */
    for (int i = 0; i < 9; i++) {
        output[pos++] = 0x55;
    }
    
    return pos;
}

size_t gcr_track_size(gcr_type_t type, int sectors) {
    switch (type) {
        case GCR_TYPE_C64:
            /* ~360 bytes per sector + inter-sector gaps */
            return sectors * 400;
            
        case GCR_TYPE_APPLE_62:
            /* ~400 bytes per sector */
            return sectors * 410;
            
        default:
            return 0;
    }
}

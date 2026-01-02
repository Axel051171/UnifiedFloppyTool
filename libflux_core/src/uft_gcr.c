/**
 * @file uft_gcr.c
 * @brief GCR Decoder Implementation
 */

#include "uft_gcr.h"
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * GCR ENCODING/DECODING TABLES
 * ======================================================================== */

/**
 * @brief 4-to-5 GCR encoding table
 * 
 * Commodore GCR uses 16 specific 5-bit codes that ensure
 * no more than two consecutive zeros in the bitstream.
 */
const uint8_t uft_gcr_encode_table[16] = {
    0x0A, 0x0B, 0x12, 0x13,  /* 0x0-0x3 */
    0x0E, 0x0F, 0x16, 0x17,  /* 0x4-0x7 */
    0x09, 0x19, 0x1A, 0x1B,  /* 0x8-0xB */
    0x0D, 0x1D, 0x1E, 0x15   /* 0xC-0xF */
};

/**
 * @brief 5-to-4 GCR decoding table
 * 
 * Index by 5-bit GCR code, returns 4-bit nibble.
 * 0xFF = invalid/illegal GCR code.
 */
const uint8_t uft_gcr_decode_table[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x00-0x07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 0x08-0x0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 0x10-0x17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 0x18-0x1F */
};

/* ========================================================================
 * C64 1541 SPEED ZONES
 * ======================================================================== */

const uft_gcr_speed_zone_t uft_c64_speed_zones[4] = {
    {
        .zone_id = 3,
        .first_track = 1,
        .last_track = 17,
        .sectors_per_track = 21,
        .bitrate_hz = 250000,     /* 2.50 MHz */
        .cell_ns = 4000           /* 4.0 µs */
    },
    {
        .zone_id = 2,
        .first_track = 18,
        .last_track = 24,
        .sectors_per_track = 19,
        .bitrate_hz = 275000,     /* 2.75 MHz */
        .cell_ns = 3636           /* 3.636 µs */
    },
    {
        .zone_id = 1,
        .first_track = 25,
        .last_track = 30,
        .sectors_per_track = 18,
        .bitrate_hz = 300000,     /* 3.00 MHz */
        .cell_ns = 3333           /* 3.333 µs */
    },
    {
        .zone_id = 0,
        .first_track = 31,
        .last_track = 35,
        .sectors_per_track = 17,
        .bitrate_hz = 325000,     /* 3.25 MHz */
        .cell_ns = 3077           /* 3.077 µs */
    }
};

/* ========================================================================
 * CONTEXT MANAGEMENT
 * ======================================================================== */

uft_rc_t uft_gcr_create(uft_gcr_ctx_t** ctx) {
    UFT_CHECK_NULL(ctx);
    
    *ctx = calloc(1, sizeof(uft_gcr_ctx_t));
    if (!*ctx) {
        return UFT_ERR_MEMORY;
    }
    
    return UFT_SUCCESS;
}

void uft_gcr_destroy(uft_gcr_ctx_t** ctx) {
    if (ctx && *ctx) {
        free(*ctx);
        *ctx = NULL;
    }
}

/* ========================================================================
 * SPEED ZONE LOOKUP
 * ======================================================================== */

const uft_gcr_speed_zone_t* uft_gcr_get_speed_zone(uint8_t track) {
    if (track < 1 || track > 35) {
        return NULL;
    }
    
    for (int i = 0; i < 4; i++) {
        if (track >= uft_c64_speed_zones[i].first_track &&
            track <= uft_c64_speed_zones[i].last_track) {
            return &uft_c64_speed_zones[i];
        }
    }
    
    return NULL;
}

/* ========================================================================
 * NIBBLE ENCODE/DECODE
 * ======================================================================== */

uft_rc_t uft_gcr_encode_byte(uint8_t byte, uint8_t gcr_out[2]) {
    UFT_CHECK_NULL(gcr_out);
    
    uint8_t hi = (byte >> 4) & 0x0F;
    uint8_t lo = byte & 0x0F;
    
    gcr_out[0] = uft_gcr_encode_table[hi];
    gcr_out[1] = uft_gcr_encode_table[lo];
    
    return UFT_SUCCESS;
}

uft_rc_t uft_gcr_decode_byte(const uint8_t gcr_in[2], uint8_t* byte_out) {
    UFT_CHECK_NULLS(gcr_in, byte_out);
    
    uint8_t hi = uft_gcr_decode_table[gcr_in[0] & 0x1F];
    uint8_t lo = uft_gcr_decode_table[gcr_in[1] & 0x1F];
    
    if (hi == 0xFF || lo == 0xFF) {
        return UFT_ERR_CORRUPTED;
    }
    
    *byte_out = (hi << 4) | lo;
    return UFT_SUCCESS;
}

/* ========================================================================
 * BIT MANIPULATION HELPERS
 * ======================================================================== */

/**
 * @brief Extract bit from bitstream
 */
static inline bool get_bit(const uint8_t* bitstream, uint32_t bit_pos) {
    uint32_t byte_idx = bit_pos / 8;
    uint8_t bit_idx = 7 - (bit_pos % 8);
    return (bitstream[byte_idx] >> bit_idx) & 1;
}

/**
 * @brief Extract N bits as uint32
 */
static uint32_t get_bits(const uint8_t* bitstream, uint32_t start_bit, uint8_t count) {
    uint32_t result = 0;
    for (uint8_t i = 0; i < count; i++) {
        result = (result << 1) | (get_bit(bitstream, start_bit + i) ? 1 : 0);
    }
    return result;
}

/* ========================================================================
 * SYNC DETECTION
 * ======================================================================== */

uft_rc_t uft_gcr_find_sync(
    const uint8_t* bitstream,
    uint32_t bit_count,
    uint32_t start_bit,
    uint32_t* sync_pos
) {
    UFT_CHECK_NULLS(bitstream, sync_pos);
    
    if (start_bit >= bit_count) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Look for 10+ consecutive '1' bits (0x3FF) */
    uint32_t ones_count = 0;
    
    for (uint32_t i = start_bit; i < bit_count; i++) {
        if (get_bit(bitstream, i)) {
            ones_count++;
            if (ones_count >= 10) {
                /* Found sync! Position after sync mark */
                *sync_pos = i + 1;
                return UFT_SUCCESS;
            }
        } else {
            ones_count = 0;
        }
    }
    
    return UFT_ERR_NOT_FOUND;
}

/* ========================================================================
 * HEADER DECODE
 * ======================================================================== */

uft_rc_t uft_gcr_decode_header(
    const uint8_t* bitstream,
    uft_gcr_header_t* header
) {
    UFT_CHECK_NULLS(bitstream, header);
    
    /* Header format (after sync):
     * - 1 byte: 0x08 (header marker)
     * - 5 GCR bytes (10 nibbles) → 5 bytes:
     *   [checksum, sector, track, id2, id1]
     */
    
    uint32_t bit_pos = 0;
    
    /* Check for 0x08 marker */
    uint8_t marker = (uint8_t)get_bits(bitstream, bit_pos, 8);
    bit_pos += 8;
    
    if (marker != 0x08) {
        return UFT_ERR_CORRUPTED;
    }
    
    /* Decode 5 GCR bytes → 5 data bytes */
    uint8_t data[5];
    for (int i = 0; i < 5; i++) {
        uint8_t gcr[2];
        gcr[0] = (uint8_t)get_bits(bitstream, bit_pos, 5);
        bit_pos += 5;
        gcr[1] = (uint8_t)get_bits(bitstream, bit_pos, 5);
        bit_pos += 5;
        
        uft_rc_t rc = uft_gcr_decode_byte(gcr, &data[i]);
        if (uft_failed(rc)) {
            return rc;
        }
    }
    
    /* Parse header fields */
    header->checksum = data[0];
    header->sector = data[1];
    header->track = data[2];
    header->id2 = data[3];
    header->id1 = data[4];
    
    /* Verify checksum: XOR of sector, track, id2, id1 */
    uint8_t computed = data[1] ^ data[2] ^ data[3] ^ data[4];
    if (computed != header->checksum) {
        return UFT_ERR_CRC;
    }
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * DATA BLOCK DECODE
 * ======================================================================== */

uft_rc_t uft_gcr_decode_data(
    const uint8_t* bitstream,
    uft_gcr_data_block_t* data
) {
    UFT_CHECK_NULLS(bitstream, data);
    
    /* Data format (after sync):
     * - 1 byte: 0x07 (data marker)
     * - 325 GCR bytes → 260 bytes (256 data + checksum)
     */
    
    uint32_t bit_pos = 0;
    
    /* Check for 0x07 marker */
    uint8_t marker = (uint8_t)get_bits(bitstream, bit_pos, 8);
    bit_pos += 8;
    
    if (marker != 0x07) {
        return UFT_ERR_CORRUPTED;
    }
    
    /* Decode 260 bytes from GCR */
    uint8_t decoded[260];
    for (int i = 0; i < 260; i++) {
        uint8_t gcr[2];
        gcr[0] = (uint8_t)get_bits(bitstream, bit_pos, 5);
        bit_pos += 5;
        gcr[1] = (uint8_t)get_bits(bitstream, bit_pos, 5);
        bit_pos += 5;
        
        uft_rc_t rc = uft_gcr_decode_byte(gcr, &decoded[i]);
        if (uft_failed(rc)) {
            return rc;
        }
    }
    
    /* First 256 bytes = data, last byte = checksum */
    memcpy(data->data, decoded, 256);
    data->checksum = decoded[256];
    
    /* Verify checksum: XOR of all data bytes */
    uint8_t computed = 0;
    for (int i = 0; i < 256; i++) {
        computed ^= data->data[i];
    }
    
    if (computed != data->checksum) {
        return UFT_ERR_CRC;
    }
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * TRACK DECODE
 * ======================================================================== */

uft_rc_t uft_gcr_decode_track(
    uft_gcr_ctx_t* ctx,
    uint8_t track_num,
    const uint8_t* bitstream,
    uint32_t bit_count,
    uft_gcr_track_t* track
) {
    UFT_CHECK_NULLS(ctx, bitstream, track);
    
    if (track_num < 1 || track_num > 35) {
        return UFT_ERR_INVALID_ARG;
    }
    
    /* Initialize track */
    memset(track, 0, sizeof(*track));
    track->track_num = track_num;
    track->bitstream_length = bit_count;
    
    /* Get expected sector count for this track */
    const uft_gcr_speed_zone_t* zone = uft_gcr_get_speed_zone(track_num);
    if (!zone) {
        return UFT_ERR_INVALID_ARG;
    }
    
    ctx->current_track = track_num;
    ctx->speed_zone = zone;
    
    /* Find and decode all sectors */
    uint32_t search_pos = 0;
    uint8_t sectors_decoded = 0;
    
    while (sectors_decoded < zone->sectors_per_track && search_pos < bit_count) {
        uint32_t sync_pos;
        uft_rc_t rc = uft_gcr_find_sync(bitstream, bit_count, search_pos, &sync_pos);
        
        if (uft_failed(rc)) {
            break;  /* No more sync marks */
        }
        
        track->sync_marks_found++;
        ctx->sync_marks_found++;
        
        /* Check if this is header (0x08) or data (0x07) */
        if (sync_pos + 8 >= bit_count) {
            break;
        }
        
        uint8_t marker = (uint8_t)get_bits(bitstream, sync_pos, 8);
        
        if (marker == 0x08) {
            /* Header block */
            uft_gcr_header_t header;
            rc = uft_gcr_decode_header(bitstream + (sync_pos / 8), &header);
            
            if (uft_success(rc) && header.sector < 21) {
                track->sectors[header.sector].header = header;
                track->sectors[header.sector].header_valid = true;
            } else {
                ctx->checksum_errors++;
            }
            
            search_pos = sync_pos + 80;  /* Skip header */
            
        } else if (marker == 0x07) {
            /* Data block - find corresponding header */
            uft_gcr_data_block_t data;
            rc = uft_gcr_decode_data(bitstream + (sync_pos / 8), &data);
            
            /* Associate with most recent header with matching track */
            for (int i = 0; i < 21; i++) {
                if (track->sectors[i].header_valid && 
                    !track->sectors[i].data_valid &&
                    track->sectors[i].header.track == track_num) {
                    
                    track->sectors[i].data = data;
                    track->sectors[i].data_valid = uft_success(rc);
                    
                    if (uft_success(rc)) {
                        sectors_decoded++;
                        ctx->sectors_decoded++;
                    } else {
                        ctx->checksum_errors++;
                    }
                    break;
                }
            }
            
            search_pos = sync_pos + 2600;  /* Skip data block */
        } else {
            search_pos = sync_pos + 8;
        }
    }
    
    track->sectors_found = sectors_decoded;
    
    return (sectors_decoded > 0) ? UFT_SUCCESS : UFT_ERR_CORRUPTED;
}

/* ========================================================================
 * FLUX TO BITSTREAM
 * ======================================================================== */

uft_rc_t uft_gcr_flux_to_bitstream(
    uft_gcr_ctx_t* ctx,
    const uint32_t* flux_ns,
    uint32_t flux_count,
    uint8_t* bitstream,
    size_t bitstream_size,
    uint32_t* bits_decoded
) {
    UFT_CHECK_NULLS(ctx, flux_ns, bitstream, bits_decoded);
    
    if (!ctx->speed_zone) {
        return UFT_ERR_INVALID_ARG;
    }
    
    uint32_t nominal_cell = ctx->speed_zone->cell_ns;
    uint32_t tolerance = nominal_cell / 4;  /* ±25% tolerance */
    
    uint32_t bit_pos = 0;
    uint32_t byte_pos = 0;
    uint8_t current_byte = 0;
    uint8_t bit_in_byte = 0;
    
    /* Convert flux transitions to bits */
    for (uint32_t i = 0; i < flux_count && byte_pos < bitstream_size; i++) {
        uint32_t interval = flux_ns[i];
        
        /* Determine number of bit cells in this interval */
        uint32_t cells = (interval + (nominal_cell / 2)) / nominal_cell;
        
        if (cells == 0) cells = 1;
        if (cells > 4) cells = 4;  /* Clamp to reasonable range */
        
        /* Write bits: (cells-1) zeros + 1 one */
        for (uint32_t j = 0; j < cells - 1; j++) {
            current_byte = (current_byte << 1) | 0;
            bit_in_byte++;
            
            if (bit_in_byte == 8) {
                bitstream[byte_pos++] = current_byte;
                current_byte = 0;
                bit_in_byte = 0;
                bit_pos += 8;
            }
        }
        
        /* Write the '1' bit */
        current_byte = (current_byte << 1) | 1;
        bit_in_byte++;
        
        if (bit_in_byte == 8) {
            bitstream[byte_pos++] = current_byte;
            current_byte = 0;
            bit_in_byte = 0;
            bit_pos += 8;
        }
    }
    
    /* Flush remaining bits */
    if (bit_in_byte > 0 && byte_pos < bitstream_size) {
        current_byte <<= (8 - bit_in_byte);
        bitstream[byte_pos++] = current_byte;
        bit_pos += bit_in_byte;
    }
    
    *bits_decoded = bit_pos;
    ctx->total_flux_reversals += flux_count;
    ctx->total_bits_decoded += bit_pos;
    
    return UFT_SUCCESS;
}

/* ========================================================================
 * COMPLETE PIPELINE
 * ======================================================================== */

uft_rc_t uft_gcr_decode_track_from_flux(
    uft_gcr_ctx_t* ctx,
    uint8_t track_num,
    const uint32_t* flux_ns,
    uint32_t flux_count,
    uft_gcr_track_t* track
) {
    UFT_CHECK_NULLS(ctx, flux_ns, track);
    
    /* Set speed zone for track */
    const uft_gcr_speed_zone_t* zone = uft_gcr_get_speed_zone(track_num);
    if (!zone) {
        return UFT_ERR_INVALID_ARG;
    }
    
    ctx->speed_zone = zone;
    
    /* Allocate bitstream buffer (estimate: flux_count * 2 bits → bytes) */
    size_t bitstream_size = (flux_count * 2) / 8 + 1024;
    uint8_t* bitstream = malloc(bitstream_size);
    if (!bitstream) {
        return UFT_ERR_MEMORY;
    }
    
    /* Step 1: Flux → Bitstream */
    uint32_t bits_decoded = 0;
    uft_rc_t rc = uft_gcr_flux_to_bitstream(ctx, flux_ns, flux_count,
                                            bitstream, bitstream_size,
                                            &bits_decoded);
    
    if (uft_failed(rc)) {
        free(bitstream);
        return rc;
    }
    
    /* Step 2: Bitstream → GCR Sectors */
    rc = uft_gcr_decode_track(ctx, track_num, bitstream, bits_decoded, track);
    
    free(bitstream);
    return rc;
}

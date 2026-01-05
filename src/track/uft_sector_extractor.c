/**
 * @file uft_sector_extractor.c
 * @brief Sector Extractor Implementation
 * 
 * EXT3-004: Extract sectors from raw track data
 * 
 * Supports:
 * - FM/MFM (IBM compatible)
 * - Amiga MFM
 * - GCR (Commodore 64)
 * - Apple GCR
 * - Auto-detection
 */

#include "uft/track/uft_sector_extractor.h"
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* MFM sync patterns */
#define MFM_SYNC_A1_RAW     0x4489
#define MFM_SYNC_C2_RAW     0x5224

/* Address marks */
#define MFM_IDAM            0xFE
#define MFM_DAM             0xFB
#define MFM_DDAM            0xF8

/* Amiga sync */
#define AMIGA_SYNC          0x4489
#define AMIGA_SECTOR_SYNC   0x44894489

/* GCR patterns */
#define GCR_SYNC_C64        0xFF
#define GCR_HEADER_C64      0x08
#define GCR_DATA_C64        0x07

/* CRC polynomial */
#define CRC_POLY            0x1021

/*===========================================================================
 * CRC Functions
 *===========================================================================*/

static uint16_t crc16_update(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ CRC_POLY;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static uint16_t crc16_buffer(const uint8_t *data, size_t size, uint16_t init)
{
    uint16_t crc = init;
    for (size_t i = 0; i < size; i++) {
        crc = crc16_update(crc, data[i]);
    }
    return crc;
}

/*===========================================================================
 * Bit Stream Helpers
 *===========================================================================*/

static uint8_t read_bit(const uint8_t *data, size_t bit_pos)
{
    size_t byte_idx = bit_pos / 8;
    size_t bit_idx = 7 - (bit_pos % 8);
    return (data[byte_idx] >> bit_idx) & 1;
}

static uint16_t read_word(const uint8_t *data, size_t bit_pos)
{
    uint16_t word = 0;
    for (int i = 0; i < 16; i++) {
        word = (word << 1) | read_bit(data, bit_pos + i);
    }
    return word;
}

static uint8_t decode_mfm_byte(const uint8_t *data, size_t bit_pos)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        /* Skip clock bit, read data bit */
        byte = (byte << 1) | read_bit(data, bit_pos + i * 2 + 1);
    }
    return byte;
}

/*===========================================================================
 * Encoding Detection
 *===========================================================================*/

uft_track_encoding_t uft_sector_detect_encoding(const uint8_t *track_data,
                                                size_t track_bits)
{
    if (!track_data || track_bits < 1000) {
        return UFT_TRACK_ENC_UNKNOWN;
    }
    
    int mfm_syncs = 0;
    int amiga_syncs = 0;
    int gcr_syncs = 0;
    
    /* Scan for sync patterns */
    for (size_t pos = 0; pos + 32 < track_bits; pos++) {
        uint16_t word = read_word(track_data, pos);
        
        if (word == MFM_SYNC_A1_RAW) {
            mfm_syncs++;
            
            /* Check for Amiga double-sync */
            if (pos + 48 < track_bits) {
                uint16_t next = read_word(track_data, pos + 16);
                if (next == MFM_SYNC_A1_RAW) {
                    amiga_syncs++;
                }
            }
        }
    }
    
    /* Check for GCR (C64) */
    size_t track_bytes = track_bits / 8;
    for (size_t i = 0; i + 5 < track_bytes; i++) {
        if (track_data[i] == GCR_SYNC_C64 && 
            track_data[i + 1] == GCR_SYNC_C64 &&
            (track_data[i + 2] == GCR_HEADER_C64 || track_data[i + 2] == GCR_DATA_C64)) {
            gcr_syncs++;
        }
    }
    
    /* Determine encoding based on sync counts */
    if (amiga_syncs >= 5) {
        return UFT_TRACK_ENC_AMIGA;
    }
    if (mfm_syncs >= 5) {
        return UFT_TRACK_ENC_MFM;
    }
    if (gcr_syncs >= 5) {
        return UFT_TRACK_ENC_GCR_C64;
    }
    
    return UFT_TRACK_ENC_UNKNOWN;
}

/*===========================================================================
 * MFM Sector Extraction
 *===========================================================================*/

static int find_mfm_sync(const uint8_t *data, size_t bits, size_t start)
{
    for (size_t pos = start; pos + 48 < bits; pos++) {
        /* Look for A1 A1 A1 pattern */
        uint16_t w1 = read_word(data, pos);
        uint16_t w2 = read_word(data, pos + 16);
        uint16_t w3 = read_word(data, pos + 32);
        
        if (w1 == MFM_SYNC_A1_RAW && w2 == MFM_SYNC_A1_RAW && w3 == MFM_SYNC_A1_RAW) {
            return pos;
        }
    }
    return -1;
}

static int extract_mfm_sector(const uint8_t *track_data, size_t track_bits,
                              size_t sync_pos, uft_extracted_sector_t *sector)
{
    if (!track_data || !sector) return -1;
    
    size_t pos = sync_pos + 48;  /* Skip 3x A1 sync */
    
    /* Read address mark */
    uint8_t am = decode_mfm_byte(track_data, pos);
    pos += 16;
    
    if (am != MFM_IDAM) {
        /* Not an ID field - might be data field */
        return -1;
    }
    
    /* Read ID field: C H R N */
    sector->track = decode_mfm_byte(track_data, pos); pos += 16;
    sector->side = decode_mfm_byte(track_data, pos); pos += 16;
    sector->sector = decode_mfm_byte(track_data, pos); pos += 16;
    sector->size_code = decode_mfm_byte(track_data, pos); pos += 16;
    
    /* Read and verify CRC */
    uint8_t crc_hi = decode_mfm_byte(track_data, pos); pos += 16;
    uint8_t crc_lo = decode_mfm_byte(track_data, pos); pos += 16;
    uint16_t stored_crc = (crc_hi << 8) | crc_lo;
    
    /* Calculate CRC over A1 A1 A1 FE C H R N */
    uint16_t calc_crc = 0xFFFF;
    calc_crc = crc16_update(calc_crc, 0xA1);
    calc_crc = crc16_update(calc_crc, 0xA1);
    calc_crc = crc16_update(calc_crc, 0xA1);
    calc_crc = crc16_update(calc_crc, MFM_IDAM);
    calc_crc = crc16_update(calc_crc, sector->track);
    calc_crc = crc16_update(calc_crc, sector->side);
    calc_crc = crc16_update(calc_crc, sector->sector);
    calc_crc = crc16_update(calc_crc, sector->size_code);
    
    sector->id_crc_ok = (stored_crc == calc_crc);
    
    /* Find data field sync */
    int data_sync = find_mfm_sync(track_data, track_bits, pos);
    if (data_sync < 0 || data_sync > (int)(pos + 800)) {  /* Max ~50 bytes gap */
        sector->has_data = false;
        return 0;
    }
    
    pos = data_sync + 48;  /* Skip 3x A1 sync */
    
    /* Read data address mark */
    uint8_t dam = decode_mfm_byte(track_data, pos);
    pos += 16;
    
    if (dam != MFM_DAM && dam != MFM_DDAM) {
        sector->has_data = false;
        return 0;
    }
    
    sector->deleted = (dam == MFM_DDAM);
    sector->has_data = true;
    sector->data_size = 128 << sector->size_code;
    
    /* Read data */
    if (sector->data_size > sizeof(sector->data)) {
        sector->data_size = sizeof(sector->data);
    }
    
    calc_crc = 0xFFFF;
    calc_crc = crc16_update(calc_crc, 0xA1);
    calc_crc = crc16_update(calc_crc, 0xA1);
    calc_crc = crc16_update(calc_crc, 0xA1);
    calc_crc = crc16_update(calc_crc, dam);
    
    for (size_t i = 0; i < sector->data_size; i++) {
        sector->data[i] = decode_mfm_byte(track_data, pos);
        calc_crc = crc16_update(calc_crc, sector->data[i]);
        pos += 16;
    }
    
    /* Read and verify data CRC */
    crc_hi = decode_mfm_byte(track_data, pos); pos += 16;
    crc_lo = decode_mfm_byte(track_data, pos); pos += 16;
    stored_crc = (crc_hi << 8) | crc_lo;
    
    sector->data_crc_ok = (stored_crc == calc_crc);
    
    return 0;
}

/*===========================================================================
 * Context Management
 *===========================================================================*/

uft_sector_extract_ctx_t *uft_sector_extract_create(void)
{
    uft_sector_extract_ctx_t *ctx = calloc(1, sizeof(uft_sector_extract_ctx_t));
    return ctx;
}

void uft_sector_extract_destroy(uft_sector_extract_ctx_t *ctx)
{
    free(ctx);
}

/*===========================================================================
 * Main Extraction Functions
 *===========================================================================*/

int uft_sector_extract_track(uft_sector_extract_ctx_t *ctx,
                             const uint8_t *track_data, size_t track_bits,
                             uft_track_encoding_t encoding)
{
    if (!ctx || !track_data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    /* Auto-detect encoding if needed */
    if (encoding == UFT_TRACK_ENC_UNKNOWN) {
        encoding = uft_sector_detect_encoding(track_data, track_bits);
    }
    ctx->encoding = encoding;
    
    /* Extract based on encoding */
    switch (encoding) {
        case UFT_TRACK_ENC_MFM:
        case UFT_TRACK_ENC_FM:
            {
                size_t pos = 0;
                while (ctx->sector_count < UFT_MAX_SECTORS_PER_TRACK) {
                    int sync_pos = find_mfm_sync(track_data, track_bits, pos);
                    if (sync_pos < 0) break;
                    
                    uft_extracted_sector_t *sector = &ctx->sectors[ctx->sector_count];
                    
                    if (extract_mfm_sector(track_data, track_bits, sync_pos, sector) == 0) {
                        sector->bit_position = sync_pos;
                        ctx->sector_count++;
                    }
                    
                    pos = sync_pos + 100;  /* Move past this sector */
                }
            }
            break;
            
        case UFT_TRACK_ENC_AMIGA:
            /* TODO: Implement Amiga sector extraction */
            break;
            
        case UFT_TRACK_ENC_GCR_C64:
            /* TODO: Implement C64 GCR sector extraction */
            break;
            
        case UFT_TRACK_ENC_GCR_APPLE:
            /* TODO: Implement Apple GCR sector extraction */
            break;
            
        default:
            return -1;
    }
    
    return ctx->sector_count;
}

int uft_sector_extract_get_sector(const uft_sector_extract_ctx_t *ctx,
                                  uint8_t sector_id,
                                  uft_extracted_sector_t *sector)
{
    if (!ctx || !sector) return -1;
    
    for (int i = 0; i < ctx->sector_count; i++) {
        if (ctx->sectors[i].sector == sector_id) {
            *sector = ctx->sectors[i];
            return 0;
        }
    }
    
    return -1;  /* Sector not found */
}

int uft_sector_extract_get_data(const uft_sector_extract_ctx_t *ctx,
                                uint8_t sector_id,
                                uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size) return -1;
    
    for (int i = 0; i < ctx->sector_count; i++) {
        if (ctx->sectors[i].sector == sector_id) {
            if (!ctx->sectors[i].has_data) {
                return -1;
            }
            
            size_t copy_size = ctx->sectors[i].data_size;
            if (copy_size > *size) {
                copy_size = *size;
            }
            
            memcpy(buffer, ctx->sectors[i].data, copy_size);
            *size = copy_size;
            
            return ctx->sectors[i].data_crc_ok ? 0 : 1;  /* 1 = CRC error */
        }
    }
    
    return -1;
}

/*===========================================================================
 * Weak Bit Detection
 *===========================================================================*/

int uft_sector_detect_weak_bits(const uint8_t *track_data1, size_t bits1,
                                const uint8_t *track_data2, size_t bits2,
                                uint32_t *weak_positions, size_t max_weak,
                                size_t *weak_count)
{
    if (!track_data1 || !track_data2 || !weak_positions || !weak_count) {
        return -1;
    }
    
    *weak_count = 0;
    
    size_t min_bits = (bits1 < bits2) ? bits1 : bits2;
    
    for (size_t pos = 0; pos < min_bits && *weak_count < max_weak; pos++) {
        uint8_t bit1 = read_bit(track_data1, pos);
        uint8_t bit2 = read_bit(track_data2, pos);
        
        if (bit1 != bit2) {
            weak_positions[*weak_count] = pos;
            (*weak_count)++;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Report Generation
 *===========================================================================*/

int uft_sector_extract_report(const uft_sector_extract_ctx_t *ctx,
                              char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    int written = 0;
    int ret;
    
    ret = snprintf(buffer + written, size - written,
        "{\n"
        "  \"encoding\": \"%s\",\n"
        "  \"sector_count\": %d,\n"
        "  \"sectors\": [\n",
        ctx->encoding == UFT_TRACK_ENC_MFM ? "MFM" :
        ctx->encoding == UFT_TRACK_ENC_FM ? "FM" :
        ctx->encoding == UFT_TRACK_ENC_AMIGA ? "Amiga" :
        ctx->encoding == UFT_TRACK_ENC_GCR_C64 ? "GCR_C64" :
        ctx->encoding == UFT_TRACK_ENC_GCR_APPLE ? "GCR_Apple" : "Unknown",
        ctx->sector_count
    );
    if (ret < 0 || ret >= (int)(size - written)) return -1;
    written += ret;
    
    for (int i = 0; i < ctx->sector_count; i++) {
        const uft_extracted_sector_t *s = &ctx->sectors[i];
        
        ret = snprintf(buffer + written, size - written,
            "    { \"id\": %d, \"track\": %d, \"side\": %d, "
            "\"size\": %zu, \"id_crc\": %s, \"data_crc\": %s, "
            "\"deleted\": %s }%s\n",
            s->sector, s->track, s->side, s->data_size,
            s->id_crc_ok ? "true" : "false",
            s->data_crc_ok ? "true" : "false",
            s->deleted ? "true" : "false",
            (i < ctx->sector_count - 1) ? "," : ""
        );
        if (ret < 0 || ret >= (int)(size - written)) return -1;
        written += ret;
    }
    
    ret = snprintf(buffer + written, size - written, "  ]\n}");
    if (ret < 0 || ret >= (int)(size - written)) return -1;
    
    return 0;
}

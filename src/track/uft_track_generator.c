/**
 * @file uft_track_generator.c
 * @brief Track Generator Implementation
 * 
 * EXT3-003: Generate formatted tracks with various encodings
 * 
 * Supports:
 * - FM (Single Density)
 * - MFM (Double/High Density)
 * - GCR (Commodore/Apple)
 * - Amiga MFM
 */

#include "uft/track/uft_track_generator.h"
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* MFM sync patterns */
#define MFM_SYNC_A1         0x4489      /**< A1 sync (missing clock) */
#define MFM_SYNC_C2         0x5224      /**< C2 sync (missing clock) */

/* Address marks */
#define MFM_IAM             0xFC        /**< Index Address Mark */
#define MFM_IDAM            0xFE        /**< ID Address Mark */
#define MFM_DAM             0xFB        /**< Data Address Mark */
#define MFM_DDAM            0xF8        /**< Deleted Data Address Mark */

/* FM patterns (with clock) */
#define FM_IAM              0xFC
#define FM_IDAM             0xFE
#define FM_DAM              0xFB
#define FM_DDAM             0xF8

/* Gap fill bytes */
#define GAP_FILL_MFM        0x4E
#define GAP_FILL_FM         0xFF

/* CRC polynomial */
#define CRC_POLY            0x1021

/*===========================================================================
 * CRC-16-CCITT
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

static uint16_t crc16_buffer(const uint8_t *data, size_t size)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = crc16_update(crc, data[i]);
    }
    return crc;
}

/*===========================================================================
 * MFM Encoding
 *===========================================================================*/

static void mfm_encode_byte(uint8_t byte, uint8_t prev_bit, uint16_t *mfm_word)
{
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (byte >> i) & 1;
        uint8_t clock = (prev_bit == 0 && bit == 0) ? 1 : 0;
        
        result = (result << 1) | clock;
        result = (result << 1) | bit;
        
        prev_bit = bit;
    }
    
    *mfm_word = result;
}

/*===========================================================================
 * Track Context Management
 *===========================================================================*/

uft_track_gen_ctx_t *uft_track_gen_create(uft_track_encoding_t encoding,
                                          size_t track_bits)
{
    uft_track_gen_ctx_t *ctx = calloc(1, sizeof(uft_track_gen_ctx_t));
    if (!ctx) return NULL;
    
    ctx->encoding = encoding;
    ctx->track_bits = track_bits;
    ctx->buffer_size = (track_bits + 7) / 8;
    ctx->buffer = calloc(ctx->buffer_size, 1);
    
    if (!ctx->buffer) {
        free(ctx);
        return NULL;
    }
    
    /* Set default parameters */
    switch (encoding) {
        case UFT_TRACK_ENC_FM:
            ctx->data_rate = 250000;
            ctx->rpm = 300;
            break;
        case UFT_TRACK_ENC_MFM:
            ctx->data_rate = 500000;
            ctx->rpm = 300;
            break;
        case UFT_TRACK_ENC_GCR_C64:
            ctx->data_rate = 307692;  /* Zone 0 */
            ctx->rpm = 300;
            break;
        case UFT_TRACK_ENC_GCR_APPLE:
            ctx->data_rate = 250000;
            ctx->rpm = 300;
            break;
        case UFT_TRACK_ENC_AMIGA:
            ctx->data_rate = 500000;
            ctx->rpm = 300;
            break;
    }
    
    return ctx;
}

void uft_track_gen_destroy(uft_track_gen_ctx_t *ctx)
{
    if (ctx) {
        free(ctx->buffer);
        free(ctx);
    }
}

void uft_track_gen_reset(uft_track_gen_ctx_t *ctx)
{
    if (ctx) {
        ctx->bit_position = 0;
        ctx->byte_position = 0;
        memset(ctx->buffer, 0, ctx->buffer_size);
    }
}

/*===========================================================================
 * Low-Level Write Functions
 *===========================================================================*/

static void write_bit(uft_track_gen_ctx_t *ctx, uint8_t bit)
{
    if (ctx->bit_position >= ctx->track_bits) return;
    
    size_t byte_idx = ctx->bit_position / 8;
    size_t bit_idx = 7 - (ctx->bit_position % 8);
    
    if (bit) {
        ctx->buffer[byte_idx] |= (1 << bit_idx);
    }
    
    ctx->bit_position++;
}

static void write_byte_raw(uft_track_gen_ctx_t *ctx, uint8_t byte)
{
    for (int i = 7; i >= 0; i--) {
        write_bit(ctx, (byte >> i) & 1);
    }
}

static void write_mfm_byte(uft_track_gen_ctx_t *ctx, uint8_t byte)
{
    uint8_t prev_bit = ctx->last_bit;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (byte >> i) & 1;
        uint8_t clock = (prev_bit == 0 && bit == 0) ? 1 : 0;
        
        write_bit(ctx, clock);
        write_bit(ctx, bit);
        
        prev_bit = bit;
    }
    
    ctx->last_bit = prev_bit;
}

static void write_mfm_sync(uft_track_gen_ctx_t *ctx)
{
    /* Write A1 sync with missing clock bit */
    /* A1 = 10100001, MFM with missing clock = 0100010010001001 = 0x4489 */
    write_bit(ctx, 0); write_bit(ctx, 1);
    write_bit(ctx, 0); write_bit(ctx, 0);
    write_bit(ctx, 0); write_bit(ctx, 1);
    write_bit(ctx, 0); write_bit(ctx, 0);
    write_bit(ctx, 1); write_bit(ctx, 0);
    write_bit(ctx, 0); write_bit(ctx, 0);
    write_bit(ctx, 1); write_bit(ctx, 0);
    write_bit(ctx, 0); write_bit(ctx, 1);
    
    ctx->last_bit = 1;
}

/*===========================================================================
 * Gap Generation
 *===========================================================================*/

int uft_track_gen_write_gap(uft_track_gen_ctx_t *ctx, size_t bytes)
{
    if (!ctx) return -1;
    
    uint8_t fill = (ctx->encoding == UFT_TRACK_ENC_FM) ? GAP_FILL_FM : GAP_FILL_MFM;
    
    for (size_t i = 0; i < bytes; i++) {
        if (ctx->encoding == UFT_TRACK_ENC_FM) {
            write_byte_raw(ctx, fill);
        } else {
            write_mfm_byte(ctx, fill);
        }
    }
    
    return 0;
}

int uft_track_gen_write_sync(uft_track_gen_ctx_t *ctx, size_t bytes)
{
    if (!ctx) return -1;
    
    if (ctx->encoding == UFT_TRACK_ENC_FM) {
        for (size_t i = 0; i < bytes; i++) {
            write_byte_raw(ctx, 0x00);
        }
    } else {
        for (size_t i = 0; i < bytes; i++) {
            write_mfm_byte(ctx, 0x00);
        }
    }
    
    return 0;
}

/*===========================================================================
 * Sector Header Generation
 *===========================================================================*/

int uft_track_gen_write_idam(uft_track_gen_ctx_t *ctx, uint8_t track, uint8_t side,
                             uint8_t sector, uint8_t size_code)
{
    if (!ctx) return -1;
    
    if (ctx->encoding == UFT_TRACK_ENC_FM) {
        /* FM: sync + IAM */
        write_byte_raw(ctx, 0x00);
        write_byte_raw(ctx, 0x00);
        write_byte_raw(ctx, 0x00);
        write_byte_raw(ctx, MFM_IDAM);
    } else {
        /* MFM: A1 A1 A1 FE */
        write_mfm_sync(ctx);
        write_mfm_sync(ctx);
        write_mfm_sync(ctx);
        write_mfm_byte(ctx, MFM_IDAM);
    }
    
    /* ID field */
    uint8_t id_field[4] = { track, side, sector, size_code };
    uint16_t crc = 0xFFFF;
    crc = crc16_update(crc, MFM_IDAM);
    
    for (int i = 0; i < 4; i++) {
        if (ctx->encoding == UFT_TRACK_ENC_FM) {
            write_byte_raw(ctx, id_field[i]);
        } else {
            write_mfm_byte(ctx, id_field[i]);
        }
        crc = crc16_update(crc, id_field[i]);
    }
    
    /* CRC */
    if (ctx->encoding == UFT_TRACK_ENC_FM) {
        write_byte_raw(ctx, crc >> 8);
        write_byte_raw(ctx, crc & 0xFF);
    } else {
        write_mfm_byte(ctx, crc >> 8);
        write_mfm_byte(ctx, crc & 0xFF);
    }
    
    return 0;
}

/*===========================================================================
 * Data Field Generation
 *===========================================================================*/

int uft_track_gen_write_data(uft_track_gen_ctx_t *ctx, const uint8_t *data,
                             size_t size, bool deleted)
{
    if (!ctx || !data) return -1;
    
    uint8_t dam = deleted ? MFM_DDAM : MFM_DAM;
    
    if (ctx->encoding == UFT_TRACK_ENC_FM) {
        write_byte_raw(ctx, 0x00);
        write_byte_raw(ctx, 0x00);
        write_byte_raw(ctx, 0x00);
        write_byte_raw(ctx, dam);
    } else {
        write_mfm_sync(ctx);
        write_mfm_sync(ctx);
        write_mfm_sync(ctx);
        write_mfm_byte(ctx, dam);
    }
    
    /* Data + CRC */
    uint16_t crc = 0xFFFF;
    crc = crc16_update(crc, dam);
    
    for (size_t i = 0; i < size; i++) {
        if (ctx->encoding == UFT_TRACK_ENC_FM) {
            write_byte_raw(ctx, data[i]);
        } else {
            write_mfm_byte(ctx, data[i]);
        }
        crc = crc16_update(crc, data[i]);
    }
    
    if (ctx->encoding == UFT_TRACK_ENC_FM) {
        write_byte_raw(ctx, crc >> 8);
        write_byte_raw(ctx, crc & 0xFF);
    } else {
        write_mfm_byte(ctx, crc >> 8);
        write_mfm_byte(ctx, crc & 0xFF);
    }
    
    return 0;
}

/*===========================================================================
 * Complete Sector Generation
 *===========================================================================*/

int uft_track_gen_write_sector(uft_track_gen_ctx_t *ctx,
                               uint8_t track, uint8_t side, uint8_t sector,
                               uint8_t size_code, const uint8_t *data,
                               uint8_t gap2, uint8_t gap3)
{
    if (!ctx || !data) return -1;
    
    size_t sector_size = 128 << size_code;
    
    /* ID Address Mark */
    uft_track_gen_write_idam(ctx, track, side, sector, size_code);
    
    /* Gap 2 */
    uft_track_gen_write_gap(ctx, gap2);
    
    /* Data sync */
    uft_track_gen_write_sync(ctx, 12);
    
    /* Data Address Mark + Data + CRC */
    uft_track_gen_write_data(ctx, data, sector_size, false);
    
    /* Gap 3 */
    uft_track_gen_write_gap(ctx, gap3);
    
    return 0;
}

/*===========================================================================
 * Format Presets
 *===========================================================================*/

int uft_track_gen_format_ibm(uft_track_gen_ctx_t *ctx,
                             const uft_format_preset_t *preset,
                             uint8_t track, uint8_t side)
{
    if (!ctx || !preset) return -1;
    
    uft_track_gen_reset(ctx);
    
    /* Gap 4a (post-index) */
    uft_track_gen_write_gap(ctx, preset->gap4a);
    
    /* Index sync */
    uft_track_gen_write_sync(ctx, 12);
    
    /* Index Address Mark (optional) */
    if (preset->write_iam) {
        if (ctx->encoding == UFT_TRACK_ENC_MFM) {
            write_mfm_sync(ctx);
            write_mfm_sync(ctx);
            write_mfm_sync(ctx);
            write_mfm_byte(ctx, MFM_IAM);
        }
    }
    
    /* Gap 1 */
    uft_track_gen_write_gap(ctx, preset->gap1);
    
    /* Sectors */
    uint8_t *empty_sector = (uint8_t*)calloc(8192, 1); if (!empty_sector) return UFT_ERR_MEMORY;
    memset(empty_sector, preset->fill_byte, sizeof(empty_sector));
    
    for (int s = 0; s < preset->sectors; s++) {
        /* Sector sync */
        uft_track_gen_write_sync(ctx, 12);
        
        /* Write sector */
        uft_track_gen_write_sector(ctx, track, side, s + preset->first_sector,
                                   preset->size_code, empty_sector,
                                   preset->gap2, preset->gap3);
    }
    
    /* Fill remaining track with gap 4b */
    while (ctx->bit_position < ctx->track_bits - 16) {
        if (ctx->encoding == UFT_TRACK_ENC_FM) {
            write_byte_raw(ctx, GAP_FILL_FM);
        } else {
            write_mfm_byte(ctx, GAP_FILL_MFM);
        }
    }
    
    return 0;
}

/*===========================================================================
 * Standard Presets
 *===========================================================================*/

static const uft_format_preset_t PRESET_IBM_DD = {
    .sectors = 9,
    .sector_size = 512,
    .size_code = 2,
    .first_sector = 1,
    .gap4a = 80,
    .gap1 = 50,
    .gap2 = 22,
    .gap3 = 80,
    .fill_byte = 0xE5,
    .write_iam = true
};

static const uft_format_preset_t PRESET_IBM_HD = {
    .sectors = 18,
    .sector_size = 512,
    .size_code = 2,
    .first_sector = 1,
    .gap4a = 80,
    .gap1 = 50,
    .gap2 = 22,
    .gap3 = 84,
    .fill_byte = 0xE5,
    .write_iam = true
};

static const uft_format_preset_t PRESET_AMIGA_DD = {
    .sectors = 11,
    .sector_size = 512,
    .size_code = 2,
    .first_sector = 0,
    .gap4a = 0,
    .gap1 = 0,
    .gap2 = 0,
    .gap3 = 0,
    .fill_byte = 0x00,
    .write_iam = false
};

const uft_format_preset_t *uft_track_gen_get_preset(const char *name)
{
    if (!name) return NULL;
    
    if (strcmp(name, "IBM_DD") == 0 || strcmp(name, "PC_DD") == 0) {
        return &PRESET_IBM_DD;
    }
    if (strcmp(name, "IBM_HD") == 0 || strcmp(name, "PC_HD") == 0) {
        return &PRESET_IBM_HD;
    }
    if (strcmp(name, "AMIGA_DD") == 0) {
        return &PRESET_AMIGA_DD;
    }
    
    return NULL;
}

/*===========================================================================
 * Output
 *===========================================================================*/

int uft_track_gen_get_data(const uft_track_gen_ctx_t *ctx,
                           uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size) return -1;
    
    size_t bytes_used = (ctx->bit_position + 7) / 8;
    
    if (*size < bytes_used) {
        *size = bytes_used;
        return -1;
    }
    
    memcpy(buffer, ctx->buffer, bytes_used);
    *size = bytes_used;
    
    return 0;
}

size_t uft_track_gen_get_bit_count(const uft_track_gen_ctx_t *ctx)
{
    return ctx ? ctx->bit_position : 0;
}

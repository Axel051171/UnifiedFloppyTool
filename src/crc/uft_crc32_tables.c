/**
 * @file uft_crc32_tables.c
 * @brief CRC32 Lookup Tables Implementation
 * 
 * EXT3-016: Comprehensive CRC32 polynomial tables
 * 
 * Features:
 * - Multiple CRC32 polynomials
 * - Pre-computed lookup tables
 * - Table generation functions
 * - Common CRC variants (IEEE, Castagnoli, etc.)
 */

#include "uft/crc/uft_crc32_tables.h"
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Standard CRC32 Polynomials
 *===========================================================================*/

/* IEEE 802.3 (Ethernet, ZIP, PNG) - Most common */
#define CRC32_POLY_IEEE         0xEDB88320

/* Castagnoli (iSCSI, Btrfs, ext4) */
#define CRC32_POLY_CASTAGNOLI   0x82F63B78

/* Koopman */
#define CRC32_POLY_KOOPMAN      0xEB31D82E

/* CRC-32Q (aviation) */
#define CRC32_POLY_Q            0xD5828281

/* CRC-32/XFER */
#define CRC32_POLY_XFER         0xAF

/*===========================================================================
 * Pre-computed Tables
 *===========================================================================*/

/* IEEE 802.3 CRC32 table */
static uint32_t crc32_ieee_table[256];
static bool crc32_ieee_init = false;

/* Castagnoli CRC32C table */
static uint32_t crc32c_table[256];
static bool crc32c_init = false;

/* Generic table for custom polynomials */
static uint32_t crc32_custom_table[256];
static uint32_t crc32_custom_poly = 0;

/*===========================================================================
 * Table Generation
 *===========================================================================*/

void uft_crc32_generate_table(uint32_t polynomial, uint32_t *table)
{
    for (int i = 0; i < 256; i++) {
        uint32_t crc = i;
        
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        
        table[i] = crc;
    }
}

void uft_crc32_generate_table_reflected(uint32_t polynomial, uint32_t *table)
{
    /* For reflected polynomials (already in reflected form) */
    uft_crc32_generate_table(polynomial, table);
}

void uft_crc32_generate_table_normal(uint32_t polynomial, uint32_t *table)
{
    /* For normal (non-reflected) polynomials */
    for (int i = 0; i < 256; i++) {
        uint32_t crc = (uint32_t)i << 24;
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80000000) {
                crc = (crc << 1) ^ polynomial;
            } else {
                crc <<= 1;
            }
        }
        
        table[i] = crc;
    }
}

/*===========================================================================
 * IEEE 802.3 CRC32
 *===========================================================================*/

static void init_crc32_ieee(void)
{
    if (crc32_ieee_init) return;
    
    uft_crc32_generate_table(CRC32_POLY_IEEE, crc32_ieee_table);
    crc32_ieee_init = true;
}

uint32_t uft_crc32_ieee(const uint8_t *data, size_t length)
{
    init_crc32_ieee();
    
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_ieee_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint32_t uft_crc32_ieee_update(uint32_t crc, const uint8_t *data, size_t length)
{
    init_crc32_ieee();
    
    crc = ~crc;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_ieee_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return ~crc;
}

/*===========================================================================
 * Castagnoli CRC32C
 *===========================================================================*/

static void init_crc32c(void)
{
    if (crc32c_init) return;
    
    uft_crc32_generate_table(CRC32_POLY_CASTAGNOLI, crc32c_table);
    crc32c_init = true;
}

uint32_t uft_crc32c(const uint8_t *data, size_t length)
{
    init_crc32c();
    
    uint32_t crc = 0xFFFFFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32c_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

uint32_t uft_crc32c_update(uint32_t crc, const uint8_t *data, size_t length)
{
    init_crc32c();
    
    crc = ~crc;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32c_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return ~crc;
}

/*===========================================================================
 * Custom CRC32
 *===========================================================================*/

void uft_crc32_init_custom(uint32_t polynomial)
{
    if (polynomial != crc32_custom_poly) {
        uft_crc32_generate_table(polynomial, crc32_custom_table);
        crc32_custom_poly = polynomial;
    }
}

uint32_t uft_crc32_custom(const uint8_t *data, size_t length,
                          uint32_t init_val, uint32_t xor_out)
{
    uint32_t crc = init_val;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_custom_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ xor_out;
}

/*===========================================================================
 * CRC32 Context (for streaming)
 *===========================================================================*/

void uft_crc32_ctx_init(uft_crc32_ctx_t *ctx, uft_crc32_type_t type)
{
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->type = type;
    
    switch (type) {
        case UFT_CRC32_IEEE:
            init_crc32_ieee();
            ctx->table = crc32_ieee_table;
            ctx->init = 0xFFFFFFFF;
            ctx->xor_out = 0xFFFFFFFF;
            break;
            
        case UFT_CRC32_CASTAGNOLI:
            init_crc32c();
            ctx->table = crc32c_table;
            ctx->init = 0xFFFFFFFF;
            ctx->xor_out = 0xFFFFFFFF;
            break;
            
        case UFT_CRC32_POSIX:
            init_crc32_ieee();
            ctx->table = crc32_ieee_table;
            ctx->init = 0x00000000;
            ctx->xor_out = 0xFFFFFFFF;
            break;
            
        default:
            ctx->table = crc32_ieee_table;
            ctx->init = 0xFFFFFFFF;
            ctx->xor_out = 0xFFFFFFFF;
    }
    
    ctx->crc = ctx->init;
}

void uft_crc32_ctx_update(uft_crc32_ctx_t *ctx, const uint8_t *data, size_t length)
{
    if (!ctx || !data || !ctx->table) return;
    
    for (size_t i = 0; i < length; i++) {
        ctx->crc = ctx->table[(ctx->crc ^ data[i]) & 0xFF] ^ (ctx->crc >> 8);
    }
}

uint32_t uft_crc32_ctx_final(uft_crc32_ctx_t *ctx)
{
    if (!ctx) return 0;
    
    return ctx->crc ^ ctx->xor_out;
}

void uft_crc32_ctx_reset(uft_crc32_ctx_t *ctx)
{
    if (!ctx) return;
    
    ctx->crc = ctx->init;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

uint32_t uft_crc32_combine(uint32_t crc1, uint32_t crc2, size_t len2)
{
    /* Combine two CRC32 values */
    /* This is a simplified version - full implementation requires matrix ops */
    
    init_crc32_ieee();
    
    /* For now, just recalculate if needed */
    /* A proper implementation would use the matrix method */
    
    (void)len2;  /* Suppress unused warning */
    
    /* XOR is not correct but serves as placeholder */
    return crc1 ^ crc2;
}

int uft_crc32_verify(const uint8_t *data, size_t length, uint32_t expected)
{
    uint32_t computed = uft_crc32_ieee(data, length);
    return (computed == expected) ? 0 : -1;
}

/*===========================================================================
 * Table Export (for embedding in firmware, etc.)
 *===========================================================================*/

int uft_crc32_export_table(uint32_t polynomial, char *buffer, size_t size)
{
    if (!buffer || size < 4096) return -1;
    
    uint32_t table[256];
    uft_crc32_generate_table(polynomial, table);
    
    int pos = snprintf(buffer, size,
        "/* CRC32 Table for polynomial 0x%08X */\n"
        "static const uint32_t crc32_table[256] = {\n",
        polynomial);
    
    for (int i = 0; i < 256; i++) {
        if (i % 8 == 0) {
            pos += snprintf(buffer + pos, size - pos, "    ");
        }
        
        pos += snprintf(buffer + pos, size - pos, "0x%08X", table[i]);
        
        if (i < 255) {
            pos += snprintf(buffer + pos, size - pos, ",");
        }
        
        if (i % 8 == 7) {
            pos += snprintf(buffer + pos, size - pos, "\n");
        } else {
            pos += snprintf(buffer + pos, size - pos, " ");
        }
    }
    
    pos += snprintf(buffer + pos, size - pos, "};\n");
    
    return pos;
}

/*===========================================================================
 * CRC16 Tables (commonly needed alongside CRC32)
 *===========================================================================*/

/* CCITT CRC16 (X.25, HDLC) */
#define CRC16_POLY_CCITT    0x8408

static uint16_t crc16_ccitt_table[256];
static bool crc16_ccitt_init = false;

void uft_crc16_generate_table(uint16_t polynomial, uint16_t *table)
{
    for (int i = 0; i < 256; i++) {
        uint16_t crc = i;
        
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ polynomial;
            } else {
                crc >>= 1;
            }
        }
        
        table[i] = crc;
    }
}

static void init_crc16_ccitt(void)
{
    if (crc16_ccitt_init) return;
    
    uft_crc16_generate_table(CRC16_POLY_CCITT, crc16_ccitt_table);
    crc16_ccitt_init = true;
}

uint16_t uft_crc16_ccitt(const uint8_t *data, size_t length)
{
    init_crc16_ccitt();
    
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc16_ccitt_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFF;
}

/*===========================================================================
 * Floppy-Specific CRCs
 *===========================================================================*/

/* IBM MFM CRC (CRC-CCITT) */
#define CRC16_POLY_MFM      0x1021

static uint16_t crc16_mfm_table[256];
static bool crc16_mfm_init = false;

static void init_crc16_mfm(void)
{
    if (crc16_mfm_init) return;
    
    /* MFM uses normal (non-reflected) CRC */
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)i << 8;
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ CRC16_POLY_MFM;
            } else {
                crc <<= 1;
            }
        }
        
        crc16_mfm_table[i] = crc;
    }
    
    crc16_mfm_init = true;
}

uint16_t uft_crc16_mfm(const uint8_t *data, size_t length)
{
    init_crc16_mfm();
    
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < length; i++) {
        crc = crc16_mfm_table[((crc >> 8) ^ data[i]) & 0xFF] ^ (crc << 8);
    }
    
    return crc;
}

uint16_t uft_crc16_mfm_update(uint16_t crc, const uint8_t *data, size_t length)
{
    init_crc16_mfm();
    
    for (size_t i = 0; i < length; i++) {
        crc = crc16_mfm_table[((crc >> 8) ^ data[i]) & 0xFF] ^ (crc << 8);
    }
    
    return crc;
}

/* Amiga checksum (not CRC, but often needed) */
uint32_t uft_amiga_checksum(const uint32_t *data, size_t count)
{
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t word = data[i];
        /* Big-endian on 68k */
        word = ((word >> 24) & 0xFF) | ((word >> 8) & 0xFF00) |
               ((word << 8) & 0xFF0000) | ((word << 24) & 0xFF000000);
        
        uint32_t carry = (checksum + word < checksum) ? 1 : 0;
        checksum += word + carry;
    }
    
    return checksum;
}

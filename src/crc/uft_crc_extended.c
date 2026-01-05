/**
 * @file uft_crc_extended.c
 * @brief Extended CRC Polynomial Support
 * 
 * EXT2-013: Extended CRC calculations for various formats
 * 
 * Features:
 * - Multiple CRC algorithms
 * - Configurable polynomials
 * - Bitwise and table-based
 * - Init/XorOut support
 * - Reflected polynomials
 */

#include "uft/crc/uft_crc_extended.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * CRC Algorithms Database
 *===========================================================================*/

/* Standard CRC definitions */
static const uft_crc_def_t crc_algorithms[] = {
    /* CRC-8 variants */
    {"CRC-8", 8, 0x07, 0x00, 0x00, false, false},
    {"CRC-8/CDMA2000", 8, 0x9B, 0xFF, 0x00, false, false},
    {"CRC-8/DARC", 8, 0x39, 0x00, 0x00, true, true},
    {"CRC-8/DVB-S2", 8, 0xD5, 0x00, 0x00, false, false},
    {"CRC-8/EBU", 8, 0x1D, 0xFF, 0x00, true, true},
    {"CRC-8/I-CODE", 8, 0x1D, 0xFD, 0x00, false, false},
    {"CRC-8/ITU", 8, 0x07, 0x00, 0x55, false, false},
    {"CRC-8/MAXIM", 8, 0x31, 0x00, 0x00, true, true},
    {"CRC-8/ROHC", 8, 0x07, 0xFF, 0x00, true, true},
    {"CRC-8/WCDMA", 8, 0x9B, 0x00, 0x00, true, true},
    
    /* CRC-16 variants */
    {"CRC-16/CCITT-FALSE", 16, 0x1021, 0xFFFF, 0x0000, false, false},
    {"CRC-16/ARC", 16, 0x8005, 0x0000, 0x0000, true, true},
    {"CRC-16/AUG-CCITT", 16, 0x1021, 0x1D0F, 0x0000, false, false},
    {"CRC-16/BUYPASS", 16, 0x8005, 0x0000, 0x0000, false, false},
    {"CRC-16/CDMA2000", 16, 0xC867, 0xFFFF, 0x0000, false, false},
    {"CRC-16/DDS-110", 16, 0x8005, 0x800D, 0x0000, false, false},
    {"CRC-16/DECT-R", 16, 0x0589, 0x0000, 0x0001, false, false},
    {"CRC-16/DECT-X", 16, 0x0589, 0x0000, 0x0000, false, false},
    {"CRC-16/DNP", 16, 0x3D65, 0x0000, 0xFFFF, true, true},
    {"CRC-16/EN-13757", 16, 0x3D65, 0x0000, 0xFFFF, false, false},
    {"CRC-16/GENIBUS", 16, 0x1021, 0xFFFF, 0xFFFF, false, false},
    {"CRC-16/MAXIM", 16, 0x8005, 0x0000, 0xFFFF, true, true},
    {"CRC-16/MCRF4XX", 16, 0x1021, 0xFFFF, 0x0000, true, true},
    {"CRC-16/MODBUS", 16, 0x8005, 0xFFFF, 0x0000, true, true},
    {"CRC-16/USB", 16, 0x8005, 0xFFFF, 0xFFFF, true, true},
    {"CRC-16/X-25", 16, 0x1021, 0xFFFF, 0xFFFF, true, true},
    {"CRC-16/XMODEM", 16, 0x1021, 0x0000, 0x0000, false, false},
    {"CRC-16/KERMIT", 16, 0x1021, 0x0000, 0x0000, true, true},
    
    /* CRC-32 variants */
    {"CRC-32", 32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true, true},
    {"CRC-32/BZIP2", 32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, false, false},
    {"CRC-32C", 32, 0x1EDC6F41, 0xFFFFFFFF, 0xFFFFFFFF, true, true},
    {"CRC-32D", 32, 0xA833982B, 0xFFFFFFFF, 0xFFFFFFFF, true, true},
    {"CRC-32/JAMCRC", 32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, true, true},
    {"CRC-32/MPEG-2", 32, 0x04C11DB7, 0xFFFFFFFF, 0x00000000, false, false},
    {"CRC-32/POSIX", 32, 0x04C11DB7, 0x00000000, 0xFFFFFFFF, false, false},
    {"CRC-32Q", 32, 0x814141AB, 0x00000000, 0x00000000, false, false},
    {"CRC-32/XFER", 32, 0x000000AF, 0x00000000, 0x00000000, false, false},
    
    /* End marker */
    {NULL, 0, 0, 0, 0, false, false}
};

/*===========================================================================
 * Bit Reflection
 *===========================================================================*/

static uint64_t reflect_bits(uint64_t value, int width)
{
    uint64_t result = 0;
    
    for (int i = 0; i < width; i++) {
        if (value & ((uint64_t)1 << i)) {
            result |= (uint64_t)1 << (width - 1 - i);
        }
    }
    
    return result;
}

/*===========================================================================
 * CRC Calculation (Bitwise)
 *===========================================================================*/

uint64_t uft_crc_calc_bits(const uft_crc_def_t *def, 
                           const uint8_t *data, size_t length)
{
    if (!def || !data) return 0;
    
    uint64_t crc = def->init;
    uint64_t topbit = (uint64_t)1 << (def->width - 1);
    uint64_t mask = ((uint64_t)1 << def->width) - 1;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        
        if (def->ref_in) {
            byte = (uint8_t)reflect_bits(byte, 8);
        }
        
        crc ^= ((uint64_t)byte << (def->width - 8));
        
        for (int bit = 0; bit < 8; bit++) {
            if (crc & topbit) {
                crc = (crc << 1) ^ def->poly;
            } else {
                crc <<= 1;
            }
        }
    }
    
    if (def->ref_out) {
        crc = reflect_bits(crc, def->width);
    }
    
    return (crc ^ def->xor_out) & mask;
}

/*===========================================================================
 * Table Generation
 *===========================================================================*/

void uft_crc_generate_table(const uft_crc_def_t *def, uint64_t *table)
{
    if (!def || !table) return;
    
    uint64_t topbit = (uint64_t)1 << (def->width - 1);
    uint64_t mask = ((uint64_t)1 << def->width) - 1;
    
    for (int i = 0; i < 256; i++) {
        uint64_t crc;
        
        if (def->ref_in) {
            crc = reflect_bits(i, 8);
            
            for (int bit = 0; bit < 8; bit++) {
                if (crc & 1) {
                    crc = (crc >> 1) ^ reflect_bits(def->poly, def->width);
                } else {
                    crc >>= 1;
                }
            }
        } else {
            crc = (uint64_t)i << (def->width - 8);
            
            for (int bit = 0; bit < 8; bit++) {
                if (crc & topbit) {
                    crc = (crc << 1) ^ def->poly;
                } else {
                    crc <<= 1;
                }
            }
        }
        
        table[i] = crc & mask;
    }
}

/*===========================================================================
 * CRC Calculation (Table-based)
 *===========================================================================*/

uint64_t uft_crc_calc_table(const uft_crc_def_t *def, const uint64_t *table,
                            const uint8_t *data, size_t length)
{
    if (!def || !table || !data) return 0;
    
    uint64_t crc = def->init;
    uint64_t mask = ((uint64_t)1 << def->width) - 1;
    
    if (def->ref_in) {
        for (size_t i = 0; i < length; i++) {
            crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }
    } else {
        int shift = def->width - 8;
        
        for (size_t i = 0; i < length; i++) {
            crc = table[((crc >> shift) ^ data[i]) & 0xFF] ^ (crc << 8);
        }
    }
    
    if (def->ref_out != def->ref_in) {
        crc = reflect_bits(crc, def->width);
    }
    
    return (crc ^ def->xor_out) & mask;
}

/*===========================================================================
 * CRC Context
 *===========================================================================*/

int uft_crc_ctx_init(uft_crc_ctx_t *ctx, const uft_crc_def_t *def)
{
    if (!ctx || !def) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->def = *def;
    ctx->crc = def->init;
    
    /* Generate table */
    ctx->table = malloc(256 * sizeof(uint64_t));
    if (!ctx->table) return -1;
    
    uft_crc_generate_table(def, ctx->table);
    
    return 0;
}

void uft_crc_ctx_free(uft_crc_ctx_t *ctx)
{
    if (!ctx) return;
    
    free(ctx->table);
    memset(ctx, 0, sizeof(*ctx));
}

void uft_crc_ctx_update(uft_crc_ctx_t *ctx, const uint8_t *data, size_t length)
{
    if (!ctx || !data || !ctx->table) return;
    
    uint64_t crc = ctx->crc;
    uint64_t mask = ((uint64_t)1 << ctx->def.width) - 1;
    
    if (ctx->def.ref_in) {
        for (size_t i = 0; i < length; i++) {
            crc = ctx->table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
        }
    } else {
        int shift = ctx->def.width - 8;
        
        for (size_t i = 0; i < length; i++) {
            crc = ctx->table[((crc >> shift) ^ data[i]) & 0xFF] ^ (crc << 8);
        }
    }
    
    ctx->crc = crc & mask;
}

uint64_t uft_crc_ctx_final(uft_crc_ctx_t *ctx)
{
    if (!ctx) return 0;
    
    uint64_t crc = ctx->crc;
    uint64_t mask = ((uint64_t)1 << ctx->def.width) - 1;
    
    if (ctx->def.ref_out != ctx->def.ref_in) {
        crc = reflect_bits(crc, ctx->def.width);
    }
    
    return (crc ^ ctx->def.xor_out) & mask;
}

void uft_crc_ctx_reset(uft_crc_ctx_t *ctx)
{
    if (!ctx) return;
    ctx->crc = ctx->def.init;
}

/*===========================================================================
 * Algorithm Lookup
 *===========================================================================*/

const uft_crc_def_t *uft_crc_find_algorithm(const char *name)
{
    if (!name) return NULL;
    
    for (int i = 0; crc_algorithms[i].name != NULL; i++) {
        if (strcmp(crc_algorithms[i].name, name) == 0) {
            return &crc_algorithms[i];
        }
    }
    
    return NULL;
}

int uft_crc_list_algorithms(const uft_crc_def_t **list, size_t *count)
{
    if (!list || !count) return -1;
    
    *count = 0;
    
    for (int i = 0; crc_algorithms[i].name != NULL; i++) {
        list[*count] = &crc_algorithms[i];
        (*count)++;
    }
    
    return 0;
}

/*===========================================================================
 * Convenience Functions
 *===========================================================================*/

uint8_t uft_crc8(const uint8_t *data, size_t length)
{
    const uft_crc_def_t *def = uft_crc_find_algorithm("CRC-8");
    return (uint8_t)uft_crc_calc_bits(def, data, length);
}

uint16_t uft_crc16_ccitt(const uint8_t *data, size_t length)
{
    const uft_crc_def_t *def = uft_crc_find_algorithm("CRC-16/CCITT-FALSE");
    return (uint16_t)uft_crc_calc_bits(def, data, length);
}

uint16_t uft_crc16_xmodem(const uint8_t *data, size_t length)
{
    const uft_crc_def_t *def = uft_crc_find_algorithm("CRC-16/XMODEM");
    return (uint16_t)uft_crc_calc_bits(def, data, length);
}

uint16_t uft_crc16_modbus(const uint8_t *data, size_t length)
{
    const uft_crc_def_t *def = uft_crc_find_algorithm("CRC-16/MODBUS");
    return (uint16_t)uft_crc_calc_bits(def, data, length);
}

uint32_t uft_crc32(const uint8_t *data, size_t length)
{
    const uft_crc_def_t *def = uft_crc_find_algorithm("CRC-32");
    return (uint32_t)uft_crc_calc_bits(def, data, length);
}

uint32_t uft_crc32c(const uint8_t *data, size_t length)
{
    const uft_crc_def_t *def = uft_crc_find_algorithm("CRC-32C");
    return (uint32_t)uft_crc_calc_bits(def, data, length);
}

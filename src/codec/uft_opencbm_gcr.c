/**
 * @file uft_opencbm_gcr.c
 * @brief GCR 4-bit to 5-bit encoding/decoding from OpenCBM
 * @version 1.0.0
 * @date 2026-01-06
 *
 * Source: OpenCBM (Wolfgang Moser, http://d81.de)
 * License: GPL v2
 *
 * This module provides Commodore GCR (Group Code Recording) conversion:
 * - 4-to-5 bit encoding for writing to disk
 * - 5-to-4 bit decoding for reading from disk
 * - Support for partial conversions and in-place operations
 *
 * Integration: R50 - OpenCBM extraction
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "uft_opencbm_gcr.h"

/*============================================================================
 * GCR Lookup Tables
 *============================================================================*/

/**
 * @brief GCR decode table (5-bit GCR to 4-bit value)
 * 255 = invalid GCR code
 */
static const uint8_t gcr_decode_table[32] = {
    255, 255, 255, 255, 255, 255, 255, 255,  /* 0x00-0x07 */
    255,   8,   0,   1, 255,  12,   4,   5,  /* 0x08-0x0F */
    255, 255,   2,   3, 255,  15,   6,   7,  /* 0x10-0x17 */
    255,   9,  10,  11, 255,  13,  14, 255   /* 0x18-0x1F */
};

/**
 * @brief GCR encode table (4-bit value to 5-bit GCR)
 */
static const uint8_t gcr_encode_table[16] = {
    10, 11, 18, 19, 14, 15, 22, 23,  /* 0x0-0x7 */
     9, 25, 26, 27, 13, 29, 30, 21   /* 0x8-0xF */
};

/*============================================================================
 * GCR Decoding (5 bytes -> 4 bytes)
 *============================================================================*/

int uft_gcr_5_to_4_decode(const uint8_t* source, uint8_t* dest,
                           size_t source_len, size_t dest_len)
{
    if (!source || !dest || source_len == 0) {
        return -1;
    }
    
    int result = 0;
    
    /* At least 24 bits for shifting into bits 16...20 */
    uint32_t tdest = *source;
    tdest <<= 13;
    
    for (int i = 5; (i < 13) && (dest_len > 0); i += 2, dest++, dest_len--) {
        source++;
        source_len--;
        
        if (source_len > 0) {
            tdest |= ((uint32_t)(*source)) << i;
        } else {
            /* No more input - check if remaining bits are valid */
            if (0 == ((tdest >> 16) & 0x3ff)) {
                break;
            }
        }
        
        /* Decode upper nybble */
        uint8_t nybble = gcr_decode_table[(tdest >> 16) & 0x1f];
        if (nybble > 15) {
            result |= 2;  /* Invalid GCR detected */
        }
        *dest = nybble << 4;
        tdest <<= 5;
        
        /* Decode lower nybble */
        nybble = gcr_decode_table[(tdest >> 16) & 0x1f];
        if (nybble > 15) {
            result |= 1;  /* Invalid GCR detected */
        }
        *dest |= (nybble & 0x0f);
        tdest <<= 5;
        
        result <<= 2;  /* Shift error flags */
    }
    
    return result;
}

/*============================================================================
 * GCR Encoding (4 bytes -> 5 bytes)
 *============================================================================*/

int uft_gcr_4_to_5_encode(const uint8_t* source, uint8_t* dest,
                           size_t source_len, size_t dest_len)
{
    if (!source || !dest || source_len == 0) {
        return -1;
    }
    
    uint32_t tdest = 0;
    
    for (int i = 2; 
         (i < 10) && (source_len > 0) && (dest_len > 0);
         i += 2, source++, source_len--, dest++, dest_len--) {
        
        /* Make room for upper nybble */
        tdest <<= 5;
        tdest |= gcr_encode_table[(*source) >> 4];
        
        /* Make room for lower nybble */
        tdest <<= 5;
        tdest |= gcr_encode_table[(*source) & 0x0f];
        
        *dest = (uint8_t)(tdest >> i);
    }
    
    /* Write remaining bits if buffer space available */
    if (dest_len > 0) {
        *dest = (uint8_t)(tdest << ((10 - 10) & 0x07));
    }
    
    return 0;
}

/*============================================================================
 * High-Level Block Operations
 *============================================================================*/

int uft_gcr_decode_block(const uint8_t* gcr_data, uint8_t* decoded,
                          uint8_t* header_out)
{
    if (!gcr_data || !decoded) {
        return UFT_GCR_ERR_NULLPTR;
    }
    
    uint8_t header[4];
    uint8_t checksum = 0;
    const uint8_t* gcr = gcr_data;
    uint8_t* out = decoded;
    
    /* Decode header (first 5 GCR bytes -> 4 bytes) */
    int err = uft_gcr_5_to_4_decode(gcr, header, 5, 4);
    if (err < 0) {
        return UFT_GCR_ERR_DECODE;
    }
    gcr += 5;
    
    /* Check data block marker */
    if (header[0] != 0x07) {
        return UFT_GCR_ERR_HEADER;
    }
    
    /* Copy header bytes 1-3 to output, compute checksum */
    for (int j = 1; j < 4; j++) {
        *out++ = header[j];
        checksum ^= header[j];
    }
    
    /* Output header if requested */
    if (header_out) {
        for (int j = 0; j < 4; j++) {
            header_out[j] = header[j];
        }
    }
    
    /* Decode main block (63 groups of 5 GCR -> 4 bytes) */
    for (int i = 1; i < UFT_GCR_BLOCK_SIZE / 4; i++) {
        err = uft_gcr_5_to_4_decode(gcr, out, 5, 4);
        if (err < 0) {
            return UFT_GCR_ERR_DECODE;
        }
        gcr += 5;
        
        for (int j = 0; j < 4; j++) {
            checksum ^= *out++;
        }
    }
    
    /* Decode trailer (checksum byte) */
    uint8_t trailer[4];
    err = uft_gcr_5_to_4_decode(gcr, trailer, 5, 4);
    if (err < 0) {
        return UFT_GCR_ERR_DECODE;
    }
    
    /* Copy last data byte */
    *out = trailer[0];
    checksum ^= trailer[0];
    
    /* Verify checksum */
    if (trailer[1] != checksum) {
        return UFT_GCR_ERR_CHECKSUM;
    }
    
    return UFT_GCR_OK;
}

int uft_gcr_encode_block(const uint8_t* data, uint8_t* gcr_out)
{
    if (!data || !gcr_out) {
        return UFT_GCR_ERR_NULLPTR;
    }
    
    uint8_t header[4] = { 0x07, 0, 0, 0 };
    uint8_t checksum = 0;
    const uint8_t* in = data;
    uint8_t* gcr = gcr_out;
    
    /* Fill header with first 3 data bytes */
    for (int j = 1; j < 4; j++) {
        header[j] = *in++;
        checksum ^= header[j];
    }
    
    /* Encode header */
    uft_gcr_4_to_5_encode(header, gcr, 4, 5);
    gcr += 5;
    
    /* Encode main block */
    for (int i = 1; i < UFT_GCR_BLOCK_SIZE / 4; i++) {
        uft_gcr_4_to_5_encode(in, gcr, 4, 5);
        gcr += 5;
        
        for (int j = 0; j < 4; j++) {
            checksum ^= *in++;
        }
    }
    
    /* Prepare trailer with last byte and checksum */
    uint8_t trailer[4] = { *in, 0, 0, 0 };
    checksum ^= trailer[0];
    trailer[1] = checksum;
    
    /* Encode trailer */
    uft_gcr_4_to_5_encode(trailer, gcr, 4, 5);
    
    return UFT_GCR_OK;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

bool uft_gcr_is_valid_code(uint8_t gcr_value)
{
    return (gcr_value < 32) && (gcr_decode_table[gcr_value] != 255);
}

uint8_t uft_gcr_encode_nybble(uint8_t nybble)
{
    return gcr_encode_table[nybble & 0x0f];
}

uint8_t uft_gcr_decode_nybble(uint8_t gcr_value, bool* valid)
{
    uint8_t result = gcr_decode_table[gcr_value & 0x1f];
    if (valid) {
        *valid = (result != 255);
    }
    return (result != 255) ? result : 0;
}

/*============================================================================
 * Sector Header Operations
 *============================================================================*/

int uft_gcr_decode_sector_header(const uint8_t* gcr_header, 
                                  uft_gcr_sector_header_t* header)
{
    if (!gcr_header || !header) {
        return UFT_GCR_ERR_NULLPTR;
    }
    
    uint8_t raw[4];
    int err = uft_gcr_5_to_4_decode(gcr_header, raw, 5, 4);
    if (err < 0) {
        return UFT_GCR_ERR_DECODE;
    }
    
    /* Check header marker (0x08 for sector header) */
    if (raw[0] != 0x08) {
        return UFT_GCR_ERR_HEADER;
    }
    
    /* Decode remaining 5 GCR bytes */
    uint8_t raw2[4];
    err = uft_gcr_5_to_4_decode(gcr_header + 5, raw2, 5, 4);
    if (err < 0) {
        return UFT_GCR_ERR_DECODE;
    }
    
    /* Parse header fields */
    header->header_checksum = raw[1];
    header->sector = raw[2];
    header->track = raw[3];
    header->id2 = raw2[0];
    header->id1 = raw2[1];
    
    /* Verify checksum */
    uint8_t calc_checksum = header->sector ^ header->track ^ header->id2 ^ header->id1;
    header->valid = (calc_checksum == header->header_checksum);
    
    return header->valid ? UFT_GCR_OK : UFT_GCR_ERR_CHECKSUM;
}

int uft_gcr_encode_sector_header(const uft_gcr_sector_header_t* header,
                                  uint8_t* gcr_out)
{
    if (!header || !gcr_out) {
        return UFT_GCR_ERR_NULLPTR;
    }
    
    /* Calculate checksum */
    uint8_t checksum = header->sector ^ header->track ^ header->id2 ^ header->id1;
    
    /* Build raw header bytes */
    uint8_t raw1[4] = { 0x08, checksum, header->sector, header->track };
    uint8_t raw2[4] = { header->id2, header->id1, 0x0f, 0x0f };
    
    /* Encode both halves */
    uft_gcr_4_to_5_encode(raw1, gcr_out, 4, 5);
    uft_gcr_4_to_5_encode(raw2, gcr_out + 5, 4, 5);
    
    return UFT_GCR_OK;
}

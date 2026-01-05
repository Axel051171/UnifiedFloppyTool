/**
 * @file uft_gcr.c
 * @brief GCR encoding/decoding implementation
 * 
 * Source: Extracted and converted from disk2disk and mac2dos projects
 * Original: Copyright (c) 1987-1989 Central Coast Software
 */

#include "encoding/uft_gcr.h"
#include <string.h>

/* ============================================================================
 * C64 GCR Tables (from disk2disk DISK.ASM)
 * 
 * Binary nibble to GCR conversion table (shifted left 3 for convenience)
 * ============================================================================ */

const uint8_t uft_c64_gcr_encode[16] = {
    0x50,  /* 0 = 01010 */
    0x58,  /* 1 = 01011 */
    0x90,  /* 2 = 10010 */
    0x98,  /* 3 = 10011 */
    0x70,  /* 4 = 01110 */
    0x78,  /* 5 = 01111 */
    0xB0,  /* 6 = 10110 */
    0xB8,  /* 7 = 10111 */
    0x48,  /* 8 = 01001 */
    0xC8,  /* 9 = 11001 */
    0xD0,  /* A = 11010 */
    0xD8,  /* B = 11011 */
    0x68,  /* C = 01101 */
    0xE8,  /* D = 11101 */
    0xF0,  /* E = 11110 */
    0xA8,  /* F = 10101 */
};

/* GCR to binary conversion table */
const int8_t uft_c64_gcr_decode[32] = {
    -1,    /* 00 */
    -1,    /* 01 */
    -1,    /* 02 */
    -1,    /* 03 */
    -1,    /* 04 */
    -1,    /* 05 */
    -1,    /* 06 */
    -1,    /* 07 */
    -1,    /* 08 */
    0x8,   /* 09 = 01001 */
    0x0,   /* 0A = 01010 */
    0x1,   /* 0B = 01011 */
    -1,    /* 0C */
    0xC,   /* 0D = 01101 */
    0x4,   /* 0E = 01110 */
    0x5,   /* 0F = 01111 */
    -1,    /* 10 */
    -1,    /* 11 */
    0x2,   /* 12 = 10010 */
    0x3,   /* 13 = 10011 */
    -1,    /* 14 */
    0xF,   /* 15 = 10101 */
    0x6,   /* 16 = 10110 */
    0x7,   /* 17 = 10111 */
    -1,    /* 18 */
    0x9,   /* 19 = 11001 */
    0xA,   /* 1A = 11010 */
    0xB,   /* 1B = 11011 */
    -1,    /* 1C */
    0xD,   /* 1D = 11101 */
    0xE,   /* 1E = 11110 */
    -1,    /* 1F */
};

/* ============================================================================
 * Mac GCR Tables (from mac2dos mfmac.asm)
 * 
 * Binary to 6+2 GCR conversion table
 * ============================================================================ */

const uint8_t uft_mac_gcr_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,  /* 00-07 */
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,  /* 08-0F */
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,  /* 10-17 */
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,  /* 18-1F */
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,  /* 20-27 */
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,  /* 28-2F */
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,  /* 30-37 */
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,  /* 38-3F */
};

/* GCR to binary conversion table (offset 0x96) */
static const int8_t mac_gcr_decode_raw[] = {
     0,  1, -1, -1,  2,  3, -1,  4,  5,  6, -1, -1, -1, -1, -1, -1,  /* 96-A5 */
     7,  8, -1, -1, -1,  9, 10, 11, 12, 13, -1, -1, 14, 15, 16, 17,  /* A6-B5 */
    18, 19, -1, 20, 21, 22, 23, 24, 25, 26, -1, -1, -1, -1, -1, -1,  /* B6-C5 */
    -1, -1, -1, -1, -1, 27, -1, 28, 29, 30, -1, -1, -1, 31, -1, -1,  /* C6-D5 */
    32, 33, -1, 34, 35, 36, 37, 38, 39, 40, -1, -1, -1, -1, -1, 41,  /* D6-E5 */
    42, 43, -1, 44, 45, 46, 47, 48, 49, 50, -1, -1, 51, 52, 53, 54,  /* E6-F5 */
    55, 56, -1, 57, 58, 59, 60, 61, 62, 63,                          /* F6-FF */
};

/* Full 256-byte decode table for fast lookup */
const int8_t uft_mac_gcr_decode[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 20-2F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 30-3F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 40-4F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 50-5F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 60-6F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 70-7F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
    -1,-1,-1,-1,-1,-1, 0, 1,-1,-1, 2, 3,-1, 4, 5, 6,  /* 90-9F */
    -1,-1,-1,-1,-1,-1, 7, 8,-1,-1,-1, 9,10,11,12,13,  /* A0-AF */
    -1,-1,14,15,16,17,18,19,-1,20,21,22,23,24,25,26,  /* B0-BF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,27,-1,28,29,30,  /* C0-CF */
    -1,-1,-1,31,-1,-1,32,33,-1,34,35,36,37,38,39,40,  /* D0-DF */
    -1,-1,-1,-1,-1,41,42,43,-1,44,45,46,47,48,49,50,  /* E0-EF */
    -1,-1,51,52,53,54,55,56,-1,57,58,59,60,61,62,63,  /* F0-FF */
};

/* ============================================================================
 * C64 Track Parameters (from disk2disk DISK.ASM TPARAM_TBL)
 * ============================================================================ */

const uft_c64_track_params_t uft_c64_track_table[4] = {
    { 21, 7692, 1, 3850 },  /* Tracks 1-17:  21 sectors, zone 3 */
    { 19, 7142, 2, 3400 },  /* Tracks 18-24: 19 sectors, zone 2 */
    { 18, 6768, 2, 3200 },  /* Tracks 25-30: 18 sectors, zone 1 */
    { 17, 6392, 2, 3053 },  /* Tracks 31-35: 17 sectors, zone 0 */
};

/* ============================================================================
 * C64 GCR Encoding/Decoding Functions
 * ============================================================================ */

/**
 * Internal: Store a byte as two GCR nibbles
 */
static void c64_gcr_put_byte(uint8_t byte, uint8_t **out, uint8_t *accum, int *bits) {
    /* High nibble first */
    uint8_t hi_gcr = uft_c64_gcr_encode[byte >> 4] >> 3;
    uint8_t lo_gcr = uft_c64_gcr_encode[byte & 0x0F] >> 3;
    
    /* Pack 10 bits (two 5-bit GCR codes) into byte stream */
    for (int i = 4; i >= 0; i--) {
        *accum = (uint8_t)((*accum << 1) | ((hi_gcr >> i) & 1));
        (*bits)++;
        if (*bits == 8) {
            *(*out)++ = *accum;
            *bits = 0;
        }
    }
    for (int i = 4; i >= 0; i--) {
        *accum = (uint8_t)((*accum << 1) | ((lo_gcr >> i) & 1));
        (*bits)++;
        if (*bits == 8) {
            *(*out)++ = *accum;
            *bits = 0;
        }
    }
}

/**
 * Internal: Get a byte from GCR nibbles
 */
static uft_gcr_error_t c64_gcr_get_byte(const uint8_t **in, size_t *remaining,
                                         uint8_t *accum, int *bits, uint8_t *byte_out) {
    uint8_t hi_gcr = 0, lo_gcr = 0;
    
    /* Extract high nibble (5 bits) */
    for (int i = 0; i < 5; i++) {
        if (*bits == 0) {
            if (*remaining == 0) return UFT_GCR_ERR_BUFFER_TOO_SMALL;
            *accum = *(*in)++;
            (*remaining)--;
            *bits = 8;
        }
        hi_gcr = (uint8_t)((hi_gcr << 1) | ((*accum >> 7) & 1));
        *accum <<= 1;
        (*bits)--;
    }
    
    /* Extract low nibble (5 bits) */
    for (int i = 0; i < 5; i++) {
        if (*bits == 0) {
            if (*remaining == 0) return UFT_GCR_ERR_BUFFER_TOO_SMALL;
            *accum = *(*in)++;
            (*remaining)--;
            *bits = 8;
        }
        lo_gcr = (uint8_t)((lo_gcr << 1) | ((*accum >> 7) & 1));
        *accum <<= 1;
        (*bits)--;
    }
    
    /* Decode nibbles */
    int8_t hi = uft_c64_gcr_decode[hi_gcr & 0x1F];
    int8_t lo = uft_c64_gcr_decode[lo_gcr & 0x1F];
    
    if (hi < 0 || lo < 0) {
        return UFT_GCR_ERR_INVALID_CODE;
    }
    
    *byte_out = (uint8_t)((hi << 4) | lo);
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_c64_gcr_encode_sector(
    const uint8_t *data,
    uint8_t *gcr_out,
    uint8_t block_id
) {
    if (!data || !gcr_out) return UFT_GCR_ERR_INVALID_PARAM;
    
    uint8_t *out = gcr_out;
    uint8_t accum = 0;
    int bits = 0;
    uint8_t checksum = 0;
    
    /* Write sync bytes */
    for (int i = 0; i < UFT_C64_SYNC_COUNT; i++) {
        *out++ = UFT_C64_SYNC_BYTE;
    }
    
    /* Encode block ID */
    c64_gcr_put_byte(block_id, &out, &accum, &bits);
    
    /* Encode 256 data bytes with running checksum */
    for (int i = 0; i < 256; i++) {
        checksum ^= data[i];
        c64_gcr_put_byte(data[i], &out, &accum, &bits);
    }
    
    /* Encode checksum */
    c64_gcr_put_byte(checksum, &out, &accum, &bits);
    
    /* Encode two null bytes */
    c64_gcr_put_byte(0x00, &out, &accum, &bits);
    c64_gcr_put_byte(0x00, &out, &accum, &bits);
    
    /* Flush remaining bits */
    if (bits > 0) {
        *out++ = (uint8_t)(accum << (8 - bits));
    }
    
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_c64_gcr_decode_sector(
    const uint8_t *gcr_in,
    uint8_t *data_out,
    uint8_t *block_id
) {
    if (!gcr_in || !data_out) return UFT_GCR_ERR_INVALID_PARAM;
    
    const uint8_t *in = gcr_in;
    size_t remaining = UFT_C64_GCR_SECTOR_SIZE;
    uint8_t accum = 0;
    int bits = 0;
    uint8_t checksum = 0;
    uint8_t byte;
    uft_gcr_error_t err;
    
    /* Decode block ID */
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &byte);
    if (err != UFT_GCR_OK) return err;
    if (block_id) *block_id = byte;
    
    /* Decode 256 data bytes */
    for (int i = 0; i < 256; i++) {
        err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &byte);
        if (err != UFT_GCR_OK) return err;
        data_out[i] = byte;
        checksum ^= byte;
    }
    
    /* Decode and verify checksum */
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &byte);
    if (err != UFT_GCR_OK) return err;
    
    if (byte != checksum) {
        return UFT_GCR_ERR_CHECKSUM;
    }
    
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_c64_gcr_encode_header(
    uint8_t track,
    uint8_t sector,
    const uint8_t disk_id[2],
    uint8_t *gcr_out
) {
    if (!disk_id || !gcr_out) return UFT_GCR_ERR_INVALID_PARAM;
    
    uint8_t *out = gcr_out;
    uint8_t accum = 0;
    int bits = 0;
    uint8_t checksum;
    
    /* Write sync bytes */
    for (int i = 0; i < UFT_C64_SYNC_COUNT; i++) {
        *out++ = UFT_C64_SYNC_BYTE;
    }
    
    /* Header consists of: ID, checksum, sector, track, disk_id[1], disk_id[0], fill, fill */
    checksum = (uint8_t)(sector ^ track ^ disk_id[1] ^ disk_id[0]);
    
    c64_gcr_put_byte(UFT_C64_BLOCK_HEADER, &out, &accum, &bits);
    c64_gcr_put_byte(checksum, &out, &accum, &bits);
    c64_gcr_put_byte(sector, &out, &accum, &bits);
    c64_gcr_put_byte(track, &out, &accum, &bits);
    c64_gcr_put_byte(disk_id[1], &out, &accum, &bits);
    c64_gcr_put_byte(disk_id[0], &out, &accum, &bits);
    c64_gcr_put_byte(0x0F, &out, &accum, &bits);  /* Fill byte */
    c64_gcr_put_byte(0x0F, &out, &accum, &bits);  /* Fill byte */
    
    /* Flush remaining bits */
    if (bits > 0) {
        *out++ = (uint8_t)(accum << (8 - bits));
    }
    
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_c64_gcr_decode_header(
    const uint8_t *gcr_in,
    uint8_t *track,
    uint8_t *sector,
    uint8_t disk_id[2]
) {
    if (!gcr_in) return UFT_GCR_ERR_INVALID_PARAM;
    
    const uint8_t *in = gcr_in;
    size_t remaining = 20;  /* Enough for header */
    uint8_t accum = 0;
    int bits = 0;
    uint8_t byte, hdr_id, checksum, sec, trk, id1, id0;
    uft_gcr_error_t err;
    
    /* Decode header fields */
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &hdr_id);
    if (err != UFT_GCR_OK) return err;
    if (hdr_id != UFT_C64_BLOCK_HEADER) return UFT_GCR_ERR_INVALID_CODE;
    
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &checksum);
    if (err != UFT_GCR_OK) return err;
    
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &sec);
    if (err != UFT_GCR_OK) return err;
    
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &trk);
    if (err != UFT_GCR_OK) return err;
    
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &id1);
    if (err != UFT_GCR_OK) return err;
    
    err = c64_gcr_get_byte(&in, &remaining, &accum, &bits, &id0);
    if (err != UFT_GCR_OK) return err;
    
    /* Verify checksum */
    uint8_t calc_checksum = (uint8_t)(sec ^ trk ^ id1 ^ id0);
    if (calc_checksum != checksum) {
        return UFT_GCR_ERR_CHECKSUM;
    }
    
    if (track) *track = trk;
    if (sector) *sector = sec;
    if (disk_id) {
        disk_id[0] = id0;
        disk_id[1] = id1;
    }
    
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_c64_get_track_params(uint8_t track, uft_c64_track_params_t *params) {
    if (!params || track == 0 || track > 40) return UFT_GCR_ERR_INVALID_PARAM;
    
    int zone;
    if (track <= 17) {
        zone = 0;  /* Tracks 1-17 */
    } else if (track <= 24) {
        zone = 1;  /* Tracks 18-24 */
    } else if (track <= 30) {
        zone = 2;  /* Tracks 25-30 */
    } else {
        zone = 3;  /* Tracks 31-35/40 */
    }
    
    *params = uft_c64_track_table[zone];
    return UFT_GCR_OK;
}

/* ============================================================================
 * Mac GCR Encoding/Decoding Functions
 * ============================================================================ */

uft_gcr_error_t uft_mac_gcr_encode_sector(
    const uint8_t *data,
    const uint8_t *tags,
    uint8_t *gcr_out,
    uint8_t sector
) {
    if (!data || !gcr_out) return UFT_GCR_ERR_INVALID_PARAM;
    
    uint8_t local_tags[12] = {0};
    if (tags) {
        memcpy(local_tags, tags, 12);
    }
    local_tags[0] = sector;  /* Sector number in tag byte 0 */
    
    uint8_t *out = gcr_out;
    uint8_t crc[3] = {0, 0, 0};
    
    /* Encode tag data (12 bytes -> 16 GCR bytes using 6+2) */
    const uint8_t *tag_ptr = local_tags;
    for (int i = 0; i < 4; i++) {
        uint8_t a = *tag_ptr++;
        uint8_t b = *tag_ptr++;
        uint8_t c = *tag_ptr++;
        
        /* Update checksums */
        uint8_t carry = (uint8_t)((crc[2] >> 7) & 1);
        crc[2] = (uint8_t)((crc[2] << 1) | carry);
        a ^= crc[2];
        crc[0] = (uint8_t)(crc[0] + a + carry);
        
        carry = (uint8_t)((crc[0] >> 8) & 1);
        b ^= crc[0];
        crc[1] = (uint8_t)(crc[1] + b + carry);
        
        carry = (uint8_t)((crc[1] >> 8) & 1);
        c ^= crc[1];
        crc[2] = (uint8_t)(crc[2] + c + carry);
        
        /* Encode 3 bytes into 4 GCR bytes (6+2 encoding) */
        uint8_t top_bits = (uint8_t)(((a >> 6) & 0x03) | ((b >> 4) & 0x0C) | ((c >> 2) & 0x30));
        *out++ = uft_mac_gcr_encode[top_bits];
        *out++ = uft_mac_gcr_encode[a & 0x3F];
        *out++ = uft_mac_gcr_encode[b & 0x3F];
        *out++ = uft_mac_gcr_encode[c & 0x3F];
    }
    
    /* Encode data (512 bytes -> 683 GCR bytes) */
    for (int i = 0; i < 170; i++) {
        uint8_t a = data[i * 3];
        uint8_t b = data[i * 3 + 1];
        uint8_t c = (i * 3 + 2 < 512) ? data[i * 3 + 2] : 0;
        
        /* Update checksums */
        uint8_t carry = (uint8_t)((crc[2] >> 7) & 1);
        crc[2] = (uint8_t)((crc[2] << 1) | carry);
        a ^= crc[2];
        crc[0] = (uint8_t)(crc[0] + a + carry);
        
        carry = (uint8_t)((crc[0] >> 8) & 1);
        b ^= crc[0];
        crc[1] = (uint8_t)(crc[1] + b + carry);
        
        if (i * 3 + 2 < 512) {
            carry = (uint8_t)((crc[1] >> 8) & 1);
            c ^= crc[1];
            crc[2] = (uint8_t)(crc[2] + c + carry);
        }
        
        /* Encode */
        uint8_t top_bits = (uint8_t)(((a >> 6) & 0x03) | ((b >> 4) & 0x0C) | ((c >> 2) & 0x30));
        *out++ = uft_mac_gcr_encode[top_bits];
        *out++ = uft_mac_gcr_encode[a & 0x3F];
        *out++ = uft_mac_gcr_encode[b & 0x3F];
        if (i * 3 + 2 < 512) {
            *out++ = uft_mac_gcr_encode[c & 0x3F];
        }
    }
    
    /* Encode CRC bytes */
    uint8_t crc_top = (uint8_t)(((crc[0] >> 6) & 0x03) | ((crc[1] >> 4) & 0x0C) | ((crc[2] >> 2) & 0x30));
    *out++ = uft_mac_gcr_encode[crc_top];
    *out++ = uft_mac_gcr_encode[crc[0] & 0x3F];
    *out++ = uft_mac_gcr_encode[crc[1] & 0x3F];
    *out++ = uft_mac_gcr_encode[crc[2] & 0x3F];
    
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_mac_gcr_decode_sector(
    const uint8_t *gcr_in,
    uint8_t *data_out,
    uint8_t *tags_out
) {
    if (!gcr_in || !data_out) return UFT_GCR_ERR_INVALID_PARAM;
    
    const uint8_t *in = gcr_in;
    uint8_t crc[3] = {0, 0, 0};
    uint8_t local_tags[12];
    
    /* Decode tag data (16 GCR bytes -> 12 bytes) */
    uint8_t *tag_ptr = local_tags;
    for (int i = 0; i < 4; i++) {
        int8_t top = uft_mac_gcr_decode[*in++];
        int8_t d0 = uft_mac_gcr_decode[*in++];
        int8_t d1 = uft_mac_gcr_decode[*in++];
        int8_t d2 = uft_mac_gcr_decode[*in++];
        
        if (top < 0 || d0 < 0 || d1 < 0 || d2 < 0) {
            return UFT_GCR_ERR_INVALID_CODE;
        }
        
        uint8_t a = (uint8_t)(((top << 6) & 0xC0) | d0);
        uint8_t b = (uint8_t)(((top << 4) & 0xC0) | d1);
        uint8_t c = (uint8_t)(((top << 2) & 0xC0) | d2);
        
        /* Update checksums and decode */
        uint8_t carry = (uint8_t)((crc[2] >> 7) & 1);
        crc[2] = (uint8_t)((crc[2] << 1) | carry);
        *tag_ptr++ = a ^ crc[2];
        crc[0] = (uint8_t)(crc[0] + a + carry);
        
        carry = (uint8_t)((crc[0] >> 8) & 1);
        *tag_ptr++ = b ^ crc[0];
        crc[1] = (uint8_t)(crc[1] + b + carry);
        
        carry = (uint8_t)((crc[1] >> 8) & 1);
        *tag_ptr++ = c ^ crc[1];
        crc[2] = (uint8_t)(crc[2] + c + carry);
    }
    
    if (tags_out) {
        memcpy(tags_out, local_tags, 12);
    }
    
    /* Decode data (683 GCR bytes -> 512 bytes) */
    for (int i = 0; i < 170; i++) {
        int8_t top = uft_mac_gcr_decode[*in++];
        int8_t d0 = uft_mac_gcr_decode[*in++];
        int8_t d1 = uft_mac_gcr_decode[*in++];
        
        if (top < 0 || d0 < 0 || d1 < 0) {
            return UFT_GCR_ERR_INVALID_CODE;
        }
        
        uint8_t a = (uint8_t)(((top << 6) & 0xC0) | d0);
        uint8_t b = (uint8_t)(((top << 4) & 0xC0) | d1);
        
        /* Update checksums and store */
        uint8_t carry = (uint8_t)((crc[2] >> 7) & 1);
        crc[2] = (uint8_t)((crc[2] << 1) | carry);
        data_out[i * 3] = a ^ crc[2];
        crc[0] = (uint8_t)(crc[0] + a + carry);
        
        carry = (uint8_t)((crc[0] >> 8) & 1);
        data_out[i * 3 + 1] = b ^ crc[0];
        crc[1] = (uint8_t)(crc[1] + b + carry);
        
        if (i * 3 + 2 < 512) {
            int8_t d2 = uft_mac_gcr_decode[*in++];
            if (d2 < 0) return UFT_GCR_ERR_INVALID_CODE;
            
            uint8_t c = (uint8_t)(((top << 2) & 0xC0) | d2);
            carry = (uint8_t)((crc[1] >> 8) & 1);
            data_out[i * 3 + 2] = c ^ crc[1];
            crc[2] = (uint8_t)(crc[2] + c + carry);
        }
    }
    
    /* Verify CRC */
    int8_t crc_top = uft_mac_gcr_decode[*in++];
    int8_t crc0 = uft_mac_gcr_decode[*in++];
    int8_t crc1 = uft_mac_gcr_decode[*in++];
    int8_t crc2 = uft_mac_gcr_decode[*in++];
    
    if (crc_top < 0 || crc0 < 0 || crc1 < 0 || crc2 < 0) {
        return UFT_GCR_ERR_INVALID_CODE;
    }
    
    uint8_t expected_crc0 = (uint8_t)(((crc_top << 6) & 0xC0) | crc0);
    uint8_t expected_crc1 = (uint8_t)(((crc_top << 4) & 0xC0) | crc1);
    uint8_t expected_crc2 = (uint8_t)(((crc_top << 2) & 0xC0) | crc2);
    
    if (expected_crc0 != crc[0] || expected_crc1 != crc[1] || expected_crc2 != crc[2]) {
        return UFT_GCR_ERR_CHECKSUM;
    }
    
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_mac_gcr_encode_header(
    uint8_t track,
    uint8_t sector,
    uint8_t side,
    uint8_t format,
    uint8_t *gcr_out
) {
    if (!gcr_out) return UFT_GCR_ERR_INVALID_PARAM;
    
    uint8_t *out = gcr_out;
    
    /* Sync pattern: D5 AA 96 */
    *out++ = UFT_MAC_SYNC_PATTERN_1;
    *out++ = UFT_MAC_SYNC_PATTERN_2;
    *out++ = UFT_MAC_SYNC_HDR;
    
    /* Encode track (may need adjustment for tracks >= 64) */
    uint8_t enc_track = track;
    uint8_t overflow = 0;
    if (track >= 64) {
        enc_track = (uint8_t)(track - 64);
        overflow = 1;
    }
    
    /* Side/overflow byte */
    uint8_t side_byte = (uint8_t)((side ? 0x20 : 0) | overflow);
    
    /* Header checksum */
    uint8_t checksum = (uint8_t)(enc_track ^ sector ^ side_byte ^ format);
    
    /* Encode header fields */
    *out++ = uft_mac_gcr_encode[enc_track & 0x3F];
    *out++ = uft_mac_gcr_encode[sector & 0x3F];
    *out++ = uft_mac_gcr_encode[side_byte & 0x3F];
    *out++ = uft_mac_gcr_encode[format & 0x3F];
    *out++ = uft_mac_gcr_encode[checksum & 0x3F];
    
    /* Trailer: DE AA */
    *out++ = 0xDE;
    *out++ = 0xAA;
    
    return UFT_GCR_OK;
}

uft_gcr_error_t uft_mac_gcr_decode_header(
    const uint8_t *gcr_in,
    uint8_t *track,
    uint8_t *sector,
    uint8_t *side,
    uint8_t *format
) {
    if (!gcr_in) return UFT_GCR_ERR_INVALID_PARAM;
    
    const uint8_t *in = gcr_in;
    
    /* Skip sync pattern D5 AA 96 */
    if (*in++ != UFT_MAC_SYNC_PATTERN_1) return UFT_GCR_ERR_SYNC_NOT_FOUND;
    if (*in++ != UFT_MAC_SYNC_PATTERN_2) return UFT_GCR_ERR_SYNC_NOT_FOUND;
    if (*in++ != UFT_MAC_SYNC_HDR) return UFT_GCR_ERR_SYNC_NOT_FOUND;
    
    /* Decode header fields */
    int8_t enc_track = uft_mac_gcr_decode[*in++];
    int8_t enc_sector = uft_mac_gcr_decode[*in++];
    int8_t enc_side = uft_mac_gcr_decode[*in++];
    int8_t enc_format = uft_mac_gcr_decode[*in++];
    int8_t enc_checksum = uft_mac_gcr_decode[*in++];
    
    if (enc_track < 0 || enc_sector < 0 || enc_side < 0 || 
        enc_format < 0 || enc_checksum < 0) {
        return UFT_GCR_ERR_INVALID_CODE;
    }
    
    /* Verify checksum */
    uint8_t calc_checksum = (uint8_t)(enc_track ^ enc_sector ^ enc_side ^ enc_format);
    if (calc_checksum != (uint8_t)enc_checksum) {
        return UFT_GCR_ERR_CHECKSUM;
    }
    
    /* Decode values */
    uint8_t real_track = (uint8_t)enc_track;
    if (enc_side & 0x01) {  /* Overflow bit */
        real_track += 64;
    }
    
    if (track) *track = real_track;
    if (sector) *sector = (uint8_t)enc_sector;
    if (side) *side = (enc_side & 0x20) ? 1 : 0;
    if (format) *format = (uint8_t)enc_format;
    
    return UFT_GCR_OK;
}

/* ============================================================================
 * Track-Level Operations
 * ============================================================================ */

ssize_t uft_gcr_find_sync(
    const uint8_t *track_data,
    size_t track_len,
    const uint8_t *pattern,
    size_t pattern_len,
    size_t start_offset
) {
    if (!track_data || !pattern || pattern_len == 0 || start_offset >= track_len) {
        return -1;
    }
    
    for (size_t i = start_offset; i <= track_len - pattern_len; i++) {
        bool match = true;
        for (size_t j = 0; j < pattern_len; j++) {
            if (track_data[i + j] != pattern[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return (ssize_t)i;
        }
    }
    
    return -1;
}

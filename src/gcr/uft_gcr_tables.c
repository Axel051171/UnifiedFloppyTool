/**
 * @file uft_gcr_tables.c
 * @brief Comprehensive GCR Encoding Tables Implementation
 * 
 * Sources:
 * - MAME flopimg.cpp (BSD-3-Clause) - GCR5, GCR6 tables
 * - Wikipedia "Group coded recording" - 4b/5b table
 * - Commodore 1571 ROM - Zone definitions
 * 
 * @copyright UFT Project 2026
 */

#include "uft/gcr/uft_gcr_tables.h"
#include <string.h>

/*============================================================================
 * Commodore GCR5 Tables (from MAME flopimg.cpp)
 *============================================================================*/

const uint8_t uft_gcr5_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

const uint8_t uft_gcr5_decode[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* 0x00-0x07: invalid */
    0x00, 0x08, 0x00, 0x01, 0x00, 0x0C, 0x04, 0x05,  /* 0x08-0x0F */
    0x00, 0x00, 0x02, 0x03, 0x00, 0x0F, 0x06, 0x07,  /* 0x10-0x17 */
    0x00, 0x09, 0x0A, 0x0B, 0x00, 0x0D, 0x0E, 0x00   /* 0x18-0x1F */
};

const uint8_t uft_gcr5_valid[32] = {
    0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x07 */
    0, 1, 1, 1, 0, 1, 1, 1,  /* 0x08-0x0F: 09,0A,0B,0D,0E,0F valid */
    0, 0, 1, 1, 0, 1, 1, 1,  /* 0x10-0x17: 12,13,15,16,17 valid */
    0, 1, 1, 1, 0, 1, 1, 0   /* 0x18-0x1F: 19,1A,1B,1D,1E valid */
};

/*============================================================================
 * Apple II / Macintosh GCR6 Tables (from MAME flopimg.cpp)
 *============================================================================*/

const uint8_t uft_gcr6_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

const uint8_t uft_gcr6_decode[256] = {
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

/*============================================================================
 * Micropolis 4b/5b Tables (from Wikipedia)
 *============================================================================*/

const uint8_t uft_gcr_4b5b_encode[16] = {
    0x19, 0x1B, 0x12, 0x13,  /* 0-3 */
    0x1D, 0x15, 0x16, 0x17,  /* 4-7 */
    0x1A, 0x09, 0x0A, 0x0B,  /* 8-B */
    0x1E, 0x0D, 0x0E, 0x0F   /* C-F */
};

const uint8_t uft_gcr_4b5b_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0x00-0x07 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0x0F,  /* 0x08-0x0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x05, 0x06, 0x07,  /* 0x10-0x17 */
    0xFF, 0x00, 0x08, 0x01, 0x04, 0xFF, 0x0C, 0xFF   /* 0x18-0x1F */
};

/*============================================================================
 * Commodore Zone Tables (from D64 specification)
 *============================================================================*/

const uint8_t uft_c64_speed_zone[42] = {
    /* Tracks 1-17: Zone 3 */
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    /* Tracks 18-24: Zone 2 */
    2, 2, 2, 2, 2, 2, 2,
    /* Tracks 25-30: Zone 1 */
    1, 1, 1, 1, 1, 1,
    /* Tracks 31-42: Zone 0 */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const uint8_t uft_c64_sectors_per_track[42] = {
    /* Tracks 1-17: 21 sectors */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    /* Tracks 18-24: 19 sectors */
    19, 19, 19, 19, 19, 19, 19,
    /* Tracks 25-30: 18 sectors */
    18, 18, 18, 18, 18, 18,
    /* Tracks 31-42: 17 sectors */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17
};

const uint16_t uft_c64_cell_size[4] = {
    4000,   /* Zone 0: 16MHz/16/4 = 4.00 µs */
    3750,   /* Zone 1: 16MHz/15/4 = 3.75 µs */
    3500,   /* Zone 2: 16MHz/14/4 = 3.50 µs */
    3250    /* Zone 3: 16MHz/13/4 = 3.25 µs */
};

/*============================================================================
 * GCR5 Encoding/Decoding Functions
 *============================================================================*/

void uft_gcr5_encode_4to5(const uint8_t in[4], uint8_t out[5])
{
    /* Extract 8 nibbles */
    uint8_t n0 = (in[0] >> 4) & 0x0F;
    uint8_t n1 = in[0] & 0x0F;
    uint8_t n2 = (in[1] >> 4) & 0x0F;
    uint8_t n3 = in[1] & 0x0F;
    uint8_t n4 = (in[2] >> 4) & 0x0F;
    uint8_t n5 = in[2] & 0x0F;
    uint8_t n6 = (in[3] >> 4) & 0x0F;
    uint8_t n7 = in[3] & 0x0F;
    
    /* Encode to 5-bit symbols */
    uint8_t g0 = uft_gcr5_encode[n0];
    uint8_t g1 = uft_gcr5_encode[n1];
    uint8_t g2 = uft_gcr5_encode[n2];
    uint8_t g3 = uft_gcr5_encode[n3];
    uint8_t g4 = uft_gcr5_encode[n4];
    uint8_t g5 = uft_gcr5_encode[n5];
    uint8_t g6 = uft_gcr5_encode[n6];
    uint8_t g7 = uft_gcr5_encode[n7];
    
    /* Pack into 5 bytes (40 bits) */
    out[0] = (g0 << 3) | (g1 >> 2);
    out[1] = (g1 << 6) | (g2 << 1) | (g3 >> 4);
    out[2] = (g3 << 4) | (g4 >> 1);
    out[3] = (g4 << 7) | (g5 << 2) | (g6 >> 3);
    out[4] = (g6 << 5) | g7;
}

int uft_gcr5_decode_5to4(const uint8_t in[5], uint8_t out[4])
{
    /* Extract 8 5-bit symbols */
    uint8_t g0 = (in[0] >> 3) & 0x1F;
    uint8_t g1 = ((in[0] << 2) | (in[1] >> 6)) & 0x1F;
    uint8_t g2 = (in[1] >> 1) & 0x1F;
    uint8_t g3 = ((in[1] << 4) | (in[2] >> 4)) & 0x1F;
    uint8_t g4 = ((in[2] << 1) | (in[3] >> 7)) & 0x1F;
    uint8_t g5 = (in[3] >> 2) & 0x1F;
    uint8_t g6 = ((in[3] << 3) | (in[4] >> 5)) & 0x1F;
    uint8_t g7 = in[4] & 0x1F;
    
    int errors = 0;
    
    /* Decode and count errors */
    uint8_t n0 = uft_gcr5_decode[g0]; if (!uft_gcr5_valid[g0]) errors++;
    uint8_t n1 = uft_gcr5_decode[g1]; if (!uft_gcr5_valid[g1]) errors++;
    uint8_t n2 = uft_gcr5_decode[g2]; if (!uft_gcr5_valid[g2]) errors++;
    uint8_t n3 = uft_gcr5_decode[g3]; if (!uft_gcr5_valid[g3]) errors++;
    uint8_t n4 = uft_gcr5_decode[g4]; if (!uft_gcr5_valid[g4]) errors++;
    uint8_t n5 = uft_gcr5_decode[g5]; if (!uft_gcr5_valid[g5]) errors++;
    uint8_t n6 = uft_gcr5_decode[g6]; if (!uft_gcr5_valid[g6]) errors++;
    uint8_t n7 = uft_gcr5_decode[g7]; if (!uft_gcr5_valid[g7]) errors++;
    
    /* Pack nibbles */
    out[0] = (n0 << 4) | n1;
    out[1] = (n2 << 4) | n3;
    out[2] = (n4 << 4) | n5;
    out[3] = (n6 << 4) | n7;
    
    return errors;
}

/*============================================================================
 * GCR6 Encoding/Decoding Functions
 *============================================================================*/

void uft_gcr6_encode_3to4(uint8_t a, uint8_t b, uint8_t c, uint8_t out[4])
{
    /* Apple II/Mac GCR6 encoding
     * 3 bytes (24 bits) → 4 disk bytes
     * Each byte split: 2 low bits to aux, 6 high bits to main
     */
    uint8_t aux = ((a & 0x03) << 4) | ((b & 0x03) << 2) | (c & 0x03);
    
    out[0] = uft_gcr6_encode[aux & 0x3F];
    out[1] = uft_gcr6_encode[(a >> 2) & 0x3F];
    out[2] = uft_gcr6_encode[(b >> 2) & 0x3F];
    out[3] = uft_gcr6_encode[(c >> 2) & 0x3F];
}

void uft_gcr6_decode_4to3(const uint8_t in[4], uint8_t *a, uint8_t *b, uint8_t *c)
{
    uint8_t aux = uft_gcr6_decode[in[0]];
    uint8_t d1 = uft_gcr6_decode[in[1]];
    uint8_t d2 = uft_gcr6_decode[in[2]];
    uint8_t d3 = uft_gcr6_decode[in[3]];
    
    *a = (d1 << 2) | ((aux >> 4) & 0x03);
    *b = (d2 << 2) | ((aux >> 2) & 0x03);
    *c = (d3 << 2) | (aux & 0x03);
}

/*============================================================================
 * Micropolis 4b/5b Functions
 *============================================================================*/

/* Bit writer for 4b/5b encoding */
typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t bitpos;
} bitwriter_t;

static void bitwriter_init(bitwriter_t *w, uint8_t *buf, size_t cap)
{
    w->buf = buf;
    w->cap = cap;
    w->bitpos = 0;
    memset(buf, 0, cap);
}

static int bitwriter_put(bitwriter_t *w, uint32_t bits, uint8_t nbits)
{
    if (w->bitpos + nbits > w->cap * 8) return -1;
    
    for (uint8_t i = 0; i < nbits; i++) {
        uint8_t b = (bits >> (nbits - 1 - i)) & 1;
        size_t byte_i = w->bitpos / 8;
        uint8_t bit_i = 7 - (w->bitpos % 8);
        w->buf[byte_i] |= (b << bit_i);
        w->bitpos++;
    }
    return 0;
}

uft_gcr_status_t uft_gcr_4b5b_encode_bytes(const uint8_t *in, size_t in_len,
                                            uint8_t *out, size_t *out_len)
{
    if (!out_len) return UFT_GCR_E_INVALID;
    
    size_t need = uft_gcr_4b5b_encoded_size(in_len);
    if (!out || *out_len < need) {
        *out_len = need;
        return UFT_GCR_E_BUF;
    }
    if (!in && in_len > 0) return UFT_GCR_E_INVALID;
    
    bitwriter_t w;
    bitwriter_init(&w, out, need);
    
    for (size_t i = 0; i < in_len; i++) {
        uint8_t hi = (in[i] >> 4) & 0x0F;
        uint8_t lo = in[i] & 0x0F;
        
        if (bitwriter_put(&w, uft_gcr_4b5b_encode[hi], 5) < 0) return UFT_GCR_E_BUF;
        if (bitwriter_put(&w, uft_gcr_4b5b_encode[lo], 5) < 0) return UFT_GCR_E_BUF;
    }
    
    *out_len = need;
    return UFT_GCR_OK;
}

/* Bit reader for 4b/5b decoding */
typedef struct {
    const uint8_t *buf;
    size_t len;
    size_t bitpos;
} bitreader_t;

static void bitreader_init(bitreader_t *r, const uint8_t *buf, size_t len)
{
    r->buf = buf;
    r->len = len;
    r->bitpos = 0;
}

static int bitreader_get(bitreader_t *r, uint8_t nbits, uint32_t *out)
{
    if (r->bitpos + nbits > r->len * 8) return -1;
    
    uint32_t v = 0;
    for (uint8_t i = 0; i < nbits; i++) {
        size_t byte_i = r->bitpos / 8;
        uint8_t bit_i = 7 - (r->bitpos % 8);
        uint8_t b = (r->buf[byte_i] >> bit_i) & 1;
        v = (v << 1) | b;
        r->bitpos++;
    }
    *out = v;
    return 0;
}

uft_gcr_status_t uft_gcr_4b5b_decode_bytes(const uint8_t *in, size_t in_len,
                                            uint8_t *out, size_t *out_len)
{
    if (!out_len) return UFT_GCR_E_INVALID;
    if (!in && in_len > 0) return UFT_GCR_E_INVALID;
    
    size_t bits = in_len * 8;
    if (bits % 10 != 0) return UFT_GCR_E_INVALID;
    
    size_t out_bytes = bits / 10;
    if (!out || *out_len < out_bytes) {
        *out_len = out_bytes;
        return UFT_GCR_E_BUF;
    }
    
    bitreader_t r;
    bitreader_init(&r, in, in_len);
    
    for (size_t i = 0; i < out_bytes; i++) {
        uint32_t s1, s2;
        if (bitreader_get(&r, 5, &s1) < 0) return UFT_GCR_E_BUF;
        if (bitreader_get(&r, 5, &s2) < 0) return UFT_GCR_E_BUF;
        
        uint8_t n1 = uft_gcr_4b5b_decode[s1 & 0x1F];
        uint8_t n2 = uft_gcr_4b5b_decode[s2 & 0x1F];
        
        if (n1 == 0xFF || n2 == 0xFF) return UFT_GCR_E_DECODE;
        
        out[i] = (n1 << 4) | (n2 & 0x0F);
    }
    
    *out_len = out_bytes;
    return UFT_GCR_OK;
}

/*============================================================================
 * Checksum Functions
 *============================================================================*/

uint8_t uft_gcr_checksum_xor(const uint8_t *data, size_t len)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

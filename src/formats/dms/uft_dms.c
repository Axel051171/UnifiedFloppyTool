/*
 * uft_dms.c — DMS (Disk Masher System) decompression library for UFT
 * ===================================================================
 * Reentrant, memory-buffer based DMS → ADF decoder.
 * Based on xDMS 1.3 by Andre Rodrigues de la Rocha (Public Domain).
 *
 * Refactored for UFT:
 *  - All global mutable state → dms_ctx_t context struct
 *  - FILE* I/O → memory buffer with cursor
 *  - Clean C11, no UCHAR/USHORT/ULONG macros
 *  - Proper error propagation
 *  - Thread-safe / reentrant
 */

#include "uft_dms.h"

#include <stdlib.h>
#include <string.h>

/* ================================================================== */
/*  Internal constants                                                 */
/* ================================================================== */

#define DMS_HEADLEN          56
#define DMS_THLEN            20
#define DMS_TRACK_BUFFER_LEN 32000
#define DMS_TEMP_BUFFER_LEN  32000

/* Deep mode constants */
#define DEEP_DBITMASK  0x3fff
#define DEEP_F         60
#define DEEP_THRESHOLD 2
#define DEEP_N_CHAR    (256 - DEEP_THRESHOLD + DEEP_F)
#define DEEP_T         (DEEP_N_CHAR * 2 - 1)
#define DEEP_R         (DEEP_T - 1)
#define DEEP_MAX_FREQ  0x8000

/* Heavy mode constants */
#define HEAVY_NC     510
#define HEAVY_NPT    20
#define HEAVY_N1     510
#define HEAVY_OFFSET 253

/* ================================================================== */
/*  Context struct — holds ALL mutable state (replaces globals)        */
/* ================================================================== */

struct dms_ctx {
    /* Memory input buffer */
    const uint8_t *in_buf;
    size_t         in_len;
    size_t         in_pos;

    /* Memory output buffer */
    uint8_t       *out_buf;
    size_t         out_cap;
    size_t         out_pos;

    /* Track buffers */
    uint8_t       *b1;           /* TRACK_BUFFER_LEN */
    uint8_t       *b2;           /* TRACK_BUFFER_LEN */

    /* Text buffer shared by decompressors (replaces global 'text') */
    uint8_t       *text;         /* TEMP_BUFFER_LEN */

    /* Bitstream reader state (replaces getbits globals) */
    uint8_t       *bit_indata;
    uint32_t       bitbuf;
    uint8_t        bitcount;

    /* Quick mode state */
    uint16_t       quick_text_loc;

    /* Medium mode state */
    uint16_t       medium_text_loc;

    /* Deep mode state */
    uint16_t       deep_text_loc;
    int            init_deep_tabs;
    uint16_t       deep_freq[DEEP_T + 1];
    uint16_t       deep_prnt[DEEP_T + DEEP_N_CHAR];
    uint16_t       deep_son[DEEP_T];

    /* Heavy mode state */
    uint16_t       heavy_text_loc;
    uint16_t       heavy_lastlen;
    uint16_t       heavy_np;
    uint16_t       heavy_left[2 * HEAVY_NC - 1];
    uint16_t       heavy_right[2 * HEAVY_NC - 1 + 9];
    uint8_t        heavy_c_len[HEAVY_NC];
    uint16_t       heavy_c_table[4096];
    uint8_t        heavy_pt_len[HEAVY_NPT];
    uint16_t       heavy_pt_table[256];

    /* Decryption state */
    uint16_t       pwd_crc;

    /* Options */
    int            override_errors;

    /* Callback */
    dms_track_cb_t track_cb;
    void          *track_cb_user;
};

/* ================================================================== */
/*  Static lookup tables (read-only, safe to share)                    */
/* ================================================================== */

static const uint32_t mask_bits[25] = {
    0x000000,0x000001,0x000003,0x000007,0x00000f,0x00001f,
    0x00003f,0x00007f,0x0000ff,0x0001ff,0x0003ff,0x0007ff,
    0x000fff,0x001fff,0x003fff,0x007fff,0x00ffff,0x01ffff,
    0x03ffff,0x07ffff,0x0fffff,0x1fffff,0x3fffff,0x7fffff,
    0xffffff
};

static const uint8_t d_code[256] = {
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,
    0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,
    0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,
    0x0C,0x0C,0x0C,0x0C,0x0D,0x0D,0x0D,0x0D,0x0E,0x0E,0x0E,0x0E,0x0F,0x0F,0x0F,0x0F,
    0x10,0x10,0x10,0x10,0x11,0x11,0x11,0x11,0x12,0x12,0x12,0x12,0x13,0x13,0x13,0x13,
    0x14,0x14,0x14,0x14,0x15,0x15,0x15,0x15,0x16,0x16,0x16,0x16,0x17,0x17,0x17,0x17,
    0x18,0x18,0x19,0x19,0x1A,0x1A,0x1B,0x1B,0x1C,0x1C,0x1D,0x1D,0x1E,0x1E,0x1F,0x1F,
    0x20,0x20,0x21,0x21,0x22,0x22,0x23,0x23,0x24,0x24,0x25,0x25,0x26,0x26,0x27,0x27,
    0x28,0x28,0x29,0x29,0x2A,0x2A,0x2B,0x2B,0x2C,0x2C,0x2D,0x2D,0x2E,0x2E,0x2F,0x2F,
    0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F,
};

static const uint8_t d_len[256] = {
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,
    0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,
    0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
    0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
    0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,
    0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
    0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
    0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,
    0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,
};

/* ================================================================== */
/*  CRC-16 + Checksum (from xDMS, by Bjorn Stenberg)                  */
/* ================================================================== */

static const uint16_t crc_tab[256] = {
    0x0000,0xC0C1,0xC181,0x0140,0xC301,0x03C0,0x0280,0xC241,
    0xC601,0x06C0,0x0780,0xC741,0x0500,0xC5C1,0xC481,0x0440,
    0xCC01,0x0CC0,0x0D80,0xCD41,0x0F00,0xCFC1,0xCE81,0x0E40,
    0x0A00,0xCAC1,0xCB81,0x0B40,0xC901,0x09C0,0x0880,0xC841,
    0xD801,0x18C0,0x1980,0xD941,0x1B00,0xDBC1,0xDA81,0x1A40,
    0x1E00,0xDEC1,0xDF81,0x1F40,0xDD01,0x1DC0,0x1C80,0xDC41,
    0x1400,0xD4C1,0xD581,0x1540,0xD701,0x17C0,0x1680,0xD641,
    0xD201,0x12C0,0x1380,0xD341,0x1100,0xD1C1,0xD081,0x1040,
    0xF001,0x30C0,0x3180,0xF141,0x3300,0xF3C1,0xF281,0x3240,
    0x3600,0xF6C1,0xF781,0x3740,0xF501,0x35C0,0x3480,0xF441,
    0x3C00,0xFCC1,0xFD81,0x3D40,0xFF01,0x3FC0,0x3E80,0xFE41,
    0xFA01,0x3AC0,0x3B80,0xFB41,0x3900,0xF9C1,0xF881,0x3840,
    0x2800,0xE8C1,0xE981,0x2940,0xEB01,0x2BC0,0x2A80,0xEA41,
    0xEE01,0x2EC0,0x2F80,0xEF41,0x2D00,0xEDC1,0xEC81,0x2C40,
    0xE401,0x24C0,0x2580,0xE541,0x2700,0xE7C1,0xE681,0x2640,
    0x2200,0xE2C1,0xE381,0x2340,0xE101,0x21C0,0x2080,0xE041,
    0xA001,0x60C0,0x6180,0xA141,0x6300,0xA3C1,0xA281,0x6240,
    0x6600,0xA6C1,0xA781,0x6740,0xA501,0x65C0,0x6480,0xA441,
    0x6C00,0xACC1,0xAD81,0x6D40,0xAF01,0x6FC0,0x6E80,0xAE41,
    0xAA01,0x6AC0,0x6B80,0xAB41,0x6900,0xA9C1,0xA881,0x6840,
    0x7800,0xB8C1,0xB981,0x7940,0xBB01,0x7BC0,0x7A80,0xBA41,
    0xBE01,0x7EC0,0x7F80,0xBF41,0x7D00,0xBDC1,0xBC81,0x7C40,
    0xB401,0x74C0,0x7580,0xB541,0x7700,0xB7C1,0xB681,0x7640,
    0x7200,0xB2C1,0xB381,0x7340,0xB101,0x71C0,0x7080,0xB041,
    0x5000,0x90C1,0x9181,0x5140,0x9301,0x53C0,0x5280,0x9241,
    0x9601,0x56C0,0x5780,0x9741,0x5500,0x95C1,0x9481,0x5440,
    0x9C01,0x5CC0,0x5D80,0x9D41,0x5F00,0x9FC1,0x9E81,0x5E40,
    0x5A00,0x9AC1,0x9B81,0x5B40,0x9901,0x59C0,0x5880,0x9841,
    0x8801,0x48C0,0x4980,0x8941,0x4B00,0x8BC1,0x8A81,0x4A40,
    0x4E00,0x8EC1,0x8F81,0x4F40,0x8D01,0x4DC0,0x4C80,0x8C41,
    0x4400,0x84C1,0x8581,0x4540,0x8701,0x47C0,0x4680,0x8641,
    0x8201,0x42C0,0x4380,0x8341,0x4100,0x81C1,0x8081,0x4040
};

static uint16_t dms_crc16(const uint8_t *mem, size_t size) {
    uint16_t crc = 0;
    for (size_t i = 0; i < size; i++)
        crc = (uint16_t)(crc_tab[((crc ^ mem[i]) & 0xFF)] ^ ((crc >> 8) & 0xFF));
    return crc;
}

static uint16_t dms_checksum(const uint8_t *mem, size_t size) {
    uint16_t u = 0;
    for (size_t i = 0; i < size; i++) u += mem[i];
    return u;
}

/* ================================================================== */
/*  Bitstream reader (context-based, replaces getbits globals)         */
/* ================================================================== */

#define GETBITS(ctx, n)  ((uint16_t)((ctx)->bitbuf >> ((ctx)->bitcount - (n))))

#define DROPBITS(ctx, n) do { \
    (ctx)->bitbuf &= mask_bits[(ctx)->bitcount -= (n)]; \
    while ((ctx)->bitcount < 16) { \
        (ctx)->bitbuf = ((ctx)->bitbuf << 8) | *(ctx)->bit_indata++; \
        (ctx)->bitcount += 8; \
    } \
} while(0)

static void init_bitbuf(dms_ctx_t *ctx, uint8_t *in) {
    ctx->bitbuf = 0;
    ctx->bitcount = 0;
    ctx->bit_indata = in;
    DROPBITS(ctx, 0);
}

/* ================================================================== */
/*  Decruncher init (replaces Init_Decrunchers)                        */
/* ================================================================== */

static void init_decrunchers(dms_ctx_t *ctx) {
    ctx->quick_text_loc  = 251;
    ctx->medium_text_loc = 0x3fbe;
    ctx->heavy_lastlen   = 0;
    ctx->heavy_text_loc  = 0;
    ctx->deep_text_loc   = 0x3fc4;
    ctx->init_deep_tabs  = 1;
    memset(ctx->text, 0, 0x3fc8);
}

/* ================================================================== */
/*  RLE decompression                                                  */
/* ================================================================== */

static uint16_t unpack_rle(dms_ctx_t *ctx, uint8_t *in, uint8_t *out, uint16_t origsize) {
    (void)ctx;
    uint8_t *outend = out + origsize;
    while (out < outend) {
        uint8_t a = *in++;
        if (a != 0x90) {
            *out++ = a;
        } else {
            uint8_t b = *in++;
            if (!b) {
                *out++ = a;
            } else {
                a = *in++;
                uint16_t n;
                if (b == 0xff) {
                    n = (uint16_t)(*in++ << 8);
                    n = (uint16_t)(n + *in++);
                } else {
                    n = b;
                }
                if (out + n > outend) return 1;
                memset(out, a, n);
                out += n;
            }
        }
    }
    return 0;
}

/* ================================================================== */
/*  QUICK decompression                                                */
/* ================================================================== */

#define QBITMASK 0xff

static uint16_t unpack_quick(dms_ctx_t *ctx, uint8_t *in, uint8_t *out, uint16_t origsize) {
    init_bitbuf(ctx, in);
    uint8_t *outend = out + origsize;
    while (out < outend) {
        if (GETBITS(ctx, 1) != 0) {
            DROPBITS(ctx, 1);
            *out++ = ctx->text[ctx->quick_text_loc++ & QBITMASK] = (uint8_t)GETBITS(ctx, 8);
            DROPBITS(ctx, 8);
        } else {
            DROPBITS(ctx, 1);
            uint16_t j = (uint16_t)(GETBITS(ctx, 2) + 2); DROPBITS(ctx, 2);
            uint16_t i = (uint16_t)(ctx->quick_text_loc - GETBITS(ctx, 8) - 1); DROPBITS(ctx, 8);
            while (j--) {
                *out++ = ctx->text[ctx->quick_text_loc++ & QBITMASK] = ctx->text[i++ & QBITMASK];
            }
        }
    }
    ctx->quick_text_loc = (uint16_t)((ctx->quick_text_loc + 5) & QBITMASK);
    return 0;
}

/* ================================================================== */
/*  MEDIUM decompression                                               */
/* ================================================================== */

#define MBITMASK 0x3fff

static uint16_t unpack_medium(dms_ctx_t *ctx, uint8_t *in, uint8_t *out, uint16_t origsize) {
    init_bitbuf(ctx, in);
    uint8_t *outend = out + origsize;
    while (out < outend) {
        if (GETBITS(ctx, 1) != 0) {
            DROPBITS(ctx, 1);
            *out++ = ctx->text[ctx->medium_text_loc++ & MBITMASK] = (uint8_t)GETBITS(ctx, 8);
            DROPBITS(ctx, 8);
        } else {
            DROPBITS(ctx, 1);
            uint16_t c = GETBITS(ctx, 8); DROPBITS(ctx, 8);
            uint16_t j = (uint16_t)(d_code[c] + 3);
            uint8_t u = d_len[c];
            c = (uint16_t)(((c << u) | GETBITS(ctx, u)) & 0xff); DROPBITS(ctx, u);
            u = d_len[c];
            c = (uint16_t)((d_code[c] << 8) | (((c << u) | GETBITS(ctx, u)) & 0xff)); DROPBITS(ctx, u);
            uint16_t i = (uint16_t)(ctx->medium_text_loc - c - 1);
            while (j--) *out++ = ctx->text[ctx->medium_text_loc++ & MBITMASK] = ctx->text[i++ & MBITMASK];
        }
    }
    ctx->medium_text_loc = (uint16_t)((ctx->medium_text_loc + 66) & MBITMASK);
    return 0;
}

/* ================================================================== */
/*  DEEP decompression (LZ + dynamic Huffman)                          */
/* ================================================================== */

static void deep_init_tabs(dms_ctx_t *ctx) {
    uint16_t i, j;
    for (i = 0; i < DEEP_N_CHAR; i++) {
        ctx->deep_freq[i] = 1;
        ctx->deep_son[i] = (uint16_t)(i + DEEP_T);
        ctx->deep_prnt[i + DEEP_T] = i;
    }
    i = 0; j = DEEP_N_CHAR;
    while (j <= DEEP_R) {
        ctx->deep_freq[j] = (uint16_t)(ctx->deep_freq[i] + ctx->deep_freq[i + 1]);
        ctx->deep_son[j] = i;
        ctx->deep_prnt[i] = ctx->deep_prnt[i + 1] = j;
        i += 2; j++;
    }
    ctx->deep_freq[DEEP_T] = 0xffff;
    ctx->deep_prnt[DEEP_R] = 0;
    ctx->init_deep_tabs = 0;
}

static void deep_reconst(dms_ctx_t *ctx) {
    uint16_t i, j, k, f, l;
    j = 0;
    for (i = 0; i < DEEP_T; i++) {
        if (ctx->deep_son[i] >= DEEP_T) {
            ctx->deep_freq[j] = (uint16_t)((ctx->deep_freq[i] + 1) / 2);
            ctx->deep_son[j] = ctx->deep_son[i];
            j++;
        }
    }
    for (i = 0, j = DEEP_N_CHAR; j < DEEP_T; i += 2, j++) {
        k = (uint16_t)(i + 1);
        f = ctx->deep_freq[j] = (uint16_t)(ctx->deep_freq[i] + ctx->deep_freq[k]);
        for (k = (uint16_t)(j - 1); f < ctx->deep_freq[k]; k--) ;
        k++;
        l = (uint16_t)((j - k) * 2);
        memmove(&ctx->deep_freq[k + 1], &ctx->deep_freq[k], l);
        ctx->deep_freq[k] = f;
        memmove(&ctx->deep_son[k + 1], &ctx->deep_son[k], l);
        ctx->deep_son[k] = i;
    }
    for (i = 0; i < DEEP_T; i++) {
        if ((k = ctx->deep_son[i]) >= DEEP_T)
            ctx->deep_prnt[k] = i;
        else
            ctx->deep_prnt[k] = ctx->deep_prnt[k + 1] = i;
    }
}

static void deep_update(dms_ctx_t *ctx, uint16_t c) {
    uint16_t i, j, k, l;
    if (ctx->deep_freq[DEEP_R] == DEEP_MAX_FREQ)
        deep_reconst(ctx);
    c = ctx->deep_prnt[c + DEEP_T];
    do {
        k = ++ctx->deep_freq[c];
        if (k > ctx->deep_freq[l = (uint16_t)(c + 1)]) {
            while (k > ctx->deep_freq[++l]) ;
            l--;
            ctx->deep_freq[c] = ctx->deep_freq[l];
            ctx->deep_freq[l] = k;
            i = ctx->deep_son[c];
            ctx->deep_prnt[i] = l;
            if (i < DEEP_T) ctx->deep_prnt[i + 1] = l;
            j = ctx->deep_son[l];
            ctx->deep_son[l] = i;
            ctx->deep_prnt[j] = c;
            if (j < DEEP_T) ctx->deep_prnt[j + 1] = c;
            ctx->deep_son[c] = j;
            c = l;
        }
    } while ((c = ctx->deep_prnt[c]) != 0);
}

static uint16_t deep_decode_char(dms_ctx_t *ctx) {
    uint16_t c = ctx->deep_son[DEEP_R];
    while (c < DEEP_T) {
        c = ctx->deep_son[c + GETBITS(ctx, 1)];
        DROPBITS(ctx, 1);
    }
    c -= DEEP_T;
    deep_update(ctx, c);
    return c;
}

static uint16_t deep_decode_position(dms_ctx_t *ctx) {
    uint16_t i = GETBITS(ctx, 8); DROPBITS(ctx, 8);
    uint16_t c = (uint16_t)(d_code[i] << 8);
    uint16_t j = d_len[i];
    i = (uint16_t)(((i << j) | GETBITS(ctx, j)) & 0xff); DROPBITS(ctx, j);
    return (uint16_t)(c | i);
}

static uint16_t unpack_deep(dms_ctx_t *ctx, uint8_t *in, uint8_t *out, uint16_t origsize) {
    init_bitbuf(ctx, in);
    if (ctx->init_deep_tabs) deep_init_tabs(ctx);

    uint8_t *outend = out + origsize;
    while (out < outend) {
        uint16_t c = deep_decode_char(ctx);
        if (c < 256) {
            *out++ = ctx->text[ctx->deep_text_loc++ & DEEP_DBITMASK] = (uint8_t)c;
        } else {
            uint16_t j = (uint16_t)(c - 255 + DEEP_THRESHOLD);
            uint16_t i = (uint16_t)(ctx->deep_text_loc - deep_decode_position(ctx) - 1);
            while (j--) *out++ = ctx->text[ctx->deep_text_loc++ & DEEP_DBITMASK] = ctx->text[i++ & DEEP_DBITMASK];
        }
    }
    ctx->deep_text_loc = (uint16_t)((ctx->deep_text_loc + 60) & DEEP_DBITMASK);
    return 0;
}

/* ================================================================== */
/*  make_table for Heavy mode (context-based)                          */
/* ================================================================== */

/* Recursive table builder state (passed as locals through recursion) */
typedef struct {
    dms_ctx_t *ctx;
    int16_t    c;
    uint16_t   n, tblsiz, len, depth, maxdepth, avail;
    uint16_t   codeword, bit;
    uint16_t  *tbl;
    uint8_t   *blen;
    uint16_t   tab_err;
} mktbl_state_t;

static uint16_t mktbl_recurse(mktbl_state_t *s) {
    uint16_t i = 0;
    if (s->tab_err) return 0;

    if (s->len == s->depth) {
        while (++(s->c) < (int16_t)s->n) {
            if (s->blen[s->c] == s->len) {
                i = s->codeword;
                s->codeword += s->bit;
                if (s->codeword > s->tblsiz) { s->tab_err = 1; return 0; }
                while (i < s->codeword) s->tbl[i++] = (uint16_t)s->c;
                return (uint16_t)s->c;
            }
        }
        s->c = -1;
        s->len++;
        s->bit >>= 1;
    }
    s->depth++;
    if (s->depth < s->maxdepth) {
        mktbl_recurse(s);
        mktbl_recurse(s);
    } else if (s->depth > 32) {
        s->tab_err = 2;
        return 0;
    } else {
        if ((i = s->avail++) >= 2 * s->n - 1) { s->tab_err = 3; return 0; }
        s->ctx->heavy_left[i] = mktbl_recurse(s);
        s->ctx->heavy_right[i] = mktbl_recurse(s);
        if (s->codeword >= s->tblsiz) { s->tab_err = 4; return 0; }
        if (s->depth == s->maxdepth) s->tbl[s->codeword++] = i;
    }
    s->depth--;
    return i;
}

static uint16_t make_table(dms_ctx_t *ctx, uint16_t nchar, uint8_t *bitlen,
                           uint16_t tablebits, uint16_t *table) {
    mktbl_state_t s;
    s.ctx = ctx;
    s.n = s.avail = nchar;
    s.blen = bitlen;
    s.tbl = table;
    s.tblsiz = (uint16_t)(1U << tablebits);
    s.bit = (uint16_t)(s.tblsiz / 2);
    s.maxdepth = (uint16_t)(tablebits + 1);
    s.depth = s.len = 1;
    s.c = -1;
    s.codeword = 0;
    s.tab_err = 0;
    mktbl_recurse(&s);
    if (s.tab_err) return s.tab_err;
    mktbl_recurse(&s);
    if (s.tab_err) return s.tab_err;
    if (s.codeword != s.tblsiz) return 5;
    return 0;
}

/* ================================================================== */
/*  HEAVY decompression (LZH)                                         */
/* ================================================================== */

static uint16_t heavy_decode_c(dms_ctx_t *ctx) {
    uint16_t j = ctx->heavy_c_table[GETBITS(ctx, 12)];
    if (j < HEAVY_N1) {
        DROPBITS(ctx, ctx->heavy_c_len[j]);
    } else {
        DROPBITS(ctx, 12);
        uint16_t i = GETBITS(ctx, 16);
        uint16_t m = 0x8000;
        do {
            if (i & m) j = ctx->heavy_right[j];
            else       j = ctx->heavy_left[j];
            m >>= 1;
        } while (j >= HEAVY_N1);
        DROPBITS(ctx, ctx->heavy_c_len[j] - 12);
    }
    return j;
}

static uint16_t heavy_decode_p(dms_ctx_t *ctx) {
    uint16_t j = ctx->heavy_pt_table[GETBITS(ctx, 8)];
    if (j < ctx->heavy_np) {
        DROPBITS(ctx, ctx->heavy_pt_len[j]);
    } else {
        DROPBITS(ctx, 8);
        uint16_t i = GETBITS(ctx, 16);
        uint16_t m = 0x8000;
        do {
            if (i & m) j = ctx->heavy_right[j];
            else       j = ctx->heavy_left[j];
            m >>= 1;
        } while (j >= ctx->heavy_np);
        DROPBITS(ctx, ctx->heavy_pt_len[j] - 8);
    }
    if (j != ctx->heavy_np - 1) {
        if (j > 0) {
            uint16_t ii = (uint16_t)(j - 1);
            j = (uint16_t)(GETBITS(ctx, ii) | (1U << ii));
            DROPBITS(ctx, ii);
        }
        ctx->heavy_lastlen = j;
    }
    return ctx->heavy_lastlen;
}

static uint16_t heavy_read_tree_c(dms_ctx_t *ctx) {
    uint16_t n = GETBITS(ctx, 9); DROPBITS(ctx, 9);
    if (n > 0) {
        for (uint16_t i = 0; i < n; i++) {
            ctx->heavy_c_len[i] = (uint8_t)GETBITS(ctx, 5);
            DROPBITS(ctx, 5);
        }
        for (uint16_t i = n; i < 510; i++) ctx->heavy_c_len[i] = 0;
        if (make_table(ctx, 510, ctx->heavy_c_len, 12, ctx->heavy_c_table)) return 1;
    } else {
        n = GETBITS(ctx, 9); DROPBITS(ctx, 9);
        memset(ctx->heavy_c_len, 0, 510);
        for (uint16_t i = 0; i < 4096; i++) ctx->heavy_c_table[i] = n;
    }
    return 0;
}

static uint16_t heavy_read_tree_p(dms_ctx_t *ctx) {
    uint16_t n = GETBITS(ctx, 5); DROPBITS(ctx, 5);
    if (n > 0) {
        for (uint16_t i = 0; i < n; i++) {
            ctx->heavy_pt_len[i] = (uint8_t)GETBITS(ctx, 4);
            DROPBITS(ctx, 4);
        }
        for (uint16_t i = n; i < ctx->heavy_np; i++) ctx->heavy_pt_len[i] = 0;
        if (make_table(ctx, ctx->heavy_np, ctx->heavy_pt_len, 8, ctx->heavy_pt_table)) return 1;
    } else {
        n = GETBITS(ctx, 5); DROPBITS(ctx, 5);
        memset(ctx->heavy_pt_len, 0, HEAVY_NPT);
        for (uint16_t i = 0; i < 256; i++) ctx->heavy_pt_table[i] = n;
    }
    return 0;
}

static uint16_t unpack_heavy(dms_ctx_t *ctx, uint8_t *in, uint8_t *out,
                             uint8_t flags, uint16_t origsize) {
    uint16_t bitmask;
    if (flags & 8) {
        ctx->heavy_np = 15;
        bitmask = 0x1fff;
    } else {
        ctx->heavy_np = 14;
        bitmask = 0x0fff;
    }
    init_bitbuf(ctx, in);
    if (flags & 2) {
        if (heavy_read_tree_c(ctx)) return 1;
        if (heavy_read_tree_p(ctx)) return 2;
    }
    uint8_t *outend = out + origsize;
    while (out < outend) {
        uint16_t c = heavy_decode_c(ctx);
        if (c < 256) {
            *out++ = ctx->text[ctx->heavy_text_loc++ & bitmask] = (uint8_t)c;
        } else {
            uint16_t j = (uint16_t)(c - HEAVY_OFFSET);
            uint16_t i = (uint16_t)(ctx->heavy_text_loc - heavy_decode_p(ctx) - 1);
            while (j--) *out++ = ctx->text[ctx->heavy_text_loc++ & bitmask] = ctx->text[i++ & bitmask];
        }
    }
    return 0;
}

/* ================================================================== */
/*  Track unpacking dispatcher                                         */
/* ================================================================== */

static uint16_t unpack_track(dms_ctx_t *ctx, uint8_t *b1, uint8_t *b2,
                             uint16_t pklen2, uint16_t unpklen,
                             uint8_t cmode, uint8_t flags) {
    switch (cmode) {
    case 0: /* NOCOMP */
        memcpy(b2, b1, unpklen);
        break;
    case 1: /* SIMPLE = RLE */
        if (unpack_rle(ctx, b1, b2, unpklen)) return DMS_ERR_BAD_DECOMP;
        break;
    case 2: /* QUICK */
        if (unpack_quick(ctx, b1, b2, pklen2)) return DMS_ERR_BAD_DECOMP;
        if (unpack_rle(ctx, b2, b1, unpklen)) return DMS_ERR_BAD_DECOMP;
        memcpy(b2, b1, unpklen);
        break;
    case 3: /* MEDIUM */
        if (unpack_medium(ctx, b1, b2, pklen2)) return DMS_ERR_BAD_DECOMP;
        if (unpack_rle(ctx, b2, b1, unpklen)) return DMS_ERR_BAD_DECOMP;
        memcpy(b2, b1, unpklen);
        break;
    case 4: /* DEEP */
        if (unpack_deep(ctx, b1, b2, pklen2)) return DMS_ERR_BAD_DECOMP;
        if (unpack_rle(ctx, b2, b1, unpklen)) return DMS_ERR_BAD_DECOMP;
        memcpy(b2, b1, unpklen);
        break;
    case 5: /* HEAVY1 */
        if (unpack_heavy(ctx, b1, b2, flags & 7, pklen2)) return DMS_ERR_BAD_DECOMP;
        if (flags & 4) {
            if (unpack_rle(ctx, b2, b1, unpklen)) return DMS_ERR_BAD_DECOMP;
            memcpy(b2, b1, unpklen);
        }
        break;
    case 6: /* HEAVY2 */
        if (unpack_heavy(ctx, b1, b2, flags | 8, pklen2)) return DMS_ERR_BAD_DECOMP;
        if (flags & 4) {
            if (unpack_rle(ctx, b2, b1, unpklen)) return DMS_ERR_BAD_DECOMP;
            memcpy(b2, b1, unpklen);
        }
        break;
    default:
        return DMS_ERR_UNKN_MODE;
    }
    if (!(flags & 1)) init_decrunchers(ctx);
    return DMS_OK;
}

/* ================================================================== */
/*  DMS decryption (simple XOR scheme)                                 */
/* ================================================================== */

static void dms_decrypt(dms_ctx_t *ctx, uint8_t *p, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        uint16_t t = p[i];
        p[i] ^= (uint8_t)ctx->pwd_crc;
        ctx->pwd_crc = (uint16_t)((ctx->pwd_crc >> 1) + t);
    }
}

/* ================================================================== */
/*  Memory buffer read helper                                          */
/* ================================================================== */

static size_t mem_read(dms_ctx_t *ctx, uint8_t *dst, size_t count) {
    size_t avail = ctx->in_len - ctx->in_pos;
    if (count > avail) count = avail;
    if (count > 0) {
        memcpy(dst, ctx->in_buf + ctx->in_pos, count);
        ctx->in_pos += count;
    }
    return count;
}

/* ================================================================== */
/*  Password CRC computation                                           */
/* ================================================================== */

static uint16_t compute_pwd_crc(const char *password) {
    if (!password || !*password) return 0;
    size_t len = strlen(password);
    return dms_crc16((const uint8_t *)password, len);
}

/* ================================================================== */
/*  Public API                                                         */
/* ================================================================== */

int dms_is_dms(const uint8_t *data, size_t len) {
    if (!data || len < DMS_HEADLEN) return 0;
    if (data[0] != 'D' || data[1] != 'M' || data[2] != 'S' || data[3] != '!') return 0;
    uint16_t hcrc = (uint16_t)((data[DMS_HEADLEN - 2] << 8) | data[DMS_HEADLEN - 1]);
    return (hcrc == dms_crc16(data + 4, DMS_HEADLEN - 6));
}

dms_error_t dms_read_info(const uint8_t *data, size_t len, dms_info_t *info) {
    if (!data || !info) return DMS_ERR_NOMEMORY;
    memset(info, 0, sizeof(*info));
    if (len < DMS_HEADLEN) return DMS_ERR_SHORT_READ;

    const uint8_t *b = data;
    if (b[0] != 'D' || b[1] != 'M' || b[2] != 'S' || b[3] != '!')
        return DMS_ERR_NOT_DMS;

    uint16_t hcrc = (uint16_t)((b[DMS_HEADLEN - 2] << 8) | b[DMS_HEADLEN - 1]);
    if (hcrc != dms_crc16(b + 4, DMS_HEADLEN - 6))
        return DMS_ERR_HEADER_CRC;

    info->geninfo         = (uint16_t)((b[10] << 8) | b[11]);
    info->creation_date   = (uint32_t)((uint32_t)b[12] << 24 | (uint32_t)b[13] << 16 |
                                       (uint32_t)b[14] << 8  | (uint32_t)b[15]);
    info->track_lo        = (uint16_t)((b[16] << 8) | b[17]);
    info->track_hi        = (uint16_t)((b[18] << 8) | b[19]);
    info->packed_size     = (uint32_t)((uint32_t)b[21] << 16 | (uint32_t)b[22] << 8 | (uint32_t)b[23]);
    info->unpacked_size   = (uint32_t)((uint32_t)b[25] << 16 | (uint32_t)b[26] << 8 | (uint32_t)b[27]);
    info->creator_version = (uint16_t)((b[46] << 8) | b[47]);
    info->disk_type       = (uint16_t)((b[50] << 8) | b[51]);
    info->comp_mode       = (uint16_t)((b[52] << 8) | b[53]);

    return DMS_OK;
}

dms_error_t dms_unpack(const uint8_t *dms_data, size_t dms_len,
                       uint8_t *adf_out, size_t adf_cap,
                       size_t *adf_written,
                       const char *password,
                       int override_errors,
                       dms_info_t *info,
                       dms_track_cb_t track_cb,
                       void *track_cb_user)
{
    if (!dms_data || dms_len < DMS_HEADLEN || !adf_out)
        return DMS_ERR_NOMEMORY;

    /* Parse file header */
    dms_info_t local_info;
    dms_info_t *pi = info ? info : &local_info;
    dms_error_t err = dms_read_info(dms_data, dms_len, pi);
    if (err != DMS_OK) return err;

    if (pi->disk_type == DMS_DISK_FMS)
        return DMS_ERR_FMS;

    /* Check encryption */
    int encrypted = (pi->geninfo & DMS_INFO_ENCRYPTED) != 0;
    uint16_t pwd_crc = 0;
    if (encrypted) {
        if (!password || !*password)
            return DMS_ERR_NO_PASSWD;
        pwd_crc = compute_pwd_crc(password);
    }

    /* Allocate context */
    dms_ctx_t *ctx = (dms_ctx_t *)calloc(1, sizeof(dms_ctx_t));
    if (!ctx) return DMS_ERR_NOMEMORY;

    ctx->b1 = (uint8_t *)calloc(DMS_TRACK_BUFFER_LEN, 1);
    ctx->b2 = (uint8_t *)calloc(DMS_TRACK_BUFFER_LEN, 1);
    ctx->text = (uint8_t *)calloc(DMS_TEMP_BUFFER_LEN, 1);
    if (!ctx->b1 || !ctx->b2 || !ctx->text) {
        free(ctx->b1); free(ctx->b2); free(ctx->text); free(ctx);
        return DMS_ERR_NOMEMORY;
    }

    ctx->in_buf = dms_data;
    ctx->in_len = dms_len;
    ctx->in_pos = DMS_HEADLEN;
    ctx->out_buf = adf_out;
    ctx->out_cap = adf_cap;
    ctx->out_pos = 0;
    ctx->override_errors = override_errors;
    ctx->track_cb = track_cb;
    ctx->track_cb_user = track_cb_user;
    ctx->pwd_crc = pwd_crc;

    init_decrunchers(ctx);

    dms_error_t ret = DMS_OK;

    /* Process tracks */
    for (;;) {
        /* Read track header */
        size_t rd = mem_read(ctx, ctx->b1, DMS_THLEN);
        if (rd == 0) { ret = DMS_OK; break; }
        if (rd != DMS_THLEN) { ret = DMS_ERR_SHORT_READ; break; }

        if (ctx->b1[0] != 'T' || ctx->b1[1] != 'R') {
            /* Not a track header — assume end of valid data (as xDMS does) */
            ret = DMS_OK;
            break;
        }

        uint16_t th_hcrc = (uint16_t)((ctx->b1[DMS_THLEN - 2] << 8) | ctx->b1[DMS_THLEN - 1]);
        if (dms_crc16(ctx->b1, DMS_THLEN - 2) != th_hcrc) {
            if (override_errors) continue;
            ret = DMS_ERR_TRACK_HCRC; break;
        }

        uint16_t number  = (uint16_t)((ctx->b1[2] << 8) | ctx->b1[3]);
        uint16_t pklen1  = (uint16_t)((ctx->b1[6] << 8) | ctx->b1[7]);
        uint16_t pklen2  = (uint16_t)((ctx->b1[8] << 8) | ctx->b1[9]);
        uint16_t unpklen = (uint16_t)((ctx->b1[10] << 8) | ctx->b1[11]);
        uint8_t  flags   = ctx->b1[12];
        uint8_t  cmode   = ctx->b1[13];
        uint16_t usum    = (uint16_t)((ctx->b1[14] << 8) | ctx->b1[15]);
        uint16_t dcrc    = (uint16_t)((ctx->b1[16] << 8) | ctx->b1[17]);

        if (pklen1 > DMS_TRACK_BUFFER_LEN || pklen2 > DMS_TRACK_BUFFER_LEN ||
            unpklen > DMS_TRACK_BUFFER_LEN) {
            if (override_errors) { ctx->in_pos += pklen1; continue; }
            ret = DMS_ERR_BIG_TRACK; break;
        }

        /* Read packed track data */
        rd = mem_read(ctx, ctx->b1, pklen1);
        if (rd != pklen1) {
            if (override_errors) continue;
            ret = DMS_ERR_SHORT_READ; break;
        }

        /* Verify data CRC */
        int crc_ok = (dms_crc16(ctx->b1, pklen1) == dcrc);
        if (!crc_ok && !override_errors) { ret = DMS_ERR_TRACK_DCRC; break; }

        /* Decrypt if needed (track 80 = FILEID.DIZ is never encrypted) */
        if (encrypted && number != 80)
            dms_decrypt(ctx, ctx->b1, pklen1);

        /* Track 80 = FILEID.DIZ, 0xFFFF = Banner */
        if (number == 0xffff) {
            /* Banner */
            memset(ctx->b2, 0, unpklen);
            uint16_t r = unpack_track(ctx, ctx->b1, ctx->b2, pklen2, unpklen, cmode, flags);
            if (r == DMS_OK && pi->banner == NULL) {
                pi->banner = (uint8_t *)malloc(unpklen + 1);
                if (pi->banner) {
                    memcpy(pi->banner, ctx->b2, unpklen);
                    pi->banner[unpklen] = 0;
                    pi->banner_len = unpklen;
                }
            }
            continue;
        }

        if (number == 80) {
            /* FILEID.DIZ */
            memset(ctx->b2, 0, unpklen);
            uint16_t r = unpack_track(ctx, ctx->b1, ctx->b2, pklen2, unpklen, cmode, flags);
            if (r == DMS_OK && pi->fileid_diz == NULL) {
                pi->fileid_diz = (uint8_t *)malloc(unpklen + 1);
                if (pi->fileid_diz) {
                    memcpy(pi->fileid_diz, ctx->b2, unpklen);
                    pi->fileid_diz[unpklen] = 0;
                    pi->fileid_diz_len = unpklen;
                }
            }
            continue;
        }

        /* Skip fake boot blocks (track 0 with only 1024 bytes) */
        if (number == 0 && unpklen <= 2048) continue;

        /* Normal data track — decompress */
        if (number < 80 && unpklen > 2048) {
            memset(ctx->b2, 0, unpklen);
            uint16_t r = unpack_track(ctx, ctx->b1, ctx->b2, pklen2, unpklen, cmode, flags);

            int checksum_ok = 1;
            if (r != DMS_OK) {
                if (override_errors) { /* continue */ }
                else if (encrypted) { ret = DMS_ERR_BAD_PASSWD; break; }
                else { ret = (dms_error_t)r; break; }
            } else {
                checksum_ok = (usum == dms_checksum(ctx->b2, unpklen));
                if (!checksum_ok && !override_errors) {
                    ret = encrypted ? DMS_ERR_BAD_PASSWD : DMS_ERR_CHECKSUM;
                    break;
                }
            }

            /* Track callback */
            if (track_cb) {
                dms_track_info_t ti;
                memset(&ti, 0, sizeof(ti));
                ti.number      = number;
                ti.packed_len  = pklen1;
                ti.unpacked_len = unpklen;
                ti.comp_mode   = cmode;
                ti.flags       = flags;
                ti.checksum    = usum;
                ti.header_crc  = th_hcrc;
                ti.data_crc    = dcrc;
                ti.crc_ok      = crc_ok;
                ti.checksum_ok = checksum_ok;
                track_cb(&ti, track_cb_user);
            }

            /* Write to output */
            if (ctx->out_pos + unpklen > ctx->out_cap) {
                if (!override_errors) { ret = DMS_ERR_OUTPUT_FULL; break; }
            } else {
                memcpy(ctx->out_buf + ctx->out_pos, ctx->b2, unpklen);
                ctx->out_pos += unpklen;
            }
        }
    }

    if (adf_written) *adf_written = ctx->out_pos;

    free(ctx->b1);
    free(ctx->b2);
    free(ctx->text);
    free(ctx);

    return ret;
}

void dms_info_free(dms_info_t *info) {
    if (!info) return;
    free(info->banner);
    info->banner = NULL;
    info->banner_len = 0;
    free(info->fileid_diz);
    info->fileid_diz = NULL;
    info->fileid_diz_len = 0;
}

const char *dms_disk_type_name(uint16_t disk_type) {
    switch (disk_type) {
    case 0: case 1: return "AmigaOS 1.x OFS";
    case 2: return "AmigaOS 2.0+ FFS";
    case 3: return "AmigaOS 3.0 OFS/International";
    case 4: return "AmigaOS 3.0 FFS/International";
    case 5: return "AmigaOS 3.0 OFS/DirCache";
    case 6: return "AmigaOS 3.0 FFS/DirCache";
    case 7: return "FMS System File";
    default: return "Unknown";
    }
}

const char *dms_comp_mode_name(uint16_t comp_mode) {
    switch (comp_mode) {
    case 0: return "NOCOMP";
    case 1: return "SIMPLE";
    case 2: return "QUICK";
    case 3: return "MEDIUM";
    case 4: return "DEEP";
    case 5: return "HEAVY1";
    case 6: return "HEAVY2";
    default: return "Unknown";
    }
}

const char *dms_error_string(dms_error_t err) {
    switch (err) {
    case DMS_OK:              return "OK";
    case DMS_FILE_END:        return "End of file";
    case DMS_ERR_NOMEMORY:    return "Out of memory";
    case DMS_ERR_NOT_DMS:     return "Not a DMS file";
    case DMS_ERR_SHORT_READ:  return "Unexpected end of data";
    case DMS_ERR_HEADER_CRC:  return "File header CRC error";
    case DMS_ERR_NOT_TRACK:   return "Invalid track header";
    case DMS_ERR_BIG_TRACK:   return "Track data too large";
    case DMS_ERR_TRACK_HCRC:  return "Track header CRC error";
    case DMS_ERR_TRACK_DCRC:  return "Track data CRC error";
    case DMS_ERR_CHECKSUM:    return "Track checksum error";
    case DMS_ERR_BAD_DECOMP:  return "Decompression error";
    case DMS_ERR_UNKN_MODE:   return "Unknown compression mode";
    case DMS_ERR_NO_PASSWD:   return "Encrypted, no password given";
    case DMS_ERR_BAD_PASSWD:  return "Wrong password";
    case DMS_ERR_FMS:         return "FMS archive (not a disk image)";
    case DMS_ERR_OUTPUT_FULL: return "Output buffer full";
    default:                  return "Unknown error";
    }
}

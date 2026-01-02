#ifndef UFT_GCRRAW_H
#define UFT_GCRRAW_H

/*
 * UFT (Universal Floppy Tool) - Generic RAW GCR Bitcell - v2.8.7
 *
 * Target systems:
 *  - Commodore 1541 / 1571
 *  - Apple II (variant)
 *
 * Purpose:
 *  - Pipeline layer between FLUX and logical decoders
 *  - Operates on normalized GCR bitcells / nibbles
 *
 * This module:
 *  - READ: ingest raw GCR bitcell stream
 *  - DECODE: optional 5-to-4 GCR decoding (no sync guessing)
 *  - CONVERT: export raw bitcells or decoded bytes
 *
 * NOTE:
 *  No track layout, no sector map, no DOS assumptions.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_gcr_rc {
    UFT_GCR_SUCCESS   = 0,
    UFT_GCR_ERR_ARG   = -1,
    UFT_GCR_ERR_NOMEM = -2,
    UFT_GCR_ERR_RANGE = -3
} uft_gcr_rc_t;

typedef struct uft_gcr_ctx {
    uint8_t* bitcells;     /* 0/1 bit stream */
    size_t   bit_count;

    uint8_t* decoded;     /* decoded 4-bit nibbles packed as bytes */
    size_t   decoded_len;
} uft_gcr_ctx_t;

/* Init */
int uft_gcr_init(uft_gcr_ctx_t* ctx);

/* Load raw bitcells */
int uft_gcr_load_bits(uft_gcr_ctx_t* ctx, const uint8_t* bits, size_t bit_count);

/* Decode 5-bit GCR to 4-bit nibbles */
int uft_gcr_decode(uft_gcr_ctx_t* ctx);

/* Export raw bitcells */
int uft_gcr_to_raw_bits(uft_gcr_ctx_t* ctx, const char* output_path);

/* Export decoded bytes */
int uft_gcr_to_bytes(uft_gcr_ctx_t* ctx, const char* output_path);

/* Close */
void uft_gcr_close(uft_gcr_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GCRRAW_H */

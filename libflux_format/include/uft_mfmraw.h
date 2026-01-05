#ifndef UFT_MFMRAW_H
#define UFT_MFMRAW_H

/*
 * UFT (Universal Floppy Tool) - Generic RAW MFM Bitcell - v2.8.7
 *
 * Purpose:
 *  - Decoder/Encoder pre-stage between FLUX and SECTOR layers
 *  - Accepts normalized flux deltas or raw bitcells
 *
 * This module:
 *  - READ: ingest bitcell stream (0/1 timing-normalized)
 *  - DECODE: optional MFM decode to bytes (no sector semantics)
 *  - CONVERT: export raw bitcells or decoded byte stream
 *
 * NOTE:
 *  This is intentionally generic and does NOT guess sector layout.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_mfm_rc {
    UFT_MFM_SUCCESS    = 0,
    UFT_MFM_ERR_ARG    = -1,
    UFT_MFM_ERR_NOMEM  = -2,
    UFT_MFM_ERR_RANGE  = -3
} uft_mfm_rc_t;

typedef struct uft_mfm_ctx {
    uint8_t* bitcells;     /* 0/1 stream */
    size_t   bit_count;

    uint8_t* decoded;     /* decoded bytes (optional) */
    size_t   decoded_len;
} uft_mfm_ctx_t;

/* Initialize context */
int mfm_init(uft_mfm_ctx_t* ctx);

/* Load raw bitcell stream */
int mfm_load_bits(uft_mfm_ctx_t* ctx, const uint8_t* bits, size_t bit_count);

/* Decode MFM bitcells to bytes (no sync search) */
int mfm_decode(uft_mfm_ctx_t* ctx);

/* Export raw bitcells */
int mfm_to_raw_bits(uft_mfm_ctx_t* ctx, const char* output_path);

/* Export decoded bytes */
int mfm_to_bytes(uft_mfm_ctx_t* ctx, const char* output_path);

/* Close */
void mfm_close(uft_mfm_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFMRAW_H */

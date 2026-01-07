#ifndef UFT_HFE_H
#define UFT_HFE_H

/*
 * UFT (Universal Floppy Tool) - HFE (UFT HFE Format) - v2.8.7
 *
 * HFE is a track-oriented container used by the UFT HFE.
 * It stores:
 *  - disk geometry
 *  - per-track bitstreams (MFM/FM encoded)
 *
 * This module focuses on:
 *  - READ: container parsing + track inventory
 *  - FLUX-READY: expose per-track bitstream blocks
 *  - CONVERT: export track bitstreams to a generic raw-bitstream dump
 *
 * NOTE:
 *  Sector-level READ/WRITE is not always meaningful for HFE,
 *  because data is stored as encoded bitstreams.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_hfe_rc {
    UFT_HFE_SUCCESS        = 0,
    UFT_HFE_ERR_ARG        = -1,
    UFT_HFE_ERR_IO         = -2,
    UFT_HFE_ERR_FORMAT     = -3,
    UFT_HFE_ERR_UNSUPPORTED= -4
} uft_hfe_rc_t;

#pragma pack(push, 1)
typedef struct uft_hfe_header {
    char     sig[8];        /* "HXCPICFE" */
    uint8_t  format_rev;
    uint8_t  tracks;
    uint8_t  sides;
    uint8_t  track_encoding;
    uint16_t bit_rate;
    uint16_t rpm;
    uint8_t  iface_mode;
    uint8_t  reserved[9];
    uint16_t track_list_offset;
} uft_hfe_header_t;

typedef struct uft_hfe_track_desc {
    uint16_t offset;
    uint16_t length;
} uft_hfe_track_desc_t;
#pragma pack(pop)

typedef struct uft_hfe_track {
    uint8_t  track;
    uint8_t  side;
    uint32_t bit_count;
    uint8_t* bitstream;
} uft_hfe_track_t;

typedef struct uft_hfe_ctx {
    uft_hfe_header_t hdr;
    uft_hfe_track_desc_t* track_table;
    uft_hfe_track_t* tracks;
    size_t track_count;
    char* path;
} uft_hfe_ctx_t;

/* Detect HFE */
bool uft_hfe_detect(const uint8_t* buffer, size_t size);

/* Open and parse */
int uft_hfe_open(uft_hfe_ctx_t* ctx, const char* path);

/* Read raw track bitstream (Flux-ready) */
int uft_hfe_read_track(uft_hfe_ctx_t* ctx,
                   uint8_t track,
                   uint8_t side,
                   uint8_t** out_bits,
                   uint32_t* out_bit_count);

/* Convert to generic raw bitstream dump */
int uft_hfe_to_raw_bits(uft_hfe_ctx_t* ctx, const char* output_path);

/* Close */
void uft_hfe_close(uft_hfe_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_H */

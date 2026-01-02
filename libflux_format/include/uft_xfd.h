#ifndef UFT_XFD_H
#define UFT_XFD_H

/*
 * UFT (Universal Floppy Tool) - XFD (Atari 8-bit raw disk image) - v2.8.7
 *
 * XFD is a "no header" raw sector dump used by Atari 8-bit tooling.
 * It is essentially like ATR data area, but WITHOUT the 16-byte ATR header.
 *
 * Common geometries (bytes) seen in the wild:
 *  - 90KB:   720 * 128  =  92160  (SS/SD)
 *  - 130KB:  1040* 128  = 133120  (SS/ED, mixed)
 *  - 180KB:  1440* 128  = 184320  (SS/DD)
 *  - 360KB:  2880* 128  = 368640  (DS/DD)
 *
 * Sector numbering in Atari images is typically linear (1..N).
 * There is no stable CHS mapping stored in XFD; CHS access here is a
 * best-effort mapping via provided geometry.
 *
 * This module supports:
 *  - READ/WRITE: linear sector access + CHS mapping when geometry known
 *  - CONVERT: to/from raw sector stream (XFD already is raw)
 *  - FLUX-READY: metadata placeholders (XFD cannot encode weak/timing)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_xfd_rc {
    UFT_XFD_SUCCESS      = 0,
    UFT_XFD_ERR_ARG      = -1,
    UFT_XFD_ERR_IO       = -2,
    UFT_XFD_ERR_FORMAT   = -3,
    UFT_XFD_ERR_RANGE    = -4
} uft_xfd_rc_t;

typedef struct uft_xfd_geometry {
    uint16_t cylinders;     /* optional */
    uint8_t  heads;         /* optional */
    uint16_t spt;           /* optional */
    uint16_t sector_size;   /* usually 128, sometimes 256 */
    uint32_t total_sectors; /* derived */
} uft_xfd_geometry_t;

/* Flux-ready sector metadata placeholder */
typedef struct uft_xfd_sector_meta {
    uint8_t has_weak_bits;  /* always 0 for XFD */
    uint8_t has_timing;     /* always 0 for XFD */
    uint8_t bad_crc;        /* unknown; 0 */
    uint8_t deleted_dam;    /* not applicable; 0 */
} uft_xfd_sector_meta_t;

typedef struct uft_xfd_ctx {
    void*    fp;            /* FILE* opaque */
    bool     writable;
    uint64_t file_size;

    uft_xfd_geometry_t geom;

    char* path;
} uft_xfd_ctx_t;

/* Detect likely XFD by size + divisibility */
bool uft_xfd_detect(const uint8_t* buffer, size_t size, uft_xfd_geometry_t* out_geom);

/* Open file; infer geometry (or force) */
int uft_xfd_open(uft_xfd_ctx_t* ctx, const char* path, bool writable, const uft_xfd_geometry_t* forced);

/* Linear sector read: sector_index is 1..N (Atari convention) */
int uft_xfd_read_sector_linear(uft_xfd_ctx_t* ctx,
                           uint32_t sector_index_1based,
                           uint8_t* out_data,
                           size_t out_len,
                           uft_xfd_sector_meta_t* meta);

/* Linear sector write */
int uft_xfd_write_sector_linear(uft_xfd_ctx_t* ctx,
                            uint32_t sector_index_1based,
                            const uint8_t* in_data,
                            size_t in_len);

/* CHS access (best-effort mapping) */
int uft_xfd_read_sector(uft_xfd_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_xfd_sector_meta_t* meta);

int uft_xfd_write_sector(uft_xfd_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/* CONVERT: XFD is raw -> copy */
int uft_xfd_to_raw(uft_xfd_ctx_t* ctx, const char* output_path);
int uft_xfd_from_raw(const char* raw_path, const char* output_xfd_path, const uft_xfd_geometry_t* geom);

/* Close */
void uft_xfd_close(uft_xfd_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XFD_H */

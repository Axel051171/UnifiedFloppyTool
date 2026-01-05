#ifndef UFT_ADF_H
#define UFT_ADF_H

/*
 * UFT (Universal Floppy Tool) - ADF (Amiga Disk File) - v2.8.7 module
 *
 * ADF in practice is a raw sector dump of an Amiga floppy disk:
 *   - DD: 80 cylinders, 2 heads, 11 sectors/track, 512 bytes/sector = 901,120 bytes
 *   - HD (rare): 80 cylinders, 2 heads, 22 sectors/track, 512 bytes/sector = 1,802,240 bytes
 *
 * Important:
 *  - ADF does not store MFM sync words, checksum words, or weak-bit artifacts.
 *  - Any copy-protection relying on timing/weak bits is not representable in plain ADF.
 *  - "Flux-ready" here means we keep uniform metadata placeholders in the API.
 *
 * Core functions:
 *  - READ: CHS sector access (track=cylinder, head, sector 1..SPT)
 *  - WRITE: in-place sector modifications
 *  - CONVERT: to/from raw sector stream (ADF is effectively already raw)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_adf_rc {
    UFT_ADF_SUCCESS      = 0,
    UFT_ADF_ERR_ARG      = -1,
    UFT_ADF_ERR_IO       = -2,
    UFT_ADF_ERR_NOMEM    = -3,
    UFT_ADF_ERR_FORMAT   = -4,
    UFT_ADF_ERR_RANGE    = -5
} uft_adf_rc_t;

typedef struct uft_adf_geometry {
    uint16_t cylinders;      /* typically 80 */
    uint8_t  heads;          /* typically 2 */
    uint16_t spt;            /* 11 (DD) or 22 (HD) */
    uint16_t sector_size;    /* 512 */
} uft_adf_geometry_t;

/* Flux-ready sector metadata placeholder */
typedef struct uft_adf_sector_meta {
    uint8_t has_weak_bits;   /* always 0 for ADF */
    uint8_t has_timing;      /* always 0 for ADF */
    uint8_t bad_crc;         /* unknown; 0 */
    uint8_t deleted_dam;     /* not applicable; 0 */
} uft_adf_sector_meta_t;

typedef struct uft_adf_ctx {
    void*    fp;             /* FILE* opaque */
    bool     writable;
    uint64_t file_size;
    uft_adf_geometry_t geom;

    uint32_t bytes_per_track;
    uint64_t bytes_per_cyl;
} uft_adf_ctx_t;

/* Detect ADF by size heuristics */
bool uft_adf_detect(const uint8_t* buffer, size_t size, uft_adf_geometry_t* out_geom);

/* Open file and derive geometry */
int uft_adf_open(uft_adf_ctx_t* ctx, const char* path, bool writable, const uft_adf_geometry_t* forced);

/* Read sector by CHS (sector is 1..spt) */
int uft_adf_read_sector(uft_adf_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_adf_sector_meta_t* meta);

/* Write sector by CHS */
int uft_adf_write_sector(uft_adf_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/* Convert to raw sector stream (file copy for ADF) */
int uft_adf_to_raw(uft_adf_ctx_t* ctx, const char* output_path);

/* Build ADF from raw stream (size must match geom) */
int uft_adf_from_raw(const char* raw_path, const char* output_adf_path, const uft_adf_geometry_t* geom);

/* Close */
void uft_adf_close(uft_adf_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADF_H */

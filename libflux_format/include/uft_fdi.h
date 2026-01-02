#ifndef UFT_FDI_H
#define UFT_FDI_H

/*
 * UFT (Universal Floppy Tool) - FDI (Flexible Disk Image) - v2.8.7
 *
 * FDI is a semi-raw container used by several emulators/tools that stores:
 *  - disk geometry
 *  - per-track data blocks
 *  - optional timing/flags
 *
 * Design goals for UFT:
 *  - READ: parse header, track table, variable sector sizes
 *  - WRITE: modify sector payloads, preserve metadata
 *  - CONVERT: export to raw sector stream
 *  - FLUX-READY: expose per-track timing placeholders and sector flags
 *
 * Note:
 *  FDI exists in several revisions. This module targets the common layout
 *  used by PC/Atari/Amiga tooling (header + track descriptors).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_fdi_rc {
    UFT_FDI_SUCCESS      = 0,
    UFT_FDI_ERR_ARG      = -1,
    UFT_FDI_ERR_IO       = -2,
    UFT_FDI_ERR_NOMEM    = -3,
    UFT_FDI_ERR_FORMAT   = -4,
    UFT_FDI_ERR_NOTFOUND = -5,
    UFT_FDI_ERR_RANGE    = -6
} uft_fdi_rc_t;

#pragma pack(push, 1)
typedef struct uft_fdi_header {
    char     sig[3];      /* "FDI" */
    uint8_t  version;
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  flags;
    uint32_t track_table_off;
} uft_fdi_header_t;

typedef struct uft_fdi_track_desc {
    uint32_t offset;
    uint32_t length;
} uft_fdi_track_desc_t;

typedef struct uft_fdi_sector_desc {
    uint8_t  C,H,R,N;
    uint8_t  st1, st2;
    uint16_t size;
} uft_fdi_sector_desc_t;
#pragma pack(pop)

typedef struct uft_fdi_sector_meta {
    uint8_t deleted_dam;
    uint8_t bad_crc;
    uint8_t has_timing;
    uint8_t has_weak_bits;
} uft_fdi_sector_meta_t;

typedef struct uft_fdi_sector {
    uft_fdi_sector_desc_t id;
    uint8_t* data;
} uft_fdi_sector_t;

typedef struct uft_fdi_track {
    uint16_t cyl;
    uint8_t  head;
    uint8_t  nsec;
    uft_fdi_sector_t* sectors;

    uint32_t track_time_ns; /* placeholder */
} uft_fdi_track_t;

typedef struct uft_fdi_ctx {
    uft_fdi_header_t hdr;

    uft_fdi_track_desc_t* track_table;
    size_t track_table_count;

    uft_fdi_track_t* tracks;
    size_t track_count;

    char* path;
    bool writable;
} uft_fdi_ctx_t;

/* Detect FDI by signature */
bool uft_fdi_detect(const uint8_t* buffer, size_t size);

/* Open and parse */
int uft_fdi_open(uft_fdi_ctx_t* ctx, const char* path, bool writable);

/* Read sector by CHS */
int uft_fdi_read_sector(uft_fdi_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_fdi_sector_meta_t* meta);

/* Write sector */
int uft_fdi_write_sector(uft_fdi_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/* Convert to raw */
int uft_fdi_to_raw(uft_fdi_ctx_t* ctx, const char* output_path);

/* Close */
void uft_fdi_close(uft_fdi_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDI_H */

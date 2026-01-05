#ifndef UFT_DSK_H
#define UFT_DSK_H

/*
 * UFT (Universal Floppy Tool) - DSK (CPC / Generic DSK) - v2.8.7
 *
 * Supports:
 *  - Standard DSK ("MV - CPCEMU Disk-File\r\nDisk-Info\r\n")
 *  - Extended DSK ("EXTENDED CPC DSK File\r\nDisk-Info\r\n")
 *
 * READ:
 *  - Parse disk + track headers
 *  - Variable sector sizes per track
 *
 * WRITE:
 *  - Modify sector payloads
 *  - Update track size tables (extended)
 *
 * CONVERT:
 *  - Export to raw sector stream (track order)
 *
 * FLUX-READY:
 *  - Preserves per-sector flags (deleted DAM / CRC)
 *  - Timing/weak-bit placeholders provided
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_dsk_rc {
    UFT_DSK_SUCCESS      = 0,
    UFT_DSK_ERR_ARG      = -1,
    UFT_DSK_ERR_IO       = -2,
    UFT_DSK_ERR_NOMEM    = -3,
    UFT_DSK_ERR_FORMAT   = -4,
    UFT_DSK_ERR_NOTFOUND = -5,
    UFT_DSK_ERR_RANGE    = -6
} uft_dsk_rc_t;

#pragma pack(push, 1)
typedef struct uft_dsk_disk_hdr {
    char     magic[34];
    uint8_t  tracks;
    uint8_t  sides;
    uint16_t track_size; /* standard DSK only */
} uft_dsk_disk_hdr_t;

typedef struct uft_dsk_track_hdr {
    char     magic[12]; /* "Track-Info\r\n" */
    uint8_t  track;
    uint8_t  side;
    uint16_t sector_size; /* 128 << n */
    uint8_t  nsec;
    uint8_t  gap3;
    uint8_t  filler;
} uft_dsk_track_hdr_t;

typedef struct uft_dsk_sector_info {
    uint8_t  C,H,R,N;
    uint8_t  st1, st2;
    uint16_t size;
} uft_dsk_sector_info_t;
#pragma pack(pop)

typedef struct uft_dsk_sector_meta {
    uint8_t deleted_dam;
    uint8_t bad_crc;
    uint8_t has_timing;
    uint8_t has_weak_bits;
} uft_dsk_sector_meta_t;

typedef struct uft_dsk_sector {
    uft_dsk_sector_info_t id;
    uint8_t* data;
} uft_dsk_sector_t;

typedef struct uft_dsk_track {
    uint8_t track;
    uint8_t side;
    uint8_t nsec;
    uft_dsk_sector_t* sectors;
} uft_dsk_track_t;

typedef struct uft_dsk_ctx {
    uft_dsk_disk_hdr_t dh;
    bool extended;

    uft_dsk_track_t* tracks;
    size_t track_count;

    char* path;
    bool writable;
} uft_dsk_ctx_t;

/* Detect DSK by header */
bool uft_dsk_detect(const uint8_t* buffer, size_t size);

/* Open and parse */
int uft_dsk_open(uft_dsk_ctx_t* ctx, const char* path, bool writable);

/* Read sector by CHS */
int uft_dsk_read_sector(uft_dsk_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_dsk_sector_meta_t* meta);

/* Write sector */
int uft_dsk_write_sector(uft_dsk_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/* Convert to raw */
int uft_dsk_to_raw(uft_dsk_ctx_t* ctx, const char* output_path);

/* Close */
void uft_dsk_close(uft_dsk_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DSK_H */

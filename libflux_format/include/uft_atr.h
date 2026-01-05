#ifndef UFT_ATR_H
#define UFT_ATR_H

/*
 * UFT (Universal Floppy Tool) - ATR (Atari 8-bit disk image) support
 *
 * ATR is a container with a 16-byte header and raw sector data behind it.
 * Header uses "paragraphs" (16-byte units) for image size and stores a nominal
 * sector size (usually 128 or 256).
 *
 * Important quirk:
 *   For many "double density" Atari images (nominal 256 bytes/sector), the first
 *   three sectors (boot sectors 1..3) are still 128 bytes in the image. This is
 *   consistent with Atari SIO boot behavior and commonly documented.
 *
 * This module aims to be:
 *   - Read/Write capable (in-place sector updates)
 *   - Conversion friendly (iterators + linear sector mapping)
 *   - "Flux tauglich" at the UFT layer: we provide a per-sector iterator that
 *     can feed a higher layer that actually synthesizes MFM/FM bitcells/flux.
 *
 * References (format notes / header):
 *   - Atarimax ATR format description
 *   - Archiveteam "Just Solve the File Format Problem" (ATR)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    UFT_ATR_OK            = 0,
    UFT_ATR_EINVAL        = -1,
    UFT_ATR_EIO           = -2,
    UFT_ATR_ENOMEM        = -3,
    UFT_ATR_EUNSUPPORTED  = -4,
    UFT_ATR_ECORRUPT      = -5,
    UFT_ATR_ENOTFOUND     = -6
};

#pragma pack(push, 1)
typedef struct uft_atr_header {
    uint16_t magic;        /* 0x0296 little-endian */
    uint16_t pars_lo;      /* size of image data in 16-byte paragraphs (low) */
    uint16_t sec_size;     /* nominal sector size: 128 or 256 (or other) */
    uint16_t pars_hi;      /* high part of paragraphs (rev >= 3.0) */
    uint8_t  flags;        /* optional flags (varies by tool); safe to ignore */
    uint8_t  reserved[7];  /* may contain CRC/unused depending on tool */
} uft_atr_header_t;
#pragma pack(pop)

/* Best-effort geometry derived from total sector count. */
typedef struct uft_atr_geometry {
    uint16_t cylinders;    /* tracks per side (40/80 common) */
    uint8_t  heads;        /* 1 or 2 */
    uint16_t spt;          /* sectors per track (18 or 26 common) */
} uft_atr_geometry_t;

typedef struct uft_atr_ctx {
    void*    fp;                 /* FILE* opaque */
    uint64_t file_size;

    uft_atr_header_t hdr;

    /* Derived */
    uint64_t data_offset;        /* usually 16 */
    uint32_t nominal_sec_size;   /* hdr.sec_size */
    uint32_t boot_sec_size;      /* usually 128 when nominal is 256 */
    uint32_t max_sec_size;       /* max(boot, nominal) */

    uint32_t total_sectors;      /* number of addressable sectors in the image */
    uft_atr_geometry_t geom;     /* best-effort CHS mapping */
    bool has_short_boot;         /* sectors 1..3 are short (128) */
    bool writable;
} uft_atr_ctx_t;

/* Callback for iteration (conversion/flux pipelines). Return false to stop. */
typedef bool (*uft_atr_sector_cb)(void* user,
                                 uint16_t cyl, uint8_t head, uint16_t sec_id,
                                 uint32_t byte_len,
                                 uint8_t deleted_dam,   /* always 0 for ATR */
                                 uint8_t bad_crc,       /* unknown in ATR; 0 */
                                 const uint8_t* data);

/* Detect ATR by header magic and basic sanity checks. */
bool uft_atr_detect(const char* path);

/*
 * Open ATR.
 *  - writable=true opens with "rb+" (fails if not possible)
 *  - writable=false opens with "rb"
 */
int uft_atr_open(uft_atr_ctx_t* ctx, const char* path, bool writable);

/*
 * Read sector by CHS.
 * sector_id is 1-based within track.
 *
 * Returns bytes read (128 or 256 typically) or negative error.
 */
int uft_atr_read_sector(uft_atr_ctx_t* ctx,
                        uint16_t cylinder,
                        uint8_t head,
                        uint16_t sector_id,
                        uint8_t* buf,
                        size_t buf_len);

/*
 * Write sector by CHS.
 * data_len must match the sector's actual length (boot 128 vs nominal).
 *
 * Returns bytes written or negative error.
 */
int uft_atr_write_sector(uft_atr_ctx_t* ctx,
                         uint16_t cylinder,
                         uint8_t head,
                         uint16_t sector_id,
                         const uint8_t* data,
                         size_t data_len);

/*
 * Iterate all sectors in CHS order (track-major).
 * This is the key hook for "convert / flux" pipelines.
 */
int uft_atr_iterate_sectors(uft_atr_ctx_t* ctx, uft_atr_sector_cb cb, void* user);

/*
 * Convert ATR -> raw linear sector dump (XFD-like).
 * Writes only the data area (no ATR header).
 *
 * Returns 0 on success.
 */
int uft_atr_convert_to_raw(uft_atr_ctx_t* ctx, const char* out_path);

/* Close/free. Safe multiple times. */
void uft_atr_close(uft_atr_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATR_H */

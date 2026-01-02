#ifndef UFT_IMG_H
#define UFT_IMG_H

/*
 * UFT (Universal Floppy Tool) - IMG (Raw PC Disk Image) - v2.8.7 module
 *
 * Scope:
 *  - READ/WRITE of raw sector images (.img/.ima/.vfd)
 *  - CONVERT to/from raw-sector streams
 *  - FLUX-READY abstraction hooks (metadata placeholders)
 *
 * Reality check:
 *  - IMG is *just* linear sector bytes. There is no on-disk header, no CRC info,
 *    no weak-bit/timing information. "Flux-ready" here means: the API exposes
 *    a place to carry per-sector metadata so higher layers can transport flux
 *    artifacts if they exist (they don't in pure IMG).
 *
 * Supported standard geometries (PC):
 *  - 360KB  (40c,2h, 9spt,512)
 *  - 720KB  (80c,2h, 9spt,512)
 *  - 1.2MB  (80c,2h,15spt,512)
 *  - 1.44MB (80c,2h,18spt,512)
 *  - 2.88MB (80c,2h,36spt,512)
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- return codes ---------- */
typedef enum uft_img_rc {
    UFT_IMG_SUCCESS      = 0,
    UFT_IMG_ERR_ARG      = -1,
    UFT_IMG_ERR_IO       = -2,
    UFT_IMG_ERR_NOMEM    = -3,
    UFT_IMG_ERR_FORMAT   = -4,
    UFT_IMG_ERR_RANGE    = -5
} uft_img_rc_t;

/* ---------- geometry ---------- */
typedef struct uft_img_geometry {
    uint16_t cylinders;          /* tracks per side */
    uint8_t  heads;              /* 1 or 2 */
    uint16_t spt;                /* sectors per track */
    uint16_t sector_size;        /* bytes per sector (512 for standard PC images) */
} uft_img_geometry_t;

/* ---------- optional sector metadata (flux-ready hook) ---------- */
typedef struct uft_img_sector_meta {
    /*
     * For IMG these are always "unknown/none", but the API is here so callers can
     * keep their pipeline shape identical across formats.
     */
    uint8_t  has_weak_bits;      /* 0 for IMG */
    uint8_t  has_timing;         /* 0 for IMG */
    uint16_t reserved;

    /* Optional weak-bit mask per byte/bit – not used for IMG. */
    const uint8_t* weak_mask;
    size_t weak_mask_len;
} uft_img_sector_meta_t;

/* ---------- context ---------- */
typedef struct uft_img_ctx {
    void*    fp;                 /* FILE* stored opaque (avoid stdio in headers) */
    bool     writable;
    uint64_t file_size;

    uft_img_geometry_t geom;

    uint32_t bytes_per_track;    /* spt * sector_size */
    uint64_t bytes_per_cyl;      /* heads * bytes_per_track */
} uft_img_ctx_t;

/*
 * Detect IMG by size-only heuristics.
 *
 * Returns true if size matches one of the supported standard geometries.
 * This function is buffer-based to match your new module convention.
 */
bool uft_img_detect(const uint8_t* buffer, size_t size, uft_img_geometry_t* out_geom);

/*
 * Open an IMG file and validate/derive geometry.
 * If 'forced' is non-NULL it must match file size exactly.
 */
int uft_img_open(uft_img_ctx_t* ctx, const char* path, bool writable, const uft_img_geometry_t* forced);

/*
 * READ: sector by CHS (track=head/cylinder naming per your convention).
 *
 * track: 0..cylinders-1
 * head : 0..heads-1
 * sector: 1..spt (1-based)
 *
 * out_data must be >= sector_size.
 * meta may be NULL (IMG provides none).
 *
 * Returns sector_size on success.
 */
int uft_img_read_sector(uft_img_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_img_sector_meta_t* meta);

/*
 * WRITE: sector by CHS.
 * in_len must equal sector_size.
 *
 * Returns sector_size on success.
 */
int uft_img_write_sector(uft_img_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/*
 * CONVERT: dump the image to a raw-sector stream (for IMG that's typically a copy).
 * Returns 0 on success.
 */
int uft_img_to_raw(uft_img_ctx_t* ctx, const char* output_path);

/*
 * CONVERT: create an IMG from a raw-sector stream.
 * raw_path must be exactly cylinders*heads*spt*sector_size bytes.
 *
 * Returns 0 on success.
 */
int uft_img_from_raw(const char* raw_path, const char* output_img_path, const uft_img_geometry_t* geom);

/* Close/cleanup. Safe to call multiple times. */
void uft_img_close(uft_img_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMG_H */

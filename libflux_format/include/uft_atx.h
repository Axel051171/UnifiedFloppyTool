#ifndef UFT_ATX_H
#define UFT_ATX_H

/*
 * UFT (Universal Floppy Tool) - ATX (Atari 8-bit protected disk image) - v2.8.7
 *
 * ATX is NOT a simple sector container.
 * It stores low-level track data including:
 *  - per-sector timing
 *  - weak / fuzzy bits
 *  - multiple reads / instability
 *
 * This module therefore:
 *  - READS and parses ATX container structures
 *  - Exposes logical sector access (best-effort)
 *  - Exposes FLUX-READY track/sector metadata
 *  - Allows WRITE of logical payloads (metadata preserved)
 *  - CONVERTS to raw sector streams (LOSSY by definition)
 *
 * Preservation truth:
 *  - Converting ATX -> IMG/RAW loses protection data.
 *  - The API makes this explicit via metadata.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_atx_rc {
    UFT_ATX_SUCCESS      = 0,
    UFT_ATX_ERR_ARG      = -1,
    UFT_ATX_ERR_IO       = -2,
    UFT_ATX_ERR_NOMEM    = -3,
    UFT_ATX_ERR_FORMAT   = -4,
    UFT_ATX_ERR_NOTFOUND = -5,
    UFT_ATX_ERR_RANGE    = -6
} uft_atx_rc_t;

#pragma pack(push, 1)
typedef struct uft_atx_header {
    char     sig[4];      /* "ATX\0" */
    uint16_t version;
    uint16_t flags;
    uint32_t image_size;
} uft_atx_header_t;
#pragma pack(pop)

/* Weak bit run (bit-level instability) */
typedef struct uft_atx_weak_run {
    uint32_t bit_offset;
    uint32_t bit_length;
} uft_atx_weak_run_t;

/* Flux/timing metadata per sector */
typedef struct uft_atx_sector_meta {
    uint8_t  has_weak_bits;
    uint8_t  has_timing;
    uint8_t  bad_crc;
    uint8_t  deleted_dam;

    uint32_t cell_time_ns;      /* nominal bitcell timing */
    uft_atx_weak_run_t* weak;   /* array of weak runs */
    size_t weak_count;
} uft_atx_sector_meta_t;

typedef struct uft_atx_sector {
    uint8_t  sector_id;
    uint16_t size;
    uint8_t* data;
    uft_atx_sector_meta_t meta;
} uft_atx_sector_t;

typedef struct uft_atx_track {
    uint16_t cyl;
    uint8_t  head;
    uint8_t  nsec;
    uft_atx_sector_t* sectors;

    /* raw track timing info (for future flux writers) */
    uint32_t track_time_ns;
} uft_atx_track_t;

typedef struct uft_atx_ctx {
    uft_atx_header_t hdr;

    uft_atx_track_t* tracks;
    size_t track_count;

    uint16_t max_cyl_plus1;
    uint8_t  max_head_plus1;

    char* path;
    bool dirty;
} uft_atx_ctx_t;

/* Detect ATX by signature */
bool uft_atx_detect(const uint8_t* buffer, size_t size);

/* Open + parse ATX (full metadata preserved) */
int uft_atx_open(uft_atx_ctx_t* ctx, const char* path);

/* READ logical sector (data-only) */
int uft_atx_read_sector(uft_atx_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_atx_sector_meta_t* meta);

/* WRITE logical sector payload (metadata untouched unless provided) */
int uft_atx_write_sector(uft_atx_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/* CONVERT: export logical data to RAW (LOSSY) */
int uft_atx_to_raw(uft_atx_ctx_t* ctx, const char* output_path);

/* Close/free */
void uft_atx_close(uft_atx_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATX_H */

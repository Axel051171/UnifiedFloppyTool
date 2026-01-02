#ifndef UFT_IMD_H
#define UFT_IMD_H

/*
 * UFT (Universal Floppy Tool) - IMD (ImageDisk) - v2.8.7 module
 *
 * Goals:
 *  - READ: parse IMD container, random sector access (logical CHS + sector ID)
 *  - WRITE: modify sectors in-memory and save back to an IMD file (rebuild)
 *  - CONVERT: export/import raw sector streams
 *  - FLUX-READY: expose per-sector metadata placeholders (weak bits/timing not
 *    representable in IMD, but deleted DAM / "data error" flags are)
 *
 * IMD structure summary:
 *  - ASCII header/comment terminated by 0x1A (CTRL-Z)
 *  - Repeating track records:
 *      mode, cylinder, head_flags, sector_count, sector_size_code
 *      sector_numbering_map[sector_count]
 *      optional cyl_map[sector_count]   (head_flags bit7)
 *      optional head_map[sector_count]  (head_flags bit6)
 *      optional size_table[sector_count] (sector_size_code == 0xFF, 16-bit LE sizes)
 *      for each sector: record_type, payload (normal) or fill byte (compressed)
 *
 * Implementation strategy:
 *  - On open(): parse the full file into memory (tracks+sectors, sector payloads expanded).
 *  - read_sector(): lookup by logical CHS+sector_id and memcpy out.
 *  - write_sector(): overwrite in-memory payload, keep flags (deleted/bad crc) unless caller changes.
 *  - save(): rebuild a valid IMD stream (we write sectors as *normal* records; optional future
 *    optimization could re-compress runs back to compressed records).
 *
 * Notes:
 *  - IMD is flexible and can represent non-uniform geometries; therefore geometry is "observed".
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_imd_rc {
    UFT_IMD_SUCCESS      = 0,
    UFT_IMD_ERR_ARG      = -1,
    UFT_IMD_ERR_IO       = -2,
    UFT_IMD_ERR_NOMEM    = -3,
    UFT_IMD_ERR_FORMAT   = -4,
    UFT_IMD_ERR_NOTFOUND = -5,
    UFT_IMD_ERR_RANGE    = -6
} uft_imd_rc_t;

/* Record types in file (leading byte before each sector payload) */
typedef enum uft_imd_rec_type {
    UFT_IMD_REC_UNAVAILABLE             = 0x00,
    UFT_IMD_REC_NORMAL                  = 0x01,
    UFT_IMD_REC_COMPRESSED              = 0x02,
    UFT_IMD_REC_NORMAL_DELETED_DAM      = 0x03,
    UFT_IMD_REC_COMPRESSED_DELETED_DAM  = 0x04,
    UFT_IMD_REC_NORMAL_DATA_ERROR       = 0x05,
    UFT_IMD_REC_COMPRESSED_DATA_ERROR   = 0x06,
    UFT_IMD_REC_DELETED_DATA_ERROR      = 0x07,
    UFT_IMD_REC_COMPRESSED_DEL_DATA_ERR = 0x08
} uft_imd_rec_type_t;

/* FLUX-READY-ish sector metadata */
typedef struct uft_imd_sector_meta {
    /*
     * IMD can express:
     *  - "deleted data address mark" (deleted_dam)
     *  - "data error" (bad_crc)  (not necessarily real CRC, but "data error" flag)
     *
     * IMD cannot express:
     *  - weak bits (copy-protection artifacts)
     *  - timing info / flux transitions
     *
     * We still keep fields so the higher UFT pipeline can be uniform.
     */
    uint8_t deleted_dam;     /* 1 if sector flagged as deleted */
    uint8_t bad_crc;         /* 1 if "data error" flagged */
    uint8_t has_weak_bits;   /* always 0 in this implementation */
    uint8_t has_timing;      /* always 0 in this implementation */
} uft_imd_sector_meta_t;

/* Forward-declared context types */
typedef struct uft_imd_sector uft_imd_sector_t;
typedef struct uft_imd_track  uft_imd_track_t;

typedef struct uft_imd_ctx {
    /* original header/comment bytes (including leading "IMD " etc) up to and incl 0x1A */
    uint8_t* header;
    size_t   header_len;

    /* parsed tracks */
    uft_imd_track_t* tracks;
    size_t track_count;

    /* observed geometry (max+1) */
    uint16_t max_track_plus1; /* max logical cylinder + 1 */
    uint8_t  max_head_plus1;  /* max logical head + 1 */

    /* path for save-back convenience */
    char*    path;

    /* dirty flag for write support */
    bool dirty;
} uft_imd_ctx_t;

/*
 * detect() signature per your spec: checks the initial bytes.
 * - buffer must contain at least a small prefix (recommend 4..64KB).
 */
bool uft_imd_detect(const uint8_t* buffer, size_t size);

/* open: parse whole file into ctx (expanded payloads). */
int uft_imd_open(uft_imd_ctx_t* ctx, const char* path);

/*
 * READ: read logical sector.
 * track == cylinder, head == logical head, sector == sector ID from numbering map.
 *
 * out_data must be large enough for the sector's byte length.
 * Returns sector byte length on success.
 */
int uft_imd_read_sector(uft_imd_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_imd_sector_meta_t* meta);

/*
 * WRITE: overwrite a sector payload (keeps byte length).
 * in_len must equal the sector byte length.
 * If meta is non-NULL, deleted_dam/bad_crc will be applied to the sector flags.
 */
int uft_imd_write_sector(uft_imd_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len,
                     const uft_imd_sector_meta_t* meta);

/*
 * CONVERT: export in-memory image to a raw-sector stream.
 * Order is "track order as stored" and per-track sector order according to IMD's sector map.
 */
int uft_imd_to_raw(uft_imd_ctx_t* ctx, const char* output_path);

/*
 * CONVERT: create a *simple* IMD from a raw-sector stream for standard PC geometries.
 * - This is intentionally scoped: it writes mode=2 (MFM), no compression, no maps.
 * - Use for pipeline/testing; not a full "arbitrary IMD builder".
 */
typedef struct uft_imd_pc_geom {
    uint16_t cylinders;
    uint8_t  heads;
    uint16_t spt;
    uint16_t sector_size; /* 128..8192, power-of-two */
    uint8_t  start_sector_id; /* usually 1 */
} uft_imd_pc_geom_t;

int uft_imd_from_raw_pc(const char* raw_path,
                    const char* output_imd_path,
                    const uft_imd_pc_geom_t* geom,
                    const char* comment_ascii);

/* Save back to ctx->path (rebuilds full IMD file). */
int uft_imd_save(uft_imd_ctx_t* ctx);

/* Close/free. Safe multiple times. */
void uft_imd_close(uft_imd_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMD_H */

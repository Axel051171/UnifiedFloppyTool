#ifndef UFT_TD0_H
#define UFT_TD0_H

/*
 * UFT (Universal Floppy Tool) - TD0 (Teledisk) - v2.8.7 module
 *
 * Scope:
 *  READ:
 *   - Parse TD0 header (normal + advanced)
 *   - Decompress track data (RLE + Huffman as used by Teledisk)
 *   - Access sectors with variable sizes per track
 *
 *  WRITE:
 *   - Modify sector payloads in-memory
 *   - Rebuild TD0 image (no recompression by default; optional stub provided)
 *
 *  CONVERT:
 *   - Export to raw-sector stream (track order)
 *   - Build simple TD0 from raw-sector stream (PC geometries)
 *
 *  FLUX-READY:
 *   - Expose per-sector metadata:
 *       * deleted DAM
 *       * CRC error flags
 *       * weak-bit placeholder
 *       * timing placeholder
 *
 * Notes:
 *  - TD0 compression is historically a mix of RLE and a simple Huffman scheme.
 *  - This implementation includes a correct RLE decoder and a precise, documented
 *    Huffman *decoder interface*. The Huffman tables are parsed and applied, but
 *    compression on write is intentionally not enabled (preservation-first).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_td0_rc {
    UFT_TD0_SUCCESS      = 0,
    UFT_TD0_ERR_ARG      = -1,
    UFT_TD0_ERR_IO       = -2,
    UFT_TD0_ERR_NOMEM    = -3,
    UFT_TD0_ERR_FORMAT   = -4,
    UFT_TD0_ERR_NOTFOUND = -5,
    UFT_TD0_ERR_RANGE    = -6,
    UFT_TD0_ERR_COMPRESS = -7
} uft_td0_rc_t;

#pragma pack(push, 1)
typedef struct uft_td0_header {
    char     sig[2];          /* "TD" */
    uint8_t  ver;             /* version */
    uint8_t  data_rate;       /* encoding / rate */
    uint8_t  drive_type;
    uint8_t  stepping;
    uint8_t  dos_alloc;
    uint16_t crc;             /* header crc */
} uft_td0_header_t;
#pragma pack(pop)

typedef struct uft_td0_sector_meta {
    uint8_t deleted_dam;
    uint8_t bad_crc;
    uint8_t has_weak_bits;
    uint8_t has_timing;
} uft_td0_sector_meta_t;

typedef struct uft_td0_sector {
    uint16_t cyl;
    uint8_t  head;
    uint8_t  sec_id;
    uint16_t size;
    uint8_t  deleted_dam;
    uint8_t  bad_crc;
    uint8_t* data;
} uft_td0_sector_t;

typedef struct uft_td0_track {
    uint16_t cyl;
    uint8_t  head;
    uint8_t  nsec;
    uft_td0_sector_t* sectors;
} uft_td0_track_t;

typedef struct uft_td0_ctx {
    uft_td0_header_t hdr;

    uft_td0_track_t* tracks;
    size_t track_count;

    uint16_t max_cyl_plus1;
    uint8_t  max_head_plus1;

    char* path;
    bool dirty;
} uft_td0_ctx_t;

/* Detect TD0 from buffer prefix */
bool uft_td0_detect(const uint8_t* buffer, size_t size);

/* Open + fully parse/decompress TD0 */
int uft_td0_open(uft_td0_ctx_t* ctx, const char* path);

/* Read sector by logical CHS */
int uft_td0_read_sector(uft_td0_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_td0_sector_meta_t* meta);

/* Write sector (no recompression; payload replaced) */
int uft_td0_write_sector(uft_td0_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len,
                     const uft_td0_sector_meta_t* meta);

/* Export to raw sector stream */
int uft_td0_to_raw(uft_td0_ctx_t* ctx, const char* output_path);

/* Build simple TD0 from raw (PC geometries, uncompressed) */
typedef struct uft_td0_pc_geom {
    uint16_t cylinders;
    uint8_t  heads;
    uint16_t spt;
    uint16_t sector_size;
    uint8_t  start_sector_id;
} uft_td0_pc_geom_t;

int uft_td0_from_raw_pc(const char* raw_path,
                    const char* output_td0_path,
                    const uft_td0_pc_geom_t* geom);

/* Save back to ctx->path */
int uft_td0_save(uft_td0_ctx_t* ctx);

/* Close/free */
void uft_td0_close(uft_td0_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TD0_H */

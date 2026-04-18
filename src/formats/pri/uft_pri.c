/**
 * @file uft_pri.c
 * @brief PRI (PCE Raw Image) Plugin — MAME/MESS flux preservation
 *
 * PRI is PCE's native raw flux image format, using big-endian chunk
 * structure similar to IFF/RIFF. It stores flux-level data per track
 * with optional weak bit markers.
 *
 * File layout:
 *   "PRI " + version(BE32) + file_crc(BE32)  [12-byte header]
 *   Chunks:
 *     TEXT — Optional description text
 *     TRAK — Track header (cylinder, head, bit_rate, bit_count)
 *     DATA — Track bitstream data
 *     WEAK — Weak bit mask
 *     END  — End marker
 *
 * Each chunk: size(BE32) + id(4 bytes) + crc(BE32) + payload
 *
 * Reference: PCE source (pri-img.c), format documentation
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PRI_MAGIC           "PRI "
#define PRI_HEADER_SIZE     12
#define PRI_CHUNK_HDR_SIZE  12      /* size(4) + id(4) + crc(4) */
#define PRI_MAX_TRACKS      168
#define PRI_MAX_TRACK_BYTES (1024 * 1024)  /* 1MB per track max */

/* ============================================================================
 * Helpers
 * ============================================================================ */

static uint32_t pri_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    uint32_t    data_offset;    /* file offset of DATA payload */
    uint32_t    data_size;      /* DATA chunk payload size */
    uint32_t    bit_count;      /* bits in this track */
    uint8_t     cylinder;
    uint8_t     head;
    bool        valid;
} pri_track_info_t;

typedef struct {
    uint8_t*            file_data;
    size_t              file_size;
    uint32_t            version;
    pri_track_info_t    tracks[PRI_MAX_TRACKS];
    uint16_t            track_count;
    uint8_t             max_cyl;
    uint8_t             max_head;
} pri_data_t;

/* ============================================================================
 * Chunk scanner
 * ============================================================================ */

static bool pri_scan(const uint8_t *data, size_t size, pri_data_t *pdata)
{
    if (size < PRI_HEADER_SIZE) return false;
    if (memcmp(data, PRI_MAGIC, 4) != 0) return false;

    pdata->version = pri_be32(data + 4);

    size_t pos = PRI_HEADER_SIZE;
    int cur_cyl = -1, cur_head = -1;

    while (pos + PRI_CHUNK_HDR_SIZE <= size) {
        uint32_t chunk_size = pri_be32(data + pos);
        /* chunk id at pos+4, crc at pos+8, payload at pos+12 */
        const uint8_t *id = data + pos + 4;
        size_t payload_off = pos + PRI_CHUNK_HDR_SIZE;
        uint32_t payload_size = (chunk_size > PRI_CHUNK_HDR_SIZE) ?
                                chunk_size - PRI_CHUNK_HDR_SIZE : 0;

        if (payload_off + payload_size > size) break;

        if (memcmp(id, "TRAK", 4) == 0 && payload_size >= 8) {
            /* TRAK: cylinder(BE32) + head(BE32) + clock_rate(BE32) + bit_count(BE32) */
            cur_cyl = (int)pri_be32(data + payload_off);
            cur_head = (int)pri_be32(data + payload_off + 4);

            if (pdata->track_count < PRI_MAX_TRACKS &&
                cur_cyl < 85 && cur_head < 2) {
                pri_track_info_t *t = &pdata->tracks[pdata->track_count];
                t->cylinder = (uint8_t)cur_cyl;
                t->head = (uint8_t)cur_head;
                if (payload_size >= 16)
                    t->bit_count = pri_be32(data + payload_off + 12);
                t->valid = true;

                if (cur_cyl > pdata->max_cyl) pdata->max_cyl = (uint8_t)cur_cyl;
                if (cur_head > pdata->max_head) pdata->max_head = (uint8_t)cur_head;
            }
        } else if (memcmp(id, "DATA", 4) == 0 && pdata->track_count < PRI_MAX_TRACKS) {
            /* DATA follows TRAK */
            pri_track_info_t *t = &pdata->tracks[pdata->track_count];
            t->data_offset = (uint32_t)payload_off;
            t->data_size = payload_size;
            pdata->track_count++;
        } else if (memcmp(id, "END ", 4) == 0) {
            break;
        }

        pos += chunk_size;
        if (chunk_size == 0) break;  /* prevent infinite loop */
    }

    return pdata->track_count > 0;
}

/* ============================================================================
 * probe
 * ============================================================================ */

bool pri_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)file_size;
    if (size < PRI_HEADER_SIZE) return false;
    if (memcmp(data, PRI_MAGIC, 4) != 0) return false;

    *confidence = 95;
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t pri_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    (void)read_only;

    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data) return UFT_ERROR_FILE_OPEN;

    pri_data_t *pdata = calloc(1, sizeof(pri_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    pdata->file_data = file_data;
    pdata->file_size = file_size;

    if (!pri_scan(file_data, file_size, pdata)) {
        free(file_data);
        free(pdata);
        return UFT_ERROR_FORMAT_INVALID;
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->max_cyl + 1;
    disk->geometry.heads = pdata->max_head + 1;
    disk->geometry.sectors = 1;     /* flux = raw track */
    disk->geometry.sector_size = 0;
    disk->geometry.total_sectors = pdata->track_count;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void pri_close(uft_disk_t *disk)
{
    pri_data_t *pdata = disk->plugin_data;
    if (pdata) {
        free(pdata->file_data);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t pri_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    pri_data_t *pdata = disk->plugin_data;
    if (!pdata) return UFT_ERROR_INVALID_STATE;

    /* Find matching track */
    for (int i = 0; i < pdata->track_count; i++) {
        pri_track_info_t *t = &pdata->tracks[i];
        if (t->cylinder == cyl && t->head == head && t->valid &&
            t->data_size > 0) {

            if (t->data_offset + t->data_size > pdata->file_size)
                return UFT_ERROR_IO;

            uft_track_init(track, cyl, head);

            uint16_t chunk = (t->data_size > 65535) ?
                             65535 : (uint16_t)t->data_size;
            uft_format_add_sector(track, 0,
                                  pdata->file_data + t->data_offset,
                                  chunk, (uint8_t)cyl, (uint8_t)head);
            return UFT_OK;
        }
    }

    return UFT_ERROR_INVALID_STATE;
}

/* ============================================================================
 * write_track — DOCUMENTED NOT_IMPLEMENTED per spec §1.3 Option 1.
 *
 * PRI (PCE Raw Image, from Hampa Hug's PCE emulator) is a chunk-based
 * container for raw MFM bitstreams, with optional flux modulator-flag
 * bits tracking weak/fuzzy cells per bit position. Writing requires
 * MFM bit synthesis and PRI chunk serialization.
 *
 * Implementation steps:
 *   1. Encode sectors → MFM bits (same shared encoder KFX/MFI need).
 *   2. Build a TRAK chunk per track: 4-byte header {cyl,head,size,_}
 *      followed by the raw MFM bitstream padded to byte boundary.
 *   3. If modulator data needs preserving, emit a matching FXMD chunk
 *      with per-bit flux-modulation flags.
 *   4. Update the END chunk or rewrite the full file with recomputed
 *      CRCs on every chunk.
 *   5. Write DONE chunk and EOF.
 *
 * Estimated effort: ~180 lines (above the shared MFM encoder).
 * Blocker: shared MFM encoder not in the tree + no PRI round-trip
 * corpus to verify chunk layout.
 * Workaround: PCE accepts HFE — use HFE writer for PCE interop.
 * ============================================================================ */

static uft_error_t pri_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    (void)disk; (void)cyl; (void)head; (void)track;
    return UFT_ERROR_NOT_SUPPORTED;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

const uft_format_plugin_t uft_format_plugin_pri = {
    .name         = "PRI",
    .description  = "PCE Raw Image (MAME/MESS flux)",
    .extensions   = "pri",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX | UFT_FORMAT_CAP_VERIFY,
    .probe        = pri_probe,
    .open         = pri_open,
    .close        = pri_close,
    .read_track   = pri_read_track,
    .write_track  = pri_write_track,
    .verify_track = uft_flux_verify_track,
};

UFT_REGISTER_FORMAT_PLUGIN(pri)

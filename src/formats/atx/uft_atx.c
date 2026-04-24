/**
 * @file uft_atx.c
 * @brief ATX (Atari 8-bit Protected / VAPI) Plugin.
 *
 * ATX stores Atari 8-bit disk data with FDC status, per-sector timing,
 * weak-bit regions, and extended-sector-header info — all critical for
 * copy-protection preservation.
 *
 * File layout:
 *   [FileHeader    48 bytes]  "AT8X" + version + density + track count
 *   [TrackOffsetTable  N×4 bytes]  LE32 offsets into file, one per track
 *   [Track records]  each containing:
 *     [TrackHeader 32 bytes]  size, type, num_sectors, flags, data_offset
 *     [Chunks]  each 8-byte header (size, type, num, data) + payload:
 *       type=0x01  SectorList    — array of 8-byte ATXSectorHeader entries
 *       type=0x10  WeakBits      — weak-byte offset within a named sector
 *       type=0x11  ExtSectorHdr  — extended sector size info
 *     [Sector data]  128-byte (or 256) blocks referenced by SectorHeader.mDataOffset
 *
 * Reference: a8rawconv/diskatx.cpp by Avery Lee (GPL-2-or-later),
 * verified against VAPI specification.
 *
 * Part of M2 TA3 (MASTER_PLAN.md). See docs/A8RAWCONV_INTEGRATION_TODO.md.
 */
#include "uft/uft_format_common.h"

/*
 * ATX signature as read by uft_read_le32 on the bytes 'A','T','8','X'
 * (0x41, 0x54, 0x38, 0x58 in file order). A little-endian read yields
 * 0x58385441, NOT 0x41543858 — see KNOWN_ISSUES.md §M.-1 (closed).
 */
#define ATX_SIGNATURE          0x58385441u  /* "AT8X" as LE32 */
#define ATX_FILE_HEADER_SIZE   48
#define ATX_TRACK_HEADER_SIZE  32
#define ATX_SECTOR_HEADER_SIZE 8
#define ATX_CHUNK_HEADER_SIZE  8
#define ATX_MAX_TRACKS         42
#define ATX_MAX_SECTORS        32          /* per track, VAPI caps at ~26 */
#define ATX_SECTOR_SIZE_SD     128
#define ATX_SECTOR_SIZE_DD     256

/* Chunk types (single byte at offset 4 of each chunk header). */
#define ATX_CHUNK_SECTOR_LIST     0x01
#define ATX_CHUNK_WEAK_BITS       0x10
#define ATX_CHUNK_EXT_SECTOR_HDR  0x11

/* Track flags (uint32_t at offset 20 of TrackHeader). */
#define ATX_TRK_FLAG_MFM          0x00000002
#define ATX_TRK_FLAG_NO_SKEW      0x00000100

/* FDC status flags (ATXSectorHeader.mFDCStatus, byte 1). Not inverted. */
#define ATX_FDC_CRC_ERROR         0x08
#define ATX_FDC_LOST_DATA         0x04      /* also indicates long-sector */
#define ATX_FDC_MISSING_DATA      0x10      /* data field absent */
#define ATX_FDC_DELETED           0x20      /* deleted-data mark (0xF8) */

typedef struct {
    uint8_t  *file_data;
    size_t    file_size;
    uint16_t  version;
    uint8_t   density;
    uint16_t  sector_size;
    uint8_t   track_count;
    uint32_t  track_offsets[ATX_MAX_TRACKS];
} atx_data_t;

/* ───────────────────────── Probe ──────────────────────────────────── */

static bool atx_plugin_probe(const uint8_t *data, size_t size,
                              size_t file_size, int *confidence) {
    (void)file_size;
    if (size < ATX_FILE_HEADER_SIZE) return false;
    if (uft_read_le32(data) != ATX_SIGNATURE) return false;
    *confidence = 98;
    return true;
}

/* ───────────────────────── Open ───────────────────────────────────── */

static uft_error_t atx_open(uft_disk_t *disk, const char *path, bool ro) {
    (void)ro;
    uft_error_t err = UFT_ERROR_NO_MEMORY;
    uint8_t *raw = NULL;
    atx_data_t *p = NULL;

    size_t raw_size = 0;
    raw = uft_read_file(path, &raw_size);
    if (!raw) return UFT_ERROR_FILE_OPEN;
    if (raw_size < ATX_FILE_HEADER_SIZE) {
        err = UFT_ERROR_FORMAT_INVALID; goto fail;
    }
    if (uft_read_le32(raw) != ATX_SIGNATURE) {
        err = UFT_ERROR_FORMAT_INVALID; goto fail;
    }

    p = calloc(1, sizeof(atx_data_t));
    if (!p) goto fail;

    p->file_data = raw;
    p->file_size = raw_size;
    p->version = uft_read_le16(raw + 4);
    p->density = raw[19];
    p->sector_size = (p->density >= 2) ? ATX_SECTOR_SIZE_DD : ATX_SECTOR_SIZE_SD;
    p->track_count = raw[20];
    if (p->track_count > ATX_MAX_TRACKS) p->track_count = ATX_MAX_TRACKS;

    /* The file header (bytes 28..31) carries mTrackDataOffset — offset
     * into the file where the track-offset table begins. In practice
     * this is almost always 48 (directly after the header). */
    uint32_t tot_offset = uft_read_le32(raw + 28);
    if (tot_offset == 0) tot_offset = ATX_FILE_HEADER_SIZE;

    for (int t = 0; t < p->track_count; t++) {
        size_t entry = tot_offset + (size_t)t * 4;
        if (entry + 4 > raw_size) break;
        p->track_offsets[t] = uft_read_le32(raw + entry);
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->track_count;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;  /* Atari DOS 2 default; overridden per-track */
    disk->geometry.sector_size = p->sector_size;
    disk->geometry.total_sectors = (uint32_t)p->track_count * 18;
    return UFT_OK;

fail:
    free(p);
    free(raw);
    return err;
}

static void atx_close(uft_disk_t *disk) {
    atx_data_t *p = disk->plugin_data;
    if (p) { free(p->file_data); free(p); disk->plugin_data = NULL; }
}

/* ───────────────────────── Read track ─────────────────────────────── */

/* One-time resolution of weak-bit chunks for a sector index. */
typedef struct {
    uint8_t  sector_index;      /* index into the SectorList */
    uint16_t weak_offset;       /* byte offset of first weak byte in sector */
} atx_weak_info_t;

static uft_error_t atx_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track) {
    atx_data_t *p = disk->plugin_data;
    if (!p || !p->file_data) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl < 0 || cyl >= p->track_count)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    uint32_t trk_off = p->track_offsets[cyl];
    if (trk_off == 0 || trk_off + ATX_TRACK_HEADER_SIZE > p->file_size)
        return UFT_OK;  /* empty/missing track — not an error */

    const uint8_t *th = p->file_data + trk_off;
    uint32_t trk_size      = uft_read_le32(th + 0);
    /* th + 4: uint16_t type, 2 reserved, 1 track, 1 reserved = 8 bytes */
    uint8_t  track_num     = th[8];
    uint16_t num_sectors   = uft_read_le16(th + 10);
    uint32_t trk_flags     = uft_read_le32(th + 20);
    uint32_t data_offset   = uft_read_le32(th + 24);

    (void)track_num;
    (void)trk_flags;

    if (trk_size < ATX_TRACK_HEADER_SIZE || trk_size > p->file_size ||
        trk_off + trk_size > p->file_size) return UFT_OK;
    if (num_sectors == 0 || num_sectors > ATX_MAX_SECTORS) return UFT_OK;

    /* Scan chunks (relative to track start).  Collect SectorList,
     * WeakBits, ExtSectorHeader. */
    uint32_t ext_size_bits[ATX_MAX_SECTORS] = {0};      /* per-sector */
    atx_weak_info_t weak[ATX_MAX_SECTORS];
    int weak_count = 0;
    uint32_t sec_list_off_in_track = 0;

    uint32_t tcpos = data_offset;
    while (tcpos + ATX_CHUNK_HEADER_SIZE <= trk_size) {
        const uint8_t *ch = th + tcpos;
        uint32_t c_size   = uft_read_le32(ch + 0);
        uint8_t  c_type   = ch[4];
        uint8_t  c_num    = ch[5];
        uint16_t c_data   = uft_read_le16(ch + 6);

        if (c_size == 0) break;
        if (c_size < ATX_CHUNK_HEADER_SIZE) break;
        if (tcpos + c_size > trk_size) break;

        if (c_type == ATX_CHUNK_SECTOR_LIST) {
            /* Payload: num_sectors × ATXSectorHeader immediately after */
            if (c_size >= ATX_CHUNK_HEADER_SIZE +
                          (uint32_t)num_sectors * ATX_SECTOR_HEADER_SIZE) {
                sec_list_off_in_track = tcpos + ATX_CHUNK_HEADER_SIZE;
            }
        } else if (c_type == ATX_CHUNK_EXT_SECTOR_HDR) {
            if (c_num < ATX_MAX_SECTORS) ext_size_bits[c_num] = c_data;
        } else if (c_type == ATX_CHUNK_WEAK_BITS) {
            if (weak_count < ATX_MAX_SECTORS) {
                weak[weak_count].sector_index = c_num;
                weak[weak_count].weak_offset  = c_data;
                weak_count++;
            }
        }

        tcpos += c_size;
    }

    if (sec_list_off_in_track == 0) return UFT_OK;  /* malformed */

    /* Process each sector in the SectorList. */
    for (uint16_t s = 0; s < num_sectors && s < ATX_MAX_SECTORS; s++) {
        size_t sh_off = trk_off + sec_list_off_in_track +
                         (size_t)s * ATX_SECTOR_HEADER_SIZE;
        if (sh_off + ATX_SECTOR_HEADER_SIZE > p->file_size) break;

        const uint8_t *sh = p->file_data + sh_off;
        uint8_t  sec_index    = sh[0];
        uint8_t  fdc_status   = sh[1];
        uint16_t timing_off   = uft_read_le16(sh + 2);
        uint32_t sec_data_off = uft_read_le32(sh + 4);

        (void)timing_off;

        /* Determine sector payload size (long sectors via ext header). */
        uint16_t size_bytes = p->sector_size;
        if (fdc_status & ATX_FDC_LOST_DATA) {
            /* Extended sector — payload size is 128 << max(1, ext & 3) */
            uint32_t e = ext_size_bits[s];
            int shift = (int)(e & 3);
            if (shift < 1) shift = 1;
            size_bytes = (uint16_t)(128u << shift);
        }

        /* Data offset is relative to the TRACK start (not file start). */
        size_t abs_data = trk_off + sec_data_off;
        if (abs_data == trk_off) continue;  /* 0 = no data */
        if (abs_data + size_bytes > p->file_size) continue;

        const uint8_t *src = p->file_data + abs_data;
        uint8_t zero_buf[256] = {0};
        if (fdc_status & ATX_FDC_MISSING_DATA) {
            src = zero_buf;
            if (size_bytes > sizeof(zero_buf)) size_bytes = sizeof(zero_buf);
        }

        uft_format_add_sector(track,
            sec_index > 0 ? sec_index - 1 : 0,
            src, size_bytes,
            (uint8_t)cyl, 0);

        if (track->sector_count == 0) continue;
        uft_sector_t *last = &track->sectors[track->sector_count - 1];

        last->crc_ok   = (fdc_status & ATX_FDC_CRC_ERROR) == 0;
        last->deleted  = (fdc_status & ATX_FDC_DELETED)   != 0;
        last->data_mark = (fdc_status & ATX_FDC_DELETED) ? 0xF8u : 0xFBu;
    }

    /* Apply weak-bit annotations (after sectors are created). */
    for (int w = 0; w < weak_count; w++) {
        uint8_t idx = weak[w].sector_index;
        if (idx >= track->sector_count) continue;
        uft_sector_t *sec = &track->sectors[idx];
        sec->weak = true;
        /* TODO: allocate weak_mask and set from weak_offset..end.
         * For now just flag the sector; byte-mask population belongs
         * to a follow-up refinement that touches uft_sector_cleanup. */
        (void)weak[w].weak_offset;
    }

    return UFT_OK;
}

/* ───────────────────────── Plugin registration ────────────────────── */

/*
 * write_track is deliberately omitted: ATX encodes per-sector timing
 * anomalies, weak bits, duplicate sector IDs, and FDC-status quirks
 * that cannot be synthesised from sector payload alone. Attempting
 * to write ATX would silently lose forensic information. Prinzip 1.
 */

const uft_format_plugin_t uft_format_plugin_atx = {
    .name = "ATX", .description = "Atari 8-bit Protected (VAPI)",
    .extensions = "atx", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WEAK_BITS |
                    UFT_FORMAT_CAP_VERIFY,
    .probe = atx_plugin_probe, .open = atx_open,
    .close = atx_close, .read_track = atx_read_track,
    .verify_track = uft_weak_bit_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(atx)

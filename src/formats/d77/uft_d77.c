/**
 * @file uft_d77.c
 * @brief D77 Disk Image Plugin (Sharp X1, FM-7, PC-88 compatible)
 *
 * D77 is the Sharp/Fujitsu variant of the NEC D88 disk image format.
 * Header layout is identical to D88:
 *
 *   Offset  Size  Description
 *   0x00    17    Disk name (null-terminated)
 *   0x1A    1     Write protect (0x00=off, 0x10=on)
 *   0x1B    1     Media type (0x00=2D, 0x10=2DD, 0x20=2HD)
 *   0x1C    4     Disk size in bytes (LE32)
 *   0x20    656   Track offset table (164 x LE32)
 *
 * Each track: sequence of sector headers (16 bytes) + data:
 *   Offset  Size  Description
 *   0x00    1     Cylinder
 *   0x01    1     Head
 *   0x02    1     Sector number (1-based)
 *   0x03    1     Size code (N: 128 << N)
 *   0x04    2     Number of sectors on track (LE16)
 *   0x06    1     Density (0x00=double, 0x40=single)
 *   0x07    1     Deleted mark (0x00=normal, 0x10=deleted)
 *   0x08    1     Status (0x00=OK, non-zero=error)
 *   0x09-0D 5     Reserved
 *   0x0E    2     Data size in bytes (LE16)
 *   0x10    N     Sector data
 *
 * Reference: D77/D88 format specification
 * Platforms: Sharp X1, FM-7/FM-77, PC-8801 (via D88 compat)
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define D77_HEADER_SIZE     0x2B0   /* 688 bytes, 164 track entries */
#define D77_HEADER_MIN      0x2A0   /* 672 bytes, 160 entries (MF-625) */
#define D77_TRACKS_160      160     /* 80 cylinders x 2 heads */
#define D77_MAX_TRACKS      164     /* 82 cylinders x 2 heads */
#define D77_SEC_HDR_SIZE    16
#define D77_MAX_SECTOR_SIZE 8192
#define D77_MAX_SPT         64

/* Media type codes */
#define D77_MEDIA_2D        0x00
#define D77_MEDIA_2DD       0x10
#define D77_MEDIA_2HD       0x20

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    uint8_t     media_type;
    uint32_t    disk_size;
    uint32_t    track_offsets[D77_MAX_TRACKS];
    char        disk_name[17];
} d77_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

/**
 * How many track-table entries this image has: 160 or 164 (MF-625).
 *
 * The variant is not stored, but it is forced by the data rather than
 * guessed: entries 160..163 would occupy 0x2A0..0x2AF, so if a track's data
 * begins at 0x2A0 those sixteen bytes cannot also be table entries. 160
 * entries is a full 80-cylinder double-sided disk, so the shorter header
 * loses no reachable track.
 */
static int d77_track_entries(const uint8_t *tbl_base)
{
    uint32_t lowest = 0;
    for (int i = 0; i < D77_TRACKS_160; i++) {
        uint32_t o = uft_read_le32(tbl_base + i * 4);
        if (o != 0 && (lowest == 0 || o < lowest)) lowest = o;
    }
    return (lowest == D77_HEADER_MIN) ? D77_TRACKS_160 : D77_MAX_TRACKS;
}

bool d77_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    if (size < D77_HEADER_MIN) return false;

    uint32_t disk_size = uft_read_le32(data + 0x1C);
    uint8_t media = data[0x1B];

    /* Disk size must be plausible */
    if (disk_size == 0 || disk_size > file_size) return false;

    /* Media type must be known */
    if (media != D77_MEDIA_2D && media != D77_MEDIA_2DD &&
        media != D77_MEDIA_2HD)
        return false;

    /* First track offset should point past header.
     *
     * MF-625: the bound was D77_HEADER_SIZE (0x2B0), which rejected every
     * image written with the 160-entry table. D77 shares D88's layout, and
     * that layout has two documented header sizes — 688 bytes with 164 track
     * entries, 672 with 160 (<https://www.pc98.org/project/doc/d88.html>;
     * MAME d88_dsk.cpp names the same pair, "0x02A0 or 0x02B0"). The lower
     * bound is therefore the smaller of the two. */
    uint32_t first_off = uft_read_le32(data + 0x20);
    if (first_off < D77_HEADER_MIN || first_off >= disk_size)
        return false;

    *confidence = 85;
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t d77_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[D77_HEADER_SIZE];
    if (fread(hdr, 1, D77_HEADER_SIZE, f) != D77_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    d77_data_t *pdata = calloc(1, sizeof(d77_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;

    /* Parse header */
    memcpy(pdata->disk_name, hdr, 16);
    pdata->disk_name[16] = '\0';
    pdata->media_type = hdr[0x1B];
    pdata->disk_size = uft_read_le32(hdr + 0x1C);

    /* Read track offset table.
     *
     * MF-625: only the entries this image actually has. On a 672-byte header
     * the last four of the 164 would be the first track's sector header read
     * as offsets — which put track_count at 164 and handed the caller an
     * 82-cylinder geometry for an 80-cylinder disk. The remaining entries
     * stay 0 = unformatted, which d77_read_track() already refuses. */
    const int entries = d77_track_entries(hdr + 0x20);
    uint8_t track_count = 0;
    for (int i = 0; i < entries; i++) {
        pdata->track_offsets[i] = uft_read_le32(hdr + 0x20 + i * 4);
        if (pdata->track_offsets[i] != 0)
            track_count = (uint8_t)(i + 1);
    }

    /* Determine geometry from media type and track count */
    uint8_t cylinders = (track_count + 1) / 2;
    if (cylinders == 0) cylinders = 40;
    if (cylinders > 82) cylinders = 82;
    uint8_t heads = (track_count > cylinders) ? 2 : 1;

    /* Read actual sector parameters from first track header */
    uint16_t sector_size = 256;
    uint16_t sectors_per_track = 16;

    if (pdata->track_offsets[0] >= D77_HEADER_MIN) {
        uint8_t sec_hdr[D77_SEC_HDR_SIZE];
        if (fseek(f, (long)pdata->track_offsets[0], SEEK_SET) == 0 &&
            fread(sec_hdr, 1, D77_SEC_HDR_SIZE, f) == D77_SEC_HDR_SIZE) {
            uint16_t ss = uft_read_le16(sec_hdr + 14);
            uint16_t sc = uft_read_le16(sec_hdr + 4);
            if (ss > 0 && ss <= D77_MAX_SECTOR_SIZE &&
                sc > 0 && sc <= D77_MAX_SPT) {
                sector_size = ss;
                sectors_per_track = sc;
            }
        }
    }

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cylinders;
    disk->geometry.heads = heads;
    disk->geometry.sectors = sectors_per_track;
    disk->geometry.sector_size = sector_size;
    disk->geometry.total_sectors =
        (uint32_t)cylinders * heads * sectors_per_track;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void d77_close(uft_disk_t *disk)
{
    d77_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t d77_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    d77_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;

    /* MF-560: negative Koordinaten abweisen, BEVOR daraus ein Index wird.
     *
     * Hier stand nur die obere Schranke gegen D77_MAX_TRACKS. Ein negatives
     * `cyl` oder `head` ergibt einen negativen Index, kommt an `>=` vorbei
     * und greift vor das Feld. Dieselbe Stelle wie in uft_mfi.c, wo ASan
     * sie in der CI gefunden hat — lokal unsichtbar, MinGW hat keinen
     * Sanitizer.
     *
     * MF-516/522 hat diese Klasse in 54 Dateien behoben; die Stellen, die
     * den Index VOR der Schranke ausrechnen, sind durchgerutscht. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    int idx = cyl * disk->geometry.heads + head;
    if (idx >= D77_MAX_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (pdata->track_offsets[idx] == 0) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    if (fseek(pdata->file, (long)pdata->track_offsets[idx], SEEK_SET) != 0)
        return UFT_ERROR_IO;

    /* Read sector headers + data until end of track */
    for (int s = 0; s < D77_MAX_SPT; s++) {
        uint8_t sec_hdr[D77_SEC_HDR_SIZE];
        if (fread(sec_hdr, 1, D77_SEC_HDR_SIZE, pdata->file) !=
            D77_SEC_HDR_SIZE)
            break;

        uint16_t data_size = uft_read_le16(sec_hdr + 14);
        uint16_t num_sectors = uft_read_le16(sec_hdr + 4);

        /* Sanity check */
        if (data_size > D77_MAX_SECTOR_SIZE) break;
        if (num_sectors == 0) break;

        /* Only read num_sectors sectors total */
        if (s >= num_sectors) break;

        /* MF-1477, same reference as the D88 plugin (MF-1475): MAME
         * src/lib/formats/d88_dsk.cpp (BSD-3-Clause, read only),
         * d88_format::load(). The ID is hs[0..3] (C, H, R, N) as recorded —
         * not the physical position, not N from the data length, and R
         * unchanged (the old `if (sec_num > 0) sec_num--` plus the +1 of
         * uft_format_add_sector() turned a recorded R=0 into R=1). A header
         * with data length 0 is a sector without data (MAME: data = nullptr),
         * not the end of the track: kept with its ID, marked unavailable.
         * Guarded by tests/test_d77_spur_nach_kopf.c. */
        if (data_size == 0) {
            const uint8_t leer = 0;
            uft_format_add_sector_with_id(track, sec_hdr[2], &leer, 1,
                                          sec_hdr[0], sec_hdr[1]);
            if (track->sector_count > 0) {
                uft_sector_t *z = &track->sectors[track->sector_count - 1];
                z->id.size_code = sec_hdr[3];
                z->data_len = 0;
                z->data_size = 0;
            }
            uft_format_mark_last_unavailable(track);
            continue;
        }

        uint8_t *buf = malloc(data_size);
        if (!buf) return UFT_ERROR_NO_MEMORY;

        if (fread(buf, 1, data_size, pdata->file) != data_size) {
            free(buf);
            return UFT_ERROR_IO;
        }

        uft_format_add_sector_with_id(track, sec_hdr[2], buf, data_size,
                                      sec_hdr[0], sec_hdr[1]);
        if (track->sector_count > 0)
            track->sectors[track->sector_count - 1].id.size_code = sec_hdr[3];
        /* D77 sector flags: byte 7=deleted (0x10), byte 8=status (0x00=OK) */
        if (track->sector_count > 0) {
            if (sec_hdr[7] == 0x10)
                track->sectors[track->sector_count - 1].deleted = true;
            /* MF-1492 (P3-659): D77 is the D88 container (FM-7), so the
             * status byte means what it means there — MAME d88_dsk.cpp
             * (0xB0 bad data CRC, 0xA0 bad address CRC) and hxcfe, executed
             * in MF-1480 (0x10 deleted, 0xE0 no address mark = not
             * findable). Before, ANY non-zero byte became set_crc(false),
             * which the disk2 bridge does not read. Other values keep that
             * old marking; 0xF0 stays undecided as in the D88 reader. */
            uft_sector_t *z = &track->sectors[track->sector_count - 1];
            switch (sec_hdr[8]) {
            case 0x00: break;
            case 0xB0: uft_sector_mark_data_crc_error(z); break;
            case 0xA0: uft_sector_mark_id_crc_error(z);   break;
            case 0x10: z->deleted = true;                 break;
            case 0xE0: uft_format_mark_last_unavailable(track); break;
            default:   uft_sector_set_crc(z, false);      break;
            }
        }
        free(buf);
    }

    return UFT_OK;
}

/* ============================================================================
 * write_track
 * ============================================================================ */

static uft_error_t d77_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    d77_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* MF-560: negative Koordinaten abweisen, BEVOR daraus ein Index wird.
     *
     * Hier stand nur die obere Schranke gegen D77_MAX_TRACKS. Ein negatives
     * `cyl` oder `head` ergibt einen negativen Index, kommt an `>=` vorbei
     * und greift vor das Feld. Dieselbe Stelle wie in uft_mfi.c, wo ASan
     * sie in der CI gefunden hat — lokal unsichtbar, MinGW hat keinen
     * Sanitizer.
     *
     * MF-516/522 hat diese Klasse in 54 Dateien behoben; die Stellen, die
     * den Index VOR der Schranke ausrechnen, sind durchgerutscht. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    int idx = cyl * disk->geometry.heads + head;
    if (idx >= D77_MAX_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (pdata->track_offsets[idx] == 0) return UFT_ERROR_INVALID_STATE;

    if (fseek(pdata->file, (long)pdata->track_offsets[idx], SEEK_SET) != 0)
        return UFT_ERROR_IO;

    /* Walk sector headers, write only the data portion */
    for (int s = 0; s < D77_MAX_SPT; s++) {
        uint8_t sec_hdr[D77_SEC_HDR_SIZE];
        if (fread(sec_hdr, 1, D77_SEC_HDR_SIZE, pdata->file) !=
            D77_SEC_HDR_SIZE)
            break;
        /* MF-1439: ISO C 7.21.5.3 — on an update stream, input must not be
         * followed by output without a positioning call in between. glibc
         * tolerates it, the MinGW runtime does not: measured, write_track
         * returned UFT_OK and 0 of 4 data fields reached the file
         * (tests/test_d77_schreiben_positioniert.c). Same pattern as
         * d88_write_track (MF-1470 on the cloud branch). */
        if (fseek(pdata->file, 0, SEEK_CUR) != 0) return UFT_ERROR_IO;

        uint16_t data_size = uft_read_le16(sec_hdr + 14);
        uint16_t num_sectors = uft_read_le16(sec_hdr + 4);

        if (data_size == 0 || data_size > D77_MAX_SECTOR_SIZE) break;
        if (num_sectors == 0) break;
        if (s >= num_sectors) break;

        /* Write sector data if we have a matching sector in track */
        if ((size_t)s < track->sector_count) {
            const uint8_t *data = track->sectors[s].data;
            uint8_t *pad = NULL;
            if (!data || track->sectors[s].data_len == 0) {
                pad = malloc(data_size);
                if (!pad) return UFT_ERROR_NO_MEMORY;
                memset(pad, 0xE5, data_size);
                data = pad;
            }
            if (fwrite(data, 1, data_size, pdata->file) != data_size) {
                free(pad);
                return UFT_ERROR_IO;
            }
            free(pad);
            /* the same rule the other way round: the next pass reads a
             * sector header again */
            if (fseek(pdata->file, 0, SEEK_CUR) != 0) return UFT_ERROR_IO;
        } else {
            /* Skip past this sector's data */
            if (fseek(pdata->file, (long)data_size, SEEK_CUR) != 0)
                return UFT_ERROR_IO;
        }
    }

    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_d77_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_d77 = {
    .name         = "D77",
    .description  = "Sharp X1/FM-7 Disk Image (D77/1DD)",
    .extensions   = "d77;1dd",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe        = d77_probe,
    .open         = d77_open,
    .close        = d77_close,
    .read_track   = d77_read_track,
    .write_track  = d77_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_d77_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_d77_features) / sizeof(uft_format_plugin_d77_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(d77)

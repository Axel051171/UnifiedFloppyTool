/**
 * @file uft_jv3.c
 * @brief JV3 (TRS-80) Disk Image Plugin
 *
 * JV3 extends JV1 with a sector directory that stores per-sector
 * metadata (track, sector ID, flags). Supports variable sector sizes,
 * deleted data marks, and mixed densities.
 *
 * Layout:
 *   Offset 0x0000: Sector directory (2901 entries × 3 bytes = 8703)
 *     Each entry: track(1) + sector_id(1) + flags(1)
 *     Flags: bit 0-1 = size (0=256, 1=128, 2=1024, 3=512)
 *            bit 2   = non-IBM flag
 *            bit 3   = CRC error
 *            bit 4   = side (0=side0, 1=side1)
 *            bit 5   = double density
 *            bit 6   = deleted address mark
 *            bit 7   = used (0xFF = free entry)
 *   Offset 0x2200: Write-protect byte
 *   Offset 0x2300: Sector data (sequential, matching directory order)
 *
 * Reference: Tim Mann's xtrs/JV3 documentation
 */

#include "uft/uft_format_common.h"

#define JV3_DIR_ENTRIES     2901
#define JV3_DIR_SIZE        (JV3_DIR_ENTRIES * 3)
#define JV3_HEADER_SIZE     0x2300      /* dir + writeprot + padding */
#define JV3_FREE_ENTRY      0xFF

/* Size code → bytes */
static const uint16_t jv3_sizes[4] = { 256, 128, 1024, 512 };

typedef struct {
    uint8_t     track;
    uint8_t     sector_id;
    uint8_t     flags;
    uint16_t    size;
    uint32_t    data_offset;
} jv3_dir_entry_t;

typedef struct {
    FILE*               file;
    jv3_dir_entry_t     entries[JV3_DIR_ENTRIES];
    uint16_t            entry_count;
    uint8_t             max_cyl;
    uint8_t             max_head;
} jv3_data_t;

bool jv3_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    (void)data;
    if (size < JV3_HEADER_SIZE || file_size < JV3_HEADER_SIZE + 256)
        return false;

    /* Check directory: at least some valid entries */
    int valid = 0;
    for (int i = 0; i < 100 && i < (int)(size / 3); i++) {
        uint8_t trk = data[i * 3];
        uint8_t flags = data[i * 3 + 2];
        if (trk == JV3_FREE_ENTRY && data[i * 3 + 1] == JV3_FREE_ENTRY)
            continue;  /* free entry */
        if (trk < 80 && (flags & 0x03) < 4)
            valid++;
    }

    if (valid >= 5) {
        *confidence = 70;
        return true;
    }
    return false;
}

static uft_error_t jv3_open(uft_disk_t *disk, const char *path,
                              bool read_only)
{
    size_t file_size = 0;
    uint8_t *file_data = uft_read_file(path, &file_size);
    if (!file_data || file_size < JV3_HEADER_SIZE + 256) {
        free(file_data);
        return UFT_ERROR_FORMAT_INVALID;
    }

    jv3_data_t *pdata = calloc(1, sizeof(jv3_data_t));
    if (!pdata) { free(file_data); return UFT_ERROR_NO_MEMORY; }

    /* Reopen as FILE — r+b for write, rb for read-only */
    pdata->file = fopen(path, read_only ? "rb" : "r+b");
    if (!pdata->file) {
        /* Fallback to read-only if r+b fails (e.g. permissions) */
        pdata->file = fopen(path, "rb");
        read_only = true;
    }
    if (!pdata->file) { free(pdata); free(file_data); return UFT_ERROR_FILE_OPEN; }
    disk->read_only = read_only;

    /* Parse directory */
    uint32_t data_off = JV3_HEADER_SIZE;
    uint8_t max_cyl = 0, max_head = 0;

    for (int i = 0; i < JV3_DIR_ENTRIES; i++) {
        uint8_t trk = file_data[i * 3];
        uint8_t sid = file_data[i * 3 + 1];
        uint8_t flg = file_data[i * 3 + 2];

        if (trk == JV3_FREE_ENTRY && sid == JV3_FREE_ENTRY) break;

        jv3_dir_entry_t *e = &pdata->entries[pdata->entry_count];
        e->track = trk;
        e->sector_id = sid;
        e->flags = flg;
        e->size = jv3_sizes[flg & 0x03];
        e->data_offset = data_off;

        uint8_t head = (flg & 0x10) ? 1 : 0;
        if (trk > max_cyl) max_cyl = trk;
        if (head > max_head) max_head = head;

        data_off += e->size;
        if (data_off > file_size) break;

        pdata->entry_count++;
    }

    free(file_data);

    pdata->max_cyl = max_cyl;
    pdata->max_head = max_head;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = max_head + 1;
    disk->geometry.sectors = 18;    /* varies per track */
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = pdata->entry_count;
    return UFT_OK;
}

static void jv3_close(uft_disk_t *disk)
{
    jv3_data_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t jv3_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    jv3_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    uint8_t buf[1024];
    for (int i = 0; i < p->entry_count; i++) {
        jv3_dir_entry_t *e = &p->entries[i];
        uint8_t e_head = (e->flags & 0x10) ? 1 : 0;
        if (e->track != cyl || e_head != head) continue;

        if (fseek(p->file, (long)e->data_offset, SEEK_SET) != 0)
            continue;
        uint16_t sz = e->size;
        if (sz > 1024) sz = 1024;
        if (fread(buf, 1, sz, p->file) != sz) continue;

        uint8_t sec_num = e->sector_id;
        if (sec_num > 0) sec_num--;
        uft_format_add_sector(track, sec_num, buf, sz,
                              (uint8_t)cyl, (uint8_t)head);
        /* JV3 flags: bit 3=CRC error, bit 6=deleted DAM */
        if (track->sector_count > 0) {
            if (e->flags & 0x08)
                track->sectors[track->sector_count - 1].crc_ok = false;
            if (e->flags & 0x40)
                track->sectors[track->sector_count - 1].deleted = true;
        }
    }
    return UFT_OK;
}

/* Write track: seeks to each sector's data_offset in the JV3 file and writes.
 * JV3 sectors have known offsets from the directory, so we can write in-place.
 * Matches incoming sectors by sector_id to directory entries. */
static uft_error_t jv3_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    jv3_data_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    for (size_t s = 0; s < track->sector_count; s++) {
        /* id.sector is 1-based (from uft_format_add_sector: sector_num + 1).
         * JV3 sector_id is the raw value from disk. In read_track we passed
         * sec_num = sector_id - 1 to uft_format_add_sector, so
         * id.sector = (sector_id - 1) + 1 = sector_id. */
        uint8_t sec_id = track->sectors[s].id.sector;
        /* Find matching directory entry */
        for (int i = 0; i < p->entry_count; i++) {
            jv3_dir_entry_t *e = &p->entries[i];
            uint8_t e_head = (e->flags & 0x10) ? 1 : 0;
            if (e->track != (uint8_t)cyl || e_head != (uint8_t)head)
                continue;
            if (e->sector_id != sec_id) continue;

            /* Found matching entry — write sector data */
            if (fseek(p->file, (long)e->data_offset, SEEK_SET) != 0)
                return UFT_ERROR_IO;
            const uint8_t *data = track->sectors[s].data;
            uint16_t sz = e->size;
            if (sz > 1024) sz = 1024;
            uint8_t pad[1024];
            if (!data || track->sectors[s].data_len == 0) {
                memset(pad, 0xE5, sz); data = pad;
            }
            if (fwrite(data, 1, sz, p->file) != sz)
                return UFT_ERROR_IO;
            break;
        }
    }
    fflush(p->file);
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_jv3 = {
    .name = "JV3", .description = "TRS-80 JV3 (with sector directory)",
    .extensions = "jv3;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = jv3_probe, .open = jv3_open, .close = jv3_close,
    .read_track = jv3_read_track, .write_track = jv3_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(jv3)

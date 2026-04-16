/**
 * @file uft_jvc_plugin.c
 * @brief JVC (TRS-80 JV1 with header) Plugin-B
 *
 * JVC extends raw JV1 with a variable-length header:
 *   Header size = file_size % 256 (remainder bytes before sector data)
 *   Byte 0: sectors per track (default 18)
 *   Byte 1: side count (default 1)
 *   Byte 2: size code (0=256, 1=128, 2=512, 3=1024)
 *   Byte 3: first sector ID (default 0)
 *   Byte 4: sector attribute flag
 *   Sector data follows header in sequential C/H/S order.
 *
 * Reference: Tim Mann's TRS-80 JVC specification
 */
#include "uft/uft_format_common.h"

/* JVC size code to bytes */
static uint16_t jvc_size_code(uint8_t code) {
    switch (code) {
        case 0: return 256;
        case 1: return 128;
        case 2: return 512;
        case 3: return 1024;
        default: return 256;
    }
}

typedef struct {
    FILE    *file;
    uint8_t  spt;           /* sectors per track */
    uint8_t  sides;
    uint16_t sec_size;
    uint8_t  first_sector;  /* first sector ID */
    bool     has_attribs;   /* sector attribute flag present */
    size_t   hdr_size;      /* header length in bytes */
    uint8_t  cylinders;
} jvc_pd_t;

static bool jvc_probe(const uint8_t *data, size_t size, size_t file_size,
                       int *confidence)
{
    (void)data; (void)size;
    if (file_size == 0) return false;

    size_t hdr = file_size % 256;
    size_t body = file_size - hdr;
    if (body == 0) return false;

    /* With no header, defaults: 18 spt, 1 side, 256 bytes */
    uint8_t spt = 18;
    uint8_t sides = 1;
    uint16_t ss = 256;

    if (hdr >= 1 && size >= 1) spt = data[0];
    if (hdr >= 2 && size >= 2) sides = data[1];
    if (hdr >= 3 && size >= 3) ss = jvc_size_code(data[2]);

    if (spt == 0 || spt > 64 || sides == 0 || sides > 2 || ss == 0)
        return false;

    size_t track_bytes = (size_t)spt * ss;
    if (track_bytes == 0 || body % track_bytes != 0) return false;

    *confidence = 40;
    return true;
}

static uft_error_t jvc_open(uft_disk_t *disk, const char *path, bool ro)
{
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs <= 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    size_t file_size = (size_t)fs;
    size_t hdr = file_size % 256;
    size_t body = file_size - hdr;
    if (body == 0) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    /* Read header bytes if present */
    uint8_t hdr_buf[5];
    memset(hdr_buf, 0, sizeof(hdr_buf));
    if (hdr > 0) {
        size_t to_read = hdr < sizeof(hdr_buf) ? hdr : sizeof(hdr_buf);
        if (fread(hdr_buf, 1, to_read, f) != to_read) {
            fclose(f); return UFT_ERROR_IO;
        }
    }

    uint8_t spt = (hdr >= 1) ? hdr_buf[0] : 18;
    uint8_t sides = (hdr >= 2) ? hdr_buf[1] : 1;
    uint16_t ss = (hdr >= 3) ? jvc_size_code(hdr_buf[2]) : 256;
    uint8_t first_sec = (hdr >= 4) ? hdr_buf[3] : 0;
    bool has_attr = (hdr >= 5) ? (hdr_buf[4] != 0) : false;

    if (spt == 0 || spt > 64 || sides == 0 || sides > 2 || ss == 0) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    size_t track_bytes = (size_t)spt * ss;
    if (track_bytes == 0 || body % track_bytes != 0) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    uint32_t total_tracks = (uint32_t)(body / track_bytes);
    uint8_t cyls = (uint8_t)(total_tracks / sides);
    if (cyls == 0 || cyls > UFT_MAX_CYLINDERS) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    jvc_pd_t *p = calloc(1, sizeof(jvc_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    p->file = f;
    p->spt = spt;
    p->sides = sides;
    p->sec_size = ss;
    p->first_sector = first_sec;
    p->has_attribs = has_attr;
    p->hdr_size = hdr;
    p->cylinders = cyls;

    disk->plugin_data = p;
    disk->geometry.cylinders = cyls;
    disk->geometry.heads = sides;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = (uint32_t)cyls * sides * spt;
    return UFT_OK;
}

static void jvc_close(uft_disk_t *disk)
{
    jvc_pd_t *p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t jvc_read_track(uft_disk_t *disk, int cyl, int head,
                                    uft_track_t *track)
{
    jvc_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    size_t track_bytes = (size_t)p->spt * p->sec_size;
    long base = (long)(p->hdr_size +
                       ((size_t)cyl * p->sides + head) * track_bytes);

    uint8_t *buf = malloc(p->sec_size);
    if (!buf) return UFT_ERROR_NO_MEMORY;

    for (int s = 0; s < p->spt; s++) {
        long off = base + (long)s * p->sec_size;
        if (fseek(p->file, off, SEEK_SET) != 0) {
            memset(buf, 0xE5, p->sec_size);
        } else if (fread(buf, 1, p->sec_size, p->file) != p->sec_size) {
            memset(buf, 0xE5, p->sec_size);
        }
        uft_format_add_sector(track, (uint8_t)s, buf, p->sec_size,
                              (uint8_t)cyl, (uint8_t)head);
    }
    free(buf);
    return UFT_OK;
}

static uft_error_t jvc_write_track(uft_disk_t *disk, int cyl, int head,
                                     const uft_track_t *track)
{
    jvc_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    size_t track_bytes = (size_t)p->spt * p->sec_size;
    long base = (long)(p->hdr_size +
                       ((size_t)cyl * p->sides + head) * track_bytes);

    for (size_t s = 0; s < track->sector_count && (int)s < p->spt; s++) {
        long off = base + (long)s * p->sec_size;
        if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;

        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[1024];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, p->sec_size);
            data = pad;
        }
        if (fwrite(data, 1, p->sec_size, p->file) != p->sec_size)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_jvc = {
    .name = "JVC", .description = "TRS-80 JVC (JV1 with header)",
    .extensions = "jvc;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = jvc_probe, .open = jvc_open, .close = jvc_close,
    .read_track = jvc_read_track, .write_track = jvc_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(jvc)

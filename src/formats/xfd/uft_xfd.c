/**
 * @file uft_xfd.c
 * @brief XFD (Atari 8-bit Raw Disk) Plugin
 *
 * XFD is a headerless raw sector dump for Atari 8-bit computers.
 * Geometry from file size: 720 sectors × 128 bytes = 92160 (SD)
 * or 720 × 256 = 184320 (DD), or 1040 × 128/256 (ED).
 */
#include "uft/uft_format_common.h"

#define XFD_SS  128
#define XFD_DS  256

typedef struct { FILE* file; uint16_t ss; uint32_t total; } xfd_data_t;

static bool uft_xfd_plugin_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    if (fs != 92160 && fs != 184320 && fs != 133120 && fs != 266240) {
        if (fs == 0 || (fs % 128 != 0 && fs % 256 != 0) || fs > 266240) return false;
        *c = 25; return true;
    }
    *c = 40;

    /* Atari DOS boot sector: byte 0 = boot flag (0x00 or 0x01),
     * bytes 1-2 = sector count (LE16), byte 3 = load address high */
    if (s >= 4) {
        if ((d[0] == 0x00 || d[0] == 0x01) && d[3] >= 0x07 && d[3] <= 0xBF)
            *c = 82;
    }
    return true;
}

static uft_error_t xfd_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    xfd_data_t *p = calloc(1, sizeof(xfd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->ss = ((size_t)fs % 256 == 0 && fs > 92160) ? XFD_DS : XFD_SS;
    p->total = (uint32_t)fs / p->ss;

    disk->plugin_data = p;
    disk->geometry.cylinders = (p->total + 17) / 18;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = p->ss;
    disk->geometry.total_sectors = p->total;
    return UFT_OK;
}

static void xfd_close(uft_disk_t *d) {
    xfd_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t xfd_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    xfd_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)cyl * 18 * p->ss;
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[256];
    for (int s = 0; s < 18; s++) {
        uint32_t sec = (uint32_t)cyl * 18 + s;
        if (sec >= p->total) break;
        if (fread(buf, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, p->ss, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t xfd_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
    xfd_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)cyl * 18 * p->ss;
    for (size_t s = 0; s < t->sector_count && s < 18; s++) {
        uint32_t sec = (uint32_t)cyl * 18 + (uint32_t)s;
        if (sec >= p->total) break;
        if (fseek(p->file, off + (long)s * p->ss, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, p->ss); data = pad; }
        if (fwrite(data, 1, p->ss, p->file) != p->ss) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_xfd = {
    .name = "XFD", .description = "Atari 8-bit Raw Disk",
    .extensions = "xfd", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = uft_xfd_plugin_probe, .open = xfd_open, .close = xfd_close,
    .read_track = xfd_read_track, .write_track = xfd_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(xfd)

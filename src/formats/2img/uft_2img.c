/**
 * @file uft_2img.c
 * @brief 2IMG (Apple II Universal Disk Image) Plugin
 *
 * 2IMG wraps Apple II disk data with a 64-byte header containing
 * format info, data offset/length, and optional comment/creator.
 *
 * Header (64 bytes):
 *   0x00 4  Magic "2IMG"
 *   0x04 4  Creator ID (4 ASCII chars)
 *   0x08 2  Header size (64)
 *   0x0A 2  Version (1)
 *   0x0C 4  Image format (0=DOS order, 1=ProDOS order, 2=NIB)
 *   0x10 4  Flags
 *   0x14 4  ProDOS blocks (0 = auto)
 *   0x18 4  Data offset
 *   0x1C 4  Data length
 *   0x20 4  Comment offset (0 = none)
 *   0x24 4  Comment length
 *   0x28 4  Creator-specific offset
 *   0x2C 4  Creator-specific length
 *   0x30-3F  Reserved
 *
 * Reference: Apple II 2IMG specification (web.archive.org)
 */
#include "uft/uft_format_common.h"

#define IMG2_MAGIC      0x474D4932  /* "2IMG" LE */
#define IMG2_HDR_SIZE   64
#define IMG2_FMT_DOS    0
#define IMG2_FMT_PRODOS 1
#define IMG2_FMT_NIB    2

typedef struct {
    FILE*       file;
    uint32_t    data_offset;
    uint32_t    data_length;
    uint32_t    img_format;
    uint16_t    sector_size;
    uint8_t     cylinders;
    uint8_t     spt;
} img2_data_t;

bool img2_probe(const uint8_t *data, size_t size, size_t file_size, int *confidence) {
    (void)file_size;
    if (size < IMG2_HDR_SIZE) return false;
    if (uft_read_le32(data) == IMG2_MAGIC) {
        *confidence = 95; return true;
    }
    return false;
}

static uft_error_t img2_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[IMG2_HDR_SIZE];
    if (fread(hdr, 1, IMG2_HDR_SIZE, f) != IMG2_HDR_SIZE) { fclose(f); return UFT_ERROR_IO; }
    if (uft_read_le32(hdr) != IMG2_MAGIC) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    img2_data_t *p = calloc(1, sizeof(img2_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    p->file = f;
    p->img_format = uft_read_le32(hdr + 0x0C);
    p->data_offset = uft_read_le32(hdr + 0x18);
    p->data_length = uft_read_le32(hdr + 0x1C);

    if (p->data_offset < IMG2_HDR_SIZE) p->data_offset = IMG2_HDR_SIZE;

    /* Geometry: Apple II = 35 tracks × 16 sectors × 256 bytes (DOS/ProDOS) */
    if (p->img_format == IMG2_FMT_NIB) {
        p->sector_size = 256; p->cylinders = 35; p->spt = 16;
    } else {
        p->sector_size = 256; p->spt = 16;
        p->cylinders = (p->data_length > 0) ?
            (uint8_t)(p->data_length / (16 * 256)) : 35;
        if (p->cylinders == 0) p->cylinders = 35;
        if (p->cylinders > 80) p->cylinders = 80;
    }

    disk->plugin_data = p;
    disk->geometry.cylinders = p->cylinders;
    disk->geometry.heads = 1;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = p->sector_size;
    disk->geometry.total_sectors = (uint32_t)p->cylinders * p->spt;
    return UFT_OK;
}

static void img2_close(uft_disk_t *d) {
    img2_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}

static uft_error_t img2_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    img2_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);

    long off = (long)(p->data_offset + (uint32_t)cyl * p->spt * p->sector_size);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;

    uint8_t buf[256];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, p->sector_size, p->file) != p->sector_size)
            return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, p->sector_size, (uint8_t)cyl, 0);
    }
    return UFT_OK;
}

static uft_error_t img2_write_track(uft_disk_t *d, int cyl, int head,
                                     const uft_track_t *t) {
    img2_data_t *p = d->plugin_data;
    if (!p || !p->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(p->data_offset + (uint32_t)cyl * p->spt * p->sector_size);
    for (size_t s = 0; s < t->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * p->sector_size, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[256];
        if (!data || t->sectors[s].data_len == 0) {
            memset(pad, 0xE5, p->sector_size); data = pad;
        }
        if (fwrite(data, 1, p->sector_size, p->file) != p->sector_size)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_2img = {
    .name = "2IMG", .description = "Apple II Universal Disk Image",
    .extensions = "2img;2mg", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = img2_probe, .open = img2_open, .close = img2_close,
    .read_track = img2_read_track, .write_track = img2_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(2img)

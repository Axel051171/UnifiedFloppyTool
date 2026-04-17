/**
 * @file uft_vdk_plugin.c
 * @brief VDK (Tandy Color Computer) Plugin-B
 *
 * VDK has a 12-byte header followed by raw sector data.
 * Header: magic(2) + header_size(2) + version(1) + compat(1) +
 *         source_id(1) + source_ver(1) + tracks(1) + sides(1) +
 *         flags(1) + compression(1)
 */
#include "uft/uft_format_common.h"

#define VDK_MAGIC   0x6B64  /* "dk" LE */
#define VDK_HDR     12
#define VDK_SS      256
#define VDK_SPT     18

typedef struct { FILE *file; uint8_t hdr_size, tracks, sides; } vdk_pd_t;

static bool vdk_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)fs;
    if (s < VDK_HDR) return false;
    if (uft_read_le16(d) == VDK_MAGIC) { *c = 92; return true; }
    return false;
}

static uft_error_t vdk_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    uint8_t hdr[VDK_HDR];
    if (fread(hdr, 1, VDK_HDR, f) != VDK_HDR) { fclose(f); return UFT_ERROR_IO; }
    if (uft_read_le16(hdr) != VDK_MAGIC) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    vdk_pd_t *p = calloc(1, sizeof(vdk_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->hdr_size = uft_read_le16(hdr + 2);
    p->tracks = hdr[8];
    p->sides = hdr[9];
    if (p->tracks == 0) p->tracks = 35;
    if (p->sides == 0) p->sides = 1;

    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks; disk->geometry.heads = p->sides;
    disk->geometry.sectors = VDK_SPT; disk->geometry.sector_size = VDK_SS;
    disk->geometry.total_sectors = (uint32_t)p->tracks * p->sides * VDK_SPT;
    return UFT_OK;
}

static void vdk_close(uft_disk_t *disk) {
    vdk_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t vdk_read_track(uft_disk_t *disk, int cyl, int head, uft_track_t *track) {
    vdk_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    long off = (long)(p->hdr_size + ((size_t)cyl * p->sides + head) * VDK_SPT * VDK_SS);
    uint8_t buf[VDK_SS];
    for (int s = 0; s < VDK_SPT; s++) {
        if (fseek(p->file, off + s * VDK_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, VDK_SS, p->file) != VDK_SS) { memset(buf, 0xE5, VDK_SS); }
        uft_format_add_sector(track, (uint8_t)s, buf, VDK_SS, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t vdk_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    vdk_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(p->hdr_size + ((size_t)cyl * p->sides + head) * VDK_SPT * VDK_SS);
    for (size_t s = 0; s < track->sector_count && (int)s < VDK_SPT; s++) {
        if (fseek(p->file, off + (long)s * VDK_SS, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[VDK_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, VDK_SS); data = pad;
        }
        if (fwrite(data, 1, VDK_SS, p->file) != VDK_SS) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_vdk = {
    .name = "VDK", .description = "Tandy CoCo Virtual Disk",
    .extensions = "vdk;dsk", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = vdk_probe, .open = vdk_open, .close = vdk_close,
    .read_track = vdk_read_track, .write_track = vdk_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(vdk)

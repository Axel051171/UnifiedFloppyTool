/**
 * @file uft_edk.c
 * @brief EDK (Ensoniq EPS/ASR) Plugin
 *
 * Ensoniq EPS/ASR/TS synthesizer disk format.
 * 80 cyl × 2 heads × 10 spt × 512 = 819200 (DD) or
 * 80 × 2 × 20 × 512 = 1638400 (HD).
 */
#include "uft/uft_format_common.h"
typedef struct { FILE* file; uint8_t spt; } edk_data_t;
bool edk_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == 819200 || fs == 1638400) { *c = 30; return true; } return false;
}
static uft_error_t edk_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f); if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }
    edk_data_t *p = calloc(1, sizeof(edk_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->spt = (fs == 1638400) ? 20 : 10;
    disk->plugin_data = p;
    disk->geometry.cylinders = 80; disk->geometry.heads = 2;
    disk->geometry.sectors = p->spt; disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = 80*2*p->spt; return UFT_OK;
}
static void edk_close(uft_disk_t *d) {
    edk_data_t *p = d->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); d->plugin_data = NULL; }
}
static uft_error_t edk_read_track(uft_disk_t *d, int cyl, int head, uft_track_t *t) {
    edk_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(t, cyl, head);
    long off = (long)(((uint32_t)cyl*2+head)*p->spt*512);
    if (fseek(p->file, off, SEEK_SET) != 0) return UFT_ERROR_IO;
    uint8_t buf[512];
    for (int s = 0; s < p->spt; s++) {
        if (fread(buf, 1, 512, p->file) != 512) return UFT_ERROR_IO;
        uft_format_add_sector(t, (uint8_t)s, buf, 512, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}
static uft_error_t edk_write_track(uft_disk_t *d, int cyl, int head,
                                    const uft_track_t *t) {
    edk_data_t *p = d->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (d->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)(((uint32_t)cyl*2+head)*p->spt*512);
    for (size_t s = 0; s < t->sector_count && (int)s < p->spt; s++) {
        if (fseek(p->file, off + (long)s * 512, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = t->sectors[s].data;
        uint8_t pad[512];
        if (!data || t->sectors[s].data_len == 0) { memset(pad, 0xE5, 512); data = pad; }
        if (fwrite(data, 1, 512, p->file) != 512) return UFT_ERROR_IO;
    }
    return UFT_OK;
}
const uft_format_plugin_t uft_format_plugin_edk = {
    .name = "EDK", .description = "Ensoniq EPS/ASR Disk",
    .extensions = "ede;edk;eds", .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = edk_probe, .open = edk_open, .close = edk_close,
    .read_track = edk_read_track, .write_track = edk_write_track,
    .verify_track = uft_generic_verify_track,
};
UFT_REGISTER_FORMAT_PLUGIN(edk)

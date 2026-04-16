/**
 * @file uft_d80.c
 * @brief Commodore 8050 D80 format — read + write
 */
#include "uft/uft_format_common.h"

#define D80_TRACKS 77
#define D80_SIZE   533248
static const uint8_t d80_spt[77] = {
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    25,25,25,25,25,25,25,25,25,25,25,
    23,23,23,23,23,23,23,23,23,23,23,23,23
};
static uint16_t d80_off[78];

typedef struct { FILE* file; } d80_data_t;

static void d80_init_off(void) {
    static bool init = false;
    if (init) return;
    d80_off[0] = 0;
    for (int t = 0; t < D80_TRACKS; t++) d80_off[t+1] = d80_off[t] + d80_spt[t];
    init = true;
}

bool d80_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size != D80_SIZE) return false;
    *confidence = 75;
    /* BAM at track 39 sector 0 — D80 link to 39/1 */
    if (size >= 0x33002 && data[0x33000] == 39 && data[0x33001] == 1)
        *confidence = 90;
    return true;
}

static uft_error_t d80_open(uft_disk_t* disk, const char* path, bool read_only) {
    d80_init_off();
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    d80_data_t* pdata = calloc(1, sizeof(d80_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    pdata->file = f;
    disk->plugin_data = pdata;
    disk->geometry.cylinders = D80_TRACKS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 29;
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = 2083;
    return UFT_OK;
}

static void d80_close(uft_disk_t* disk) {
    d80_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d80_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    d80_data_t* p = disk->plugin_data;
    if (!p || !p->file || head != 0 || cyl >= D80_TRACKS) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    uint8_t buf[256];
    for (int s = 0; s < d80_spt[cyl]; s++) {
        if (fseek(p->file, (long)(d80_off[cyl] + s) * 256, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, 256, p->file) != 256) { memset(buf, 0xE5, 256); }
        uft_format_add_sector(track, (uint8_t)s, buf, 256, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t d80_write_track(uft_disk_t* disk, int cyl, int head,
                                    const uft_track_t* track) {
    d80_data_t* p = disk->plugin_data;
    if (!p || !p->file || head != 0 || cyl >= D80_TRACKS) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    for (size_t s = 0; s < track->sector_count && (int)s < d80_spt[cyl]; s++) {
        if (fseek(p->file, (long)(d80_off[cyl] + s) * 256, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[256];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0, 256); data = pad;
        }
        if (fwrite(data, 1, 256, p->file) != 256) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d80 = {
    .name = "D80", .description = "Commodore 8050", .extensions = "d80",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d80_probe, .open = d80_open, .close = d80_close,
    .read_track = d80_read_track, .write_track = d80_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d80)

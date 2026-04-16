/**
 * @file uft_d82.c
 * @brief Commodore 8250 D82 format — read + write
 */
#include "uft/uft_format_common.h"

#define D82_TRACKS 77
#define D82_SIZE   1066496
#define D82_SIDE_SECTORS 2083
static const uint8_t d82_spt[77] = {
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    25,25,25,25,25,25,25,25,25,25,25,
    23,23,23,23,23,23,23,23,23,23,23,23,23
};
static uint16_t d82_off[78];

typedef struct { FILE* file; } d82_data_t;

static void d82_init_off(void) {
    static bool init = false;
    if (init) return;
    d82_off[0] = 0;
    for (int t = 0; t < D82_TRACKS; t++) d82_off[t+1] = d82_off[t] + d82_spt[t];
    init = true;
}

bool d82_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size != D82_SIZE) return false;
    *confidence = 75;
    /* D82 = dual-sided D80. BAM at same track structure */
    if (size >= 0x33002 && data[0x33000] == 39 && data[0x33001] == 1)
        *confidence = 88;
    return true;
}

static uft_error_t d82_open(uft_disk_t* disk, const char* path, bool read_only) {
    d82_init_off();
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    d82_data_t* p = calloc(1, sizeof(d82_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = D82_TRACKS;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 29;
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = D82_SIDE_SECTORS * 2;
    return UFT_OK;
}

static void d82_close(uft_disk_t* disk) {
    d82_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d82_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    d82_data_t* p = disk->plugin_data;
    if (!p || !p->file || cyl >= D82_TRACKS || head > 1) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    long side_off = (head == 1) ? (long)D82_SIDE_SECTORS * 256 : 0;
    uint8_t buf[256];
    for (int s = 0; s < d82_spt[cyl]; s++) {
        if (fseek(p->file, side_off + (long)(d82_off[cyl] + s) * 256, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        if (fread(buf, 1, 256, p->file) != 256) { memset(buf, 0xE5, 256); }
        uft_format_add_sector(track, (uint8_t)s, buf, 256, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t d82_write_track(uft_disk_t* disk, int cyl, int head,
                                    const uft_track_t* track) {
    d82_data_t* p = disk->plugin_data;
    if (!p || !p->file || cyl >= D82_TRACKS || head > 1) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long side_off = (head == 1) ? (long)D82_SIDE_SECTORS * 256 : 0;
    for (size_t s = 0; s < track->sector_count && (int)s < d82_spt[cyl]; s++) {
        if (fseek(p->file, side_off + (long)(d82_off[cyl] + s) * 256, SEEK_SET) != 0)
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

const uft_format_plugin_t uft_format_plugin_d82 = {
    .name = "D82", .description = "Commodore 8250", .extensions = "d82",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d82_probe, .open = d82_open, .close = d82_close,
    .read_track = d82_read_track, .write_track = d82_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d82)

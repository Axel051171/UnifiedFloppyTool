#include <stdio.h>  // FIXED R18
#include "uft/uft_format_common.h"

#define D82_TRACKS 77
#define D82_SIZE   1066496
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

static bool d82_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == D82_SIZE) { *confidence = 75; return true; }
    return false;
}

static uft_error_t d82_open(uft_disk_t* disk, const char* path, bool read_only) {
    d82_init_off();
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    d82_data_t* p = calloc(1, sizeof(d82_data_t));
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = D82_TRACKS;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 29;
    disk->geometry.sector_size = 256;
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
    size_t side_off = (head == 1) ? 2083 * 256 : 0;
    uint8_t buf[256];
    for (int s = 0; s < d82_spt[cyl]; s++) {
        if (fseek(p->file, side_off + (d82_off[cyl] + s) * 256, SEEK_SET) != 0) { /* seek error */ }
        if (fread(buf, 1, 256, p->file) != 256) { /* I/O error */ }
        uft_format_add_sector(track, s, buf, 256, cyl, head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d82 = {
    .name = "D82", .description = "Commodore 8250", .extensions = "d82",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = d82_probe, .open = d82_open, .close = d82_close, .read_track = d82_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d82)

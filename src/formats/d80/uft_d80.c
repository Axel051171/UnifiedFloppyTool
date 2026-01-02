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

static bool d80_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == D80_SIZE) { *confidence = 75; return true; }
    return false;
}

static uft_error_t d80_open(uft_disk_t* disk, const char* path, bool read_only) {
    d80_init_off();
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    d80_data_t* pdata = calloc(1, sizeof(d80_data_t));
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
        fseek(p->file, (d80_off[cyl] + s) * 256, SEEK_SET);
        fread(buf, 1, 256, p->file);
        uft_format_add_sector(track, s, buf, 256, cyl, head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d80 = {
    .name = "D80", .description = "Commodore 8050", .extensions = "d80",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = d80_probe, .open = d80_open, .close = d80_close, .read_track = d80_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d80)

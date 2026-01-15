/**
 * @file uft_ssd.c
 * @brief BBC Micro SSD format core
 * @version 3.8.0
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t tracks, sides, spt; } ssd_data_t;

bool ssd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == 102400 || file_size == 204800 || file_size == 409600) {
        *confidence = 70; return true;
    }
    return false;
}

static uft_error_t ssd_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    (void)fseek(f, 0, SEEK_END); size_t sz = ftell(f); (void)fseek(f, 0, SEEK_SET);
    
    ssd_data_t* p = calloc(1, sizeof(ssd_data_t));
    p->file = f; p->spt = 10;
    bool dsd = (strstr(path, ".dsd") != NULL || strstr(path, ".DSD") != NULL);
    if (dsd || sz == 409600) { p->tracks = 80; p->sides = 2; }
    else if (sz == 204800) { p->tracks = 80; p->sides = 1; }
    else { p->tracks = 40; p->sides = 1; }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = 256;
    return UFT_OK;
}

static void ssd_close(uft_disk_t* disk) {
    ssd_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t ssd_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    ssd_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    size_t off = ((size_t)cyl * p->sides + head) * p->spt * 256;
    uint8_t buf[256];
    for (int s = 0; s < p->spt; s++) {
        if (fseek(p->file, off + s * 256, SEEK_SET) != 0) { /* seek error */ }
        if (fread(buf, 1, 256, p->file) != 256) { /* I/O error */ }
        uft_format_add_sector(track, s, buf, 256, cyl, head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_ssd = {
    .name = "SSD/DSD", .description = "BBC Micro Acorn DFS", .extensions = "ssd;dsd",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = ssd_probe, .open = ssd_open, .close = ssd_close, .read_track = ssd_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(ssd)

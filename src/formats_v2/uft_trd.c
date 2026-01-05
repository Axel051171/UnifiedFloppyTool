#include <stdio.h>  // FIXED R18
#include "uft/uft_format_common.h"

#define TRD_SEC_SIZE 256
#define TRD_SPT 16

typedef struct { FILE* file; uint8_t tracks, sides; } trd_data_t;

static bool trd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == 655360 || file_size == 327680 || file_size == 163840) {
        *confidence = 70;
        return true;
    }
    return false;
}

static uft_error_t trd_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t sz = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    trd_data_t* p = calloc(1, sizeof(trd_data_t));
    p->file = f;
    if (sz == 655360) { p->tracks = 80; p->sides = 2; }
    else if (sz == 327680) { p->tracks = 80; p->sides = 1; }
    else { p->tracks = 40; p->sides = 1; }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = TRD_SPT;
    disk->geometry.sector_size = TRD_SEC_SIZE;
    return UFT_OK;
}

static void trd_close(uft_disk_t* disk) {
    trd_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t trd_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    trd_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    size_t off = ((size_t)cyl * p->sides + head) * TRD_SPT * TRD_SEC_SIZE;
    uint8_t buf[TRD_SEC_SIZE];
    for (int s = 0; s < TRD_SPT; s++) {
        if (fseek(p->file, off + s * TRD_SEC_SIZE, SEEK_SET) != 0) { /* seek error */ }
        if (fread(buf, 1, TRD_SEC_SIZE, p->file) != TRD_SEC_SIZE) { /* I/O error */ }
        uft_format_add_sector(track, s, buf, TRD_SEC_SIZE, cyl, head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_trd = {
    .name = "TRD", .description = "TR-DOS Spectrum", .extensions = "trd",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = trd_probe, .open = trd_open, .close = trd_close, .read_track = trd_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(trd)

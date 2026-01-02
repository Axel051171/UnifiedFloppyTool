#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint8_t num_tracks; uint32_t* offsets; } g71_data_t;

static bool g71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 8 && memcmp(data, "GCR-1571", 8) == 0) { *confidence = 95; return true; }
    if (size >= 8 && memcmp(data, "GCR-1541", 8) == 0 && data[9] > 84) { *confidence = 70; return true; }
    return false;
}

static uft_error_t g71_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[12];
    fread(hdr, 1, 12, f);
    
    g71_data_t* p = calloc(1, sizeof(g71_data_t));
    p->file = f;
    p->num_tracks = hdr[9];
    p->offsets = calloc(p->num_tracks, sizeof(uint32_t));
    
    for (int i = 0; i < p->num_tracks; i++) {
        uint8_t buf[4];
        fread(buf, 1, 4, f);
        p->offsets[i] = uft_read_le32(buf);
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->num_tracks / 2;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 21;
    disk->geometry.sector_size = 256;
    return UFT_OK;
}

static void g71_close(uft_disk_t* disk) {
    g71_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p->offsets); free(p); disk->plugin_data = NULL; }
}

const uft_format_plugin_t uft_format_plugin_g71 = {
    .name = "G71", .description = "Commodore 1571 GCR", .extensions = "g71",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX,
    .probe = g71_probe, .open = g71_open, .close = g71_close,
};
UFT_REGISTER_FORMAT_PLUGIN(g71)

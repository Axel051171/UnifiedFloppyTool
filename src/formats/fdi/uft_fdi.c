/**
 * @file uft_fdi.c
 * @brief Formatted Disk Image FDI core
 * @version 3.8.0
 */
#include "uft/uft_format_common.h"

typedef struct { FILE* file; uint16_t cyls, heads; } fdi_data_t;

bool fdi_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 3 && memcmp(data, "FDI", 3) == 0) { *confidence = 95; return true; }
    return false;
}

static uft_error_t fdi_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[14];
    if (fread(hdr, 1, 14, f) != 14) { /* I/O error */ }
    if (memcmp(hdr, "FDI", 3) != 0) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }
    
    fdi_data_t* p = calloc(1, sizeof(fdi_data_t));
    p->file = f;
    p->cyls = uft_read_le16(&hdr[4]);
    p->heads = uft_read_le16(&hdr[6]);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->cyls;
    disk->geometry.heads = p->heads;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = 512;
    return UFT_OK;
}

static void fdi_close(uft_disk_t* disk) {
    fdi_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

const uft_format_plugin_t uft_format_plugin_fdi = {
    .name = "FDI", .description = "Formatted Disk Image", .extensions = "fdi",
    .format = UFT_FORMAT_FDI, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = fdi_probe, .open = fdi_open, .close = fdi_close,
};
UFT_REGISTER_FORMAT_PLUGIN(fdi)

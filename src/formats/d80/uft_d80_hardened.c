/**
 * @file uft_d80_hardened.c
 * @brief Commodore 8050 D80 Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define D80_TRACKS      77
#define D80_SECTOR_SIZE 256
#define D80_SIZE        533248
#define D80_TOTAL_SEC   2083

static const uint8_t d80_spt[77] = {
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    25,25,25,25,25,25,25,25,25,25,25,
    23,23,23,23,23,23,23,23,23,23,23,23,23
};

static uint16_t d80_offset[78];
static bool d80_init_done = false;

static void d80_init_offsets(void) {
    if (d80_init_done) return;
    d80_offset[0] = 0;
    for (int t = 0; t < D80_TRACKS; t++) {
        d80_offset[t + 1] = d80_offset[t] + d80_spt[t];
    }
    d80_init_done = true;
}

typedef struct { FILE* file; } d80_data_t;

static bool d80_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == D80_SIZE) { *confidence = 75; return true; }
    return false;
}

static uft_error_t d80_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    d80_init_offsets();
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    d80_data_t* p = calloc(1, sizeof(d80_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = D80_TRACKS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 29;
    disk->geometry.sector_size = D80_SECTOR_SIZE;
    disk->geometry.total_sectors = D80_TOTAL_SEC;
    
    return UFT_OK;
}

static void d80_close(uft_disk_t* disk) {
    if (!disk) return;
    d80_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d80_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    d80_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl < 0 || cyl >= D80_TRACKS) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    uint8_t buf[D80_SECTOR_SIZE];
    for (int s = 0; s < d80_spt[cyl]; s++) {
        size_t offset = ((size_t)d80_offset[cyl] + s) * D80_SECTOR_SIZE;
        if (fseek(p->file, (long)offset, SEEK_SET) != 0) continue;
        if (fread(buf, 1, D80_SECTOR_SIZE, p->file) != D80_SECTOR_SIZE) continue;
        uft_format_add_sector(track, (uint8_t)s, buf, D80_SECTOR_SIZE, (uint8_t)cyl, 0);
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d80_hardened = {
    .name = "D80", .description = "Commodore 8050 (HARDENED)", .extensions = "d80",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d80_probe, .open = d80_open, .close = d80_close, .read_track = d80_read_track,
};

/**
 * @file uft_d82_hardened.c  
 * @brief Commodore 8250 D82 Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define D82_TRACKS      77
#define D82_SECTOR_SIZE 256
#define D82_SIZE        1066496
#define D82_SIDE0_SEC   2083

static const uint8_t d82_spt[77] = {
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    25,25,25,25,25,25,25,25,25,25,25,
    23,23,23,23,23,23,23,23,23,23,23,23,23
};

static uint16_t d82_offset[78];
static bool d82_init_done = false;

static void d82_init_offsets(void) {
    if (d82_init_done) return;
    d82_offset[0] = 0;
    for (int t = 0; t < D82_TRACKS; t++) {
        d82_offset[t + 1] = d82_offset[t] + d82_spt[t];
    }
    d82_init_done = true;
}

typedef struct { FILE* file; } d82_data_t;

static bool d82_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == D82_SIZE) { *confidence = 75; return true; }
    return false;
}

static uft_error_t d82_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    d82_init_offsets();
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    d82_data_t* p = calloc(1, sizeof(d82_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    disk->plugin_data = p;
    disk->geometry.cylinders = D82_TRACKS;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 29;
    disk->geometry.sector_size = D82_SECTOR_SIZE;
    
    return UFT_OK;
}

static void d82_close(uft_disk_t* disk) {
    if (!disk) return;
    d82_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d82_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    d82_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || cyl >= D82_TRACKS || head < 0 || head > 1) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    size_t side_offset = (head == 1) ? (D82_SIDE0_SEC * D82_SECTOR_SIZE) : 0;
    uint8_t buf[D82_SECTOR_SIZE];
    
    for (int s = 0; s < d82_spt[cyl]; s++) {
        size_t offset = side_offset + ((size_t)d82_offset[cyl] + s) * D82_SECTOR_SIZE;
        if (fseek(p->file, (long)offset, SEEK_SET) != 0) continue;
        if (fread(buf, 1, D82_SECTOR_SIZE, p->file) != D82_SECTOR_SIZE) continue;
        uft_format_add_sector(track, (uint8_t)s, buf, D82_SECTOR_SIZE, (uint8_t)cyl, (uint8_t)head);
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d82_hardened = {
    .name = "D82", .description = "Commodore 8250 (HARDENED)", .extensions = "d82",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d82_probe, .open = d82_open, .close = d82_close, .read_track = d82_read_track,
};

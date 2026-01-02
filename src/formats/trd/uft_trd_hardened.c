/**
 * @file uft_trd_hardened.c
 * @brief TR-DOS TRD Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define TRD_SEC_SIZE 256
#define TRD_SPT      16
#define TRD_SIZE_80DS 655360
#define TRD_SIZE_40DS 327680
#define TRD_SIZE_80SS 327680
#define TRD_SIZE_40SS 163840

typedef struct {
    FILE*   file;
    uint8_t tracks;
    uint8_t sides;
} trd_data_t;

static bool trd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == TRD_SIZE_80DS || file_size == TRD_SIZE_40DS ||
        file_size == TRD_SIZE_80SS || file_size == TRD_SIZE_40SS) {
        *confidence = 70;
        return true;
    }
    return false;
}

static uft_error_t trd_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    size_t sz = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    
    trd_data_t* p = calloc(1, sizeof(trd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    
    if (sz == TRD_SIZE_80DS) { p->tracks = 80; p->sides = 2; }
    else if (sz == TRD_SIZE_40DS) { p->tracks = 40; p->sides = 2; }
    else if (sz == TRD_SIZE_80SS) { p->tracks = 80; p->sides = 1; }
    else { p->tracks = 40; p->sides = 1; }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = TRD_SPT;
    disk->geometry.sector_size = TRD_SEC_SIZE;
    
    return UFT_OK;
}

static void trd_close(uft_disk_t* disk) {
    if (!disk) return;
    trd_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t trd_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    trd_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    
    if (cyl < 0 || cyl >= p->tracks) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head >= p->sides) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    size_t off = ((size_t)cyl * p->sides + head) * TRD_SPT * TRD_SEC_SIZE;
    uint8_t buf[TRD_SEC_SIZE];
    
    for (int s = 0; s < TRD_SPT; s++) {
        if (fseek(p->file, (long)(off + s * TRD_SEC_SIZE), SEEK_SET) != 0) continue;
        if (fread(buf, 1, TRD_SEC_SIZE, p->file) != TRD_SEC_SIZE) continue;
        uft_format_add_sector(track, (uint8_t)s, buf, TRD_SEC_SIZE, (uint8_t)cyl, (uint8_t)head);
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_trd_hardened = {
    .name = "TRD", .description = "TR-DOS (HARDENED)", .extensions = "trd",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = trd_probe, .open = trd_open, .close = trd_close, .read_track = trd_read_track,
};

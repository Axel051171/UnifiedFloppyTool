#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define SSD_SEC_SIZE 256
#define SSD_SPT      10

typedef struct { FILE* file; uint8_t tracks; uint8_t sides; } ssd_data_t;

static bool ssd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == 102400 || file_size == 204800 || file_size == 409600) {
        *confidence = 70; return true;
    }
    return false;
}

static uft_error_t ssd_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    size_t sz = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    
    ssd_data_t* p = calloc(1, sizeof(ssd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    if (sz == 102400) { p->tracks = 40; p->sides = 1; }
    else if (sz == 204800) { p->tracks = 80; p->sides = 1; }
    else { p->tracks = 80; p->sides = 2; }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = SSD_SPT;
    disk->geometry.sector_size = SSD_SEC_SIZE;
    return UFT_OK;
}

static void ssd_close(uft_disk_t* disk) {
    if (!disk) return;
    ssd_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t ssd_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    ssd_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || cyl >= p->tracks || head < 0 || head >= p->sides) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    size_t off = ((size_t)cyl * p->sides + head) * SSD_SPT * SSD_SEC_SIZE;
    uint8_t buf[SSD_SEC_SIZE];
    
    for (int s = 0; s < SSD_SPT; s++) {
        if (fseek(p->file, (long)(off + s * SSD_SEC_SIZE), SEEK_SET) != 0) continue;
        if (fread(buf, 1, SSD_SEC_SIZE, p->file) != SSD_SEC_SIZE) continue;
        uft_format_add_sector(track, (uint8_t)s, buf, SSD_SEC_SIZE, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_ssd_hardened = {
    .name = "SSD", .description = "BBC Micro (HARDENED)", .extensions = "ssd;dsd",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = ssd_probe, .open = ssd_open, .close = ssd_close, .read_track = ssd_read_track,
};

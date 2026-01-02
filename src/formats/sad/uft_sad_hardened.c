#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define SAD_MAGIC "SAD!"
#define SAD_SEC_SIZE 512

typedef struct { FILE* file; uint8_t tracks; uint8_t sides; uint8_t spt; bool has_header; } sad_data_t;

static bool sad_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 4 && memcmp(data, SAD_MAGIC, 4) == 0) {
        *confidence = 95; return true;
    }
    if (file_size == 819200) { *confidence = 60; return true; }
    return false;
}

static uft_error_t sad_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[22];
    if (fread(hdr, 1, 22, f) != 22) { fclose(f); return UFT_ERROR_FILE_READ; }
    
    sad_data_t* p = calloc(1, sizeof(sad_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    
    if (memcmp(hdr, SAD_MAGIC, 4) == 0) {
        p->has_header = true;
        p->sides = hdr[4];
        p->tracks = hdr[5];
        p->spt = hdr[6];
    } else {
        p->has_header = false;
        p->tracks = 80; p->sides = 2; p->spt = 10;
        fseek(f, 0, SEEK_SET);
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = p->spt;
    disk->geometry.sector_size = SAD_SEC_SIZE;
    return UFT_OK;
}

static void sad_close(uft_disk_t* disk) {
    if (!disk) return;
    sad_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t sad_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    sad_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (cyl < 0 || cyl >= p->tracks || head < 0 || head >= p->sides) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    size_t hdr_off = p->has_header ? 22 : 0;
    size_t track_sz = (size_t)p->spt * SAD_SEC_SIZE;
    size_t off = hdr_off + ((size_t)cyl * p->sides + head) * track_sz;
    uint8_t buf[SAD_SEC_SIZE];
    
    for (int s = 0; s < p->spt; s++) {
        if (fseek(p->file, (long)(off + s * SAD_SEC_SIZE), SEEK_SET) != 0) continue;
        if (fread(buf, 1, SAD_SEC_SIZE, p->file) != SAD_SEC_SIZE) continue;
        uft_format_add_sector(track, (uint8_t)s, buf, SAD_SEC_SIZE, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_sad_hardened = {
    .name = "SAD", .description = "Sam Coupe (HARDENED)", .extensions = "sad;mgt",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = sad_probe, .open = sad_open, .close = sad_close, .read_track = sad_read_track,
};

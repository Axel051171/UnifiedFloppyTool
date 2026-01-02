#include "uft/uft_format_common.h"

#define SAD_SIZE 819200

typedef struct { FILE* file; bool header; } sad_data_t;

static bool sad_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 4 && memcmp(data, "SAD!", 4) == 0) { *confidence = 95; return true; }
    if (file_size == SAD_SIZE) { *confidence = 70; return true; }
    return false;
}

static uft_error_t sad_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[22];
    fread(hdr, 1, 22, f);
    bool has_hdr = (memcmp(hdr, "SAD!", 4) == 0);
    if (!has_hdr) fseek(f, 0, SEEK_SET);
    
    sad_data_t* p = calloc(1, sizeof(sad_data_t));
    p->file = f; p->header = has_hdr;
    
    disk->plugin_data = p;
    disk->geometry.cylinders = has_hdr ? hdr[5] : 80;
    disk->geometry.heads = has_hdr ? hdr[4] : 2;
    disk->geometry.sectors = has_hdr ? hdr[6] : 10;
    disk->geometry.sector_size = 512;
    return UFT_OK;
}

static void sad_close(uft_disk_t* disk) {
    sad_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t sad_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    sad_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    size_t off = (p->header ? 22 : 0) + ((size_t)cyl * disk->geometry.heads + head) * 
                 disk->geometry.sectors * 512;
    uint8_t buf[512];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        fseek(p->file, off + s * 512, SEEK_SET);
        fread(buf, 1, 512, p->file);
        uft_format_add_sector(track, s, buf, 512, cyl, head);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_sad = {
    .name = "SAD", .description = "Sam Coupe", .extensions = "sad;mgt",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = sad_probe, .open = sad_open, .close = sad_close, .read_track = sad_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(sad)

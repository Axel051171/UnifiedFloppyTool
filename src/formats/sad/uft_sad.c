/**
 * @file uft_sad.c
 * @brief SAM Coupe SAD format — read + write
 * @version 3.8.1
 */
#include "uft/uft_format_common.h"

#define SAD_SIZE 819200

typedef struct { FILE* file; bool header; } sad_data_t;

bool sad_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 4 && memcmp(data, "SAD!", 4) == 0) { *confidence = 95; return true; }
    if (file_size == SAD_SIZE) { *confidence = 70; return true; }
    return false;
}

static uft_error_t sad_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[22];
    if (fread(hdr, 1, 22, f) != 22) { fclose(f); return UFT_ERROR_IO; }
    bool has_hdr = (memcmp(hdr, "SAD!", 4) == 0);
    if (!has_hdr) {
        if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }
    }

    sad_data_t* p = calloc(1, sizeof(sad_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f; p->header = has_hdr;

    disk->plugin_data = p;
    disk->geometry.cylinders = has_hdr ? hdr[5] : 80;
    disk->geometry.heads = has_hdr ? hdr[4] : 2;
    disk->geometry.sectors = has_hdr ? hdr[6] : 10;
    disk->geometry.sector_size = 512;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders *
                                   disk->geometry.heads * disk->geometry.sectors;
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
    long off = (long)((p->header ? 22 : 0) +
               ((size_t)cyl * disk->geometry.heads + head) *
               disk->geometry.sectors * 512);
    uint8_t buf[512];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        if (fseek(p->file, off + s * 512, SEEK_SET) != 0) return UFT_ERROR_IO;
        if (fread(buf, 1, 512, p->file) != 512) { memset(buf, 0xE5, 512); }
        uft_format_add_sector(track, (uint8_t)s, buf, 512, (uint8_t)cyl, (uint8_t)head);
    }
    return UFT_OK;
}

static uft_error_t sad_write_track(uft_disk_t* disk, int cyl, int head,
                                    const uft_track_t* track) {
    sad_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    long off = (long)((p->header ? 22 : 0) +
               ((size_t)cyl * disk->geometry.heads + head) *
               disk->geometry.sectors * 512);
    for (size_t s = 0; s < track->sector_count && (int)s < disk->geometry.sectors; s++) {
        if (fseek(p->file, off + (long)s * 512, SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[512];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, 512); data = pad;
        }
        if (fwrite(data, 1, 512, p->file) != 512) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_sad = {
    .name = "SAD", .description = "Sam Coupe", .extensions = "sad;mgt",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = sad_probe, .open = sad_open, .close = sad_close,
    .read_track = sad_read_track, .write_track = sad_write_track,
};
UFT_REGISTER_FORMAT_PLUGIN(sad)

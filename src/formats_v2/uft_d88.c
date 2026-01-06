#include "uft/uft_format_common.h"

#define D88_HEADER 0x2B0

typedef struct { FILE* file; uint8_t media; uint32_t track_off[164]; } d88_data_t;

static bool d88_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < D88_HEADER) return false;
    uint32_t dsz = uft_read_le32(data + 0x1C);
    uint8_t media = data[0x1B];
    if (dsz <= file_size && (media == 0 || media == 0x10 || media == 0x20)) {
        *confidence = 90; return true;
    }
    return false;
}

static uft_error_t d88_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[D88_HEADER];
    if (fread(hdr, 1, D88_HEADER, f) != D88_HEADER) { /* I/O error */ }
    d88_data_t* p = calloc(1, sizeof(d88_data_t));
    p->file = f;
    p->media = hdr[0x1B];
    for (int i = 0; i < 164; i++) p->track_off[i] = uft_read_le32(&hdr[0x20 + i*4]);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = (p->media == 0x20) ? 77 : 80;
    disk->geometry.heads = 2;
    disk->geometry.sectors = (p->media == 0x20) ? 8 : 16;
    disk->geometry.sector_size = (p->media == 0x20) ? 1024 : 256;
    return UFT_OK;
}

static void d88_close(uft_disk_t* disk) {
    d88_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t d88_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    d88_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    
    int idx = cyl * 2 + head;
    if (idx >= 164 || p->track_off[idx] == 0) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    if (fseek(p->file, p->track_off[idx], SEEK_SET) != 0) { /* seek error */ }
    uint8_t sec_hdr[16];
    for (int s = 0; s < disk->geometry.sectors; s++) {
        if (fread(sec_hdr, 1, 16, p->file) != 16) break;
        uint16_t dsize = uft_read_le16(&sec_hdr[14]);
        if (dsize == 0 || dsize > 8192) break;
        
        uint8_t* buf = malloc(dsize);
        if (fread(buf, 1, dsize, p->file) != dsize) { /* I/O error */ }
        uft_format_add_sector(track, sec_hdr[2] - 1, buf, dsize, cyl, head);
        free(buf);
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d88 = {
    .name = "D88", .description = "PC-88/PC-98", .extensions = "d88;88d;d98",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = d88_probe, .open = d88_open, .close = d88_close, .read_track = d88_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(d88)

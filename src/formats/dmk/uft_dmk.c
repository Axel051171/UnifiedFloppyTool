#include "uft/uft_format_common.h"

#define DMK_HDR 16
#define DMK_IDAM_SIZE 128

typedef struct { FILE* file; uint8_t tracks; uint16_t track_len; bool ss; } dmk_data_t;

static bool dmk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < DMK_HDR) return false;
    uint8_t tracks = data[1];
    uint16_t tlen = uft_read_le16(&data[2]);
    if (tracks > 0 && tracks <= 96 && tlen >= 1000 && tlen <= 20000) {
        *confidence = 85; return true;
    }
    return false;
}

static uft_error_t dmk_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[DMK_HDR];
    fread(hdr, 1, DMK_HDR, f);
    
    dmk_data_t* p = calloc(1, sizeof(dmk_data_t));
    p->file = f;
    p->tracks = hdr[1];
    p->track_len = uft_read_le16(&hdr[2]);
    p->ss = (hdr[4] & 0x10) != 0;
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->ss ? 1 : 2;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = 256;
    return UFT_OK;
}

static void dmk_close(uft_disk_t* disk) {
    dmk_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t dmk_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    dmk_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    uft_track_init(track, cyl, head);
    
    size_t off = DMK_HDR + ((size_t)cyl * (p->ss ? 1 : 2) + head) * p->track_len;
    fseek(p->file, off, SEEK_SET);
    
    uint8_t* tbuf = malloc(p->track_len);
    fread(tbuf, 1, p->track_len, p->file);
    
    // Parse IDAM table
    for (int i = 0; i < 64; i++) {
        uint16_t ptr = uft_read_le16(&tbuf[i * 2]);
        if (ptr == 0 || ptr == 0xFFFF) break;
        uint16_t idam_off = (ptr & 0x3FFF) - DMK_IDAM_SIZE;
        if (idam_off >= p->track_len - 20) continue;
        
        uint8_t* idam = &tbuf[DMK_IDAM_SIZE + idam_off];
        if (idam[0] != 0xFE) continue;
        
        uint8_t sec_id = idam[3], sz = idam[4] & 3;
        uint16_t sec_sz = 128 << sz;
        
        // Find DAM
        for (int j = 7; j < 60 && idam_off + j < p->track_len - sec_sz; j++) {
            if (idam[j] == 0xFB || idam[j] == 0xF8) {
                uft_format_add_sector(track, sec_id - 1, &idam[j + 1], sec_sz, cyl, head);
                break;
            }
        }
    }
    
    free(tbuf);
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_dmk = {
    .name = "DMK", .description = "TRS-80 David Keil", .extensions = "dmk",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ,
    .probe = dmk_probe, .open = dmk_open, .close = dmk_close, .read_track = dmk_read_track,
};
UFT_REGISTER_FORMAT_PLUGIN(dmk)

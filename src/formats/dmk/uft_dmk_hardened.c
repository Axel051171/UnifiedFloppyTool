#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define DMK_HEADER_SIZE 16
#define DMK_IDAM_TABLE_SIZE 128
#define DMK_MAX_SECTORS 64

typedef struct {
    FILE* file;
    uint8_t tracks;
    uint8_t sides;
    uint16_t track_len;
    bool single_density;
} dmk_data_t;

static bool dmk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < DMK_HEADER_SIZE) return false;
    uint16_t track_len = uft_read_le16(&data[2]);
    uint8_t tracks = data[1];
    if (tracks > 0 && tracks <= 86 && track_len >= 128 && track_len <= 16384) {
        size_t expected = DMK_HEADER_SIZE + (size_t)tracks * track_len;
        if (file_size >= expected * 0.9 && file_size <= expected * 1.1) {
            *confidence = 85; return true;
        }
    }
    return false;
}

static uft_error_t dmk_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[DMK_HEADER_SIZE];
    if (fread(hdr, 1, DMK_HEADER_SIZE, f) != DMK_HEADER_SIZE) {
        fclose(f); return UFT_ERROR_FILE_READ;
    }
    
    dmk_data_t* p = calloc(1, sizeof(dmk_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    p->tracks = hdr[1];
    p->track_len = uft_read_le16(&hdr[2]);
    p->sides = (hdr[4] & 0x10) ? 1 : 2;
    p->single_density = (hdr[4] & 0x40) != 0;
    
    if (p->tracks == 0 || p->track_len < 128) {
        free(p); fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks / p->sides;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = 256;
    return UFT_OK;
}

static void dmk_close(uft_disk_t* disk) {
    if (!disk) return;
    dmk_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t dmk_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    dmk_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    
    int track_idx = cyl * p->sides + head;
    if (track_idx < 0 || track_idx >= p->tracks) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    size_t offset = DMK_HEADER_SIZE + (size_t)track_idx * p->track_len;
    if (fseek(p->file, (long)offset, SEEK_SET) != 0) return UFT_ERROR_FILE_SEEK;
    
    uint8_t* track_data = malloc(p->track_len);
    if (!track_data) return UFT_ERROR_NO_MEMORY;
    
    if (fread(track_data, 1, p->track_len, p->file) != p->track_len) {
        free(track_data);
        return UFT_ERROR_FILE_READ;
    }
    
    // Parse IDAM table
    for (int i = 0; i < 64; i++) {
        uint16_t ptr = uft_read_le16(&track_data[i * 2]);
        if (ptr == 0) break;
        
        uint16_t idam_off = ptr & 0x3FFF;
        if (idam_off >= p->track_len - 10) continue;
        
        uint8_t* idam = &track_data[idam_off];
        if (idam[0] != 0xFE) continue;
        
        uint8_t sec_cyl = idam[1], sec_head = idam[2];
        uint8_t sec_id = idam[3], size_code = idam[4];
        
        uint16_t sec_size = 128 << (size_code & 7);
        if (sec_size > 8192) sec_size = 8192;
        
        // Find data mark
        for (int j = idam_off + 7; j < p->track_len - sec_size - 3; j++) {
            if (track_data[j] == 0xFB || track_data[j] == 0xF8) {
                uft_format_add_sector(track, sec_id > 0 ? sec_id - 1 : 0,
                                     &track_data[j + 1], sec_size,
                                     sec_cyl, sec_head);
                break;
            }
        }
    }
    
    free(track_data);
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_dmk_hardened = {
    .name = "DMK", .description = "TRS-80 DMK (HARDENED)", .extensions = "dmk",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = dmk_probe, .open = dmk_open, .close = dmk_close, .read_track = dmk_read_track,
};

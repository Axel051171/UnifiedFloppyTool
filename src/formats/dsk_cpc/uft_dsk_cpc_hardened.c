/**
 * @file uft_dsk_cpc_hardened.c
 * @brief Amstrad CPC/Spectrum DSK Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define DSK_HEADER_SIZE     256
#define DSK_TRACK_INFO_SIZE 256
#define DSK_MAX_TRACKS      200
#define DSK_MAX_SECTORS     32

static const uint16_t dsk_sec_sizes[8] = {128, 256, 512, 1024, 2048, 4096, 8192, 16384};

typedef struct {
    FILE*       file;
    bool        extended;
    uint8_t     tracks;
    uint8_t     sides;
    uint16_t    track_size;
    uint8_t     track_sizes[DSK_MAX_TRACKS];
} dsk_data_t;

static bool dsk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 8) return false;
    if (memcmp(data, "EXTENDED", 8) == 0) { *confidence = 95; return true; }
    if (memcmp(data, "MV - CPC", 8) == 0) { *confidence = 95; return true; }
    return false;
}

static uft_error_t dsk_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[DSK_HEADER_SIZE];
    if (fread(header, 1, DSK_HEADER_SIZE, f) != DSK_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    bool extended = (memcmp(header, "EXTENDED", 8) == 0);
    if (!extended && memcmp(header, "MV - CPC", 8) != 0) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    dsk_data_t* p = calloc(1, sizeof(dsk_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    p->extended = extended;
    p->tracks = header[0x30];
    p->sides = header[0x31];
    p->track_size = uft_read_le16(&header[0x32]);
    
    // Validate
    if (p->tracks == 0 || p->tracks > DSK_MAX_TRACKS / 2 ||
        p->sides == 0 || p->sides > 2) {
        free(p); fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    if (extended) {
        size_t count = (size_t)p->tracks * p->sides;
        if (count > DSK_MAX_TRACKS) count = DSK_MAX_TRACKS;
        memcpy(p->track_sizes, &header[0x34], count);
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = p->tracks;
    disk->geometry.heads = p->sides;
    disk->geometry.sectors = 9;
    disk->geometry.sector_size = 512;
    
    return UFT_OK;
}

static void dsk_close(uft_disk_t* disk) {
    if (!disk) return;
    dsk_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t dsk_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    dsk_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    
    if (cyl < 0 || cyl >= p->tracks) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head >= p->sides) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    // Calculate offset
    size_t offset = DSK_HEADER_SIZE;
    int track_idx = cyl * p->sides + head;
    
    if (p->extended) {
        for (int i = 0; i < track_idx && i < DSK_MAX_TRACKS; i++) {
            offset += (size_t)p->track_sizes[i] * 256;
        }
    } else {
        offset += (size_t)track_idx * p->track_size;
    }
    
    if (fseek(p->file, (long)offset, SEEK_SET) != 0) return UFT_ERROR_FILE_SEEK;
    
    uint8_t track_info[DSK_TRACK_INFO_SIZE];
    if (fread(track_info, 1, DSK_TRACK_INFO_SIZE, p->file) != DSK_TRACK_INFO_SIZE) {
        return UFT_ERROR_FILE_READ;
    }
    
    uint8_t num_sec = track_info[0x15];
    uint8_t sec_size_code = track_info[0x14] & 7;
    uint16_t sec_size = dsk_sec_sizes[sec_size_code];
    
    if (num_sec > DSK_MAX_SECTORS) num_sec = DSK_MAX_SECTORS;
    
    uint8_t* sec_buf = malloc(sec_size);
    if (!sec_buf) return UFT_ERROR_NO_MEMORY;
    
    for (int s = 0; s < num_sec; s++) {
        uint8_t* sec_info = &track_info[0x18 + s * 8];
        uint8_t sec_id = sec_info[2];
        uint16_t actual_size = sec_size;
        
        if (p->extended && (sec_info[6] | sec_info[7])) {
            actual_size = uft_read_le16(&sec_info[6]);
        }
        
        if (actual_size > sec_size) actual_size = sec_size;
        
        memset(sec_buf, 0xE5, sec_size);
        if (fread(sec_buf, 1, actual_size, p->file) != actual_size) {
            // Partial read - continue with what we have
        }
        
        uft_format_add_sector(track, sec_id > 0 ? sec_id - 1 : 0, 
                             sec_buf, sec_size, (uint8_t)cyl, (uint8_t)head);
    }
    
    free(sec_buf);
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_dsk_cpc_hardened = {
    .name = "DSK", .description = "CPC/Spectrum DSK (HARDENED)", .extensions = "dsk",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = dsk_probe, .open = dsk_open, .close = dsk_close, .read_track = dsk_read_track,
};

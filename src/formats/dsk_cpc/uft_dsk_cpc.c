/**
 * @file uft_dsk_cpc.c
 * @brief Amstrad CPC/Spectrum DSK Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"

#define DSK_HEADER_SIZE     256
#define DSK_TRACK_INFO_SIZE 256
static const uint16_t dsk_sector_sizes[8] = { 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };

typedef struct {
    FILE*       file;
    bool        extended;
    uint8_t     tracks, sides;
    uint16_t    track_size;
    uint8_t     track_sizes[200];
} dsk_data_t;

bool dsk_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 8) return false;
    if (memcmp(data, "EXTENDED", 8) == 0) { *confidence = 95; return true; }
    if (memcmp(data, "MV - CPC", 8) == 0) { *confidence = 95; return true; }
    return false;
}

static uft_error_t dsk_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[DSK_HEADER_SIZE];
    if (fread(header, 1, DSK_HEADER_SIZE, f) != DSK_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    bool extended = (memcmp(header, "EXTENDED", 8) == 0);
    if (!extended && memcmp(header, "MV - CPC", 8) != 0) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    dsk_data_t* pdata = calloc(1, sizeof(dsk_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->extended = extended;
    pdata->tracks = header[0x30];
    pdata->sides = header[0x31];
    pdata->track_size = uft_read_le16(&header[0x32]);
    
    if (extended) {
        memcpy(pdata->track_sizes, &header[0x34], pdata->tracks * pdata->sides);
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->tracks;
    disk->geometry.heads = pdata->sides;
    disk->geometry.sectors = 9;
    disk->geometry.sector_size = 512;
    
    return UFT_OK;
}

static void dsk_close(uft_disk_t* disk) {
    dsk_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t dsk_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    dsk_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    
    uft_track_init(track, cyl, head);
    
    // Calculate offset
    size_t offset = DSK_HEADER_SIZE;
    int track_idx = cyl * pdata->sides + head;
    
    if (pdata->extended) {
        for (int i = 0; i < track_idx; i++) {
            offset += pdata->track_sizes[i] * 256;
        }
    } else {
        offset += track_idx * pdata->track_size;
    }
    
    if (fseek(pdata->file, offset, SEEK_SET) != 0) { /* seek error */ }
    uint8_t track_info[DSK_TRACK_INFO_SIZE];
    if (fread(track_info, 1, DSK_TRACK_INFO_SIZE, pdata->file) != DSK_TRACK_INFO_SIZE)
        return UFT_ERROR_FILE_READ;
    
    uint8_t num_sec = track_info[0x15];
    uint8_t sec_size_code = track_info[0x14];
    uint16_t sec_size = dsk_sector_sizes[sec_size_code & 7];
    
    uint8_t* sec_buf = malloc(sec_size);
    for (int s = 0; s < num_sec; s++) {
        uint8_t* sec_info = &track_info[0x18 + s * 8];
        uint8_t sec_id = sec_info[2];
        uint16_t actual_size = sec_size;
        
        if (pdata->extended && sec_info[6] + sec_info[7] > 0) {
            actual_size = uft_read_le16(&sec_info[6]);
        }
        
        memset(sec_buf, 0xE5, sec_size);
        if (fread(sec_buf, 1, actual_size, pdata->file) != actual_size) { /* I/O error */ }
        uft_format_add_sector(track, sec_id - 1, sec_buf, sec_size, cyl, head);
    }
    free(sec_buf);
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_dsk_cpc = {
    .name = "DSK",
    .description = "Amstrad CPC/Spectrum DSK",
    .extensions = "dsk",
    .version = 0x00010000,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = dsk_probe,
    .open = dsk_open,
    .close = dsk_close,
    .read_track = dsk_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(dsk_cpc)

/**
 * @file uft_imd.c
 * @brief ImageDisk (IMD) Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"

#define IMD_HEADER_END      0x1A
#define IMD_SEC_UNAVAILABLE 0x00
#define IMD_SEC_NORMAL      0x01
#define IMD_SEC_COMPRESSED  0x02

static const uint16_t imd_sector_sizes[7] = { 128, 256, 512, 1024, 2048, 4096, 8192 };

typedef struct {
    FILE*   file;
    long    data_start;
    char    comment[256];
    uint8_t max_cyl, max_head, max_sec;
    uint16_t sector_size;
} imd_data_t;

static bool imd_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 4 && data[0] == 'I' && data[1] == 'M' && data[2] == 'D' && data[3] == ' ') {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t imd_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    // Skip header until 0x1A
    int ch;
    size_t comment_len = 0;
    char comment[256] = {0};
    while ((ch = fgetc(f)) != EOF && ch != IMD_HEADER_END && comment_len < 255) {
        comment[comment_len++] = ch;
    }
    if (ch != IMD_HEADER_END) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }
    
    imd_data_t* pdata = calloc(1, sizeof(imd_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->data_start = ftell(f);
    strncpy(pdata->comment, comment, 255);
    
    // First pass: determine geometry
    uint8_t max_cyl = 0, max_head = 0, max_sec = 0;
    uint8_t size_code = 2;
    
    while (!feof(f)) {
        uint8_t hdr[5];
        if (fread(hdr, 1, 5, f) != 5) break;
        uint8_t mode = hdr[0], cyl = hdr[1], head = hdr[2] & 1;
        uint8_t num_sec = hdr[3], sz = hdr[4];
        
        if (mode > 5) break;
        if (cyl > max_cyl) max_cyl = cyl;
        if (head > max_head) max_head = head;
        if (num_sec > max_sec) max_sec = num_sec;
        if (sz > size_code && sz < 7) size_code = sz;
        
        fseek(f, num_sec, SEEK_CUR);  // sector map
        if (hdr[2] & 0x80) fseek(f, num_sec, SEEK_CUR);  // cyl map
        if (hdr[2] & 0x40) fseek(f, num_sec, SEEK_CUR);  // head map
        
        uint16_t sec_size = imd_sector_sizes[sz < 7 ? sz : 2];
        for (int s = 0; s < num_sec; s++) {
            uint8_t dtype = fgetc(f);
            if (dtype == IMD_SEC_UNAVAILABLE) continue;
            else if (dtype == IMD_SEC_COMPRESSED || dtype == 4 || dtype == 6 || dtype == 8)
                if (fseek(f, 1, SEEK_CUR) != 0) { /* seek error */ }
            else
                if (fseek(f, sec_size, SEEK_CUR) != 0) { /* seek error */ }
        }
    }
    
    pdata->max_cyl = max_cyl;
    pdata->max_head = max_head;
    pdata->max_sec = max_sec;
    pdata->sector_size = imd_sector_sizes[size_code];
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = max_head + 1;
    disk->geometry.sectors = max_sec;
    disk->geometry.sector_size = pdata->sector_size;
    
    return UFT_OK;
}

static void imd_close(uft_disk_t* disk) {
    imd_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t imd_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    imd_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    
    uft_track_init(track, cyl, head);
    if (fseek(pdata->file, pdata->data_start, SEEK_SET) != 0) { /* seek error */ }
    while (!feof(pdata->file)) {
        uint8_t hdr[5];
        if (fread(hdr, 1, 5, pdata->file) != 5) break;
        
        uint8_t mode = hdr[0], t_cyl = hdr[1], t_head = hdr[2] & 1;
        uint8_t num_sec = hdr[3], sz = hdr[4];
        if (mode > 5) break;
        
        uint16_t sec_size = imd_sector_sizes[sz < 7 ? sz : 2];
        uint8_t sec_map[256];
        if (fread(sec_map, 1, num_sec, pdata->file) != num_sec) { /* I/O error */ }
        if (hdr[2] & 0x80) fseek(pdata->file, num_sec, SEEK_CUR);
        if (hdr[2] & 0x40) fseek(pdata->file, num_sec, SEEK_CUR);
        
        if (t_cyl == cyl && t_head == head) {
            uint8_t* sec_buf = malloc(sec_size);
            for (int s = 0; s < num_sec; s++) {
                uint8_t dtype = fgetc(pdata->file);
                memset(sec_buf, 0, sec_size);
                
                if (dtype == IMD_SEC_UNAVAILABLE) {
                    // skip
                } else if (dtype == IMD_SEC_COMPRESSED || dtype == 4 || dtype == 6 || dtype == 8) {
                    uint8_t fill = fgetc(pdata->file);
                    memset(sec_buf, fill, sec_size);
                } else {
                    if (fread(sec_buf, 1, sec_size, pdata->file) != sec_size) { /* I/O error */ }
                }
                uft_format_add_sector(track, sec_map[s] - 1, sec_buf, sec_size, cyl, head);
            }
            free(sec_buf);
            return UFT_OK;
        } else {
            for (int s = 0; s < num_sec; s++) {
                uint8_t dtype = fgetc(pdata->file);
                if (dtype == IMD_SEC_UNAVAILABLE) continue;
                else if (dtype == IMD_SEC_COMPRESSED || dtype == 4 || dtype == 6 || dtype == 8)
                    if (fseek(pdata->file, 1, SEEK_CUR) != 0) { /* seek error */ }
                else
                    if (fseek(pdata->file, sec_size, SEEK_CUR) != 0) { /* seek error */ }
            }
        }
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_imd = {
    .name = "IMD",
    .description = "ImageDisk (Dave Dunfield)",
    .extensions = "imd",
    .version = 0x00010000,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = imd_probe,
    .open = imd_open,
    .close = imd_close,
    .read_track = imd_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(imd)

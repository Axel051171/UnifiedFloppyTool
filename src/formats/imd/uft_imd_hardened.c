/**
 * @file uft_imd_hardened.c
 * @brief ImageDisk IMD Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define IMD_HEADER_END      0x1A
#define IMD_SEC_UNAVAIL     0x00
#define IMD_SEC_NORMAL      0x01
#define IMD_SEC_COMPRESSED  0x02
#define IMD_MAX_TRACKS      86
#define IMD_MAX_HEADS       2
#define IMD_MAX_SECTORS     64
#define IMD_COMMENT_MAX     4096

static const uint16_t imd_sec_sizes[8] = {128, 256, 512, 1024, 2048, 4096, 8192, 16384};

typedef struct {
    FILE*   file;
    long    data_start;
    char    comment[IMD_COMMENT_MAX];
    uint8_t max_cyl;
    uint8_t max_head;
    uint8_t max_sec;
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
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    // Skip header until 0x1A
    int ch;
    size_t comment_len = 0;
    char comment[IMD_COMMENT_MAX];
    
    while ((ch = fgetc(f)) != EOF && ch != IMD_HEADER_END && comment_len < IMD_COMMENT_MAX - 1) {
        comment[comment_len++] = (char)ch;
    }
    comment[comment_len] = '\0';
    
    if (ch != IMD_HEADER_END) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    imd_data_t* p = calloc(1, sizeof(imd_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    p->data_start = ftell(f);
    memcpy(p->comment, comment, comment_len + 1);
    
    // First pass: determine geometry
    uint8_t max_cyl = 0, max_head = 0, max_sec = 0;
    uint8_t size_code = 2;
    
    while (!feof(f)) {
        uint8_t hdr[5];
        if (fread(hdr, 1, 5, f) != 5) break;
        
        uint8_t mode = hdr[0], cyl = hdr[1], head = hdr[2] & 1;
        uint8_t num_sec = hdr[3], sz = hdr[4];
        
        if (mode > 5) break;  // Invalid mode
        
        if (cyl > max_cyl) max_cyl = cyl;
        if (head > max_head) max_head = head;
        if (num_sec > max_sec) max_sec = num_sec;
        if (sz < 7 && sz > size_code) size_code = sz;
        
        // Skip sector map
        if (fseek(f, num_sec, SEEK_CUR) != 0) break;
        
        // Skip optional cylinder/head maps
        if (hdr[2] & 0x80) { if (fseek(f, num_sec, SEEK_CUR) != 0) break; }
        if (hdr[2] & 0x40) { if (fseek(f, num_sec, SEEK_CUR) != 0) break; }
        
        // Skip sector data
        uint16_t sec_size = imd_sec_sizes[sz < 7 ? sz : 2];
        for (int s = 0; s < num_sec; s++) {
            int dtype = fgetc(f);
            if (dtype == EOF) break;
            
            if (dtype == IMD_SEC_UNAVAIL) continue;
            else if (dtype == IMD_SEC_COMPRESSED || dtype == 4 || dtype == 6 || dtype == 8) {
                if (fseek(f, 1, SEEK_CUR) != 0) { /* seek error */ }
            } else {
                if (fseek(f, sec_size, SEEK_CUR) != 0) { /* seek error */ }
            }
        }
    }
    
    p->max_cyl = max_cyl;
    p->max_head = max_head;
    p->max_sec = max_sec;
    p->sector_size = imd_sec_sizes[size_code < 7 ? size_code : 2];
    
    disk->plugin_data = p;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = max_head + 1;
    disk->geometry.sectors = max_sec;
    disk->geometry.sector_size = p->sector_size;
    
    return UFT_OK;
}

static void imd_close(uft_disk_t* disk) {
    if (!disk) return;
    imd_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t imd_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    imd_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    
    if (cyl < 0 || cyl > p->max_cyl) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head > p->max_head) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    if (fseek(p->file, p->data_start, SEEK_SET) != 0) return UFT_ERROR_FILE_SEEK;
    
    while (!feof(p->file)) {
        uint8_t hdr[5];
        if (fread(hdr, 1, 5, p->file) != 5) break;
        
        uint8_t mode = hdr[0], t_cyl = hdr[1], t_head = hdr[2] & 1;
        uint8_t num_sec = hdr[3], sz = hdr[4];
        
        if (mode > 5) break;
        if (num_sec > IMD_MAX_SECTORS) num_sec = IMD_MAX_SECTORS;
        
        uint16_t sec_size = imd_sec_sizes[sz < 7 ? sz : 2];
        
        uint8_t sec_map[IMD_MAX_SECTORS];
        if (fread(sec_map, 1, num_sec, p->file) != num_sec) break;
        
        if (hdr[2] & 0x80) fseek(p->file, num_sec, SEEK_CUR);
        if (hdr[2] & 0x40) fseek(p->file, num_sec, SEEK_CUR);
        
        if (t_cyl == cyl && t_head == head) {
            uint8_t* sec_buf = malloc(sec_size);
            if (!sec_buf) return UFT_ERROR_NO_MEMORY;
            
            for (int s = 0; s < num_sec; s++) {
                int dtype = fgetc(p->file);
                if (dtype == EOF) break;
                
                memset(sec_buf, 0, sec_size);
                
                if (dtype == IMD_SEC_UNAVAIL) {
                    // Skip
                } else if (dtype == IMD_SEC_COMPRESSED || dtype == 4 || dtype == 6 || dtype == 8) {
                    int fill = fgetc(p->file);
                    if (fill != EOF) memset(sec_buf, fill, sec_size);
                } else {
                    if (fread(sec_buf, 1, sec_size, p->file) != sec_size) { /* I/O error */ }
                }
                
                uft_format_add_sector(track, sec_map[s] > 0 ? sec_map[s] - 1 : 0,
                                     sec_buf, sec_size, (uint8_t)cyl, (uint8_t)head);
            }
            
            free(sec_buf);
            return UFT_OK;
        } else {
            // Skip this track's sectors
            for (int s = 0; s < num_sec; s++) {
                int dtype = fgetc(p->file);
                if (dtype == EOF) break;
                if (dtype == IMD_SEC_UNAVAIL) continue;
                else if (dtype == IMD_SEC_COMPRESSED || dtype == 4 || dtype == 6 || dtype == 8) {
                    if (fseek(p->file, 1, SEEK_CUR) != 0) { /* seek error */ }
                } else {
                    if (fseek(p->file, sec_size, SEEK_CUR) != 0) { /* seek error */ }
                }
            }
        }
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_imd_hardened = {
    .name = "IMD", .description = "ImageDisk (HARDENED)", .extensions = "imd",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = imd_probe, .open = imd_open, .close = imd_close, .read_track = imd_read_track,
};

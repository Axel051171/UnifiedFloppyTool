/**
 * @file uft_td0.c
 * @brief Teledisk (TD0) Format Plugin - API-konform
 */

#include <stdio.h>  // FIXED R18
#include "uft/uft_format_common.h"

#define TD0_MAGIC_NORMAL    0x5444
#define TD0_MAGIC_ADVANCED  0x6474
#define TD0_HEADER_SIZE     12

typedef struct {
    FILE*       file;
    uint8_t     version;
    uint8_t     data_rate;
    uint8_t     sides;
    bool        compressed;
    long        data_start;
} td0_data_t;

static const uint16_t td0_sector_sizes[8] = { 128, 256, 512, 1024, 2048, 4096, 8192, 16384 };

static bool td0_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 2) return false;
    uint16_t magic = uft_read_le16(data);
    if (magic == TD0_MAGIC_NORMAL || magic == TD0_MAGIC_ADVANCED) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t td0_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[TD0_HEADER_SIZE];
    if (fread(header, 1, TD0_HEADER_SIZE, f) != TD0_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    uint16_t magic = uft_read_le16(header);
    if (magic != TD0_MAGIC_NORMAL && magic != TD0_MAGIC_ADVANCED) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    td0_data_t* pdata = calloc(1, sizeof(td0_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->version = header[4];
    pdata->data_rate = header[5];
    pdata->sides = header[9];
    pdata->compressed = (magic == TD0_MAGIC_ADVANCED);
    
    // Skip comment if present
    if (pdata->version >= 0x10) {
        uint8_t com_hdr[10];
        if (fread(com_hdr, 1, 10, f) != 10) { /* I/O error */ }
        uint16_t com_len = uft_read_le16(com_hdr + 2);
        if (fseek(f, com_len, SEEK_CUR) != 0) { /* seek error */ }
    }
    pdata->data_start = ftell(f);
    
    // Scan for geometry
    uint8_t max_cyl = 0, max_sec = 0;
    while (!feof(f)) {
        uint8_t trk_hdr[4];
        if (fread(trk_hdr, 1, 4, f) != 4) break;
        if (trk_hdr[0] == 0xFF) break;
        
        uint8_t num_sec = trk_hdr[0], cyl = trk_hdr[1];
        if (cyl > max_cyl) max_cyl = cyl;
        if (num_sec > max_sec) max_sec = num_sec;
        
        for (int s = 0; s < num_sec; s++) {
            uint8_t sec_hdr[6];
            if (fread(sec_hdr, 1, 6, f) != 6) { /* I/O error */ }
            if (!(sec_hdr[4] & 0x30)) {
                uint8_t len_buf[2];
                if (fread(len_buf, 1, 2, f) != 2) { /* I/O error */ }
                uint16_t len = uft_read_le16(len_buf);
                if (fseek(f, len - 1, SEEK_CUR) != 0) { /* seek error */ }
            }
        }
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = max_cyl + 1;
    disk->geometry.heads = pdata->sides;
    disk->geometry.sectors = max_sec;
    disk->geometry.sector_size = 512;
    
    return UFT_OK;
}

static void td0_close(uft_disk_t* disk) {
    td0_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

const uft_format_plugin_t uft_format_plugin_td0 = {
    .name = "TD0",
    .description = "Teledisk Archive",
    .extensions = "td0",
    .version = 0x00010000,
    .format = UFT_FORMAT_TD0,
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe = td0_probe,
    .open = td0_open,
    .close = td0_close,
};

UFT_REGISTER_FORMAT_PLUGIN(td0)

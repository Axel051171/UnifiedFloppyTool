/**
 * @file uft_ipf.c
 * @brief CAPS/SPS IPF Format Plugin - API-konform
 */

#include <stdio.h>  // FIXED R18
#include "uft/uft_format_common.h"

#define IPF_CAPS    0x43415053
#define IPF_INFO    0x494E464F
#define IPF_IMGE    0x494D4745
#define IPF_DATA    0x44415441
#define IPF_HEADER_SIZE  12

typedef struct {
    FILE*       file;
    uint32_t    encoder_type;
    uint32_t    min_track, max_track;
    uint32_t    min_side, max_side;
    uint32_t*   track_offsets;
    uint32_t*   track_sizes;
} ipf_data_t;

static bool ipf_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < IPF_HEADER_SIZE) return false;
    uint32_t type = uft_read_be32(data);
    if (type == IPF_CAPS) { *confidence = 98; return true; }
    return false;
}

static uft_error_t ipf_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[IPF_HEADER_SIZE];
    if (fread(header, 1, IPF_HEADER_SIZE, f) != IPF_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    if (uft_read_be32(header) != IPF_CAPS) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    ipf_data_t* pdata = calloc(1, sizeof(ipf_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->min_track = 0;
    pdata->max_track = 79;
    pdata->min_side = 0;
    pdata->max_side = 1;
    
    // Parse records to find INFO
    while (!feof(f)) {
        uint8_t rec[12];
        if (fread(rec, 1, 12, f) != 12) break;
        
        uint32_t type = uft_read_be32(rec);
        uint32_t len = uft_read_be32(rec + 4);
        
        if (type == IPF_INFO && len >= 40) {
            uint8_t info[64];
            size_t to_read = len < 64 ? len : 64;
            if (fread(info, 1, to_read, f) != to_read) { /* I/O error */ }
            pdata->min_track = uft_read_be32(info + 24);
            pdata->max_track = uft_read_be32(info + 28);
            pdata->min_side = uft_read_be32(info + 32);
            pdata->max_side = uft_read_be32(info + 36);
            if (len > 64) (void)fseek(f, len - 64, SEEK_CUR);
        } else {
            if (fseek(f, len, SEEK_CUR) != 0) { /* seek error */ }
        }
        
        if (len > 10000000) break;
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = pdata->max_track - pdata->min_track + 1;
    disk->geometry.heads = pdata->max_side - pdata->min_side + 1;
    disk->geometry.sectors = 11;
    disk->geometry.sector_size = 512;
    
    return UFT_OK;
}

static void ipf_close(uft_disk_t* disk) {
    ipf_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata->track_offsets);
        free(pdata->track_sizes);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

const uft_format_plugin_t uft_format_plugin_ipf = {
    .name = "IPF",
    .description = "Interchangeable Preservation Format (CAPS/SPS)",
    .extensions = "ipf",
    .version = 0x00010000,
    .format = UFT_FORMAT_IPF,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX,
    .probe = ipf_probe,
    .open = ipf_open,
    .close = ipf_close,
};

UFT_REGISTER_FORMAT_PLUGIN(ipf)

/**
 * @file uft_d81.c
 * @brief Commodore 1581 (D81) 3.5" Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"

#define D81_CYLINDERS           80
#define D81_SECTORS_PER_TRACK   40
#define D81_SECTOR_SIZE         256
#define D81_TOTAL_SECTORS       3200
#define D81_SIZE_STANDARD       819200
#define D81_SIZE_WITH_ERRORS    822400

typedef struct {
    FILE*       file;
    bool        has_errors;
    uint8_t*    error_table;
} d81_data_t;

bool d81_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size != D81_SIZE_STANDARD && file_size != D81_SIZE_WITH_ERRORS)
        return false;
    *confidence = 80;

    /* D81 header at track 40 sector 0 (offset 0x61800):
     * Byte 0-1 = directory link (40/3 typical)
     * Byte 2 = DOS version 'D' (0x44) for 1581 */
    if (size >= 0x61803) {
        if (data[0x61800] == 40 && data[0x61801] == 3) *confidence = 92;
        else if (data[0x61802] == 0x44) *confidence = 88;
    }
    return true;
}

static uft_error_t d81_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_FILE_OPEN; }
    size_t file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_FILE_OPEN; }
    bool has_errors = (file_size == D81_SIZE_WITH_ERRORS);
    if (file_size != D81_SIZE_STANDARD && file_size != D81_SIZE_WITH_ERRORS) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    d81_data_t* pdata = calloc(1, sizeof(d81_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->has_errors = has_errors;
    
    if (has_errors) {
        pdata->error_table = malloc(D81_TOTAL_SECTORS);
        if (pdata->error_table) {
            if (fseek(f, D81_SIZE_STANDARD, SEEK_SET) != 0) { free(pdata->error_table); pdata->error_table = NULL; pdata->has_errors = false; }
            else if (fread(pdata->error_table, 1, D81_TOTAL_SECTORS, f) != D81_TOTAL_SECTORS) { free(pdata->error_table); pdata->error_table = NULL; pdata->has_errors = false; }
        }
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = D81_CYLINDERS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = D81_SECTORS_PER_TRACK;
    disk->geometry.sector_size = D81_SECTOR_SIZE;
    disk->geometry.total_sectors = D81_TOTAL_SECTORS;
    
    return UFT_OK;
}

static void d81_close(uft_disk_t* disk) {
    d81_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata->error_table);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d81_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    d81_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file || head != 0) return UFT_ERROR_INVALID_STATE;
    
    uft_track_init(track, cyl, head);
    
    size_t track_offset = (size_t)cyl * D81_SECTORS_PER_TRACK * D81_SECTOR_SIZE;
    uint8_t sector_buf[D81_SECTOR_SIZE];
    
    for (int sec = 0; sec < D81_SECTORS_PER_TRACK; sec++) {
        if (fseek(pdata->file, track_offset + sec * D81_SECTOR_SIZE, SEEK_SET) != 0) continue;
        if (fread(sector_buf, 1, D81_SECTOR_SIZE, pdata->file) == D81_SECTOR_SIZE) {
            uft_format_add_sector(track, sec, sector_buf, D81_SECTOR_SIZE, cyl, head);
        }
    }
    
    return UFT_OK;
}

static uft_error_t d81_write_track(uft_disk_t* disk, int cyl, int head,
                                    const uft_track_t* track) {
    d81_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    long track_offset = (long)cyl * D81_SECTORS_PER_TRACK * D81_SECTOR_SIZE;
    for (size_t s = 0; s < track->sector_count && (int)s < D81_SECTORS_PER_TRACK; s++) {
        if (fseek(pdata->file, track_offset + (long)s * D81_SECTOR_SIZE, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[D81_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0, D81_SECTOR_SIZE); data = pad;
        }
        if (fwrite(data, 1, D81_SECTOR_SIZE, pdata->file) != D81_SECTOR_SIZE)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d81 = {
    .name = "D81",
    .description = "Commodore 1581 3.5\" Disk Image",
    .extensions = "d81",
    .version = 0x00010000,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d81_probe,
    .open = d81_open,
    .close = d81_close,
    .read_track = d81_read_track,
    .write_track = d81_write_track,
};

UFT_REGISTER_FORMAT_PLUGIN(d81)

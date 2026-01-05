/**
 * @file uft_d71_hardened.c
 * @brief Commodore 1571 D71 Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define D71_SECTOR_SIZE     256
#define D71_TRACKS_PER_SIDE 35
#define D71_TOTAL_TRACKS    70
#define D71_SECTORS_SIDE0   683
#define D71_TOTAL_SECTORS   1366
#define D71_SIZE_STD        349696
#define D71_SIZE_ERR        351062

static const uint8_t d71_spt[35] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,18,18,18,18,18,18,17,17,17,17,17
};

typedef struct {
    FILE*       file;
    bool        has_errors;
    uint8_t*    error_table;
    size_t      file_size;
} d71_data_t;

static size_t d71_get_offset(int track, int sector) {
    if (track < 1 || track > D71_TOTAL_TRACKS) return 0;
    
    int side = (track > D71_TRACKS_PER_SIDE) ? 1 : 0;
    int side_track = side ? (track - D71_TRACKS_PER_SIDE) : track;
    
    if (side_track < 1 || side_track > D71_TRACKS_PER_SIDE) return 0;
    if (sector < 0 || sector >= d71_spt[side_track - 1]) return 0;
    
    size_t offset = side ? (D71_SECTORS_SIDE0 * D71_SECTOR_SIZE) : 0;
    for (int t = 1; t < side_track; t++) {
        offset += (size_t)d71_spt[t - 1] * D71_SECTOR_SIZE;
    }
    offset += (size_t)sector * D71_SECTOR_SIZE;
    return offset;
}

static bool d71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == D71_SIZE_STD || file_size == D71_SIZE_ERR) {
        *confidence = 80;
        return true;
    }
    return false;
}

static uft_error_t d71_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    long pos = ftell(f);
    if (pos < 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    size_t file_size = (size_t)pos;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    
    bool has_errors = (file_size == D71_SIZE_ERR);
    if (file_size != D71_SIZE_STD && file_size != D71_SIZE_ERR) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    d71_data_t* pdata = calloc(1, sizeof(d71_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->has_errors = has_errors;
    pdata->file_size = file_size;
    
    if (has_errors) {
        pdata->error_table = malloc(D71_TOTAL_SECTORS);
        if (!pdata->error_table) {
            free(pdata); fclose(f);
            return UFT_ERROR_NO_MEMORY;
        }
        if (fseek(f, D71_SIZE_STD, SEEK_SET) != 0 ||
            fread(pdata->error_table, 1, D71_TOTAL_SECTORS, f) != D71_TOTAL_SECTORS) {
            free(pdata->error_table); free(pdata); fclose(f);
            return UFT_ERROR_FILE_READ;
        }
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = D71_TRACKS_PER_SIDE;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 21;
    disk->geometry.sector_size = D71_SECTOR_SIZE;
    disk->geometry.total_sectors = D71_TOTAL_SECTORS;
    
    return UFT_OK;
}

static void d71_close(uft_disk_t* disk) {
    if (!disk) return;
    d71_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata->error_table);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d71_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    d71_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    
    if (cyl < 0 || cyl >= D71_TRACKS_PER_SIDE) return UFT_ERROR_INVALID_ARG;
    if (head < 0 || head > 1) return UFT_ERROR_INVALID_ARG;
    
    int actual_track = (head == 0) ? (cyl + 1) : (cyl + 1 + D71_TRACKS_PER_SIDE);
    int num_sectors = d71_spt[cyl];
    
    uft_track_init(track, cyl, head);
    
    uint8_t buf[D71_SECTOR_SIZE];
    for (int sec = 0; sec < num_sectors; sec++) {
        size_t offset = d71_get_offset(actual_track, sec);
        if (offset == 0) continue;
        
        if (fseek(pdata->file, (long)offset, SEEK_SET) != 0) continue;
        if (fread(buf, 1, D71_SECTOR_SIZE, pdata->file) != D71_SECTOR_SIZE) continue;
        
        uft_error_t err = uft_format_add_sector(track, (uint8_t)sec, buf, 
                                                 D71_SECTOR_SIZE, (uint8_t)cyl, (uint8_t)head);
        (void)err;  // Continue on error
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d71_hardened = {
    .name = "D71", .description = "Commodore 1571 (HARDENED)", .extensions = "d71",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d71_probe, .open = d71_open, .close = d71_close, .read_track = d71_read_track,
};

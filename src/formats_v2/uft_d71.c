/**
 * @file uft_d71.c
 * @brief Commodore 1571 (D71) Format Plugin - API-konform
 */

#include <stdio.h>  // FIXED R18
#include "uft/uft_format_common.h"

#define D71_TRACKS_PER_SIDE     35
#define D71_TOTAL_TRACKS        70
#define D71_SECTOR_SIZE         256
#define D71_SECTORS_SIDE0       683
#define D71_TOTAL_SECTORS       1366
#define D71_SIZE_STANDARD       349696
#define D71_SIZE_WITH_ERRORS    351062
#define D71_BAM_TRACK           18

static const uint8_t d71_sectors_per_track[35] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

typedef struct {
    FILE*       file;
    uint8_t     num_sides;
    bool        has_errors;
    uint8_t*    error_table;
    size_t      file_size;
} d71_data_t;

static size_t d71_get_offset(int track, int sector) {
    if (track < 1 || track > D71_TOTAL_TRACKS) return 0;
    int side = (track > D71_TRACKS_PER_SIDE) ? 1 : 0;
    int side_track = side ? (track - D71_TRACKS_PER_SIDE) : track;
    if (side_track < 1 || side_track > D71_TRACKS_PER_SIDE) return 0;
    if (sector < 0 || sector >= d71_sectors_per_track[side_track - 1]) return 0;
    
    size_t offset = 0;
    if (side == 1) offset = D71_SECTORS_SIDE0 * D71_SECTOR_SIZE;
    for (int t = 1; t < side_track; t++)
        offset += d71_sectors_per_track[t - 1] * D71_SECTOR_SIZE;
    offset += sector * D71_SECTOR_SIZE;
    return offset;
}

static bool d71_detect_variant(size_t file_size, bool* has_errors) {
    if (file_size == D71_SIZE_STANDARD) { *has_errors = false; return true; }
    if (file_size == D71_SIZE_WITH_ERRORS) { *has_errors = true; return true; }
    return false;
}

static bool d71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    bool has_errors;
    if (!d71_detect_variant(file_size, &has_errors)) return false;
    
    if (size >= D71_SECTOR_SIZE * 358) {
        size_t bam_offset = d71_get_offset(D71_BAM_TRACK, 0);
        if (bam_offset + 4 <= size) {
            const uint8_t* bam = data + bam_offset;
            if (bam[0] == 18 && bam[1] == 1) { *confidence = 90; return true; }
        }
    }
    *confidence = 70;
    return true;
}

static uft_error_t d71_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    bool has_errors;
    if (!d71_detect_variant(file_size, &has_errors)) { fclose(f); return UFT_ERROR_FORMAT_INVALID; }
    
    d71_data_t* pdata = calloc(1, sizeof(d71_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    pdata->num_sides = 2;
    pdata->has_errors = has_errors;
    pdata->file_size = file_size;
    
    if (has_errors) {
        pdata->error_table = malloc(D71_TOTAL_SECTORS);
        if (pdata->error_table) {
            if (fseek(f, D71_SIZE_STANDARD, SEEK_SET) != 0) { /* seek error */ }
            if (fread(pdata->error_table, 1, D71_TOTAL_SECTORS, f) != D71_TOTAL_SECTORS) { /* I/O error */ }
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
    d71_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata->error_table);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d71_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    d71_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    
    int actual_track = (head == 0) ? (cyl + 1) : (cyl + 1 + D71_TRACKS_PER_SIDE);
    if (actual_track < 1 || actual_track > D71_TOTAL_TRACKS) return UFT_ERROR_INVALID_ARG;
    
    int side_track = (head == 0) ? (cyl + 1) : (cyl + 1);
    int num_sectors = d71_sectors_per_track[side_track - 1];
    
    uft_track_init(track, cyl, head);
    
    uint8_t sector_buf[D71_SECTOR_SIZE];
    for (int sec = 0; sec < num_sectors; sec++) {
        size_t offset = d71_get_offset(actual_track, sec);
        if (fseek(pdata->file, offset, SEEK_SET) != 0) { /* seek error */ }
        if (fread(sector_buf, 1, D71_SECTOR_SIZE, pdata->file) == D71_SECTOR_SIZE) {
            uft_format_add_sector(track, sec, sector_buf, D71_SECTOR_SIZE, cyl, head);
        }
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d71 = {
    .name = "D71",
    .description = "Commodore 1571 Disk Image",
    .extensions = "d71",
    .version = 0x00010000,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d71_probe,
    .open = d71_open,
    .close = d71_close,
    .read_track = d71_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(d71)

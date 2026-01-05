/**
 * @file uft_d81_hardened.c
 * @brief Commodore 1581 D81 Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define D81_CYLINDERS   80
#define D81_SPT         40
#define D81_SECTOR_SIZE 256
#define D81_TOTAL_SEC   3200
#define D81_SIZE_STD    819200
#define D81_SIZE_ERR    822400

typedef struct {
    FILE*       file;
    bool        has_errors;
    uint8_t*    error_table;
} d81_data_t;

static bool d81_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size == D81_SIZE_STD || file_size == D81_SIZE_ERR) {
        *confidence = 85; return true;
    }
    return false;
}

static uft_error_t d81_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    size_t file_size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    
    bool has_errors = (file_size == D81_SIZE_ERR);
    if (file_size != D81_SIZE_STD && file_size != D81_SIZE_ERR) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    
    d81_data_t* p = calloc(1, sizeof(d81_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    p->has_errors = has_errors;
    
    if (has_errors) {
        p->error_table = malloc(D81_TOTAL_SEC);
        if (!p->error_table) { free(p); fclose(f); return UFT_ERROR_NO_MEMORY; }
        if (fseek(f, D81_SIZE_STD, SEEK_SET) != 0 ||
            fread(p->error_table, 1, D81_TOTAL_SEC, f) != D81_TOTAL_SEC) {
            free(p->error_table); free(p); fclose(f);
            return UFT_ERROR_FILE_READ;
        }
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = D81_CYLINDERS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = D81_SPT;
    disk->geometry.sector_size = D81_SECTOR_SIZE;
    disk->geometry.total_sectors = D81_TOTAL_SEC;
    
    return UFT_OK;
}

static void d81_close(uft_disk_t* disk) {
    if (!disk) return;
    d81_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p->error_table);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t d81_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    d81_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl < 0 || cyl >= D81_CYLINDERS) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    size_t track_offset = (size_t)cyl * D81_SPT * D81_SECTOR_SIZE;
    uint8_t buf[D81_SECTOR_SIZE];
    
    for (int sec = 0; sec < D81_SPT; sec++) {
        if (fseek(p->file, (long)(track_offset + sec * D81_SECTOR_SIZE), SEEK_SET) != 0) continue;
        if (fread(buf, 1, D81_SECTOR_SIZE, p->file) != D81_SECTOR_SIZE) continue;
        uft_format_add_sector(track, (uint8_t)sec, buf, D81_SECTOR_SIZE, (uint8_t)cyl, 0);
    }
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_d81_hardened = {
    .name = "D81", .description = "Commodore 1581 (HARDENED)", .extensions = "d81",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = d81_probe, .open = d81_open, .close = d81_close, .read_track = d81_read_track,
};

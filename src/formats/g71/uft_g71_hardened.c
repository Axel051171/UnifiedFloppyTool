/**
 * @file uft_g71_hardened.c
 * @brief Commodore 1571 GCR (G71) Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define G71_MAX_TRACKS 168
#define G71_HEADER_SIZE 12

typedef struct {
    FILE*       file;
    uint8_t     num_tracks;
    uint32_t*   offsets;
    size_t      offsets_count;
} g71_data_t;

static bool g71_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size >= 8 && memcmp(data, "GCR-1571", 8) == 0) {
        *confidence = 95;
        return true;
    }
    if (size >= 8 && memcmp(data, "GCR-1541", 8) == 0 && data[9] > 84) {
        *confidence = 70;
        return true;
    }
    return false;
}

static uft_error_t g71_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[G71_HEADER_SIZE];
    if (fread(hdr, 1, G71_HEADER_SIZE, f) != G71_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    uint8_t num_tracks = hdr[9];
    if (num_tracks == 0 || num_tracks > G71_MAX_TRACKS) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Allocate plugin data
    g71_data_t* p = calloc(1, sizeof(g71_data_t));
    if (!p) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    // Allocate offsets array
    p->offsets = calloc(num_tracks, sizeof(uint32_t));
    if (!p->offsets) {
        free(p);
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    p->file = f;
    p->num_tracks = num_tracks;
    p->offsets_count = num_tracks;
    
    // Read track offsets
    for (int i = 0; i < num_tracks; i++) {
        uint8_t buf[4];
        if (fread(buf, 1, 4, f) != 4) {
            free(p->offsets);
            free(p);
            fclose(f);
            return UFT_ERROR_FILE_READ;
        }
        p->offsets[i] = uft_read_le32(buf);
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = num_tracks / 2;
    disk->geometry.heads = 2;
    disk->geometry.sectors = 21;
    disk->geometry.sector_size = 256;
    
    return UFT_OK;
}

static void g71_close(uft_disk_t* disk) {
    if (!disk) return;
    
    g71_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) {
            fclose(p->file);
            p->file = NULL;
        }
        free(p->offsets);
        p->offsets = NULL;
        free(p);
        disk->plugin_data = NULL;
    }
}

const uft_format_plugin_t uft_format_plugin_g71_hardened = {
    .name = "G71",
    .description = "Commodore 1571 GCR (HARDENED)",
    .extensions = "g71",
    .version = 0x00010001,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX,
    .probe = g71_probe,
    .open = g71_open,
    .close = g71_close,
};

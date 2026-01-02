#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define WOZ1_MAGIC 0x315A4F57
#define WOZ2_MAGIC 0x325A4F57
#define WOZ_HEADER_SIZE 12

typedef struct {
    FILE* file;
    uint8_t version;
    uint8_t tmap[160];
    uint32_t trks_offset;
} woz_data_t;

static bool woz_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 8) return false;
    uint32_t magic = uft_read_le32(data);
    if (magic == WOZ1_MAGIC || magic == WOZ2_MAGIC) {
        if (data[4] == 0xFF && data[5] == 0x0A && data[6] == 0x0D && data[7] == 0x0A) {
            *confidence = 98; return true;
        }
    }
    return false;
}

static uft_error_t woz_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[WOZ_HEADER_SIZE];
    if (fread(header, 1, WOZ_HEADER_SIZE, f) != WOZ_HEADER_SIZE) {
        fclose(f); return UFT_ERROR_FILE_READ;
    }
    
    uint32_t magic = uft_read_le32(header);
    if (magic != WOZ1_MAGIC && magic != WOZ2_MAGIC) {
        fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }
    
    woz_data_t* p = calloc(1, sizeof(woz_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    p->version = (magic == WOZ2_MAGIC) ? 2 : 1;
    memset(p->tmap, 0xFF, 160);
    
    // Find chunks
    while (!feof(f)) {
        uint8_t chunk[8];
        if (fread(chunk, 1, 8, f) != 8) break;
        
        uint32_t chunk_id = uft_read_le32(chunk);
        uint32_t chunk_size = uft_read_le32(&chunk[4]);
        
        if (chunk_id == 0x50414D54) {  // TMAP
            if (chunk_size >= 160) {
                fread(p->tmap, 1, 160, f);
            }
            fseek(f, chunk_size - 160, SEEK_CUR);
        } else if (chunk_id == 0x534B5254) {  // TRKS
            p->trks_offset = (uint32_t)ftell(f);
            fseek(f, chunk_size, SEEK_CUR);
        } else {
            fseek(f, chunk_size, SEEK_CUR);
        }
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = 35;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 16;
    disk->geometry.sector_size = 256;
    
    return UFT_OK;
}

static void woz_close(uft_disk_t* disk) {
    if (!disk) return;
    woz_data_t* p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

static uft_error_t woz_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    woz_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl < 0 || cyl >= 80) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    // WOZ stores raw flux/bits - for now just mark track present
    track->raw_size = 0;
    
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_woz_hardened = {
    .name = "WOZ", .description = "Applesauce WOZ (HARDENED)", .extensions = "woz",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX,
    .probe = woz_probe, .open = woz_open, .close = woz_close, .read_track = woz_read_track,
};

#include "uft/uft_format_common.h"

#define WOZ1_MAGIC 0x315A4F57
#define WOZ2_MAGIC 0x325A4F57
#define WOZ_TAIL   0x0A0D0AFF

typedef struct {
    FILE* file;
    uint8_t version;
    uint8_t disk_type;
    uint8_t tmap[160];
    uint32_t* track_bits;
    uint8_t** track_data;
    int track_count;
} woz_data_t;

static bool woz_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < 12) return false;
    uint32_t magic = uft_read_le32(data);
    uint32_t tail = uft_read_le32(data + 4);
    if ((magic == WOZ1_MAGIC || magic == WOZ2_MAGIC) && tail == WOZ_TAIL) {
        *confidence = 98; return true;
    }
    return false;
}

static uft_error_t woz_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t hdr[12];
    if (fread(hdr, 1, 12, f) != 12) { /* I/O error */ }
    uint32_t magic = uft_read_le32(hdr);
    bool v2 = (magic == WOZ2_MAGIC);
    
    woz_data_t* p = calloc(1, sizeof(woz_data_t));
    p->file = f;
    p->version = v2 ? 2 : 1;
    memset(p->tmap, 0xFF, 160);
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t fsize = ftell(f);
    if (fseek(f, 12, SEEK_SET) != 0) { /* seek error */ }
    // Parse chunks
    while (ftell(f) < (long)fsize - 8) {
        uint8_t chunk[8];
        if (fread(chunk, 1, 8, f) != 8) break;
        uint32_t id = uft_read_le32(chunk);
        uint32_t len = uft_read_le32(chunk + 4);
        long pos = ftell(f);
        
        if (id == 0x4F464E49) { // INFO
            uint8_t info[60];
            if (fread(info, 1, len < 60 ? len : 60, f) != len < 60 ? len : 60) { /* I/O error */ }
            p->disk_type = info[1];
        } else if (id == 0x50414D54) { // TMAP
            if (fread(p->tmap, 1, len < 160 ? len : 160, f) != len < 160 ? len : 160) { /* I/O error */ }
        }
        if (fseek(f, pos + len, SEEK_SET) != 0) { /* seek error */ }
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = (p->disk_type == 2) ? 80 : 35;
    disk->geometry.heads = (p->disk_type == 2) ? 2 : 1;
    disk->geometry.sectors = 16;
    disk->geometry.sector_size = (p->disk_type == 2) ? 512 : 256;
    return UFT_OK;
}

static void woz_close(uft_disk_t* disk) {
    woz_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        if (p->track_data) {
            for (int i = 0; i < p->track_count; i++) free(p->track_data[i]);
            free(p->track_data);
        }
        free(p->track_bits);
        free(p);
        disk->plugin_data = NULL;
    }
}

const uft_format_plugin_t uft_format_plugin_woz = {
    .name = "WOZ", .description = "Applesauce Apple II", .extensions = "woz",
    .format = UFT_FORMAT_DSK, .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_FLUX,
    .probe = woz_probe, .open = woz_open, .close = woz_close,
};
UFT_REGISTER_FORMAT_PLUGIN(woz)

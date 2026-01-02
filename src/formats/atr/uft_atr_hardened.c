/**
 * @file uft_atr_hardened.c
 * @brief Atari 8-bit ATR Format Plugin - HARDENED VERSION
 */

#include "uft/core/uft_safe_math.h"
#include "uft/uft_format_common.h"
#include "uft/uft_safe.h"

#define ATR_MAGIC           0x0296
#define ATR_HEADER_SIZE     16
#define ATR_BOOT_SECTORS    3
#define ATR_BOOT_SEC_SIZE   128
#define ATR_MAX_SECTORS     65535

typedef struct {
    FILE*       file;
    uint16_t    sector_size;
    uint32_t    total_sectors;
    size_t      file_size;
} atr_data_t;

static size_t atr_sector_offset(uint32_t sector, uint16_t sector_size) {
    if (sector < 1) return 0;
    if (sector <= ATR_BOOT_SECTORS) {
        return ATR_HEADER_SIZE + (sector - 1) * ATR_BOOT_SEC_SIZE;
    }
    return ATR_HEADER_SIZE + ATR_BOOT_SECTORS * ATR_BOOT_SEC_SIZE +
           (sector - ATR_BOOT_SECTORS - 1) * sector_size;
}

static bool atr_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < ATR_HEADER_SIZE) return false;
    if (uft_read_le16(data) == ATR_MAGIC) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t atr_open(uft_disk_t* disk, const char* path, bool read_only) {
    UFT_REQUIRE_NOT_NULL2(disk, path);
    
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[ATR_HEADER_SIZE];
    if (fread(header, 1, ATR_HEADER_SIZE, f) != ATR_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FILE_READ;
    }
    
    if (uft_read_le16(header) != ATR_MAGIC) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Get file size
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    size_t file_size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_FILE_SEEK; }
    
    atr_data_t* p = calloc(1, sizeof(atr_data_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    p->file = f;
    p->file_size = file_size;
    
    uint32_t paragraphs = uft_read_le16(&header[2]) | ((uint32_t)header[6] << 16);
    uint32_t disk_bytes = paragraphs * 16;
    
    p->sector_size = uft_read_le16(&header[4]);
    if (p->sector_size != 128 && p->sector_size != 256) {
        p->sector_size = 128;  // Default
    }
    
    // Calculate total sectors
    if (disk_bytes <= ATR_BOOT_SECTORS * ATR_BOOT_SEC_SIZE) {
        p->total_sectors = disk_bytes / ATR_BOOT_SEC_SIZE;
    } else {
        p->total_sectors = ATR_BOOT_SECTORS + 
            (disk_bytes - ATR_BOOT_SECTORS * ATR_BOOT_SEC_SIZE) / p->sector_size;
    }
    
    if (p->total_sectors > ATR_MAX_SECTORS) {
        p->total_sectors = ATR_MAX_SECTORS;
    }
    
    disk->plugin_data = p;
    disk->geometry.cylinders = (p->total_sectors + 17) / 18;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = p->sector_size;
    disk->geometry.total_sectors = p->total_sectors;
    
    return UFT_OK;
}

static void atr_close(uft_disk_t* disk) {
    if (!disk) return;
    atr_data_t* p = disk->plugin_data;
    if (p) {
        if (p->file) fclose(p->file);
        free(p);
        disk->plugin_data = NULL;
    }
}

static uft_error_t atr_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    UFT_REQUIRE_NOT_NULL2(disk, track);
    
    atr_data_t* p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (head != 0 || cyl < 0) return UFT_ERROR_INVALID_ARG;
    
    uft_track_init(track, cyl, head);
    
    uint8_t* sec_buf = malloc(p->sector_size);
    if (!sec_buf) return UFT_ERROR_NO_MEMORY;
    
    for (int s = 0; s < 18; s++) {
        uint32_t sector_num = (uint32_t)cyl * 18 + s + 1;
        if (sector_num > p->total_sectors) break;
        
        uint16_t this_size = (sector_num <= ATR_BOOT_SECTORS) ? 
                            ATR_BOOT_SEC_SIZE : p->sector_size;
        
        size_t offset = atr_sector_offset(sector_num, p->sector_size);
        if (offset + this_size > p->file_size) break;
        
        memset(sec_buf, 0, p->sector_size);
        if (fseek(p->file, (long)offset, SEEK_SET) != 0) continue;
        if (fread(sec_buf, 1, this_size, p->file) != this_size) continue;
        
        uft_format_add_sector(track, (uint8_t)s, sec_buf, this_size, (uint8_t)cyl, 0);
    }
    
    free(sec_buf);
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_atr_hardened = {
    .name = "ATR", .description = "Atari 8-bit (HARDENED)", .extensions = "atr;xfd",
    .version = 0x00010001, .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = atr_probe, .open = atr_open, .close = atr_close, .read_track = atr_read_track,
};

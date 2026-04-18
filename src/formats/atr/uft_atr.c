/**
 * @file uft_atr.c
 * @brief Atari 8-bit ATR Format Plugin - API-konform
 */

#include "uft/uft_format_common.h"

#define ATR_MAGIC           0x0296
#define ATR_HEADER_SIZE     16
#define ATR_BOOT_SECTORS    3
#define ATR_BOOT_SECTOR_SIZE 128

typedef struct {
    FILE*       file;
    uint16_t    sector_size;
    uint32_t    total_sectors;
} atr_data_t;

static size_t atr_sector_offset(int sector, uint16_t sector_size) {
    if (sector < 1) return 0;
    if (sector <= ATR_BOOT_SECTORS)
        return ATR_HEADER_SIZE + (sector - 1) * ATR_BOOT_SECTOR_SIZE;
    return ATR_HEADER_SIZE + ATR_BOOT_SECTORS * ATR_BOOT_SECTOR_SIZE +
           (sector - ATR_BOOT_SECTORS - 1) * sector_size;
}

bool atr_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < ATR_HEADER_SIZE) return false;
    if (uft_read_le16(data) == ATR_MAGIC) {
        *confidence = 95;
        return true;
    }
    return false;
}

static uft_error_t atr_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    uint8_t header[ATR_HEADER_SIZE];
    if (fread(header, 1, ATR_HEADER_SIZE, f) != ATR_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    if (uft_read_le16(header) != ATR_MAGIC) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    atr_data_t* pdata = calloc(1, sizeof(atr_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    
    pdata->file = f;
    uint32_t paragraphs = uft_read_le16(&header[2]) | ((uint32_t)header[6] << 16);
    uint32_t disk_bytes = paragraphs * 16;
    pdata->sector_size = uft_read_le16(&header[4]);
    if (pdata->sector_size != 128 && pdata->sector_size != 256) pdata->sector_size = 128;
    
    if (disk_bytes <= ATR_BOOT_SECTORS * ATR_BOOT_SECTOR_SIZE) {
        pdata->total_sectors = disk_bytes / ATR_BOOT_SECTOR_SIZE;
    } else {
        pdata->total_sectors = ATR_BOOT_SECTORS + 
            (disk_bytes - ATR_BOOT_SECTORS * ATR_BOOT_SECTOR_SIZE) / pdata->sector_size;
    }
    
    disk->plugin_data = pdata;
    disk->geometry.cylinders = (pdata->total_sectors + 17) / 18;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 18;
    disk->geometry.sector_size = pdata->sector_size;
    disk->geometry.total_sectors = pdata->total_sectors;
    
    return UFT_OK;
}

static void atr_close(uft_disk_t* disk) {
    atr_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

static uft_error_t atr_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    atr_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file || head != 0) return UFT_ERROR_INVALID_STATE;
    
    uft_track_init(track, cyl, head);
    
    uint8_t* sec_buf = malloc(pdata->sector_size);
    if (!sec_buf) return UFT_ERROR_NO_MEMORY;
    for (int s = 0; s < 18; s++) {
        uint32_t sector_num = cyl * 18 + s + 1;
        if (sector_num > pdata->total_sectors) break;
        
        uint16_t this_size = (sector_num <= ATR_BOOT_SECTORS) ? 
                            ATR_BOOT_SECTOR_SIZE : pdata->sector_size;
        
        memset(sec_buf, 0, pdata->sector_size);
        if (fseek(pdata->file, atr_sector_offset(sector_num, pdata->sector_size), SEEK_SET) != 0) { free(sec_buf); return UFT_ERROR_IO; }
        if (fread(sec_buf, 1, this_size, pdata->file) != this_size) {
            memset(sec_buf, 0xE5, this_size); /* forensic fill on read error */
        }
        uft_format_add_sector(track, s, sec_buf, this_size, cyl, head);
    }
    free(sec_buf);
    
    return UFT_OK;
}

static uft_error_t atr_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track) {
    atr_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file || head != 0) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    for (size_t s = 0; s < track->sector_count && s < 18; s++) {
        uint32_t sec_num = (uint32_t)(cyl * 18 + s + 1);
        if (sec_num > pdata->total_sectors) break;
        uint16_t this_size = (sec_num <= ATR_BOOT_SECTORS) ?
                             ATR_BOOT_SECTOR_SIZE : pdata->sector_size;
        if (fseek(pdata->file, atr_sector_offset(sec_num, pdata->sector_size),
                  SEEK_SET) != 0) return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        if (!data) continue;
        size_t len = track->sectors[s].data_len;
        if (len > this_size) len = this_size;
        if (fwrite(data, 1, len, pdata->file) != len) return UFT_ERROR_IO;
    }
    return UFT_OK;
}

const uft_format_plugin_t uft_format_plugin_atr = {
    .name = "ATR",
    .description = "Atari 8-bit Disk Image",
    .extensions = "atr;xfd",
    .version = 0x00010000,
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe = atr_probe,
    .open = atr_open,
    .close = atr_close,
    .read_track = atr_read_track,
    .write_track = atr_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_OFFICIAL_FULL,  /* SIO2PC/APE docs, Atari 810 hw reference fully describe ATR */
};

UFT_REGISTER_FORMAT_PLUGIN(atr)

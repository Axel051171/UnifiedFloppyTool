/**
 * @file lif.c
 * @brief HP LIF (Logical Interchange Format) implementation
 */

#include "uft/formats/lif.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int lif_probe(const uint8_t *data, size_t size) {
    if (size < sizeof(LifVolumeHeader)) return 0;
    
    const LifVolumeHeader *hdr = (const LifVolumeHeader *)data;
    
    // Check LIF ID (0x8000 in big-endian)
    if (hdr->lif_id == 0x0080) return 85;  // Little-endian check
    if (hdr->lif_id == 0x8000) return 85;  // Big-endian
    
    // Also check by file size
    size_t expected = LIF_TRACKS * LIF_SECTORS_TRACK * LIF_SECTOR_SIZE;
    if (size == expected) return 60;
    if (size == expected * 2) return 60;  // Double-sided
    
    return 0;
}

int lif_open(LifDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    fclose(f);
    
    dev->cylinders = LIF_TRACKS;
    dev->sectors = LIF_SECTORS_TRACK;
    dev->sectorSize = LIF_SECTOR_SIZE;
    
    size_t expected = LIF_TRACKS * LIF_SECTORS_TRACK * LIF_SECTOR_SIZE;
    dev->heads = (size >= expected * 2) ? 2 : 1;
    
    dev->internal_ctx = strdup(path);
    return 0;
}

int lif_close(LifDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int lif_read_sector(LifDevice *dev, uint32_t c, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (c >= dev->cylinders || h >= dev->heads || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    size_t offset = ((c * dev->heads + h) * dev->sectors + s) * dev->sectorSize;
    if (fseek(f, offset, SEEK_SET) != 0) { /* seek error */ }
    size_t read = fread(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    return (read == dev->sectorSize) ? 0 : -1;
}

int lif_list_files(LifDevice *dev) {
    if (!dev || !dev->internal_ctx) return -1;
    
    /* HP LIF directory starts at sector 2.
     * Volume header (sector 0) contains dir_start and dir_length.
     * Each directory entry is 32 bytes:
     *   Bytes 0-9: Filename
     *   Bytes 10-11: File type (big-endian)
     *   Bytes 12-15: Start sector (big-endian)
     *   Bytes 16-19: File length in sectors (big-endian)
     *   Bytes 20-25: Date/time
     *   0xFFFF at filename start = end of directory */
    uint8_t sector_buf[256];
    
    /* Read volume header to get directory location */
    if (lif_read_sector(dev, 0, 0, 0, sector_buf) != 0) return -1;
    
    uint32_t dir_start = ((uint32_t)sector_buf[8] << 24) | ((uint32_t)sector_buf[9] << 16) |
                         ((uint32_t)sector_buf[10] << 8) | sector_buf[11];
    uint32_t dir_len = ((uint32_t)sector_buf[16] << 24) | ((uint32_t)sector_buf[17] << 16) |
                       ((uint32_t)sector_buf[18] << 8) | sector_buf[19];
    
    if (dir_start == 0) dir_start = 2;  /* Default */
    if (dir_len == 0 || dir_len > 1000) dir_len = 8;  /* Reasonable default */
    
    int file_count = 0;
    int entries_per_sector = dev->sectorSize / 32;
    
    for (uint32_t ds = 0; ds < dir_len; ds++) {
        uint32_t abs_sector = dir_start + ds;
        uint32_t t = abs_sector / (dev->heads * dev->sectors);
        uint32_t rem = abs_sector % (dev->heads * dev->sectors);
        uint32_t h = rem / dev->sectors;
        uint32_t s = rem % dev->sectors;
        
        if (lif_read_sector(dev, t, h, s, sector_buf) != 0) break;
        
        for (int i = 0; i < entries_per_sector; i++) {
            const uint8_t *entry = sector_buf + i * 32;
            
            /* Check for end of directory */
            if (entry[0] == 0xFF && entry[1] == 0xFF) goto done;
            if (entry[0] == 0x00) continue;  /* Empty entry */
            
            /* Extract filename */
            char name[11];
            memcpy(name, entry, 10);
            name[10] = '\0';
            for (int j = 9; j >= 0 && name[j] == ' '; j--) name[j] = '\0';
            
            /* File type and size */
            uint16_t file_type = ((uint16_t)entry[10] << 8) | entry[11];
            uint32_t file_sectors = ((uint32_t)entry[16] << 24) | ((uint32_t)entry[17] << 16) |
                                    ((uint32_t)entry[18] << 8) | entry[19];
            
            printf("%-10s  Type:%04X  %6u sectors\n", name, file_type,
                   (unsigned)file_sectors);
            file_count++;
        }
    }
done:
    return file_count;
}

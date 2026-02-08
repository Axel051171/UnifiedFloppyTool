/**
 * @file cpm.c
 * @brief CP/M disk format implementation
 */

#include "uft/formats/cpm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

CpmFormat cpm_detect_format(const uint8_t *data, size_t size) {
    // Common CP/M disk sizes
    switch (size) {
        case 256256:   // 77 tracks × 26 sectors × 128 bytes
            return CPM_8_SSSD;
        case 184320:   // 40 tracks × 9 sectors × 512 bytes
            return CPM_525_SSDD;
        case 368640:   // 40 tracks × 2 sides × 9 sectors × 512 bytes
            return CPM_525_DSDD;
        case 737280:   // 80 tracks × 2 sides × 9 sectors × 512 bytes
            return CPM_35_DSDD;
        default:
            return CPM_UNKNOWN;
    }
}

int cpm_probe(const uint8_t *data, size_t size) {
    CpmFormat fmt = cpm_detect_format(data, size);
    if (fmt != CPM_UNKNOWN) return 60;  // Lower confidence, need more checks
    
    return 0;
}

int cpm_open(CpmDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    fclose(f);
    
    dev->format = cpm_detect_format(NULL, size);
    
    switch (dev->format) {
        case CPM_8_SSSD:
            dev->tracks = 77;
            dev->heads = 1;
            dev->sectors = 26;
            dev->sectorSize = 128;
            dev->blockSize = 1024;
            dev->dirEntries = 64;
            dev->reservedTracks = 2;
            break;
        case CPM_525_SSDD:
            dev->tracks = 40;
            dev->heads = 1;
            dev->sectors = 9;
            dev->sectorSize = 512;
            dev->blockSize = 1024;
            dev->dirEntries = 64;
            dev->reservedTracks = 1;
            break;
        case CPM_525_DSDD:
            dev->tracks = 40;
            dev->heads = 2;
            dev->sectors = 9;
            dev->sectorSize = 512;
            dev->blockSize = 2048;
            dev->dirEntries = 128;
            dev->reservedTracks = 1;
            break;
        case CPM_35_DSDD:
            dev->tracks = 80;
            dev->heads = 2;
            dev->sectors = 9;
            dev->sectorSize = 512;
            dev->blockSize = 2048;
            dev->dirEntries = 128;
            dev->reservedTracks = 1;
            break;
        default:
            return -1;
    }
    
    dev->internal_ctx = strdup(path);
    return 0;
}

int cpm_close(CpmDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int cpm_read_sector(CpmDevice *dev, uint32_t c, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (c >= dev->tracks || h >= dev->heads || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    size_t offset = ((c * dev->heads + h) * dev->sectors + s) * dev->sectorSize;
    if (fseek(f, offset, SEEK_SET) != 0) { /* seek error */ }
    size_t read = fread(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    return (read == dev->sectorSize) ? 0 : -1;
}

int cpm_list_files(CpmDevice *dev) {
    if (!dev || !dev->internal_ctx) return -1;
    
    // Read directory from reserved tracks
    // CP/M directory starts at the first track after reserved tracks.
    // Each entry is 32 bytes:
    //   Byte 0: User number (0-15, 0xE5 = deleted)
    //   Bytes 1-8: Filename (space-padded)
    //   Bytes 9-11: Extension (high bits may be flags)
    //   Byte 12: Extent counter low
    //   Bytes 13-14: Reserved
    //   Byte 15: Record count (records in this extent, max 128)
    //   Bytes 16-31: Allocation map (block numbers)
    
    uint32_t dir_sectors = (dev->dirEntries * 32 + dev->sectorSize - 1) / dev->sectorSize;
    uint8_t sector_buf[1024];
    int file_count = 0;
    
    uint32_t dir_track = dev->reservedTracks;
    uint32_t dir_sector = 0;
    
    for (uint32_t ds = 0; ds < dir_sectors; ds++) {
        uint32_t s = dir_sector + ds;
        uint32_t t = dir_track + s / dev->sectors;
        s = s % dev->sectors;
        
        if (cpm_read_sector(dev, t, 0, s, sector_buf) != 0) break;
        
        int entries_per_sector = dev->sectorSize / 32;
        for (int i = 0; i < entries_per_sector; i++) {
            const uint8_t *entry = sector_buf + i * 32;
            uint8_t user = entry[0];
            
            if (user == 0xE5) continue;  /* Deleted */
            if (user > 15) continue;     /* Invalid */
            if (entry[12] != 0) continue; /* Not first extent, skip duplicates */
            
            /* Extract filename */
            char name[9], ext[4];
            memcpy(name, entry + 1, 8);
            name[8] = '\0';
            /* Strip high bits (attribute flags) from extension */
            for (int j = 0; j < 3; j++) ext[j] = entry[9 + j] & 0x7F;
            ext[3] = '\0';
            
            /* Trim trailing spaces */
            for (int j = 7; j >= 0 && name[j] == ' '; j--) name[j] = '\0';
            for (int j = 2; j >= 0 && ext[j] == ' '; j--) ext[j] = '\0';
            
            /* Count total records across all extents for size */
            int records = entry[15];
            
            /* Print entry */
            printf("%d: %-8s.%-3s %5dK\n", user, name, ext,
                   (records * 128 + 1023) / 1024);
            file_count++;
        }
    }
    
    return file_count;
}

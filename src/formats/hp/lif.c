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
    // TODO: Implement directory listing
    return 0;
}

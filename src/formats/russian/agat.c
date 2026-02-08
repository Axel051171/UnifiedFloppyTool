/**
 * @file agat.c
 * @brief Agat (Soviet Apple II clone) disk format
 * 
 * Agat used a different disk format than Apple II:
 * 80 tracks, 21 sectors per track, 256 bytes per sector = 430,080 bytes
 */

#include "uft/formats/agat.h"
#include "uft/formats/uft_format_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int agat_probe(const uint8_t *data, size_t size) {
    // Agat: 80 tracks × 21 sectors × 256 bytes = 430,080
    if (size == 80 * 21 * 256) return 85;
    
    // Agat 840: 80 tracks × 21 sectors × 256 bytes × 2 sides = 860,160
    if (size == 80 * 21 * 256 * 2) return 85;
    
    return 0;
}

int agat_open(AgatDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    fclose(f);
    
    dev->tracks = 80;
    dev->sectors = 21;
    dev->sectorSize = 256;
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int agat_close(AgatDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int agat_read_sector(AgatDevice *dev, uint32_t t, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (t >= dev->tracks || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    size_t offset = (t * dev->sectors + s) * dev->sectorSize;
    if (fseek(f, offset, SEEK_SET) != 0) { /* seek error */ }
    size_t read = fread(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    return (read == dev->sectorSize) ? 0 : -1;
}

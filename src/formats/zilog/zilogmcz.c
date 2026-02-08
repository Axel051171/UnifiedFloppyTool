/**
 * @file zilogmcz.c
 * @brief Zilog MCZ development system disk format
 * 
 * Zilog MCZ 1/25 and 1/35 used 8" hard-sectored disks.
 * 77 tracks, 32 sectors (hard-sectored), 132 bytes/sector.
 */

#include "uft/formats/zilogmcz.h"
#include "uft/formats/uft_format_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define ZILOG_TRACKS    77
#define ZILOG_SECTORS   32
#define ZILOG_SECTOR_SIZE 132

int zilogmcz_probe(const uint8_t *data, size_t size) {
    // Zilog MCZ: 77 × 32 × 132 = 325,248 bytes
    if (size == ZILOG_TRACKS * ZILOG_SECTORS * ZILOG_SECTOR_SIZE) {
        return 75;
    }
    return 0;
}

int zilogmcz_open(ZilogMczDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    fclose(f);
    
    if (size != ZILOG_TRACKS * ZILOG_SECTORS * ZILOG_SECTOR_SIZE) {
        return -1;
    }
    
    dev->tracks = ZILOG_TRACKS;
    dev->sectors = ZILOG_SECTORS;
    dev->sectorSize = ZILOG_SECTOR_SIZE;
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int zilogmcz_close(ZilogMczDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int zilogmcz_read_sector(ZilogMczDevice *dev, uint32_t t, uint32_t s, uint8_t *buf) {
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

/**
 * @file northstar.c
 * @brief North Star disk format implementation
 * 
 * North Star Horizon/Advantage used hard-sectored 10-sector disks.
 * FM (single density) or MFM (double density).
 */

#include "uft/formats/northstar.h"
#include "uft/formats/uft_format_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int northstar_probe(const uint8_t *data, size_t size) {
    // North Star sizes:
    // 35 tracks × 10 sectors × 256 bytes = 89,600 (SD)
    // 35 tracks × 10 sectors × 512 bytes = 179,200 (DD)
    
    if (size == 35 * 10 * 256) return 75;
    if (size == 35 * 10 * 512) return 75;
    
    return 0;
}

int northstar_open(NorthStarDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fclose(f);
    
    dev->tracks = 35;
    dev->sectors = 10;  // Hard-sectored
    
    if (size == 35 * 10 * 256) {
        dev->sectorSize = 256;
        dev->double_density = false;
    } else if (size == 35 * 10 * 512) {
        dev->sectorSize = 512;
        dev->double_density = true;
    } else {
        return -1;
    }
    
    dev->internal_ctx = strdup(path);
    return 0;
}

int northstar_close(NorthStarDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int northstar_read_sector(NorthStarDevice *dev, uint32_t t, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (t >= dev->tracks || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    size_t offset = (t * dev->sectors + s) * dev->sectorSize;
    fseek(f, offset, SEEK_SET);
    size_t read = fread(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    return (read == dev->sectorSize) ? 0 : -1;
}

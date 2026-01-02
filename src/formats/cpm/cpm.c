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
    
    fseek(f, 0, SEEK_END);
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
    fseek(f, offset, SEEK_SET);
    size_t read = fread(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    return (read == dev->sectorSize) ? 0 : -1;
}

int cpm_list_files(CpmDevice *dev) {
    if (!dev || !dev->internal_ctx) return -1;
    
    // Read directory from reserved tracks
    // TODO: Full implementation
    return 0;
}

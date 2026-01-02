/**
 * @file tids990.c
 * @brief TI DS/990 minicomputer disk format
 */

#include "uft/formats/tids990.h"
#include "uft/formats/uft_format_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int tids990_probe(const uint8_t *data, size_t size) {
    // TI DS/990 used various geometries
    // Common: 77 tracks × 26 sectors × 128 bytes = 256,256 bytes
    if (size == 77 * 26 * 128) return 70;
    if (size == 77 * 26 * 256) return 70;
    return 0;
}

int tids990_open(TiDs990Device *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fclose(f);
    
    dev->tracks = 77;
    dev->heads = 1;
    dev->sectors = 26;
    
    if (size == 77 * 26 * 128) {
        dev->sectorSize = 128;
        dev->double_density = false;
    } else if (size == 77 * 26 * 256) {
        dev->sectorSize = 256;
        dev->double_density = true;
    } else {
        return -1;
    }
    
    dev->internal_ctx = strdup(path);
    return 0;
}

int tids990_close(TiDs990Device *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int tids990_read_sector(TiDs990Device *dev, uint32_t c, uint32_t h, uint32_t s, uint8_t *buf) {
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

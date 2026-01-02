/**
 * @file micropolis.c
 * @brief Micropolis disk format implementation
 * 
 * Micropolis drives used hard-sectored 16-sector disks.
 * Used by Vector Graphic, Exidy Sorcerer, and others.
 */

#include "uft/formats/micropolis.h"
#include "uft/formats/uft_format_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Micropolis sector sizes
#define MICROPOLIS_SECTOR_SIZE_STD  266
#define MICROPOLIS_SECTOR_SIZE_VG   275

// Micropolis checksum types
typedef enum {
    CHECKSUM_AUTO = 0,
    CHECKSUM_MICROPOLIS,
    CHECKSUM_MZOS,
} micropolis_checksum_t;

// Calculate Micropolis checksum
static uint8_t micropolis_checksum(const uint8_t *data, int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// Calculate MZOS checksum (XOR)
static uint8_t mzos_checksum(const uint8_t *data, int len) {
    uint8_t xor = 0;
    for (int i = 0; i < len; i++) {
        xor ^= data[i];
    }
    return xor;
}

int micropolis_probe(const uint8_t *data, size_t size) {
    // Common Micropolis sizes:
    // 35 tracks × 16 sectors × 266 bytes = 148,960
    // 77 tracks × 16 sectors × 266 bytes = 327,712
    // 35 tracks × 16 sectors × 275 bytes = 154,000
    // 77 tracks × 16 sectors × 275 bytes = 339,500
    
    if (size == 35 * 16 * 266) return 80;
    if (size == 77 * 16 * 266) return 80;
    if (size == 35 * 16 * 275) return 80;
    if (size == 77 * 16 * 275) return 80;
    
    return 0;
}

int micropolis_open(MicropolisDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fclose(f);
    
    // Determine format from size
    dev->sectors = 16;  // Always 16 (hard-sectored)
    
    if (size == 35 * 16 * 266 || size == 35 * 16 * 275) {
        dev->tracks = 35;
    } else if (size == 77 * 16 * 266 || size == 77 * 16 * 275) {
        dev->tracks = 77;
    } else {
        return -1;
    }
    
    if (size % 266 == 0) {
        dev->sectorSize = 266;
        dev->type = MICROPOLIS_METAFLOPPY;
    } else if (size % 275 == 0) {
        dev->sectorSize = 275;
        dev->type = MICROPOLIS_VECTOR_GRAPHIC;
    } else {
        return -1;
    }
    
    dev->double_density = false;  // TODO: detect DD
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int micropolis_close(MicropolisDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int micropolis_read_sector(MicropolisDevice *dev, uint32_t t, uint32_t s, uint8_t *buf) {
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

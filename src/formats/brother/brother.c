/**
 * @file brother.c
 * @brief Brother Word Processor disk format implementation
 */

#include "uft/formats/brother.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Brother GCR encoding table (5-to-8)
const uint8_t BROTHER_GCR_ENCODE[32] = {
    0x0a, 0x0b, 0x12, 0x13, 0x0e, 0x0f, 0x16, 0x17,
    0x09, 0x19, 0x1a, 0x1b, 0x0d, 0x1d, 0x1e, 0x15,
    0x4a, 0x4b, 0x52, 0x53, 0x4e, 0x4f, 0x56, 0x57,
    0x49, 0x59, 0x5a, 0x5b, 0x4d, 0x5d, 0x5e, 0x55
};

// Reverse decode table
static uint8_t g_brother_decode[256];
static bool g_brother_decode_init = false;

static void init_decode_table(void) {
    if (g_brother_decode_init) return;
    memset(g_brother_decode, 0xFF, 256);
    for (int i = 0; i < 32; i++) {
        g_brother_decode[BROTHER_GCR_ENCODE[i]] = i;
    }
    g_brother_decode_init = true;
}

int brother_probe(const uint8_t *data, size_t size) {
    // Brother disks are 78 or 120 tracks × 12 sectors × 256 bytes
    // = 239,616 bytes (78 tracks) or 368,640 bytes (120 tracks)
    if (size == 78 * 12 * 256) return 80;
    if (size == 120 * 12 * 256) return 80;
    return 0;
}

int brother_open(BrotherDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    init_decode_table();
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    fclose(f);
    
    if (size == 78 * 12 * 256) {
        dev->tracks = 78;
        dev->is_120_track = false;
    } else if (size == 120 * 12 * 256) {
        dev->tracks = 120;
        dev->is_120_track = true;
    } else {
        return -1;
    }
    
    dev->sectors = 12;
    dev->sectorSize = 256;
    
    // Store file handle
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int brother_close(BrotherDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int brother_read_sector(BrotherDevice *dev, uint32_t t, uint32_t s, uint8_t *buf) {
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

int brother_write_sector(BrotherDevice *dev, uint32_t t, uint32_t s, const uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (t >= dev->tracks || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "r+b");
    if (!f) return -1;
    
    size_t offset = (t * dev->sectors + s) * dev->sectorSize;
    if (fseek(f, offset, SEEK_SET) != 0) { /* seek error */ }
    size_t written = fwrite(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    return (written == dev->sectorSize) ? 0 : -1;
}

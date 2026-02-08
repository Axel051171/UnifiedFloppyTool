/**
 * @file qdos.c
 * @brief QDOS (Sinclair QL) implementation
 */

#include "uft/formats/qdos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint16_t read_be16(const uint8_t *p) {
    return (p[0] << 8) | p[1];
}

int qdos_probe(const uint8_t *data, size_t size) {
    if (size < sizeof(QdosHeader)) return 0;
    
    const QdosHeader *hdr = (const QdosHeader *)data;
    
    if (memcmp(hdr->signature, "QL5A", 4) == 0) return 90;
    if (memcmp(hdr->signature, "QL5B", 4) == 0) return 90;
    
    return 0;
}

int qdos_open(QdosDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    QdosHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    
    if (memcmp(hdr.signature, "QL5A", 4) != 0 &&
        memcmp(hdr.signature, "QL5B", 4) != 0) {
        return -1;
    }
    
    uint16_t spt = read_be16((uint8_t*)&hdr.sectors_per_track);
    uint16_t spc = read_be16((uint8_t*)&hdr.sectors_per_cyl);
    uint16_t cps = read_be16((uint8_t*)&hdr.cyls_per_side);
    
    dev->cylinders = cps;
    dev->heads = (spt > 0) ? spc / spt : 2;
    dev->sectors = spt;
    dev->is_hd = (hdr.signature[3] == 'B');
    
    memcpy(dev->label, hdr.label, 10);
    dev->label[10] = '\0';
    
    dev->internal_ctx = strdup(path);
    return 0;
}

int qdos_close(QdosDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int qdos_read_sector(QdosDevice *dev, uint32_t c, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (c >= dev->cylinders || h >= dev->heads || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    size_t offset = ((c * dev->heads + h) * dev->sectors + s) * 512;
    if (fseek(f, offset, SEEK_SET) != 0) { /* seek error */ }
    size_t read = fread(buf, 1, 512, f);
    fclose(f);
    
    return (read == 512) ? 0 : -1;
}

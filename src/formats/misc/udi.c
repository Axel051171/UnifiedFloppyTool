/**
 * @file udi.c
 * @brief UDI (Universal Disk Image) implementation
 */

#include "uft/formats/udi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint32_t udi_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= ~data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
        crc = ~crc;
    }
    return crc;
}

int udi_probe(const uint8_t *data, size_t size) {
    if (size < sizeof(UdiHeader)) return 0;
    
    const UdiHeader *hdr = (const UdiHeader *)data;
    
    if (memcmp(hdr->signature, UDI_SIGNATURE, 4) == 0) return 90;
    if (memcmp(hdr->signature, UDI_SIGNATURE_COMP, 4) == 0) return 85;  // Compressed
    
    return 0;
}

int udi_open(UdiDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    UdiHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    
    if (memcmp(hdr.signature, UDI_SIGNATURE, 4) != 0 &&
        memcmp(hdr.signature, UDI_SIGNATURE_COMP, 4) != 0) {
        fclose(f);
        return -1;
    }
    
    dev->cylinders = hdr.max_cyl + 1;
    dev->heads = (hdr.max_head & 1) + 1;
    dev->compressed = (memcmp(hdr.signature, UDI_SIGNATURE_COMP, 4) == 0);
    
    fclose(f);
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int udi_close(UdiDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int udi_read_track(UdiDevice *dev, uint32_t c, uint32_t h, uint8_t *buf, size_t *size) {
    if (!dev || !buf || !size || !dev->internal_ctx) return -1;
    if (c >= dev->cylinders || h >= dev->heads) return -1;
    
    if (dev->compressed) {
        // TODO: Implement decompression
        return -1;
    }
    
    // TODO: Implement track reading
    *size = 0;
    return -1;
}

/**
 * @file sap.c
 * @brief SAP (Thomson) implementation
 */

#include "uft/formats/sap.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void sap_decrypt_sector(uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        data[i] ^= SAP_CRYPT_BYTE;
    }
}

void sap_encrypt_sector(uint8_t *data, size_t size) {
    sap_decrypt_sector(data, size);  // XOR is symmetric
}

int sap_probe(const uint8_t *data, size_t size) {
    if (size < sizeof(SapHeader)) return 0;
    
    const SapHeader *hdr = (const SapHeader *)data;
    
    if (memcmp(hdr->signature, SAP_SIGNATURE, 66) == 0) return 95;
    
    return 0;
}

int sap_open(SapDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    SapHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    
    if (memcmp(hdr.signature, SAP_SIGNATURE, 66) != 0) {
        return -1;
    }
    
    dev->tracks = 80;
    dev->sectors = SAP_SECTORS_PER_TRACK;
    dev->sectorSize = 256;
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int sap_close(SapDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int sap_read_sector(SapDevice *dev, uint32_t t, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (t >= dev->tracks || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    // Skip header
    fseek(f, sizeof(SapHeader), SEEK_SET);
    
    // Each sector has 4-byte header + data
    size_t sector_size = sizeof(SapSector) + dev->sectorSize;
    size_t offset = sizeof(SapHeader) + (t * dev->sectors + s) * sector_size;
    
    fseek(f, offset + sizeof(SapSector), SEEK_SET);
    size_t read = fread(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    if (read != dev->sectorSize) return -1;
    
    // Decrypt
    sap_decrypt_sector(buf, dev->sectorSize);
    
    return 0;
}

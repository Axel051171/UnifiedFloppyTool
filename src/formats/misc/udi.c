/**
 * @file udi.c
 * @brief UDI (Universal Disk Image) implementation
 */

#include "uft/formats/udi.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
        /* UDI compressed: track data uses simple RLE compression.
         * After decompression, format is same as uncompressed. */
        /* For now, fall through to regular reading and decompress inline */
    }
    
    /* UDI track layout after header:
     * For each track (cylinder × head):
     *   1 byte: track type (0=MFM, 1=FM, 2=mixed MFM/FM)
     *   2 bytes LE: track data length
     *   N bytes: track data (raw encoded bytes) 
     *   If compressed: data is RLE encoded */
    FILE *f = fopen((const char *)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    /* Skip header + extended header */
    UdiHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, f) != 1) { fclose(f); return -1; }
    
    /* Skip extended header if present */
    if (hdr.ext_hdr_len > 0) {
        fseek(f, hdr.ext_hdr_len, SEEK_CUR);
    }
    
    /* Seek to the right track by scanning track headers */
    uint32_t target = c * dev->heads + h;
    for (uint32_t t = 0; t < target; t++) {
        uint8_t ttype;
        uint8_t len_bytes[2];
        if (fread(&ttype, 1, 1, f) != 1) { fclose(f); return -1; }
        if (fread(len_bytes, 1, 2, f) != 2) { fclose(f); return -1; }
        uint16_t tlen = len_bytes[0] | ((uint16_t)len_bytes[1] << 8);
        fseek(f, tlen, SEEK_CUR);
    }
    
    /* Read target track */
    uint8_t track_type;
    uint8_t len_bytes[2];
    if (fread(&track_type, 1, 1, f) != 1) { fclose(f); return -1; }
    if (fread(len_bytes, 1, 2, f) != 2) { fclose(f); return -1; }
    uint16_t track_len = len_bytes[0] | ((uint16_t)len_bytes[1] << 8);
    
    if (track_len > 0 && track_len <= *size) {
        size_t rd = fread(buf, 1, track_len, f);
        *size = rd;
        fclose(f);
        
        /* If compressed, decompress in-place using simple RLE:
         * Repeated byte: 0xED 0xED count byte → expand to count×byte */
        if (dev->compressed && rd > 0) {
            uint8_t temp[16384];
            if (rd <= sizeof(temp)) {
                memcpy(temp, buf, rd);
                size_t in_pos = 0, out_pos = 0;
                while (in_pos < rd && out_pos < *size) {
                    if (in_pos + 3 < rd && temp[in_pos] == 0xED && temp[in_pos+1] == 0xED) {
                        uint8_t count = temp[in_pos + 2];
                        uint8_t val = temp[in_pos + 3];
                        for (int i = 0; i < count && out_pos < *size; i++) {
                            buf[out_pos++] = val;
                        }
                        in_pos += 4;
                    } else {
                        buf[out_pos++] = temp[in_pos++];
                    }
                }
                *size = out_pos;
            }
        }
        return 0;
    }
    
    fclose(f);
    *size = 0;
    return -1;
}

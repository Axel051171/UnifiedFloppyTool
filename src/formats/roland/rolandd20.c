/**
 * @file rolandd20.c
 * @brief Roland D-20/D-10/D-110 synthesizer disk format
 * 
 * Roland synthesizers use 3.5" DD disks with custom filesystem
 * for storing patches, samples, and sequences.
 */

#include "uft/formats/rolandd20.h"
#include "uft/formats/uft_format_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Roland disk geometry
#define ROLAND_TRACKS   80
#define ROLAND_HEADS    2
#define ROLAND_SECTORS  9
#define ROLAND_SECTOR_SIZE 512

int roland_probe(const uint8_t *data, size_t size) {
    // Roland D-20: 80 × 2 × 9 × 512 = 737,280 (720KB)
    if (size == ROLAND_TRACKS * ROLAND_HEADS * ROLAND_SECTORS * ROLAND_SECTOR_SIZE) {
        /* Check for Roland signature patterns in boot area:
         * Roland D-20/D-10/D-110 disks have specific byte patterns.
         * Track 0, Sector 0 typically contains model identifier. */
        int confidence = 50;  /* Base confidence for correct size */
        
        /* Roland disks often have "ROLAND" or model name in first sector */
        if (size >= 512) {
            for (int i = 0; i < 500; i++) {
                if (data[i] == 'R' && data[i+1] == 'O' && data[i+2] == 'L' &&
                    data[i+3] == 'A' && data[i+4] == 'N' && data[i+5] == 'D') {
                    confidence = 90;
                    break;
                }
                /* Also check for D-20, D-10, D-110, D-5, D-50 patterns */
                if (data[i] == 'D' && data[i+1] == '-' &&
                    (data[i+2] >= '0' && data[i+2] <= '9')) {
                    confidence = 85;
                    break;
                }
            }
            /* Check for 0xF0 SysEx header bytes (MIDI patch data) */
            if (data[0] == 0xF0 && data[1] == 0x41) {
                confidence = 80;  /* Roland SysEx manufacturer ID */
            }
        }
        return confidence;
    }
    return 0;
}

int roland_open(RolandDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(f);
    fclose(f);
    
    if (size != ROLAND_TRACKS * ROLAND_HEADS * ROLAND_SECTORS * ROLAND_SECTOR_SIZE) {
        return -1;
    }
    
    dev->tracks = ROLAND_TRACKS;
    dev->heads = ROLAND_HEADS;
    dev->sectors = ROLAND_SECTORS;
    dev->sectorSize = ROLAND_SECTOR_SIZE;
    /* Detect Roland model from disk content:
     * D-20: SysEx device ID 0x14 (20 decimal)
     * D-10: SysEx device ID 0x16
     * D-110: SysEx device ID 0x10
     * D-50: SysEx device ID 0x0D */
    strncpy(dev->model, "D-20", sizeof(dev->model)-1);  /* Default */
    {
        FILE *mf = fopen(path, "rb");
        if (mf) {
            uint8_t boot[512];
            if (fread(boot, 1, 512, mf) == 512) {
                /* Scan for Roland SysEx: F0 41 xx yy where yy = model ID */
                for (int i = 0; i < 508; i++) {
                    if (boot[i] == 0xF0 && boot[i+1] == 0x41) {
                        uint8_t model_id = boot[i+3];
                        switch (model_id) {
                            case 0x14: strncpy(dev->model, "D-20", sizeof(dev->model)-1); break;
                            case 0x16: strncpy(dev->model, "D-10", sizeof(dev->model)-1); break;
                            case 0x10: strncpy(dev->model, "D-110", sizeof(dev->model)-1); break;
                            case 0x0D: strncpy(dev->model, "D-50", sizeof(dev->model)-1); break;
                            case 0x2B: strncpy(dev->model, "D-70", sizeof(dev->model)-1); break;
                            default: break;
                        }
                        break;
                    }
                    /* Also check for text model name */
                    if (boot[i] == 'D' && boot[i+1] == '-') {
                        char model_str[8] = {0};
                        int j = 0;
                        while (j < 6 && (boot[i+j] >= '0' || boot[i+j] == '-' || boot[i+j] == 'D')) {
                            model_str[j] = boot[i+j];
                            j++;
                        }
                        if (j >= 3) {
                            strncpy(dev->model, model_str, sizeof(dev->model)-1);
                        }
                        break;
                    }
                }
            }
            fclose(mf);
        }
    }
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int roland_close(RolandDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int roland_read_sector(RolandDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (t >= dev->tracks || h >= dev->heads || s >= dev->sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    size_t offset = ((t * dev->heads + h) * dev->sectors + s) * dev->sectorSize;
    if (fseek(f, offset, SEEK_SET) != 0) { /* seek error */ }
    size_t read = fread(buf, 1, dev->sectorSize, f);
    fclose(f);
    
    return (read == dev->sectorSize) ? 0 : -1;
}

int roland_list_patches(RolandDevice *dev) {
    if (!dev || !dev->internal_ctx) return -1;
    
    /* Roland D-20 patch structure:
     * Patches are stored starting at a known offset depending on model.
     * D-20: 128 patches, each 64 bytes (some models differ).
     * Patch name is stored in first 10 bytes of each patch entry. */
    
    uint8_t sector[ROLAND_SECTOR_SIZE];
    int patch_count = 0;
    
    /* Patches typically start at track 1 for D-20 series.
     * Read through data area looking for patch names */
    for (uint32_t t = 1; t < 4 && patch_count < 128; t++) {
        for (uint32_t s = 0; s < (uint32_t)dev->sectors && patch_count < 128; s++) {
            if (roland_read_sector(dev, t, 0, s, sector) != 0) continue;
            
            /* Scan for patch boundaries (64-byte aligned entries with printable names) */
            for (int off = 0; off + 64 <= ROLAND_SECTOR_SIZE; off += 64) {
                const uint8_t *patch = sector + off;
                
                /* Check if first 10 bytes look like a patch name (printable ASCII) */
                bool valid_name = true;
                bool all_zero = true;
                for (int i = 0; i < 10; i++) {
                    if (patch[i] != 0) all_zero = false;
                    if (patch[i] != 0 && (patch[i] < 0x20 || patch[i] > 0x7E)) {
                        valid_name = false;
                        break;
                    }
                }
                
                if (valid_name && !all_zero) {
                    char name[11];
                    memcpy(name, patch, 10);
                    name[10] = '\0';
                    /* Trim trailing spaces */
                    for (int i = 9; i >= 0 && name[i] == ' '; i--) name[i] = '\0';
                    
                    printf("%3d: %s\n", patch_count + 1, name);
                    patch_count++;
                }
            }
        }
    }
    
    return patch_count;
}

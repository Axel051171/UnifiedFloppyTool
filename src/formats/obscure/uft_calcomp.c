/**
 * @file uft_calcomp.c
 * @brief Calcomp Vistagraphics 4500 Disk Format Support
 * @version 4.1.3
 * 
 * Calcomp Vistagraphics 4500 - Graphics Workstation
 * Professional plotter/CAD workstation from the 1980s
 * 
 * Disk format:
 * - DS/DD: 80 tracks, 9 sectors, 512 bytes = 720 KB
 * 
 * FC5025 compatible format
 */

#include "uft/formats/uft_calcomp.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define CALCOMP_TRACKS      80
#define CALCOMP_SECTORS     9
#define CALCOMP_HEADS       2
#define CALCOMP_SECTOR_SIZE 512
#define CALCOMP_IMAGE_SIZE  (80 * 9 * 2 * 512)

int uft_calcomp_probe(const uint8_t *data, size_t size) {
    if (!data || size != CALCOMP_IMAGE_SIZE) return 0;
    
    int confidence = 35;
    
    /* Check for boot sector patterns */
    if (data[0] == 0xEB || data[0] == 0xE9 || data[0] == 0x00) {
        confidence += 10;
    }
    
    /* Check for non-empty content */
    int non_zero = 0;
    for (int i = 0; i < 512 && i < (int)size; i++) {
        if (data[i] != 0x00 && data[i] != 0xE5 && data[i] != 0xFF) non_zero++;
    }
    if (non_zero > 50) confidence += 15;
    
    return confidence > 45 ? confidence : 0;
}

int uft_calcomp_read(const char *path, uft_calcomp_image_t **image) {
    if (!path || !image) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size != CALCOMP_IMAGE_SIZE) { fclose(f); return UFT_ERR_UNKNOWN_FORMAT; }
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }
    
    if (fread(data, 1, size, f) != size) {
        free(data); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);
    
    uft_calcomp_image_t *img = calloc(1, sizeof(uft_calcomp_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    img->data = data;
    img->size = size;
    img->tracks = CALCOMP_TRACKS;
    img->sectors = CALCOMP_SECTORS;
    img->heads = CALCOMP_HEADS;
    img->sector_size = CALCOMP_SECTOR_SIZE;
    
    *image = img;
    return UFT_OK;
}

void uft_calcomp_free(uft_calcomp_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_calcomp_get_info(uft_calcomp_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Calcomp Vistagraphics 4500 Disk Image\n"
        "Geometry: 80 tracks x 9 sectors x 2 heads\n"
        "Total Size: 720 KB\n");
    return UFT_OK;
}

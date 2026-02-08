/**
 * @file uft_pravetz.c
 * @brief Pravetz Computer Disk Format Support (Bulgaria)
 * @version 4.1.4
 * 
 * Pravetz 82/8M/8A/8C - Bulgarian Apple II Clone (1982-1990)
 * 6502-compatible CM630 CPU
 * Apple DOS 3.3 / ProDOS compatible with local extensions
 * 
 * Disk formats:
 * - Pravetz 82: 35 tracks, 16 sectors, 256 bytes = 140 KB (Apple compatible)
 * - Pravetz 8M: 35 tracks, 16 sectors, 256 bytes = 140 KB
 * - Pravetz 8A/8C: 80 tracks, 16 sectors, 256 bytes = 320 KB (extended)
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_pravetz.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_pravetz_geom[] = {
    { 35, 16, 1, 256, 143360,  "Pravetz 82/8M 140KB (Apple compat)" },
    { 40, 16, 1, 256, 163840,  "Pravetz 160KB extended" },
    { 80, 16, 1, 256, 327680,  "Pravetz 8A/8C 320KB" },
    { 80, 16, 2, 256, 655360,  "Pravetz 640KB DS" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_pravetz_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_pravetz_geom[i].name; i++) {
        if (size == g_pravetz_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for Apple DOS 3.3 / ProDOS patterns */
            /* VTOC at track 17, sector 0 for DOS 3.3 */
            if (size >= 143360) {
                size_t vtoc_offset = 17 * 16 * 256;
                if (vtoc_offset < size && data[vtoc_offset] == 0x04) {
                    confidence += 25; /* DOS 3.3 VTOC signature */
                }
            }
            
            /* Check for Cyrillic text patterns (Bulgarian) */
            int cyrillic = 0;
            for (int j = 0; j < 256 && j < (int)size; j++) {
                if (data[j] >= 0xC0 && data[j] <= 0xFF) cyrillic++;
            }
            if (cyrillic > 10) confidence += 10;
            
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_pravetz_read(const char *path, uft_pravetz_image_t **image) {
    if (!path || !image) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) return UFT_ERR_IO;
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }
    
    if (fread(data, 1, size, f) != size) {
        free(data); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);
    
    uft_pravetz_image_t *img = calloc(1, sizeof(uft_pravetz_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_pravetz_geom[i].name; i++) {
        if (size == g_pravetz_geom[i].total_size) {
            img->tracks = g_pravetz_geom[i].tracks;
            img->sectors = g_pravetz_geom[i].sectors;
            img->heads = g_pravetz_geom[i].heads;
            img->sector_size = g_pravetz_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_pravetz_free(uft_pravetz_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_pravetz_get_info(uft_pravetz_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Pravetz Disk Image (Bulgaria - Apple II Clone)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

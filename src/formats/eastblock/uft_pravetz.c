/**
 * @file uft_pravetz.c
 * @brief Pravetz 82/8M Disk Format Support (Bulgaria)
 * @version 4.1.4
 * 
 * Pravetz 82/8M - Bulgarian Apple II Clone (1985-1990s)
 * Compatible with Apple II DOS 3.3 and ProDOS
 * MOS 6502 compatible CPU
 * 
 * Disk formats:
 * - Pravetz 82: 35 tracks, 16 sectors, 256 bytes = 140 KB (Apple II)
 * - Pravetz 8M: 80 tracks, 16 sectors, 256 bytes = 320 KB
 * - Pravetz 8D: 80 tracks, 9 sectors, 512 bytes = 360 KB
 */

#include "uft/formats/uft_pravetz.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_pravetz_geom[] = {
    { 35, 16, 1, 256, 143360,  "Pravetz 82 (Apple II) 140KB" },
    { 40, 16, 1, 256, 163840,  "Pravetz 82+ 160KB" },
    { 80, 16, 1, 256, 327680,  "Pravetz 8M 320KB" },
    { 80, 9, 2, 512, 737280,   "Pravetz 8D DS/DD 720KB" },
    { 80, 9, 1, 512, 368640,   "Pravetz 8D SS/DD 360KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_pravetz_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_pravetz_geom[i].name; i++) {
        if (size == g_pravetz_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for DOS 3.3 VTOC at track 17 */
            if (size >= 143360 && g_pravetz_geom[i].sector_size == 256) {
                size_t vtoc_offset = 17 * 16 * 256;
                if (vtoc_offset < size && data[vtoc_offset] == 0x04) {
                    confidence += 25;
                }
            }
            
            /* Check for ProDOS boot block */
            if (data[0] == 0x01 && data[1] == 0x38 && data[2] == 0xB0) {
                confidence += 20;
            }
            
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
    fseek(f, 0, SEEK_SET);
    
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
        "Sector Size: %d bytes\nTotal: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

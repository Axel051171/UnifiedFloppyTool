/**
 * @file uft_pyldin.c
 * @brief Pyldin 601 Disk Format Support
 * @version 4.1.3
 * 
 * Pyldin 601 - Bulgarian home computer (1985-1990)
 * Z80A compatible CPU (CM601), 64K RAM
 * CP/M compatible operating system (UniDOS)
 * 
 * Disk formats:
 * - SS/DD: 80 tracks, 9 sectors, 512 bytes = 360 KB
 * - DS/DD: 80 tracks, 9 sectors, 512 bytes = 720 KB
 * - SS/SD: 40 tracks, 9 sectors, 256 bytes = 90 KB
 */

#include "uft/formats/uft_pyldin.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_pyldin_geom[] = {
    { 80, 9, 2, 512, 737280,  "Pyldin DS/DD 720KB" },
    { 80, 9, 1, 512, 368640,  "Pyldin SS/DD 360KB" },
    { 80, 5, 2, 1024, 819200, "Pyldin DS/DD 1024b 800KB" },
    { 40, 9, 1, 256, 92160,   "Pyldin SS/SD 90KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_pyldin_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_pyldin_geom[i].name; i++) {
        if (size == g_pyldin_geom[i].total_size) {
            int confidence = 30;
            
            /* Check for CP/M-like boot sector or UniDOS */
            if (data[0] == 0xC3 || data[0] == 0xEB || data[0] == 0x00) {
                confidence += 15;
            }
            
            /* Check for 0xE5 fill in directory area */
            int sector_size = g_pyldin_geom[i].sector_size;
            if (size >= (size_t)(sector_size * 3)) {
                int e5_count = 0;
                for (int j = 0; j < sector_size && j < (int)size - sector_size * 2; j++) {
                    if (data[sector_size * 2 + j] == 0xE5) e5_count++;
                }
                if (e5_count > sector_size / 2) confidence += 20;
            }
            
            return confidence > 40 ? confidence : 0;
        }
    }
    return 0;
}

int uft_pyldin_read(const char *path, uft_pyldin_image_t **image) {
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
    
    uft_pyldin_image_t *img = calloc(1, sizeof(uft_pyldin_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_pyldin_geom[i].name; i++) {
        if (size == g_pyldin_geom[i].total_size) {
            img->tracks = g_pyldin_geom[i].tracks;
            img->sectors = g_pyldin_geom[i].sectors;
            img->heads = g_pyldin_geom[i].heads;
            img->sector_size = g_pyldin_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_pyldin_free(uft_pyldin_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_pyldin_get_info(uft_pyldin_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Pyldin 601 Disk Image (Bulgaria)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

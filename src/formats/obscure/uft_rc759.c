/**
 * @file uft_rc759.c
 * @brief RC759 Piccoline Disk Format Support
 * @version 4.1.3
 * 
 * RC759 Piccoline - Danish personal computer (1984)
 * Made by Regnecentralen, 8088 CPU
 * CP/M-86 and MS-DOS compatible
 * 
 * Disk formats:
 * - DS/DD: 80 tracks, 9 sectors, 512 bytes = 720 KB
 * - DS/DD: 80 tracks, 8 sectors, 512 bytes = 640 KB
 * Uses non-standard sector interleave
 */

#include "uft/formats/uft_rc759.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_rc759_geom[] = {
    { 80, 9, 2, 512, 737280,  "RC759 DS/DD 720KB" },
    { 80, 8, 2, 512, 655360,  "RC759 DS/DD 640KB" },
    { 40, 9, 2, 512, 368640,  "RC759 DS/DD 40T 360KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_rc759_probe(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0;
    
    for (int i = 0; g_rc759_geom[i].name; i++) {
        if (size == g_rc759_geom[i].total_size) {
            int confidence = 30;
            
            /* Check for x86 boot code */
            if (data[0] == 0xEB || data[0] == 0xE9) {
                confidence += 15;
            }
            
            /* Check for BPB-like structure at offset 11 */
            if (size >= 30) {
                uint16_t bytes_per_sector = data[11] | (data[12] << 8);
                if (bytes_per_sector == 512) confidence += 15;
            }
            
            return confidence > 40 ? confidence : 0;
        }
    }
    return 0;
}

int uft_rc759_read(const char *path, uft_rc759_image_t **image) {
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
    
    uft_rc759_image_t *img = calloc(1, sizeof(uft_rc759_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_rc759_geom[i].name; i++) {
        if (size == g_rc759_geom[i].total_size) {
            img->tracks = g_rc759_geom[i].tracks;
            img->sectors = g_rc759_geom[i].sectors;
            img->heads = g_rc759_geom[i].heads;
            img->sector_size = g_rc759_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_rc759_free(uft_rc759_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_rc759_get_info(uft_rc759_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "RC759 Piccoline Disk Image (Denmark)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Total Size: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->size / 1024);
    return UFT_OK;
}

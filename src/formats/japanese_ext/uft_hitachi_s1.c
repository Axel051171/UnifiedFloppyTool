/**
 * @file uft_hitachi_s1.c
 * @brief Hitachi S1 Disk Format Support
 * @version 4.1.4
 * 
 * Hitachi S1 - Japanese Business Computer (1979-1985)
 * Used in banking and business applications
 * 
 * Disk formats:
 * - 8" SSSD: 77 tracks, 26 sectors, 128 bytes = 256 KB
 * - 8" DSDD: 77 tracks, 26 sectors, 256 bytes, DS = 1 MB
 * - 5.25" DD: 80 tracks, 16 sectors, 256 bytes = 320 KB
 */

#include "uft/formats/uft_hitachi_s1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_hitachi_geom[] = {
    { 77, 26, 1, 128, 256256,   "S1 8\" SSSD 250KB" },
    { 77, 26, 2, 256, 1025024,  "S1 8\" DSDD 1MB" },
    { 80, 16, 2, 256, 655360,   "S1 5.25\" DS/DD 640KB" },
    { 80, 8, 2, 512, 655360,    "S1 5.25\" 8-sector 640KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_hitachi_s1_probe(const uint8_t *data, size_t size) {
    if (!data || size < 128) return 0;
    
    for (int i = 0; g_hitachi_geom[i].name; i++) {
        if (size == g_hitachi_geom[i].total_size) {
            int confidence = 30;
            
            /* Hitachi S1 specific patterns */
            if (data[0] != 0x00 && data[0] != 0xFF) {
                confidence += 15;
            }
            
            return confidence > 35 ? confidence : 0;
        }
    }
    return 0;
}

int uft_hitachi_s1_read(const char *path, uft_hitachi_s1_image_t **image) {
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
    
    uft_hitachi_s1_image_t *img = calloc(1, sizeof(uft_hitachi_s1_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_hitachi_geom[i].name; i++) {
        if (size == g_hitachi_geom[i].total_size) {
            img->tracks = g_hitachi_geom[i].tracks;
            img->sectors = g_hitachi_geom[i].sectors;
            img->heads = g_hitachi_geom[i].heads;
            img->sector_size = g_hitachi_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_hitachi_s1_free(uft_hitachi_s1_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_hitachi_s1_get_info(uft_hitachi_s1_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Hitachi S1 Disk Image (Japan Business)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

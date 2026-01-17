/**
 * @file uft_cromemco.c
 * @brief Cromemco CDOS Disk Format Support
 * @version 4.1.4
 * 
 * Cromemco - S-100 Bus Computer (1976-1980s)
 * Z80 CPU, CDOS (Cromemco Disk Operating System)
 * 
 * Disk formats:
 * - Large 5.25": 77 tracks, 16 sectors, 512 bytes = 616 KB
 * - Small 5.25": 40 tracks, 18 sectors, 128 bytes = 90 KB
 * - 8" SSSD: 77 tracks, 26 sectors, 128 bytes = 256 KB
 * - 8" DSDD: 77 tracks, 26 sectors, 1024 bytes, DS = 4 MB
 */

#include "uft/formats/uft_cromemco.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_cromemco_geom[] = {
    { 77, 16, 1, 512, 631808,   "Cromemco Large 5.25\" 616KB" },
    { 40, 18, 1, 128, 92160,    "Cromemco Small 5.25\" 90KB" },
    { 77, 26, 1, 128, 256256,   "Cromemco 8\" SSSD 250KB" },
    { 77, 26, 2, 256, 1025024,  "Cromemco 8\" DSDD 1MB" },
    { 80, 10, 2, 512, 819200,   "Cromemco CDOS 800KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_cromemco_probe(const uint8_t *data, size_t size) {
    if (!data || size < 128) return 0;
    
    for (int i = 0; g_cromemco_geom[i].name; i++) {
        if (size == g_cromemco_geom[i].total_size) {
            int confidence = 30;
            
            /* CDOS has specific boot patterns */
            if (data[0] == 0xC3) { /* JP instruction */
                confidence += 20;
            }
            
            /* Check for CDOS directory patterns */
            if (size >= 256 && data[128] != 0xFF) {
                confidence += 10;
            }
            
            return confidence > 40 ? confidence : 0;
        }
    }
    return 0;
}

int uft_cromemco_read(const char *path, uft_cromemco_image_t **image) {
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
    
    uft_cromemco_image_t *img = calloc(1, sizeof(uft_cromemco_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_cromemco_geom[i].name; i++) {
        if (size == g_cromemco_geom[i].total_size) {
            img->tracks = g_cromemco_geom[i].tracks;
            img->sectors = g_cromemco_geom[i].sectors;
            img->heads = g_cromemco_geom[i].heads;
            img->sector_size = g_cromemco_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_cromemco_free(uft_cromemco_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_cromemco_get_info(uft_cromemco_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Cromemco CDOS Disk Image (S-100)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

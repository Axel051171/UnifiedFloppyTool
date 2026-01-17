/**
 * @file uft_cromemco.c
 * @brief Cromemco CDOS Disk Format Support
 * @version 4.1.4
 * 
 * Cromemco - S-100 Bus Computers (1976-1987)
 * Z80 CPU, CDOS / Cromix operating systems
 * Used in industrial, scientific, and business applications
 * 
 * Disk formats:
 * - CDOS SS: 40 tracks, 10 sectors, 512 bytes = 200 KB
 * - CDOS DS: 80 tracks, 10 sectors, 512 bytes = 800 KB
 * - Large format: 77 tracks, 26 sectors, 128 bytes = 250 KB (8")
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
    { 40, 10, 1, 512, 204800,  "Cromemco CDOS SS 200KB" },
    { 40, 10, 2, 512, 409600,  "Cromemco CDOS DS 400KB" },
    { 80, 10, 2, 512, 819200,  "Cromemco CDOS DS 800KB" },
    { 77, 26, 1, 128, 256256,  "Cromemco 8\" SS/SD 250KB" },
    { 77, 26, 2, 256, 1025024, "Cromemco 8\" DS/DD 1MB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_cromemco_probe(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0;
    
    for (int i = 0; g_cromemco_geom[i].name; i++) {
        if (size == g_cromemco_geom[i].total_size) {
            int confidence = 30;
            
            /* CDOS boot sector patterns */
            if (data[0] == 0xC3 || data[0] == 0x00) confidence += 15;
            
            /* Check for directory area patterns */
            int e5_count = 0;
            int sector_size = g_cromemco_geom[i].sector_size;
            if (size >= (size_t)(sector_size * 2)) {
                for (int j = 0; j < sector_size && j < (int)size - sector_size; j++) {
                    if (data[sector_size + j] == 0xE5) e5_count++;
                }
                if (e5_count > sector_size / 2) confidence += 20;
            }
            
            return confidence > 45 ? confidence : 0;
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
        "Cromemco Disk Image (S-100 Bus)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n"
        "Operating System: CDOS / Cromix\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

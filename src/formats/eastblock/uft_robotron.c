/**
 * @file uft_robotron.c
 * @brief Robotron KC 85/87 Disk Format Support (DDR)
 * @version 4.1.4
 * 
 * Robotron KC 85/87 - East German Home Computer (1984-1990)
 * Z80-compatible U880 CPU @ 1.75 MHz
 * MicroDOS (CP/M compatible) operating system
 * 
 * Disk formats (5.25" and 3.5"):
 * - KC 85/3-4: 40 tracks, 5 sectors, 1024 bytes, SS = 200 KB
 * - KC 85/4+:  80 tracks, 5 sectors, 1024 bytes, DS = 800 KB
 * - MicroDOS:  80 tracks, 9 sectors, 512 bytes, DS = 720 KB
 */

#include "uft/formats/uft_robotron.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_robotron_geom[] = {
    { 40, 5, 1, 1024, 204800,  "KC 85/3 SS 200KB" },
    { 80, 5, 1, 1024, 409600,  "KC 85/4 SS 400KB" },
    { 80, 5, 2, 1024, 819200,  "KC 85/4 DS 800KB" },
    { 80, 9, 2, 512,  737280,  "KC MicroDOS 720KB" },
    { 40, 9, 2, 512,  368640,  "KC MicroDOS 360KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_robotron_probe(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0;
    
    for (int i = 0; g_robotron_geom[i].name; i++) {
        if (size == g_robotron_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for MicroDOS/CP/M patterns */
            if (data[0] == 0xC3 || data[0] == 0x00 || data[0] == 0xE5) {
                confidence += 15;
            }
            
            /* Check for 0xE5 fill (empty directory) */
            int sector_size = g_robotron_geom[i].sector_size;
            if (size >= (size_t)(sector_size * 3)) {
                int e5_count = 0;
                for (int j = 0; j < sector_size; j++) {
                    if (data[sector_size * 2 + j] == 0xE5) e5_count++;
                }
                if (e5_count > sector_size / 2) confidence += 20;
            }
            
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_robotron_read(const char *path, uft_robotron_image_t **image) {
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
    
    uft_robotron_image_t *img = calloc(1, sizeof(uft_robotron_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_robotron_geom[i].name; i++) {
        if (size == g_robotron_geom[i].total_size) {
            img->tracks = g_robotron_geom[i].tracks;
            img->sectors = g_robotron_geom[i].sectors;
            img->heads = g_robotron_geom[i].heads;
            img->sector_size = g_robotron_geom[i].sector_size;
            strncpy(img->variant, g_robotron_geom[i].name, sizeof(img->variant)-1);
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_robotron_free(uft_robotron_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_robotron_get_info(uft_robotron_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Robotron KC 85/87 Disk Image (DDR)\n"
        "Variant: %s\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal: %zu KB\n",
        img->variant, img->tracks, img->sectors, img->heads,
        img->sector_size, img->size / 1024);
    return UFT_OK;
}

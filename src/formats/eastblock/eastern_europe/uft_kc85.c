/**
 * @file uft_kc85.c
 * @brief Robotron KC 85/87 Disk Format Support (DDR)
 * @version 4.1.4
 * 
 * Robotron KC 85/87 - East German (DDR) Home Computer (1984-1989)
 * Z80-compatible U880 CPU at 1.75 MHz
 * MicroDOS / CAOS operating system
 * 
 * Disk formats (5.25" and 3.5"):
 * - KC 85/87: 40 tracks, 5 sectors, 1024 bytes, SS = 200 KB
 * - KC 85/87: 80 tracks, 5 sectors, 1024 bytes, SS = 400 KB
 * - KC 85/87: 80 tracks, 9 sectors, 512 bytes, DS = 720 KB (PC-compatible)
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_kc85.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_kc85_geom[] = {
    { 40, 5, 1, 1024, 204800,  "KC 85 SS 200KB MicroDOS" },
    { 80, 5, 1, 1024, 409600,  "KC 85 SS 400KB MicroDOS" },
    { 80, 5, 2, 1024, 819200,  "KC 85 DS 800KB MicroDOS" },
    { 80, 9, 2, 512, 737280,   "KC 85 DS 720KB PC-compat" },
    { 40, 9, 2, 512, 368640,   "KC 85 DS 360KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_kc85_probe(const uint8_t *data, size_t size) {
    if (!data || size < 1024) return 0;
    
    for (int i = 0; g_kc85_geom[i].name; i++) {
        if (size == g_kc85_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for MicroDOS/CAOS patterns */
            /* Boot sector often has Z80 JP instruction (0xC3) */
            if (data[0] == 0xC3) confidence += 15;
            
            /* Check for directory patterns */
            if (g_kc85_geom[i].sector_size == 1024) {
                int e5_count = 0;
                for (int j = 0; j < 256 && j < (int)size - 1024; j++) {
                    if (data[1024 + j] == 0xE5) e5_count++;
                }
                if (e5_count > 200) confidence += 20;
            }
            
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_kc85_read(const char *path, uft_kc85_image_t **image) {
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
    
    uft_kc85_image_t *img = calloc(1, sizeof(uft_kc85_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_kc85_geom[i].name; i++) {
        if (size == g_kc85_geom[i].total_size) {
            img->tracks = g_kc85_geom[i].tracks;
            img->sectors = g_kc85_geom[i].sectors;
            img->heads = g_kc85_geom[i].heads;
            img->sector_size = g_kc85_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_kc85_free(uft_kc85_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_kc85_get_info(uft_kc85_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Robotron KC 85/87 Disk Image (DDR)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n"
        "Operating System: MicroDOS / CAOS\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

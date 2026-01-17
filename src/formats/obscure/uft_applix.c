/**
 * @file uft_applix.c
 * @brief Applix 1616 Disk Format Support
 * @version 4.1.3
 * 
 * Applix 1616 - Australian 68000-based computer (1986)
 * Motorola 68000 CPU at 7.5 MHz, 512K-2MB RAM
 * Multi-user/multi-tasking OS (1616/OS)
 * 
 * Disk formats (3.5" and 5.25"):
 * - DS/DD 3.5": 80 tracks, 9 sectors, 512 bytes = 720 KB
 * - DS/HD 3.5": 80 tracks, 18 sectors, 512 bytes = 1.44 MB
 * - DS/DD 5.25": 80 tracks, 9 sectors, 512 bytes = 720 KB
 */

#include "uft/formats/uft_applix.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_applix_geom[] = {
    { 80, 9, 2, 512, 737280,   "Applix DS/DD 720KB" },
    { 80, 18, 2, 512, 1474560, "Applix DS/HD 1.44MB" },
    { 80, 10, 2, 512, 819200,  "Applix DS/DD 800KB" },
    { 40, 9, 2, 512, 368640,   "Applix DS/DD 40T 360KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_applix_probe(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0;
    
    for (int i = 0; g_applix_geom[i].name; i++) {
        if (size == g_applix_geom[i].total_size) {
            int confidence = 30;
            
            /* Check for 68000 code patterns */
            if (size > 2) {
                uint16_t word = (data[0] << 8) | data[1];
                if ((word & 0xF000) == 0x4000 ||  /* LEA, CHK, etc */
                    (word & 0xF000) == 0x6000 ||  /* Bcc */
                    word == 0x4E75) {             /* RTS */
                    confidence += 15;
                }
            }
            
            /* Check for 1616/OS boot block signature */
            int printable = 0;
            for (int j = 0; j < 32 && j < (int)size; j++) {
                if ((data[j] >= 0x20 && data[j] <= 0x7E) || data[j] == 0) {
                    printable++;
                }
            }
            if (printable > 20) confidence += 15;
            
            return confidence > 40 ? confidence : 0;
        }
    }
    return 0;
}

int uft_applix_read(const char *path, uft_applix_image_t **image) {
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
    
    uft_applix_image_t *img = calloc(1, sizeof(uft_applix_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_applix_geom[i].name; i++) {
        if (size == g_applix_geom[i].total_size) {
            img->tracks = g_applix_geom[i].tracks;
            img->sectors = g_applix_geom[i].sectors;
            img->heads = g_applix_geom[i].heads;
            img->sector_size = g_applix_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_applix_free(uft_applix_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_applix_get_info(uft_applix_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Applix 1616 Disk Image (Australia)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Total Size: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->size / 1024);
    return UFT_OK;
}

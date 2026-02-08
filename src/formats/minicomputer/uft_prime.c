/**
 * @file uft_prime.c
 * @brief Prime Computer Disk Format Support
 * @version 4.1.4
 * 
 * Prime Computer - 32-bit Minicomputers (1972-1992)
 * PRIMOS operating system
 * 
 * Floppy formats:
 * - 8" DS/DD: 77 tracks, 26 sectors, 256 bytes = 1 MB
 * - 5.25" DS/HD: 80 tracks, 15 sectors, 512 bytes = 1.2 MB
 */

#include "uft/formats/uft_prime.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_prime_geom[] = {
    { 77, 26, 2, 256, 1025024,  "Prime 8\" DS/DD 1MB" },
    { 80, 15, 2, 512, 1228800,  "Prime 5.25\" DS/HD 1.2MB" },
    { 80, 9, 2, 512, 737280,    "Prime 5.25\" DS/DD 720KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_prime_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_prime_geom[i].name; i++) {
        if (size == g_prime_geom[i].total_size) {
            int confidence = 30;
            
            /* PRIMOS uses specific volume label format */
            int printable = 0;
            for (int j = 0; j < 128 && j < (int)size; j++) {
                if ((data[j] >= 0x20 && data[j] <= 0x7E) || data[j] == 0) {
                    printable++;
                }
            }
            if (printable > 80) confidence += 20;
            
            return confidence > 40 ? confidence : 0;
        }
    }
    return 0;
}

int uft_prime_read(const char *path, uft_prime_image_t **image) {
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
    
    uft_prime_image_t *img = calloc(1, sizeof(uft_prime_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_prime_geom[i].name; i++) {
        if (size == g_prime_geom[i].total_size) {
            img->tracks = g_prime_geom[i].tracks;
            img->sectors = g_prime_geom[i].sectors;
            img->heads = g_prime_geom[i].heads;
            img->sector_size = g_prime_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_prime_free(uft_prime_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_prime_get_info(uft_prime_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Prime Computer Disk Image\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n"
        "Operating System: PRIMOS\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

/**
 * @file uft_sharp_x1.c
 * @brief Sharp X1 Disk Format Support (Japan)
 * @version 4.1.4
 * 
 * Sharp X1/X1 Turbo/X1 Turbo Z - Japanese Home Computer (1982-1988)
 * Z80A CPU at 4 MHz, CZ-8FB01 floppy controller
 * S-OS, CP/M, Hu-BASIC operating systems
 * 
 * Disk formats (5.25" and 3.5"):
 * - 2D: 40 tracks, 16 sectors, 256 bytes, DS = 320 KB
 * - 2DD: 80 tracks, 16 sectors, 256 bytes, DS = 640 KB
 * - 2HD: 77 tracks, 8 sectors, 1024 bytes, DS = 1.2 MB
 */

#include "uft/formats/uft_sharp_x1.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_sharp_x1_geom[] = {
    { 40, 16, 2, 256, 327680,   "Sharp X1 2D 320KB" },
    { 80, 16, 2, 256, 655360,   "Sharp X1 2DD 640KB" },
    { 77, 8, 2, 1024, 1261568,  "Sharp X1 2HD 1.2MB" },
    { 80, 9, 2, 512, 737280,    "Sharp X1 720KB (PC-compat)" },
    { 40, 16, 1, 256, 163840,   "Sharp X1 SS 160KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_sharp_x1_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_sharp_x1_geom[i].name; i++) {
        if (size == g_sharp_x1_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for S-OS or Hu-BASIC boot patterns */
            if (data[0] == 0xC3 || data[0] == 0xEB) confidence += 10;
            
            /* X1 often has specific IPL signature */
            if (size > 16) {
                int ascii_count = 0;
                for (int j = 0; j < 16; j++) {
                    if ((data[j] >= 0x20 && data[j] <= 0x7E) || data[j] == 0) {
                        ascii_count++;
                    }
                }
                if (ascii_count > 10) confidence += 15;
            }
            
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_sharp_x1_read(const char *path, uft_sharp_x1_image_t **image) {
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
    
    uft_sharp_x1_image_t *img = calloc(1, sizeof(uft_sharp_x1_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_sharp_x1_geom[i].name; i++) {
        if (size == g_sharp_x1_geom[i].total_size) {
            img->tracks = g_sharp_x1_geom[i].tracks;
            img->sectors = g_sharp_x1_geom[i].sectors;
            img->heads = g_sharp_x1_geom[i].heads;
            img->sector_size = g_sharp_x1_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_sharp_x1_free(uft_sharp_x1_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_sharp_x1_get_info(uft_sharp_x1_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Sharp X1 Disk Image (Japan)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n"
        "Operating System: S-OS / Hu-BASIC / CP/M\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

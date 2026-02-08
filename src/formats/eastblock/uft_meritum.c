/**
 * @file uft_meritum.c
 * @brief Meritum/TNS Disk Format Support (Poland/Czechoslovakia)
 * @version 4.1.4
 * 
 * Meritum (Poland) / TNS (Czechoslovakia) - TRS-80 Clone (1985-1990)
 * Z80 compatible CPU
 * TRSDOS/NEWDOS compatible
 * 
 * Disk formats:
 * - SS/SD: 40 tracks, 10 sectors, 256 bytes = 100 KB
 * - SS/DD: 40 tracks, 18 sectors, 256 bytes = 180 KB  
 * - DS/DD: 80 tracks, 18 sectors, 256 bytes = 360 KB
 */

#include "uft/formats/uft_meritum.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_meritum_geom[] = {
    { 40, 10, 1, 256, 102400,  "Meritum SS/SD 100KB" },
    { 40, 18, 1, 256, 184320,  "Meritum SS/DD 180KB" },
    { 40, 18, 2, 256, 368640,  "Meritum DS/DD 360KB" },
    { 80, 18, 2, 256, 737280,  "Meritum DS/DD 80T 720KB" },
    { 35, 10, 1, 256, 89600,   "TNS SS/SD 87KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_meritum_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_meritum_geom[i].name; i++) {
        if (size == g_meritum_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for TRSDOS-like boot sector */
            if (data[0] == 0x00 || data[0] == 0xFE || data[0] == 0x00) {
                confidence += 15;
            }
            
            /* Check directory area for TRSDOS patterns */
            if (size >= 256 * 3) {
                int valid = 0;
                for (int j = 0; j < 8; j++) {
                    if (data[256 * 2 + j * 32] == 0xFF || 
                        data[256 * 2 + j * 32] == 0x00 ||
                        (data[256 * 2 + j * 32] >= 0x20 && data[256 * 2 + j * 32] <= 0x7E)) {
                        valid++;
                    }
                }
                if (valid >= 6) confidence += 20;
            }
            
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_meritum_read(const char *path, uft_meritum_image_t **image) {
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
    
    uft_meritum_image_t *img = calloc(1, sizeof(uft_meritum_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_meritum_geom[i].name; i++) {
        if (size == g_meritum_geom[i].total_size) {
            img->tracks = g_meritum_geom[i].tracks;
            img->sectors = g_meritum_geom[i].sectors;
            img->heads = g_meritum_geom[i].heads;
            img->sector_size = g_meritum_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_meritum_free(uft_meritum_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_meritum_get_info(uft_meritum_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Meritum/TNS Disk Image (Poland/Czechoslovakia - TRS-80 Clone)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

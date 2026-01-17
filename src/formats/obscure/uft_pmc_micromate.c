/**
 * @file uft_pmc_micromate.c
 * @brief PMC MicroMate CP/M Disk Format Support
 * @version 4.1.3
 * 
 * PMC MicroMate - CP/M workstation (1983)
 * Z80A processor, 128K RAM, integrated 5.25" drive
 * "Terminal Expander" for TRS-80 Model 100
 * 
 * Native formats:
 * - Type A: DS/DD, 40 tracks, 9 sectors, 512 bytes = 360 KB
 * - Type B: DS/DD, 80 tracks, 9 sectors, 512 bytes = 720 KB
 * 
 * FC5025 compatible format
 */

#include "uft/formats/uft_pmc_micromate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_pmc_geom[] = {
    { 40, 9, 2, 512, 368640,  "PMC Type A DS/DD 360KB" },
    { 80, 9, 2, 512, 737280,  "PMC Type B DS/DD 720KB" },
    { 40, 9, 1, 512, 184320,  "PMC SS/DD 180KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_pmc_probe(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0;
    
    for (int i = 0; g_pmc_geom[i].name; i++) {
        if (size == g_pmc_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for CP/M directory patterns (0xE5 fill) */
            if (size >= 512 * 9) {
                int e5_count = 0;
                for (int j = 0; j < 512; j++) {
                    if (data[512 * 2 + j] == 0xE5) e5_count++;
                }
                if (e5_count > 400) confidence += 25;
            }
            
            /* Check for valid CP/M boot sector */
            if (data[0] == 0xC3 || data[0] == 0x00) confidence += 10;
            
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_pmc_read(const char *path, uft_pmc_image_t **image) {
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
    
    uft_pmc_image_t *img = calloc(1, sizeof(uft_pmc_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_pmc_geom[i].name; i++) {
        if (size == g_pmc_geom[i].total_size) {
            img->tracks = g_pmc_geom[i].tracks;
            img->sectors = g_pmc_geom[i].sectors;
            img->heads = g_pmc_geom[i].heads;
            img->sector_size = g_pmc_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_pmc_free(uft_pmc_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_pmc_get_info(uft_pmc_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "PMC MicroMate CP/M Disk Image\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Total Size: %zu KB\n",
        img->tracks, img->sectors, img->heads, img->size / 1024);
    return UFT_OK;
}

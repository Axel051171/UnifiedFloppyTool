/**
 * @file uft_dg_nova.c
 * @brief Data General Nova/Eclipse Disk Format Support
 * @version 4.1.4
 * 
 * Data General Nova/Eclipse - 16-bit Minicomputers (1969-1980s)
 * RDOS, AOS, AOS/VS operating systems
 * 
 * Floppy formats (8" and 5.25"):
 * - 8" SS/SD: 77 tracks, 26 sectors, 128 bytes = 250 KB
 * - 8" DS/DD: 77 tracks, 26 sectors, 256 bytes, DS = 1 MB
 * - 5.25" DS/DD: 80 tracks, 9 sectors, 512 bytes = 720 KB
 */

#include "uft/formats/uft_dg_nova.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_dg_nova_geom[] = {
    { 77, 26, 1, 128, 256256,   "DG Nova 8\" SS/SD 250KB" },
    { 77, 26, 2, 128, 512512,   "DG Nova 8\" DS/SD 500KB" },
    { 77, 26, 2, 256, 1025024,  "DG Eclipse 8\" DS/DD 1MB" },
    { 80, 9, 2, 512, 737280,    "DG MV 5.25\" DS/DD 720KB" },
    { 80, 15, 2, 512, 1228800,  "DG MV 5.25\" DS/HD 1.2MB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_dg_nova_probe(const uint8_t *data, size_t size) {
    if (!data || size < 128) return 0;
    
    for (int i = 0; g_dg_nova_geom[i].name; i++) {
        if (size == g_dg_nova_geom[i].total_size) {
            int confidence = 30;
            
            /* DG uses big-endian, check for patterns */
            /* Boot block often starts with specific jump vectors */
            if (size > 4) {
                /* Nova uses 16-bit words, check for valid instruction patterns */
                uint16_t word = (data[0] << 8) | data[1];
                if ((word & 0xE000) == 0x0000 ||  /* Memory reference */
                    (word & 0xE000) == 0x2000) {  /* I/O instruction */
                    confidence += 15;
                }
            }
            
            /* Check for RDOS volume label patterns */
            int printable = 0;
            for (int j = 0; j < 64 && j < (int)size; j++) {
                if ((data[j] >= 0x20 && data[j] <= 0x7E) || data[j] == 0) {
                    printable++;
                }
            }
            if (printable > 40) confidence += 15;
            
            return confidence > 40 ? confidence : 0;
        }
    }
    return 0;
}

int uft_dg_nova_read(const char *path, uft_dg_nova_image_t **image) {
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
    
    uft_dg_nova_image_t *img = calloc(1, sizeof(uft_dg_nova_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_dg_nova_geom[i].name; i++) {
        if (size == g_dg_nova_geom[i].total_size) {
            img->tracks = g_dg_nova_geom[i].tracks;
            img->sectors = g_dg_nova_geom[i].sectors;
            img->heads = g_dg_nova_geom[i].heads;
            img->sector_size = g_dg_nova_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_dg_nova_free(uft_dg_nova_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_dg_nova_get_info(uft_dg_nova_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Data General Nova/Eclipse Disk Image\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n"
        "Operating System: RDOS / AOS\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

/**
 * @file uft_versados.c
 * @brief Motorola VersaDOS Disk Format Support
 * @version 4.1.3
 * 
 * VersaDOS - Motorola 68000 Real-Time OS (1980s)
 * Used on EXORmacs development systems
 * 
 * Disk formats (5.25" DD):
 * - Standard: 77 tracks, 26 sectors, 128 bytes = 256 KB
 * - Extended: 77 tracks, 26 sectors, 256 bytes = 512 KB
 * - DS: 77 tracks, 26 sectors, 256 bytes, DS = 1 MB
 * 
 * FC5025 compatible format
 */

#include "uft/formats/uft_versados.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Constants */
#define VERSADOS_TRACKS         77
#define VERSADOS_SECTORS        26
#define VERSADOS_SECTOR_128     128
#define VERSADOS_SECTOR_256     256

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_versados_geom[] = {
    { 77, 26, 1, 128, 256256,  "VersaDOS SS 128b 250KB" },
    { 77, 26, 1, 256, 512512,  "VersaDOS SS 256b 500KB" },
    { 77, 26, 2, 256, 1025024, "VersaDOS DS 256b 1MB" },
    { 80, 16, 2, 256, 655360,  "VersaDOS 80T 640KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_versados_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_versados_geom[i].name; i++) {
        if (size == g_versados_geom[i].total_size) {
            /* Check for VersaDOS volume label patterns */
            /* First sector typically contains boot code or volume info */
            int confidence = 40;
            
            /* Check for 68000 code patterns (MOVE, JMP, etc) */
            if (size > 4) {
                uint16_t word1 = (data[0] << 8) | data[1];
                if ((word1 & 0xF000) == 0x4000 ||  /* LEA, CHK, etc */
                    (word1 & 0xF000) == 0x6000) {  /* Bcc */
                    confidence += 15;
                }
            }
            
            /* Check for printable volume name */
            int printable = 0;
            for (int j = 0; j < 16 && j < (int)size; j++) {
                if ((data[j] >= 0x20 && data[j] <= 0x7E) || data[j] == 0) {
                    printable++;
                }
            }
            if (printable > 10) confidence += 15;
            
            return confidence > 50 ? confidence : 0;
        }
    }
    return 0;
}

int uft_versados_read(const char *path, uft_versados_image_t **image) {
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
    
    uft_versados_image_t *img = calloc(1, sizeof(uft_versados_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    /* Find matching geometry */
    for (int i = 0; g_versados_geom[i].name; i++) {
        if (size == g_versados_geom[i].total_size) {
            img->tracks = g_versados_geom[i].tracks;
            img->sectors = g_versados_geom[i].sectors;
            img->heads = g_versados_geom[i].heads;
            img->sector_size = g_versados_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_versados_free(uft_versados_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_versados_get_info(uft_versados_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    
    snprintf(buf, buf_size,
        "Motorola VersaDOS Disk Image\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\n"
        "Total Size: %zu bytes (%zu KB)\n",
        img->tracks, img->sectors, img->heads,
        img->sector_size, img->size, img->size / 1024);
    return UFT_OK;
}

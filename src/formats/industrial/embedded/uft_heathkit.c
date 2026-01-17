/**
 * @file uft_heathkit.c
 * @brief Heathkit H8/H89 Disk Format Support
 * @version 4.1.4
 * 
 * Heathkit H8/H89/H88 - Personal computers (1977-1985)
 * Z80 CPU at 2 MHz, HDOS / CP/M operating systems
 * Uses HARD-SECTORED disks (10 sectors, index hole per sector)
 * 
 * Disk formats (5.25" hard-sectored):
 * - H17 SS: 40 tracks, 10 sectors, 256 bytes = 100 KB
 * - H17 DS: 40 tracks, 10 sectors, 256 bytes, DS = 200 KB
 * - H37 SS: 40 tracks, 10 sectors, 512 bytes = 200 KB
 * - H37 DS: 80 tracks, 10 sectors, 512 bytes, DS = 800 KB
 * - Soft-sectored CP/M: 77 tracks, 26 sectors, 128 bytes
 */

#include "uft/formats/uft_heathkit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
    int hard_sectored;
} g_heathkit_geom[] = {
    { 40, 10, 1, 256, 102400,  "Heathkit H17 SS 100KB", 1 },
    { 40, 10, 2, 256, 204800,  "Heathkit H17 DS 200KB", 1 },
    { 40, 10, 1, 512, 204800,  "Heathkit H37 SS 200KB", 1 },
    { 40, 10, 2, 512, 409600,  "Heathkit H37 DS 400KB", 1 },
    { 80, 10, 2, 512, 819200,  "Heathkit H37 DS 800KB", 1 },
    { 77, 26, 1, 128, 256256,  "Heathkit CP/M 8\" 250KB", 0 },
    { 40, 18, 1, 256, 184320,  "Heathkit soft-sector 180KB", 0 },
    { 0, 0, 0, 0, 0, NULL, 0 }
};

int uft_heathkit_probe(const uint8_t *data, size_t size) {
    if (!data || size < 256) return 0;
    
    for (int i = 0; g_heathkit_geom[i].name; i++) {
        if (size == g_heathkit_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for HDOS volume label (starts with ASCII) */
            int printable = 0;
            for (int j = 0; j < 64 && j < (int)size; j++) {
                if ((data[j] >= 0x20 && data[j] <= 0x7E) || data[j] == 0) {
                    printable++;
                }
            }
            if (printable > 40) confidence += 15;
            
            /* Hard-sectored disks have specific sector interleave */
            if (g_heathkit_geom[i].hard_sectored) {
                /* Check for HDOS signatures */
                if (data[0] == 0xAF || data[0] == 0x00) confidence += 10;
            }
            
            return confidence > 45 ? confidence : 0;
        }
    }
    return 0;
}

int uft_heathkit_read(const char *path, uft_heathkit_image_t **image) {
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
    
    uft_heathkit_image_t *img = calloc(1, sizeof(uft_heathkit_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_heathkit_geom[i].name; i++) {
        if (size == g_heathkit_geom[i].total_size) {
            img->tracks = g_heathkit_geom[i].tracks;
            img->sectors = g_heathkit_geom[i].sectors;
            img->heads = g_heathkit_geom[i].heads;
            img->sector_size = g_heathkit_geom[i].sector_size;
            img->hard_sectored = g_heathkit_geom[i].hard_sectored;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_heathkit_free(uft_heathkit_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_heathkit_get_info(uft_heathkit_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Heathkit H8/H89 Disk Image\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n"
        "Type: %s\n"
        "Operating System: HDOS / CP/M\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024,
        img->hard_sectored ? "Hard-Sectored" : "Soft-Sectored");
    return UFT_OK;
}

/**
 * @file uft_sanyo_mbc.c
 * @brief Sanyo MBC-55x Disk Format Support (Japan)
 * @version 4.1.4
 * 
 * Sanyo MBC-550/555/560 - MS-DOS Compatible (1982-1986)
 * 8088 CPU at 3.58 MHz
 * Sanyo MS-DOS (modified), CP/M-86
 * Also sold as Kaypro 2200 in USA
 * 
 * Disk formats (5.25"):
 * - SS/DD: 40 tracks, 9 sectors, 512 bytes = 180 KB
 * - DS/DD: 40 tracks, 9 sectors, 512 bytes, DS = 360 KB
 * - DS/DD: 80 tracks, 9 sectors, 512 bytes, DS = 720 KB
 */

#include "uft/formats/uft_sanyo_mbc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_sanyo_mbc_geom[] = {
    { 40, 9, 1, 512, 184320,   "Sanyo MBC-550 SS/DD 180KB" },
    { 40, 9, 2, 512, 368640,   "Sanyo MBC-555 DS/DD 360KB" },
    { 80, 9, 2, 512, 737280,   "Sanyo MBC-560 DS/DD 720KB" },
    { 40, 8, 1, 512, 163840,   "Sanyo MBC SS 160KB" },
    { 40, 8, 2, 512, 327680,   "Sanyo MBC DS 320KB" },
    { 0, 0, 0, 0, 0, NULL }
};

int uft_sanyo_mbc_probe(const uint8_t *data, size_t size) {
    if (!data || size < 512) return 0;
    
    for (int i = 0; g_sanyo_mbc_geom[i].name; i++) {
        if (size == g_sanyo_mbc_geom[i].total_size) {
            int confidence = 35;
            
            /* Check for MS-DOS boot sector */
            if (data[0] == 0xEB || data[0] == 0xE9) {
                confidence += 15;
            }
            
            /* Check for BPB at offset 11 */
            if (size >= 30) {
                uint16_t bytes_per_sector = data[11] | (data[12] << 8);
                if (bytes_per_sector == 512) confidence += 15;
                
                /* Sanyo-specific media descriptor */
                if (data[21] == 0xFE || data[21] == 0xFC || 
                    data[21] == 0xFF || data[21] == 0xFD) {
                    confidence += 10;
                }
            }
            
            return confidence > 50 ? confidence : 0;
        }
    }
    return 0;
}

int uft_sanyo_mbc_read(const char *path, uft_sanyo_mbc_image_t **image) {
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
    
    uft_sanyo_mbc_image_t *img = calloc(1, sizeof(uft_sanyo_mbc_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    for (int i = 0; g_sanyo_mbc_geom[i].name; i++) {
        if (size == g_sanyo_mbc_geom[i].total_size) {
            img->tracks = g_sanyo_mbc_geom[i].tracks;
            img->sectors = g_sanyo_mbc_geom[i].sectors;
            img->heads = g_sanyo_mbc_geom[i].heads;
            img->sector_size = g_sanyo_mbc_geom[i].sector_size;
            break;
        }
    }
    
    img->data = data;
    img->size = size;
    *image = img;
    return UFT_OK;
}

void uft_sanyo_mbc_free(uft_sanyo_mbc_image_t *image) {
    if (image) { free(image->data); free(image); }
}

int uft_sanyo_mbc_get_info(uft_sanyo_mbc_image_t *img, char *buf, size_t buf_size) {
    if (!img || !buf) return UFT_ERR_INVALID_PARAM;
    snprintf(buf, buf_size,
        "Sanyo MBC-55x Disk Image (Japan/USA as Kaypro 2200)\n"
        "Geometry: %d tracks x %d sectors x %d heads\n"
        "Sector Size: %d bytes\nTotal Size: %zu KB\n"
        "Operating System: Sanyo MS-DOS / CP/M-86\n",
        img->tracks, img->sectors, img->heads, img->sector_size, img->size / 1024);
    return UFT_OK;
}

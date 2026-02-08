/**
 * @file uft_abc800.c
 * @brief Luxor ABC 80/800 Disk Format Support
 * @version 4.1.2
 * 
 * Luxor ABC 80/800 - Swedish home/office computers (1978-1985)
 * 
 * Disk formats:
 * - SS/SD: 40 tracks, 16 sectors, 256 bytes = 160 KB
 * - SS/DD: 40 tracks, 16 sectors, 256 bytes = 160 KB (FM encoding)
 * - SS/DD: 80 tracks, 16 sectors, 256 bytes = 320 KB
 * - DS/DD: 80 tracks, 16 sectors, 256 bytes = 640 KB
 * 
 * Filesystems: ABC-DOS, UFD-DOS
 * 
 * Reference: Luxor ABC 800 Technical Manual
 */

#include "uft/formats/uft_abc800.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Constants */
#define ABC_SECTOR_SIZE     256
#define ABC_SECTORS_PER_TRACK 16

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_abc_geom[] = {
    { 40, 16, 1, 256, 163840,  "ABC SS/SD 160KB" },
    { 80, 16, 1, 256, 327680,  "ABC SS/DD 320KB" },
    { 80, 16, 2, 256, 655360,  "ABC DS/DD 640KB" },
    { 77, 16, 2, 256, 630784,  "ABC DS/DD 8\" 616KB" },
    { 0, 0, 0, 0, 0, NULL }
};

/* ABC-DOS directory entry structure */
#define ABC_DIR_ENTRY_SIZE  32
#define ABC_DIR_NAME_LEN    8
#define ABC_DIR_TYPE_LEN    3

static int abc_calc_offset(int track, int head, int sector, int heads) {
    return ((track * heads + head) * ABC_SECTORS_PER_TRACK + sector) * ABC_SECTOR_SIZE;
}

int uft_abc800_probe(const uint8_t *data, size_t size, int *out_tracks,
                     int *out_heads, const char **out_name) {
    if (!data || size < ABC_SECTOR_SIZE * 16) return 0;
    
    for (int i = 0; g_abc_geom[i].name; i++) {
        if (size != g_abc_geom[i].total_size) continue;
        
        /* Check for ABC-DOS signature on track 0 */
        /* ABC-DOS boot sector typically has specific patterns */
        int confidence = 40;  /* Base confidence for size match */
        
        /* Check first sector for boot code patterns */
        if (data[0] == 0xC3 || data[0] == 0x00) {
            confidence += 15;  /* Z80 JP instruction or empty */
        }
        
        /* Check for printable characters in directory area */
        size_t dir_offset = ABC_SECTOR_SIZE * 2;  /* Directory often at sector 2 */
        if (dir_offset < size) {
            int printable = 0;
            for (int j = 0; j < 32 && j + dir_offset < size; j++) {
                uint8_t c = data[dir_offset + j];
                if ((c >= 0x20 && c <= 0x7E) || c == 0x00) printable++;
            }
            if (printable > 20) confidence += 20;
        }
        
        if (confidence > 50) {
            if (out_tracks) *out_tracks = g_abc_geom[i].tracks;
            if (out_heads) *out_heads = g_abc_geom[i].heads;
            if (out_name) *out_name = g_abc_geom[i].name;
            return confidence;
        }
    }
    return 0;
}

int uft_abc800_read(const char *path, uft_abc800_image_t **image) {
    if (!path || !image) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return UFT_ERR_IO; }
    size_t size = (size_t)sz;
    if (fseek(f, 0, SEEK_SET) != 0) return UFT_ERR_IO;
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }
    
    if (fread(data, 1, size, f) != size) {
        free(data); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);
    
    int tracks, heads;
    const char *name;
    if (uft_abc800_probe(data, size, &tracks, &heads, &name) < 40) {
        free(data); return UFT_ERR_UNKNOWN_FORMAT;
    }
    
    uft_abc800_image_t *img = calloc(1, sizeof(uft_abc800_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    img->data = data;
    img->size = size;
    img->tracks = tracks;
    img->sectors = ABC_SECTORS_PER_TRACK;
    img->heads = heads;
    img->sector_size = ABC_SECTOR_SIZE;
    
    *image = img;
    return UFT_OK;
}

void uft_abc800_free(uft_abc800_image_t *image) {
    if (image) {
        free(image->data);
        free(image);
    }
}

int uft_abc800_read_sector(uft_abc800_image_t *image, int track, int head,
                           int sector, uint8_t *buffer) {
    if (!image || !buffer) return UFT_ERR_INVALID_PARAM;
    
    if (track < 0 || track >= image->tracks ||
        head < 0 || head >= image->heads ||
        sector < 0 || sector >= image->sectors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t offset = (size_t)abc_calc_offset(track, head, sector, image->heads);
    if (offset + ABC_SECTOR_SIZE > image->size) return UFT_ERR_INCOMPLETE;
    
    memcpy(buffer, image->data + offset, ABC_SECTOR_SIZE);
    return UFT_OK;
}

int uft_abc800_write_sector(uft_abc800_image_t *image, int track, int head,
                            int sector, const uint8_t *buffer) {
    if (!image || !buffer) return UFT_ERR_INVALID_PARAM;
    
    if (track < 0 || track >= image->tracks ||
        head < 0 || head >= image->heads ||
        sector < 0 || sector >= image->sectors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t offset = (size_t)abc_calc_offset(track, head, sector, image->heads);
    if (offset + ABC_SECTOR_SIZE > image->size) return UFT_ERR_INCOMPLETE;
    
    memcpy(image->data + offset, buffer, ABC_SECTOR_SIZE);
    return UFT_OK;
}

int uft_abc800_get_info(uft_abc800_image_t *image, char *buf, size_t buf_size) {
    if (!image || !buf) return UFT_ERR_INVALID_PARAM;
    
    const char *geom_name = "Unknown";
    for (int i = 0; g_abc_geom[i].name; i++) {
        if (image->tracks == g_abc_geom[i].tracks &&
            image->heads == g_abc_geom[i].heads) {
            geom_name = g_abc_geom[i].name;
            break;
        }
    }
    
    snprintf(buf, buf_size,
             "Luxor ABC 80/800 Disk Image\n"
             "Format: %s\n"
             "Geometry: %d tracks x %d sectors x %d sides\n"
             "Sector Size: %d bytes\n"
             "Total Size: %zu bytes (%zu KB)\n",
             geom_name,
             image->tracks, image->sectors, image->heads,
             image->sector_size,
             image->size, image->size / 1024);
    
    return UFT_OK;
}

int uft_abc800_create(const char *path, int tracks, int heads) {
    if (!path) return UFT_ERR_INVALID_PARAM;
    
    if (tracks <= 0) tracks = 80;
    if (heads <= 0) heads = 2;
    
    size_t size = (size_t)tracks * ABC_SECTORS_PER_TRACK * heads * ABC_SECTOR_SIZE;
    uint8_t *data = calloc(1, size);
    if (!data) return UFT_ERR_MEMORY;
    
    /* Initialize with 0xE5 (standard CP/M/DOS empty pattern) */
    memset(data, 0xE5, size);
    
    /* Clear boot sector */
    memset(data, 0x00, ABC_SECTOR_SIZE);
    
    FILE *f = fopen(path, "wb");
    if (!f) { free(data); return UFT_ERR_IO; }
    fwrite(data, 1, size, f);
    fclose(f); free(data);
    return UFT_OK;
}

int uft_abc800_write(uft_abc800_image_t *image, const char *path) {
    if (!image || !path) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    size_t written = fwrite(image->data, 1, image->size, f);
    fclose(f);
    
    return (written == image->size) ? UFT_OK : UFT_ERR_IO;
}

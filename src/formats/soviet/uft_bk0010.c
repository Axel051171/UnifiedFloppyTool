/**
 * @file uft_bk0010.c
 * @brief Elektronika BK-0010/0011 Disk Format Support
 * @version 4.1.2
 * 
 * BK-0010/0011 - Soviet 16-bit PDP-11 compatible home computers (1985-1990s)
 * 
 * Disk formats:
 * - Standard: 80 tracks, 10 sectors, 512 bytes = 800 KB (DS/DD)
 * - RT-11 compatible format
 * - ANDOS, MK-DOS, CSI-DOS compatible
 * 
 * The BK series used the K1801VM1 CPU (LSI-11 compatible)
 * and could run RT-11 class operating systems.
 * 
 * Reference: BK-0010 Technical Documentation
 */

#include "uft/formats/uft_bk0010.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Constants */
#define BK_SECTOR_SIZE      512
#define BK_TRACKS           80
#define BK_SECTORS          10
#define BK_HEADS            2
#define BK_IMAGE_SIZE       (BK_TRACKS * BK_SECTORS * BK_HEADS * BK_SECTOR_SIZE)

/* RT-11 home block location */
#define RT11_HOME_BLOCK     1
#define RT11_PACK_CLUSTER   0x1C0  /* Offset in home block */

/* Standard geometries */
static const struct {
    int tracks, sectors, heads, sector_size;
    size_t total_size;
    const char *name;
} g_bk_geom[] = {
    { 80, 10, 2, 512, 819200, "BK DS/DD 800KB" },
    { 80, 10, 1, 512, 409600, "BK SS/DD 400KB" },
    { 40, 10, 2, 512, 409600, "BK DS/DD 40T 400KB" },
    { 0, 0, 0, 0, 0, NULL }
};

static int bk_calc_offset(int track, int head, int sector, int heads) {
    /* Sectors are 1-based in RT-11 */
    return ((track * heads + head) * BK_SECTORS + (sector - 1)) * BK_SECTOR_SIZE;
}

int uft_bk0010_probe(const uint8_t *data, size_t size, int *out_tracks,
                     int *out_heads, bk_dos_type_t *out_dos) {
    if (!data || size < BK_SECTOR_SIZE * 10) return 0;
    
    int confidence = 0;
    int tracks = 0, heads = 0;
    bk_dos_type_t dos = BK_DOS_UNKNOWN;
    
    /* Check size */
    for (int i = 0; g_bk_geom[i].name; i++) {
        if (size == g_bk_geom[i].total_size) {
            tracks = g_bk_geom[i].tracks;
            heads = g_bk_geom[i].heads;
            confidence = 35;
            break;
        }
    }
    
    if (confidence == 0) return 0;
    
    /* Check RT-11 home block (block 1) */
    if (size >= BK_SECTOR_SIZE * 2) {
        const uint8_t *home = data + BK_SECTOR_SIZE;  /* Block 1 */
        
        /* RT-11 home block checks */
        /* Pack cluster size at offset 0x1C0 should be reasonable (1-16) */
        uint16_t pack_cluster = home[0x1C0] | (home[0x1C1] << 8);
        if (pack_cluster >= 1 && pack_cluster <= 16) {
            confidence += 20;
            dos = BK_DOS_RT11;
        }
        
        /* Check for volume ID at offset 0x1F0 */
        int valid_id = 1;
        for (int j = 0; j < 12; j++) {
            uint8_t c = home[0x1F0 + j];
            if (c != 0 && c != ' ' && (c < 0x20 || c > 0x7E)) {
                valid_id = 0;
                break;
            }
        }
        if (valid_id) confidence += 10;
    }
    
    /* Check boot sector for known patterns */
    /* PDP-11 instructions often start with MOV, JMP, etc. */
    uint16_t first_word = data[0] | (data[1] << 8);
    if ((first_word & 0xF000) == 0x1000 ||  /* MOV */
        (first_word & 0xFF00) == 0x0000 ||  /* HALT/WAIT */
        first_word == 0x0240) {             /* CLR R0 */
        confidence += 10;
    }
    
    /* Check for non-empty data */
    int non_zero = 0;
    for (int i = 0; i < 512 && i < (int)size; i++) {
        if (data[i] != 0x00 && data[i] != 0xFF) non_zero++;
    }
    if (non_zero > 50) confidence += 10;
    
    if (confidence > 90) confidence = 90;
    
    if (out_tracks) *out_tracks = tracks;
    if (out_heads) *out_heads = heads;
    if (out_dos) *out_dos = dos;
    
    return confidence;
}

int uft_bk0010_read(const char *path, uft_bk0010_image_t **image) {
    if (!path || !image) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0) { fclose(f); return UFT_ERR_IO; }
    size_t size = (size_t)sz;
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }
    
    if (fread(data, 1, size, f) != size) {
        free(data); fclose(f); return UFT_ERR_IO;
    }
    fclose(f);
    
    int tracks, heads;
    bk_dos_type_t dos;
    if (uft_bk0010_probe(data, size, &tracks, &heads, &dos) < 35) {
        free(data); return UFT_ERR_UNKNOWN_FORMAT;
    }
    
    uft_bk0010_image_t *img = calloc(1, sizeof(uft_bk0010_image_t));
    if (!img) { free(data); return UFT_ERR_MEMORY; }
    
    img->data = data;
    img->size = size;
    img->tracks = tracks;
    img->sectors = BK_SECTORS;
    img->heads = heads;
    img->sector_size = BK_SECTOR_SIZE;
    img->dos_type = dos;
    
    *image = img;
    return UFT_OK;
}

void uft_bk0010_free(uft_bk0010_image_t *image) {
    if (image) {
        free(image->data);
        free(image);
    }
}

int uft_bk0010_read_sector(uft_bk0010_image_t *image, int track, int head,
                           int sector, uint8_t *buffer) {
    if (!image || !buffer) return UFT_ERR_INVALID_PARAM;
    
    if (track < 0 || track >= image->tracks ||
        head < 0 || head >= image->heads ||
        sector < 1 || sector > image->sectors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t offset = (size_t)bk_calc_offset(track, head, sector, image->heads);
    if (offset + BK_SECTOR_SIZE > image->size) return UFT_ERR_INCOMPLETE;
    
    memcpy(buffer, image->data + offset, BK_SECTOR_SIZE);
    return UFT_OK;
}

int uft_bk0010_write_sector(uft_bk0010_image_t *image, int track, int head,
                            int sector, const uint8_t *buffer) {
    if (!image || !buffer) return UFT_ERR_INVALID_PARAM;
    
    if (track < 0 || track >= image->tracks ||
        head < 0 || head >= image->heads ||
        sector < 1 || sector > image->sectors) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t offset = (size_t)bk_calc_offset(track, head, sector, image->heads);
    if (offset + BK_SECTOR_SIZE > image->size) return UFT_ERR_INCOMPLETE;
    
    memcpy(image->data + offset, buffer, BK_SECTOR_SIZE);
    return UFT_OK;
}

int uft_bk0010_get_info(uft_bk0010_image_t *image, char *buf, size_t buf_size) {
    if (!image || !buf) return UFT_ERR_INVALID_PARAM;
    
    const char *dos_name;
    switch (image->dos_type) {
        case BK_DOS_RT11:  dos_name = "RT-11"; break;
        case BK_DOS_ANDOS: dos_name = "ANDOS"; break;
        case BK_DOS_MKDOS: dos_name = "MK-DOS"; break;
        case BK_DOS_CSIDOS: dos_name = "CSI-DOS"; break;
        default:          dos_name = "Unknown"; break;
    }
    
    snprintf(buf, buf_size,
             "Elektronika BK-0010/0011 Disk Image\n"
             "Geometry: %d tracks x %d sectors x %d sides\n"
             "Sector Size: %d bytes\n"
             "Total Size: %zu bytes (%zu KB)\n"
             "DOS Type: %s\n",
             image->tracks, image->sectors, image->heads,
             image->sector_size,
             image->size, image->size / 1024,
             dos_name);
    
    return UFT_OK;
}

int uft_bk0010_create(const char *path, int tracks, int heads, int init_rt11) {
    if (!path) return UFT_ERR_INVALID_PARAM;
    
    if (tracks <= 0) tracks = 80;
    if (heads <= 0) heads = 2;
    
    size_t size = (size_t)tracks * BK_SECTORS * heads * BK_SECTOR_SIZE;
    uint8_t *data = calloc(1, size);
    if (!data) return UFT_ERR_MEMORY;
    
    if (init_rt11 && size >= BK_SECTOR_SIZE * 6) {
        /* Initialize RT-11 home block */
        uint8_t *home = data + BK_SECTOR_SIZE;
        
        /* Pack cluster size */
        home[0x1C0] = 1;
        home[0x1C1] = 0;
        
        /* First directory segment */
        home[0x1C4] = 6;  /* Directory starts at block 6 */
        home[0x1C5] = 0;
        
        /* System version (V05) */
        home[0x1F6] = 5;
        
        /* Volume ID */
        memcpy(home + 0x1F8, "BK      ", 8);
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) { free(data); return UFT_ERR_IO; }
    fwrite(data, 1, size, f);
    fclose(f); free(data);
    return UFT_OK;
}

int uft_bk0010_write(uft_bk0010_image_t *image, const char *path) {
    if (!image || !path) return UFT_ERR_INVALID_PARAM;
    
    FILE *f = fopen(path, "wb");
    if (!f) return UFT_ERR_IO;
    
    size_t written = fwrite(image->data, 1, image->size, f);
    fclose(f);
    
    return (written == image->size) ? UFT_OK : UFT_ERR_IO;
}

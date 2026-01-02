/**
 * @file d71_writer.c
 * @brief D71 Image Writer for Commodore 1571
 * 
 * D71 is essentially two D64 images concatenated (double-sided).
 * Side 0: Tracks 1-35, Side 1: Tracks 36-70
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define D71_SIZE        349696   /* 35 tracks × 2 sides */
#define D71_SIZE_ERRORS 351062   /* With error info */
#define D64_SIDE_SIZE   174848   /* Single side (35 tracks) */

/* Sectors per track */
static const uint8_t SECTORS_PER_TRACK[] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

typedef struct {
    uint8_t *data;
    size_t   size;
    bool     has_errors;
    uint8_t *errors;
} d71_image_t;

/**
 * @brief Create empty D71 image
 */
int d71_create(d71_image_t *img, bool with_errors)
{
    if (!img) return -1;
    
    img->size = with_errors ? D71_SIZE_ERRORS : D71_SIZE;
    img->data = calloc(1, img->size);
    img->has_errors = with_errors;
    
    if (!img->data) return -1;
    
    if (with_errors) {
        img->errors = img->data + D71_SIZE;
        memset(img->errors, 0x01, 1366);  /* No errors */
    }
    
    return 0;
}

/**
 * @brief Get sector offset in D71 image
 */
static size_t d71_sector_offset(int track, int sector)
{
    size_t offset = 0;
    int t = track;
    
    /* Handle side 1 (tracks 36-70) */
    if (track > 35) {
        offset = D64_SIDE_SIZE;
        t = track - 35;
    }
    
    /* Calculate offset within side */
    for (int i = 1; i < t; i++) {
        offset += SECTORS_PER_TRACK[i - 1] * 256;
    }
    offset += sector * 256;
    
    return offset;
}

/**
 * @brief Write sector to D71 image
 */
int d71_write_sector(d71_image_t *img, int track, int sector,
                     const uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (track < 1 || track > 70) return -1;
    
    int t = (track > 35) ? track - 35 : track;
    if (sector < 0 || sector >= SECTORS_PER_TRACK[t - 1]) return -1;
    
    size_t offset = d71_sector_offset(track, sector);
    memcpy(img->data + offset, data, 256);
    
    return 0;
}

/**
 * @brief Read sector from D71 image
 */
int d71_read_sector(const d71_image_t *img, int track, int sector,
                    uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (track < 1 || track > 70) return -1;
    
    int t = (track > 35) ? track - 35 : track;
    if (sector < 0 || sector >= SECTORS_PER_TRACK[t - 1]) return -1;
    
    size_t offset = d71_sector_offset(track, sector);
    memcpy(data, img->data + offset, 256);
    
    return 0;
}

/**
 * @brief Format D71 with empty directory
 */
int d71_format(d71_image_t *img, const char *disk_name, const char *disk_id)
{
    if (!img || !img->data) return -1;
    
    /* Clear image */
    memset(img->data, 0, D71_SIZE);
    
    /* BAM (Block Allocation Map) - Track 18, Sector 0 */
    uint8_t bam[256] = {0};
    
    /* Directory track/sector pointer */
    bam[0] = 18;  /* Track */
    bam[1] = 1;   /* Sector */
    bam[2] = 0x41; /* DOS version (A) */
    bam[3] = 0x80; /* Double-sided flag */
    
    /* BAM entries for tracks 1-35 (side 0) */
    size_t bam_pos = 4;
    for (int t = 1; t <= 35; t++) {
        int sectors = SECTORS_PER_TRACK[t - 1];
        bam[bam_pos++] = sectors;  /* Free sectors */
        
        /* Bitmap: all sectors free (except track 18) */
        uint32_t bitmap = (1 << sectors) - 1;
        if (t == 18) bitmap = 0;  /* Directory track */
        
        bam[bam_pos++] = bitmap & 0xFF;
        bam[bam_pos++] = (bitmap >> 8) & 0xFF;
        bam[bam_pos++] = (bitmap >> 16) & 0xFF;
    }
    
    /* Disk name (16 chars, padded with 0xA0) */
    memset(bam + 0x90, 0xA0, 16);
    if (disk_name) {
        size_t len = strlen(disk_name);
        if (len > 16) len = 16;
        memcpy(bam + 0x90, disk_name, len);
    }
    
    /* Disk ID (2 chars) */
    bam[0xA2] = disk_id ? disk_id[0] : '0';
    bam[0xA3] = disk_id ? disk_id[1] : '0';
    bam[0xA4] = 0xA0;
    bam[0xA5] = '2';  /* DOS type */
    bam[0xA6] = 'A';
    
    d71_write_sector(img, 18, 0, bam);
    
    /* First directory sector - Track 18, Sector 1 */
    uint8_t dir[256] = {0};
    dir[0] = 0x00;  /* No next track */
    dir[1] = 0xFF;  /* Last sector marker */
    d71_write_sector(img, 18, 1, dir);
    
    /* Side 1 BAM - Track 53, Sector 0 */
    memset(bam, 0, 256);
    bam_pos = 0;
    for (int t = 36; t <= 70; t++) {
        int sectors = SECTORS_PER_TRACK[(t - 36)];
        bam[bam_pos++] = sectors;
        
        uint32_t bitmap = (1 << sectors) - 1;
        if (t == 53) bitmap = 0;
        
        bam[bam_pos++] = bitmap & 0xFF;
        bam[bam_pos++] = (bitmap >> 8) & 0xFF;
        bam[bam_pos++] = (bitmap >> 16) & 0xFF;
    }
    d71_write_sector(img, 53, 0, bam);
    
    return 0;
}

/**
 * @brief Save D71 image to file
 */
int d71_save(const d71_image_t *img, const char *filename)
{
    if (!img || !img->data || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(img->data, 1, img->size, fp);
    fclose(fp);
    
    return (written == img->size) ? 0 : -1;
}

/**
 * @brief Convert two D64 images to D71
 */
int d71_from_d64_pair(const char *d64_side0, const char *d64_side1,
                      const char *d71_file)
{
    FILE *fp0 = fopen(d64_side0, "rb");
    FILE *fp1 = d64_side1 ? fopen(d64_side1, "rb") : NULL;
    
    if (!fp0) return -1;
    
    d71_image_t img;
    d71_create(&img, false);
    
    /* Read side 0 */
    fread(img.data, 1, D64_SIDE_SIZE, fp0);
    fclose(fp0);
    
    /* Read side 1 if provided */
    if (fp1) {
        fread(img.data + D64_SIDE_SIZE, 1, D64_SIDE_SIZE, fp1);
        fclose(fp1);
    }
    
    int result = d71_save(&img, d71_file);
    free(img.data);
    
    return result;
}

/**
 * @brief Free D71 image
 */
void d71_free(d71_image_t *img)
{
    if (img) {
        free(img->data);
        img->data = NULL;
        img->errors = NULL;
        img->size = 0;
    }
}

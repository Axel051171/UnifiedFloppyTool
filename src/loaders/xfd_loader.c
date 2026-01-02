/**
 * @file xfd_loader.c
 * @brief XFD Image Loader/Writer for Atari 8-Bit
 * 
 * XFD is a simple headerless raw sector format used by XFormer.
 * Just raw sectors concatenated without any header.
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* XFD Sizes (headerless, raw sectors) */
#define XFD_SD_SIZE     92160   /* 720 × 128 = Single Density */
#define XFD_ED_SIZE     133120  /* 1040 × 128 = Enhanced Density */
#define XFD_DD_SIZE     183936  /* 720 × 256 = Double Density (first 3 are 128) */
#define XFD_QD_SIZE     368256  /* 1440 × 256 = Quad Density */

typedef enum {
    XFD_FORMAT_SD,    /* 90K Single Density */
    XFD_FORMAT_ED,    /* 130K Enhanced Density */
    XFD_FORMAT_DD,    /* 180K Double Density */
    XFD_FORMAT_QD,    /* 360K Quad Density */
    XFD_FORMAT_UNKNOWN
} xfd_format_t;

typedef struct {
    uint8_t     *data;
    size_t       size;
    xfd_format_t format;
    int          sector_count;
    int          sector_size;
} xfd_image_t;

/**
 * @brief Detect XFD format from file size
 */
xfd_format_t xfd_detect_format(size_t size)
{
    if (size == XFD_SD_SIZE) return XFD_FORMAT_SD;
    if (size == XFD_ED_SIZE) return XFD_FORMAT_ED;
    if (size == XFD_DD_SIZE) return XFD_FORMAT_DD;
    if (size == XFD_QD_SIZE) return XFD_FORMAT_QD;
    
    /* Try to guess from approximate size */
    if (size <= XFD_SD_SIZE + 1024) return XFD_FORMAT_SD;
    if (size <= XFD_ED_SIZE + 1024) return XFD_FORMAT_ED;
    if (size <= XFD_DD_SIZE + 1024) return XFD_FORMAT_DD;
    
    return XFD_FORMAT_UNKNOWN;
}

/**
 * @brief Create empty XFD image
 */
int xfd_create(xfd_image_t *img, xfd_format_t format)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    img->format = format;
    
    switch (format) {
        case XFD_FORMAT_SD:
            img->sector_count = 720;
            img->sector_size = 128;
            img->size = XFD_SD_SIZE;
            break;
        case XFD_FORMAT_ED:
            img->sector_count = 1040;
            img->sector_size = 128;
            img->size = XFD_ED_SIZE;
            break;
        case XFD_FORMAT_DD:
            img->sector_count = 720;
            img->sector_size = 256;  /* Boot sectors still 128 */
            img->size = XFD_DD_SIZE;
            break;
        case XFD_FORMAT_QD:
            img->sector_count = 1440;
            img->sector_size = 256;
            img->size = XFD_QD_SIZE;
            break;
        default:
            return -1;
    }
    
    img->data = calloc(1, img->size);
    return img->data ? 0 : -1;
}

/**
 * @brief Load XFD file
 */
int xfd_load(const char *filename, xfd_image_t *img)
{
    if (!filename || !img) return -1;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return -1;
    
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    xfd_format_t format = xfd_detect_format(size);
    if (format == XFD_FORMAT_UNKNOWN) {
        fclose(fp);
        return -1;
    }
    
    if (xfd_create(img, format) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Read all data */
    size_t read_size = (size < img->size) ? size : img->size;
    fread(img->data, 1, read_size, fp);
    
    fclose(fp);
    return 0;
}

/**
 * @brief Get sector offset in XFD
 */
static size_t xfd_sector_offset(const xfd_image_t *img, int sector)
{
    if (sector < 1 || sector > img->sector_count) return (size_t)-1;
    
    if (img->format == XFD_FORMAT_DD || img->format == XFD_FORMAT_QD) {
        /* First 3 sectors are 128 bytes */
        if (sector <= 3) {
            return (sector - 1) * 128;
        }
        return 3 * 128 + (sector - 4) * img->sector_size;
    }
    
    /* SD/ED: all sectors same size */
    return (sector - 1) * img->sector_size;
}

/**
 * @brief Read sector from XFD
 */
int xfd_read_sector(const xfd_image_t *img, int sector, uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (sector < 1 || sector > img->sector_count) return -1;
    
    size_t offset = xfd_sector_offset(img, sector);
    int size = (img->format >= XFD_FORMAT_DD && sector > 3) ? 
               img->sector_size : 128;
    
    memcpy(data, img->data + offset, size);
    return 0;
}

/**
 * @brief Write sector to XFD
 */
int xfd_write_sector(xfd_image_t *img, int sector, const uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (sector < 1 || sector > img->sector_count) return -1;
    
    size_t offset = xfd_sector_offset(img, sector);
    int size = (img->format >= XFD_FORMAT_DD && sector > 3) ? 
               img->sector_size : 128;
    
    memcpy(img->data + offset, data, size);
    return 0;
}

/**
 * @brief Save XFD to file
 */
int xfd_save(const xfd_image_t *img, const char *filename)
{
    if (!img || !img->data || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(img->data, 1, img->size, fp);
    fclose(fp);
    
    return (written == img->size) ? 0 : -1;
}

/**
 * @brief Convert ATR to XFD
 */
int xfd_from_atr(const char *atr_file, const char *xfd_file)
{
    FILE *fp = fopen(atr_file, "rb");
    if (!fp) return -1;
    
    /* Read ATR header */
    uint8_t header[16];
    if (fread(header, 1, 16, fp) != 16) {
        fclose(fp);
        return -1;
    }
    
    /* Verify ATR signature */
    if (header[0] != 0x96 || header[1] != 0x02) {
        fclose(fp);
        return -1;
    }
    
    /* Get sector size and determine format */
    int sector_size = header[4] | (header[5] << 8);
    uint32_t paragraphs = header[2] | (header[3] << 8) | (header[6] << 16);
    size_t data_size = paragraphs * 16;
    
    xfd_format_t format;
    if (sector_size == 128) {
        format = (data_size > 100000) ? XFD_FORMAT_ED : XFD_FORMAT_SD;
    } else {
        format = (data_size > 200000) ? XFD_FORMAT_QD : XFD_FORMAT_DD;
    }
    
    xfd_image_t xfd;
    xfd_create(&xfd, format);
    
    /* Read data (skip header) */
    size_t read_size = (data_size < xfd.size) ? data_size : xfd.size;
    fread(xfd.data, 1, read_size, fp);
    fclose(fp);
    
    int result = xfd_save(&xfd, xfd_file);
    xfd_free(&xfd);
    
    return result;
}

/**
 * @brief Convert XFD to ATR
 */
int xfd_to_atr(const char *xfd_file, const char *atr_file)
{
    xfd_image_t xfd;
    if (xfd_load(xfd_file, &xfd) != 0) return -1;
    
    FILE *fp = fopen(atr_file, "wb");
    if (!fp) {
        xfd_free(&xfd);
        return -1;
    }
    
    /* Write ATR header */
    uint8_t header[16] = {0};
    header[0] = 0x96;
    header[1] = 0x02;
    
    uint32_t paragraphs = xfd.size / 16;
    header[2] = paragraphs & 0xFF;
    header[3] = (paragraphs >> 8) & 0xFF;
    header[4] = xfd.sector_size & 0xFF;
    header[5] = (xfd.sector_size >> 8) & 0xFF;
    header[6] = (paragraphs >> 16) & 0xFF;
    
    fwrite(header, 1, 16, fp);
    fwrite(xfd.data, 1, xfd.size, fp);
    
    fclose(fp);
    xfd_free(&xfd);
    
    return 0;
}

/**
 * @brief Free XFD image
 */
void xfd_free(xfd_image_t *img)
{
    if (img) {
        free(img->data);
        img->data = NULL;
        img->size = 0;
    }
}

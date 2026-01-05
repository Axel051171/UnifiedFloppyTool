/**
 * @file atr_writer.c
 * @brief ATR Image Writer for Atari 8-Bit Computers
 * 
 * ATR is the standard disk image format for Atari 8-bit computers.
 * Supports SD (90K), ED (130K), and DD (180K) formats.
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ATR Header */
#define ATR_SIGNATURE   0x0296  /* "NICKATARI" signature bytes */
#define ATR_HEADER_SIZE 16

/* Disk formats */
#define ATR_SD_SECTORS   720    /* Single Density: 90K */
#define ATR_ED_SECTORS   1040   /* Enhanced Density: 130K */
#define ATR_DD_SECTORS   720    /* Double Density: 180K (256 byte sectors) */
#define ATR_QD_SECTORS   1440   /* Quad Density: 360K */

#define ATR_SD_SECTOR_SIZE  128
#define ATR_DD_SECTOR_SIZE  256

#pragma pack(push, 1)
typedef struct {
    uint16_t signature;       /* 0x0296 */
    uint16_t size_paragraphs; /* Size in 16-byte paragraphs (low word) */
    uint16_t sector_size;     /* Sector size (128 or 256) */
    uint8_t  size_high;       /* High byte of size */
    uint8_t  reserved[7];     /* Reserved, set to 0 */
    uint8_t  flags;           /* Flags (typically 0) */
    uint8_t  bad_sectors_lo;  /* First bad sector (low) */
    uint8_t  bad_sectors_hi;  /* First bad sector (high) */
    uint8_t  unused[5];       /* Unused */
} atr_header_t;
#pragma pack(pop)

typedef enum {
    ATR_FORMAT_SD,    /* 90K Single Density */
    ATR_FORMAT_ED,    /* 130K Enhanced Density */
    ATR_FORMAT_DD,    /* 180K Double Density */
    ATR_FORMAT_QD     /* 360K Quad Density */
} atr_format_t;

typedef struct {
    atr_header_t header;
    uint8_t     *data;
    size_t       data_size;
    atr_format_t format;
    int          sector_count;
    int          sector_size;
} atr_image_t;

/**
 * @brief Create ATR image
 */
int atr_create(atr_image_t *img, atr_format_t format)
{
    if (!img) return -1;
    
    memset(img, 0, sizeof(*img));
    img->format = format;
    
    switch (format) {
        case ATR_FORMAT_SD:
            img->sector_count = ATR_SD_SECTORS;
            img->sector_size = ATR_SD_SECTOR_SIZE;
            break;
        case ATR_FORMAT_ED:
            img->sector_count = ATR_ED_SECTORS;
            img->sector_size = ATR_SD_SECTOR_SIZE;
            break;
        case ATR_FORMAT_DD:
            img->sector_count = ATR_DD_SECTORS;
            img->sector_size = ATR_DD_SECTOR_SIZE;
            break;
        case ATR_FORMAT_QD:
            img->sector_count = ATR_QD_SECTORS;
            img->sector_size = ATR_DD_SECTOR_SIZE;
            break;
        default:
            return -1;
    }
    
    /* First 3 sectors are always 128 bytes (boot sectors) */
    img->data_size = 3 * 128 + (img->sector_count - 3) * img->sector_size;
    img->data = calloc(1, img->data_size);
    if (!img->data) return -1;
    
    /* Setup header */
    img->header.signature = ATR_SIGNATURE;
    
    /* Size in paragraphs (16-byte units) */
    uint32_t size_paras = img->data_size / 16;
    img->header.size_paragraphs = size_paras & 0xFFFF;
    img->header.size_high = (size_paras >> 16) & 0xFF;
    img->header.sector_size = img->sector_size;
    
    return 0;
}

/**
 * @brief Get sector offset in ATR data
 */
static size_t atr_sector_offset(const atr_image_t *img, int sector)
{
    if (sector < 1 || sector > img->sector_count) return (size_t)-1;
    
    /* Sectors 1-3 are always 128 bytes */
    if (sector <= 3) {
        return (sector - 1) * 128;
    }
    
    /* Remaining sectors use configured sector size */
    return 3 * 128 + (sector - 4) * img->sector_size;
}

/**
 * @brief Write sector to ATR image
 */
int atr_write_sector(atr_image_t *img, int sector, const uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (sector < 1 || sector > img->sector_count) return -1;
    
    size_t offset = atr_sector_offset(img, sector);
    int size = (sector <= 3) ? 128 : img->sector_size;
    
    memcpy(img->data + offset, data, size);
    return 0;
}

/**
 * @brief Read sector from ATR image
 */
int atr_read_sector(const atr_image_t *img, int sector, uint8_t *data)
{
    if (!img || !img->data || !data) return -1;
    if (sector < 1 || sector > img->sector_count) return -1;
    
    size_t offset = atr_sector_offset(img, sector);
    int size = (sector <= 3) ? 128 : img->sector_size;
    
    memcpy(data, img->data + offset, size);
    return 0;
}

/**
 * @brief Format ATR with Atari DOS 2.5 structure
 */
int atr_format_dos25(atr_image_t *img, const char *disk_name)
{
    if (!img || !img->data) return -1;
    
    /* Clear image */
    memset(img->data, 0, img->data_size);
    
    /* Boot sectors (1-3) - minimal boot code */
    uint8_t boot[128] = {0};
    boot[0] = 0x00;  /* Boot flag (0 = not bootable) */
    boot[1] = 0x01;  /* Number of sectors to load */
    boot[2] = 0x00;  /* Load address low */
    boot[3] = 0x07;  /* Load address high (0x0700) */
    boot[4] = 0x00;  /* Init address low */
    boot[5] = 0x07;  /* Init address high */
    atr_write_sector(img, 1, boot);
    
    /* VTOC (Volume Table of Contents) - Sector 360 */
    uint8_t vtoc[128] = {0};
    vtoc[0] = 0x02;  /* DOS 2.x */
    vtoc[1] = (img->sector_count) & 0xFF;        /* Total sectors low */
    vtoc[2] = (img->sector_count >> 8) & 0xFF;   /* Total sectors high */
    vtoc[3] = (img->sector_count - 8) & 0xFF;    /* Free sectors low */
    vtoc[4] = ((img->sector_count - 8) >> 8) & 0xFF;  /* Free sectors high */
    
    /* Bitmap: mark sectors 1-8 as used (boot + VTOC + directory) */
    vtoc[10] = 0x00;  /* Sectors 0-7 used */
    for (int i = 11; i < 100; i++) {
        vtoc[i] = 0xFF;  /* All other sectors free */
    }
    atr_write_sector(img, 360, vtoc);
    
    /* Directory (Sectors 361-368) */
    uint8_t dir[128];
    memset(dir, 0, 128);
    
    /* First entry: disk name if provided */
    if (disk_name) {
        dir[0] = 0x42;  /* In use, DOS 2 file */
        dir[1] = 0;     /* Sector count low */
        dir[2] = 0;     /* Sector count high */
        dir[3] = 0;     /* Starting sector low */
        dir[4] = 0;     /* Starting sector high */
        
        /* Filename (8.3 format, space padded) */
        memset(dir + 5, ' ', 11);
        size_t len = strlen(disk_name);
        if (len > 8) len = 8;
        memcpy(dir + 5, disk_name, len);
    }
    
    atr_write_sector(img, 361, dir);
    
    /* Clear remaining directory sectors */
    memset(dir, 0, 128);
    for (int s = 362; s <= 368; s++) {
        atr_write_sector(img, s, dir);
    }
    
    return 0;
}

/**
 * @brief Save ATR image to file
 */
int atr_save(const atr_image_t *img, const char *filename)
{
    if (!img || !img->data || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    /* Write header */
    if (fwrite(&img->header, ATR_HEADER_SIZE, 1, fp) != 1) { /* I/O error */ }
    /* Write data */
    if (fwrite(img->data, 1, img->data_size, fp) != img->data_size) { /* I/O error */ }
    fclose(fp);
    return 0;
}

/**
 * @brief Create ATR from raw sector data
 */
int atr_from_raw(const char *raw_file, const char *atr_file, 
                 atr_format_t format)
{
    FILE *fp = fopen(raw_file, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return -1;
    }
    if (fread(data, 1, size, fp) != size) { /* I/O error */ }
    fclose(fp);
    
    atr_image_t img;
    atr_create(&img, format);
    
    /* Copy data */
    size_t copy_size = (size < img.data_size) ? size : img.data_size;
    memcpy(img.data, data, copy_size);
    free(data);
    
    int result = atr_save(&img, atr_file);
    atr_free(&img);
    
    return result;
}

/**
 * @brief Convert XFD to ATR
 */
int atr_from_xfd(const char *xfd_file, const char *atr_file)
{
    /* XFD is just raw sectors without header */
    FILE *fp = fopen(xfd_file, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    /* Determine format from size */
    atr_format_t format;
    if (size <= 92160) {
        format = ATR_FORMAT_SD;   /* 720 × 128 = 92160 */
    } else if (size <= 133120) {
        format = ATR_FORMAT_ED;   /* 1040 × 128 = 133120 */
    } else if (size <= 184320) {
        format = ATR_FORMAT_DD;   /* 720 × 256 = 184320 */
    } else {
        format = ATR_FORMAT_QD;
    }
    
    uint8_t *data = malloc(size);
    if (fread(data, 1, size, fp) != size) { /* I/O error */ }
    fclose(fp);
    
    atr_image_t img;
    atr_create(&img, format);
    
    memcpy(img.data, data, (size < img.data_size) ? size : img.data_size);
    free(data);
    
    int result = atr_save(&img, atr_file);
    atr_free(&img);
    
    return result;
}

/**
 * @brief Free ATR image
 */
void atr_free(atr_image_t *img)
{
    if (img) {
        free(img->data);
        img->data = NULL;
        img->data_size = 0;
    }
}

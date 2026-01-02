/**
 * @file po_writer.c
 * @brief PO (ProDOS Order) Image Writer for Apple II
 * 
 * PO files use ProDOS sector interleaving:
 * Physical: 0,D,B,9,7,5,3,1,E,C,A,8,6,4,2,F
 * 
 * @version 5.29.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PO_DISK_SIZE    143360  /* 35 tracks × 16 sectors × 256 bytes */
#define SECTORS_PER_TRACK 16
#define SECTOR_SIZE     256
#define NUM_TRACKS      35

/* ProDOS to Physical sector interleave */
static const uint8_t PRODOS_TO_PHYS[16] = {
    0x0, 0x2, 0x4, 0x6, 0x8, 0xA, 0xC, 0xE,
    0x1, 0x3, 0x5, 0x7, 0x9, 0xB, 0xD, 0xF
};

/* Physical to ProDOS sector interleave */
static const uint8_t PHYS_TO_PRODOS[16] = {
    0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB,
    0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF
};

/* DOS 3.3 to Physical sector interleave */
static const uint8_t DOS33_TO_PHYS[16] = {
    0x0, 0xD, 0xB, 0x9, 0x7, 0x5, 0x3, 0x1,
    0xE, 0xC, 0xA, 0x8, 0x6, 0x4, 0x2, 0xF
};

typedef struct {
    uint8_t data[PO_DISK_SIZE];
} po_image_t;

/**
 * @brief Create empty PO image
 */
int po_create(po_image_t *img)
{
    if (!img) return -1;
    memset(img->data, 0, PO_DISK_SIZE);
    return 0;
}

/**
 * @brief Get sector offset in PO image
 */
static size_t po_sector_offset(int track, int sector)
{
    return (track * SECTORS_PER_TRACK + sector) * SECTOR_SIZE;
}

/**
 * @brief Write sector to PO image (ProDOS logical sector)
 */
int po_write_sector(po_image_t *img, int track, int sector,
                    const uint8_t *data)
{
    if (!img || !data) return -1;
    if (track < 0 || track >= NUM_TRACKS) return -1;
    if (sector < 0 || sector >= SECTORS_PER_TRACK) return -1;
    
    size_t offset = po_sector_offset(track, sector);
    memcpy(img->data + offset, data, SECTOR_SIZE);
    return 0;
}

/**
 * @brief Read sector from PO image
 */
int po_read_sector(const po_image_t *img, int track, int sector,
                   uint8_t *data)
{
    if (!img || !data) return -1;
    if (track < 0 || track >= NUM_TRACKS) return -1;
    if (sector < 0 || sector >= SECTORS_PER_TRACK) return -1;
    
    size_t offset = po_sector_offset(track, sector);
    memcpy(data, img->data + offset, SECTOR_SIZE);
    return 0;
}

/**
 * @brief Save PO image to file
 */
int po_save(const po_image_t *img, const char *filename)
{
    if (!img || !filename) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(img->data, 1, PO_DISK_SIZE, fp);
    fclose(fp);
    
    return (written == PO_DISK_SIZE) ? 0 : -1;
}

/**
 * @brief Convert DO (DOS 3.3 order) to PO (ProDOS order)
 */
int po_from_do(const char *do_file, const char *po_file)
{
    FILE *fp = fopen(do_file, "rb");
    if (!fp) return -1;
    
    uint8_t do_data[PO_DISK_SIZE];
    if (fread(do_data, 1, PO_DISK_SIZE, fp) != PO_DISK_SIZE) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    po_image_t po;
    po_create(&po);
    
    /* Convert sector ordering */
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int dos_sector = 0; dos_sector < SECTORS_PER_TRACK; dos_sector++) {
            /* DOS 3.3 logical → physical → ProDOS logical */
            int phys = DOS33_TO_PHYS[dos_sector];
            int prodos = PHYS_TO_PRODOS[phys];
            
            size_t src_offset = (track * SECTORS_PER_TRACK + dos_sector) * SECTOR_SIZE;
            size_t dst_offset = (track * SECTORS_PER_TRACK + prodos) * SECTOR_SIZE;
            
            memcpy(po.data + dst_offset, do_data + src_offset, SECTOR_SIZE);
        }
    }
    
    return po_save(&po, po_file);
}

/**
 * @brief Convert PO (ProDOS order) to DO (DOS 3.3 order)
 */
int po_to_do(const char *po_file, const char *do_file)
{
    FILE *fp = fopen(po_file, "rb");
    if (!fp) return -1;
    
    uint8_t po_data[PO_DISK_SIZE];
    if (fread(po_data, 1, PO_DISK_SIZE, fp) != PO_DISK_SIZE) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    uint8_t do_data[PO_DISK_SIZE];
    
    /* Convert sector ordering */
    for (int track = 0; track < NUM_TRACKS; track++) {
        for (int prodos_sector = 0; prodos_sector < SECTORS_PER_TRACK; prodos_sector++) {
            /* ProDOS logical → physical → DOS 3.3 logical */
            int phys = PRODOS_TO_PHYS[prodos_sector];
            int dos = 0;
            for (int i = 0; i < 16; i++) {
                if (DOS33_TO_PHYS[i] == phys) {
                    dos = i;
                    break;
                }
            }
            
            size_t src_offset = (track * SECTORS_PER_TRACK + prodos_sector) * SECTOR_SIZE;
            size_t dst_offset = (track * SECTORS_PER_TRACK + dos) * SECTOR_SIZE;
            
            memcpy(do_data + dst_offset, po_data + src_offset, SECTOR_SIZE);
        }
    }
    
    fp = fopen(do_file, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(do_data, 1, PO_DISK_SIZE, fp);
    fclose(fp);
    
    return (written == PO_DISK_SIZE) ? 0 : -1;
}

/**
 * @brief Write raw data to PO file
 */
int po_write(const char *filename, const uint8_t *data, size_t size)
{
    if (!filename || !data || size != PO_DISK_SIZE) return -1;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;
    
    size_t written = fwrite(data, 1, size, fp);
    fclose(fp);
    
    return (written == size) ? 0 : -1;
}

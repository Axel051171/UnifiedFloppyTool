/**
 * @file uft_diskio.c
 * @brief FatFs Disk I/O Module for UFT Image Files
 * 
 * Implements the FatFs disk I/O interface for reading/writing
 * floppy disk image files (IMG, IMA, DSK, etc.).
 * 
 * @copyright UFT Project 2026
 */

#include "ff.h"
#include "diskio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*============================================================================
 * Configuration
 *============================================================================*/

#define UFT_MAX_DRIVES      4       /* Maximum mounted images */
#define UFT_SECTOR_SIZE     512     /* Standard floppy sector size */

/*============================================================================
 * Drive State
 *============================================================================*/

typedef struct {
    FILE    *fp;            /* File pointer to image */
    uint32_t sectors;       /* Total sectors */
    uint16_t sector_size;   /* Bytes per sector */
    uint8_t  status;        /* Drive status */
    uint8_t  readonly;      /* Read-only flag */
    char     path[260];     /* Image file path */
} uft_drive_t;

static uft_drive_t drives[UFT_MAX_DRIVES] = {0};

/*============================================================================
 * UFT Image API
 *============================================================================*/

/**
 * @brief Mount an image file as a FatFs drive
 * 
 * @param pdrv Physical drive number (0-3)
 * @param path Path to image file
 * @param readonly Open in read-only mode
 * @return 0 on success, -1 on error
 */
int uft_mount_image(BYTE pdrv, const char *path, int readonly)
{
    if (pdrv >= UFT_MAX_DRIVES || !path) return -1;
    
    /* Close existing if any */
    if (drives[pdrv].fp) {
        fclose(drives[pdrv].fp);
        drives[pdrv].fp = NULL;
    }
    
    /* Open image file */
    FILE *fp = fopen(path, readonly ? "rb" : "r+b");
    if (!fp && !readonly) {
        /* Try read-only if read-write failed */
        fp = fopen(path, "rb");
        readonly = 1;
    }
    if (!fp) return -1;
    
    /* Get file size */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    if (size < 0 || size > 0x7FFFFFFF) {
        fclose(fp);
        return -1;
    }
    
    /* Configure drive */
    drives[pdrv].fp = fp;
    drives[pdrv].sector_size = UFT_SECTOR_SIZE;
    drives[pdrv].sectors = (uint32_t)(size / UFT_SECTOR_SIZE);
    drives[pdrv].readonly = readonly ? 1 : 0;
    drives[pdrv].status = 0;
    strncpy(drives[pdrv].path, path, sizeof(drives[pdrv].path) - 1);
    
    return 0;
}

/**
 * @brief Unmount an image file
 */
int uft_unmount_image(BYTE pdrv)
{
    if (pdrv >= UFT_MAX_DRIVES) return -1;
    
    if (drives[pdrv].fp) {
        fclose(drives[pdrv].fp);
        drives[pdrv].fp = NULL;
    }
    
    memset(&drives[pdrv], 0, sizeof(uft_drive_t));
    drives[pdrv].status = STA_NOINIT;
    
    return 0;
}

/**
 * @brief Get drive information
 */
int uft_get_drive_info(BYTE pdrv, uint32_t *sectors, uint16_t *sector_size)
{
    if (pdrv >= UFT_MAX_DRIVES || !drives[pdrv].fp) return -1;
    
    if (sectors) *sectors = drives[pdrv].sectors;
    if (sector_size) *sector_size = drives[pdrv].sector_size;
    
    return 0;
}

/*============================================================================
 * FatFs Disk I/O Implementation
 *============================================================================*/

/**
 * @brief Initialize a drive
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv >= UFT_MAX_DRIVES) return STA_NOINIT;
    
    if (!drives[pdrv].fp) {
        return STA_NOINIT | STA_NODISK;
    }
    
    drives[pdrv].status = drives[pdrv].readonly ? STA_PROTECT : 0;
    return drives[pdrv].status;
}

/**
 * @brief Get drive status
 */
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv >= UFT_MAX_DRIVES) return STA_NOINIT;
    
    if (!drives[pdrv].fp) {
        return STA_NOINIT | STA_NODISK;
    }
    
    return drives[pdrv].status;
}

/**
 * @brief Read sectors
 */
DRESULT disk_read(
    BYTE pdrv,
    BYTE *buff,
    LBA_t sector,
    UINT count)
{
    if (pdrv >= UFT_MAX_DRIVES) return RES_PARERR;
    if (!drives[pdrv].fp) return RES_NOTRDY;
    if (drives[pdrv].status & STA_NOINIT) return RES_NOTRDY;
    
    /* Check bounds */
    if (sector + count > drives[pdrv].sectors) {
        return RES_PARERR;
    }
    
    /* Seek to sector */
    long offset = (long)sector * drives[pdrv].sector_size;
    if (fseek(drives[pdrv].fp, offset, SEEK_SET) != 0) {
        return RES_ERROR;
    }
    
    /* Read data */
    size_t bytes = (size_t)count * drives[pdrv].sector_size;
    if (fread(buff, 1, bytes, drives[pdrv].fp) != bytes) {
        return RES_ERROR;
    }
    
    return RES_OK;
}

/**
 * @brief Write sectors
 */
DRESULT disk_write(
    BYTE pdrv,
    const BYTE *buff,
    LBA_t sector,
    UINT count)
{
    if (pdrv >= UFT_MAX_DRIVES) return RES_PARERR;
    if (!drives[pdrv].fp) return RES_NOTRDY;
    if (drives[pdrv].status & STA_NOINIT) return RES_NOTRDY;
    if (drives[pdrv].status & STA_PROTECT) return RES_WRPRT;
    
    /* Check bounds */
    if (sector + count > drives[pdrv].sectors) {
        return RES_PARERR;
    }
    
    /* Seek to sector */
    long offset = (long)sector * drives[pdrv].sector_size;
    if (fseek(drives[pdrv].fp, offset, SEEK_SET) != 0) {
        return RES_ERROR;
    }
    
    /* Write data */
    size_t bytes = (size_t)count * drives[pdrv].sector_size;
    if (fwrite(buff, 1, bytes, drives[pdrv].fp) != bytes) {
        return RES_ERROR;
    }
    
    /* Flush to disk */
    fflush(drives[pdrv].fp);
    
    return RES_OK;
}

/**
 * @brief Disk I/O control
 */
DRESULT disk_ioctl(
    BYTE pdrv,
    BYTE cmd,
    void *buff)
{
    if (pdrv >= UFT_MAX_DRIVES) return RES_PARERR;
    if (!drives[pdrv].fp) return RES_NOTRDY;
    if (drives[pdrv].status & STA_NOINIT) return RES_NOTRDY;
    
    switch (cmd) {
        case CTRL_SYNC:
            fflush(drives[pdrv].fp);
            return RES_OK;
            
        case GET_SECTOR_COUNT:
            *(LBA_t *)buff = drives[pdrv].sectors;
            return RES_OK;
            
        case GET_SECTOR_SIZE:
            *(WORD *)buff = drives[pdrv].sector_size;
            return RES_OK;
            
        case GET_BLOCK_SIZE:
            *(DWORD *)buff = 1;  /* No erase blocks for images */
            return RES_OK;
            
        default:
            return RES_PARERR;
    }
}

/*============================================================================
 * FatFs System Hooks
 *============================================================================*/

/* Current time for FAT timestamps */
DWORD get_fattime(void)
{
    /* Return a fixed time: 2025-01-01 00:00:00 */
    return ((DWORD)(2025 - 1980) << 25)
         | ((DWORD)1 << 21)
         | ((DWORD)1 << 16)
         | ((DWORD)0 << 11)
         | ((DWORD)0 << 5)
         | ((DWORD)0 >> 1);
}

/*============================================================================
 * Example Usage
 *============================================================================*/

#ifdef UFT_DISKIO_TEST

#include <stdio.h>

int main(int argc, char *argv[])
{
    FATFS fs;
    FRESULT res;
    DIR dir;
    FILINFO fno;
    
    if (argc < 2) {
        printf("Usage: %s <image.img>\n", argv[0]);
        return 1;
    }
    
    /* Mount image */
    if (uft_mount_image(0, argv[1], 0) != 0) {
        printf("Failed to open image: %s\n", argv[1]);
        return 1;
    }
    
    uint32_t sectors;
    uint16_t sector_size;
    uft_get_drive_info(0, &sectors, &sector_size);
    printf("Image: %u sectors x %u bytes = %u KB\n",
           sectors, sector_size, (sectors * sector_size) / 1024);
    
    /* Mount filesystem */
    res = f_mount(&fs, "", 1);
    if (res != FR_OK) {
        printf("f_mount failed: %d\n", res);
        uft_unmount_image(0);
        return 1;
    }
    
    /* List root directory */
    res = f_opendir(&dir, "/");
    if (res == FR_OK) {
        printf("\nDirectory listing:\n");
        while (1) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            
            printf("  %c%c%c%c%c %10lu  %s\n",
                   (fno.fattrib & AM_DIR) ? 'D' : '-',
                   (fno.fattrib & AM_RDO) ? 'R' : '-',
                   (fno.fattrib & AM_HID) ? 'H' : '-',
                   (fno.fattrib & AM_SYS) ? 'S' : '-',
                   (fno.fattrib & AM_ARC) ? 'A' : '-',
                   fno.fsize,
                   fno.fname);
        }
        f_closedir(&dir);
    }
    
    /* Cleanup */
    f_mount(0, "", 0);
    uft_unmount_image(0);
    
    return 0;
}

#endif /* UFT_DISKIO_TEST */

#include <stdint.h>
#include <stdbool.h>
/**
 * @file uft_mega65_d81.c
 * @brief MEGA65 D81 disk image format implementation
 * 
 * D81 is the Commodore 1581 3.5" floppy disk format:
 * - 80 tracks, 40 sectors/track
 * - 256 bytes/sector
 * - 800 KB total (819200 bytes)
 * 
 * Used by: Commodore 1581, Commodore 128D, MEGA65
 * 
 * Directory structure:
 * - Track 40, Sector 0: Header sector
 * - Track 40, Sectors 1-2: BAM (Block Availability Map)
 * - Track 40, Sector 3+: Directory entries
 * 
 * Ported from MEGA65 fdisk project (GPL-3.0)
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Include our header - checking both possible locations */
#ifdef UFT_MEGA65_H
#include "uft/formats/uft_mega65.h"
#else
/* Standalone definitions if header not found */
#define UFT_D81_TRACKS          80
#define UFT_D81_SECTORS_TRACK   40
#define UFT_D81_SECTOR_SIZE     256
#define UFT_D81_SIZE            819200

#define UFT_D81_DIR_TRACK       40
#define UFT_D81_BAM_TRACK       40

#define UFT_D81_TYPE_DEL    0x00
#define UFT_D81_TYPE_SEQ    0x01
#define UFT_D81_TYPE_PRG    0x02
#define UFT_D81_TYPE_USR    0x03
#define UFT_D81_TYPE_REL    0x04
#define UFT_D81_TYPE_CBM    0x05

#define UFT_MEGA65_OK               0
#define UFT_MEGA65_ERROR_READ       (-1)
#define UFT_MEGA65_ERROR_WRITE      (-2)
#define UFT_MEGA65_ERROR_FORMAT     (-3)
#define UFT_MEGA65_ERROR_FULL       (-4)
#define UFT_MEGA65_ERROR_NOT_FOUND  (-5)
#define UFT_MEGA65_ERROR_PARAM      (-6)
#endif

/*============================================================================*
 * STRUCTURES (if not defined in header)
 *============================================================================*/

#ifndef UFT_D81_IMAGE_DEFINED
typedef struct {
    uint8_t *data;
    size_t   size;
    char     disk_name[17];
    char     disk_id[3];
    uint16_t free_blocks;
    int      modified;
} uft_d81_image_t;

typedef struct {
    char     name[17];
    uint8_t  type;
    uint16_t blocks;
    uint32_t size;
    uint8_t  locked;
    uint8_t  closed;
    uint8_t  first_track;
    uint8_t  first_sector;
} uft_d81_file_info_t;
#endif

/* Forward declaration for function used before definition */
uint16_t uft_d81_free_blocks(const uft_d81_image_t *image);

/* SD card info struct (used by detect function) */
#ifndef UFT_MEGA65_SD_INFO_DEFINED
#define UFT_MEGA65_SD_INFO_DEFINED
typedef struct {
    uint32_t sys_start;
    uint32_t sys_size;
    uint32_t data_start;
    uint32_t data_size;
    uint32_t total_sectors;
    int has_sys_partition;
    int valid;
} uft_mega65_sd_info_t;
#endif

/*============================================================================*
 * PETSCII CONVERSION
 *============================================================================*/

/**
 * @brief PETSCII to ASCII conversion table
 */
static const char petscii_to_ascii_table[256] = {
    /* 0x00-0x1F: Control codes */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\n', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0x20-0x3F: Standard ASCII range */
    ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
    '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
    /* 0x40-0x5F: Letters (uppercase in PETSCII) */
    '@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
    'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '[', '\\', ']', '^', '_',
    /* 0x60-0x7F: Graphics characters -> placeholders */
    '-', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '+', '|', '+', '-', '|',
    /* 0x80-0x9F: More control/graphics */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '\n', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    /* 0xA0-0xBF: Shifted space and graphics */
    ' ', '|', '-', '-', '-', '|', '|', '|', '|', '+', '+', '+', '+', '+', '+', '+',
    '+', '+', '+', '+', '-', '-', '|', '|', '|', '|', '|', '+', '+', '+', '+', '+',
    /* 0xC0-0xDF: Lowercase letters in shifted mode */
    '-', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '+', '|', '+', '-', '|',
    /* 0xE0-0xFF: More graphics */
    ' ', '|', '-', '-', '-', '|', '|', '|', '|', '+', '+', '+', '+', '+', '+', '+',
    '+', '+', '+', '+', '-', '-', '|', '|', '|', '|', '|', '+', '+', '+', '+', ' '
};

void uft_petscii_to_ascii(const uint8_t *petscii, char *ascii, size_t len)
{
    for (size_t i = 0; i < len && petscii[i] != 0 && petscii[i] != 0xA0; i++) {
        char c = petscii_to_ascii_table[petscii[i]];
        ascii[i] = c ? c : '?';
        ascii[i + 1] = '\0';
    }
}

void uft_ascii_to_petscii(const char *ascii, uint8_t *petscii, size_t len)
{
    for (size_t i = 0; i < len && ascii[i] != '\0'; i++) {
        unsigned char c = (unsigned char)ascii[i];
        
        if (c >= 'a' && c <= 'z') {
            petscii[i] = c - 'a' + 0x41;  /* Lowercase -> PETSCII uppercase */
        } else if (c >= 'A' && c <= 'Z') {
            petscii[i] = c - 'A' + 0xC1;  /* Uppercase -> PETSCII shifted */
        } else if (c >= 0x20 && c <= 0x3F) {
            petscii[i] = c;  /* Standard ASCII range */
        } else {
            petscii[i] = '?';
        }
    }
    
    /* Pad with shifted space (0xA0) */
    for (size_t i = strlen(ascii); i < len; i++) {
        petscii[i] = 0xA0;
    }
}

/*============================================================================*
 * D81 FILE TYPE STRINGS
 *============================================================================*/

const char *uft_d81_file_type_str(uint8_t type)
{
    type &= 0x0F;  /* Mask off flags */
    
    switch (type) {
    case UFT_D81_TYPE_DEL: return "DEL";
    case UFT_D81_TYPE_SEQ: return "SEQ";
    case UFT_D81_TYPE_PRG: return "PRG";
    case UFT_D81_TYPE_USR: return "USR";
    case UFT_D81_TYPE_REL: return "REL";
    case UFT_D81_TYPE_CBM: return "CBM";
    default: return "???";
    }
}

/*============================================================================*
 * D81 SECTOR CALCULATION
 *============================================================================*/

uint32_t uft_d81_sector_offset(uint8_t track, uint8_t sector)
{
    if (track < 1 || track > UFT_D81_TRACKS || sector >= UFT_D81_SECTORS_TRACK) {
        return 0xFFFFFFFF;  /* Invalid */
    }
    
    /* D81: Simple linear layout */
    /* Track 1 = offset 0, Track 2 = offset 40*256, etc. */
    return ((track - 1) * UFT_D81_SECTORS_TRACK + sector) * UFT_D81_SECTOR_SIZE;
}

/*============================================================================*
 * D81 PROBING
 *============================================================================*/

int uft_d81_probe(const uint8_t *data, size_t size)
{
    /* Check size */
    if (size != UFT_D81_SIZE) {
        return 0;
    }
    
    /* Check header sector (Track 40, Sector 0) */
    uint32_t header_offset = uft_d81_sector_offset(40, 0);
    if (header_offset >= size) {
        return 0;
    }
    
    const uint8_t *header = data + header_offset;
    
    /* Check directory track/sector */
    if (header[0] != 40 || header[1] != 3) {
        return 0;
    }
    
    /* Check disk format byte ('D' for 1581) */
    if (header[2] != 'D') {
        return 0;
    }
    
    return 1;
}

/*============================================================================*
 * D81 IMAGE FUNCTIONS
 *============================================================================*/

int uft_d81_open(uft_d81_image_t *image, const char *filename)
{
    FILE *f;
    size_t read_size;
    
    if (!image || !filename) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    memset(image, 0, sizeof(*image));
    
    f = fopen(filename, "rb");
    if (!f) {
        return UFT_MEGA65_ERROR_READ;
    }
    
    /* Check file size */
    fseek(f, 0, SEEK_END);
    image->size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (image->size != UFT_D81_SIZE) {
        fclose(f);
        return UFT_MEGA65_ERROR_FORMAT;
    }
    
    /* Allocate and read data */
    image->data = (uint8_t *)malloc(UFT_D81_SIZE);
    if (!image->data) {
        fclose(f);
        return UFT_MEGA65_ERROR_READ;
    }
    
    read_size = fread(image->data, 1, UFT_D81_SIZE, f);
    fclose(f);
    
    if (read_size != UFT_D81_SIZE) {
        free(image->data);
        image->data = NULL;
        return UFT_MEGA65_ERROR_READ;
    }
    
    /* Validate */
    if (!uft_d81_probe(image->data, image->size)) {
        free(image->data);
        image->data = NULL;
        return UFT_MEGA65_ERROR_FORMAT;
    }
    
    /* Extract disk name and ID from header */
    uint32_t header_off = uft_d81_sector_offset(40, 0);
    uft_petscii_to_ascii(image->data + header_off + 4, image->disk_name, 16);
    uft_petscii_to_ascii(image->data + header_off + 22, image->disk_id, 2);
    
    /* Count free blocks */
    image->free_blocks = uft_d81_free_blocks(image);
    image->modified = 0;
    
    return UFT_MEGA65_OK;
}

int uft_d81_create(uft_d81_image_t *image, const char *disk_name, const char *disk_id)
{
    if (!image) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    memset(image, 0, sizeof(*image));
    
    /* Allocate image data */
    image->data = (uint8_t *)calloc(1, UFT_D81_SIZE);
    if (!image->data) {
        return UFT_MEGA65_ERROR_READ;
    }
    
    image->size = UFT_D81_SIZE;
    
    /* Initialize header sector (Track 40, Sector 0) */
    uint8_t *header = image->data + uft_d81_sector_offset(40, 0);
    
    header[0] = 40;     /* Directory track */
    header[1] = 3;      /* Directory sector */
    header[2] = 'D';    /* Disk format (1581) */
    header[3] = 0;
    
    /* Disk name (padded with 0xA0) */
    memset(header + 4, 0xA0, 16);
    if (disk_name) {
        uft_ascii_to_petscii(disk_name, header + 4, 16);
        strncpy(image->disk_name, disk_name, 16);
        image->disk_name[16] = '\0';
    } else {
        memcpy(header + 4, "EMPTY DISK      ", 16);
        snprintf(image->disk_name, sizeof(image->disk_name), "EMPTY DISK");
    }
    
    header[20] = 0xA0;
    header[21] = 0xA0;
    
    /* Disk ID */
    if (disk_id && strlen(disk_id) >= 2) {
        uft_ascii_to_petscii(disk_id, header + 22, 2);
        image->disk_id[0] = disk_id[0];
        image->disk_id[1] = disk_id[1];
        image->disk_id[2] = '\0';
    } else {
        header[22] = '0';
        header[23] = '0';
        snprintf(image->disk_id, sizeof(image->disk_id), "00");
    }
    
    header[24] = 0xA0;
    header[25] = '3';   /* DOS version */
    header[26] = 'D';
    header[27] = 0xA0;
    header[28] = 0xA0;
    header[29] = 0xA0;
    header[30] = 0xA0;
    
    /* Initialize BAM sectors (Track 40, Sectors 1-2) */
    for (int bam_sector = 1; bam_sector <= 2; bam_sector++) {
        uint8_t *bam = image->data + uft_d81_sector_offset(40, bam_sector);
        
        bam[0] = 40;    /* Directory track */
        bam[1] = (bam_sector == 1) ? 2 : 0xFF;  /* Next BAM sector or end */
        bam[2] = 'D';
        bam[3] = 0;
        
        /* Initialize BAM entries - all sectors free except track 40 */
        int start_track = (bam_sector == 1) ? 1 : 41;
        int end_track = (bam_sector == 1) ? 40 : 80;
        
        for (int t = start_track; t <= end_track; t++) {
            int entry_offset = 4 + (t - start_track) * 6;
            
            if (t == 40) {
                /* Track 40 (directory) - mark header/BAM/first dir sector as used */
                bam[entry_offset] = 36;  /* 40 - 4 used sectors */
                bam[entry_offset + 1] = 0xF0;  /* Sectors 0-3 used */
                bam[entry_offset + 2] = 0xFF;
                bam[entry_offset + 3] = 0xFF;
                bam[entry_offset + 4] = 0xFF;
                bam[entry_offset + 5] = 0xFF;
            } else {
                /* All other tracks - all sectors free */
                bam[entry_offset] = 40;  /* 40 free sectors */
                bam[entry_offset + 1] = 0xFF;
                bam[entry_offset + 2] = 0xFF;
                bam[entry_offset + 3] = 0xFF;
                bam[entry_offset + 4] = 0xFF;
                bam[entry_offset + 5] = 0xFF;
            }
        }
    }
    
    /* Initialize first directory sector (Track 40, Sector 3) */
    uint8_t *dir = image->data + uft_d81_sector_offset(40, 3);
    dir[0] = 0;     /* No next track (end of directory) */
    dir[1] = 0xFF;  /* End marker */
    
    /* Calculate free blocks */
    /* 80 tracks * 40 sectors - directory track overhead (4 sectors) */
    image->free_blocks = (80 * 40) - 4;
    image->modified = 1;
    
    return UFT_MEGA65_OK;
}

int uft_d81_save(const uft_d81_image_t *image, const char *filename)
{
    FILE *f;
    size_t written;
    
    if (!image || !image->data || !filename) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    f = fopen(filename, "wb");
    if (!f) {
        return UFT_MEGA65_ERROR_WRITE;
    }
    
    written = fwrite(image->data, 1, image->size, f);
    fclose(f);
    
    if (written != image->size) {
        return UFT_MEGA65_ERROR_WRITE;
    }
    
    return UFT_MEGA65_OK;
}

void uft_d81_close(uft_d81_image_t *image)
{
    if (image) {
        if (image->data) {
            free(image->data);
            image->data = NULL;
        }
        memset(image, 0, sizeof(*image));
    }
}

int uft_d81_read_sector(const uft_d81_image_t *image,
                        uint8_t track, uint8_t sector,
                        uint8_t *buffer)
{
    uint32_t offset;
    
    if (!image || !image->data || !buffer) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    offset = uft_d81_sector_offset(track, sector);
    if (offset == 0xFFFFFFFF || offset + UFT_D81_SECTOR_SIZE > image->size) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    memcpy(buffer, image->data + offset, UFT_D81_SECTOR_SIZE);
    return UFT_MEGA65_OK;
}

int uft_d81_write_sector(uft_d81_image_t *image,
                         uint8_t track, uint8_t sector,
                         const uint8_t *buffer)
{
    uint32_t offset;
    
    if (!image || !image->data || !buffer) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    offset = uft_d81_sector_offset(track, sector);
    if (offset == 0xFFFFFFFF || offset + UFT_D81_SECTOR_SIZE > image->size) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    memcpy(image->data + offset, buffer, UFT_D81_SECTOR_SIZE);
    image->modified = 1;
    return UFT_MEGA65_OK;
}

uint16_t uft_d81_free_blocks(const uft_d81_image_t *image)
{
    uint16_t free = 0;
    
    if (!image || !image->data) {
        return 0;
    }
    
    /* Sum free blocks from both BAM sectors */
    for (int bam_sector = 1; bam_sector <= 2; bam_sector++) {
        uint8_t *bam = image->data + uft_d81_sector_offset(40, bam_sector);
        int start_track = (bam_sector == 1) ? 1 : 41;
        int end_track = (bam_sector == 1) ? 40 : 80;
        
        for (int t = start_track; t <= end_track; t++) {
            int entry_offset = 4 + (t - start_track) * 6;
            free += bam[entry_offset];
        }
    }
    
    return free;
}

int uft_d81_list_files(const uft_d81_image_t *image,
                       uft_d81_file_info_t *files,
                       int max_files,
                       int *count)
{
    uint8_t track, sector;
    int file_count = 0;
    uint8_t sector_data[UFT_D81_SECTOR_SIZE];
    
    if (!image || !image->data || !files || !count) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    *count = 0;
    
    /* Start at first directory sector */
    track = 40;
    sector = 3;
    
    while (track != 0 && file_count < max_files) {
        if (uft_d81_read_sector(image, track, sector, sector_data) != UFT_MEGA65_OK) {
            break;
        }
        
        /* Each directory sector has 8 entries of 32 bytes */
        for (int i = 0; i < 8 && file_count < max_files; i++) {
            uint8_t *entry = sector_data + (i * 32);
            
            /* Skip first 2 bytes of first entry (link to next sector) */
            if (i == 0) {
                entry += 2;
            }
            
            uint8_t file_type = entry[0];
            
            /* Skip empty and deleted entries */
            if (file_type == 0 || (file_type & 0x0F) == UFT_D81_TYPE_DEL) {
                continue;
            }
            
            uft_d81_file_info_t *f = &files[file_count];
            
            /* Extract filename */
            uft_petscii_to_ascii(entry + 3, f->name, 16);
            
            /* Trim trailing spaces */
            for (int j = 15; j >= 0 && f->name[j] == ' '; j--) {
                f->name[j] = '\0';
            }
            
            f->type = file_type & 0x0F;
            f->locked = (file_type & 0x40) ? 1 : 0;
            f->closed = (file_type & 0x80) ? 1 : 0;
            f->first_track = entry[1];
            f->first_sector = entry[2];
            f->blocks = entry[28] | (entry[29] << 8);
            f->size = f->blocks * 254;  /* Approximate (254 data bytes per block) */
            
            file_count++;
        }
        
        /* Get next directory sector */
        track = sector_data[0];
        sector = sector_data[1];
    }
    
    *count = file_count;
    return UFT_MEGA65_OK;
}

int uft_d81_validate(const uft_d81_image_t *image)
{
    if (!image || !image->data || image->size != UFT_D81_SIZE) {
        return UFT_MEGA65_ERROR_FORMAT;
    }
    
    return uft_d81_probe(image->data, image->size) ? UFT_MEGA65_OK : UFT_MEGA65_ERROR_FORMAT;
}

/*============================================================================*
 * MEGA65 SD CARD DETECTION
 *============================================================================*/

int uft_mega65_detect(int (*read_sector)(uint32_t, uint8_t*, void*),
                      void *user_data,
                      uft_mega65_sd_info_t *info)
{
    uint8_t mbr[512];
    
    if (!read_sector || !info) {
        return UFT_MEGA65_ERROR_PARAM;
    }
    
    memset(info, 0, sizeof(*info));
    
    /* Read MBR */
    if (read_sector(0, mbr, user_data) != 0) {
        return UFT_MEGA65_ERROR_READ;
    }
    
    /* Check MBR signature */
    if (mbr[510] != 0x55 || mbr[511] != 0xAA) {
        return UFT_MEGA65_ERROR_FORMAT;
    }
    
    /* Parse partition table */
    uint8_t *ptable = mbr + 446;
    
    for (int i = 0; i < 4; i++) {
        uint8_t *entry = ptable + (i * 16);
        uint8_t type = entry[4];
        uint32_t start = entry[8] | (entry[9] << 8) | (entry[10] << 16) | (entry[11] << 24);
        uint32_t size = entry[12] | (entry[13] << 8) | (entry[14] << 16) | (entry[15] << 24);
        
        if (type == 0x41) {
            /* MEGA65 system partition */
            info->sys_start = start;
            info->sys_size = size;
            info->has_sys_partition = 1;
        } else if (type == 0x0B || type == 0x0C) {
            /* FAT32 partition */
            info->data_start = start;
            info->data_size = size;
        }
        
        if (start + size > info->total_sectors) {
            info->total_sectors = start + size;
        }
    }
    
    /* Valid if we have at least a FAT32 data partition */
    info->valid = (info->data_size > 0) ? 1 : 0;
    
    return info->valid ? UFT_MEGA65_OK : UFT_MEGA65_ERROR_FORMAT;
}

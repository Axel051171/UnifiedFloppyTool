/**
 * @file uft_atari8_disk.c
 * @brief Atari 8-bit Floppy Disk Access Implementation
 * 
 * Based on "Direct Atari Disk Access" by Andrew Lieberman
 * COMPUTE! Magazine, Issue 34, March 1983
 */

#include "uft_atari8_disk.h"
#include <string.h>

/*===========================================================================
 * ATR Image Handling
 *===========================================================================*/

int uft_atari_atr_parse_header(const uint8_t *data, uft_atari_atr_header_t *hdr)
{
    if (!data || !hdr) return -1;
    
    /* Check magic */
    uint16_t magic = data[0] | (data[1] << 8);
    if (magic != UFT_ATARI_ATR_MAGIC) {
        return -1;  /* Not an ATR file */
    }
    
    hdr->magic = magic;
    hdr->paragraphs = data[2] | (data[3] << 8);
    hdr->sector_size = data[4] | (data[5] << 8);
    hdr->paragraphs_hi = data[6];
    hdr->crc = data[7] | (data[8] << 8) | (data[9] << 16) | (data[10] << 24);
    hdr->unused = data[11] | (data[12] << 8) | (data[13] << 16) | (data[14] << 24);
    hdr->flags = data[15];
    
    return 0;
}

int uft_atari_atr_read_sector(const uint8_t *image, size_t image_size,
                               uint16_t sector, uint8_t *buffer,
                               uint16_t sector_size)
{
    if (!image || !buffer || sector < 1) return -1;
    
    size_t offset = uft_atari_atr_sector_offset(sector, sector_size);
    size_t size = (sector <= 3) ? 128 : sector_size;
    
    if (offset + size > image_size) {
        return -1;  /* Sector out of range */
    }
    
    memcpy(buffer, image + offset, size);
    
    /* Pad with zeros if needed */
    if (size < sector_size) {
        memset(buffer + size, 0, sector_size - size);
    }
    
    return 0;
}

int uft_atari_atr_write_sector(uint8_t *image, size_t image_size,
                                uint16_t sector, const uint8_t *buffer,
                                uint16_t sector_size)
{
    if (!image || !buffer || sector < 1) return -1;
    
    size_t offset = uft_atari_atr_sector_offset(sector, sector_size);
    size_t size = (sector <= 3) ? 128 : sector_size;
    
    if (offset + size > image_size) {
        return -1;  /* Sector out of range */
    }
    
    memcpy(image + offset, buffer, size);
    return 0;
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

uft_atari_density_t uft_atari_detect_density(const uint8_t *image, 
                                              size_t image_size)
{
    /* Check if ATR format */
    if (image_size >= 16) {
        uint16_t magic = image[0] | (image[1] << 8);
        if (magic == UFT_ATARI_ATR_MAGIC) {
            /* Parse ATR header */
            uint16_t sector_size = image[4] | (image[5] << 8);
            uint32_t paragraphs = (image[2] | (image[3] << 8)) | 
                                  ((uint32_t)image[6] << 16);
            size_t data_size = paragraphs * 16;
            
            if (sector_size == 256) {
                return UFT_ATARI_FMT_DD;
            } else if (data_size > 92160) {
                return UFT_ATARI_FMT_ED;  /* Enhanced density */
            } else {
                return UFT_ATARI_FMT_SD;
            }
        }
    }
    
    /* XFD format - determine by size */
    if (image_size == 92160) {
        return UFT_ATARI_FMT_SD;   /* 720 * 128 */
    } else if (image_size == 133120) {
        return UFT_ATARI_FMT_ED;  /* 1040 * 128 */
    } else if (image_size == 183936 || image_size == 184320) {
        return UFT_ATARI_FMT_DD;  /* ~720 * 256 (with boot sectors at 128) */
    } else if (image_size == 368640) {
        return UFT_ATARI_FMT_QD;  /* Double sided */
    } else if (image_size == 737280) {
        return UFT_ATARI_FMT_HD;  /* 3.5" HD */
    }
    
    return UFT_ATARI_FMT_UNKNOWN;
}

/*===========================================================================
 * VTOC and Directory
 *===========================================================================*/

int uft_atari_read_vtoc(const uint8_t *image, size_t image_size,
                         uft_atari_vtoc_t *vtoc)
{
    if (!image || !vtoc) return -1;
    
    /* Determine format */
    uft_atari_density_t density = uft_atari_detect_density(image, image_size);
    if (density == UFT_ATARI_FMT_UNKNOWN) return -1;
    
    /* Read VTOC sector (360) */
    uint8_t sector[256];
    uint16_t sector_size = (density == UFT_ATARI_FMT_DD) ? 256 : 128;
    
    /* Check if ATR */
    uint16_t magic = image[0] | (image[1] << 8);
    int is_atr = (magic == UFT_ATARI_ATR_MAGIC);
    
    size_t offset;
    if (is_atr) {
        offset = uft_atari_atr_sector_offset(UFT_ATARI_VTOC_SECTOR, sector_size);
    } else {
        offset = uft_atari_xfd_sector_offset(UFT_ATARI_VTOC_SECTOR, sector_size);
    }
    
    if (offset + sector_size > image_size) return -1;
    
    memcpy(sector, image + offset, sector_size);
    
    /* Parse VTOC */
    vtoc->dos_code = sector[0];
    vtoc->total_sectors = sector[1] | (sector[2] << 8);
    vtoc->free_sectors = sector[3] | (sector[4] << 8);
    memcpy(vtoc->unused, &sector[5], 5);
    memcpy(vtoc->bitmap, &sector[10], 90);
    
    return 0;
}

int uft_atari_read_dir_entry(const uint8_t *image, size_t image_size,
                              int index, uft_atari_dir_entry_t *entry)
{
    if (!image || !entry || index < 0 || index >= UFT_ATARI_DIR_ENTRIES) {
        return -1;
    }
    
    /* Determine format */
    uft_atari_density_t density = uft_atari_detect_density(image, image_size);
    if (density == UFT_ATARI_FMT_UNKNOWN) return -1;
    
    uint16_t sector_size = (density == UFT_ATARI_FMT_DD) ? 256 : 128;
    
    /* Calculate directory sector and offset */
    /* Directory is sectors 361-368, 8 entries per sector (SD) */
    int entries_per_sector = sector_size / UFT_ATARI_DIR_ENTRY_SIZE;
    int sector_num = UFT_ATARI_DIR_START + (index / entries_per_sector);
    int entry_offset = (index % entries_per_sector) * UFT_ATARI_DIR_ENTRY_SIZE;
    
    /* Check if ATR */
    uint16_t magic = image[0] | (image[1] << 8);
    int is_atr = (magic == UFT_ATARI_ATR_MAGIC);
    
    size_t offset;
    if (is_atr) {
        offset = uft_atari_atr_sector_offset(sector_num, sector_size);
    } else {
        offset = uft_atari_xfd_sector_offset(sector_num, sector_size);
    }
    
    if (offset + entry_offset + UFT_ATARI_DIR_ENTRY_SIZE > image_size) {
        return -1;
    }
    
    /* Copy entry data */
    const uint8_t *e = image + offset + entry_offset;
    entry->flags = e[0];
    entry->sector_count = e[1] | (e[2] << 8);
    entry->start_sector = e[3] | (e[4] << 8);
    memcpy(entry->filename, &e[5], 8);
    memcpy(entry->extension, &e[13], 3);
    
    return 0;
}

int uft_atari_find_file(const uint8_t *image, size_t image_size,
                         const char *filename, uft_atari_dir_entry_t *entry)
{
    if (!image || !filename || !entry) return -1;
    
    /* Prepare search name (8.3, space-padded) */
    uint8_t search_name[8];
    uint8_t search_ext[3];
    memset(search_name, ' ', 8);
    memset(search_ext, ' ', 3);
    
    /* Parse filename.ext */
    const char *dot = strchr(filename, '.');
    size_t name_len = dot ? (size_t)(dot - filename) : strlen(filename);
    if (name_len > 8) name_len = 8;
    memcpy(search_name, filename, name_len);
    
    if (dot) {
        size_t ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        memcpy(search_ext, dot + 1, ext_len);
    }
    
    /* Convert to uppercase */
    for (int i = 0; i < 8; i++) {
        if (search_name[i] >= 'a' && search_name[i] <= 'z') {
            search_name[i] -= 32;
        }
    }
    for (int i = 0; i < 3; i++) {
        if (search_ext[i] >= 'a' && search_ext[i] <= 'z') {
            search_ext[i] -= 32;
        }
    }
    
    /* Search directory */
    for (int i = 0; i < UFT_ATARI_DIR_ENTRIES; i++) {
        if (uft_atari_read_dir_entry(image, image_size, i, entry) != 0) {
            continue;
        }
        
        /* Check if entry is valid (not deleted, not empty) */
        if (entry->flags == 0 || (entry->flags & UFT_ATARI_DIR_DELETED)) {
            continue;
        }
        
        /* Compare name and extension */
        if (memcmp(entry->filename, search_name, 8) == 0 &&
            memcmp(entry->extension, search_ext, 3) == 0) {
            return i;
        }
    }
    
    return -1;  /* Not found */
}

/*===========================================================================
 * File Extraction
 *===========================================================================*/

/**
 * @brief Extract file data from disk image
 * @param image Disk image
 * @param image_size Image size
 * @param entry Directory entry of file
 * @param buffer Output buffer (must be large enough)
 * @param buffer_size Size of output buffer
 * @param bytes_read Output: actual bytes read
 * @return 0 on success
 */
int uft_atari_extract_file(const uint8_t *image, size_t image_size,
                            const uft_atari_dir_entry_t *entry,
                            uint8_t *buffer, size_t buffer_size,
                            size_t *bytes_read)
{
    if (!image || !entry || !buffer || !bytes_read) return -1;
    
    *bytes_read = 0;
    
    /* Determine format */
    uft_atari_density_t density = uft_atari_detect_density(image, image_size);
    if (density == UFT_ATARI_FMT_UNKNOWN) return -1;
    
    uint16_t sector_size = (density == UFT_ATARI_FMT_DD) ? 256 : 128;
    uint16_t data_bytes = (density == UFT_ATARI_FMT_DD) ? 253 : 125;
    
    /* Check if ATR */
    uint16_t magic = image[0] | (image[1] << 8);
    int is_atr = (magic == UFT_ATARI_ATR_MAGIC);
    
    /* Follow sector chain */
    uint16_t sector = entry->start_sector;
    int sectors_remaining = entry->sector_count;
    
    while (sector != 0 && sectors_remaining > 0) {
        /* Calculate sector offset */
        size_t offset;
        if (is_atr) {
            offset = uft_atari_atr_sector_offset(sector, sector_size);
        } else {
            offset = uft_atari_xfd_sector_offset(sector, sector_size);
        }
        
        if (offset + sector_size > image_size) {
            return -1;  /* Invalid sector */
        }
        
        /* Read sector data */
        const uint8_t *sec = image + offset;
        
        /* Get bytes used in this sector */
        uint8_t used = sec[sector_size - 1];
        if (used > data_bytes) used = data_bytes;
        
        /* Copy data */
        if (*bytes_read + used > buffer_size) {
            return -1;  /* Buffer too small */
        }
        memcpy(buffer + *bytes_read, sec + 1, used);  /* Skip first byte (file num) */
        *bytes_read += used;
        
        /* Get next sector */
        uint16_t next_lo = sec[sector_size - 2];
        uint16_t next_hi = sec[0] & 0x03;
        sector = next_lo | (next_hi << 8);
        
        sectors_remaining--;
    }
    
    return 0;
}

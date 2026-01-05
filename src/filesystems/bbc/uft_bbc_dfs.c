/**
 * @file uft_bbc_dfs.c
 * @brief BBC Micro DFS Implementation
 * 
 * Based on bbctapedisc by W.H.Scholten, R.Schmidt, Jon Welch et al.
 */

#include "uft_bbc_dfs.h"
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Catalog Parsing
 *===========================================================================*/

int uft_dfs_read_entry(const uft_dfs_cat0_t *cat0,
                       const uft_dfs_cat1_t *cat1,
                       int index, uft_dfs_file_entry_t *entry)
{
    if (!cat0 || !cat1 || !entry) return -1;
    
    int num_files = uft_dfs_get_file_count(cat1);
    if (index < 0 || index >= num_files) return -1;
    
    /* Catalog sector 0: filename at offset 8 + index*8 */
    const uint8_t *name_entry = cat0->entries + (index * 8);
    
    /* Copy filename (7 chars) */
    for (int i = 0; i < 7; i++) {
        char c = name_entry[i] & 0x7F;
        entry->filename[i] = (c == ' ') ? '\0' : c;
    }
    entry->filename[7] = '\0';
    
    /* Directory and locked flag from byte 7 */
    entry->directory = name_entry[7] & 0x7F;
    entry->locked = (name_entry[7] & 0x80) != 0;
    
    /* Catalog sector 1: file info at offset 8 + index*8 */
    const uint8_t *info_entry = cat1->info + (index * 8);
    
    /* Load address (low 16 bits) */
    uint16_t load_lo = info_entry[0] | (info_entry[1] << 8);
    
    /* Exec address (low 16 bits) */
    uint16_t exec_lo = info_entry[2] | (info_entry[3] << 8);
    
    /* Length (low 16 bits) */
    uint16_t len_lo = info_entry[4] | (info_entry[5] << 8);
    
    /* Mixed bits */
    uint8_t mixed = info_entry[6];
    
    /* Start sector (low 8 bits) */
    uint8_t start_lo = info_entry[7];
    
    /* Combine with high bits from mixed byte */
    entry->start_sector = start_lo | (UFT_DFS_MIXED_START_HI(mixed) << 8);
    entry->load_addr = load_lo | (UFT_DFS_MIXED_LOAD_HI(mixed) << 16);
    entry->exec_addr = exec_lo | (UFT_DFS_MIXED_EXEC_HI(mixed) << 16);
    entry->length = len_lo | (UFT_DFS_MIXED_LEN_HI(mixed) << 16);
    
    return 0;
}

/*===========================================================================
 * Disk Image Creation
 *===========================================================================*/

void uft_dfs_create_catalog(uint8_t *buffer, uint16_t sectors,
                            const char *title, uft_dfs_boot_t boot_option)
{
    /* Clear catalog sectors */
    memset(buffer, 0, 512);
    
    /* Copy title (up to 12 chars, split between sector 0 and 1) */
    if (title) {
        size_t len = strlen(title);
        if (len > 12) len = 12;
        
        /* First 8 chars to sector 0 */
        size_t first = (len > 8) ? 8 : len;
        memcpy(buffer, title, first);
        
        /* Remaining chars to sector 1 */
        if (len > 8) {
            memcpy(buffer + 256, title + 8, len - 8);
        }
    }
    
    /* Sector 1 setup */
    uft_dfs_cat1_t *cat1 = (uft_dfs_cat1_t *)(buffer + 256);
    
    /* Sequence number = 0 */
    cat1->sequence = 0;
    
    /* Number of entries = 0 */
    cat1->num_entries = 0;
    
    /* Boot option and sectors */
    cat1->opt_sectors_hi = ((boot_option & 0x03) << 4) | ((sectors >> 8) & 0x03);
    cat1->sectors_lo = sectors & 0xFF;
}

/*===========================================================================
 * File Operations
 *===========================================================================*/

int uft_dfs_add_file(uint8_t *image, size_t image_size,
                     const char *filename, uint32_t load_addr,
                     uint32_t exec_addr, const uint8_t *data,
                     size_t length)
{
    if (!image || image_size < 512 || !filename) return -1;
    
    uft_dfs_cat0_t *cat0 = (uft_dfs_cat0_t *)image;
    uft_dfs_cat1_t *cat1 = (uft_dfs_cat1_t *)(image + 256);
    
    /* Get current file count */
    int num_files = uft_dfs_get_file_count(cat1);
    if (num_files >= UFT_DFS_MAX_FILES) return -1;
    
    /* Parse filename (format: "D.NAME" or just "NAME") */
    char dir = '$';
    const char *name = filename;
    
    if (strlen(filename) >= 2 && filename[1] == '.') {
        dir = filename[0];
        name = filename + 2;
    }
    
    /* Calculate start sector */
    uint16_t start_sector;
    if (num_files == 0) {
        start_sector = 2;  /* First file starts at sector 2 */
    } else {
        /* Get previous file's start sector and length */
        uft_dfs_file_entry_t prev;
        uft_dfs_read_entry(cat0, cat1, 0, &prev);  /* Last added is first in catalog */
        start_sector = prev.start_sector + ((prev.length + 255) >> 8);
    }
    
    /* Check if file fits */
    uint16_t total_sectors = uft_dfs_get_sectors(cat1);
    uint16_t needed_sectors = (length + 255) >> 8;
    
    if (start_sector + needed_sectors > total_sectors) {
        return -1;  /* Not enough space */
    }
    
    /* Shift existing entries down to make room */
    for (int i = 255 - 8; i >= 8; i--) {
        image[i + 8] = image[i];
        image[256 + i + 8] = image[256 + i];
    }
    
    /* Add filename entry (sector 0) */
    uint8_t *name_entry = cat0->entries;
    memset(name_entry, ' ', 7);
    size_t name_len = strlen(name);
    if (name_len > 7) name_len = 7;
    memcpy(name_entry, name, name_len);
    name_entry[7] = dir;  /* Directory letter, bit 7 clear = unlocked */
    
    /* Add info entry (sector 1) */
    uint8_t *info_entry = cat1->info;
    
    /* Load address */
    info_entry[0] = load_addr & 0xFF;
    info_entry[1] = (load_addr >> 8) & 0xFF;
    
    /* Exec address */
    info_entry[2] = exec_addr & 0xFF;
    info_entry[3] = (exec_addr >> 8) & 0xFF;
    
    /* Length */
    info_entry[4] = length & 0xFF;
    info_entry[5] = (length >> 8) & 0xFF;
    
    /* Mixed bits */
    info_entry[6] = UFT_DFS_MAKE_MIXED(start_sector, load_addr, length, exec_addr);
    
    /* Start sector */
    info_entry[7] = start_sector & 0xFF;
    
    /* Update file count */
    cat1->num_entries += 8;
    
    /* Copy file data */
    if (data && length > 0) {
        size_t offset = start_sector * UFT_DFS_SECTOR_SIZE;
        if (offset + length <= image_size) {
            memcpy(image + offset, data, length);
        }
    }
    
    return 0;
}

int uft_dfs_extract_file(const uint8_t *image, size_t image_size,
                         const uft_dfs_file_entry_t *entry,
                         uint8_t *buffer, size_t buffer_size)
{
    if (!image || !entry || !buffer) return -1;
    
    size_t offset = entry->start_sector * UFT_DFS_SECTOR_SIZE;
    size_t length = entry->length;
    
    if (offset + length > image_size) {
        length = image_size - offset;
    }
    
    if (length > buffer_size) {
        length = buffer_size;
    }
    
    memcpy(buffer, image + offset, length);
    return (int)length;
}

/*===========================================================================
 * Format Detection
 *===========================================================================*/

const char *uft_bbc_detect_format(const uint8_t *data, size_t size)
{
    if (!data || size < 512) {
        return "Unknown";
    }
    
    /* Check for DFS format */
    if (uft_dfs_is_valid(data, size)) {
        uint16_t sectors = uft_dfs_get_sectors((const uft_dfs_cat1_t *)(data + 256));
        
        if (size == UFT_DFS_SS40_SIZE && sectors == 400) {
            return "DFS SS/40 (SSD)";
        } else if (size == UFT_DFS_SS80_SIZE && sectors == 800) {
            return "DFS SS/80 (SSD)";
        } else if (size == UFT_DFS_DS40_SIZE) {
            return "DFS DS/40 (DSD)";
        } else if (size == UFT_DFS_DS80_SIZE) {
            return "DFS DS/80 (DSD)";
        }
        return "DFS (Unknown geometry)";
    }
    
    /* Check for ADFS */
    /* ADFS has different structure at start */
    if (size >= 512 && data[0] == 0x07) {
        /* ADFS old map format */
        return "ADFS (Old map)";
    }
    
    /* Check for ADFS new map (E/F/G format) */
    if (size >= 1024) {
        /* New map has "Hugo" or "Nick" at boot block */
        if (memcmp(data + 0x201, "Hugo", 4) == 0 ||
            memcmp(data + 0x201, "Nick", 4) == 0) {
            if (size <= 819200) {
                return "ADFS E format";
            } else if (size <= 1638400) {
                return "ADFS F format";
            } else {
                return "ADFS G/+ format";
            }
        }
    }
    
    return "Unknown BBC format";
}

/*===========================================================================
 * Tape Block Handling
 *===========================================================================*/

/**
 * @brief Parse tape block from raw data
 * @param data Raw tape data (after sync byte)
 * @param size Data size
 * @param block Output block structure
 * @return Bytes consumed, or -1 on error
 */
int uft_bbc_parse_tape_block(const uint8_t *data, size_t size,
                              uft_bbc_tape_block_t *block)
{
    if (!data || !block || size < 1) return -1;
    
    /* Clear block */
    memset(block, 0, sizeof(*block));
    
    /* Read filename (null-terminated, max 10 chars) */
    size_t pos = 0;
    int name_len = 0;
    
    while (pos < size && pos < 10 && data[pos] != 0) {
        block->filename[name_len++] = data[pos++];
    }
    block->filename[name_len] = '\0';
    
    if (pos >= size || data[pos] != 0) {
        return -1;  /* Filename too long or no null terminator */
    }
    pos++;  /* Skip null terminator */
    
    /* Need at least 19 more bytes for header */
    if (pos + 19 > size) return -1;
    
    /* Load address (32-bit little-endian) */
    block->load_addr = data[pos] | (data[pos+1] << 8) |
                       (data[pos+2] << 16) | (data[pos+3] << 24);
    pos += 4;
    
    /* Exec address (32-bit little-endian) */
    block->exec_addr = data[pos] | (data[pos+1] << 8) |
                       (data[pos+2] << 16) | (data[pos+3] << 24);
    pos += 4;
    
    /* Block number (16-bit little-endian) */
    block->block_num = data[pos] | (data[pos+1] << 8);
    pos += 2;
    
    /* Block length (16-bit little-endian) */
    block->length = data[pos] | (data[pos+1] << 8);
    pos += 2;
    
    /* Flags */
    block->flags = data[pos++];
    
    /* Spare bytes */
    memcpy(block->spare, data + pos, 4);
    pos += 4;
    
    /* Header CRC (16-bit big-endian) */
    block->header_crc = (data[pos] << 8) | data[pos+1];
    pos += 2;
    
    /* Data (if any) */
    if (block->length > 0) {
        if (pos + block->length + 2 > size) {
            return -1;  /* Not enough data */
        }
        
        block->data = (uint8_t *)&data[pos];
        pos += block->length;
        
        /* Data CRC (16-bit big-endian) */
        block->data_crc = (data[pos] << 8) | data[pos+1];
        pos += 2;
        
        /* Verify data CRC */
        uint16_t calc_crc = uft_bbc_crc16(block->data, block->length);
        block->valid = (calc_crc == block->data_crc);
    } else {
        block->valid = true;
    }
    
    return (int)pos;
}

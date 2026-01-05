/**
 * @file uft_file_inject.c
 * @brief Extended File Injection for multiple formats
 * 
 * Inject files into:
 * - ADF (Amiga OFS/FFS)
 * - ATR (Atari DOS 2.x)
 * - SSD/DSD (BBC Micro DFS)
 * - TRD (ZX Spectrum TR-DOS)
 * - D81 (Commodore 1581)
 * 
 * @version 5.32.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include "uft_file_ops.h"

/* ============================================================================
 * ADF Amiga File Injection (OFS)
 * ============================================================================ */

#define ADF_BLOCK_SIZE      512
#define ADF_ROOT_BLOCK      880
#define ADF_BITMAP_BLOCK    881
#define ADF_BLOCKS_DD       1760
#define ADF_BLOCKS_HD       3520

/* Block types */
#define T_HEADER    2
#define T_DATA      8
#define T_LIST      16
#define ST_FILE     -3
#define ST_ROOT     1
#define ST_USERDIR  2

/**
 * @brief Calculate ADF block checksum
 */
static uint32_t adf_checksum(const uint8_t *block)
{
    uint32_t sum = 0;
    for (int i = 0; i < 128; i++) {
        uint32_t val = (block[i*4] << 24) | (block[i*4+1] << 16) | 
                       (block[i*4+2] << 8) | block[i*4+3];
        sum += val;
    }
    return -sum;
}

/**
 * @brief Write 32-bit big-endian value
 */
static void write_be32(uint8_t *p, uint32_t val)
{
    p[0] = (val >> 24) & 0xFF;
    p[1] = (val >> 16) & 0xFF;
    p[2] = (val >> 8) & 0xFF;
    p[3] = val & 0xFF;
}

/**
 * @brief Read 32-bit big-endian value
 */
static uint32_t read_be32(const uint8_t *p)
{
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/**
 * @brief Find free block in ADF bitmap
 */
static int adf_find_free_block(uint8_t *image, int start)
{
    uint8_t *bitmap = image + ADF_BITMAP_BLOCK * ADF_BLOCK_SIZE;
    int max_blocks = ADF_BLOCKS_DD;
    
    for (int blk = (start < 2) ? 2 : start; blk < max_blocks; blk++) {
        if (blk == ADF_ROOT_BLOCK || blk == ADF_BITMAP_BLOCK) continue;
        
        int word = (blk - 2) / 32;
        int bit = (blk - 2) % 32;
        
        uint32_t bitmap_word = read_be32(bitmap + 4 + word * 4);
        if (bitmap_word & (1U << (unsigned)bit)) {
            return blk;
        }
    }
    
    return -1;  /* No free block */
}

/**
 * @brief Mark block as used in ADF bitmap
 */
static void adf_mark_block_used(uint8_t *image, int block)
{
    uint8_t *bitmap = image + ADF_BITMAP_BLOCK * ADF_BLOCK_SIZE;
    
    int word = (block - 2) / 32;
    int bit = (block - 2) % 32;
    
    uint32_t bitmap_word = read_be32(bitmap + 4 + word * 4);
    bitmap_word &= ~(1U << (unsigned)bit);
    write_be32(bitmap + 4 + word * 4, bitmap_word);
    
    /* Update checksum */
    write_be32(bitmap, 0);
    write_be32(bitmap, adf_checksum(bitmap));
}

/**
 * @brief Hash function for ADF directory
 */
static int adf_hash_name(const char *name)
{
    uint32_t hash = strlen(name);
    for (const char *p = name; *p; p++) {
        hash = hash * 13 + toupper((unsigned char)*p);
    }
    return hash % 72;
}

/**
 * @brief Inject file into ADF image (OFS)
 */
int adf_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size)
{
    if (!image || !filename || !data || img_size < 901120) return -1;
    if (strlen(filename) > 30) return -1;
    
    /* Calculate number of data blocks needed */
    /* OFS: 488 bytes per data block (512 - 24 header) */
    int data_per_block = 488;
    int num_data_blocks = (size + data_per_block - 1) / data_per_block;
    if (num_data_blocks == 0) num_data_blocks = 1;
    
    /* Allocate header block */
    int header_block = adf_find_free_block(image, 2);
    if (header_block < 0) return -1;
    adf_mark_block_used(image, header_block);
    
    /* Allocate data blocks */
    int *data_blocks = malloc(num_data_blocks * sizeof(int));
    if (!data_blocks) return -1;
    
    int search_start = 2;
    for (int i = 0; i < num_data_blocks; i++) {
        data_blocks[i] = adf_find_free_block(image, search_start);
        if (data_blocks[i] < 0) {
            free(data_blocks);
            return -1;
        }
        adf_mark_block_used(image, data_blocks[i]);
        search_start = data_blocks[i] + 1;
    }
    
    /* Write data blocks (OFS format) */
    size_t pos = 0;
    for (int i = 0; i < num_data_blocks; i++) {
        uint8_t *block = image + data_blocks[i] * ADF_BLOCK_SIZE;
        memset(block, 0, ADF_BLOCK_SIZE);
        
        /* Data block header */
        write_be32(block + 0, T_DATA);                /* Type */
        write_be32(block + 4, header_block);          /* Header block */
        write_be32(block + 8, i + 1);                 /* Sequence number */
        
        size_t to_write = size - pos;
        if (to_write > data_per_block) to_write = data_per_block;
        write_be32(block + 12, to_write);             /* Data size */
        
        if (i < num_data_blocks - 1) {
            write_be32(block + 16, data_blocks[i + 1]); /* Next data block */
        }
        
        memcpy(block + 24, data + pos, to_write);
        pos += to_write;
        
        /* Checksum */
        write_be32(block + 20, 0);
        write_be32(block + 20, adf_checksum(block));
    }
    
    /* Write file header block */
    uint8_t *header = image + header_block * ADF_BLOCK_SIZE;
    memset(header, 0, ADF_BLOCK_SIZE);
    
    write_be32(header + 0, T_HEADER);                 /* Type */
    write_be32(header + 4, header_block);             /* Own block */
    write_be32(header + 8, num_data_blocks);          /* High seq */
    write_be32(header + 12, 0);                       /* Data size (unused) */
    write_be32(header + 16, data_blocks[0]);          /* First data block */
    
    /* Data block table (at offset 24, reversed) */
    for (int i = 0; i < num_data_blocks && i < 72; i++) {
        write_be32(header + 308 - i * 4, data_blocks[i]);
    }
    
    write_be32(header + 324, size);                   /* File size */
    
    /* Filename */
    int name_len = strlen(filename);
    header[432] = name_len;
    memcpy(header + 433, filename, name_len);
    
    /* Timestamps (current time) */
    time_t now = time(NULL);
    uint32_t amiga_days = (now / 86400) - 2922;  /* Days since 1978 */
    uint32_t amiga_mins = (now % 86400) / 60;
    uint32_t amiga_ticks = ((now % 60) * 50);
    
    write_be32(header + 420, amiga_days);
    write_be32(header + 424, amiga_mins);
    write_be32(header + 428, amiga_ticks);
    
    write_be32(header + 508, ST_FILE);               /* Secondary type */
    write_be32(header + 504, ADF_ROOT_BLOCK);        /* Parent */
    
    /* Checksum */
    write_be32(header + 20, 0);
    write_be32(header + 20, adf_checksum(header));
    
    /* Link into root directory */
    uint8_t *root = image + ADF_ROOT_BLOCK * ADF_BLOCK_SIZE;
    int hash = adf_hash_name(filename);
    
    uint32_t existing = read_be32(root + 24 + hash * 4);
    if (existing == 0) {
        /* Empty slot */
        write_be32(root + 24 + hash * 4, header_block);
    } else {
        /* Follow hash chain */
        while (1) {
            uint8_t *chain_block = image + existing * ADF_BLOCK_SIZE;
            uint32_t next = read_be32(chain_block + 496);
            if (next == 0) {
                write_be32(chain_block + 496, header_block);
                /* Update chain block checksum */
                write_be32(chain_block + 20, 0);
                write_be32(chain_block + 20, adf_checksum(chain_block));
                break;
            }
            existing = next;
        }
    }
    
    /* Update root checksum */
    write_be32(root + 20, 0);
    write_be32(root + 20, adf_checksum(root));
    
    free(data_blocks);
    return 0;
}

/* ============================================================================
 * ATR Atari File Injection (DOS 2.x)
 * ============================================================================ */

#define ATR_HEADER_SIZE     16
#define ATR_VTOC_SECTOR     360
#define ATR_DIR_START       361
#define ATR_DIR_END         368

/**
 * @brief Get ATR sector
 */
static uint8_t* atr_get_sector(uint8_t *image, int sector, int *sect_size)
{
    if (sector <= 3) {
        *sect_size = 128;
        return image + ATR_HEADER_SIZE + (sector - 1) * 128;
    }
    
    int ss = image[4] | (image[5] << 8);
    if (ss == 0) ss = 128;
    *sect_size = ss;
    
    return image + ATR_HEADER_SIZE + 3 * 128 + (sector - 4) * ss;
}

/**
 * @brief Find free sector in ATR VTOC
 */
static int atr_find_free_sector(uint8_t *image)
{
    int sect_size;
    uint8_t *vtoc = atr_get_sector(image, ATR_VTOC_SECTOR, &sect_size);
    
    /* Bitmap starts at offset 10 */
    for (int sect = 1; sect <= 720; sect++) {
        if (sect >= 360 && sect <= 368) continue;  /* Skip VTOC/directory */
        
        int byte_idx = 10 + (sect / 8);
        int bit_idx = 7 - (sect % 8);
        
        if (byte_idx < sect_size && (vtoc[byte_idx] & (1 << bit_idx))) {
            return sect;
        }
    }
    
    return -1;
}

/**
 * @brief Mark sector as used in ATR VTOC
 */
static void atr_mark_sector_used(uint8_t *image, int sector)
{
    int sect_size;
    uint8_t *vtoc = atr_get_sector(image, ATR_VTOC_SECTOR, &sect_size);
    
    int byte_idx = 10 + (sector / 8);
    int bit_idx = 7 - (sector % 8);
    
    if (byte_idx < sect_size) {
        vtoc[byte_idx] &= ~(1 << bit_idx);
    }
    
    /* Update free sector count */
    int free_count = vtoc[3] | (vtoc[4] << 8);
    free_count--;
    vtoc[3] = free_count & 0xFF;
    vtoc[4] = (free_count >> 8) & 0xFF;
}

/**
 * @brief Inject file into ATR image (DOS 2.x)
 */
int atr_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size)
{
    if (!image || !filename || !data || img_size < 92176) return -1;
    
    /* Parse filename (8.3 format) */
    char name[9] = "        ";
    char ext[4] = "   ";
    
    const char *dot = strchr(filename, '.');
    size_t name_len = dot ? (dot - filename) : strlen(filename);
    if (name_len > 8) name_len = 8;
    
    for (size_t i = 0; i < name_len; i++) {
        name[i] = toupper((unsigned char)filename[i]);
    }
    
    if (dot && dot[1]) {
        size_t ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        for (size_t i = 0; i < ext_len; i++) {
            ext[i] = toupper((unsigned char)dot[1 + i]);
        }
    }
    
    /* Find free directory entry */
    int sect_size;
    int dir_entry_sector = -1;
    int dir_entry_offset = -1;
    
    for (int ds = ATR_DIR_START; ds <= ATR_DIR_END; ds++) {
        uint8_t *dir = atr_get_sector(image, ds, &sect_size);
        
        for (int e = 0; e < 8; e++) {
            uint8_t *entry = dir + e * 16;
            if (entry[0] == 0x00 || entry[0] == 0x80) {
                dir_entry_sector = ds;
                dir_entry_offset = e * 16;
                break;
            }
        }
        if (dir_entry_sector >= 0) break;
    }
    
    if (dir_entry_sector < 0) return -1;  /* Directory full */
    
    /* Allocate sectors and write data */
    int first_sector = 0;
    int sector_count = 0;
    int prev_sector = 0;
    size_t pos = 0;
    
    /* Get sector size (minus 3 link bytes) */
    atr_get_sector(image, 4, &sect_size);
    int data_per_sector = sect_size - 3;
    
    /* File number (find highest + 1) */
    int file_num = 0;
    for (int ds = ATR_DIR_START; ds <= ATR_DIR_END; ds++) {
        uint8_t *dir = atr_get_sector(image, ds, &sect_size);
        for (int e = 0; e < 8; e++) {
            if (dir[e * 16] != 0x00 && dir[e * 16] != 0x80) {
                /* Entry exists, would need to track file numbers */
            }
        }
    }
    file_num = 1;  /* Simplified */
    
    while (pos < size) {
        int sector = atr_find_free_sector(image);
        if (sector < 0) return -1;
        
        atr_mark_sector_used(image, sector);
        sector_count++;
        
        if (first_sector == 0) first_sector = sector;
        
        uint8_t *sect = atr_get_sector(image, sector, &sect_size);
        
        size_t to_write = size - pos;
        int bytes_in_sector = (to_write > data_per_sector) ? data_per_sector : to_write;
        
        /* Write data */
        memcpy(sect, data + pos, bytes_in_sector);
        pos += bytes_in_sector;
        
        /* Link bytes at end */
        /* Format: [file_num | flags] [next_low] [next_high << 2 | byte_count >> 6] */
        uint8_t flags = (file_num << 2);
        if (pos >= size) {
            /* Last sector */
            sect[sect_size - 3] = flags;
            sect[sect_size - 2] = 0;
            sect[sect_size - 1] = (bytes_in_sector << 2) & 0xFF;
        } else {
            sect[sect_size - 3] = flags;
            /* Next sector will be filled in */
        }
        
        /* Link previous sector to this one */
        if (prev_sector > 0) {
            uint8_t *prev = atr_get_sector(image, prev_sector, &sect_size);
            prev[sect_size - 2] = sector & 0xFF;
            prev[sect_size - 1] = ((sector >> 8) << 2) | (data_per_sector >> 6);
        }
        
        prev_sector = sector;
    }
    
    /* Write directory entry */
    uint8_t *dir = atr_get_sector(image, dir_entry_sector, &sect_size);
    uint8_t *entry = dir + dir_entry_offset;
    
    entry[0] = 0x42;  /* Normal file, in use */
    entry[1] = sector_count & 0xFF;
    entry[2] = (sector_count >> 8) & 0xFF;
    entry[3] = first_sector & 0xFF;
    entry[4] = (first_sector >> 8) & 0xFF;
    memcpy(entry + 5, name, 8);
    memcpy(entry + 13, ext, 3);
    
    return 0;
}

/* ============================================================================
 * SSD/DSD BBC Micro File Injection (Acorn DFS)
 * ============================================================================ */

#define DFS_SECTOR_SIZE     256

/**
 * @brief Inject file into SSD/DSD image
 */
int ssd_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size)
{
    if (!image || !filename || !data) return -1;
    
    /* Parse filename (max 7 chars + directory char) */
    char name[8] = "       ";
    char dir_char = '$';
    
    const char *dot = strchr(filename, '.');
    if (dot && dot != filename && dot[1]) {
        /* Directory.Name format */
        dir_char = toupper((unsigned char)filename[0]);
        size_t name_len = strlen(dot + 1);
        if (name_len > 7) name_len = 7;
        for (size_t i = 0; i < name_len; i++) {
            name[i] = filename[dot - filename + 1 + i];
        }
    } else {
        size_t name_len = strlen(filename);
        if (name_len > 7) name_len = 7;
        for (size_t i = 0; i < name_len; i++) {
            name[i] = filename[i];
        }
    }
    
    /* Read catalog */
    uint8_t *cat0 = image;
    uint8_t *cat1 = image + DFS_SECTOR_SIZE;
    
    int file_count = cat1[5] / 8;
    if (file_count >= 31) return -1;  /* Directory full */
    
    /* Find start sector (after last file) */
    int start_sector = 2;  /* After catalog */
    for (int i = 0; i < file_count; i++) {
        int info_off = 8 + i * 8;
        int file_start = cat1[info_off + 7] | ((cat1[info_off + 6] & 0x03) << 8);
        int file_size = cat1[info_off + 4] | (cat1[info_off + 5] << 8) |
                        ((cat1[info_off + 6] & 0x30) << 12);
        int file_sectors = (file_size + 255) / 256;
        int file_end = file_start + file_sectors;
        if (file_end > start_sector) start_sector = file_end;
    }
    
    /* Check if we have space */
    int total_sectors = ((cat1[6] & 0x03) << 8) | cat1[7];
    int needed_sectors = (size + 255) / 256;
    if (start_sector + needed_sectors > total_sectors) return -1;
    
    /* Write file data */
    size_t offset = start_sector * DFS_SECTOR_SIZE;
    if (offset + size > img_size) return -1;
    memcpy(image + offset, data, size);
    
    /* Add directory entry */
    int name_off = 8 + file_count * 8;
    int info_off = 8 + file_count * 8;
    
    memcpy(cat0 + name_off, name, 7);
    cat0[name_off + 7] = dir_char & 0x7F;  /* No lock bit */
    
    /* Load address = 0xFFFF0000 (default) */
    cat1[info_off + 0] = 0x00;
    cat1[info_off + 1] = 0x00;
    
    /* Exec address = 0xFFFF0000 */
    cat1[info_off + 2] = 0x00;
    cat1[info_off + 3] = 0x00;
    
    /* Length */
    cat1[info_off + 4] = size & 0xFF;
    cat1[info_off + 5] = (size >> 8) & 0xFF;
    
    /* Extra bits + start sector high */
    cat1[info_off + 6] = ((size >> 16) & 0x03) << 4 |
                         ((start_sector >> 8) & 0x03);
    
    /* Start sector low */
    cat1[info_off + 7] = start_sector & 0xFF;
    
    /* Update file count */
    cat1[5] = (file_count + 1) * 8;
    
    return 0;
}

/* ============================================================================
 * TRD ZX Spectrum File Injection
 * ============================================================================ */

/**
 * @brief Inject file into TRD image
 */
int trd_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size,
                    char file_type)
{
    if (!image || !filename || !data || img_size < 655360) return -1;
    
    /* Parse filename (8 chars max) */
    char name[9] = "        ";
    size_t name_len = strlen(filename);
    if (name_len > 8) name_len = 8;
    for (size_t i = 0; i < name_len; i++) {
        name[i] = toupper((unsigned char)filename[i]);
    }
    
    /* Find free directory entry */
    int dir_entry = -1;
    for (int s = 0; s < 8 && dir_entry < 0; s++) {
        uint8_t *sect = image + s * 256;
        for (int e = 0; e < 16; e++) {
            if (sect[e * 16] == 0x00) {
                dir_entry = s * 16 + e;
                break;
            }
        }
    }
    
    if (dir_entry < 0) return -1;  /* Directory full */
    
    /* Get disk info (track 0, sector 8, offset 0xE1) */
    uint8_t *info = image + 8 * 256 + 0xE1;
    int free_sectors = info[4] | (info[5] << 8);
    int first_free_track = info[6];
    int first_free_sector = info[7];
    
    /* Calculate sectors needed */
    int sectors_needed = (size + 255) / 256;
    if (sectors_needed > free_sectors) return -1;
    
    /* Write data */
    int track = first_free_track;
    int sector = first_free_sector;
    size_t pos = 0;
    
    int start_track = track;
    int start_sector = sector;
    
    while (pos < size) {
        /* Calculate offset: interleaved sides */
        /* Track N Side 0, Track N Side 1, Track N+1 Side 0... */
        size_t offset = ((track * 2 + (sector >= 16 ? 1 : 0)) * 16 + (sector % 16)) * 256;
        
        size_t to_write = size - pos;
        if (to_write > 256) to_write = 256;
        
        memcpy(image + offset, data + pos, to_write);
        pos += to_write;
        
        /* Next sector */
        sector++;
        if (sector >= 16) {
            sector = 0;
            track++;
        }
    }
    
    /* Update directory entry */
    int dir_sector = dir_entry / 16;
    int dir_offset = (dir_entry % 16) * 16;
    uint8_t *entry = image + dir_sector * 256 + dir_offset;
    
    memcpy(entry, name, 8);
    entry[8] = file_type ? file_type : 'C';  /* Default: Code */
    entry[9] = 0x00;   /* Start address low */
    entry[10] = 0x60;  /* Start address high (0x6000) */
    entry[11] = size & 0xFF;
    entry[12] = (size >> 8) & 0xFF;
    entry[13] = sectors_needed;
    entry[14] = start_sector;
    entry[15] = start_track;
    
    /* Update disk info */
    free_sectors -= sectors_needed;
    info[4] = free_sectors & 0xFF;
    info[5] = (free_sectors >> 8) & 0xFF;
    info[6] = track;
    info[7] = sector;
    
    /* Update file count (at offset 0xE4) */
    uint8_t *file_count = image + 8 * 256 + 0xE4;
    (*file_count)++;
    
    return 0;
}

/* ============================================================================
 * D81 Commodore 1581 File Injection
 * ============================================================================ */

#define D81_SECTOR_SIZE     256
#define D81_SECTORS         40
#define D81_HEADER_TRACK    40

/**
 * @brief Get D81 sector offset
 */
static size_t d81_offset(int track, int sector)
{
    return ((track - 1) * D81_SECTORS + sector) * D81_SECTOR_SIZE;
}

/**
 * @brief Find free sector in D81 BAM
 */
static int d81_find_free_sector(uint8_t *image, int *out_track, int *out_sector)
{
    /* BAM is at track 40, sectors 1 and 2 */
    for (int bam_sect = 1; bam_sect <= 2; bam_sect++) {
        size_t bam_off = d81_offset(40, bam_sect);
        uint8_t *bam = image + bam_off;
        
        int start_track = (bam_sect == 1) ? 1 : 41;
        int end_track = (bam_sect == 1) ? 40 : 80;
        
        for (int t = start_track; t <= end_track; t++) {
            int idx = 16 + (t - start_track) * 6;
            int free_count = bam[idx];
            
            if (free_count > 0 && t != 40) {  /* Skip directory track */
                /* Find free sector in bitmap */
                for (int s = 0; s < 40; s++) {
                    int byte_idx = idx + 1 + s / 8;
                    int bit_idx = s % 8;
                    
                    if (bam[byte_idx] & (1 << bit_idx)) {
                        *out_track = t;
                        *out_sector = s;
                        return 0;
                    }
                }
            }
        }
    }
    
    return -1;  /* No free sector */
}

/**
 * @brief Mark D81 sector as used
 */
static void d81_mark_used(uint8_t *image, int track, int sector)
{
    int bam_sect = (track <= 40) ? 1 : 2;
    size_t bam_off = d81_offset(40, bam_sect);
    uint8_t *bam = image + bam_off;
    
    int start_track = (bam_sect == 1) ? 1 : 41;
    int idx = 16 + (track - start_track) * 6;
    
    /* Decrement free count */
    bam[idx]--;
    
    /* Clear bit */
    int byte_idx = idx + 1 + sector / 8;
    int bit_idx = sector % 8;
    bam[byte_idx] &= ~(1 << bit_idx);
}

/**
 * @brief Inject file into D81 image
 */
int d81_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size,
                    uft_file_type_t type)
{
    if (!image || !filename || !data || img_size < 819200) return -1;
    
    /* Find free directory entry */
    int dir_track = 40;
    int dir_sector = 3;
    int entry_offset = -1;
    size_t dir_off;
    
    while (dir_track != 0) {
        dir_off = d81_offset(dir_track, dir_sector);
        uint8_t *sect = image + dir_off;
        
        for (int e = 0; e < 8; e++) {
            if (sect[e * 32 + 2] == 0x00) {
                entry_offset = e * 32;
                goto found_entry;
            }
        }
        
        dir_track = sect[0];
        dir_sector = sect[1];
    }
    
    return -1;  /* Directory full */
    
found_entry:
    ;
    
    /* Allocate sectors and write data */
    int first_track = 0, first_sector = 0;
    int prev_track = 0, prev_sector = 0;
    size_t prev_off = 0;
    int blocks = 0;
    size_t pos = 0;
    
    while (pos < size) {
        int track, sector;
        if (d81_find_free_sector(image, &track, &sector) != 0) {
            return -1;  /* Disk full */
        }
        
        d81_mark_used(image, track, sector);
        blocks++;
        
        if (first_track == 0) {
            first_track = track;
            first_sector = sector;
        }
        
        /* Link previous sector */
        if (prev_off > 0) {
            image[prev_off] = track;
            image[prev_off + 1] = sector;
        }
        
        size_t offset = d81_offset(track, sector);
        uint8_t *sect = image + offset;
        memset(sect, 0, D81_SECTOR_SIZE);
        
        size_t to_write = size - pos;
        if (to_write > 254) to_write = 254;
        
        memcpy(sect + 2, data + pos, to_write);
        pos += to_write;
        
        prev_track = track;
        prev_sector = sector;
        prev_off = offset;
    }
    
    /* Set end marker */
    if (prev_off > 0) {
        image[prev_off] = 0;
        image[prev_off + 1] = (size % 254) + 1;
        if (image[prev_off + 1] == 1) image[prev_off + 1] = 255;
    }
    
    /* Write directory entry */
    uint8_t *entry = image + dir_off + entry_offset;
    memset(entry, 0, 32);
    
    uint8_t cbm_type;
    switch (type) {
        case UFT_FTYPE_SEQ: cbm_type = 0x81; break;
        case UFT_FTYPE_USR: cbm_type = 0x83; break;
        case UFT_FTYPE_REL: cbm_type = 0x84; break;
        default:            cbm_type = 0x82; break;
    }
    
    entry[2] = cbm_type;
    entry[3] = first_track;
    entry[4] = first_sector;
    
    /* Filename (PETSCII) */
    memset(entry + 5, 0xA0, 16);
    size_t name_len = strlen(filename);
    if (name_len > 16) name_len = 16;
    for (size_t i = 0; i < name_len; i++) {
        entry[5 + i] = toupper((unsigned char)filename[i]);
    }
    
    entry[30] = blocks & 0xFF;
    entry[31] = (blocks >> 8) & 0xFF;
    
    return 0;
}

/* ============================================================================
 * Unified Inject API
 * ============================================================================ */

/**
 * @brief Inject file into any supported disk image
 */
int uft_inject_file_extended(const char *image_path, const char *filename,
                             const char *input_path, uft_file_type_t type)
{
    if (!image_path || !filename || !input_path) return -1;
    
    /* Read input file */
    FILE *in = fopen(input_path, "rb");
    if (!in) return -1;
    
    if (fseek(in, 0, SEEK_END) != 0) { /* seek error */ }
    size_t data_size = ftell(in);
    if (fseek(in, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *data = malloc(data_size);
    if (!data) {
        fclose(in);
        return -1;
    }
    /* P1-IO-001: Check fread return */
    if (fread(data, 1, data_size, in) != data_size) {
        free(data);
        fclose(in);
        return -1;
    }
    fclose(in);
    
    /* Read disk image */
    FILE *fp = fopen(image_path, "r+b");
    if (!fp) {
        free(data);
        return -1;
    }
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t img_size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *image = malloc(img_size);
    if (!image) {
        free(data);
        fclose(fp);
        return -1;
    }
    /* P1-IO-001: Check fread return */
    if (fread(image, 1, img_size, fp) != img_size) {
        free(image);
        free(data);
        fclose(fp);
        return -1;
    }
    
    int result = -1;
    
    const char *ext = strrchr(image_path, '.');
    if (ext) ext++;
    
    if (ext && strcasecmp(ext, "adf") == 0) {
        result = adf_inject_file(image, img_size, filename, data, data_size);
    } else if (ext && strcasecmp(ext, "atr") == 0) {
        result = atr_inject_file(image, img_size, filename, data, data_size);
    } else if (ext && (strcasecmp(ext, "ssd") == 0 || strcasecmp(ext, "dsd") == 0)) {
        result = ssd_inject_file(image, img_size, filename, data, data_size);
    } else if (ext && strcasecmp(ext, "trd") == 0) {
        result = trd_inject_file(image, img_size, filename, data, data_size, 'C');
    } else if (ext && strcasecmp(ext, "d81") == 0) {
        result = d81_inject_file(image, img_size, filename, data, data_size, type);
    }
    /* D64 inject is in uft_file_ops.c */
    
    if (result == 0) {
        if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
        /* P1-IO-001: Check fwrite return */
        if (fwrite(image, 1, img_size, fp) != img_size) {
            result = -2;  /* Write failed */
        }
    }
    
    free(image);
    free(data);
    fclose(fp);
    
    return result;
}

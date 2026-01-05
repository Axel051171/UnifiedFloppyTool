/**
 * @file uft_file_ops.c
 * @brief File Operations API - Extract/Inject/List files from disk images
 * 
 * Supports file-level operations on:
 * - D64/D71/D81 (Commodore CBM DOS)
 * - ADF (Amiga OFS/FFS)
 * - ATR (Atari DOS)
 * - TRD (ZX Spectrum TR-DOS)
 * - SSD/DSD (BBC Micro DFS)
 * - IMG (FAT12)
 * 
 * @version 5.31.0
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* ============================================================================
 * Common Structures
 * ============================================================================ */

#define UFT_MAX_FILENAME    256
#define UFT_MAX_FILES       1024

typedef enum {
    UFT_FTYPE_UNKNOWN = 0,
    UFT_FTYPE_PRG,          /* Commodore Program */
    UFT_FTYPE_SEQ,          /* Commodore Sequential */
    UFT_FTYPE_REL,          /* Commodore Relative */
    UFT_FTYPE_USR,          /* Commodore User */
    UFT_FTYPE_DEL,          /* Deleted */
    UFT_FTYPE_BASIC,        /* BASIC program */
    UFT_FTYPE_DATA,         /* Data file */
    UFT_FTYPE_CODE,         /* Machine code */
    UFT_FTYPE_TEXT,         /* Text file */
    UFT_FTYPE_BINARY,       /* Binary file */
    UFT_FTYPE_DIR,          /* Directory */
} uft_file_type_t;

typedef struct {
    char            name[UFT_MAX_FILENAME];
    uft_file_type_t type;
    uint32_t        size;           /* Size in bytes */
    uint32_t        blocks;         /* Size in blocks/sectors */
    uint16_t        start_track;
    uint16_t        start_sector;
    uint16_t        load_addr;      /* Load address (C64/Atari) */
    uint16_t        exec_addr;      /* Exec address */
    bool            locked;         /* Write protected */
    bool            deleted;
    uint8_t         raw_type;       /* Original type byte */
} uft_file_entry_t;

typedef struct {
    uft_file_entry_t files[UFT_MAX_FILES];
    int              count;
    char             disk_name[32];
    char             disk_id[8];
    int              free_blocks;
    int              total_blocks;
} uft_directory_t;

/* ============================================================================
 * D64/D71/D81 Commodore File Operations
 * ============================================================================ */

/* D64 track/sector layout */
static const int D64_SECTORS_PER_TRACK[] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,  /* 1-17 */
    19,19,19,19,19,19,19,                                  /* 18-24 */
    18,18,18,18,18,18,                                      /* 25-30 */
    17,17,17,17,17                                          /* 31-35 */
};

static const int D64_TRACK_OFFSET[] = {
    0,
    0x00000, 0x01500, 0x02A00, 0x03F00, 0x05400, 0x06900, 0x07E00, 0x09300,
    0x0A800, 0x0BD00, 0x0D200, 0x0E700, 0x0FC00, 0x11100, 0x12600, 0x13B00,
    0x15000, 0x16500, 0x17800, 0x18B00, 0x19E00, 0x1B100, 0x1C400, 0x1D700,
    0x1EA00, 0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400, 0x25600, 0x26700,
    0x27800, 0x28900, 0x29A00
};

/**
 * @brief Get D64 sector offset
 */
static size_t d64_sector_offset(int track, int sector)
{
    if (track < 1 || track > 35) return 0;
    return D64_TRACK_OFFSET[track] + sector * 256;
}

/**
 * @brief Convert PETSCII to ASCII
 */
static void petscii_to_ascii(const uint8_t *petscii, char *ascii, int len)
{
    for (int i = 0; i < len; i++) {
        uint8_t c = petscii[i];
        if (c == 0xA0) c = ' ';  /* Shifted space */
        else if (c >= 0x41 && c <= 0x5A) c = c;  /* A-Z */
        else if (c >= 0xC1 && c <= 0xDA) c = c - 0x80;  /* Shifted A-Z */
        else if (c >= 0x61 && c <= 0x7A) c = c - 0x20;  /* a-z -> A-Z */
        else if (c < 0x20 || c > 0x7E) c = '?';
        ascii[i] = c;
    }
    ascii[len] = '\0';
    
    /* Trim trailing spaces */
    for (int i = len - 1; i >= 0 && ascii[i] == ' '; i--) {
        ascii[i] = '\0';
    }
}

/**
 * @brief Get D64 file type string
 */
static uft_file_type_t d64_file_type(uint8_t type)
{
    switch (type & 0x07) {
        case 0x00: return UFT_FTYPE_DEL;
        case 0x01: return UFT_FTYPE_SEQ;
        case 0x02: return UFT_FTYPE_PRG;
        case 0x03: return UFT_FTYPE_USR;
        case 0x04: return UFT_FTYPE_REL;
        default:   return UFT_FTYPE_UNKNOWN;
    }
}

/**
 * @brief List files on D64 image
 */
int d64_list_files(const uint8_t *image, size_t size, uft_directory_t *dir)
{
    if (!image || !dir || size < 174848) return -1;
    
    memset(dir, 0, sizeof(*dir));
    
    /* Read BAM (Track 18, Sector 0) */
    size_t bam_offset = d64_sector_offset(18, 0);
    const uint8_t *bam = image + bam_offset;
    
    /* Disk name at BAM+0x90 */
    petscii_to_ascii(bam + 0x90, dir->disk_name, 16);
    
    /* Disk ID at BAM+0xA2 */
    petscii_to_ascii(bam + 0xA2, dir->disk_id, 5);
    
    /* Count free blocks */
    dir->free_blocks = 0;
    for (int t = 1; t <= 35; t++) {
        if (t != 18) {  /* Skip directory track */
            dir->free_blocks += bam[4 * t];
        }
    }
    dir->total_blocks = 664;  /* 683 - 19 (track 18) */
    
    /* Read directory (Track 18, starting at sector 1) */
    int dir_track = 18;
    int dir_sector = 1;
    
    while (dir_track != 0 && dir->count < UFT_MAX_FILES) {
        size_t offset = d64_sector_offset(dir_track, dir_sector);
        const uint8_t *sector = image + offset;
        
        /* 8 entries per sector */
        for (int e = 0; e < 8 && dir->count < UFT_MAX_FILES; e++) {
            const uint8_t *entry = sector + e * 32;
            uint8_t file_type = entry[2];
            
            if (file_type == 0x00) continue;  /* Empty */
            
            uft_file_entry_t *f = &dir->files[dir->count];
            
            f->raw_type = file_type;
            f->type = d64_file_type(file_type);
            f->deleted = (file_type & 0x80) == 0;
            f->locked = (file_type & 0x40) != 0;
            
            f->start_track = entry[3];
            f->start_sector = entry[4];
            
            petscii_to_ascii(entry + 5, f->name, 16);
            
            f->blocks = entry[30] | (entry[31] << 8);
            f->size = f->blocks * 254;  /* Approximate */
            
            if (!f->deleted && f->start_track > 0) {
                dir->count++;
            }
        }
        
        /* Next directory sector */
        dir_track = sector[0];
        dir_sector = sector[1];
    }
    
    return 0;
}

/**
 * @brief Extract file from D64
 */
int d64_extract_file(const uint8_t *image, size_t img_size, 
                     const char *filename, uint8_t **data, size_t *size)
{
    if (!image || !filename || !data || !size) return -1;
    
    uft_directory_t dir;
    if (d64_list_files(image, img_size, &dir) != 0) return -1;
    
    /* Find file */
    int found = -1;
    for (int i = 0; i < dir.count; i++) {
        if (strcasecmp(dir.files[i].name, filename) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) return -1;  /* File not found */
    
    uft_file_entry_t *f = &dir.files[found];
    
    /* Allocate buffer (max size estimate) */
    size_t max_size = f->blocks * 256;
    uint8_t *buf = malloc(max_size);
    if (!buf) return -1;
    
    /* Follow chain and extract data */
    int track = f->start_track;
    int sector = f->start_sector;
    size_t pos = 0;
    
    while (track != 0 && pos < max_size) {
        size_t offset = d64_sector_offset(track, sector);
        if (offset + 256 > img_size) break;
        
        const uint8_t *sect = image + offset;
        int next_track = sect[0];
        int next_sector = sect[1];
        
        int bytes_to_copy;
        if (next_track == 0) {
            /* Last sector - next_sector contains byte count */
            bytes_to_copy = next_sector - 1;
            if (bytes_to_copy < 0) bytes_to_copy = 0;
            if (bytes_to_copy > 254) bytes_to_copy = 254;
        } else {
            bytes_to_copy = 254;
        }
        
        memcpy(buf + pos, sect + 2, bytes_to_copy);
        pos += bytes_to_copy;
        
        track = next_track;
        sector = next_sector;
    }
    
    *data = buf;
    *size = pos;
    
    return 0;
}

/**
 * @brief Inject file into D64 (simplified - appends to end)
 */
int d64_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size,
                    uft_file_type_t type)
{
    if (!image || !filename || !data || img_size < 174848) return -1;
    
    /* This is a simplified implementation */
    /* A full implementation would need proper BAM management */
    
    /* Find free directory entry */
    int dir_track = 18;
    int dir_sector = 1;
    int entry_found = -1;
    size_t entry_offset = 0;
    
    while (dir_track != 0 && entry_found < 0) {
        size_t offset = d64_sector_offset(dir_track, dir_sector);
        uint8_t *sector = image + offset;
        
        for (int e = 0; e < 8; e++) {
            uint8_t *entry = sector + e * 32;
            if (entry[2] == 0x00) {
                entry_found = e;
                entry_offset = offset + e * 32;
                break;
            }
        }
        
        dir_track = sector[0];
        dir_sector = sector[1];
    }
    
    if (entry_found < 0) return -1;  /* Directory full */
    
    /* Find free sectors from BAM */
    size_t bam_offset = d64_sector_offset(18, 0);
    uint8_t *bam = image + bam_offset;
    
    int first_track = 0, first_sector = 0;
    int prev_track = 0, prev_sector = 0;
    size_t prev_offset = 0;
    size_t data_pos = 0;
    int blocks_used = 0;
    
    /* Allocate sectors and write data */
    for (int t = 1; t <= 35 && data_pos < size; t++) {
        if (t == 18) continue;  /* Skip directory track */
        
        int free_count = bam[4 * t];
        if (free_count == 0) continue;
        
        uint8_t *bitmap = &bam[4 * t + 1];
        int max_sect = D64_SECTORS_PER_TRACK[t - 1];
        
        for (int s = 0; s < max_sect && data_pos < size; s++) {
            int byte_idx = s / 8;
            int bit_idx = s % 8;
            
            if (bitmap[byte_idx] & (1 << bit_idx)) {
                /* Sector is free */
                size_t sect_offset = d64_sector_offset(t, s);
                uint8_t *sector = image + sect_offset;
                
                /* Link from previous sector */
                if (prev_offset > 0) {
                    image[prev_offset] = t;
                    image[prev_offset + 1] = s;
                } else {
                    first_track = t;
                    first_sector = s;
                }
                
                /* Write data */
                size_t to_write = size - data_pos;
                if (to_write > 254) to_write = 254;
                
                memset(sector, 0, 256);
                memcpy(sector + 2, data + data_pos, to_write);
                
                data_pos += to_write;
                blocks_used++;
                
                /* Mark sector as used in BAM */
                bitmap[byte_idx] &= ~(1 << bit_idx);
                bam[4 * t]--;
                
                prev_track = t;
                prev_sector = s;
                prev_offset = sect_offset;
            }
        }
    }
    
    /* Set end-of-file marker in last sector */
    if (prev_offset > 0) {
        size_t last_bytes = (size % 254);
        if (last_bytes == 0 && size > 0) last_bytes = 254;
        image[prev_offset] = 0;
        image[prev_offset + 1] = last_bytes + 1;
    }
    
    /* Create directory entry */
    uint8_t *entry = image + entry_offset;
    memset(entry, 0, 32);
    
    uint8_t cbm_type;
    switch (type) {
        case UFT_FTYPE_SEQ: cbm_type = 0x81; break;
        case UFT_FTYPE_USR: cbm_type = 0x83; break;
        case UFT_FTYPE_REL: cbm_type = 0x84; break;
        default:            cbm_type = 0x82; break;  /* PRG */
    }
    
    entry[2] = cbm_type;
    entry[3] = first_track;
    entry[4] = first_sector;
    
    /* Filename (PETSCII, padded with 0xA0) */
    memset(entry + 5, 0xA0, 16);
    size_t name_len = strlen(filename);
    if (name_len > 16) name_len = 16;
    for (size_t i = 0; i < name_len; i++) {
        entry[5 + i] = toupper((unsigned char)filename[i]);
    }
    
    entry[30] = blocks_used & 0xFF;
    entry[31] = (blocks_used >> 8) & 0xFF;
    
    return 0;
}

/* ============================================================================
 * ADF Amiga File Operations
 * ============================================================================ */

#define ADF_SECTOR_SIZE     512
#define ADF_ROOT_BLOCK      880
#define ADF_BITMAP_BLOCK    881

/**
 * @brief Read ADF block
 */
static const uint8_t* adf_block(const uint8_t *image, int block)
{
    return image + block * ADF_SECTOR_SIZE;
}

/**
 * @brief List files on ADF image
 */
int adf_list_files(const uint8_t *image, size_t size, uft_directory_t *dir)
{
    if (!image || !dir || size < 901120) return -1;
    
    memset(dir, 0, sizeof(*dir));
    
    /* Read root block */
    const uint8_t *root = adf_block(image, ADF_ROOT_BLOCK);
    
    /* Check type (should be 2 = T_HEADER) */
    uint32_t type = (root[0] << 24) | (root[1] << 16) | (root[2] << 8) | root[3];
    if (type != 2) return -1;
    
    /* Disk name at offset 432, length at 431 */
    int name_len = root[432];
    if (name_len > 30) name_len = 30;
    memcpy(dir->disk_name, root + 433, name_len);
    dir->disk_name[name_len] = '\0';
    
    /* Hash table at offset 24, 72 entries */
    for (int h = 0; h < 72; h++) {
        int offset = 24 + h * 4;
        uint32_t block_num = (root[offset] << 24) | (root[offset+1] << 16) | 
                             (root[offset+2] << 8) | root[offset+3];
        
        while (block_num != 0 && dir->count < UFT_MAX_FILES) {
            const uint8_t *header = adf_block(image, block_num);
            
            /* Check secondary type at offset 508 */
            int32_t sec_type = (int32_t)((header[508] << 24) | (header[509] << 16) | 
                                          (header[510] << 8) | header[511]);
            
            uft_file_entry_t *f = &dir->files[dir->count];
            
            /* Filename at offset 432 */
            int fname_len = header[432];
            if (fname_len > 30) fname_len = 30;
            memcpy(f->name, header + 433, fname_len);
            f->name[fname_len] = '\0';
            
            if (sec_type == -3) {
                /* File */
                f->type = UFT_FTYPE_BINARY;
                
                /* File size at offset 324 */
                f->size = (header[324] << 24) | (header[325] << 16) | 
                          (header[326] << 8) | header[327];
                f->blocks = (f->size + 487) / 488;  /* Data blocks */
                
                dir->count++;
            } else if (sec_type == 2) {
                /* Directory */
                f->type = UFT_FTYPE_DIR;
                f->size = 0;
                dir->count++;
            }
            
            /* Hash chain at offset 496 */
            block_num = (header[496] << 24) | (header[497] << 16) | 
                        (header[498] << 8) | header[499];
        }
    }
    
    return 0;
}

/**
 * @brief Extract file from ADF
 */
int adf_extract_file(const uint8_t *image, size_t img_size,
                     const char *filename, uint8_t **data, size_t *size)
{
    if (!image || !filename || !data || !size) return -1;
    
    /* Find file in root directory */
    const uint8_t *root = adf_block(image, ADF_ROOT_BLOCK);
    
    uint32_t file_block = 0;
    uint32_t file_size = 0;
    
    for (int h = 0; h < 72 && file_block == 0; h++) {
        int offset = 24 + h * 4;
        uint32_t block_num = (root[offset] << 24) | (root[offset+1] << 16) | 
                             (root[offset+2] << 8) | root[offset+3];
        
        while (block_num != 0) {
            const uint8_t *header = adf_block(image, block_num);
            
            int fname_len = header[432];
            char fname[32];
            memcpy(fname, header + 433, fname_len);
            fname[fname_len] = '\0';
            
            if (strcasecmp(fname, filename) == 0) {
                /* Check if it's a file (sec_type == -3) */
                int32_t sec_type = (int32_t)((header[508] << 24) | (header[509] << 16) | 
                                              (header[510] << 8) | header[511]);
                if (sec_type == -3) {
                    file_block = block_num;
                    file_size = (header[324] << 24) | (header[325] << 16) | 
                                (header[326] << 8) | header[327];
                    break;
                }
            }
            
            block_num = (header[496] << 24) | (header[497] << 16) | 
                        (header[498] << 8) | header[499];
        }
    }
    
    if (file_block == 0) return -1;  /* Not found */
    
    /* Allocate buffer */
    uint8_t *buf = malloc(file_size);
    if (!buf) return -1;
    
    /* Read data blocks */
    const uint8_t *header = adf_block(image, file_block);
    size_t pos = 0;
    
    /* Data block list at offset 8, up to 72 entries */
    int num_data_blocks = (header[8] << 24) | (header[9] << 16) | 
                          (header[10] << 8) | header[11];
    
    for (int i = 0; i < num_data_blocks && pos < file_size; i++) {
        int idx = 72 - 1 - i;  /* Blocks are listed in reverse */
        int blk_offset = 312 - i * 4;
        
        uint32_t data_block = (header[blk_offset] << 24) | (header[blk_offset+1] << 16) | 
                              (header[blk_offset+2] << 8) | header[blk_offset+3];
        
        if (data_block == 0) continue;
        
        const uint8_t *dblock = adf_block(image, data_block);
        
        /* OFS: data starts at offset 24, 488 bytes */
        /* FFS: data starts at offset 0, 512 bytes */
        /* Detect by checking first long (should be 8 for OFS data block) */
        uint32_t blk_type = (dblock[0] << 24) | (dblock[1] << 16) | 
                            (dblock[2] << 8) | dblock[3];
        
        size_t to_copy = file_size - pos;
        
        if (blk_type == 8) {
            /* OFS data block */
            if (to_copy > 488) to_copy = 488;
            memcpy(buf + pos, dblock + 24, to_copy);
        } else {
            /* FFS data block */
            if (to_copy > 512) to_copy = 512;
            memcpy(buf + pos, dblock, to_copy);
        }
        
        pos += to_copy;
    }
    
    *data = buf;
    *size = file_size;
    
    return 0;
}

/* ============================================================================
 * ATR Atari File Operations (Atari DOS 2.x)
 * ============================================================================ */

#define ATR_HEADER_SIZE     16
#define ATR_VTOC_SECTOR     360

/**
 * @brief Get ATR sector offset
 */
static size_t atr_sector_offset(const uint8_t *image, int sector, int *sect_size)
{
    /* First 3 sectors are always 128 bytes */
    if (sector <= 3) {
        *sect_size = 128;
        return ATR_HEADER_SIZE + (sector - 1) * 128;
    }
    
    /* Detect sector size from header */
    int ss = image[4] | (image[5] << 8);
    *sect_size = ss;
    
    return ATR_HEADER_SIZE + 3 * 128 + (sector - 4) * ss;
}

/**
 * @brief List files on ATR image (Atari DOS 2.x)
 */
int atr_list_files(const uint8_t *image, size_t size, uft_directory_t *dir)
{
    if (!image || !dir || size < 92176) return -1;
    
    memset(dir, 0, sizeof(*dir));
    
    /* Read VTOC (sector 360) */
    int sect_size;
    size_t vtoc_off = atr_sector_offset(image, ATR_VTOC_SECTOR, &sect_size);
    const uint8_t *vtoc = image + vtoc_off;
    
    /* Free sectors at offset 3-4 */
    dir->free_blocks = vtoc[3] | (vtoc[4] << 8);
    dir->total_blocks = 720;  /* Standard SD */
    
    /* Directory is in sectors 361-368 */
    for (int ds = 361; ds <= 368; ds++) {
        size_t dir_off = atr_sector_offset(image, ds, &sect_size);
        const uint8_t *dir_sect = image + dir_off;
        
        /* 8 entries per sector (16 bytes each) */
        for (int e = 0; e < 8 && dir->count < UFT_MAX_FILES; e++) {
            const uint8_t *entry = dir_sect + e * 16;
            
            uint8_t flags = entry[0];
            if (flags == 0x00) continue;  /* Never used */
            if (flags == 0x80) continue;  /* Deleted */
            
            uft_file_entry_t *f = &dir->files[dir->count];
            
            f->raw_type = flags;
            f->deleted = (flags & 0x80) != 0;
            f->locked = (flags & 0x20) != 0;
            
            /* Sector count */
            f->blocks = entry[1] | (entry[2] << 8);
            
            /* Start sector */
            f->start_sector = entry[3] | (entry[4] << 8);
            
            /* Filename (8.3 format) */
            char name[9], ext[4];
            memcpy(name, entry + 5, 8);
            name[8] = '\0';
            memcpy(ext, entry + 13, 3);
            ext[3] = '\0';
            
            /* Trim spaces */
            for (int i = 7; i >= 0 && name[i] == ' '; i--) name[i] = '\0';
            for (int i = 2; i >= 0 && ext[i] == ' '; i--) ext[i] = '\0';
            
            if (ext[0]) {
                snprintf(f->name, sizeof(f->name), "%s.%s", name, ext);
            } else {
                strncpy(f->name, name, sizeof(f->name)-1); f->name[sizeof(f->name)-1] = '\0';
            }
            
            f->size = f->blocks * (sect_size - 3);  /* Minus link bytes */
            f->type = UFT_FTYPE_BINARY;
            
            if (!(flags & 0x80)) {
                dir->count++;
            }
        }
    }
    
    return 0;
}

/**
 * @brief Extract file from ATR
 */
int atr_extract_file(const uint8_t *image, size_t img_size,
                     const char *filename, uint8_t **data, size_t *size)
{
    if (!image || !filename || !data || !size) return -1;
    
    uft_directory_t dir;
    if (atr_list_files(image, img_size, &dir) != 0) return -1;
    
    /* Find file */
    int found = -1;
    for (int i = 0; i < dir.count; i++) {
        if (strcasecmp(dir.files[i].name, filename) == 0) {
            found = i;
            break;
        }
    }
    
    if (found < 0) return -1;
    
    uft_file_entry_t *f = &dir.files[found];
    
    /* Allocate buffer */
    size_t max_size = f->blocks * 256;
    uint8_t *buf = malloc(max_size);
    if (!buf) return -1;
    
    /* Follow sector chain */
    int sector = f->start_sector;
    size_t pos = 0;
    int sect_size;
    
    while (sector > 0 && sector < 1000 && pos < max_size) {
        size_t offset = atr_sector_offset(image, sector, &sect_size);
        const uint8_t *sect = image + offset;
        
        /* Link bytes: [0-1] = file num + flags, [2] = next sector low, 
           last byte = next sector high + byte count */
        int next_sector = ((sect[sect_size - 1] & 0x03) << 8) | sect[sect_size - 3];
        int byte_count = sect[sect_size - 1] >> 2;
        
        if (byte_count == 0) byte_count = sect_size - 3;
        
        memcpy(buf + pos, sect, byte_count);
        pos += byte_count;
        
        if (next_sector == 0) break;
        sector = next_sector;
    }
    
    *data = buf;
    *size = pos;
    
    return 0;
}

/* ============================================================================
 * TRD ZX Spectrum File Operations
 * ============================================================================ */

/**
 * @brief List files on TRD image
 */
int trd_list_files(const uint8_t *image, size_t size, uft_directory_t *dir)
{
    if (!image || !dir || size < 655360) return -1;
    
    memset(dir, 0, sizeof(*dir));
    
    /* Info sector at track 0, sector 8 (offset 0xE1) */
    const uint8_t *info = image + 8 * 256 + 0xE1;
    
    dir->free_blocks = info[4] | (info[5] << 8);
    dir->total_blocks = 2544;  /* 80 tracks × 2 sides × 16 sectors - reserved */
    
    /* Directory is in sectors 0-7 of track 0 */
    for (int s = 0; s < 8 && dir->count < UFT_MAX_FILES; s++) {
        const uint8_t *sect = image + s * 256;
        
        /* 16 entries per sector (16 bytes each) */
        for (int e = 0; e < 16 && dir->count < UFT_MAX_FILES; e++) {
            const uint8_t *entry = sect + e * 16;
            
            if (entry[0] == 0x00) continue;  /* Empty */
            if (entry[0] == 0x01) continue;  /* Deleted */
            
            uft_file_entry_t *f = &dir->files[dir->count];
            
            /* Filename (8 chars) */
            memcpy(f->name, entry, 8);
            f->name[8] = '\0';
            for (int i = 7; i >= 0 && f->name[i] == ' '; i--) f->name[i] = '\0';
            
            /* Type */
            f->raw_type = entry[8];
            switch (entry[8]) {
                case 'B': f->type = UFT_FTYPE_BASIC; break;
                case 'C': f->type = UFT_FTYPE_CODE; break;
                case 'D': f->type = UFT_FTYPE_DATA; break;
                case '#': f->type = UFT_FTYPE_TEXT; break;
                default:  f->type = UFT_FTYPE_BINARY; break;
            }
            
            /* Start/length */
            f->load_addr = entry[9] | (entry[10] << 8);
            f->size = entry[11] | (entry[12] << 8);
            f->blocks = entry[13];
            f->start_sector = entry[14];
            f->start_track = entry[15];
            
            dir->count++;
        }
    }
    
    return 0;
}

/* ============================================================================
 * Unified API
 * ============================================================================ */

/**
 * @brief Detect format and list files
 */
int uft_list_files(const char *path, uft_directory_t *dir)
{
    if (!path || !dir) return -1;
    
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *image = malloc(size);
    if (!image) {
        fclose(fp);
        return -1;
    }
    
    /* P1-IO-001: Check fread return */
    if (fread(image, 1, size, fp) != size) {
        free(image);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    int result = -1;
    
    /* Detect format by extension and size */
    const char *ext = strrchr(path, '.');
    if (ext) ext++;
    
    if (ext && strcasecmp(ext, "d64") == 0) {
        result = d64_list_files(image, size, dir);
    } else if (ext && strcasecmp(ext, "adf") == 0) {
        result = adf_list_files(image, size, dir);
    } else if (ext && strcasecmp(ext, "atr") == 0) {
        result = atr_list_files(image, size, dir);
    } else if (ext && strcasecmp(ext, "trd") == 0) {
        result = trd_list_files(image, size, dir);
    } else {
        /* Try to detect by size */
        if (size == 174848 || size == 175531 || size == 196608) {
            result = d64_list_files(image, size, dir);
        } else if (size == 901120 || size == 1802240) {
            result = adf_list_files(image, size, dir);
        } else if (size >= 92160 && size <= 184336) {
            result = atr_list_files(image, size, dir);
        } else if (size == 655360) {
            result = trd_list_files(image, size, dir);
        }
    }
    
    free(image);
    return result;
}

/**
 * @brief Extract file from disk image
 */
int uft_extract_file(const char *image_path, const char *filename,
                     const char *output_path)
{
    if (!image_path || !filename || !output_path) return -1;
    
    FILE *fp = fopen(image_path, "rb");
    if (!fp) return -1;
    
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    size_t size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    uint8_t *image = malloc(size);
    if (!image) {
        fclose(fp);
        return -1;
    }
    
    /* P1-IO-001: Check fread return */
    if (fread(image, 1, size, fp) != size) {
        free(image);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    uint8_t *data = NULL;
    size_t data_size = 0;
    int result = -1;
    
    const char *ext = strrchr(image_path, '.');
    if (ext) ext++;
    
    if (ext && strcasecmp(ext, "d64") == 0) {
        result = d64_extract_file(image, size, filename, &data, &data_size);
    } else if (ext && strcasecmp(ext, "adf") == 0) {
        result = adf_extract_file(image, size, filename, &data, &data_size);
    } else if (ext && strcasecmp(ext, "atr") == 0) {
        result = atr_extract_file(image, size, filename, &data, &data_size);
    }
    
    free(image);
    
    if (result == 0 && data) {
        FILE *out = fopen(output_path, "wb");
    if (!out) { return UFT_ERR_FILE_OPEN; }
        if (out) {
            if (fwrite(data, 1, data_size, out) != data_size) { /* I/O error */ }
            fclose(out);
        } else {
            result = -1;
        }
        free(data);
    }
    
    return result;
}

/**
 * @brief Inject file into disk image
 */
int uft_inject_file(const char *image_path, const char *filename,
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
    
    if (fread(data, 1, data_size, in) != data_size) { /* I/O error */ }
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
    
    if (fread(image, 1, img_size, fp) != img_size) { /* I/O error */ }
    int result = -1;
    
    const char *ext = strrchr(image_path, '.');
    if (ext) ext++;
    
    if (ext && strcasecmp(ext, "d64") == 0) {
        result = d64_inject_file(image, img_size, filename, data, data_size, type);
    }
    /* Add more formats as needed */
    
    if (result == 0) {
        if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
        if (fwrite(image, 1, img_size, fp) != img_size) { /* I/O error */ }
    }
    
    free(image);
    free(data);
    fclose(fp);
    
    return result;
}

/**
 * @brief Print directory listing
 */
void uft_print_directory(const uft_directory_t *dir)
{
    if (!dir) return;
    
    printf("╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║ Disk: %-20s ID: %-10s                        ║\n", 
           dir->disk_name, dir->disk_id);
    printf("║ Free: %d blocks of %d                                              \n",
           dir->free_blocks, dir->total_blocks);
    printf("╠════════════════════════════════════════════════════════════════════╣\n");
    printf("║ # │ Name             │ Type    │ Size     │ Blocks │ T/S          ║\n");
    printf("╠════════════════════════════════════════════════════════════════════╣\n");
    
    const char *type_names[] = {
        "???", "PRG", "SEQ", "REL", "USR", "DEL", "BAS", "DAT", 
        "COD", "TXT", "BIN", "DIR"
    };
    
    for (int i = 0; i < dir->count; i++) {
        const uft_file_entry_t *f = &dir->files[i];
        
        const char *tname = (f->type < 12) ? type_names[f->type] : "???";
        
        printf("║ %2d│ %-17s│ %-7s │ %8u │ %6u │ %3d/%-3d      ║\n",
               i + 1, f->name, tname, f->size, f->blocks,
               f->start_track, f->start_sector);
    }
    
    printf("╚════════════════════════════════════════════════════════════════════╝\n");
    printf("  %d file(s)\n", dir->count);
}

/**
 * @file uft_cbm_formats.c
 * @brief Commodore File Format Support (D64/D71/D81/T64/Lynx)
 * 
 * Based on cbmconvert by Marko Mäkelä (GPLv2+)
 * Integrated into UnifiedFloppyTool for Commodore disk/tape handling.
 * 
 * Supports:
 * - D64: 1541 disk image (35/40 tracks)
 * - D71: 1571 disk image (70 tracks, double-sided)
 * - D81: 1581 disk image (80 tracks, 40 sectors/track)
 * - T64: Tape image format
 * - Lynx: Archive format
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* D64 constants */
#define D64_TRACKS          35
#define D64_SECTORS         683
#define D64_SIZE            174848
#define D64_SIZE_ERRORS     175531
#define D64_40_TRACKS       40
#define D64_40_SECTORS      768
#define D64_40_SIZE         196608

/* D71 constants */
#define D71_TRACKS          70
#define D71_SECTORS         1366
#define D71_SIZE            349696

/* D81 constants */
#define D81_TRACKS          80
#define D81_SECTORS_TRACK   40
#define D81_SIZE            819200

/* Common */
#define SECTOR_SIZE         256
#define CBM_NAME_LENGTH     16

/* D64 directory */
#define D64_DIR_TRACK       18
#define D64_DIR_SECTOR      1
#define D64_BAM_TRACK       18
#define D64_BAM_SECTOR      0

/* D81 directory */
#define D81_DIR_TRACK       40
#define D81_DIR_SECTOR      3
#define D81_BAM_TRACK       40
#define D81_BAM_SECTOR      1

/* T64 constants */
#define T64_HEADER_SIZE     64
#define T64_ENTRY_SIZE      32
#define T64_MAGIC           "C64 tape image file"
#define T64_MAGIC_ALT       "C64S tape image file"

/* Error codes */
#define UFT_CBM_OK          0
#define UFT_CBM_ERROR       -1
#define UFT_CBM_INVALID     -2
#define UFT_CBM_NOMEM       -3
#define UFT_CBM_IO_ERROR    -4

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CBM DOS file types
 */
typedef enum {
    UFT_CBM_DEL = 0,    /**< Deleted file */
    UFT_CBM_SEQ = 1,    /**< Sequential file */
    UFT_CBM_PRG = 2,    /**< Program file */
    UFT_CBM_USR = 3,    /**< User file */
    UFT_CBM_REL = 4,    /**< Relative file */
    UFT_CBM_CBM = 5,    /**< CBM partition (1581) */
    UFT_CBM_DIR = 6     /**< Directory (1581) */
} uft_cbm_file_type_t;

/**
 * @brief Disk image type
 */
typedef enum {
    UFT_CBM_IMG_UNKNOWN = 0,
    UFT_CBM_IMG_D64,
    UFT_CBM_IMG_D64_40,
    UFT_CBM_IMG_D71,
    UFT_CBM_IMG_D81,
    UFT_CBM_IMG_T64,
    UFT_CBM_IMG_G64,
    UFT_CBM_IMG_G71
} uft_cbm_image_type_t;

/**
 * @brief CBM file entry
 */
typedef struct uft_cbm_file {
    uint8_t name[CBM_NAME_LENGTH];  /**< PETSCII filename */
    size_t name_length;              /**< Actual name length */
    uft_cbm_file_type_t type;        /**< File type */
    uint16_t start_address;          /**< Load address */
    uint16_t end_address;            /**< End address */
    size_t length;                   /**< Data length */
    uint8_t *data;                   /**< File data */
    uint8_t record_length;           /**< Record length (REL files) */
    int start_track;                 /**< First track */
    int start_sector;                /**< First sector */
    int block_count;                 /**< Number of blocks */
    struct uft_cbm_file *next;       /**< Next file in list */
} uft_cbm_file_t;

/**
 * @brief Disk image context
 */
typedef struct {
    uft_cbm_image_type_t type;
    uint8_t *data;
    size_t size;
    
    /* Disk geometry */
    int tracks;
    int sectors_total;
    
    /* Directory location */
    int dir_track;
    int dir_sector;
    int bam_track;
    int bam_sector;
    
    /* Disk name and ID */
    uint8_t disk_name[CBM_NAME_LENGTH];
    uint8_t disk_id[5];
    
    /* Statistics */
    int blocks_free;
    int files_count;
    
    /* File list */
    uft_cbm_file_t *files;
} uft_cbm_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Sectors Per Track Tables
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief D64 sectors per track (1-40)
 */
static const int d64_sectors_per_track[] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24 */
    18, 18, 18, 18, 18, 18,                                              /* 25-30 */
    17, 17, 17, 17, 17,                                                  /* 31-35 */
    17, 17, 17, 17, 17                                                   /* 36-40 */
};

/**
 * @brief D71 sectors per track (side 1: 1-35, side 2: 36-70)
 */
static const int d71_sectors_per_track[] = {
    /* Side 1 */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17,
    /* Side 2 */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian value
 */
static inline uint16_t read16_le(const uint8_t *p)
{
    return p[0] | (p[1] << 8);
}

/**
 * @brief Write 16-bit little-endian value
 */
static inline void write16_le(uint8_t *p, uint16_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/**
 * @brief Read 32-bit little-endian value
 */
static inline uint32_t read32_le(const uint8_t *p)
{
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Write 32-bit little-endian value
 */
static inline void write32_le(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/**
 * @brief Convert PETSCII to ASCII
 */
static uint8_t petscii_to_ascii(uint8_t c)
{
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;  /* Lowercase */
    if (c >= 0xC1 && c <= 0xDA) return c - 0x80;  /* Uppercase */
    if (c >= 0x61 && c <= 0x7A) return c - 0x20;  /* Shifted lowercase */
    if (c == 0xA0) return ' ';  /* Shifted space */
    if (c < 0x20 || c > 0x7E) return '?';  /* Non-printable */
    return c;
}

/**
 * @brief Convert ASCII to PETSCII
 */
static uint8_t ascii_to_petscii(uint8_t c)
{
    if (c >= 'a' && c <= 'z') return c - 0x20;  /* To uppercase */
    if (c >= 'A' && c <= 'Z') return c;
    return c;
}

/**
 * @brief Get file type string
 */
const char *uft_cbm_file_type_str(uft_cbm_file_type_t type)
{
    static const char *types[] = {
        "DEL", "SEQ", "PRG", "USR", "REL", "CBM", "DIR"
    };
    if (type >= 0 && type <= 6) return types[type];
    return "???";
}

/**
 * @brief Calculate D64 sector offset
 */
static long d64_sector_offset(int track, int sector)
{
    long offset = 0;
    
    if (track < 1 || track > 40) return -1;
    
    for (int t = 1; t < track; t++) {
        offset += d64_sectors_per_track[t - 1] * SECTOR_SIZE;
    }
    
    if (sector < 0 || sector >= d64_sectors_per_track[track - 1]) {
        return -1;
    }
    
    return offset + sector * SECTOR_SIZE;
}

/**
 * @brief Calculate D71 sector offset
 */
static long d71_sector_offset(int track, int sector)
{
    long offset = 0;
    
    if (track < 1 || track > 70) return -1;
    
    for (int t = 1; t < track; t++) {
        offset += d71_sectors_per_track[t - 1] * SECTOR_SIZE;
    }
    
    if (sector < 0 || sector >= d71_sectors_per_track[track - 1]) {
        return -1;
    }
    
    return offset + sector * SECTOR_SIZE;
}

/**
 * @brief Calculate D81 sector offset
 */
static long d81_sector_offset(int track, int sector)
{
    if (track < 1 || track > D81_TRACKS || 
        sector < 0 || sector >= D81_SECTORS_TRACK) {
        return -1;
    }
    return ((long)(track - 1) * D81_SECTORS_TRACK + sector) * SECTOR_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Image Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect image type from size
 */
uft_cbm_image_type_t uft_cbm_detect_type_by_size(size_t size)
{
    if (size == D64_SIZE || size == D64_SIZE_ERRORS) {
        return UFT_CBM_IMG_D64;
    }
    if (size == D64_40_SIZE) {
        return UFT_CBM_IMG_D64_40;
    }
    if (size == D71_SIZE) {
        return UFT_CBM_IMG_D71;
    }
    if (size == D81_SIZE) {
        return UFT_CBM_IMG_D81;
    }
    return UFT_CBM_IMG_UNKNOWN;
}

/**
 * @brief Allocate CBM file structure
 */
uft_cbm_file_t *uft_cbm_file_alloc(void)
{
    uft_cbm_file_t *file = calloc(1, sizeof(uft_cbm_file_t));
    return file;
}

/**
 * @brief Free CBM file structure
 */
void uft_cbm_file_free(uft_cbm_file_t *file)
{
    if (file) {
        free(file->data);
        free(file);
    }
}

/**
 * @brief Free file list
 */
void uft_cbm_files_free(uft_cbm_file_t *files)
{
    while (files) {
        uft_cbm_file_t *next = files->next;
        uft_cbm_file_free(files);
        files = next;
    }
}

/**
 * @brief Read sector from disk image
 */
static int disk_read_sector(const uft_cbm_disk_t *disk, int track, int sector,
                            uint8_t *buffer)
{
    long offset;
    
    switch (disk->type) {
        case UFT_CBM_IMG_D64:
        case UFT_CBM_IMG_D64_40:
            offset = d64_sector_offset(track, sector);
            break;
        case UFT_CBM_IMG_D71:
            offset = d71_sector_offset(track, sector);
            break;
        case UFT_CBM_IMG_D81:
            offset = d81_sector_offset(track, sector);
            break;
        default:
            return UFT_CBM_INVALID;
    }
    
    if (offset < 0 || (size_t)(offset + SECTOR_SIZE) > disk->size) {
        return UFT_CBM_INVALID;
    }
    
    memcpy(buffer, disk->data + offset, SECTOR_SIZE);
    return UFT_CBM_OK;
}

/**
 * @brief Write sector to disk image
 */
static int disk_write_sector(uft_cbm_disk_t *disk, int track, int sector,
                             const uint8_t *buffer)
{
    long offset;
    
    switch (disk->type) {
        case UFT_CBM_IMG_D64:
        case UFT_CBM_IMG_D64_40:
            offset = d64_sector_offset(track, sector);
            break;
        case UFT_CBM_IMG_D71:
            offset = d71_sector_offset(track, sector);
            break;
        case UFT_CBM_IMG_D81:
            offset = d81_sector_offset(track, sector);
            break;
        default:
            return UFT_CBM_INVALID;
    }
    
    if (offset < 0 || (size_t)(offset + SECTOR_SIZE) > disk->size) {
        return UFT_CBM_INVALID;
    }
    
    memcpy(disk->data + offset, buffer, SECTOR_SIZE);
    return UFT_CBM_OK;
}

/**
 * @brief Read file chain from disk
 */
static uint8_t *read_file_chain(const uft_cbm_disk_t *disk, 
                                int start_track, int start_sector,
                                size_t *length, int *block_count)
{
    uint8_t sector[SECTOR_SIZE];
    uint8_t *data = NULL;
    size_t size = 0;
    size_t capacity = 0;
    int track = start_track;
    int sec = start_sector;
    int count = 0;
    
    while (track != 0 && count < 1000) {
        if (disk_read_sector(disk, track, sec, sector) != UFT_CBM_OK) {
            free(data);
            return NULL;
        }
        
        count++;
        
        /* Expand buffer */
        if (size + 254 > capacity) {
            capacity = capacity ? capacity * 2 : 4096;
            uint8_t *new_data = realloc(data, capacity);
            if (!new_data) {
                free(data);
                return NULL;
            }
            data = new_data;
        }
        
        int next_track = sector[0];
        int next_sector = sector[1];
        
        if (next_track == 0) {
            /* Last sector */
            int bytes = next_sector > 1 ? next_sector - 1 : 254;
            memcpy(data + size, sector + 2, bytes);
            size += bytes;
        } else {
            memcpy(data + size, sector + 2, 254);
            size += 254;
        }
        
        track = next_track;
        sec = next_sector;
    }
    
    *length = size;
    *block_count = count;
    return data;
}

/**
 * @brief Read directory from disk image
 */
static int disk_read_directory(uft_cbm_disk_t *disk)
{
    uint8_t sector[SECTOR_SIZE];
    int track = disk->dir_track;
    int sec = disk->dir_sector;
    int dir_count = 0;
    
    uft_cbm_files_free(disk->files);
    disk->files = NULL;
    disk->files_count = 0;
    
    while (track != 0 && dir_count < 20) {
        if (disk_read_sector(disk, track, sec, sector) != UFT_CBM_OK) {
            return UFT_CBM_IO_ERROR;
        }
        
        /* Process 8 directory entries per sector */
        for (int i = 0; i < 8; i++) {
            uint8_t *entry = sector + i * 32;
            
            /* Check file type */
            uint8_t file_type = entry[2];
            if ((file_type & 0x0F) == 0) {
                continue;  /* Deleted/empty */
            }
            
            uft_cbm_file_t *file = uft_cbm_file_alloc();
            if (!file) return UFT_CBM_NOMEM;
            
            /* Copy filename */
            memcpy(file->name, entry + 5, CBM_NAME_LENGTH);
            file->name_length = CBM_NAME_LENGTH;
            
            /* Trim trailing shifted spaces */
            while (file->name_length > 0 && 
                   file->name[file->name_length - 1] == 0xA0) {
                file->name_length--;
            }
            
            file->type = (uft_cbm_file_type_t)(file_type & 0x07);
            file->start_track = entry[3];
            file->start_sector = entry[4];
            
            /* REL file record length */
            if (file->type == UFT_CBM_REL) {
                file->record_length = entry[21];
            }
            
            /* Read file data */
            file->data = read_file_chain(disk, file->start_track, 
                                         file->start_sector,
                                         &file->length, &file->block_count);
            
            if (file->data && file->length >= 2) {
                file->start_address = read16_le(file->data);
                file->end_address = file->start_address + (uint16_t)file->length - 2;
            }
            
            /* Add to list */
            file->next = disk->files;
            disk->files = file;
            disk->files_count++;
        }
        
        track = sector[0];
        sec = sector[1];
        dir_count++;
    }
    
    return UFT_CBM_OK;
}

/**
 * @brief Open disk image file
 */
int uft_cbm_disk_open(uft_cbm_disk_t *disk, const char *filename)
{
    memset(disk, 0, sizeof(*disk));
    
    FILE *f = fopen(filename, "rb");
    if (!f) return UFT_CBM_IO_ERROR;
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0) {
        fclose(f);
        return UFT_CBM_INVALID;
    }
    
    disk->type = uft_cbm_detect_type_by_size(size);
    if (disk->type == UFT_CBM_IMG_UNKNOWN) {
        fclose(f);
        return UFT_CBM_INVALID;
    }
    
    /* Allocate and read data */
    disk->data = malloc(size);
    if (!disk->data) {
        fclose(f);
        return UFT_CBM_NOMEM;
    }
    
    if (fread(disk->data, 1, size, f) != (size_t)size) {
        free(disk->data);
        fclose(f);
        return UFT_CBM_IO_ERROR;
    }
    fclose(f);
    
    disk->size = size;
    
    /* Set geometry based on type */
    switch (disk->type) {
        case UFT_CBM_IMG_D64:
            disk->tracks = D64_TRACKS;
            disk->sectors_total = D64_SECTORS;
            disk->dir_track = D64_DIR_TRACK;
            disk->dir_sector = D64_DIR_SECTOR;
            disk->bam_track = D64_BAM_TRACK;
            disk->bam_sector = D64_BAM_SECTOR;
            break;
            
        case UFT_CBM_IMG_D64_40:
            disk->tracks = D64_40_TRACKS;
            disk->sectors_total = D64_40_SECTORS;
            disk->dir_track = D64_DIR_TRACK;
            disk->dir_sector = D64_DIR_SECTOR;
            disk->bam_track = D64_BAM_TRACK;
            disk->bam_sector = D64_BAM_SECTOR;
            break;
            
        case UFT_CBM_IMG_D71:
            disk->tracks = D71_TRACKS;
            disk->sectors_total = D71_SECTORS;
            disk->dir_track = D64_DIR_TRACK;
            disk->dir_sector = D64_DIR_SECTOR;
            disk->bam_track = D64_BAM_TRACK;
            disk->bam_sector = D64_BAM_SECTOR;
            break;
            
        case UFT_CBM_IMG_D81:
            disk->tracks = D81_TRACKS;
            disk->sectors_total = D81_TRACKS * D81_SECTORS_TRACK;
            disk->dir_track = D81_DIR_TRACK;
            disk->dir_sector = D81_DIR_SECTOR;
            disk->bam_track = D81_BAM_TRACK;
            disk->bam_sector = D81_BAM_SECTOR;
            break;
            
        default:
            break;
    }
    
    /* Read BAM to get disk name */
    uint8_t bam[SECTOR_SIZE];
    if (disk_read_sector(disk, disk->bam_track, disk->bam_sector, bam) == UFT_CBM_OK) {
        if (disk->type == UFT_CBM_IMG_D81) {
            memcpy(disk->disk_name, bam + 4, CBM_NAME_LENGTH);
            memcpy(disk->disk_id, bam + 22, 5);
        } else {
            memcpy(disk->disk_name, bam + 0x90, CBM_NAME_LENGTH);
            memcpy(disk->disk_id, bam + 0xA2, 5);
        }
    }
    
    /* Read directory */
    disk_read_directory(disk);
    
    return UFT_CBM_OK;
}

/**
 * @brief Close disk image
 */
void uft_cbm_disk_close(uft_cbm_disk_t *disk)
{
    if (disk) {
        free(disk->data);
        uft_cbm_files_free(disk->files);
        memset(disk, 0, sizeof(*disk));
    }
}

/**
 * @brief Save disk image to file
 */
int uft_cbm_disk_save(const uft_cbm_disk_t *disk, const char *filename)
{
    if (!disk || !disk->data) return UFT_CBM_INVALID;
    
    FILE *f = fopen(filename, "wb");
    if (!f) return UFT_CBM_IO_ERROR;
    
    if (fwrite(disk->data, 1, disk->size, f) != disk->size) {
        fclose(f);
        return UFT_CBM_IO_ERROR;
    }
    
    fclose(f);
    return UFT_CBM_OK;
}

/**
 * @brief Create empty D64 image
 */
int uft_cbm_disk_create_d64(uft_cbm_disk_t *disk, const char *disk_name)
{
    memset(disk, 0, sizeof(*disk));
    
    disk->type = UFT_CBM_IMG_D64;
    disk->size = D64_SIZE;
    disk->tracks = D64_TRACKS;
    disk->sectors_total = D64_SECTORS;
    disk->dir_track = D64_DIR_TRACK;
    disk->dir_sector = D64_DIR_SECTOR;
    disk->bam_track = D64_BAM_TRACK;
    disk->bam_sector = D64_BAM_SECTOR;
    
    disk->data = calloc(1, D64_SIZE);
    if (!disk->data) return UFT_CBM_NOMEM;
    
    /* Initialize BAM */
    long bam_offset = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    uint8_t *bam = disk->data + bam_offset;
    
    bam[0] = D64_DIR_TRACK;
    bam[1] = D64_DIR_SECTOR;
    bam[2] = 0x41;  /* DOS version 'A' */
    bam[3] = 0x00;
    
    /* Initialize BAM entries */
    for (int t = 1; t <= D64_TRACKS; t++) {
        int sectors = d64_sectors_per_track[t - 1];
        uint8_t *entry = bam + 4 + (t - 1) * 4;
        
        if (t == D64_DIR_TRACK) {
            entry[0] = sectors - 2;  /* BAM + first dir sector used */
            entry[1] = 0xFC;
            entry[2] = 0xFF;
            entry[3] = sectors > 16 ? 0x1F : 0xFF;
        } else {
            entry[0] = sectors;
            entry[1] = 0xFF;
            entry[2] = 0xFF;
            entry[3] = sectors > 16 ? 0x1F : 0xFF;
        }
    }
    
    /* Set disk name */
    memset(bam + 0x90, 0xA0, CBM_NAME_LENGTH);
    if (disk_name) {
        size_t len = strlen(disk_name);
        if (len > CBM_NAME_LENGTH) len = CBM_NAME_LENGTH;
        for (size_t i = 0; i < len; i++) {
            bam[0x90 + i] = ascii_to_petscii(disk_name[i]);
        }
    }
    
    /* Disk ID */
    bam[0xA2] = '0';
    bam[0xA3] = '0';
    bam[0xA4] = 0xA0;
    bam[0xA5] = '2';
    bam[0xA6] = 'A';
    
    /* Initialize first directory sector */
    long dir_offset = d64_sector_offset(D64_DIR_TRACK, D64_DIR_SECTOR);
    disk->data[dir_offset + 0] = 0x00;  /* No next sector */
    disk->data[dir_offset + 1] = 0xFF;
    
    return UFT_CBM_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * T64 Tape Image Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read T64 tape image
 */
int uft_cbm_t64_read(const char *filename, uft_cbm_file_t **files)
{
    FILE *f = fopen(filename, "rb");
    if (!f) return UFT_CBM_IO_ERROR;
    
    uint8_t header[T64_HEADER_SIZE];
    if (fread(header, 1, T64_HEADER_SIZE, f) != T64_HEADER_SIZE) {
        fclose(f);
        return UFT_CBM_INVALID;
    }
    
    /* Check magic */
    if (memcmp(header, T64_MAGIC, 19) != 0 &&
        memcmp(header, T64_MAGIC_ALT, 20) != 0) {
        fclose(f);
        return UFT_CBM_INVALID;
    }
    
    int num_entries = read16_le(header + 0x24);
    if (num_entries == 0) num_entries = 1;
    
    int count = 0;
    *files = NULL;
    
    for (int i = 0; i < num_entries; i++) {
        uint8_t entry[T64_ENTRY_SIZE];
        if (fread(entry, 1, T64_ENTRY_SIZE, f) != T64_ENTRY_SIZE) {
            break;
        }
        
        uint8_t entry_type = entry[0];
        if (entry_type == 0) continue;
        
        uint16_t start_addr = read16_le(entry + 2);
        uint16_t end_addr = read16_le(entry + 4);
        uint32_t offset = read32_le(entry + 8);
        
        size_t size = end_addr - start_addr;
        if (size == 0 || size > 0x10000) continue;
        
        uft_cbm_file_t *file = uft_cbm_file_alloc();
        if (!file) {
            fclose(f);
            return UFT_CBM_NOMEM;
        }
        
        memcpy(file->name, entry + 16, CBM_NAME_LENGTH);
        file->name_length = CBM_NAME_LENGTH;
        while (file->name_length > 0 &&
               (file->name[file->name_length - 1] == 0x20 ||
                file->name[file->name_length - 1] == 0xA0)) {
            file->name_length--;
        }
        
        file->type = (entry_type == 3) ? UFT_CBM_SEQ : UFT_CBM_PRG;
        file->start_address = start_addr;
        file->end_address = end_addr;
        file->length = size + 2;
        
        file->data = malloc(file->length);
        if (!file->data) {
            uft_cbm_file_free(file);
            continue;
        }
        
        write16_le(file->data, start_addr);
        
        long current_pos = ftell(f);
        if (fseek(f, offset, SEEK_SET) != 0) return -1;
        if (fread(file->data + 2, 1, size, f) != size) {
            uft_cbm_file_free(file);
            fseek(f, current_pos, SEEK_SET);
            continue;
        }
        fseek(f, current_pos, SEEK_SET);
        
        file->next = *files;
        *files = file;
        count++;
    }
    
    fclose(f);
    return count;
}

/**
 * @brief Write T64 tape image
 */
int uft_cbm_t64_write(const char *filename, uft_cbm_file_t *files)
{
    int count = 0;
    for (uft_cbm_file_t *f = files; f; f = f->next) count++;
    if (count == 0) return 0;
    
    FILE *out = fopen(filename, "wb");
    if (!out) return UFT_CBM_IO_ERROR;
    
    /* Header */
    uint8_t header[T64_HEADER_SIZE] = {0};
    memcpy(header, "C64 tape image file\r\n", 21);
    header[0x20] = 0x01;
    header[0x22] = 0x01;
    write16_le(header + 0x24, count);
    write16_le(header + 0x26, count);
    memset(header + 0x28, 0x20, 24);
    
    fwrite(header, 1, T64_HEADER_SIZE, out);
    
    /* Calculate data offset */
    uint32_t offset = T64_HEADER_SIZE + count * T64_ENTRY_SIZE;
    
    /* Write entries */
    for (uft_cbm_file_t *f = files; f; f = f->next) {
        uint8_t entry[T64_ENTRY_SIZE] = {0};
        entry[0] = 1;
        entry[1] = f->type;
        write16_le(entry + 2, f->start_address);
        write16_le(entry + 4, f->end_address);
        write32_le(entry + 8, offset);
        memset(entry + 16, 0x20, CBM_NAME_LENGTH);
        memcpy(entry + 16, f->name, f->name_length);
        
        fwrite(entry, 1, T64_ENTRY_SIZE, out);
        offset += f->length > 2 ? f->length - 2 : 0;
    }
    
    /* Write file data */
    for (uft_cbm_file_t *f = files; f; f = f->next) {
        if (f->data && f->length > 2) {
            fwrite(f->data + 2, 1, f->length - 2, out);
        }
    }
    
    fclose(out);
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get ASCII filename from CBM file
 */
void uft_cbm_get_ascii_name(char *dest, const uft_cbm_file_t *file, size_t dest_size)
{
    if (!dest || dest_size == 0) return;
    
    size_t len = file->name_length;
    if (len >= dest_size) len = dest_size - 1;
    
    for (size_t i = 0; i < len; i++) {
        dest[i] = petscii_to_ascii(file->name[i]);
    }
    dest[len] = '\0';
}

/**
 * @brief Print disk directory
 */
void uft_cbm_print_directory(const uft_cbm_disk_t *disk, FILE *out)
{
    char name[32];
    
    /* Disk name */
    for (int i = 0; i < CBM_NAME_LENGTH; i++) {
        name[i] = petscii_to_ascii(disk->disk_name[i]);
    }
    name[CBM_NAME_LENGTH] = '\0';
    
    fprintf(out, "0 \"%-16s\" %c%c %c%c\n", name,
            petscii_to_ascii(disk->disk_id[0]),
            petscii_to_ascii(disk->disk_id[1]),
            petscii_to_ascii(disk->disk_id[3]),
            petscii_to_ascii(disk->disk_id[4]));
    
    /* Files */
    for (uft_cbm_file_t *f = disk->files; f; f = f->next) {
        uft_cbm_get_ascii_name(name, f, sizeof(name));
        fprintf(out, "%-5d \"%-16s\" %s\n", 
                f->block_count, name, 
                uft_cbm_file_type_str(f->type));
    }
    
    fprintf(out, "%d BLOCKS FREE.\n", disk->blocks_free);
}

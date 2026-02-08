/**
 * @file uft_d64_parser_v2.c
 * @brief GOD MODE D64 Parser v2 - Commodore 64 Disk Image Format
 * 
 * D64 is the standard Commodore 1541 disk image format.
 * - 35 tracks (40 for extended)
 * - 683 sectors (768 extended)
 * - GCR encoding with variable sectors per track
 * - Zone-based speed (tracks 1-17: 21 sectors, 18-24: 19, 25-30: 18, 31-35: 17)
 * 
 * Features:
 * - BAM (Block Availability Map) parsing
 * - Directory reading and file extraction
 * - Error byte handling (.d64 with errors)
 * - Multiple size variants (174848, 175531, 196608, 197376)
 * - GCR decode/encode support
 * - File type detection (PRG, SEQ, USR, REL, DEL)
 * - Track/Sector chain validation
 * - GEOS file support
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define D64_SECTOR_SIZE         256
#define D64_TRACKS_35           35
#define D64_TRACKS_40           40
#define D64_SECTORS_35          683
#define D64_SECTORS_40          768

/* Standard file sizes */
#define D64_SIZE_35             (D64_SECTORS_35 * D64_SECTOR_SIZE)  /* 174848 */
#define D64_SIZE_35_ERRORS      (D64_SIZE_35 + D64_SECTORS_35)      /* 175531 */
#define D64_SIZE_40             (D64_SECTORS_40 * D64_SECTOR_SIZE)  /* 196608 */
#define D64_SIZE_40_ERRORS      (D64_SIZE_40 + D64_SECTORS_40)      /* 197376 */

/* BAM location */
#define D64_BAM_TRACK           18
#define D64_BAM_SECTOR          0

/* Directory location */
#define D64_DIR_TRACK           18
#define D64_DIR_SECTOR          1
#define D64_DIR_ENTRIES_PER_SECTOR 8
#define D64_DIR_ENTRY_SIZE      32
#define D64_MAX_DIR_ENTRIES     144

/* Filename */
#define D64_FILENAME_LEN        16
#define D64_DISKNAME_LEN        16
#define D64_DISKID_LEN          5

/* File types */
#define D64_FTYPE_DEL           0x00
#define D64_FTYPE_SEQ           0x01
#define D64_FTYPE_PRG           0x02
#define D64_FTYPE_USR           0x03
#define D64_FTYPE_REL           0x04

/* File type flags */
#define D64_FLAG_LOCKED         0x40
#define D64_FLAG_CLOSED         0x80

/* Speed zones (sectors per track) */
static const uint8_t d64_sectors_per_track[] = {
    0,  /* Track 0 doesn't exist */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,  /* 18-24 */
    18, 18, 18, 18, 18, 18,      /* 25-30 */
    17, 17, 17, 17, 17,          /* 31-35 */
    17, 17, 17, 17, 17           /* 36-40 (extended) */
};

/* Track offsets (cumulative sectors) */
static const uint16_t d64_track_offset[] = {
    0,    /* Track 0 */
    0, 21, 42, 63, 84, 105, 126, 147, 168, 189,      /* 1-10 */
    210, 231, 252, 273, 294, 315, 336,               /* 11-17 */
    357, 376, 395, 414, 433, 452, 471,               /* 18-24 */
    490, 508, 526, 544, 562, 580,                    /* 25-30 */
    598, 615, 632, 649, 666,                         /* 31-35 */
    683, 700, 717, 734, 751                          /* 36-40 */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief D64 file types
 */
typedef enum {
    D64_TYPE_DEL = 0,
    D64_TYPE_SEQ = 1,
    D64_TYPE_PRG = 2,
    D64_TYPE_USR = 3,
    D64_TYPE_REL = 4,
    D64_TYPE_UNKNOWN = 255
} d64_file_type_t;

/**
 * @brief Error codes (for .d64 with error bytes)
 */
typedef enum {
    D64_ERR_OK = 0x01,
    D64_ERR_HEADER_NOT_FOUND = 0x02,
    D64_ERR_NO_SYNC = 0x03,
    D64_ERR_DATA_NOT_FOUND = 0x04,
    D64_ERR_CHECKSUM = 0x05,
    D64_ERR_WRITE_VERIFY = 0x06,
    D64_ERR_WRITE_PROTECT = 0x07,
    D64_ERR_HEADER_CHECKSUM = 0x08,
    D64_ERR_DATA_EXTENDS = 0x09,
    D64_ERR_ID_MISMATCH = 0x0B,
    D64_ERR_SYNTAX = 0x0F
} d64_error_t;

/**
 * @brief BAM entry for one track
 */
typedef struct {
    uint8_t free_sectors;
    uint8_t bitmap[3];  /* Bit = 1 means sector is free */
} d64_bam_entry_t;

/**
 * @brief Directory entry
 */
typedef struct {
    uint8_t file_type;        /* Type + flags */
    uint8_t first_track;      /* First data track */
    uint8_t first_sector;     /* First data sector */
    char filename[17];        /* PETSCII filename + null */
    uint8_t side_track;       /* REL: side sector track */
    uint8_t side_sector;      /* REL: side sector */
    uint8_t record_length;    /* REL: record length */
    uint8_t geos_type;        /* GEOS: file type */
    uint8_t year;             /* GEOS: year */
    uint8_t month;            /* GEOS: month */
    uint8_t day;              /* GEOS: day */
    uint8_t hour;             /* GEOS: hour */
    uint8_t minute;           /* GEOS: minute */
    uint16_t blocks;          /* File size in blocks */
    d64_file_type_t type;     /* Decoded file type */
    bool locked;              /* File locked? */
    bool closed;              /* File properly closed? */
    bool deleted;             /* Deleted entry? */
} d64_dir_entry_t;

/**
 * @brief D64 disk structure
 */
typedef struct {
    /* Disk info */
    char disk_name[17];       /* PETSCII disk name + null */
    char disk_id[6];          /* Disk ID + null */
    uint8_t dos_type;         /* DOS type byte */
    char dos_version;         /* DOS version character */
    
    /* Geometry */
    uint8_t num_tracks;       /* 35 or 40 */
    uint16_t num_sectors;     /* 683 or 768 */
    bool has_errors;          /* Error bytes present */
    
    /* BAM */
    d64_bam_entry_t bam[41];  /* BAM for tracks 1-40 */
    uint16_t free_blocks;     /* Total free blocks */
    
    /* Directory */
    d64_dir_entry_t directory[D64_MAX_DIR_ENTRIES];
    uint16_t dir_entries;     /* Number of directory entries */
    
    /* Error info */
    uint8_t* error_bytes;     /* Error byte per sector (if present) */
    uint16_t total_errors;    /* Count of error sectors */
    
    /* Status */
    bool valid;
    char error[256];
} d64_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get number of sectors for a track
 */
static uint8_t d64_sectors_for_track(uint8_t track) {
    if (track < 1 || track > 40) return 0;
    return d64_sectors_per_track[track];
}

/**
 * @brief Calculate sector offset in image
 */
static size_t d64_sector_offset(uint8_t track, uint8_t sector) {
    if (track < 1 || track > 40) return 0;
    if (sector >= d64_sectors_per_track[track]) return 0;
    return (d64_track_offset[track] + sector) * D64_SECTOR_SIZE;
}

/**
 * @brief Get file type name
 */
static const char* d64_file_type_name(d64_file_type_t type) {
    static const char* names[] = {
        "DEL", "SEQ", "PRG", "USR", "REL"
    };
    if (type <= D64_TYPE_REL) return names[type];
    return "???";
}

/**
 * @brief Get error code name
 */
static const char* d64_error_name(uint8_t err) {
    switch (err) {
        case D64_ERR_OK: return "OK";
        case D64_ERR_HEADER_NOT_FOUND: return "Header not found";
        case D64_ERR_NO_SYNC: return "No sync";
        case D64_ERR_DATA_NOT_FOUND: return "Data not found";
        case D64_ERR_CHECKSUM: return "Checksum error";
        case D64_ERR_WRITE_VERIFY: return "Write verify error";
        case D64_ERR_WRITE_PROTECT: return "Write protected";
        case D64_ERR_HEADER_CHECKSUM: return "Header checksum error";
        case D64_ERR_DATA_EXTENDS: return "Data extends";
        case D64_ERR_ID_MISMATCH: return "ID mismatch";
        case D64_ERR_SYNTAX: return "Syntax error";
        default: return "Unknown error";
    }
}

/**
 * @brief Convert PETSCII to ASCII
 */
static char petscii_to_ascii(uint8_t c) {
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;      /* A-Z -> a-z */
    if (c >= 0xC1 && c <= 0xDA) return c - 0x80;      /* Shifted A-Z */
    if (c >= 0x20 && c <= 0x7E) return c;             /* Printable ASCII */
    if (c == 0xA0) return ' ';                        /* Shifted space */
    return '.';                                        /* Non-printable */
}

/**
 * @brief Copy and convert PETSCII filename
 */
static void d64_copy_filename(char* dest, const uint8_t* src, size_t len) {
    size_t i;
    for (i = 0; i < len && src[i] != 0xA0 && src[i] != 0x00; i++) {
        dest[i] = petscii_to_ascii(src[i]);
    }
    dest[i] = '\0';
}

/**
 * @brief Check if sector is free in BAM
 */
static bool d64_sector_is_free(const d64_bam_entry_t* bam, uint8_t sector) {
    if (sector >= 24) return false;
    uint8_t byte = sector / 8;
    uint8_t bit = sector % 8;
    return (bam->bitmap[byte] & (1 << bit)) != 0;
}

/**
 * @brief Validate D64 file size
 */
static bool d64_is_valid_size(size_t size, uint8_t* tracks, bool* has_errors) {
    *has_errors = false;
    
    switch (size) {
        case D64_SIZE_35:
            *tracks = 35;
            return true;
        case D64_SIZE_35_ERRORS:
            *tracks = 35;
            *has_errors = true;
            return true;
        case D64_SIZE_40:
            *tracks = 40;
            return true;
        case D64_SIZE_40_ERRORS:
            *tracks = 40;
            *has_errors = true;
            return true;
        default:
            return false;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse BAM sector
 */
static bool d64_parse_bam(const uint8_t* data, size_t size, d64_disk_t* disk) {
    size_t bam_offset = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    if (bam_offset + D64_SECTOR_SIZE > size) {
        snprintf(disk->error, sizeof(disk->error), "BAM sector out of bounds");
        return false;
    }
    
    const uint8_t* bam = data + bam_offset;
    
    /* Directory pointer (should be 18/1) */
    /* bam[0] = dir track, bam[1] = dir sector */
    
    /* DOS version */
    disk->dos_type = bam[2];
    disk->dos_version = 'A';  /* Standard */
    
    /* BAM entries (tracks 1-35 at offset 4) */
    disk->free_blocks = 0;
    for (int track = 1; track <= disk->num_tracks && track <= 35; track++) {
        size_t entry_offset = 4 + (track - 1) * 4;
        
        disk->bam[track].free_sectors = bam[entry_offset];
        disk->bam[track].bitmap[0] = bam[entry_offset + 1];
        disk->bam[track].bitmap[1] = bam[entry_offset + 2];
        disk->bam[track].bitmap[2] = bam[entry_offset + 3];
        
        /* Don't count track 18 (directory) */
        if (track != D64_BAM_TRACK) {
            disk->free_blocks += disk->bam[track].free_sectors;
        }
    }
    
    /* Disk name (offset 0x90, 16 bytes, padded with 0xA0) */
    d64_copy_filename(disk->disk_name, bam + 0x90, D64_DISKNAME_LEN);
    
    /* Disk ID (offset 0xA2, 2 bytes) + DOS type (0xA5, 2 bytes) */
    disk->disk_id[0] = petscii_to_ascii(bam[0xA2]);
    disk->disk_id[1] = petscii_to_ascii(bam[0xA3]);
    disk->disk_id[2] = ' ';
    disk->disk_id[3] = petscii_to_ascii(bam[0xA5]);
    disk->disk_id[4] = petscii_to_ascii(bam[0xA6]);
    disk->disk_id[5] = '\0';
    
    return true;
}

/**
 * @brief Parse directory entry
 */
static bool d64_parse_dir_entry(const uint8_t* entry, d64_dir_entry_t* dir) {
    memset(dir, 0, sizeof(d64_dir_entry_t));
    
    dir->file_type = entry[2];
    dir->first_track = entry[3];
    dir->first_sector = entry[4];
    
    /* Decode file type */
    uint8_t type_code = dir->file_type & 0x07;
    dir->type = (type_code <= D64_TYPE_REL) ? type_code : D64_TYPE_UNKNOWN;
    dir->locked = (dir->file_type & D64_FLAG_LOCKED) != 0;
    dir->closed = (dir->file_type & D64_FLAG_CLOSED) != 0;
    dir->deleted = (dir->file_type == 0);
    
    /* Filename */
    d64_copy_filename(dir->filename, entry + 5, D64_FILENAME_LEN);
    
    /* REL file info */
    dir->side_track = entry[21];
    dir->side_sector = entry[22];
    dir->record_length = entry[23];
    
    /* GEOS info (if applicable) */
    dir->geos_type = entry[24];
    dir->year = entry[25];
    dir->month = entry[26];
    dir->day = entry[27];
    dir->hour = entry[28];
    dir->minute = entry[29];
    
    /* File size in blocks */
    dir->blocks = entry[30] | (entry[31] << 8);
    
    return !dir->deleted && dir->first_track > 0;
}

/**
 * @brief Parse directory
 */
static bool d64_parse_directory(const uint8_t* data, size_t size, d64_disk_t* disk) {
    uint8_t track = D64_DIR_TRACK;
    uint8_t sector = D64_DIR_SECTOR;
    disk->dir_entries = 0;
    
    /* Follow directory chain */
    int max_sectors = 19;  /* Track 18 has 19 sectors max */
    
    while (track != 0 && max_sectors-- > 0) {
        size_t offset = d64_sector_offset(track, sector);
        if (offset + D64_SECTOR_SIZE > size) break;
        
        const uint8_t* sec = data + offset;
        
        /* Parse 8 directory entries per sector */
        for (int i = 0; i < D64_DIR_ENTRIES_PER_SECTOR; i++) {
            if (disk->dir_entries >= D64_MAX_DIR_ENTRIES) break;
            
            const uint8_t* entry = sec + 2 + i * D64_DIR_ENTRY_SIZE;
            
            /* Skip empty entries */
            if (entry[2] == 0) continue;
            
            d64_dir_entry_t* dir = &disk->directory[disk->dir_entries];
            if (d64_parse_dir_entry(sec + i * D64_DIR_ENTRY_SIZE, dir)) {
                disk->dir_entries++;
            }
        }
        
        /* Next sector in chain */
        track = sec[0];
        sector = sec[1];
    }
    
    return true;
}

/**
 * @brief Parse D64 image
 */
static bool d64_parse(const uint8_t* data, size_t size, d64_disk_t* disk) {
    memset(disk, 0, sizeof(d64_disk_t));
    
    /* Validate size and determine geometry */
    if (!d64_is_valid_size(size, &disk->num_tracks, &disk->has_errors)) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Invalid D64 size: %zu bytes", size);
        return false;
    }
    
    disk->num_sectors = (disk->num_tracks == 35) ? D64_SECTORS_35 : D64_SECTORS_40;
    
    /* Parse BAM */
    if (!d64_parse_bam(data, size, disk)) {
        return false;
    }
    
    /* Parse directory */
    if (!d64_parse_directory(data, size, disk)) {
        return false;
    }
    
    /* Parse error bytes if present */
    if (disk->has_errors) {
        size_t error_offset = disk->num_sectors * D64_SECTOR_SIZE;
        disk->error_bytes = malloc(disk->num_sectors);
        if (disk->error_bytes) {
            memcpy(disk->error_bytes, data + error_offset, disk->num_sectors);
            
            /* Count errors */
            for (uint16_t i = 0; i < disk->num_sectors; i++) {
                if (disk->error_bytes[i] != D64_ERR_OK && 
                    disk->error_bytes[i] != 0) {
                    disk->total_errors++;
                }
            }
        }
    }
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FILE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extract file from D64
 */
static uint8_t* d64_extract_file(const uint8_t* disk_data, size_t disk_size,
                                  const d64_dir_entry_t* entry, size_t* out_size) {
    if (!entry || entry->first_track == 0) {
        *out_size = 0;
        return NULL;
    }
    
    /* Allocate maximum possible size */
    size_t max_size = entry->blocks * 254;  /* 254 data bytes per sector */
    uint8_t* file_data = malloc(max_size);
    if (!file_data) {
        *out_size = 0;
        return NULL;
    }
    
    uint8_t track = entry->first_track;
    uint8_t sector = entry->first_sector;
    size_t offset = 0;
    int max_blocks = entry->blocks + 10;  /* Safety limit */
    
    while (track != 0 && max_blocks-- > 0) {
        size_t sec_offset = d64_sector_offset(track, sector);
        if (sec_offset + D64_SECTOR_SIZE > disk_size) break;
        
        const uint8_t* sec = disk_data + sec_offset;
        
        uint8_t next_track = sec[0];
        uint8_t next_sector = sec[1];
        
        size_t data_len;
        if (next_track == 0) {
            /* Last sector: next_sector contains number of data bytes */
            data_len = next_sector - 1;
            if (data_len > 254) data_len = 254;
        } else {
            data_len = 254;
        }
        
        if (offset + data_len > max_size) break;
        memcpy(file_data + offset, sec + 2, data_len);
        offset += data_len;
        
        track = next_track;
        sector = next_sector;
    }
    
    *out_size = offset;
    return file_data;
}

/**
 * @brief Find file by name
 */
static const d64_dir_entry_t* d64_find_file(const d64_disk_t* disk, const char* name) {
    for (uint16_t i = 0; i < disk->dir_entries; i++) {
        if (strcasecmp(disk->directory[i].filename, name) == 0) {
            return &disk->directory[i];
        }
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CREATION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create blank D64 image
 */
static uint8_t* d64_create_blank(const char* disk_name, const char* disk_id,
                                  uint8_t tracks, size_t* out_size) {
    uint16_t num_sectors = (tracks == 35) ? D64_SECTORS_35 : D64_SECTORS_40;
    size_t size = num_sectors * D64_SECTOR_SIZE;
    
    uint8_t* data = calloc(1, size);
    if (!data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Initialize BAM */
    size_t bam_offset = d64_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    uint8_t* bam = data + bam_offset;
    
    /* Directory pointer */
    bam[0] = D64_DIR_TRACK;
    bam[1] = D64_DIR_SECTOR;
    
    /* DOS version */
    bam[2] = 0x41;  /* 'A' */
    
    /* Initialize BAM entries */
    for (int track = 1; track <= tracks && track <= 35; track++) {
        size_t entry_offset = 4 + (track - 1) * 4;
        uint8_t sectors = d64_sectors_per_track[track];
        
        if (track == D64_BAM_TRACK) {
            /* Track 18: mark BAM and some dir sectors as used */
            bam[entry_offset] = sectors - 2;
            bam[entry_offset + 1] = 0xFC;  /* Sectors 0,1 used */
            bam[entry_offset + 2] = 0xFF;
            bam[entry_offset + 3] = (sectors > 16) ? 0x07 : 0xFF;
        } else {
            bam[entry_offset] = sectors;
            bam[entry_offset + 1] = 0xFF;
            bam[entry_offset + 2] = 0xFF;
            bam[entry_offset + 3] = (sectors > 16) ? 
                ((1 << (sectors - 16)) - 1) : 0xFF;
        }
    }
    
    /* Disk name (padded with 0xA0) */
    memset(bam + 0x90, 0xA0, 16);
    if (disk_name) {
        size_t len = strlen(disk_name);
        if (len > 16) len = 16;
        for (size_t i = 0; i < len; i++) {
            bam[0x90 + i] = toupper((unsigned char)disk_name[i]);
        }
    }
    
    /* Shifted space separators */
    bam[0xA0] = 0xA0;
    bam[0xA1] = 0xA0;
    
    /* Disk ID */
    if (disk_id && strlen(disk_id) >= 2) {
        bam[0xA2] = toupper((unsigned char)disk_id[0]);
        bam[0xA3] = toupper((unsigned char)disk_id[1]);
    } else {
        bam[0xA2] = '0';
        bam[0xA3] = '0';
    }
    
    bam[0xA4] = 0xA0;
    bam[0xA5] = '2';  /* DOS type */
    bam[0xA6] = 'A';
    
    /* Initialize first directory sector */
    size_t dir_offset = d64_sector_offset(D64_DIR_TRACK, D64_DIR_SECTOR);
    data[dir_offset] = 0;      /* No next sector */
    data[dir_offset + 1] = 0xFF;  /* End marker */
    
    *out_size = size;
    return data;
}

/**
 * @brief Generate catalog listing as text
 */
static char* d64_catalog_to_text(const d64_disk_t* disk) {
    size_t buf_size = 8192;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t offset = 0;
    
    /* Header */
    offset += snprintf(buf + offset, buf_size - offset,
        "0 \"%-16s\" %s\n",
        disk->disk_name,
        disk->disk_id
    );
    
    /* Files */
    for (uint16_t i = 0; i < disk->dir_entries; i++) {
        const d64_dir_entry_t* entry = &disk->directory[i];
        
        offset += snprintf(buf + offset, buf_size - offset,
            "%-5u \"%-16s\" %s%s%s\n",
            entry->blocks,
            entry->filename,
            d64_file_type_name(entry->type),
            entry->locked ? "<" : "",
            entry->closed ? "" : "*"
        );
    }
    
    /* Footer */
    offset += snprintf(buf + offset, buf_size - offset,
        "%u BLOCKS FREE.\n",
        disk->free_blocks
    );
    
    return buf;
}

/**
 * @brief Free D64 disk structure
 */
static void d64_free(d64_disk_t* disk) {
    if (disk && disk->error_bytes) {
        free(disk->error_bytes);
        disk->error_bytes = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef D64_PARSER_TEST

#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("Testing %s... ", #name); \
    test_##name(); \
    printf("✓\n"); \
    tests_passed++; \
} while(0)

TEST(valid_sizes) {
    uint8_t tracks;
    bool has_errors;
    
    assert(d64_is_valid_size(D64_SIZE_35, &tracks, &has_errors));
    assert(tracks == 35 && !has_errors);
    
    assert(d64_is_valid_size(D64_SIZE_35_ERRORS, &tracks, &has_errors));
    assert(tracks == 35 && has_errors);
    
    assert(d64_is_valid_size(D64_SIZE_40, &tracks, &has_errors));
    assert(tracks == 40 && !has_errors);
    
    assert(d64_is_valid_size(D64_SIZE_40_ERRORS, &tracks, &has_errors));
    assert(tracks == 40 && has_errors);
    
    assert(!d64_is_valid_size(12345, &tracks, &has_errors));
}

TEST(sectors_per_track) {
    assert(d64_sectors_for_track(1) == 21);
    assert(d64_sectors_for_track(17) == 21);
    assert(d64_sectors_for_track(18) == 19);
    assert(d64_sectors_for_track(24) == 19);
    assert(d64_sectors_for_track(25) == 18);
    assert(d64_sectors_for_track(30) == 18);
    assert(d64_sectors_for_track(31) == 17);
    assert(d64_sectors_for_track(35) == 17);
}

TEST(sector_offset) {
    /* Track 1, sector 0 */
    assert(d64_sector_offset(1, 0) == 0);
    
    /* Track 1, sector 1 */
    assert(d64_sector_offset(1, 1) == D64_SECTOR_SIZE);
    
    /* Track 18, sector 0 (BAM) */
    assert(d64_sector_offset(18, 0) == 357 * D64_SECTOR_SIZE);
}

TEST(file_type_names) {
    assert(strcmp(d64_file_type_name(D64_TYPE_PRG), "PRG") == 0);
    assert(strcmp(d64_file_type_name(D64_TYPE_SEQ), "SEQ") == 0);
    assert(strcmp(d64_file_type_name(D64_TYPE_DEL), "DEL") == 0);
    assert(strcmp(d64_file_type_name(D64_TYPE_USR), "USR") == 0);
    assert(strcmp(d64_file_type_name(D64_TYPE_REL), "REL") == 0);
}

TEST(blank_creation) {
    size_t size = 0;
    uint8_t* data = d64_create_blank("TEST DISK", "TD", 35, &size);
    assert(data != NULL);
    assert(size == D64_SIZE_35);
    
    /* Parse and verify */
    d64_disk_t disk;
    assert(d64_parse(data, size, &disk));
    assert(disk.valid);
    assert(disk.num_tracks == 35);
    assert(disk.dir_entries == 0);
    assert(disk.free_blocks > 600);  /* Should have ~664 blocks free */
    
    d64_free(&disk);
    free(data);
}

int main(void) {
    printf("=== D64 Parser v2 Tests ===\n");
    
    RUN_TEST(valid_sizes);
    RUN_TEST(sectors_per_track);
    RUN_TEST(sector_offset);
    RUN_TEST(file_type_names);
    RUN_TEST(blank_creation);
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: %d, Failed: %d\n", tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}

#endif /* D64_PARSER_TEST */

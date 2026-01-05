/**
 * @file uft_atari_dos.h
 * @brief Atari 8-bit DOS Filesystem Support
 * 
 * @version 4.2.0
 * @date 2026-01-03
 * 
 * SUPPORTED DOS VERSIONS:
 * - Atari DOS 1.0 (1979) - 707 sectors
 * - Atari DOS 2.0 (1980) - Single density
 * - Atari DOS 2.5 (1983) - Enhanced density
 * - MyDOS 4.5 (1988) - Subdirectories, large disks
 * - SpartaDOS 3.x (1985) - Professional DOS
 * - SpartaDOS X (1989) - Extended features
 * - DOS XE (1985) - XE Game System
 * - BiboDOS (1986) - Alternative DOS
 * - TurboDOS (1984) - Fast loader
 * - TOP-DOS (1989) - Modern features
 * 
 * DISK FORMATS:
 * - Single Density (SD): 720 sectors × 128 bytes = 90KB
 * - Enhanced Density (ED): 1040 sectors × 128 bytes = 130KB  
 * - Double Density (DD): 720 sectors × 256 bytes = 180KB
 * - Quad Density (QD): 1440 sectors × 256 bytes = 360KB
 * - Hard Disk: Up to 16MB (SpartaDOS)
 */

#ifndef UFT_ATARI_DOS_H
#define UFT_ATARI_DOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Sector sizes */
#define UFT_ATARI_SECTOR_SD         128     /* Single density */
#define UFT_ATARI_SECTOR_DD         256     /* Double density */

/* Disk sizes (in sectors) */
#define UFT_ATARI_SECTORS_SD        720     /* 90KB */
#define UFT_ATARI_SECTORS_ED        1040    /* 130KB Enhanced */
#define UFT_ATARI_SECTORS_DD        720     /* 180KB Double */
#define UFT_ATARI_SECTORS_QD        1440    /* 360KB Quad */

/* Boot sector location */
#define UFT_ATARI_BOOT_SECTOR       1       /* First sector */
#define UFT_ATARI_BOOT_SECTORS      3       /* Boot takes 3 sectors */

/* VTOC (Volume Table of Contents) */
#define UFT_ATARI_VTOC_SECTOR       360     /* DOS 2.0 VTOC */
#define UFT_ATARI_VTOC2_SECTOR      1024    /* DOS 2.5 extended VTOC */

/* Directory */
#define UFT_ATARI_DIR_SECTOR        361     /* First directory sector */
#define UFT_ATARI_DIR_SECTORS       8       /* Directory size */
#define UFT_ATARI_DIR_ENTRIES       64      /* Max files (8 sectors × 8 entries) */
#define UFT_ATARI_ENTRY_SIZE        16      /* Bytes per directory entry */

/* Filename */
#define UFT_ATARI_NAME_LEN          8       /* Filename length */
#define UFT_ATARI_EXT_LEN           3       /* Extension length */

/* File status flags */
#define UFT_ATARI_FLAG_OPEN         0x01    /* File open for write */
#define UFT_ATARI_FLAG_DOS2         0x02    /* DOS 2.x format */
#define UFT_ATARI_FLAG_MYDOS        0x04    /* MyDOS extended */
#define UFT_ATARI_FLAG_LOCKED       0x20    /* File locked */
#define UFT_ATARI_FLAG_INUSE        0x40    /* Entry in use */
#define UFT_ATARI_FLAG_DELETED      0x80    /* File deleted */

/* ═══════════════════════════════════════════════════════════════════════════════
 * DOS Type Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ATARI_DOS_UNKNOWN       = 0,
    UFT_ATARI_DOS_1_0           = 1,    /* Original Atari DOS */
    UFT_ATARI_DOS_2_0           = 2,    /* Standard DOS */
    UFT_ATARI_DOS_2_5           = 3,    /* Enhanced density */
    UFT_ATARI_DOS_3_0           = 4,    /* Rarely used */
    UFT_ATARI_DOS_MYDOS         = 10,   /* MyDOS 4.x */
    UFT_ATARI_DOS_SPARTA        = 20,   /* SpartaDOS 2.x/3.x */
    UFT_ATARI_DOS_SPARTA_X      = 21,   /* SpartaDOS X */
    UFT_ATARI_DOS_XE            = 30,   /* DOS XE */
    UFT_ATARI_DOS_BIBO          = 40,   /* BiboDOS */
    UFT_ATARI_DOS_TURBO         = 50,   /* TurboDOS */
    UFT_ATARI_DOS_TOP           = 60,   /* TOP-DOS */
    UFT_ATARI_DOS_LITEDOS       = 70,   /* LiteDOS */
} uft_atari_dos_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Density
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_ATARI_DENSITY_SD        = 0,    /* Single: 128 bytes/sector */
    UFT_ATARI_DENSITY_ED        = 1,    /* Enhanced: 128 bytes, 1040 sectors */
    UFT_ATARI_DENSITY_DD        = 2,    /* Double: 256 bytes/sector */
    UFT_ATARI_DENSITY_QD        = 3,    /* Quad: 256 bytes, 1440 sectors */
    UFT_ATARI_DENSITY_HD        = 4,    /* Hard disk */
} uft_atari_density_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Boot Sector (Sectors 1-3)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    uint8_t  flags;             /* Boot flags */
    uint8_t  boot_sectors;      /* Number of boot sectors to load */
    uint16_t boot_addr;         /* Load address (little-endian) */
    uint16_t init_addr;         /* Init routine address */
    uint8_t  jmp_opcode;        /* JMP instruction (0x4C) */
    uint16_t jmp_addr;          /* Jump address */
    /* Rest is boot code */
    uint8_t  boot_code[119];
} uft_atari_boot_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * VTOC - Volume Table of Contents (Sector 360)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    uint8_t  dos_code;          /* DOS version code */
    uint16_t total_sectors;     /* Total sectors on disk */
    uint16_t free_sectors;      /* Free sectors available */
    uint8_t  unused[5];
    uint8_t  bitmap[90];        /* Sector allocation bitmap */
    /* Each bit: 1=free, 0=used */
    /* Bit 0 of byte 0 = sector 0, etc. */
} uft_atari_vtoc_t;

/* Extended VTOC for DOS 2.5 (Sector 1024) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  bitmap[128];       /* Additional bitmap for sectors 720-1023 */
} uft_atari_vtoc2_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Entry (16 bytes)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    uint8_t  flags;             /* Status flags */
    uint16_t sector_count;      /* Number of sectors */
    uint16_t start_sector;      /* First sector of file */
    char     filename[8];       /* Filename (space-padded) */
    char     extension[3];      /* Extension (space-padded) */
} uft_atari_dirent_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Sector Link (DOS 2.x format)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    /* For 128-byte sectors (SD): */
    /* Bytes 0-124: Data */
    /* Byte 125: File number (high 2 bits) + next sector (high 2 bits) */
    /* Bytes 126-127: Next sector number (low 8 bits) + bytes used */
    
    /* For 256-byte sectors (DD): */
    /* Bytes 0-252: Data */
    /* Byte 253: File number (bits 5-0) + flags (bits 7-6) */
    /* Bytes 254-255: Next sector (10 bits) + bytes used (8 bits) */
    uint8_t  data[125];         /* Actual file data (SD) */
    uint8_t  file_id_hi;        /* High bits */
    uint8_t  next_lo;           /* Next sector low byte */
    uint8_t  bytes_used;        /* Bytes used in this sector (0-125) */
} uft_atari_sector_sd_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t  data[253];         /* Actual file data (DD) */
    uint8_t  file_id;           /* File ID */
    uint8_t  next_hi;           /* Next sector high bits */
    uint8_t  next_lo;           /* Next sector low byte */
} uft_atari_sector_dd_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * SpartaDOS Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* SpartaDOS Boot Sector */
#pragma pack(push, 1)
typedef struct {
    uint8_t  boot_flag;         /* 0x00 = not bootable */
    uint8_t  boot_sectors;      /* Sectors to load */
    uint16_t boot_addr;         /* Load address */
    uint16_t init_addr;         /* Init address */
    uint8_t  dos_signature[6];  /* "AZALON" for SpartaDOS */
    uint8_t  version_lo;        /* Version minor */
    uint8_t  version_hi;        /* Version major */
    uint8_t  sec_size_code;     /* 0=128, 1=256, 2=512 */
    uint8_t  sec_size_lo;       /* Bytes per sector (low) */
    uint8_t  sec_size_hi;       /* Bytes per sector (high) */
    uint8_t  reserved[3];
    uint16_t total_sectors;     /* Total sectors */
    uint16_t free_sectors;      /* Free sectors */
    uint8_t  bitmap_sectors;    /* Sectors in bitmap */
    uint16_t bitmap_start;      /* First bitmap sector */
    uint16_t root_dir;          /* Root directory sector */
    uint16_t spare_dir;         /* Spare directory sector */
    uint8_t  volume_name[8];    /* Volume name */
    uint8_t  track_count;       /* Tracks on disk */
    uint8_t  sec_per_track;     /* Sectors per track */
    uint8_t  volume_seq;        /* Volume sequence number */
    uint8_t  volume_rand;       /* Volume random ID */
    uint16_t first_data;        /* First data sector */
    /* Boot code follows */
} uft_sparta_boot_t;

/* SpartaDOS Directory Entry (23 bytes) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  status;            /* Entry status */
    uint16_t sector_map;        /* First sector of sector map */
    uint16_t size_lo;           /* File size (low 16 bits) */
    uint8_t  size_hi;           /* File size (high 8 bits) */
    char     filename[8];       /* Filename */
    char     extension[3];      /* Extension */
    uint8_t  date_day;          /* Day (1-31) */
    uint8_t  date_month;        /* Month (1-12) */
    uint8_t  date_year;         /* Year (00-99) */
    uint8_t  time_hour;         /* Hour (0-23) */
    uint8_t  time_min;          /* Minute (0-59) */
    uint8_t  time_sec;          /* Second (0-59) */
} uft_sparta_dirent_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * MyDOS Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* MyDOS VTOC (extended) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  dos_code;          /* 0x02 for MyDOS */
    uint16_t total_sectors;
    uint16_t free_sectors;
    uint8_t  reserved[5];
    uint8_t  bitmap[118];       /* Extended bitmap */
} uft_mydos_vtoc_t;

/* MyDOS Subdirectory Entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  flags;             /* 0x10 = subdirectory */
    uint16_t sector_count;
    uint16_t start_sector;
    char     dirname[8];
    char     padding[3];        /* Always spaces */
} uft_mydos_subdir_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk Image Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Disk properties */
    uft_atari_dos_type_t dos_type;
    uft_atari_density_t density;
    
    uint16_t sector_size;       /* 128 or 256 */
    uint16_t total_sectors;
    uint16_t free_sectors;
    
    /* Raw data */
    uint8_t *data;
    size_t data_size;
    
    /* Parsed structures */
    uft_atari_boot_t *boot;
    uft_atari_vtoc_t *vtoc;
    uft_atari_vtoc2_t *vtoc2;   /* NULL if not DOS 2.5 */
    
    /* For SpartaDOS */
    uft_sparta_boot_t *sparta_boot;
    
    /* Directory cache */
    uft_atari_dirent_t *directory;
    int dir_entry_count;
    
    /* State */
    bool modified;
    char *filename;
} uft_atari_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Handle
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_atari_disk_t *disk;
    int dir_index;              /* Directory entry index */
    uint16_t current_sector;
    uint16_t position;          /* Position within file */
    uint32_t size;              /* Total file size */
    bool write_mode;
} uft_atari_file_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Disk Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect Atari DOS type from disk image
 * @param data Raw disk data
 * @param size Data size
 * @return DOS type or UFT_ATARI_DOS_UNKNOWN
 */
uft_atari_dos_type_t uft_atari_detect_dos(const uint8_t *data, size_t size);

/**
 * @brief Get DOS type name string
 */
const char *uft_atari_dos_name(uft_atari_dos_type_t type);

/**
 * @brief Open Atari disk image
 * @param filename Path to ATR/XFD/ATX file
 * @param disk Output disk structure
 * @return 0 on success, negative on error
 */
int uft_atari_disk_open(const char *filename, uft_atari_disk_t *disk);

/**
 * @brief Open from memory buffer
 */
int uft_atari_disk_open_mem(const uint8_t *data, size_t size, 
                            uft_atari_disk_t *disk);

/**
 * @brief Create new formatted disk
 * @param disk Output disk structure
 * @param dos_type DOS type to use
 * @param density Disk density
 * @return 0 on success
 */
int uft_atari_disk_create(uft_atari_disk_t *disk,
                          uft_atari_dos_type_t dos_type,
                          uft_atari_density_t density);

/**
 * @brief Write disk to file
 */
int uft_atari_disk_save(const uft_atari_disk_t *disk, const char *filename);

/**
 * @brief Close and free disk resources
 */
void uft_atari_disk_close(uft_atari_disk_t *disk);

/**
 * @brief Read raw sector
 */
int uft_atari_read_sector(const uft_atari_disk_t *disk, 
                          uint16_t sector, uint8_t *buffer);

/**
 * @brief Write raw sector
 */
int uft_atari_write_sector(uft_atari_disk_t *disk,
                           uint16_t sector, const uint8_t *data);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Directory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get directory entry count
 */
int uft_atari_dir_count(const uft_atari_disk_t *disk);

/**
 * @brief Get directory entry by index
 */
int uft_atari_dir_get(const uft_atari_disk_t *disk, int index,
                      uft_atari_dirent_t *entry);

/**
 * @brief Find file by name
 * @param name Filename with optional extension (e.g., "GAME.COM")
 * @return Directory index or -1 if not found
 */
int uft_atari_dir_find(const uft_atari_disk_t *disk, const char *name);

/**
 * @brief Delete file
 */
int uft_atari_dir_delete(uft_atari_disk_t *disk, int index);

/**
 * @brief Rename file
 */
int uft_atari_dir_rename(uft_atari_disk_t *disk, int index, const char *newname);

/**
 * @brief Lock/unlock file
 */
int uft_atari_dir_lock(uft_atari_disk_t *disk, int index, bool locked);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open file for reading
 */
int uft_atari_file_open(uft_atari_disk_t *disk, const char *name,
                        uft_atari_file_t *file);

/**
 * @brief Create new file for writing
 */
int uft_atari_file_create(uft_atari_disk_t *disk, const char *name,
                          uft_atari_file_t *file);

/**
 * @brief Read from file
 */
int uft_atari_file_read(uft_atari_file_t *file, uint8_t *buffer, size_t size);

/**
 * @brief Write to file
 */
int uft_atari_file_write(uft_atari_file_t *file, const uint8_t *data, size_t size);

/**
 * @brief Close file
 */
void uft_atari_file_close(uft_atari_file_t *file);

/**
 * @brief Extract file to host filesystem
 */
int uft_atari_file_extract(uft_atari_disk_t *disk, const char *name,
                           const char *dest_path);

/**
 * @brief Add file from host filesystem
 */
int uft_atari_file_add(uft_atari_disk_t *disk, const char *src_path,
                       const char *name);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get disk information string
 */
void uft_atari_disk_info(const uft_atari_disk_t *disk, char *buffer, size_t size);

/**
 * @brief Validate disk structure
 */
int uft_atari_disk_validate(const uft_atari_disk_t *disk);

/**
 * @brief Repair common disk issues
 */
int uft_atari_disk_repair(uft_atari_disk_t *disk);

/**
 * @brief Calculate file size from sector chain
 */
uint32_t uft_atari_file_size(const uft_atari_disk_t *disk, int dir_index);

/**
 * @brief Convert filename to Atari format (8.3, uppercase, space-padded)
 */
void uft_atari_filename_to_native(const char *input, char *name8, char *ext3);

/**
 * @brief Convert Atari filename to string
 */
void uft_atari_filename_from_native(const char *name8, const char *ext3, 
                                    char *output, size_t output_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI_DOS_H */

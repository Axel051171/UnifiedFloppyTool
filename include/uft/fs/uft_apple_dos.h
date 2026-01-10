/**
 * @file uft_apple_dos.h
 * @brief Apple II DOS 3.3 and ProDOS Filesystem Layer
 * @version 3.7.0
 * 
 * Complete Apple II filesystem implementation:
 * - DOS 3.3: VTOC, Catalog, T/S Lists
 * - ProDOS: Volume Directory, Subdirectories, Sparse Files
 * - File types: A/B/T/I/R/S (Integer/Applesoft/Binary/Text/Relocatable/System)
 * - Operations: list, extract, inject, delete, rename
 * - Image formats: DSK, DO, PO, 2IMG, NIB (sector-level)
 * 
 * @note Part of UnifiedFloppyTool preservation suite
 */

#ifndef UFT_APPLE_DOS_H
#define UFT_APPLE_DOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include "uft/uft_compiler.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Sector size */
#define UFT_APPLE_SECTOR_SIZE       256

/** Track count for standard disk */
#define UFT_APPLE_TRACKS            35

/** Sectors per track (DOS 3.3 / ProDOS) */
#define UFT_APPLE_SECTORS_PER_TRACK 16

/** Total sectors on standard disk */
#define UFT_APPLE_TOTAL_SECTORS     560

/** DOS 3.3 VTOC track */
#define UFT_DOS33_VTOC_TRACK        17

/** DOS 3.3 VTOC sector */
#define UFT_DOS33_VTOC_SECTOR       0

/** DOS 3.3 catalog track */
#define UFT_DOS33_CATALOG_TRACK     17

/** DOS 3.3 catalog first sector */
#define UFT_DOS33_CATALOG_SECTOR    15

/** ProDOS key block */
#define UFT_PRODOS_KEY_BLOCK        2

/** Maximum filename length (DOS 3.3) */
#define UFT_DOS33_MAX_NAME          30

/** Maximum filename length (ProDOS) */
#define UFT_PRODOS_MAX_NAME         15

/** Maximum path length */
#define UFT_APPLE_MAX_PATH          128

/*===========================================================================
 * Filesystem Types
 *===========================================================================*/

/** Apple filesystem type */
typedef enum {
    UFT_APPLE_FS_UNKNOWN = 0,
    UFT_APPLE_FS_DOS33   = 1,     /**< Apple DOS 3.3 */
    UFT_APPLE_FS_DOS32   = 2,     /**< Apple DOS 3.2 (13 sectors) */
    UFT_APPLE_FS_PRODOS  = 3,     /**< ProDOS */
    UFT_APPLE_FS_PASCAL  = 4,     /**< Apple Pascal */
    UFT_APPLE_FS_CPM     = 5      /**< CP/M on Apple II */
} uft_apple_fs_t;

/** Sector interleave type */
typedef enum {
    UFT_APPLE_ORDER_DOS  = 0,     /**< DOS 3.3 sector order (DSK/DO) */
    UFT_APPLE_ORDER_PRODOS = 1,   /**< ProDOS sector order (PO) */
    UFT_APPLE_ORDER_PHYSICAL = 2  /**< Physical sector order */
} uft_apple_order_t;

/*===========================================================================
 * DOS 3.3 File Types
 *===========================================================================*/

/** DOS 3.3 file type codes */
typedef enum {
    UFT_DOS33_TYPE_TEXT      = 0x00,   /**< T - Text file */
    UFT_DOS33_TYPE_INTEGER   = 0x01,   /**< I - Integer BASIC */
    UFT_DOS33_TYPE_APPLESOFT = 0x02,   /**< A - Applesoft BASIC */
    UFT_DOS33_TYPE_BINARY    = 0x04,   /**< B - Binary */
    UFT_DOS33_TYPE_S         = 0x08,   /**< S - Type S */
    UFT_DOS33_TYPE_REL       = 0x10,   /**< R - Relocatable */
    UFT_DOS33_TYPE_AA        = 0x20,   /**< A - Type A */
    UFT_DOS33_TYPE_BB        = 0x40    /**< B - Type B */
} uft_dos33_type_t;

/** DOS 3.3 file type characters */
static const char DOS33_TYPE_CHARS[] = "TIAB SRA B";

/*===========================================================================
 * ProDOS File Types
 *===========================================================================*/

/** ProDOS file type codes */
typedef enum {
    UFT_PRODOS_TYPE_UNK = 0x00,   /**< Unknown */
    UFT_PRODOS_TYPE_BAD = 0x01,   /**< Bad block file */
    UFT_PRODOS_TYPE_TXT = 0x04,   /**< ASCII text */
    UFT_PRODOS_TYPE_BIN = 0x06,   /**< Binary */
    UFT_PRODOS_TYPE_DIR = 0x0F,   /**< Directory */
    UFT_PRODOS_TYPE_ADB = 0x19,   /**< AppleWorks Database */
    UFT_PRODOS_TYPE_AWP = 0x1A,   /**< AppleWorks Word Processor */
    UFT_PRODOS_TYPE_ASP = 0x1B,   /**< AppleWorks Spreadsheet */
    UFT_PRODOS_TYPE_BAS = 0xFC,   /**< Applesoft BASIC */
    UFT_PRODOS_TYPE_VAR = 0xFD,   /**< Applesoft Variables */
    UFT_PRODOS_TYPE_REL = 0xFE,   /**< Relocatable code */
    UFT_PRODOS_TYPE_SYS = 0xFF    /**< ProDOS system file */
} uft_prodos_type_t;

/** ProDOS storage types */
typedef enum {
    UFT_PRODOS_STORAGE_DELETED  = 0x0,
    UFT_PRODOS_STORAGE_SEEDLING = 0x1,   /**< 1 block (≤512 bytes) */
    UFT_PRODOS_STORAGE_SAPLING  = 0x2,   /**< Index block (≤128KB) */
    UFT_PRODOS_STORAGE_TREE     = 0x3,   /**< Master index (≤16MB) */
    UFT_PRODOS_STORAGE_PASCAL   = 0x4,   /**< Pascal area */
    UFT_PRODOS_STORAGE_SUBDIR   = 0xD,   /**< Subdirectory header */
    UFT_PRODOS_STORAGE_VOLDIR   = 0xE,   /**< Volume directory header */
    UFT_PRODOS_STORAGE_VOLKEY   = 0xF    /**< Volume directory key */
} uft_prodos_storage_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

#define UFT_APPLE_ERR_INVALID     -1
#define UFT_APPLE_ERR_NOMEM       -2
#define UFT_APPLE_ERR_IO          -3
#define UFT_APPLE_ERR_NOTFOUND    -4
#define UFT_APPLE_ERR_EXISTS      -5
#define UFT_APPLE_ERR_DISKFULL    -6
#define UFT_APPLE_ERR_READONLY    -7
#define UFT_APPLE_ERR_BADCHAIN    -8
#define UFT_APPLE_ERR_BADTYPE     -9

/*===========================================================================
 * DOS 3.3 Structures
 *===========================================================================*/

/**
 * @brief DOS 3.3 VTOC (Volume Table of Contents)
 * Located at Track 17, Sector 0
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  unused1;             /**< 0x00: Unused (usually 0) */
    uint8_t  catalog_track;       /**< 0x01: First catalog track */
    uint8_t  catalog_sector;      /**< 0x02: First catalog sector */
    uint8_t  dos_version;         /**< 0x03: DOS version (3 = DOS 3.3) */
    uint8_t  unused2[2];          /**< 0x04-05: Unused */
    uint8_t  volume_number;       /**< 0x06: Volume number (1-254) */
    uint8_t  unused3[32];         /**< 0x07-26: Unused */
    uint8_t  max_ts_pairs;        /**< 0x27: Max T/S pairs per sector (122) */
    uint8_t  unused4[8];          /**< 0x28-2F: Unused */
    uint8_t  last_track_alloc;    /**< 0x30: Last track allocated (+/-1) */
    int8_t   alloc_direction;     /**< 0x31: Allocation direction (+1/-1) */
    uint8_t  unused5[2];          /**< 0x32-33: Unused */
    uint8_t  tracks_per_disk;     /**< 0x34: Tracks per disk */
    uint8_t  sectors_per_track;   /**< 0x35: Sectors per track */
    uint16_t bytes_per_sector;    /**< 0x36-37: Bytes per sector (LE) */
    uint8_t  bitmap[200];         /**< 0x38-FF: Free sector bitmap */
} uft_dos33_vtoc_t;
UFT_PACK_END

/**
 * @brief DOS 3.3 Catalog Entry
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  ts_list_track;       /**< 0x00: T/S list track (0 = deleted) */
    uint8_t  ts_list_sector;      /**< 0x01: T/S list sector */
    uint8_t  file_type;           /**< 0x02: File type + flags */
    char     filename[30];        /**< 0x03-20: Filename (high bit set) */
    uint16_t sector_count;        /**< 0x21-22: Sector count (LE) */
} uft_dos33_entry_t;
UFT_PACK_END

/**
 * @brief DOS 3.3 Catalog Sector
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  unused;              /**< 0x00: Unused */
    uint8_t  next_track;          /**< 0x01: Next catalog track */
    uint8_t  next_sector;         /**< 0x02: Next catalog sector */
    uint8_t  reserved[8];         /**< 0x03-0A: Reserved */
    uft_dos33_entry_t entries[7]; /**< 0x0B-FF: File entries */
} uft_dos33_catalog_t;
UFT_PACK_END

/**
 * @brief DOS 3.3 Track/Sector List
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  unused;              /**< 0x00: Unused */
    uint8_t  next_track;          /**< 0x01: Next T/S list track */
    uint8_t  next_sector;         /**< 0x02: Next T/S list sector */
    uint8_t  reserved[2];         /**< 0x03-04: Reserved */
    uint16_t offset;              /**< 0x05-06: Sector offset (LE) */
    uint8_t  reserved2[5];        /**< 0x07-0B: Reserved */
    struct {
        uint8_t track;
        uint8_t sector;
    } pairs[122];                 /**< 0x0C-FF: T/S pairs */
} uft_dos33_tslist_t;
UFT_PACK_END

/*===========================================================================
 * ProDOS Structures
 *===========================================================================*/

/**
 * @brief ProDOS Date/Time
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t date;                /**< YYYYYYYMMMMDDDDD */
    uint16_t time;                /**< 000HHHHH00MMMMMM */
} uft_prodos_datetime_t;
UFT_PACK_END

/**
 * @brief ProDOS Directory Entry (39 bytes)
 */
UFT_PACK_BEGIN
typedef struct {
    uint8_t  storage_type_len;    /**< 0x00: Storage type (hi) + name length (lo) */
    char     filename[15];        /**< 0x01-0F: Filename */
    uint8_t  file_type;           /**< 0x10: File type */
    uint16_t key_pointer;         /**< 0x11-12: Key block / first block */
    uint16_t blocks_used;         /**< 0x13-14: Blocks used */
    uint8_t  eof[3];              /**< 0x15-17: EOF (24-bit, LE) */
    uft_prodos_datetime_t created;  /**< 0x18-1B: Creation date/time */
    uint8_t  version;             /**< 0x1C: Version */
    uint8_t  min_version;         /**< 0x1D: Minimum version */
    uint8_t  access;              /**< 0x1E: Access bits */
    uint16_t aux_type;            /**< 0x1F-20: Auxiliary type */
    uft_prodos_datetime_t modified; /**< 0x21-24: Modification date/time */
    uint16_t header_pointer;      /**< 0x25-26: Header block pointer */
} uft_prodos_entry_t;
UFT_PACK_END

/**
 * @brief ProDOS Directory Block Header
 */
UFT_PACK_BEGIN
typedef struct {
    uint16_t prev_block;          /**< 0x00-01: Previous block */
    uint16_t next_block;          /**< 0x02-03: Next block */
    uint8_t  storage_type_len;    /**< 0x04: Storage type + name length */
    char     name[15];            /**< 0x05-13: Directory/Volume name */
    uint8_t  reserved[8];         /**< 0x14-1B: Reserved */
    uft_prodos_datetime_t created;  /**< 0x1C-1F: Creation date/time */
    uint8_t  version;             /**< 0x20: Version */
    uint8_t  min_version;         /**< 0x21: Minimum version */
    uint8_t  access;              /**< 0x22: Access bits */
    uint8_t  entry_length;        /**< 0x23: Entry length (39) */
    uint8_t  entries_per_block;   /**< 0x24: Entries per block (13) */
    uint16_t file_count;          /**< 0x25-26: Active file count */
    uint16_t bitmap_pointer;      /**< 0x27-28: Bitmap block pointer */
    uint16_t total_blocks;        /**< 0x29-2A: Total blocks */
} uft_prodos_vol_header_t;
UFT_PACK_END

/*===========================================================================
 * Runtime Structures
 *===========================================================================*/

/**
 * @brief Parsed file entry (unified)
 */
typedef struct {
    char     name[UFT_APPLE_MAX_PATH];  /**< Full path or filename */
    uint8_t  file_type;           /**< File type code */
    char     type_char;           /**< Type character (T/I/A/B/S/R/etc) */
    uint32_t size;                /**< File size in bytes */
    uint16_t aux_type;            /**< Load address (BIN) or record length */
    uint16_t blocks;              /**< Blocks/sectors used */
    time_t   created;             /**< Creation time */
    time_t   modified;            /**< Modification time */
    bool     locked;              /**< File is locked/protected */
    bool     is_directory;        /**< Is a directory (ProDOS) */
    
    /* Internal */
    uint16_t key_block;           /**< First data block / T/S list */
    uint8_t  storage_type;        /**< ProDOS storage type */
    uint16_t dir_block;           /**< Directory block containing entry */
    uint8_t  entry_index;         /**< Index within directory block */
} uft_apple_entry_t;

/**
 * @brief Directory listing
 */
typedef struct {
    uft_apple_entry_t *entries;
    size_t count;
    size_t capacity;
    char path[UFT_APPLE_MAX_PATH];
    uint16_t dir_block;           /**< Directory block (ProDOS) */
} uft_apple_dir_t;

/**
 * @brief Detection result
 */
typedef struct {
    bool valid;
    uft_apple_fs_t fs_type;
    uft_apple_order_t order;
    int confidence;
    char volume_name[16];
    uint16_t total_blocks;
    uint16_t free_blocks;
    char description[64];
} uft_apple_detect_t;

/**
 * @brief Filesystem context
 */
typedef struct {
    uint8_t *data;
    size_t data_size;
    bool owns_data;
    bool modified;
    bool read_only;
    
    uft_apple_fs_t fs_type;
    uft_apple_order_t order;
    
    /* DOS 3.3 specific */
    uft_dos33_vtoc_t vtoc;
    
    /* ProDOS specific */
    char volume_name[16];
    uint16_t total_blocks;
    uint16_t bitmap_block;
    
    /* Sector interleave tables */
    const uint8_t *sector_map;
} uft_apple_ctx_t;

/*===========================================================================
 * API - Lifecycle
 *===========================================================================*/

/**
 * @brief Create filesystem context
 */
uft_apple_ctx_t *uft_apple_create(void);

/**
 * @brief Destroy filesystem context
 */
void uft_apple_destroy(uft_apple_ctx_t *ctx);

/**
 * @brief Open disk image
 */
int uft_apple_open(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size, bool copy);

/**
 * @brief Open from file
 */
int uft_apple_open_file(uft_apple_ctx_t *ctx, const char *filename);

/**
 * @brief Save to file
 */
int uft_apple_save(uft_apple_ctx_t *ctx, const char *filename);

/**
 * @brief Close image
 */
void uft_apple_close(uft_apple_ctx_t *ctx);

/*===========================================================================
 * API - Detection
 *===========================================================================*/

/**
 * @brief Detect filesystem type
 */
int uft_apple_detect(const uint8_t *data, size_t size, uft_apple_detect_t *result);

/**
 * @brief Get volume name
 */
int uft_apple_get_volume_name(const uft_apple_ctx_t *ctx, char *name, size_t size);

/*===========================================================================
 * API - Sector Access
 *===========================================================================*/

/**
 * @brief Read sector (track/sector addressing)
 */
int uft_apple_read_sector(const uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector,
                          uint8_t *buffer);

/**
 * @brief Write sector (track/sector addressing)
 */
int uft_apple_write_sector(uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector,
                           const uint8_t *buffer);

/**
 * @brief Read block (ProDOS block addressing)
 */
int uft_apple_read_block(const uft_apple_ctx_t *ctx, uint16_t block, uint8_t *buffer);

/**
 * @brief Write block
 */
int uft_apple_write_block(uft_apple_ctx_t *ctx, uint16_t block, const uint8_t *buffer);

/*===========================================================================
 * API - Directory Operations
 *===========================================================================*/

/**
 * @brief Initialize directory structure
 */
void uft_apple_dir_init(uft_apple_dir_t *dir);

/**
 * @brief Free directory structure
 */
void uft_apple_dir_free(uft_apple_dir_t *dir);

/**
 * @brief Read directory (DOS 3.3: catalog, ProDOS: directory)
 */
int uft_apple_read_dir(const uft_apple_ctx_t *ctx, const char *path, uft_apple_dir_t *dir);

/**
 * @brief Find file
 */
int uft_apple_find(const uft_apple_ctx_t *ctx, const char *path, uft_apple_entry_t *entry);

/**
 * @brief Iterate over entries
 */
typedef int (*uft_apple_dir_callback_t)(const uft_apple_entry_t *entry, void *user_data);
int uft_apple_foreach(const uft_apple_ctx_t *ctx, const char *path,
                      uft_apple_dir_callback_t callback, void *user_data);

/*===========================================================================
 * API - File Operations
 *===========================================================================*/

/**
 * @brief Extract file to memory
 */
uint8_t *uft_apple_extract(const uft_apple_ctx_t *ctx, const char *path,
                           uint8_t *buffer, size_t *size);

/**
 * @brief Extract file to disk
 */
int uft_apple_extract_to_file(const uft_apple_ctx_t *ctx, const char *path,
                              const char *dest_path);

/**
 * @brief Inject file
 */
int uft_apple_inject(uft_apple_ctx_t *ctx, const char *path, uint8_t file_type,
                     uint16_t aux_type, const uint8_t *data, size_t size);

/**
 * @brief Delete file
 */
int uft_apple_delete(uft_apple_ctx_t *ctx, const char *path);

/**
 * @brief Rename file
 */
int uft_apple_rename(uft_apple_ctx_t *ctx, const char *old_path, const char *new_path);

/**
 * @brief Lock/unlock file
 */
int uft_apple_set_locked(uft_apple_ctx_t *ctx, const char *path, bool locked);

/**
 * @brief Create directory (ProDOS only)
 */
int uft_apple_mkdir(uft_apple_ctx_t *ctx, const char *path);

/*===========================================================================
 * API - Bitmap/Free Space
 *===========================================================================*/

/**
 * @brief Get free sector/block count
 */
int uft_apple_get_free(const uft_apple_ctx_t *ctx, uint16_t *free_count);

/**
 * @brief Allocate sector (DOS 3.3)
 */
int uft_apple_alloc_sector(uft_apple_ctx_t *ctx, uint8_t *track, uint8_t *sector);

/**
 * @brief Free sector (DOS 3.3)
 */
int uft_apple_free_sector(uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector);

/**
 * @brief Allocate block (ProDOS)
 */
int uft_apple_alloc_block(uft_apple_ctx_t *ctx, uint16_t *block);

/**
 * @brief Free block (ProDOS)
 */
int uft_apple_free_block(uft_apple_ctx_t *ctx, uint16_t block);

/*===========================================================================
 * API - Utilities
 *===========================================================================*/

/**
 * @brief Convert DOS 3.3 file type to character
 */
char uft_dos33_type_char(uint8_t type);

/**
 * @brief Convert ProDOS file type to string
 */
const char *uft_prodos_type_string(uint8_t type);

/**
 * @brief Convert ProDOS date/time to Unix time
 */
time_t uft_prodos_to_unix_time(uft_prodos_datetime_t dt);

/**
 * @brief Convert Unix time to ProDOS date/time
 */
uft_prodos_datetime_t uft_prodos_from_unix_time(time_t t);

/**
 * @brief Print directory listing
 */
void uft_apple_print_dir(const uft_apple_ctx_t *ctx, const char *path, FILE *fp);

/**
 * @brief Generate JSON report
 */
size_t uft_apple_to_json(const uft_apple_ctx_t *ctx, char *buffer, size_t size);

/**
 * @brief Get error message
 */
const char *uft_apple_strerror(int error);

/*===========================================================================
 * API - Image Creation
 *===========================================================================*/

/**
 * @brief Create new DOS 3.3 image
 */
int uft_apple_create_dos33(const char *filename, uint8_t volume);

/**
 * @brief Create new ProDOS image
 */
int uft_apple_create_prodos(const char *filename, const char *volume_name,
                            uint16_t blocks);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_DOS_H */

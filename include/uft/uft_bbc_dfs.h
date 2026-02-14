/**
 * @file uft_bbc_dfs.h
 * @brief BBC Micro DFS (Disc Filing System) Support
 * 
 * Based on:
 * - bbctapedisc by W.H.Scholten, R.Schmidt, Jon Welch et al.
 * - BBC Micro Advanced User Guide (AUG)
 * 
 * Supports:
 * - Acorn DFS (standard BBC disc format)
 * - Watford DFS (62 files)
 * - ADFS (Acorn Advanced DFS)
 * - SSD/DSD disc images
 */

#ifndef UFT_BBC_DFS_H
#define UFT_BBC_DFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * BBC DFS Disk Geometry
 *===========================================================================*/

/** Standard DFS disk parameters */
#define UFT_DFS_SECTOR_SIZE         256     /**< Bytes per sector */
#define UFT_DFS_SECTORS_PER_TRACK   10      /**< Sectors per track */
#define UFT_DFS_TRACKS_40           40      /**< 40-track disk */
#define UFT_DFS_TRACKS_80           80      /**< 80-track disk */

/** Disk image sizes */
#define UFT_DFS_SS40_SECTORS        400     /**< Single-sided 40-track */
#define UFT_DFS_SS80_SECTORS        800     /**< Single-sided 80-track */
#define UFT_DFS_DS40_SECTORS        800     /**< Double-sided 40-track */
#define UFT_DFS_DS80_SECTORS        1600    /**< Double-sided 80-track */

#define UFT_DFS_SS40_SIZE           (400 * 256)     /**< 100KB */
#define UFT_DFS_SS80_SIZE           (800 * 256)     /**< 200KB */
#define UFT_DFS_DS40_SIZE           (800 * 256)     /**< 200KB */
#define UFT_DFS_DS80_SIZE           (1600 * 256)    /**< 400KB */

/** Catalog location */
#define UFT_DFS_CAT_SECTOR0         0       /**< First catalog sector */
#define UFT_DFS_CAT_SECTOR1         1       /**< Second catalog sector */

/*===========================================================================
 * DFS Directory Structure
 *===========================================================================*/

/** Maximum files per catalog */
#define UFT_DFS_MAX_FILES           31      /**< Standard DFS */
#define UFT_DFS_MAX_FILES_WATFORD   62      /**< Watford DFS */

#define UFT_DFS_FILENAME_LEN        7       /**< Filename length (excl. dir) */
#define UFT_DFS_ENTRY_SIZE          8       /**< Bytes per catalog entry */

/** Boot options (stored in sector 1, byte 6, bits 4-5) */
typedef enum {
    UFT_DFS_BOOT_NONE   = 0,    /**< No boot action */
    UFT_DFS_BOOT_LOAD   = 1,    /**< *LOAD $.!BOOT */
    UFT_DFS_BOOT_RUN    = 2,    /**< *RUN $.!BOOT */
    UFT_DFS_BOOT_EXEC   = 3     /**< *EXEC $.!BOOT */
} uft_dfs_boot_t;

/*===========================================================================
 * DFS Catalog Structure
 *===========================================================================*/

/**
 * @brief DFS Catalog Sector 0 (256 bytes)
 * 
 * Contains filenames and directory letters for up to 31 files.
 * Entry 0 is at offset 8, each entry is 8 bytes.
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  title1[8];         /**< Disk title (first 8 chars) */
    /**< Followed by file entries, 8 bytes each:
     *   - Bytes 0-6: Filename (space-padded)
     *   - Byte 7: Directory (bit 7 = locked flag)
     */
    uint8_t  entries[248];      /**< File entries (31 max) */
} uft_dfs_cat0_t;
#pragma pack(pop)

/**
 * @brief DFS Catalog Sector 1 (256 bytes)
 * 
 * Contains disk title continuation and file metadata.
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  title2[4];         /**< Disk title (last 4 chars) */
    uint8_t  sequence;          /**< Disk sequence number (BCD) */
    uint8_t  num_entries;       /**< Number of catalog entries * 8 */
    uint8_t  opt_sectors_hi;    /**< Boot option (bits 4-5) + sectors (bits 0-1) */
    uint8_t  sectors_lo;        /**< Total sectors on disk (low byte) */
    /**< Followed by file info entries, 8 bytes each:
     *   - Bytes 0-1: Load address (low)
     *   - Bytes 2-3: Exec address (low)
     *   - Bytes 4-5: File length (low)
     *   - Byte 6: Mixed bits (see below)
     *   - Byte 7: Start sector (low)
     */
    uint8_t  info[248];         /**< File info entries */
} uft_dfs_cat1_t;
#pragma pack(pop)

/**
 * @brief DFS File Catalog Entry (combined from sectors 0 and 1)
 */
typedef struct {
    char     filename[8];       /**< 7-char name + null */
    char     directory;         /**< Directory letter (usually '$') */
    bool     locked;            /**< File is locked */
    uint32_t load_addr;         /**< Load address (18-bit) */
    uint32_t exec_addr;         /**< Exec address (18-bit) */
    uint32_t length;            /**< File length (18-bit) */
    uint16_t start_sector;      /**< Start sector (10-bit) */
} uft_dfs_file_entry_t;

/*===========================================================================
 * DFS Mixed Bits Byte (Catalog Sector 1, Entry Byte 6)
 *===========================================================================*/

/**
 * Mixed bits byte layout:
 * 
 * Bits 0-1: Start sector (bits 8-9)
 * Bits 2-3: Load address (bits 16-17)
 * Bits 4-5: File length (bits 16-17)
 * Bits 6-7: Exec address (bits 16-17)
 */

#define UFT_DFS_MIXED_START_HI(m)   ((m) & 0x03)
#define UFT_DFS_MIXED_LOAD_HI(m)    (((m) >> 2) & 0x03)
#define UFT_DFS_MIXED_LEN_HI(m)     (((m) >> 4) & 0x03)
#define UFT_DFS_MIXED_EXEC_HI(m)    (((m) >> 6) & 0x03)

/** Create mixed bits byte */
#define UFT_DFS_MAKE_MIXED(start, load, len, exec) \
    ((((start) >> 8) & 0x03) | \
     ((((load) >> 14) & 0x03) << 2) | \
     ((((len) >> 12) & 0x03) << 4) | \
     ((((exec) >> 10) & 0x03) << 6))

/*===========================================================================
 * ADFS (Advanced Disc Filing System)
 *===========================================================================*/

/** ADFS disk identifiers */
#define UFT_ADFS_OLD_MAP_SIGNATURE  0x00    /**< Old map format */
#define UFT_ADFS_NEW_MAP_SIGNATURE  0x01    /**< New map format */

/** ADFS sector sizes */
#define UFT_ADFS_SECTOR_256         256
#define UFT_ADFS_SECTOR_512         512
#define UFT_ADFS_SECTOR_1024        1024

/** ADFS formats */
typedef enum {
    UFT_ADFS_S      = 0,    /**< 160KB */
    UFT_ADFS_M      = 1,    /**< 320KB */
    UFT_ADFS_L      = 2,    /**< 640KB (interleaved) */
    UFT_ADFS_D      = 3,    /**< 800KB (hard disc) */
    UFT_ADFS_E      = 4,    /**< 800KB (new format) */
    UFT_ADFS_F      = 5,    /**< 1.6MB (new format) */
    UFT_ADFS_G      = 6,    /**< Hard disc (big) */
    UFT_ADFS_PLUS   = 7     /**< ADFS+ */
} uft_adfs_format_t;

/**
 * @brief ADFS Old Map Free Space Entry
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  start[3];          /**< Start sector (24-bit, little-endian) */
    uint8_t  length[3];         /**< Length in sectors (24-bit, little-endian) */
} uft_adfs_free_entry_t;
#pragma pack(pop)

/**
 * @brief ADFS Old Directory Entry (26 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  name[10];          /**< Filename (bit 7 of byte 0 = read perm) */
    uint32_t load_addr;         /**< Load address */
    uint32_t exec_addr;         /**< Exec address */
    uint32_t length;            /**< File length */
    uint8_t  start[3];          /**< Start sector (24-bit) */
} uft_adfs_dir_entry_t;
#pragma pack(pop)

#define UFT_ADFS_ENTRY_SIZE         26
#define UFT_ADFS_DIR_ENTRIES        47      /**< Max entries per directory */

/** ADFS attributes */
#define UFT_ADFS_ATTR_READ          0x01    /**< Owner read */
#define UFT_ADFS_ATTR_WRITE         0x02    /**< Owner write */
#define UFT_ADFS_ATTR_LOCKED        0x04    /**< Locked */
#define UFT_ADFS_ATTR_DIRECTORY     0x08    /**< Is directory */
#define UFT_ADFS_ATTR_EXEC          0x10    /**< Owner execute */
#define UFT_ADFS_ATTR_PUBLIC_READ   0x20    /**< Public read */
#define UFT_ADFS_ATTR_PUBLIC_WRITE  0x40    /**< Public write */
#define UFT_ADFS_ATTR_PUBLIC_EXEC   0x80    /**< Public execute */

/*===========================================================================
 * BBC Tape Format
 *===========================================================================*/

/** Tape block header */
#define UFT_BBC_TAPE_SYNC_BYTE      0x2A    /**< Block sync byte '*' */
#define UFT_BBC_TAPE_FILENAME_LEN   10      /**< Max filename length */

/** Tape block flags */
#define UFT_BBC_TAPE_FLAG_LAST      0x80    /**< Last block of file */
#define UFT_BBC_TAPE_FLAG_LOCKED    0x01    /**< File is locked */

/**
 * @brief BBC Tape Block Header
 * 
 * Stored after sync byte (0x2A) and filename.
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t load_addr;         /**< Load address (32-bit) */
    uint32_t exec_addr;         /**< Exec address (32-bit) */
    uint16_t block_num;         /**< Block number */
    uint16_t length;            /**< Data length in this block */
    uint8_t  flags;             /**< Block flags */
    uint8_t  spare[4];          /**< Reserved */
    uint16_t header_crc;        /**< Header CRC-16 (big-endian) */
} uft_bbc_tape_header_t;
#pragma pack(pop)

/**
 * @brief Complete tape block structure
 */
typedef struct {
    char     filename[11];      /**< Filename (10 chars + null) */
    uint32_t load_addr;
    uint32_t exec_addr;
    uint16_t block_num;
    uint16_t length;
    uint8_t  flags;
    uint8_t  spare[4];
    uint16_t header_crc;
    uint8_t  *data;             /**< Block data */
    uint16_t data_crc;          /**< Data CRC-16 (big-endian) */
    bool     valid;             /**< CRC check passed */
} uft_bbc_tape_block_t;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Calculate BBC CRC-16
 * 
 * Uses the polynomial from the Advanced User Guide (p.348).
 * The BBC uses a non-standard CRC-16 algorithm.
 */
static inline uint16_t uft_bbc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc ^= 0x0810;
                crc = (crc << 1) | 1;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

/**
 * @brief Get total sectors from DFS catalog
 */
static inline uint16_t uft_dfs_get_sectors(const uft_dfs_cat1_t *cat1)
{
    return cat1->sectors_lo | ((cat1->opt_sectors_hi & 0x03) << 8);
}

/**
 * @brief Get boot option from DFS catalog
 */
static inline uft_dfs_boot_t uft_dfs_get_boot_option(const uft_dfs_cat1_t *cat1)
{
    return (uft_dfs_boot_t)((cat1->opt_sectors_hi >> 4) & 0x03);
}

/**
 * @brief Get number of files in DFS catalog
 */
static inline int uft_dfs_get_file_count(const uft_dfs_cat1_t *cat1)
{
    return cat1->num_entries / 8;
}

/**
 * @brief Read DFS file entry from catalog
 * @param cat0 Catalog sector 0
 * @param cat1 Catalog sector 1
 * @param index File index (0-30)
 * @param entry Output entry
 * @return 0 on success, -1 if index out of range
 */
int uft_dfs_read_entry(const uft_dfs_cat0_t *cat0,
                       const uft_dfs_cat1_t *cat1,
                       int index, uft_dfs_file_entry_t *entry);

/**
 * @brief Create new DFS disk image
 * @param buffer Output buffer (at least 512 bytes for catalog)
 * @param sectors Total sectors (400 or 800)
 * @param title Disk title (up to 12 chars)
 * @param boot_option Boot option
 */
void uft_dfs_create_catalog(uint8_t *buffer, uint16_t sectors,
                            const char *title, uft_dfs_boot_t boot_option);

/**
 * @brief Add file to DFS disk image
 * @param image Disk image buffer
 * @param image_size Size of image
 * @param filename BBC filename (e.g., "$.PROGRAM")
 * @param load_addr Load address
 * @param exec_addr Exec address  
 * @param data File data
 * @param length Data length
 * @return 0 on success, -1 on error
 */
int uft_dfs_add_file(uint8_t *image, size_t image_size,
                     const char *filename, uint32_t load_addr,
                     uint32_t exec_addr, const uint8_t *data,
                     size_t length);

/**
 * @brief Extract file from DFS disk image
 * @param image Disk image
 * @param image_size Image size
 * @param entry File entry to extract
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes extracted, or -1 on error
 */
int uft_dfs_extract_file(const uint8_t *image, size_t image_size,
                         const uft_dfs_file_entry_t *entry,
                         uint8_t *buffer, size_t buffer_size);

/**
 * @brief Detect disk image format
 * @param data Image data
 * @param size Image size
 * @return Format description string
 */
const char *uft_bbc_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Check if SSD/DSD image is valid
 */
static inline bool uft_dfs_is_valid(const uint8_t *data, size_t size)
{
    if (size < 512) return false;
    
    /* Check catalog sector 1 */
    const uft_dfs_cat1_t *cat1 = (const uft_dfs_cat1_t *)(data + 256);
    
    /* Number of entries should be multiple of 8 and <= 248 */
    if (cat1->num_entries > 248 || (cat1->num_entries % 8) != 0) {
        return false;
    }
    
    /* Total sectors should match image size */
    uint16_t sectors = uft_dfs_get_sectors(cat1);
    if (sectors == 0 || sectors > 1600) return false;
    
    return true;
}

/*===========================================================================
 * Disk Image File Extensions
 *===========================================================================*/

/**
 * Common BBC disk image extensions:
 * 
 * .ssd - Single-sided disc (SS/40 or SS/80)
 * .dsd - Double-sided disc (DS/40 or DS/80)
 * .adf - ADFS disc image (various formats)
 * .adl - ADFS L format (interleaved)
 * .adm - ADFS M format
 * .ads - ADFS S format
 * .img - Generic disc image
 * .uef - Unified Emulator Format (tape)
 * .csw - Compressed Square Wave (tape)
 */

#ifdef __cplusplus
}
#endif

#endif /* UFT_BBC_DFS_H */

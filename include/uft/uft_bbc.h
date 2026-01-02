/**
 * @file uft_bbc.h
 * @brief BBC Micro Filesystem Support (DFS, ADFS)
 * @version 3.1.4.004
 *
 * Support for BBC Micro and Acorn computer filesystems:
 *
 * **DFS (Disc Filing System)**
 * - Acorn's standard 8-bit filesystem
 * - 200KB (40T SS), 400KB (80T SS), 800KB (80T DS)
 * - 10 sectors per track, 256 bytes per sector
 * - FM encoding
 * - Maximum 31 files per disc
 *
 * **ADFS (Advanced Disc Filing System)**
 * - Multiple variants: S, M, L, D, E, F, G
 * - Support for old-map and new-map formats
 * - 256-byte or 1024-byte sectors
 * - Hierarchical directory structure
 * - File types (load/exec addresses or filetype)
 *
 * Based on RiscOS PRMs and BBC Micro documentation.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_BBC_H
#define UFT_BBC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DFS Constants
 *============================================================================*/

/** DFS sector size */
#define UFT_DFS_SECTOR_SIZE     256

/** Maximum files in DFS catalogue */
#define UFT_DFS_MAX_FILES       31

/** Boot option codes */
typedef enum {
    UFT_DFS_BOOT_NONE       = 0,    /**< No action */
    UFT_DFS_BOOT_LOAD       = 1,    /**< *LOAD !BOOT */
    UFT_DFS_BOOT_RUN        = 2,    /**< *RUN !BOOT */
    UFT_DFS_BOOT_EXEC       = 3,    /**< *EXEC !BOOT */
} uft_dfs_boot_t;

/*============================================================================
 * ADFS Constants
 *============================================================================*/

/** ADFS 8-bit sector size */
#define UFT_ADFS_SECTOR_256     256

/** ADFS 16-bit sector size */
#define UFT_ADFS_SECTOR_1024    1024

/** Old map entry size */
#define UFT_ADFS_OLDMAP_ENTRY   3

/** Old map entries count */
#define UFT_ADFS_OLDMAP_LEN     82

/** Boot block offset (new map) */
#define UFT_ADFS_BOOTBLOCK_OFF  0xC00

/** Disc record offset in boot block */
#define UFT_ADFS_BOOTDR_OFF     0x1C0

/** ADFS format variants */
typedef enum {
    UFT_ADFS_UNKNOWN = 0,
    UFT_ADFS_S,         /**< 160KB: 40T×1×16×256 */
    UFT_ADFS_M,         /**< 320KB: 80T×1×16×256 */
    UFT_ADFS_L,         /**< 640KB: 80T×2×16×256 */
    UFT_ADFS_D,         /**< 800KB: 80T×2×5×1024 (old-map, new-dir) */
    UFT_ADFS_E,         /**< 800KB: 80T×2×5×1024 (new-map, new-dir) */
    UFT_ADFS_E_PLUS,    /**< E+ with big directory */
    UFT_ADFS_F,         /**< 1.6MB: 80T×2×10×1024 (boot block) */
    UFT_ADFS_F_PLUS,    /**< F+ with big directory */
    UFT_ADFS_G,         /**< 3.2MB: 80T×2×20×1024 */
} uft_adfs_format_t;

/** ADFS map type */
typedef enum {
    UFT_ADFS_OLDMAP = 0,
    UFT_ADFS_NEWMAP = 1,
} uft_adfs_map_type_t;

/** ADFS directory type */
typedef enum {
    UFT_ADFS_OLDDIR = 0,
    UFT_ADFS_NEWDIR = 1,
} uft_adfs_dir_type_t;

/*============================================================================
 * DFS Structures
 *============================================================================*/

/** DFS catalogue entry (in memory) */
typedef struct {
    char        filename[8];    /**< 7-char name + directory */
    uint8_t     directory;      /**< Directory character */
    uint8_t     locked;         /**< Locked flag */
    uint32_t    load_addr;      /**< Load address */
    uint32_t    exec_addr;      /**< Execute address */
    uint32_t    length;         /**< File length */
    uint16_t    start_sector;   /**< Starting sector */
} uft_dfs_entry_t;

/** DFS disc information */
typedef struct {
    char        title[13];      /**< Disc title (12 chars + NUL) */
    uint8_t     boot_option;    /**< Boot option */
    uint8_t     write_count;    /**< Write operations (BCD) */
    uint16_t    total_sectors;  /**< Total sectors on disc */
    int         num_files;      /**< Number of files */
    uft_dfs_entry_t files[UFT_DFS_MAX_FILES];
} uft_dfs_info_t;

/*============================================================================
 * ADFS Structures
 *============================================================================*/

/** ADFS old map structure (512 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t     freestart[UFT_ADFS_OLDMAP_ENTRY * UFT_ADFS_OLDMAP_LEN];
    uint8_t     reserved;       /**< Must be 0 */
    uint8_t     oldname0[5];    /**< First half of disc name */
    uint8_t     oldsize[3];     /**< Disc size in sectors */
    uint8_t     check0;         /**< First checksum */
    uint8_t     freelen[UFT_ADFS_OLDMAP_ENTRY * UFT_ADFS_OLDMAP_LEN];
    uint8_t     oldname1[5];    /**< Second half of disc name */
    uint16_t    oldid;          /**< Disc ID */
    uint8_t     oldboot;        /**< Boot option */
    uint8_t     freeend;        /**< Pointer to end of free list */
    uint8_t     check1;         /**< Second checksum */
} uft_adfs_oldmap_t;

/** ADFS disc record (60 bytes) */
typedef struct __attribute__((packed)) {
    uint8_t     log2secsize;    /**< Log2 of sector size */
    uint8_t     secspertrack;   /**< Sectors per track */
    uint8_t     heads;          /**< Heads (surfaces) */
    uint8_t     density;        /**< Density (1=FM, 2=MFM) */
    uint8_t     idlen;          /**< ID field length */
    uint8_t     log2bpmb;       /**< Log2 bytes per map bit */
    uint8_t     skew;           /**< Track skew */
    uint8_t     bootoption;     /**< Boot option */
    uint8_t     lowsector;      /**< Lowest sector ID */
    uint8_t     nzones;         /**< Number of zones */
    uint16_t    zone_spare;     /**< Zone spare bits */
    uint32_t    root;           /**< Root directory address */
    uint32_t    disc_size;      /**< Disc size in bytes */
    uint16_t    disc_id;        /**< Disc ID */
    char        disc_name[10];  /**< Disc name */
    uint32_t    disc_type;      /**< Disc type */
    uint32_t    disc_size_hi;   /**< High word of disc size */
    uint8_t     share_size;     /**< Share size */
    uint8_t     big_flag;       /**< Big directory flag */
    uint8_t     nzones_hi;      /**< High byte of nzones */
    uint8_t     reserved1;
    uint32_t    format_ver;     /**< Format version */
    uint32_t    root_size;      /**< Root directory size (E+/F+) */
    uint8_t     reserved2[8];
} uft_adfs_discrecord_t;

/** ADFS directory entry */
typedef struct {
    char        name[11];       /**< Filename (10 chars + NUL) */
    uint32_t    load_addr;      /**< Load address or filetype<<8 + date */
    uint32_t    exec_addr;      /**< Exec address or date/time */
    uint32_t    length;         /**< File length */
    uint32_t    ind_disc_addr;  /**< Internal disc address */
    uint8_t     newattr;        /**< Attributes (new dir) */
} uft_adfs_entry_t;

/** ADFS disc information */
typedef struct {
    uft_adfs_format_t format;
    uft_adfs_map_type_t map_type;
    uft_adfs_dir_type_t dir_type;
    int         sector_size;
    int         sectors_per_track;
    char        disc_name[11];
    uint16_t    disc_id;
    uint8_t     boot_option;
    uint32_t    disc_size;
    uint32_t    root_addr;
} uft_adfs_info_t;

/*============================================================================
 * DFS API
 *============================================================================*/

/**
 * @brief Validate DFS catalogue
 * @param data Sector 0 and 1 data (512 bytes)
 * @param total_sectors Output: total sectors
 * @return true if valid DFS
 */
bool uft_dfs_validate(const uint8_t *data, int *total_sectors);

/**
 * @brief Read DFS disc information
 * @param data Sector 0 and 1 data
 * @param info Output info structure
 * @return 0 on success
 */
int uft_dfs_read_info(const uint8_t *data, uft_dfs_info_t *info);

/**
 * @brief Get DFS filename for entry
 * @param data Sector 0 data
 * @param entry Entry index (1-31)
 * @param filename Output buffer (10 bytes)
 * @param locked Output: locked flag
 */
void uft_dfs_get_filename(const uint8_t *data, int entry,
                          char *filename, bool *locked);

/**
 * @brief Get DFS file addresses
 * @param data Sector 1 data
 * @param entry Entry index (1-31)
 * @param load_addr Output: load address
 * @param exec_addr Output: exec address
 * @param length Output: file length
 * @param start_sector Output: start sector
 */
void uft_dfs_get_file(const uint8_t *data, int entry,
                      uint32_t *load_addr, uint32_t *exec_addr,
                      uint32_t *length, uint16_t *start_sector);

/*============================================================================
 * ADFS API
 *============================================================================*/

/**
 * @brief Detect ADFS format
 * @param data First 2KB of disc
 * @param data_len Data length
 * @return ADFS format or UFT_ADFS_UNKNOWN
 */
uft_adfs_format_t uft_adfs_detect(const uint8_t *data, size_t data_len);

/**
 * @brief Validate ADFS old map checksums
 * @param data 512 bytes of old map
 * @return true if valid
 */
bool uft_adfs_validate_oldmap(const uint8_t *data);

/**
 * @brief Validate ADFS new map zone
 * @param data Zone data
 * @param log2_sector_size Log2 of sector size
 * @param zone Zone number
 * @return Calculated check byte
 */
uint8_t uft_adfs_validate_zone(const uint8_t *data,
                               int log2_sector_size, int zone);

/**
 * @brief Calculate ADFS checksum
 * @param data Sector data
 * @param sector_size 256 or 1024
 * @return Checksum byte
 */
uint8_t uft_adfs_checksum(const uint8_t *data, int sector_size);

/**
 * @brief Read ADFS disc record
 * @param data Disc record data (60 bytes)
 * @param dr Output disc record
 */
void uft_adfs_read_discrecord(const uint8_t *data, uft_adfs_discrecord_t *dr);

/**
 * @brief Get ADFS filetype name
 * @param filetype 12-bit filetype
 * @return Filetype name or NULL
 */
const char *uft_adfs_filetype_name(uint16_t filetype);

/*============================================================================
 * Common Acorn Filetypes
 *============================================================================*/

#define UFT_ADFS_TYPE_TEXT      0xFFF   /**< Text */
#define UFT_ADFS_TYPE_DATA      0xFFD   /**< Data */
#define UFT_ADFS_TYPE_COMMAND   0xFFE   /**< Command (script) */
#define UFT_ADFS_TYPE_BASIC     0xFFB   /**< BASIC program */
#define UFT_ADFS_TYPE_MODULE    0xFFA   /**< Relocatable module */
#define UFT_ADFS_TYPE_SPRITE    0xFF9   /**< Sprite file */
#define UFT_ADFS_TYPE_OBEY      0xFEB   /**< Obey file (script) */
#define UFT_ADFS_TYPE_DESKTOP   0xFEA   /**< Desktop file */
#define UFT_ADFS_TYPE_TEMPLATE  0xFEC   /**< Window template */
#define UFT_ADFS_TYPE_PALETTE   0xFED   /**< Palette file */
#define UFT_ADFS_TYPE_FONT      0xFF6   /**< Font file */
#define UFT_ADFS_TYPE_DRAWFILE  0xAFF   /**< Draw file */
#define UFT_ADFS_TYPE_JPEG      0xC85   /**< JPEG image */
#define UFT_ADFS_TYPE_GIF       0x695   /**< GIF image */
#define UFT_ADFS_TYPE_PNG       0xB60   /**< PNG image */
#define UFT_ADFS_TYPE_HTML      0xFAF   /**< HTML file */
#define UFT_ADFS_TYPE_ZIP       0xA91   /**< ZIP archive */

/*============================================================================
 * Teledisk (TD0) Format Constants
 *============================================================================*/

/** Teledisk signature */
#define UFT_TD0_SIGNATURE_NORM  "TD"    /**< Normal (uncompressed) */
#define UFT_TD0_SIGNATURE_ADV   "td"    /**< Advanced (compressed) */

/** Teledisk CRC polynomial */
#define UFT_TD0_CRC_POLY        0xA097

/** Teledisk last track marker */
#define UFT_TD0_LAST_TRACK      0xFF

/** Teledisk flags */
#define UFT_TD0_FLAGS_DELDATA   0x04    /**< Deleted data mark */

/*============================================================================
 * Teledisk Structures
 *============================================================================*/

/** Teledisk file header */
typedef struct __attribute__((packed)) {
    uint8_t     signature[2];   /**< "TD" or "td" */
    uint8_t     sequence;       /**< Sequence (volume) number */
    uint8_t     checkseq;       /**< Check sequence */
    uint8_t     version;        /**< Version */
    uint8_t     datarate;       /**< Data rate + density */
    uint8_t     drivetype;      /**< Drive type */
    uint8_t     stepping;       /**< Track stepping */
    uint8_t     dosflag;        /**< DOS allocation flag */
    uint8_t     sides;          /**< Number of sides */
    uint16_t    crc;            /**< Header CRC */
} uft_td0_header_t;

/** Teledisk comment block */
typedef struct __attribute__((packed)) {
    uint16_t    crc;            /**< CRC of data + header */
    uint16_t    datalen;        /**< Comment length */
    uint8_t     year;           /**< Year - 1900 */
    uint8_t     month;          /**< Month (0-11) */
    uint8_t     day;            /**< Day */
    uint8_t     hour;           /**< Hour */
    uint8_t     minute;         /**< Minute */
    uint8_t     second;         /**< Second */
} uft_td0_comment_t;

/** Teledisk track header */
typedef struct __attribute__((packed)) {
    uint8_t     sectors;        /**< Sectors in track (0xFF = end) */
    uint8_t     track;          /**< Track number */
    uint8_t     head;           /**< Head number */
    uint8_t     crc;            /**< Track CRC */
} uft_td0_track_t;

/** Teledisk sector header */
typedef struct __attribute__((packed)) {
    uint8_t     track;          /**< Logical track */
    uint8_t     head;           /**< Logical head */
    uint8_t     sector;         /**< Sector number */
    uint8_t     size;           /**< Sector size code */
    uint8_t     flags;          /**< Flags */
    uint8_t     crc;            /**< Sector CRC */
} uft_td0_sector_t;

/** Teledisk data header */
typedef struct __attribute__((packed)) {
    uint16_t    blocksize;      /**< Block size + 1 */
    uint8_t     encoding;       /**< Encoding method */
} uft_td0_data_t;

/*============================================================================
 * Teledisk API
 *============================================================================*/

/**
 * @brief Calculate Teledisk CRC16
 * @param data Data buffer
 * @param len Data length
 * @param initial Initial CRC value
 * @return CRC16 value
 */
uint16_t uft_td0_crc16(const uint8_t *data, int len, uint16_t initial);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BBC_H */

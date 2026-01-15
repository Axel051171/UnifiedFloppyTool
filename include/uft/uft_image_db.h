/**
 * @file uft_image_db.h
 * @brief Disk Image Database - Known Image Identification
 * 
 * Features:
 * - Identify known disk images by hash/signature
 * - Boot sector fingerprinting
 * - OEM name database
 * - Game/Software identification
 * - Corruption detection via known-good references
 * 
 * Inspired by DiskImageTool's database feature.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_IMAGE_DB_H
#define UFT_IMAGE_DB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * DATABASE ENTRY TYPES
 *===========================================================================*/

/**
 * @brief Image category
 */
typedef enum {
    UFT_IMG_CAT_UNKNOWN = 0,
    UFT_IMG_CAT_GAME,           /**< Game disk */
    UFT_IMG_CAT_APPLICATION,    /**< Application/utility */
    UFT_IMG_CAT_SYSTEM,         /**< OS/System disk */
    UFT_IMG_CAT_DEMO,           /**< Demo/shareware */
    UFT_IMG_CAT_DATA,           /**< Data disk */
    UFT_IMG_CAT_MAGAZINE,       /**< Magazine coverdisk */
    UFT_IMG_CAT_CUSTOM          /**< User-defined */
} uft_image_category_t;

/**
 * @brief Platform/system type
 */
typedef enum {
    UFT_IMG_PLAT_UNKNOWN = 0,
    UFT_IMG_PLAT_MSDOS,         /**< MS-DOS / PC-DOS */
    UFT_IMG_PLAT_WINDOWS,       /**< Windows 3.x/9x */
    UFT_IMG_PLAT_AMIGA,         /**< Commodore Amiga */
    UFT_IMG_PLAT_ATARI_ST,      /**< Atari ST */
    UFT_IMG_PLAT_C64,           /**< Commodore 64 */
    UFT_IMG_PLAT_APPLE_II,      /**< Apple II */
    UFT_IMG_PLAT_APPLE_MAC,     /**< Macintosh */
    UFT_IMG_PLAT_CPM,           /**< CP/M */
    UFT_IMG_PLAT_MSX,           /**< MSX */
    UFT_IMG_PLAT_BBC,           /**< BBC Micro */
    UFT_IMG_PLAT_MULTI          /**< Multi-platform */
} uft_image_platform_t;

/**
 * @brief Protection type
 */
typedef enum {
    UFT_IMG_PROT_NONE = 0,
    UFT_IMG_PROT_WEAK_BITS,     /**< Weak bit protection */
    UFT_IMG_PROT_LONG_TRACK,    /**< Long track */
    UFT_IMG_PROT_FUZZY_BITS,    /**< Fuzzy/random bits */
    UFT_IMG_PROT_TIMING,        /**< Timing-based */
    UFT_IMG_PROT_BAD_SECTOR,    /**< Intentional bad sectors */
    UFT_IMG_PROT_CUSTOM_FORMAT, /**< Non-standard format */
    UFT_IMG_PROT_MULTIPLE       /**< Multiple methods */
} uft_image_protection_t;

/*===========================================================================
 * DATABASE ENTRY
 *===========================================================================*/

/**
 * @brief Hash/signature for identification
 */
typedef struct {
    uint32_t crc32;             /**< CRC32 of entire image */
    uint32_t boot_crc32;        /**< CRC32 of boot sector */
    uint8_t  md5[16];           /**< MD5 hash (optional, all zeros if unused) */
    uint8_t  sha1[20];          /**< SHA1 hash (optional) */
} uft_image_hash_t;

/**
 * @brief Boot sector signature
 */
typedef struct {
    char     oem_name[12];      /**< OEM name from boot sector */
    uint16_t bytes_per_sector;  /**< Bytes per sector */
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
} uft_boot_signature_t;

/**
 * @brief Database entry for a known image
 */
typedef struct {
    /* Identification */
    uint32_t id;                /**< Unique database ID */
    char name[64];              /**< Image/software name */
    char publisher[48];         /**< Publisher/developer */
    char version[16];           /**< Version string */
    uint16_t year;              /**< Release year */
    
    /* Classification */
    uft_image_category_t category;
    uft_image_platform_t platform;
    uft_image_protection_t protection;
    
    /* Signatures */
    uft_image_hash_t hash;
    uft_boot_signature_t boot_sig;
    
    /* Disk info */
    uint8_t disk_number;        /**< Disk N of M */
    uint8_t disk_total;         /**< Total disks in set */
    uint32_t image_size;        /**< Expected size in bytes */
    
    /* Metadata */
    char notes[128];            /**< Additional notes */
    uint32_t flags;             /**< Various flags */
} uft_image_entry_t;

/*===========================================================================
 * MATCH RESULT
 *===========================================================================*/

/**
 * @brief Match confidence level
 */
typedef enum {
    UFT_MATCH_NONE = 0,         /**< No match */
    UFT_MATCH_POSSIBLE,         /**< Weak match (partial) */
    UFT_MATCH_LIKELY,           /**< Good match */
    UFT_MATCH_EXACT             /**< Exact match (hash) */
} uft_match_level_t;

/**
 * @brief Match result
 */
typedef struct {
    uft_match_level_t level;
    int confidence;             /**< 0-100 confidence score */
    const uft_image_entry_t *entry;
    char match_reason[64];      /**< Why it matched */
} uft_match_result_t;

/*===========================================================================
 * OEM NAME DATABASE
 *===========================================================================*/

/**
 * @brief OEM name entry
 */
typedef struct {
    char oem_name[12];          /**< OEM string as found */
    char correct_name[12];      /**< Correct/canonical name */
    char description[48];       /**< What created this */
    bool is_windows_modified;   /**< Modified by Windows */
    bool is_valid;              /**< Valid original name */
} uft_oem_entry_t;

/**
 * @brief Known OEM names
 */
static const uft_oem_entry_t UFT_OEM_DATABASE[] = {
    /* Standard DOS/Windows */
    {"MSDOS5.0",  "MSDOS5.0",  "MS-DOS 5.0+", false, true},
    {"MSDOS4.0",  "MSDOS4.0",  "MS-DOS 4.0", false, true},
    {"MSDOS3.3",  "MSDOS3.3",  "MS-DOS 3.3", false, true},
    {"IBM  3.3",  "IBM  3.3",  "PC-DOS 3.3", false, true},
    {"IBM  5.0",  "IBM  5.0",  "PC-DOS 5.0", false, true},
    {"MSWIN4.0",  "MSWIN4.0",  "Windows 95", false, true},
    {"MSWIN4.1",  "MSWIN4.1",  "Windows 98", false, true},
    
    /* Linux tools */
    {"mkdosfs",   "MSDOS5.0",  "Linux mkdosfs - should fix", false, false},
    {"mkfs.fat",  "MSDOS5.0",  "Linux mkfs.fat - should fix", false, false},
    {"dosfstools","MSDOS5.0",  "dosfstools", false, false},
    
    /* Other tools */
    {"FreeDOS",   "FreeDOS ",  "FreeDOS", false, true},
    {"FRDOS4.1",  "FRDOS4.1",  "FreeDOS 1.0", false, true},
    {"WINIMAGE",  "MSDOS5.0",  "WinImage - non-standard", false, false},
    {"        ",  "MSDOS5.0",  "Blank - should fix", false, false},
    
    /* Windows modifications (bad) */
    {"MSDOS5.0",  "MSDOS5.0",  "OK", false, true},
    {"NO NAME",   "MSDOS5.0",  "Windows modified", true, false},
    
    /* Sentinel */
    {"", "", "", false, false}
};

/*===========================================================================
 * DATABASE LIFECYCLE
 *===========================================================================*/

/**
 * @brief Initialize image database
 */
int uft_image_db_init(void);

/**
 * @brief Shutdown database
 */
void uft_image_db_shutdown(void);

/**
 * @brief Load database from file
 */
int uft_image_db_load(const char *path);

/**
 * @brief Save database to file
 */
int uft_image_db_save(const char *path);

/**
 * @brief Get database entry count
 */
size_t uft_image_db_count(void);

/*===========================================================================
 * LOOKUP FUNCTIONS
 *===========================================================================*/

/**
 * @brief Find image by CRC32
 */
const uft_image_entry_t* uft_image_db_find_by_crc(uint32_t crc32);

/**
 * @brief Find image by boot sector CRC
 */
const uft_image_entry_t* uft_image_db_find_by_boot_crc(uint32_t boot_crc);

/**
 * @brief Find image by MD5 hash
 */
const uft_image_entry_t* uft_image_db_find_by_md5(const uint8_t md5[16]);

/**
 * @brief Find image by name (partial match)
 */
int uft_image_db_find_by_name(const char *name, 
                              const uft_image_entry_t **results,
                              int max_results);

/**
 * @brief Find images by platform
 */
int uft_image_db_find_by_platform(uft_image_platform_t platform,
                                   const uft_image_entry_t **results,
                                   int max_results);

/*===========================================================================
 * IDENTIFICATION
 *===========================================================================*/

/**
 * @brief Identify image from raw data
 * 
 * @param data Image data
 * @param size Image size
 * @param result Match result (output)
 * @return 0 on success (even if no match)
 */
int uft_image_db_identify(const uint8_t *data, size_t size,
                          uft_match_result_t *result);

/**
 * @brief Identify image from file
 */
int uft_image_db_identify_file(const char *path, uft_match_result_t *result);

/**
 * @brief Get multiple possible matches
 */
int uft_image_db_identify_multi(const uint8_t *data, size_t size,
                                uft_match_result_t *results, int max_results);

/*===========================================================================
 * BOOT SECTOR ANALYSIS
 *===========================================================================*/

/**
 * @brief Parse boot sector signature
 */
int uft_image_db_parse_boot(const uint8_t *boot_sector,
                            uft_boot_signature_t *sig);

/**
 * @brief Check if boot sector was modified by Windows
 */
bool uft_image_db_is_windows_modified(const uint8_t *boot_sector);

/**
 * @brief Get OEM name info
 */
const uft_oem_entry_t* uft_image_db_lookup_oem(const char *oem_name);

/**
 * @brief Suggest correct OEM name
 */
const char* uft_image_db_suggest_oem(const uint8_t *boot_sector);

/*===========================================================================
 * DATABASE MODIFICATION
 *===========================================================================*/

/**
 * @brief Add entry to database
 */
int uft_image_db_add(const uft_image_entry_t *entry);

/**
 * @brief Remove entry from database
 */
int uft_image_db_remove(uint32_t id);

/**
 * @brief Update entry
 */
int uft_image_db_update(uint32_t id, const uft_image_entry_t *entry);

/**
 * @brief Create entry from image data
 */
int uft_image_db_create_entry(const uint8_t *data, size_t size,
                               const char *name,
                               uft_image_entry_t *entry);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Calculate CRC32
 */
uint32_t uft_image_db_crc32(const uint8_t *data, size_t size);

/**
 * @brief Calculate MD5
 */
void uft_image_db_md5(const uint8_t *data, size_t size, uint8_t md5[16]);

/**
 * @brief Get category name
 */
const char* uft_image_db_category_name(uft_image_category_t cat);

/**
 * @brief Get platform name
 */
const char* uft_image_db_platform_name(uft_image_platform_t plat);

/**
 * @brief Get protection name
 */
const char* uft_image_db_protection_name(uft_image_protection_t prot);

/**
 * @brief Print database statistics
 */
void uft_image_db_print_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMAGE_DB_H */

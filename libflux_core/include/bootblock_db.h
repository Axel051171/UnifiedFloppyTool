// SPDX-License-Identifier: MIT
/*
 * bootblock_db.h - Amiga Bootblock Detection Database
 * 
 * Based on AmigaBootBlockReader v6.0 by Jason and Jordan Smith
 * Database: 2,988 bootblock signatures (422 viruses!)
 * 
 * Detection Methods:
 * - Pattern matching (fast, offset,value pairs)
 * - CRC32 checksum (exact match)
 * 
 * @version 1.0.0
 * @date 2024-12-25
 */

#ifndef UFT_BOOTBLOCK_DB_H
#define UFT_BOOTBLOCK_DB_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * CONSTANTS
 *============================================================================*/

#define BOOTBLOCK_SIZE 1024  /**< Amiga bootblock size (first 1024 bytes of track 0) */
#define BOOTBLOCK_MAX_PATTERNS 20  /**< Max pattern elements per signature */

/*============================================================================*
 * BOOTBLOCK CATEGORIES
 *============================================================================*/

/**
 * @brief Bootblock category codes (from AmigaBootBlockReader)
 */
typedef enum {
    BB_CAT_UNKNOWN = 0,     /**< Unknown */
    BB_CAT_UTILITY,         /**< u - Utility bootblock */
    BB_CAT_VIRUS,           /**< v - VIRUS! */
    BB_CAT_LOADER,          /**< l - Loader */
    BB_CAT_SCENE,           /**< sc - Scene/Screen */
    BB_CAT_INTRO,           /**< i - Intro (Demo) */
    BB_CAT_BOOTLOADER,      /**< bl - Bootloader */
    BB_CAT_XCOPY,           /**< xc - X-Copy related! */
    BB_CAT_CUSTOM,          /**< cust - Custom */
    BB_CAT_DEMOSCENE,       /**< ds - Demoscene */
    BB_CAT_VIRUS_FAKE,      /**< vfm - Virus (Fake/Modified) */
    BB_CAT_GAME,            /**< g - Game */
    BB_CAT_PASSWORD,        /**< p - Password/Picture */
} bootblock_category_t;

/*============================================================================*
 * BOOTBLOCK PATTERN
 *============================================================================*/

/**
 * @brief Pattern element (offset, value pair)
 */
typedef struct {
    uint16_t offset;  /**< Byte offset in bootblock (0-1023) */
    uint8_t value;    /**< Expected value at offset */
} bb_pattern_element_t;

/**
 * @brief Bootblock pattern signature
 */
typedef struct {
    bb_pattern_element_t elements[BOOTBLOCK_MAX_PATTERNS];
    uint8_t count;  /**< Number of elements (0 = no pattern) */
} bb_pattern_t;

/*============================================================================*
 * BOOTBLOCK SIGNATURE
 *============================================================================*/

/**
 * @brief Bootblock signature entry
 */
typedef struct {
    char name[128];                 /**< Bootblock name (e.g., "16-Bit Crew Virus") */
    bootblock_category_t category;  /**< Category */
    uint32_t crc32;                 /**< CRC32 checksum (0 = no CRC) */
    bb_pattern_t pattern;           /**< Detection pattern */
    
    // Metadata
    bool bootable;                  /**< Is bootable? */
    bool has_data;                  /**< Contains data? */
    char kickstart[16];             /**< Kickstart version (e.g., "KS1.3") */
    char notes[256];                /**< Additional notes */
    char url[256];                  /**< Reference URL (virus help, demozoo, etc) */
} bb_signature_t;

/*============================================================================*
 * DETECTION RESULT
 *============================================================================*/

/**
 * @brief Bootblock detection result
 */
typedef struct {
    bool detected;                  /**< Was a match found? */
    bb_signature_t signature;       /**< Matched signature */
    
    // Match details
    bool matched_by_pattern;        /**< Matched by pattern? */
    bool matched_by_crc;            /**< Matched by CRC32? */
    uint32_t computed_crc;          /**< Computed CRC32 of bootblock */
    
    // Bootblock info
    uint32_t checksum;              /**< Bootblock checksum (bytes 4-7) */
    bool checksum_valid;            /**< Is checksum valid? */
    uint8_t dos_type;               /**< DOS type byte (byte 3): 0=OFS, 1=FFS */
} bb_detection_result_t;

/*============================================================================*
 * DATABASE API
 *============================================================================*/

/**
 * @brief Initialize bootblock database
 * 
 * Loads brainfile.xml from the specified path.
 * 
 * @param xml_path Path to brainfile.xml (NULL = use default)
 * @return int 0 on success, negative on error
 */
int bb_db_init(const char *xml_path);

/**
 * @brief Free bootblock database
 */
void bb_db_free(void);

/**
 * @brief Get database statistics
 * 
 * @param total_count Output: Total signatures
 * @param virus_count Output: Virus signatures
 * @param xcopy_count Output: X-Copy signatures
 * @return int 0 on success, negative on error
 */
int bb_db_get_stats(int *total_count, int *virus_count, int *xcopy_count);

/*============================================================================*
 * DETECTION API
 *============================================================================*/

/**
 * @brief Detect bootblock signature
 * 
 * Analyzes a 1024-byte bootblock and identifies it using:
 * 1. Pattern matching (fast)
 * 2. CRC32 verification (exact)
 * 
 * @param bootblock Pointer to 1024-byte bootblock data
 * @param result Output detection result
 * @return int 0 on success, negative on error
 */
int bb_detect(const uint8_t bootblock[BOOTBLOCK_SIZE], bb_detection_result_t *result);

/**
 * @brief Verify bootblock checksum
 * 
 * Amiga bootblocks have a checksum in bytes 4-7.
 * This function verifies it and can optionally fix it.
 * 
 * @param bootblock Pointer to bootblock (may be modified if fix=true)
 * @param is_valid Output: Is checksum valid?
 * @param fix If true, fix the checksum
 * @return int 0 on success, negative on error
 */
int bb_verify_checksum(uint8_t bootblock[BOOTBLOCK_SIZE], bool *is_valid, bool fix);

/*============================================================================*
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get category name string
 * 
 * @param category Category code
 * @return const char* Category name (e.g., "Virus", "X-Copy")
 */
const char* bb_category_name(bootblock_category_t category);

/**
 * @brief Is category a virus?
 * 
 * @param category Category code
 * @return bool true if virus or fake virus
 */
static inline bool bb_is_virus(bootblock_category_t category)
{
    return (category == BB_CAT_VIRUS || category == BB_CAT_VIRUS_FAKE);
}

/**
 * @brief Compute CRC32 of bootblock
 * 
 * @param bootblock Pointer to 1024-byte bootblock
 * @return uint32_t CRC32 checksum
 */
uint32_t bb_crc32(const uint8_t bootblock[BOOTBLOCK_SIZE]);

/*============================================================================*
 * STATISTICS
 *============================================================================*/

/**
 * @brief Bootblock scan statistics
 */
typedef struct {
    uint32_t total_disks;           /**< Total disks scanned */
    uint32_t detected_count;        /**< Bootblocks detected */
    uint32_t virus_count;           /**< Viruses found */
    uint32_t xcopy_count;           /**< X-Copy bootblocks */
    uint32_t demoscene_count;       /**< Demoscene intros */
    uint32_t unknown_count;         /**< Unknown bootblocks */
} bb_scan_stats_t;

/**
 * @brief Initialize scan statistics
 * 
 * @param stats Statistics structure
 */
void bb_stats_init(bb_scan_stats_t *stats);

/**
 * @brief Add detection result to statistics
 * 
 * @param stats Statistics structure
 * @param result Detection result
 */
void bb_stats_add(bb_scan_stats_t *stats, const bb_detection_result_t *result);

/**
 * @brief Print scan statistics
 * 
 * @param stats Statistics structure
 */
void bb_stats_print(const bb_scan_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BOOTBLOCK_DB_H */

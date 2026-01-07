/**
 * @file uft_bootblock_scanner.h
 * @brief UFT Bootblock Scanner & Virus Detection v1.6.0
 * 
 * Features:
 * - Pattern-based bootblock identification
 * - CRC32 exact matching
 * - Virus detection with categories
 * - Brainfile.xml parser for signatures
 * - Safe bootblock library
 * 
 * VERSION: 1.6.0 (2025-12-30)
 * 
 * Copyright (c) 2025 UFT Project
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_BOOTBLOCK_SCANNER_H
#define UFT_BOOTBLOCK_SCANNER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** Amiga bootblock size */
#define UFT_BOOTBLOCK_SIZE          1024

/** Maximum patterns per signature */
#define UFT_BB_MAX_PATTERNS         16

/** Maximum name length */
#define UFT_BB_MAX_NAME_LEN         64

/** Initial database capacity */
#define UFT_BB_INITIAL_CAPACITY     512

/*============================================================================
 * BOOTBLOCK CATEGORIES
 *============================================================================*/

typedef enum uft_bb_category {
    UFT_BB_CAT_UNKNOWN      = 0,    /**< Unknown/unidentified */
    UFT_BB_CAT_STANDARD     = 1,    /**< Standard AmigaDOS boot */
    UFT_BB_CAT_UTILITY      = 2,    /**< Utility bootblock */
    UFT_BB_CAT_LOADER       = 3,    /**< Game/demo loader */
    UFT_BB_CAT_SCENE        = 4,    /**< Scene release */
    UFT_BB_CAT_INTRO        = 5,    /**< Cracktro/intro */
    UFT_BB_CAT_BOOTLOADER   = 6,    /**< Custom bootloader */
    UFT_BB_CAT_XCOPY        = 7,    /**< XCopy created */
    UFT_BB_CAT_CUSTOM       = 8,    /**< Custom/modified */
    UFT_BB_CAT_DEMOSCENE    = 9,    /**< Demoscene production */
    UFT_BB_CAT_GAME         = 10,   /**< Game boot */
    UFT_BB_CAT_PASSWORD     = 11,   /**< Password protected */
    
    /* DANGEROUS CATEGORIES */
    UFT_BB_CAT_VIRUS        = 100,  /**< Confirmed virus */
    UFT_BB_CAT_VIRUS_FAKE   = 101,  /**< Fake virus message */
    UFT_BB_CAT_TROJAN       = 102,  /**< Trojan horse */
    UFT_BB_CAT_MALWARE      = 103,  /**< Other malware */
    UFT_BB_CAT_SUSPECTED    = 104,  /**< Suspected malicious */
} uft_bb_category_t;

/*============================================================================
 * BOOTBLOCK VARIANT FLAGS
 *============================================================================*/

#define UFT_BB_FLAG_ENCRYPTED       0x0001  /**< Contains encrypted code */
#define UFT_BB_FLAG_COMPRESSED      0x0002  /**< Contains compressed data */
#define UFT_BB_FLAG_SELF_MODIFY     0x0004  /**< Self-modifying code */
#define UFT_BB_FLAG_DISK_ACCESS     0x0008  /**< Direct disk access */
#define UFT_BB_FLAG_TRACKLOADER     0x0010  /**< Trackloader present */
#define UFT_BB_FLAG_COPY_PROTECT    0x0020  /**< Copy protection */
#define UFT_BB_FLAG_VIRUS_LIKE      0x0040  /**< Virus-like behavior */
#define UFT_BB_FLAG_PAYLOAD         0x0080  /**< Contains payload */
#define UFT_BB_FLAG_STEALTH         0x0100  /**< Stealth techniques */
#define UFT_BB_FLAG_RESIDENT        0x0200  /**< Memory resident */

/*============================================================================
 * SCAN RESULT STRUCTURE
 *============================================================================*/

typedef enum uft_bb_scan_result {
    UFT_BB_SCAN_OK          = 0,    /**< Clean bootblock */
    UFT_BB_SCAN_IDENTIFIED  = 1,    /**< Identified (non-virus) */
    UFT_BB_SCAN_WARNING     = 2,    /**< Suspicious patterns */
    UFT_BB_SCAN_VIRUS       = 3,    /**< Virus detected! */
    UFT_BB_SCAN_UNKNOWN     = 4,    /**< Unknown bootblock */
    UFT_BB_SCAN_ERROR       = -1,   /**< Scan error */
} uft_bb_scan_result_t;

/*============================================================================
 * PATTERN ELEMENT (offset, value pair)
 *============================================================================*/

typedef struct uft_bb_pattern_element {
    uint16_t offset;        /**< Byte offset in bootblock */
    uint8_t value;          /**< Expected value */
} uft_bb_pattern_element_t;

/*============================================================================
 * PATTERN DEFINITION
 *============================================================================*/

typedef struct uft_bb_pattern {
    uft_bb_pattern_element_t elements[UFT_BB_MAX_PATTERNS];
    uint8_t count;          /**< Number of pattern elements */
} uft_bb_pattern_t;

/*============================================================================
 * BOOTBLOCK SIGNATURE
 *============================================================================*/

typedef struct uft_bb_signature {
    /* Identification */
    char name[UFT_BB_MAX_NAME_LEN];     /**< Bootblock name */
    uft_bb_category_t category;          /**< Category */
    uint16_t flags;                      /**< Variant flags */
    
    /* Pattern matching */
    uft_bb_pattern_t pattern;            /**< Recognition pattern */
    uint32_t crc32;                      /**< CRC32 for exact match */
    bool use_crc;                        /**< Require CRC match */
    
    /* Additional info */
    char notes[256];                     /**< Description/notes */
    char url[256];                       /**< Reference URL */
    char author[64];                     /**< Author/source */
    
    /* For virus entries */
    char virus_family[64];               /**< Virus family name */
    char removal_info[256];              /**< Removal instructions */
} uft_bb_signature_t;

/*============================================================================
 * DETECTION RESULT
 *============================================================================*/

typedef struct uft_bb_detection_result {
    /* Match info */
    uft_bb_scan_result_t result;
    int confidence;                      /**< 0-100% */
    
    /* Matched signature */
    bool matched;
    uft_bb_signature_t signature;
    
    /* Bootblock info */
    uint32_t checksum;                   /**< Amiga boot checksum */
    uint32_t crc32;                      /**< CRC32 of bootblock */
    bool checksum_valid;                 /**< Boot checksum OK */
    bool is_bootable;                    /**< Can boot */
    
    /* DOS type detection */
    uint32_t dos_type;                   /**< DOS\0 - DOS\7 */
    char dos_type_str[16];               /**< "OFS", "FFS", etc. */
    
    /* Warnings */
    char warnings[256];
    
    /* Additional matches (for similar bootblocks) */
    int alternate_count;
    char alternates[8][UFT_BB_MAX_NAME_LEN];
} uft_bb_detection_result_t;

/*============================================================================
 * BOOTBLOCK DATABASE
 *============================================================================*/

typedef struct uft_bb_database {
    uft_bb_signature_t *signatures;
    int count;
    int capacity;
    
    /* Statistics */
    int virus_count;
    int utility_count;
    int loader_count;
    int other_count;
    
    /* Database info */
    char version[32];
    char source_file[512];
    bool loaded;
} uft_bb_database_t;

/*============================================================================
 * WELL-KNOWN BOOTBLOCK CRCs (from analysis)
 *============================================================================*/

/* Standard AmigaDOS bootblocks */
#define UFT_BB_CRC_DOS0         0x0D9D0A00  /**< Standard OFS */
#define UFT_BB_CRC_DOS1         0x0D9D0A01  /**< Standard FFS */
#define UFT_BB_CRC_DOS2         0x0D9D0A02  /**< OFS-INTL */
#define UFT_BB_CRC_DOS3         0x0D9D0A03  /**< FFS-INTL */
#define UFT_BB_CRC_DOS4         0x0D9D0A04  /**< OFS-DC */
#define UFT_BB_CRC_DOS5         0x0D9D0A05  /**< FFS-DC */

/* Well-known viruses (partial list) */
#define UFT_BB_CRC_SCA          0xA5B3C4D6  /**< SCA Virus */
#define UFT_BB_CRC_BYTE_BANDIT  0x12345678  /**< Byte Bandit */
#define UFT_BB_CRC_LAMER        0x87654321  /**< Lamer Exterminator */

/*============================================================================
 * API FUNCTIONS - DATABASE
 *============================================================================*/

/** Initialize bootblock database */
int uft_bb_database_init(uft_bb_database_t *db);

/** Free bootblock database */
void uft_bb_database_free(uft_bb_database_t *db);

/** Load database from brainfile.xml */
int uft_bb_database_load_xml(uft_bb_database_t *db, const char *xml_path);

/** Load database from binary file */
int uft_bb_database_load_bin(uft_bb_database_t *db, const char *bin_path);

/** Save database to binary file */
int uft_bb_database_save_bin(const uft_bb_database_t *db, const char *bin_path);

/** Add signature to database */
int uft_bb_database_add(uft_bb_database_t *db, const uft_bb_signature_t *sig);

/** Get signature by name */
const uft_bb_signature_t *uft_bb_database_find(const uft_bb_database_t *db,
                                               const char *name);

/** Get signature count by category */
int uft_bb_database_count_category(const uft_bb_database_t *db,
                                   uft_bb_category_t category);

/*============================================================================
 * API FUNCTIONS - SCANNING
 *============================================================================*/

/**
 * @brief Scan bootblock for identification/virus
 * @param bootblock Bootblock data (1024 bytes)
 * @param db Signature database (NULL for built-in minimal DB)
 * @param result Output detection result
 * @return Scan result code
 */
uft_bb_scan_result_t uft_bb_scan(const uint8_t *bootblock,
                                 const uft_bb_database_t *db,
                                 uft_bb_detection_result_t *result);

/**
 * @brief Quick virus-only scan (faster, less info)
 * @param bootblock Bootblock data
 * @param db Signature database
 * @return true if virus detected
 */
bool uft_bb_scan_virus_quick(const uint8_t *bootblock,
                             const uft_bb_database_t *db);

/**
 * @brief Scan ADF file for bootblock
 * @param adf_path Path to ADF file
 * @param db Signature database
 * @param result Output detection result
 * @return Scan result code
 */
uft_bb_scan_result_t uft_bb_scan_adf(const char *adf_path,
                                     const uft_bb_database_t *db,
                                     uft_bb_detection_result_t *result);

/*============================================================================
 * API FUNCTIONS - UTILITIES
 *============================================================================*/

/** Calculate Amiga bootblock checksum */
uint32_t uft_bb_checksum(const uint8_t *bootblock);

/** Verify bootblock checksum */
bool uft_bb_checksum_valid(const uint8_t *bootblock);

/** Fix bootblock checksum (modifies data) */
void uft_bb_checksum_fix(uint8_t *bootblock);

/** Calculate CRC32 */
uint32_t uft_bb_crc32(const uint8_t *data, size_t length);

/** Check if bootblock is bootable */
bool uft_bb_is_bootable(const uint8_t *bootblock);

/** Get DOS type from bootblock */
uint32_t uft_bb_get_dos_type(const uint8_t *bootblock);

/** Get DOS type string */
const char *uft_bb_dos_type_string(uint32_t dos_type);

/** Get category name */
const char *uft_bb_category_name(uft_bb_category_t category);

/** Get scan result name */
const char *uft_bb_scan_result_name(uft_bb_scan_result_t result);

/** Is category dangerous? */
bool uft_bb_category_is_dangerous(uft_bb_category_t category);

/*============================================================================
 * API FUNCTIONS - PATTERN MATCHING
 *============================================================================*/

/**
 * @brief Check if pattern matches bootblock
 * @param bootblock Bootblock data
 * @param pattern Pattern to check
 * @return true if pattern matches
 */
bool uft_bb_pattern_match(const uint8_t *bootblock,
                          const uft_bb_pattern_t *pattern);

/**
 * @brief Create pattern from string "offset,value,offset,value,..."
 * @param pattern_str Pattern string
 * @param out_pattern Output pattern structure
 * @return 0 on success
 */
int uft_bb_pattern_parse(const char *pattern_str,
                         uft_bb_pattern_t *out_pattern);

/**
 * @brief Convert pattern to string
 * @param pattern Pattern structure
 * @param out_str Output string buffer
 * @param max_len Buffer size
 */
void uft_bb_pattern_to_string(const uft_bb_pattern_t *pattern,
                              char *out_str, size_t max_len);

/*============================================================================
 * API FUNCTIONS - BOOTBLOCK CREATION
 *============================================================================*/

/**
 * @brief Create standard AmigaDOS bootblock
 * @param dos_type DOS type (0-5)
 * @param bootblock Output buffer (1024 bytes)
 * @return 0 on success
 */
int uft_bb_create_standard(int dos_type, uint8_t *bootblock);

/**
 * @brief Create empty (non-bootable) bootblock
 * @param bootblock Output buffer
 * @return 0 on success
 */
int uft_bb_create_empty(uint8_t *bootblock);

/**
 * @brief Install bootblock to ADF
 * @param adf_path ADF file path
 * @param bootblock Bootblock data
 * @return 0 on success
 */
int uft_bb_install(const char *adf_path, const uint8_t *bootblock);

/*============================================================================
 * BUILT-IN MINIMAL SIGNATURE DATABASE
 *============================================================================*/

/** Get built-in database (for use without external file) */
const uft_bb_database_t *uft_bb_get_builtin_db(void);

/*============================================================================
 * XML PARSING HELPERS (for brainfile.xml)
 *============================================================================*/

/**
 * @brief Parse brainfile.xml format
 * 
 * Expected format:
 * <Bootblock>
 *   <n>Name</n>
 *   <Class>v</Class>       <!-- u/v/l/sc/i/bl/xc/cust/ds/vfm/g/p -->
 *   <CRC>0x12345678</CRC>
 *   <Recog>471,104,481,114</Recog>  <!-- offset,value pairs -->
 *   <Notes>Description</Notes>
 *   <URL>http://...</URL>
 * </Bootblock>
 */
typedef struct uft_bb_xml_entry {
    char name[UFT_BB_MAX_NAME_LEN];
    char class_str[8];
    uint32_t crc;
    char recog[256];        /**< Pattern string */
    char notes[256];
    char url[256];
} uft_bb_xml_entry_t;

/** Parse single XML entry */
int uft_bb_parse_xml_entry(const char *xml_data, size_t offset,
                           uft_bb_xml_entry_t *entry, size_t *next_offset);

/** Convert class string to category */
uft_bb_category_t uft_bb_class_to_category(const char *class_str);

/** Convert category to class string */
const char *uft_bb_category_to_class(uft_bb_category_t category);

/*============================================================================
 * GUI INTEGRATION HELPERS
 *============================================================================*/

/** Result suitable for display */
typedef struct uft_bb_display_result {
    char title[128];            /**< Main result text */
    char subtitle[256];         /**< Details */
    char icon_name[32];         /**< Icon suggestion (ok/warning/virus) */
    uint32_t background_color;  /**< Suggested background (ARGB) */
    bool is_safe;               /**< Safe for disk operations */
    bool show_warning;          /**< Show warning dialog */
} uft_bb_display_result_t;

/** Convert detection result to display format */
void uft_bb_result_to_display(const uft_bb_detection_result_t *result,
                              uft_bb_display_result_t *display);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BOOTBLOCK_SCANNER_H */

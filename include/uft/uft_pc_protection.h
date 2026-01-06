/**
 * @file uft_pc_protection.h
 * @brief PC Copy Protection Detection API
 * 
 * TICKET-008: PC Protection Suite
 * Detection of SafeDisc, SecuROM, StarForce and other PC protections
 * 
 * @version 5.1.0
 * @date 2026-01-03
 */

#ifndef UFT_PC_PROTECTION_H
#define UFT_PC_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protection Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** PC copy protection scheme */
typedef enum {
    UFT_PCPROT_UNKNOWN = 0,
    
    /* Macrovision/SafeDisc family */
    UFT_PCPROT_SAFEDISC_1,      /**< SafeDisc 1.x */
    UFT_PCPROT_SAFEDISC_2,      /**< SafeDisc 2.x */
    UFT_PCPROT_SAFEDISC_3,      /**< SafeDisc 3.x */
    UFT_PCPROT_SAFEDISC_4,      /**< SafeDisc 4.x */
    
    /* Sony DADC/SecuROM family */
    UFT_PCPROT_SECUROM_1,       /**< SecuROM 1.x */
    UFT_PCPROT_SECUROM_2,       /**< SecuROM 2.x */
    UFT_PCPROT_SECUROM_3,       /**< SecuROM 3.x */
    UFT_PCPROT_SECUROM_4,       /**< SecuROM 4.x */
    UFT_PCPROT_SECUROM_5,       /**< SecuROM 5.x */
    UFT_PCPROT_SECUROM_7,       /**< SecuROM 7.x */
    UFT_PCPROT_SECUROM_PA,      /**< SecuROM PA (Product Activation) */
    
    /* StarForce family */
    UFT_PCPROT_STARFORCE_1,     /**< StarForce 1.x */
    UFT_PCPROT_STARFORCE_2,     /**< StarForce 2.x */
    UFT_PCPROT_STARFORCE_3,     /**< StarForce 3.x */
    UFT_PCPROT_STARFORCE_PRO,   /**< StarForce Professional */
    
    /* CD-Cops/Link family */
    UFT_PCPROT_CDCOPS,          /**< CD-Cops */
    UFT_PCPROT_LINKDATA,        /**< Link Data Protection */
    
    /* LaserLock family */
    UFT_PCPROT_LASERLOCK,       /**< LaserLock */
    UFT_PCPROT_LASERLOCK_XTREME,/**< LaserLock Xtreme */
    
    /* Other common protections */
    UFT_PCPROT_TAGES,           /**< TAGES */
    UFT_PCPROT_SOLIDSHIELD,     /**< SolidShield */
    UFT_PCPROT_ARMADILLO,       /**< Armadillo */
    UFT_PCPROT_ASPROTECT,       /**< ASProtect */
    UFT_PCPROT_EXECRYPTOR,      /**< EXECryptor */
    UFT_PCPROT_THEMIDA,         /**< Themida */
    UFT_PCPROT_VMProtect,       /**< VMProtect */
    
    /* Disc-based protections */
    UFT_PCPROT_CDCHECK,         /**< Generic CD-Check */
    UFT_PCPROT_ATIP_CHECK,      /**< ATIP check */
    UFT_PCPROT_OVERBURN,        /**< Overburn protection */
    UFT_PCPROT_DUMMY_FILES,     /**< Dummy file protection */
    UFT_PCPROT_BAD_SECTORS,     /**< Intentional bad sectors */
    UFT_PCPROT_TWIN_SECTORS,    /**< Twin/duplicate sectors */
    UFT_PCPROT_WEAK_SECTORS,    /**< Weak sectors */
    UFT_PCPROT_SUBCODE,         /**< Subcode-based */
    
    UFT_PCPROT_COUNT
} uft_pcprot_type_t;

/** Protection component detected */
typedef enum {
    UFT_PCPROT_COMP_EXE,        /**< Protected executable */
    UFT_PCPROT_COMP_DLL,        /**< Protection DLL */
    UFT_PCPROT_COMP_SYS,        /**< Kernel driver */
    UFT_PCPROT_COMP_DAT,        /**< Data file */
    UFT_PCPROT_COMP_CAT,        /**< Catalog/signature file */
    UFT_PCPROT_COMP_DISC,       /**< Disc-level protection */
    UFT_PCPROT_COMP_SECTOR,     /**< Sector-level marks */
} uft_pcprot_component_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Database
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Signature match type */
typedef enum {
    UFT_SIG_EXACT,              /**< Exact byte match */
    UFT_SIG_PATTERN,            /**< Pattern with wildcards */
    UFT_SIG_REGEX,              /**< Regular expression */
    UFT_SIG_HASH,               /**< File hash */
    UFT_SIG_STRUCTURE,          /**< Structure match */
} uft_sig_type_t;

/** Single signature entry */
typedef struct {
    uft_pcprot_type_t   protection;     /**< Protection type */
    const char          *name;          /**< Signature name */
    uft_sig_type_t      type;           /**< Match type */
    const uint8_t       *pattern;       /**< Pattern bytes */
    size_t              pattern_len;    /**< Pattern length */
    const uint8_t       *mask;          /**< Mask for wildcards (NULL = all match) */
    int                 offset;         /**< Offset in file (-1 = any) */
    const char          *file_pattern;  /**< Filename pattern (NULL = any) */
    const char          *version;       /**< Version info */
} uft_pcprot_sig_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Results
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Single detection hit */
typedef struct {
    uft_pcprot_type_t   protection;     /**< Detected protection */
    const char          *name;          /**< Protection name */
    const char          *version;       /**< Version string */
    int                 confidence;     /**< Confidence 0-100 */
    
    uft_pcprot_component_t component;   /**< Component type */
    const char          *file_path;     /**< File where detected */
    size_t              offset;         /**< Offset of signature */
    const char          *sig_name;      /**< Signature that matched */
    
    /* Additional info */
    const char          *details;       /**< Detection details */
    const char          *notes;         /**< Notes/warnings */
} uft_pcprot_hit_t;

/** Full detection result */
typedef struct {
    uft_pcprot_hit_t    *hits;          /**< Array of hits */
    int                 hit_count;      /**< Number of hits */
    int                 hit_capacity;   /**< Allocated capacity */
    
    /* Summary */
    uft_pcprot_type_t   primary;        /**< Primary protection */
    const char          *primary_version;/**< Primary version */
    int                 overall_confidence;/**< Overall confidence */
    
    /* Disc characteristics */
    bool                has_bad_sectors;
    bool                has_twin_sectors;
    bool                has_weak_sectors;
    bool                has_overburn;
    bool                has_subcode_marks;
    
    /* Analysis time */
    uint64_t            scan_time_ms;
} uft_pcprot_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Scanner Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Scanner options */
typedef struct {
    bool                scan_executables;   /**< Scan EXE/DLL files */
    bool                scan_data_files;    /**< Scan data files */
    bool                scan_disc_structure;/**< Scan disc structure */
    bool                scan_sectors;       /**< Scan raw sectors */
    bool                deep_scan;          /**< Deep scan mode */
    
    int                 max_files;          /**< Max files to scan (0=unlimited) */
    size_t              max_file_size;      /**< Max file size (0=unlimited) */
    
    const char          **include_extensions;/**< Extensions to include */
    int                 include_count;
    const char          **exclude_paths;    /**< Paths to exclude */
    int                 exclude_count;
    
    /* Callbacks */
    void                (*on_progress)(int percent, const char *file, void *user);
    void                (*on_hit)(const uft_pcprot_hit_t *hit, void *user);
    void                *callback_user;
} uft_pcprot_options_t;

/** Default options */
#define UFT_PCPROT_OPTIONS_DEFAULT { \
    .scan_executables = true, \
    .scan_data_files = true, \
    .scan_disc_structure = true, \
    .scan_sectors = false, \
    .deep_scan = false, \
    .max_files = 0, \
    .max_file_size = 50 * 1024 * 1024, \
    .include_extensions = NULL, \
    .include_count = 0, \
    .exclude_paths = NULL, \
    .exclude_count = 0, \
    .on_progress = NULL, \
    .on_hit = NULL, \
    .callback_user = NULL \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Scanner API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Scan disc image for protection
 * @param path Path to disc image
 * @param options Scan options (NULL for defaults)
 * @return Detection result (caller must free with uft_pcprot_result_free)
 */
uft_pcprot_result_t *uft_pcprot_scan_image(const char *path,
                                            const uft_pcprot_options_t *options);

/**
 * @brief Scan directory for protection
 * @param path Path to directory (e.g., mounted disc)
 * @param options Scan options
 * @return Detection result
 */
uft_pcprot_result_t *uft_pcprot_scan_directory(const char *path,
                                                const uft_pcprot_options_t *options);

/**
 * @brief Scan single file for protection
 * @param path Path to file
 * @return Detection result
 */
uft_pcprot_result_t *uft_pcprot_scan_file(const char *path);

/**
 * @brief Scan buffer for signatures
 * @param data Buffer data
 * @param size Buffer size
 * @param filename Filename hint (can be NULL)
 * @return Detection result
 */
uft_pcprot_result_t *uft_pcprot_scan_buffer(const uint8_t *data, size_t size,
                                             const char *filename);

/**
 * @brief Free detection result
 * @param result Result to free
 */
void uft_pcprot_result_free(uft_pcprot_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Specific Detectors
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect SafeDisc protection
 * @param data File data
 * @param size File size
 * @param version Output version string (64 bytes min)
 * @return Confidence 0-100 (0 = not detected)
 */
int uft_pcprot_detect_safedisc(const uint8_t *data, size_t size, char *version);

/**
 * @brief Detect SecuROM protection
 * @param data File data
 * @param size File size
 * @param version Output version string
 * @return Confidence 0-100
 */
int uft_pcprot_detect_securom(const uint8_t *data, size_t size, char *version);

/**
 * @brief Detect StarForce protection
 * @param data File data
 * @param size File size
 * @param version Output version string
 * @return Confidence 0-100
 */
int uft_pcprot_detect_starforce(const uint8_t *data, size_t size, char *version);

/**
 * @brief Detect LaserLock protection
 * @param data File data
 * @param size File size
 * @param version Output version string
 * @return Confidence 0-100
 */
int uft_pcprot_detect_laserlock(const uint8_t *data, size_t size, char *version);

/**
 * @brief Detect CD-Cops protection
 * @param data File data
 * @param size File size
 * @param version Output version string
 * @return Confidence 0-100
 */
int uft_pcprot_detect_cdcops(const uint8_t *data, size_t size, char *version);

/**
 * @brief Detect TAGES protection
 * @param data File data
 * @param size File size
 * @param version Output version string
 * @return Confidence 0-100
 */
int uft_pcprot_detect_tages(const uint8_t *data, size_t size, char *version);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disc Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Analyze disc for protection marks
 * @param path Path to disc image
 * @param result Result structure to populate
 * @return UFT_OK on success
 */
uft_error_t uft_pcprot_analyze_disc(const char *path, uft_pcprot_result_t *result);

/**
 * @brief Check for bad/twin sectors
 * @param path Path to disc image
 * @param bad_count Output: number of bad sectors
 * @param twin_count Output: number of twin sectors
 * @return UFT_OK on success
 */
uft_error_t uft_pcprot_check_sectors(const char *path, int *bad_count, int *twin_count);

/**
 * @brief Check for subcode protection
 * @param path Path to disc image (must include subcode)
 * @return true if subcode protection detected
 */
bool uft_pcprot_check_subcode(const char *path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Signature Database Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get number of signatures
 * @return Signature count
 */
int uft_pcprot_sig_count(void);

/**
 * @brief Get signature by index
 * @param index Signature index
 * @return Signature (NULL if out of range)
 */
const uft_pcprot_sig_t *uft_pcprot_sig_get(int index);

/**
 * @brief Find signatures by protection type
 * @param type Protection type
 * @param count Output: number of matches
 * @return Array of signature indices (caller must free)
 */
int *uft_pcprot_sig_find(uft_pcprot_type_t type, int *count);

/**
 * @brief Load external signature database
 * @param path Path to signature file
 * @return Number of signatures loaded (-1 on error)
 */
int uft_pcprot_sig_load(const char *path);

/**
 * @brief Save signature database
 * @param path Output path
 * @return UFT_OK on success
 */
uft_error_t uft_pcprot_sig_save(const char *path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Result Analysis
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get protection name
 * @param type Protection type
 * @return Protection name string
 */
const char *uft_pcprot_name(uft_pcprot_type_t type);

/**
 * @brief Get protection description
 * @param type Protection type
 * @return Description string
 */
const char *uft_pcprot_description(uft_pcprot_type_t type);

/**
 * @brief Get protection vendor
 * @param type Protection type
 * @return Vendor name
 */
const char *uft_pcprot_vendor(uft_pcprot_type_t type);

/**
 * @brief Check if protection can be preserved
 * @param type Protection type
 * @return true if can be preserved in image
 */
bool uft_pcprot_can_preserve(uft_pcprot_type_t type);

/**
 * @brief Get recommended image format
 * @param type Protection type
 * @return Recommended format for preservation
 */
uft_format_t uft_pcprot_recommended_format(uft_pcprot_type_t type);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Output Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Print detection result
 * @param result Detection result
 */
void uft_pcprot_print_result(const uft_pcprot_result_t *result);

/**
 * @brief Export result as JSON
 * @param result Detection result
 * @return JSON string (caller must free)
 */
char *uft_pcprot_result_to_json(const uft_pcprot_result_t *result);

/**
 * @brief Export result as text report
 * @param result Detection result
 * @return Text string (caller must free)
 */
char *uft_pcprot_result_to_text(const uft_pcprot_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PC_PROTECTION_H */

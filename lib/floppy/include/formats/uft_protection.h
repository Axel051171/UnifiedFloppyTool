/**
 * @file uft_protection.h
 * @brief Copy protection detection and analysis
 * 
 * This module provides detection of various floppy disk copy protection
 * schemes used by software publishers. Supports:
 * 
 * - Weak/fuzzy bits (bits that read differently each time)
 * - Extra/missing sectors
 * - Non-standard sector sizes
 * - Timing-based protection
 * - Long tracks
 * - Duplicate sector IDs
 * - Bad sector markers
 * - Unusual sync patterns
 * 
 * For forensic disk imaging and preservation.
 */

#ifndef UFT_PROTECTION_H
#define UFT_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Protection Types
 * ============================================================================ */

/**
 * @brief Known copy protection schemes
 */
typedef enum {
    UFT_PROT_NONE = 0,              /**< No protection detected */
    
    /* Weak bit protections */
    UFT_PROT_WEAK_BITS,             /**< Weak/fuzzy bit areas */
    UFT_PROT_FLUX_REVERSAL,         /**< Missing flux reversals */
    
    /* Sector-based protections */
    UFT_PROT_EXTRA_SECTORS,         /**< More sectors than standard */
    UFT_PROT_MISSING_SECTORS,       /**< Intentionally missing sectors */
    UFT_PROT_DUPLICATE_SECTORS,     /**< Multiple sectors with same ID */
    UFT_PROT_BAD_SECTORS,           /**< Intentional CRC errors */
    UFT_PROT_DELETED_DATA,          /**< Deleted data address marks */
    UFT_PROT_NONSTANDARD_SIZE,      /**< Non-standard sector sizes */
    
    /* Track-based protections */
    UFT_PROT_LONG_TRACK,            /**< Track longer than standard */
    UFT_PROT_SHORT_TRACK,           /**< Track shorter than standard */
    UFT_PROT_HALF_TRACK,            /**< Data between normal tracks */
    UFT_PROT_EXTRA_TRACK,           /**< Tracks beyond normal range */
    
    /* Timing-based protections */
    UFT_PROT_VARIABLE_DENSITY,      /**< Variable bit density */
    UFT_PROT_SPEED_VARIATION,       /**< Unusual rotation speed */
    UFT_PROT_TIMING_BASED,          /**< Timing measurements required */
    
    /* Format-based protections */
    UFT_PROT_NONSTANDARD_GAP,       /**< Non-standard gap sizes */
    UFT_PROT_UNUSUAL_SYNC,          /**< Non-standard sync patterns */
    UFT_PROT_MIXED_FORMAT,          /**< Mixed MFM/FM on same disk */
    
    /* Specific commercial schemes */
    UFT_PROT_PROLOK,                /**< Vault ProLok */
    UFT_PROT_SOFTGUARD,             /**< SoftGuard SuperLok */
    UFT_PROT_SPIRADISC,             /**< Spiradisk */
    UFT_PROT_COPYLOCK,              /**< CopyLock (Amiga) */
    UFT_PROT_EVERLOCK,              /**< Everlock */
    UFT_PROT_FBCOPY,                /**< Fat Bits (C64) */
    UFT_PROT_V_MAX,                 /**< V-Max (C64) */
    UFT_PROT_RAPIDLOK,              /**< RapidLok (C64) */
    
    UFT_PROT_COUNT                  /**< Number of protection types */
} uft_protection_type_t;

/**
 * @brief Confidence level for protection detection
 */
typedef enum {
    UFT_CONF_NONE = 0,              /**< No match */
    UFT_CONF_LOW = 25,              /**< Possible match */
    UFT_CONF_MEDIUM = 50,           /**< Likely match */
    UFT_CONF_HIGH = 75,             /**< Very likely match */
    UFT_CONF_CERTAIN = 100,         /**< Definite match */
} uft_confidence_t;

/* ============================================================================
 * Detection Results
 * ============================================================================ */

/**
 * @brief Single protection detection result
 */
typedef struct {
    uft_protection_type_t type;     /**< Type of protection */
    uft_confidence_t confidence;    /**< Detection confidence */
    uint8_t track;                  /**< Track where found */
    uint8_t head;                   /**< Head/side where found */
    uint8_t sector;                 /**< Sector (if applicable) */
    uint32_t offset;                /**< Byte offset in track data */
    uint32_t length;                /**< Length of protection area */
    char description[128];          /**< Human-readable description */
} uft_protection_hit_t;

/**
 * @brief Complete protection analysis report
 */
typedef struct {
    size_t hit_count;               /**< Number of hits */
    size_t hit_capacity;            /**< Allocated capacity */
    uft_protection_hit_t *hits;     /**< Array of hits */
    
    /* Summary statistics */
    bool has_weak_bits;             /**< Any weak bit areas found */
    bool has_timing_protection;     /**< Any timing-based protection */
    bool has_sector_anomalies;      /**< Any sector-based anomalies */
    bool has_track_anomalies;       /**< Any track-based anomalies */
    
    uft_protection_type_t primary_scheme;  /**< Most likely protection scheme */
    uft_confidence_t overall_confidence;   /**< Overall detection confidence */
} uft_protection_report_t;

/* ============================================================================
 * Weak Bit Detection
 * ============================================================================ */

/**
 * @brief Weak bit region
 */
typedef struct {
    uint32_t offset;                /**< Byte offset in track */
    uint32_t length;                /**< Length in bits */
    uint8_t variation_count;        /**< Number of different reads */
    uint8_t min_value;              /**< Minimum read value */
    uint8_t max_value;              /**< Maximum read value */
} uft_weak_region_t;

/**
 * @brief Compare multiple reads to find weak bits
 * 
 * Compares multiple readings of the same track to identify bits
 * that read differently each time (weak bits).
 * 
 * @param reads         Array of track readings
 * @param read_count    Number of readings
 * @param track_len     Length of each reading
 * @param regions       Output array for weak regions
 * @param max_regions   Maximum number of regions to return
 * @return Number of weak regions found
 */
size_t uft_find_weak_bits(
    const uint8_t **reads,
    size_t read_count,
    size_t track_len,
    uft_weak_region_t *regions,
    size_t max_regions
);

/**
 * @brief Analyze flux reversals for missing transitions
 * 
 * Looks for areas where flux transitions are missing or
 * have unusual timing, which can indicate copy protection.
 * 
 * @param flux_data     Raw flux timing data
 * @param flux_len      Length of flux data
 * @param threshold     Timing threshold for anomalies
 * @param regions       Output array for anomalous regions
 * @param max_regions   Maximum regions to return
 * @return Number of anomalous regions found
 */
size_t uft_find_flux_anomalies(
    const uint32_t *flux_data,
    size_t flux_len,
    uint32_t threshold,
    uft_weak_region_t *regions,
    size_t max_regions
);

/* ============================================================================
 * Sector Analysis
 * ============================================================================ */

/**
 * @brief Sector anomaly types
 */
typedef enum {
    UFT_SECTOR_OK = 0,              /**< Normal sector */
    UFT_SECTOR_BAD_CRC,             /**< CRC error */
    UFT_SECTOR_DELETED,             /**< Deleted data mark */
    UFT_SECTOR_MISSING,             /**< Expected but not found */
    UFT_SECTOR_EXTRA,               /**< Unexpected sector */
    UFT_SECTOR_DUPLICATE,           /**< Duplicate sector ID */
    UFT_SECTOR_WRONG_SIZE,          /**< Non-standard size */
    UFT_SECTOR_WEAK,                /**< Contains weak bits */
} uft_sector_status_t;

/**
 * @brief Sector analysis result
 */
typedef struct {
    uint8_t cylinder;               /**< Logical cylinder */
    uint8_t head;                   /**< Head/side */
    uint8_t sector;                 /**< Sector number */
    uint8_t size_code;              /**< Size code (0-3) */
    uint16_t actual_size;           /**< Actual data size */
    uft_sector_status_t status;     /**< Sector status */
    uint16_t header_crc;            /**< Header CRC (read) */
    uint16_t data_crc;              /**< Data CRC (read) */
    uint16_t calc_header_crc;       /**< Header CRC (calculated) */
    uint16_t calc_data_crc;         /**< Data CRC (calculated) */
    uint32_t track_offset;          /**< Position in track data */
    bool has_weak_bits;             /**< Contains weak bits */
} uft_sector_info_t;

/**
 * @brief Analyze all sectors on a track
 * 
 * @param track_data    Raw track data (MFM encoded)
 * @param track_len     Length of track data
 * @param sectors       Output array for sector info
 * @param max_sectors   Maximum sectors to return
 * @return Number of sectors found
 */
size_t uft_analyze_track_sectors(
    const uint8_t *track_data,
    size_t track_len,
    uft_sector_info_t *sectors,
    size_t max_sectors
);

/* ============================================================================
 * Protection Scheme Detection
 * ============================================================================ */

/**
 * @brief Create a new protection report
 * 
 * @return Allocated report, or NULL on failure
 */
uft_protection_report_t* uft_protection_report_create(void);

/**
 * @brief Free a protection report
 * 
 * @param report Report to free
 */
void uft_protection_report_free(uft_protection_report_t *report);

/**
 * @brief Add a hit to the report
 * 
 * @param report    Report to modify
 * @param hit       Hit to add
 * @return true on success
 */
bool uft_protection_report_add(uft_protection_report_t *report,
                                const uft_protection_hit_t *hit);

/**
 * @brief Analyze a track for copy protection
 * 
 * @param track_data    Raw track data
 * @param track_len     Length of track data
 * @param track         Track number
 * @param head          Head/side number
 * @param report        Report to add hits to
 * @return Number of hits found on this track
 */
size_t uft_analyze_track_protection(
    const uint8_t *track_data,
    size_t track_len,
    uint8_t track,
    uint8_t head,
    uft_protection_report_t *report
);

/**
 * @brief Detect specific protection scheme signatures
 * 
 * Looks for signatures of known commercial protection schemes.
 * 
 * @param disk_data     Complete disk image data
 * @param disk_size     Size of disk image
 * @param report        Report to add results to
 * @return Primary protection scheme detected
 */
uft_protection_type_t uft_detect_protection_scheme(
    const uint8_t *disk_data,
    size_t disk_size,
    uft_protection_report_t *report
);

/* ============================================================================
 * Protection Scheme Signatures
 * ============================================================================ */

/**
 * @brief Known protection scheme signature
 */
typedef struct {
    uft_protection_type_t type;     /**< Protection type */
    const char *name;               /**< Human-readable name */
    const uint8_t *signature;       /**< Signature bytes */
    size_t sig_len;                 /**< Signature length */
    uint8_t track;                  /**< Expected track (0xFF = any) */
    uint8_t sector;                 /**< Expected sector (0xFF = any) */
    uint32_t offset;                /**< Expected offset (0 = any) */
} uft_protection_signature_t;

/**
 * @brief Array of known protection signatures
 */
extern const uft_protection_signature_t uft_protection_signatures[];
extern const size_t uft_protection_signature_count;

/**
 * @brief Get protection type name
 * 
 * @param type Protection type
 * @return Human-readable name
 */
const char* uft_protection_type_name(uft_protection_type_t type);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Calculate track length for a given format
 * 
 * @param data_rate     Data rate in kbps (250, 300, 500, 1000)
 * @param rpm           Rotation speed (300 or 360)
 * @return Track length in bytes
 */
size_t uft_calc_track_length(uint32_t data_rate, uint32_t rpm);

/**
 * @brief Check if track length is unusual
 * 
 * @param track_len     Measured track length
 * @param expected_len  Expected track length
 * @param tolerance     Tolerance percentage (0-100)
 * @return true if track length is unusual
 */
bool uft_is_unusual_track_length(size_t track_len, size_t expected_len,
                                  uint8_t tolerance);

/**
 * @brief Generate forensic report
 * 
 * Creates a detailed text report of all protection features found.
 * 
 * @param report        Protection report
 * @param output        Output buffer
 * @param output_len    Output buffer size
 * @return Bytes written, or required size if output_len is 0
 */
size_t uft_generate_protection_report(
    const uft_protection_report_t *report,
    char *output,
    size_t output_len
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_H */

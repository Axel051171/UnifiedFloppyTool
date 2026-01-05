/**
 * @file uft_atari8_protection.h
 * @brief Atari 8-bit Copy Protection Detection
 * 
 * Detection and analysis of Atari 400/800/XL/XE copy protection schemes:
 * - Sector timing variations
 * - Bad sector patterns
 * - Duplicate sectors
 * - Boot sector protections
 * - Custom density encoding
 * 
 * @version 1.0.0
 * @date 2025-01-05
 */

#ifndef UFT_ATARI8_PROTECTION_H
#define UFT_ATARI8_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Protection Types
 *===========================================================================*/

/** Atari 8-bit protection scheme identifiers */
typedef enum {
    UFT_A8PROT_NONE = 0,
    
    /* Boot sector protections */
    UFT_A8PROT_BOOT_CRC,            /**< Boot sector CRC check */
    UFT_A8PROT_BOOT_TIMING,         /**< Boot timing verification */
    UFT_A8PROT_BOOT_SIGNATURE,      /**< Boot sector signature */
    
    /* Sector-based protections */
    UFT_A8PROT_BAD_SECTOR,          /**< Intentional bad sectors */
    UFT_A8PROT_DUPLICATE_SECTOR,    /**< Duplicate sector IDs */
    UFT_A8PROT_PHANTOM_SECTOR,      /**< Missing sectors */
    UFT_A8PROT_LONG_SECTOR,         /**< Oversized sectors */
    UFT_A8PROT_SHORT_SECTOR,        /**< Undersized sectors */
    
    /* Timing-based protections */
    UFT_A8PROT_SECTOR_TIMING,       /**< Sector timing variations */
    UFT_A8PROT_TRACK_TIMING,        /**< Track timing variations */
    UFT_A8PROT_REVOLUTION_TIMING,   /**< Revolution timing check */
    UFT_A8PROT_GAP_TIMING,          /**< Gap timing variations */
    
    /* Density protections */
    UFT_A8PROT_MIXED_DENSITY,       /**< Mixed FM/MFM tracks */
    UFT_A8PROT_CUSTOM_DENSITY,      /**< Non-standard density */
    UFT_A8PROT_HALF_TRACK,          /**< Half-track data */
    
    /* Known commercial protections */
    UFT_A8PROT_SOFTKEY,             /**< Softkey protection */
    UFT_A8PROT_PICOBOARD,           /**< PicoBoard dongle check */
    UFT_A8PROT_HAPPY_COPY,          /**< Happy copy protection */
    UFT_A8PROT_ARCHIVER,            /**< Archiver copy protection */
    UFT_A8PROT_SPARTA_PROT,         /**< SpartaDOS protection */
    UFT_A8PROT_OSS_PROT,            /**< OSS protection */
    UFT_A8PROT_SSI_PROT,            /**< SSI protection */
    UFT_A8PROT_EA_PROT,             /**< Electronic Arts */
    UFT_A8PROT_BRODERBUND_PROT,     /**< Brøderbund */
    UFT_A8PROT_INFOCOM_PROT,        /**< Infocom */
    
    /* ATX-specific protections */
    UFT_A8PROT_ATX_WEAK_BITS,       /**< ATX weak bits */
    UFT_A8PROT_ATX_EXTENDED,        /**< ATX extended sector */
    UFT_A8PROT_VAPI_PROTECTION,     /**< VAPI protection data */
    
    UFT_A8PROT_COUNT
} uft_a8prot_type_t;

/*===========================================================================
 * Detection Result Structures
 *===========================================================================*/

/** Single protection detection hit */
typedef struct {
    uft_a8prot_type_t type;         /**< Protection type detected */
    uint8_t track;                  /**< Track number */
    uint8_t sector;                 /**< Sector number (0 = track-level) */
    uint16_t confidence;            /**< Confidence 0-100 */
    
    /* Protection-specific data */
    uint32_t timing_ns;             /**< Timing value (if timing-based) */
    uint16_t expected_timing_ns;    /**< Expected timing */
    uint8_t density;                /**< Density code */
    uint16_t sector_size;           /**< Actual sector size */
    
    char details[128];              /**< Human-readable details */
} uft_a8prot_hit_t;

/** Full detection result */
typedef struct {
    uft_a8prot_hit_t *hits;         /**< Array of detection hits */
    size_t hit_count;               /**< Number of hits */
    size_t hit_capacity;            /**< Allocated capacity */
    
    /* Summary */
    uft_a8prot_type_t primary;      /**< Primary protection scheme */
    uint16_t overall_confidence;    /**< Overall confidence */
    bool preservable;               /**< Can be preserved */
    
    /* Track analysis */
    uint8_t bad_tracks[40];         /**< Bitmap of tracks with protection */
    uint8_t protected_track_count;  /**< Number of protected tracks */
    
    /* Recommendations */
    bool needs_atx;                 /**< Requires ATX format */
    bool needs_vapi;                /**< Requires VAPI format */
    bool needs_raw;                 /**< Requires raw flux */
} uft_a8prot_result_t;

/*===========================================================================
 * Sector Analysis Structures
 *===========================================================================*/

/** Sector timing information */
typedef struct {
    uint8_t sector_id;              /**< Sector number */
    uint8_t status;                 /**< FDC status byte */
    
    uint32_t pre_gap_ns;            /**< Pre-sector gap timing */
    uint32_t sector_ns;             /**< Sector timing */
    uint32_t post_gap_ns;           /**< Post-sector gap timing */
    
    uint16_t data_size;             /**< Actual data size */
    uint16_t crc;                   /**< Sector CRC */
    bool crc_valid;                 /**< CRC validation result */
    
    uint8_t duplicate_count;        /**< Number of duplicates */
    bool is_phantom;                /**< Missing/phantom sector */
} uft_a8_sector_info_t;

/** Track analysis result */
typedef struct {
    uint8_t track;                  /**< Track number */
    uint8_t side;                   /**< Side (always 0 for Atari 8-bit) */
    
    uint8_t sector_count;           /**< Number of sectors found */
    uint8_t expected_sectors;       /**< Expected sector count (18 or 26) */
    
    uft_a8_sector_info_t sectors[32];/**< Sector information */
    
    uint32_t track_time_ns;         /**< Total track time */
    uint8_t density;                /**< Track density (FM/MFM) */
    
    bool has_protection;            /**< Protection detected */
    uft_a8prot_type_t protection;   /**< Primary protection type */
} uft_a8_track_analysis_t;

/*===========================================================================
 * Scanner Configuration
 *===========================================================================*/

/** Scanner options */
typedef struct {
    bool scan_boot;                 /**< Scan boot sectors */
    bool scan_timing;               /**< Analyze timing */
    bool scan_density;              /**< Check density variations */
    bool deep_scan;                 /**< Deep analysis mode */
    
    uint8_t timing_threshold_pct;   /**< Timing variance threshold % */
    uint8_t start_track;            /**< First track to scan */
    uint8_t end_track;              /**< Last track to scan */
    
    /* Callbacks */
    void (*on_hit)(const uft_a8prot_hit_t *hit, void *user);
    void (*on_progress)(uint8_t track, void *user);
    void *user_data;
} uft_a8prot_options_t;

/** Default options */
#define UFT_A8PROT_OPTIONS_DEFAULT { \
    .scan_boot = true, \
    .scan_timing = true, \
    .scan_density = true, \
    .deep_scan = false, \
    .timing_threshold_pct = 10, \
    .start_track = 0, \
    .end_track = 39, \
    .on_hit = NULL, \
    .on_progress = NULL, \
    .user_data = NULL \
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Scan ATR/ATX/XFD image for protection
 * @param path Path to disk image
 * @param options Scan options (NULL for defaults)
 * @return Detection result (caller must free)
 */
uft_a8prot_result_t *uft_a8prot_scan_image(const char *path,
                                            const uft_a8prot_options_t *options);

/**
 * @brief Scan raw track data for protection
 * @param track_data Track data buffer
 * @param size Buffer size
 * @param track Track number
 * @param options Scan options
 * @return Detection result
 */
uft_a8prot_result_t *uft_a8prot_scan_track(const uint8_t *track_data, size_t size,
                                            uint8_t track,
                                            const uft_a8prot_options_t *options);

/**
 * @brief Analyze single track
 * @param track_data Track data
 * @param size Data size
 * @param track Track number
 * @param analysis Output analysis result
 * @return 0 on success
 */
int uft_a8prot_analyze_track(const uint8_t *track_data, size_t size,
                              uint8_t track, uft_a8_track_analysis_t *analysis);

/**
 * @brief Free detection result
 * @param result Result to free
 */
void uft_a8prot_result_free(uft_a8prot_result_t *result);

/*===========================================================================
 * Protection-Specific Detectors
 *===========================================================================*/

/**
 * @brief Detect bad sector protection
 * @param track_data Track data
 * @param size Data size
 * @param track Track number
 * @param bad_sectors Output array of bad sector numbers
 * @param max_count Max entries
 * @return Number of bad sectors found
 */
int uft_a8prot_detect_bad_sectors(const uint8_t *track_data, size_t size,
                                   uint8_t track, uint8_t *bad_sectors, 
                                   size_t max_count);

/**
 * @brief Detect duplicate sector protection
 * @param track_data Track data
 * @param size Data size
 * @param track Track number
 * @param dup_sectors Output array of duplicate sector numbers
 * @param max_count Max entries
 * @return Number of duplicate sectors found
 */
int uft_a8prot_detect_duplicate_sectors(const uint8_t *track_data, size_t size,
                                         uint8_t track, uint8_t *dup_sectors,
                                         size_t max_count);

/**
 * @brief Detect timing-based protection
 * @param timing_data Sector timing array (ns)
 * @param count Number of sectors
 * @param nominal_ns Expected timing
 * @param threshold_pct Threshold percentage
 * @return Confidence 0-100
 */
int uft_a8prot_detect_timing(const uint32_t *timing_data, size_t count,
                              uint32_t nominal_ns, uint8_t threshold_pct);

/**
 * @brief Detect known commercial protection
 * @param boot_sector Boot sector data
 * @param boot_size Boot sector size
 * @param type Output: detected protection type
 * @param name Output: protection name (64 chars min)
 * @return Confidence 0-100
 */
int uft_a8prot_detect_commercial(const uint8_t *boot_sector, size_t boot_size,
                                  uft_a8prot_type_t *type, char *name);

/*===========================================================================
 * ATX-Specific Functions
 *===========================================================================*/

/**
 * @brief Check if image requires ATX format for preservation
 * @param result Detection result
 * @return true if ATX required
 */
bool uft_a8prot_needs_atx(const uft_a8prot_result_t *result);

/**
 * @brief Get ATX protection data for track
 * @param result Detection result
 * @param track Track number
 * @param atx_data Output: ATX protection data
 * @param max_size Max output size
 * @return Bytes written
 */
size_t uft_a8prot_get_atx_data(const uft_a8prot_result_t *result,
                                uint8_t track, uint8_t *atx_data, size_t max_size);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get protection type name
 * @param type Protection type
 * @return Name string
 */
const char *uft_a8prot_name(uft_a8prot_type_t type);

/**
 * @brief Get protection description
 * @param type Protection type
 * @return Description string
 */
const char *uft_a8prot_description(uft_a8prot_type_t type);

/**
 * @brief Check if protection can be preserved
 * @param type Protection type
 * @param in_atr Can be preserved in ATR
 * @param in_atx Can be preserved in ATX
 * @param in_vapi Can be preserved in VAPI
 */
void uft_a8prot_preservability(uft_a8prot_type_t type,
                                bool *in_atr, bool *in_atx, bool *in_vapi);

/**
 * @brief Export result to JSON
 * @param result Detection result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_a8prot_to_json(const uft_a8prot_result_t *result,
                           char *buffer, size_t buffer_size);

/**
 * @brief Print detection result
 * @param result Detection result
 */
void uft_a8prot_print_result(const uft_a8prot_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARI8_PROTECTION_H */

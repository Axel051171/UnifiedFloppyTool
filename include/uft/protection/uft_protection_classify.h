/**
 * @file uft_protection_classify.h
 * @brief Unified Protection Classification API
 * 
 * Provides a unified interface for detecting and classifying copy protection
 * schemes across all supported platforms (Amiga, C64, Apple II, Atari ST, PC).
 * 
 * This API integrates:
 * - CopyLock (Amiga)
 * - Speedlock (Amiga)
 * - Longtrack variants (Amiga)
 * - C64 protections (V-MAX, RapidLok, etc.)
 * - Apple II protections
 * - PC protections
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_PROTECTION_CLASSIFY_H
#define UFT_PROTECTION_CLASSIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

/* Include specific protection headers */
#include "uft_copylock.h"
#include "uft_speedlock.h"
#include "uft_longtrack.h"

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum number of protections detected on single disk */
#define UFT_PROTECT_MAX_DETECTIONS      16

/** Maximum protection name length */
#define UFT_PROTECT_NAME_LEN            64

/** Maximum detail string length */
#define UFT_PROTECT_DETAIL_LEN          256

/*===========================================================================
 * Platform Enumeration
 *===========================================================================*/

/**
 * @brief Target platform for protection detection
 */
typedef enum {
    UFT_PLATFORM_UNKNOWN = 0,
    UFT_PLATFORM_AMIGA,            /**< Commodore Amiga */
    UFT_PLATFORM_C64,              /**< Commodore 64/128 */
    UFT_PLATFORM_APPLE2,           /**< Apple II series */
    UFT_PLATFORM_ATARI_ST,         /**< Atari ST/STE */
    UFT_PLATFORM_ATARI_8BIT,       /**< Atari 400/800/XL/XE */
    UFT_PLATFORM_PC,               /**< IBM PC compatible */
    UFT_PLATFORM_BBC,              /**< BBC Micro */
    UFT_PLATFORM_MSX,              /**< MSX */
    UFT_PLATFORM_SPECTRUM,         /**< ZX Spectrum */
    UFT_PLATFORM_CPC,              /**< Amstrad CPC */
    UFT_PLATFORM_AUTO              /**< Auto-detect platform */
} uft_platform_t;

/*===========================================================================
 * Protection Category Enumeration
 *===========================================================================*/

/**
 * @brief Protection category/technique
 */
typedef enum {
    UFT_PCAT_NONE = 0,
    
    /* Timing-based */
    UFT_PCAT_VARIABLE_DENSITY,     /**< Variable bitcell timing */
    UFT_PCAT_TIMING_SENSITIVE,     /**< Requires precise timing */
    
    /* Track-based */
    UFT_PCAT_LONGTRACK,            /**< Extra-long track */
    UFT_PCAT_SHORTTRACK,           /**< Shortened track */
    UFT_PCAT_HALFTRACK,            /**< Half-track data */
    UFT_PCAT_EXTRA_TRACKS,         /**< Tracks beyond standard */
    
    /* Data-based */
    UFT_PCAT_LFSR_ENCODED,         /**< LFSR-generated data */
    UFT_PCAT_ENCRYPTED,            /**< Encrypted sectors */
    UFT_PCAT_SIGNATURE,            /**< Signature-based */
    
    /* Sync/Format */
    UFT_PCAT_CUSTOM_SYNC,          /**< Non-standard sync marks */
    UFT_PCAT_CUSTOM_FORMAT,        /**< Non-standard sector format */
    UFT_PCAT_INVALID_DATA,         /**< Intentionally invalid data */
    
    /* Weak bits */
    UFT_PCAT_WEAK_BITS,            /**< Weak/fuzzy bits */
    UFT_PCAT_NO_FLUX,              /**< No flux transitions */
    
    /* GCR-specific */
    UFT_PCAT_GCR_TIMING,           /**< GCR timing variations */
    UFT_PCAT_GCR_INVALID,          /**< Invalid GCR values */
    UFT_PCAT_FAT_TRACK,            /**< Fat/wide track */
    
    /* Composite */
    UFT_PCAT_MULTI_TECHNIQUE       /**< Multiple techniques combined */
} uft_protection_category_t;

/*===========================================================================
 * Protection Type Enumeration (Specific Schemes)
 *===========================================================================*/

/**
 * @brief Specific protection scheme identifier
 */
typedef enum {
    UFT_PROT_UNKNOWN = 0,
    
    /* === Amiga Protections === */
    UFT_PROT_COPYLOCK,             /**< Rob Northen CopyLock */
    UFT_PROT_COPYLOCK_OLD,         /**< Old CopyLock variant */
    UFT_PROT_SPEEDLOCK,            /**< Speedlock variable-density */
    UFT_PROT_LONGTRACK_PROTEC,     /**< PROTEC longtrack */
    UFT_PROT_LONGTRACK_PROTOSCAN,  /**< Protoscan (Lotus) */
    UFT_PROT_LONGTRACK_TIERTEX,    /**< Tiertex (Strider II) */
    UFT_PROT_LONGTRACK_SILMARILS,  /**< Silmarils */
    UFT_PROT_LONGTRACK_INFOGRAMES, /**< Infogrames */
    UFT_PROT_LONGTRACK_PROLANCE,   /**< Prolance (B.A.T.) */
    UFT_PROT_LONGTRACK_APP,        /**< Amiga Power Pack */
    UFT_PROT_LONGTRACK_SEVENCITIES,/**< Seven Cities of Gold */
    UFT_PROT_LONGTRACK_SMB_GCR,    /**< Super Methane Bros GCR */
    
    /* === C64 Protections === */
    UFT_PROT_VMAX_V1,              /**< V-MAX! v1 */
    UFT_PROT_VMAX_V2,              /**< V-MAX! v2 */
    UFT_PROT_VMAX_V3,              /**< V-MAX! v3 */
    UFT_PROT_RAPIDLOK_V1,          /**< RapidLok v1 */
    UFT_PROT_RAPIDLOK_V2,          /**< RapidLok v2 */
    UFT_PROT_RAPIDLOK_V3,          /**< RapidLok v3 */
    UFT_PROT_RAPIDLOK_V4,          /**< RapidLok v4 */
    UFT_PROT_VORPAL,               /**< Vorpal */
    UFT_PROT_PIRATESLAYER,         /**< PirateSlayer */
    UFT_PROT_TIMELOAD,             /**< Timeload */
    UFT_PROT_FAT_TRACK,            /**< Fat Track protection */
    
    /* === Apple II Protections === */
    UFT_PROT_APPLE_SPIRALDOS,      /**< Spiral DOS */
    UFT_PROT_APPLE_NIBBLE_COUNT,   /**< Nibble count */
    UFT_PROT_APPLE_HALFTRACK,      /**< Half-track */
    UFT_PROT_APPLE_TIMING,         /**< Timing-based */
    
    /* === Atari ST Protections === */
    UFT_PROT_COPYLOCK_ST,          /**< CopyLock ST variant */
    UFT_PROT_MACRODOS,             /**< MacroDOS */
    UFT_PROT_FUZZY_BITS,           /**< Fuzzy bits */
    
    /* === PC Protections === */
    UFT_PROT_WEAK_SECTOR,          /**< Weak sector */
    UFT_PROT_LONG_SECTOR,          /**< Extra-long sector */
    UFT_PROT_DUPLICATE_SECTOR,     /**< Duplicate sector IDs */
    
    /* Sentinel */
    UFT_PROT_TYPE_COUNT
} uft_protection_type_t;

/*===========================================================================
 * Confidence Level
 *===========================================================================*/

/**
 * @brief Detection confidence level
 */
typedef enum {
    UFT_PCONF_NONE = 0,            /**< Not detected */
    UFT_PCONF_POSSIBLE = 25,       /**< Some indicators found */
    UFT_PCONF_LIKELY = 50,         /**< Multiple indicators */
    UFT_PCONF_PROBABLE = 75,       /**< Strong indicators */
    UFT_PCONF_CERTAIN = 100        /**< Definitive detection */
} uft_protection_confidence_t;

/*===========================================================================
 * Protection Detection Result
 *===========================================================================*/

/**
 * @brief Single protection detection result
 */
typedef struct {
    /* Type identification */
    uft_protection_type_t     type;
    uft_protection_category_t category;
    uft_platform_t            platform;
    
    /* Confidence */
    uft_protection_confidence_t confidence;
    uint8_t                   confidence_pct;   /**< 0-100 percentage */
    
    /* Location */
    uint8_t                   track;
    uint8_t                   head;
    uint32_t                  bit_offset;       /**< Where detected in track */
    
    /* Names */
    char                      name[UFT_PROTECT_NAME_LEN];
    char                      variant[UFT_PROTECT_NAME_LEN];
    char                      detail[UFT_PROTECT_DETAIL_LEN];
    
    /* Protection-specific data */
    union {
        uft_copylock_result_t   copylock;
        uft_speedlock_result_t  speedlock;
        uft_longtrack_result_t  longtrack;
        uint8_t                 raw_data[512];
    } data;
    bool                      has_data;
    
    /* Preservation info */
    bool                      requires_timing;  /**< Needs timing data to preserve */
    bool                      requires_flux;    /**< Needs flux data to preserve */
    bool                      reconstructable;  /**< Can be reconstructed from seed */
    uint32_t                  seed;             /**< LFSR seed if reconstructable */
} uft_protection_detection_t;

/**
 * @brief Complete protection analysis result
 */
typedef struct {
    /* Platform detection */
    uft_platform_t            detected_platform;
    uft_platform_t            requested_platform;
    
    /* Detection results */
    uint8_t                   detection_count;
    uft_protection_detection_t detections[UFT_PROTECT_MAX_DETECTIONS];
    
    /* Primary detection (highest confidence) */
    uft_protection_detection_t *primary;
    
    /* Overall assessment */
    bool                      is_protected;
    bool                      is_standard;      /**< Standard format, no protection */
    bool                      all_reconstructable;
    
    /* Statistics */
    uint8_t                   tracks_analyzed;
    uint8_t                   tracks_protected;
    uint32_t                  analysis_time_ms;
    
    /* Report */
    char                      summary[UFT_PROTECT_DETAIL_LEN];
} uft_protection_analysis_t;

/*===========================================================================
 * Detection Context
 *===========================================================================*/

/**
 * @brief Detection context with options
 */
typedef struct {
    /* Input options */
    uft_platform_t            platform;         /**< Target platform (AUTO to detect) */
    bool                      quick_scan;       /**< Fast scan, may miss some */
    bool                      deep_scan;        /**< Thorough scan, slower */
    uint8_t                   start_track;      /**< First track to analyze */
    uint8_t                   end_track;        /**< Last track (0 = all) */
    
    /* Detection options */
    bool                      detect_timing;    /**< Detect timing-based protections */
    bool                      detect_weak_bits; /**< Detect weak bit protections */
    bool                      detect_longtrack; /**< Detect longtrack protections */
    bool                      detect_gcr;       /**< Detect GCR-based protections */
    
    /* Output options */
    bool                      include_raw_data; /**< Include raw detection data */
    bool                      verbose;          /**< Verbose detection messages */
    
    /* Callbacks */
    void (*progress_cb)(uint8_t track, uint8_t head, void *user);
    void (*detection_cb)(const uft_protection_detection_t *det, void *user);
    void *user_data;
} uft_protection_context_t;

/*===========================================================================
 * Core Detection Functions
 *===========================================================================*/

/**
 * @brief Initialize default detection context
 * 
 * @param ctx Context to initialize
 */
void uft_protect_init_context(uft_protection_context_t *ctx);

/**
 * @brief Analyze single track for protection
 * 
 * @param track_data Raw track data
 * @param track_bits Number of bits in track
 * @param timing_data Per-bit timing (NULL if unavailable)
 * @param track Track number
 * @param head Head number
 * @param ctx Detection context
 * @param result Output: detection result
 * @return Number of protections detected
 */
int uft_protect_analyze_track(const uint8_t *track_data,
                               uint32_t track_bits,
                               const uint16_t *timing_data,
                               uint8_t track,
                               uint8_t head,
                               const uft_protection_context_t *ctx,
                               uft_protection_analysis_t *result);

/**
 * @brief Analyze complete disk image for protection
 * 
 * @param image_path Path to disk image file
 * @param ctx Detection context
 * @param result Output: analysis result
 * @return 0 on success
 */
int uft_protect_analyze_image(const char *image_path,
                               const uft_protection_context_t *ctx,
                               uft_protection_analysis_t *result);

/**
 * @brief Quick check for any protection on track
 * 
 * Fast screening without detailed analysis.
 * 
 * @param track_data Raw track data
 * @param track_bits Number of bits
 * @param platform Target platform
 * @return true if protection likely present
 */
bool uft_protect_quick_check(const uint8_t *track_data,
                              uint32_t track_bits,
                              uft_platform_t platform);

/*===========================================================================
 * Platform-Specific Detection
 *===========================================================================*/

/**
 * @brief Detect Amiga protections
 */
int uft_protect_detect_amiga(const uint8_t *track_data,
                              uint32_t track_bits,
                              const uint16_t *timing_data,
                              uint8_t track,
                              uint8_t head,
                              uft_protection_analysis_t *result);

/**
 * @brief Detect C64 protections
 */
int uft_protect_detect_c64(const uint8_t *track_data,
                            uint32_t track_bits,
                            const uint16_t *timing_data,
                            uint8_t track,
                            uint8_t head,
                            uft_protection_analysis_t *result);

/**
 * @brief Detect Apple II protections
 */
int uft_protect_detect_apple2(const uint8_t *track_data,
                               uint32_t track_bits,
                               uint8_t track,
                               uint8_t head,
                               uft_protection_analysis_t *result);

/**
 * @brief Detect Atari ST protections
 */
int uft_protect_detect_atari_st(const uint8_t *track_data,
                                 uint32_t track_bits,
                                 const uint16_t *timing_data,
                                 uint8_t track,
                                 uint8_t head,
                                 uft_protection_analysis_t *result);

/**
 * @brief Auto-detect platform from track data
 */
uft_platform_t uft_protect_detect_platform(const uint8_t *track_data,
                                            uint32_t track_bits);

/*===========================================================================
 * Classification Functions
 *===========================================================================*/

/**
 * @brief Classify protection by category
 * 
 * @param type Protection type
 * @return Category
 */
uft_protection_category_t uft_protect_get_category(uft_protection_type_t type);

/**
 * @brief Get platform for protection type
 * 
 * @param type Protection type
 * @return Platform
 */
uft_platform_t uft_protect_get_platform(uft_protection_type_t type);

/**
 * @brief Check if protection requires timing data
 * 
 * @param type Protection type
 * @return true if timing data needed for preservation
 */
bool uft_protect_requires_timing(uft_protection_type_t type);

/**
 * @brief Check if protection can be reconstructed from seed
 * 
 * @param type Protection type
 * @return true if reconstructable
 */
bool uft_protect_is_reconstructable(uft_protection_type_t type);

/*===========================================================================
 * Name/String Functions
 *===========================================================================*/

/**
 * @brief Get protection type name
 */
const char* uft_protect_type_name(uft_protection_type_t type);

/**
 * @brief Get protection category name
 */
const char* uft_protect_category_name(uft_protection_category_t cat);

/**
 * @brief Get platform name
 */
const char* uft_protect_platform_name(uft_platform_t platform);

/**
 * @brief Get confidence level name
 */
const char* uft_protect_confidence_name(uft_protection_confidence_t conf);

/*===========================================================================
 * Reporting Functions
 *===========================================================================*/

/**
 * @brief Generate protection analysis report
 * 
 * @param result Analysis result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_protect_report(const uft_protection_analysis_t *result,
                           char *buffer, size_t buffer_size);

/**
 * @brief Generate single detection report
 * 
 * @param det Detection result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_protect_detection_report(const uft_protection_detection_t *det,
                                     char *buffer, size_t buffer_size);

/**
 * @brief Export analysis to JSON
 * 
 * @param result Analysis result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_protect_export_json(const uft_protection_analysis_t *result,
                                char *buffer, size_t buffer_size);

/**
 * @brief Free analysis result resources
 * 
 * @param result Result to free
 */
void uft_protect_free_result(uft_protection_analysis_t *result);

/*===========================================================================
 * Protection Database
 *===========================================================================*/

/**
 * @brief Protection database entry
 */
typedef struct {
    uft_protection_type_t     type;
    const char               *name;
    const char               *publisher;        /**< Protection publisher */
    const char               *description;
    uft_protection_category_t category;
    uft_platform_t            platform;
    uint16_t                  year_introduced;
    bool                      requires_timing;
    bool                      requires_flux;
    bool                      reconstructable;
} uft_protection_db_entry_t;

/**
 * @brief Get protection database entry
 * 
 * @param type Protection type
 * @return Database entry, or NULL if unknown
 */
const uft_protection_db_entry_t* uft_protect_get_db_entry(uft_protection_type_t type);

/**
 * @brief Get all protections for platform
 * 
 * @param platform Target platform
 * @param entries Output array
 * @param max_entries Maximum entries
 * @return Number of entries found
 */
int uft_protect_get_platform_protections(uft_platform_t platform,
                                          const uft_protection_db_entry_t **entries,
                                          int max_entries);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_CLASSIFY_H */

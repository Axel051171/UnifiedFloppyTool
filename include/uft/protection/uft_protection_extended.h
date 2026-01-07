/**
 * @file uft_protection_extended.h
 * @brief Extended Copy Protection Detection
 * 
 * Based on DTC learnings - adds detection for:
 * - Teque (UK publisher)
 * - TDP (The Disk Protector)
 * - Big Five (publisher protection)
 * - OziSoft (Australian publisher)
 * - PirateBusters v1.0/v2.0
 * - PirateSlayer
 * 
 * CLEAN-ROOM implementation based on observable patterns.
 */

#ifndef UFT_PROTECTION_EXTENDED_H
#define UFT_PROTECTION_EXTENDED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Protection Type IDs
 * ============================================================================ */

typedef enum {
    /* Standard protections (already implemented) */
    UFT_PROT_NONE = 0,
    UFT_PROT_MICROPROSE = 1,
    UFT_PROT_RAPIDLOK = 2,
    UFT_PROT_RAPIDLOK_KEY = 3,
    UFT_PROT_DATASOFT = 4,
    UFT_PROT_VORPAL = 5,
    UFT_PROT_VORPAL2 = 6,
    UFT_PROT_VMAX = 7,
    UFT_PROT_CYAN_A = 8,
    UFT_PROT_CYAN_B = 9,
    
    /* Extended protections (new) */
    UFT_PROT_TEQUE = 20,
    UFT_PROT_TDP = 21,
    UFT_PROT_BIGFIVE = 22,
    UFT_PROT_OZISOFT = 23,
    UFT_PROT_PIRATEBUSTERS_1 = 24,
    UFT_PROT_PIRATEBUSTERS_2A = 25,
    UFT_PROT_PIRATEBUSTERS_2B = 26,
    UFT_PROT_PIRATESLAYER = 27,
    
    /* Additional protections */
    UFT_PROT_RAINBOW_ARTS = 30,
    UFT_PROT_EA = 31,
    UFT_PROT_EPYX = 32,
    UFT_PROT_GREMLIN = 33,
    UFT_PROT_MARTECH = 34,
    UFT_PROT_MASTERTRONIC = 35,
    UFT_PROT_OCEAN = 36,
    UFT_PROT_US_GOLD = 37,
    
    UFT_PROT_UNKNOWN = 0xFF
} uft_protection_id_t;

/* ============================================================================
 * Protection Characteristics
 * ============================================================================ */

/** Protection detection flags */
#define UFT_PROT_FLAG_WEAK_BITS      0x0001  /**< Uses weak/random bits */
#define UFT_PROT_FLAG_TIMING         0x0002  /**< Timing-dependent */
#define UFT_PROT_FLAG_NON_STANDARD   0x0004  /**< Non-standard sector layout */
#define UFT_PROT_FLAG_TRACK_SYNC     0x0008  /**< Track sync manipulation */
#define UFT_PROT_FLAG_DENSITY        0x0010  /**< Density manipulation */
#define UFT_PROT_FLAG_HALF_TRACK     0x0020  /**< Half-track data */
#define UFT_PROT_FLAG_LONG_SECTOR    0x0040  /**< Oversized sectors */
#define UFT_PROT_FLAG_SHORT_SECTOR   0x0080  /**< Undersized sectors */
#define UFT_PROT_FLAG_EXTRA_SECTOR   0x0100  /**< More sectors than normal */
#define UFT_PROT_FLAG_MISSING_SECTOR 0x0200  /**< Missing sectors */
#define UFT_PROT_FLAG_BAD_GCR        0x0400  /**< Invalid GCR encoding */
#define UFT_PROT_FLAG_HEADER_MOD     0x0800  /**< Modified sector headers */

/* ============================================================================
 * Data Types
 * ============================================================================ */

/**
 * @brief Protection signature pattern
 */
typedef struct {
    uint8_t  track;           /**< Track where pattern appears */
    uint8_t  sector;          /**< Sector number (0xFF = any) */
    uint16_t offset;          /**< Offset within sector */
    uint8_t  pattern[16];     /**< Pattern bytes */
    uint8_t  mask[16];        /**< Mask for pattern matching */
    uint8_t  pattern_len;     /**< Pattern length */
} uft_prot_signature_t;

/**
 * @brief Protection descriptor
 */
typedef struct {
    uft_protection_id_t id;
    const char *name;
    const char *publisher;
    const char *description;
    
    uint16_t flags;           /**< Protection characteristics */
    
    /* Detection hints */
    uint8_t  typical_track;   /**< Track where protection usually appears */
    uint8_t  track_range_start;
    uint8_t  track_range_end;
    
    /* Signatures */
    const uft_prot_signature_t *signatures;
    size_t signature_count;
    
    /* Weak bit info */
    uint8_t  weak_bit_track;
    uint8_t  weak_bit_sector;
    uint16_t weak_bit_offset;
    uint16_t weak_bit_length;
    
} uft_protection_info_t;

/**
 * @brief Detection result for single protection
 */
typedef struct {
    uft_protection_id_t id;
    uint8_t  confidence;       /**< 0-100% */
    uint8_t  track_found;      /**< Track where detected */
    uint8_t  sector_found;     /**< Sector where detected */
    uint16_t flags_detected;   /**< Which characteristics found */
    char     details[256];     /**< Human-readable details */
} uft_protection_result_t;

/**
 * @brief Full disk protection analysis
 */
typedef struct {
    uft_protection_result_t protections[8];  /**< Up to 8 protections */
    size_t protection_count;
    
    bool has_weak_bits;
    bool has_timing_protection;
    bool has_density_variation;
    
    uint8_t overall_confidence;
    char summary[512];
} uft_protection_analysis_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Get protection info by ID
 */
const uft_protection_info_t* uft_protection_get_info(uft_protection_id_t id);

/**
 * @brief Get protection info by name
 */
const uft_protection_info_t* uft_protection_get_by_name(const char *name);

/**
 * @brief Get all known protections
 */
size_t uft_protection_get_all(
    const uft_protection_info_t **infos,
    size_t max
);

/**
 * @brief Detect protection in track data
 * 
 * @param track_data      GCR-encoded track data
 * @param track_size      Size of track data
 * @param track_num       Track number
 * @param result          Output result
 * @return 0 on success
 */
int uft_protection_detect_track(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/**
 * @brief Analyze entire disk for protections
 * 
 * @param get_track       Callback to get track data
 * @param user_data       User data for callback
 * @param analysis        Output analysis
 * @return 0 on success
 */
typedef const uint8_t* (*uft_get_track_fn)(uint8_t track, size_t *size, void *user_data);

int uft_protection_analyze_disk(
    uft_get_track_fn get_track,
    void *user_data,
    uft_protection_analysis_t *analysis
);

/**
 * @brief Detect weak bits in track
 * 
 * @param track_data_rev1   First revolution data
 * @param track_data_rev2   Second revolution data
 * @param track_size        Size of track data
 * @param weak_positions    Output array of weak bit positions
 * @param max_positions     Maximum positions to return
 * @return Number of weak bits found
 */
size_t uft_protection_detect_weak_bits(
    const uint8_t *track_data_rev1,
    const uint8_t *track_data_rev2,
    size_t track_size,
    uint16_t *weak_positions,
    size_t max_positions
);

/* ============================================================================
 * Teque Protection
 * ============================================================================ */

/**
 * @brief Detect Teque protection
 * 
 * Teque (UK) protection characteristics:
 * - Modified header checksums
 * - Extra sectors on specific tracks
 * - Non-standard sync patterns
 */
int uft_protection_detect_teque(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/* ============================================================================
 * TDP (The Disk Protector)
 * ============================================================================ */

/**
 * @brief Detect TDP protection
 * 
 * TDP characteristics:
 * - Track 36+ data
 * - Modified sector headers
 * - Specific loader signature
 */
int uft_protection_detect_tdp(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/* ============================================================================
 * Big Five Protection
 * ============================================================================ */

/**
 * @brief Detect Big Five protection
 * 
 * Big Five characteristics:
 * - Custom GCR encoding on track 18
 * - Modified directory track
 * - Specific signature bytes
 */
int uft_protection_detect_bigfive(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/* ============================================================================
 * OziSoft Protection
 * ============================================================================ */

/**
 * @brief Detect OziSoft protection
 * 
 * OziSoft (Australian) characteristics:
 * - Density variation on outer tracks
 * - Custom loader
 * - Specific track patterns
 */
int uft_protection_detect_ozisoft(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/* ============================================================================
 * PirateBusters Protection
 * ============================================================================ */

/**
 * @brief Detect PirateBusters v1.0
 */
int uft_protection_detect_piratebusters_v1(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/**
 * @brief Detect PirateBusters v2.0 Track A
 */
int uft_protection_detect_piratebusters_v2a(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/**
 * @brief Detect PirateBusters v2.0 Track B
 */
int uft_protection_detect_piratebusters_v2b(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/* ============================================================================
 * PirateSlayer Protection
 * ============================================================================ */

/**
 * @brief Detect PirateSlayer protection
 */
int uft_protection_detect_pirateslayer(
    const uint8_t *track_data,
    size_t track_size,
    uint8_t track_num,
    uft_protection_result_t *result
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get protection name
 */
const char* uft_protection_name(uft_protection_id_t id);

/**
 * @brief Get copy strategy for protection
 */
typedef enum {
    UFT_COPY_STANDARD,
    UFT_COPY_WITH_ERRORS,
    UFT_COPY_MULTI_REV,
    UFT_COPY_FLUX_LEVEL,
    UFT_COPY_IMPOSSIBLE
} uft_copy_strategy_t;

uft_copy_strategy_t uft_protection_get_copy_strategy(uft_protection_id_t id);

/**
 * @brief Export analysis to JSON
 */
size_t uft_protection_analysis_to_json(
    const uft_protection_analysis_t *analysis,
    char *buffer,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_EXTENDED_H */

/**
 * @file uft_c64_protection_ext.h
 * @brief Extended C64 Copy Protection Detection
 * 
 * Additional protection schemes beyond the core set:
 * - TimeWarp, Densitron, Kracker Jax
 * - Formaster, Microforte, Rainbow Arts
 * - Track-based protections and signature detection
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_C64_PROTECTION_EXT_H
#define UFT_C64_PROTECTION_EXT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Protection Type Definitions
 * ============================================================================ */

/**
 * @brief Extended protection types
 */
typedef enum {
    C64_PROT_EXT_NONE               = 0,
    
    /* Track-based protections */
    C64_PROT_EXT_TIMEWARP           = 0x0100,   /**< TimeWarp protection */
    C64_PROT_EXT_DENSITRON          = 0x0101,   /**< Densitron (density-based) */
    C64_PROT_EXT_KRACKER_JAX        = 0x0102,   /**< Kracker Jax */
    C64_PROT_EXT_FORMASTER          = 0x0103,   /**< Formaster */
    C64_PROT_EXT_MICROFORTE         = 0x0104,   /**< Microforte */
    C64_PROT_EXT_RAINBOW_ARTS       = 0x0105,   /**< Rainbow Arts */
    
    /* Sector-based protections */
    C64_PROT_EXT_GMA                = 0x0200,   /**< GMA (Game Maker's Archive) */
    C64_PROT_EXT_ABACUS             = 0x0201,   /**< Abacus */
    C64_PROT_EXT_BUBBLE_BURST       = 0x0202,   /**< Bubble Burst */
    C64_PROT_EXT_TRILOGIC           = 0x0203,   /**< Trilogic */
    
    /* Loader-based protections */
    C64_PROT_EXT_TURBO_TAPE         = 0x0300,   /**< Turbo Tape variants */
    C64_PROT_EXT_PAVLODA            = 0x0301,   /**< Pavloda */
    C64_PROT_EXT_FLASHLOAD          = 0x0302,   /**< Flashload */
    C64_PROT_EXT_HYPRA_LOAD         = 0x0303,   /**< Hypra Load */
    
    /* Publisher-specific */
    C64_PROT_EXT_OCEAN              = 0x0400,   /**< Ocean Software */
    C64_PROT_EXT_US_GOLD            = 0x0401,   /**< US Gold */
    C64_PROT_EXT_MASTERTRONIC       = 0x0402,   /**< Mastertronic */
    C64_PROT_EXT_CODEMASTERS        = 0x0403,   /**< Codemasters */
    C64_PROT_EXT_ACTIVISION         = 0x0404,   /**< Activision */
    C64_PROT_EXT_EPYX               = 0x0405,   /**< Epyx */
    
    /* Hardware-based */
    C64_PROT_EXT_FREEZE_FRAME       = 0x0500,   /**< Freeze Frame detection */
    C64_PROT_EXT_FAST_HACK_EM       = 0x0501,   /**< Fast Hack'em */
    
    /* Misc */
    C64_PROT_EXT_FAT_TRACK          = 0x0600,   /**< Fat track protection */
    C64_PROT_EXT_SYNC_MARK          = 0x0601,   /**< Custom sync marks */
    C64_PROT_EXT_GAP_LENGTH         = 0x0602,   /**< Gap length variation */
    C64_PROT_EXT_DENSITY_KEY        = 0x0603,   /**< Density key track */
} c64_prot_ext_type_t;

/**
 * @brief TimeWarp detection result
 */
typedef struct {
    bool        detected;           /**< TimeWarp detected */
    int         version;            /**< Version (1-3) */
    int         key_track;          /**< Key track number */
    uint8_t     signature[8];       /**< Signature bytes */
    char        description[64];    /**< Description */
} c64_timewarp_result_t;

/**
 * @brief Densitron detection result
 */
typedef struct {
    bool        detected;           /**< Densitron detected */
    int         key_tracks[4];      /**< Key track numbers */
    int         num_key_tracks;     /**< Number of key tracks */
    uint8_t     density_pattern[4]; /**< Expected densities */
    char        description[64];    /**< Description */
} c64_densitron_result_t;

/**
 * @brief Kracker Jax detection result
 */
typedef struct {
    bool        detected;           /**< Kracker Jax detected */
    int         volume;             /**< Volume number */
    int         issue;              /**< Issue number */
    uint8_t     signature[16];      /**< Signature bytes */
    char        description[64];    /**< Description */
} c64_kracker_jax_result_t;

/**
 * @brief Generic protection detection result
 */
typedef struct {
    c64_prot_ext_type_t type;       /**< Protection type */
    bool        detected;           /**< Protection detected */
    int         confidence;         /**< Confidence (0-100) */
    int         track;              /**< Detection track */
    int         sector;             /**< Detection sector (-1 if N/A) */
    uint8_t     signature[32];      /**< Signature bytes */
    size_t      signature_len;      /**< Signature length */
    char        name[32];           /**< Protection name */
    char        description[128];   /**< Description */
} c64_prot_ext_result_t;

/**
 * @brief Extended protection scan result
 */
typedef struct {
    int                     num_found;          /**< Number found */
    c64_prot_ext_result_t   results[16];        /**< Detection results */
    char                    summary[256];       /**< Summary text */
} c64_prot_ext_scan_t;

/* ============================================================================
 * Protection Signatures
 * ============================================================================ */

/** TimeWarp signature bytes */
#define TIMEWARP_SIG_V1         { 0xA9, 0x00, 0x85, 0x02, 0xA9, 0x36 }
#define TIMEWARP_SIG_V2         { 0xA9, 0x00, 0x8D, 0x00, 0xDD, 0xA9 }
#define TIMEWARP_SIG_V3         { 0x78, 0xA9, 0x7F, 0x8D, 0x0D, 0xDC }

/** Densitron key track pattern */
#define DENSITRON_PATTERN       { 0x3, 0x2, 0x1, 0x0 }  /* Density gradient */

/** Kracker Jax loader signature */
#define KRACKER_JAX_SIG         { 0x4B, 0x52, 0x41, 0x43, 0x4B }  /* "KRACK" */

/** Formaster signature */
#define FORMASTER_SIG           { 0xEE, 0x00, 0x1C, 0xAD, 0x00, 0x1C }

/** Rainbow Arts signature */
#define RAINBOW_ARTS_SIG        { 0x52, 0x41, 0x49, 0x4E }  /* "RAIN" */

/* ============================================================================
 * API Functions - TimeWarp
 * ============================================================================ */

/**
 * @brief Detect TimeWarp protection
 * @param data Track/disk data
 * @param size Data size
 * @param result Output result
 * @return true if detected
 */
bool c64_detect_timewarp(const uint8_t *data, size_t size,
                         c64_timewarp_result_t *result);

/**
 * @brief Detect TimeWarp in GCR track
 * @param track_data GCR track data
 * @param track_len Track length
 * @param track Track number
 * @param result Output result
 * @return true if detected
 */
bool c64_detect_timewarp_track(const uint8_t *track_data, size_t track_len,
                               int track, c64_timewarp_result_t *result);

/* ============================================================================
 * API Functions - Densitron
 * ============================================================================ */

/**
 * @brief Detect Densitron protection
 * @param track_densities Density per track (84 entries for halftracks)
 * @param num_tracks Number of tracks
 * @param result Output result
 * @return true if detected
 */
bool c64_detect_densitron(const uint8_t *track_densities, int num_tracks,
                          c64_densitron_result_t *result);

/**
 * @brief Check if density pattern indicates Densitron
 * @param densities Array of 4 densities
 * @return true if matches Densitron pattern
 */
bool c64_is_densitron_pattern(const uint8_t *densities);

/* ============================================================================
 * API Functions - Kracker Jax
 * ============================================================================ */

/**
 * @brief Detect Kracker Jax
 * @param data Disk data
 * @param size Data size
 * @param result Output result
 * @return true if detected
 */
bool c64_detect_kracker_jax(const uint8_t *data, size_t size,
                            c64_kracker_jax_result_t *result);

/**
 * @brief Detect Kracker Jax in D64
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param result Output result
 * @return true if detected
 */
bool c64_detect_kracker_jax_d64(const uint8_t *d64_data, size_t d64_size,
                                c64_kracker_jax_result_t *result);

/* ============================================================================
 * API Functions - Generic Detection
 * ============================================================================ */

/**
 * @brief Detect specific protection type
 * @param type Protection type to detect
 * @param data Data to scan
 * @param size Data size
 * @param result Output result
 * @return true if detected
 */
bool c64_detect_protection_ext(c64_prot_ext_type_t type,
                               const uint8_t *data, size_t size,
                               c64_prot_ext_result_t *result);

/**
 * @brief Scan for all extended protections
 * @param data Data to scan
 * @param size Data size
 * @param scan Output scan results
 * @return Number of protections found
 */
int c64_scan_protections_ext(const uint8_t *data, size_t size,
                             c64_prot_ext_scan_t *scan);

/**
 * @brief Scan GCR tracks for protections
 * @param track_data Array of track data pointers
 * @param track_lengths Array of track lengths
 * @param track_densities Array of track densities
 * @param num_tracks Number of tracks
 * @param scan Output scan results
 * @return Number of protections found
 */
int c64_scan_gcr_protections(const uint8_t **track_data,
                             const size_t *track_lengths,
                             const uint8_t *track_densities,
                             int num_tracks,
                             c64_prot_ext_scan_t *scan);

/* ============================================================================
 * API Functions - Track Analysis
 * ============================================================================ */

/**
 * @brief Check for fat track
 * @param track_data Track data
 * @param track_len Track length
 * @param expected_capacity Expected capacity for density
 * @return true if fat track detected
 */
bool c64_is_fat_track(const uint8_t *track_data, size_t track_len,
                      size_t expected_capacity);

/**
 * @brief Check for custom sync marks
 * @param track_data Track data
 * @param track_len Track length
 * @param sync_byte Expected sync byte (usually 0xFF)
 * @return Number of non-standard syncs found
 */
int c64_check_custom_sync(const uint8_t *track_data, size_t track_len,
                          uint8_t sync_byte);

/**
 * @brief Analyze gap lengths in track
 * @param track_data Track data
 * @param track_len Track length
 * @param min_gap Output: minimum gap
 * @param max_gap Output: maximum gap
 * @param avg_gap Output: average gap
 * @return Number of gaps found
 */
int c64_analyze_gaps(const uint8_t *track_data, size_t track_len,
                     int *min_gap, int *max_gap, int *avg_gap);

/**
 * @brief Check for density key track
 * @param track_data Track data
 * @param track_len Track length
 * @param actual_density Actual measured density
 * @param expected_density Expected density for track
 * @return true if density mismatch indicates protection
 */
bool c64_is_density_key(const uint8_t *track_data, size_t track_len,
                        int actual_density, int expected_density);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get protection type name
 * @param type Protection type
 * @return Protection name string
 */
const char *c64_prot_ext_name(c64_prot_ext_type_t type);

/**
 * @brief Get protection category
 * @param type Protection type
 * @return Category string
 */
const char *c64_prot_ext_category(c64_prot_ext_type_t type);

/**
 * @brief Check if protection uses track anomalies
 * @param type Protection type
 * @return true if track-based
 */
bool c64_prot_ext_is_track_based(c64_prot_ext_type_t type);

/**
 * @brief Check if protection uses density variations
 * @param type Protection type
 * @return true if density-based
 */
bool c64_prot_ext_is_density_based(c64_prot_ext_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_PROTECTION_EXT_H */

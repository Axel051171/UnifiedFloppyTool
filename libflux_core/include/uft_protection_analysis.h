/**
 * @file uft_protection_analysis.h
 * @brief Copy Protection Analysis Framework (High-Fidelity Layer)
 * 
 * Implements Alcohol 120%/BlindWrite-style physical signature analysis
 * for floppy disk preservation.
 * 
 * Core Principles:
 * 1. SEPARATE logical data from physical signature
 * 2. DETECT protection schemes, don't bypass them
 * 3. PRESERVE all physical anomalies in flux profiles
 * 4. MEASURE exact timing/positioning (DPM)
 * 
 * Protection Schemes Supported:
 * - Rob Northen Copylock (Amiga) - Weak bits on track 0
 * - Speedlock (C64/Amiga) - Variable bitrate
 * - RapidLok (C64) - Track alignment timing
 * - Vortex Tracker (C64) - Bad sectors
 * - Dungeon Master (Atari ST) - Weak sectors
 * - SafeDisc-style DPM - Sector position measurement
 * 
 * @version 2.12.0
 * @date 2024-12-27
 */

#ifndef UFT_PROTECTION_ANALYSIS_H
#define UFT_PROTECTION_ANALYSIS_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Known protection schemes
 */
typedef enum {
    UFT_PROTECTION_NONE = 0,
    
    /* Weak bit based */
    UFT_PROTECTION_COPYLOCK,        /**< Rob Northen Copylock (Amiga) */
    UFT_PROTECTION_DUNGEON_MASTER,  /**< Dungeon Master (Atari ST) weak sectors */
    
    /* Timing based */
    UFT_PROTECTION_SPEEDLOCK,       /**< Speedlock (variable bitrate) */
    UFT_PROTECTION_RAPIDLOK,        /**< RapidLok (track alignment) */
    UFT_PROTECTION_DPM,             /**< DPM-style sector positioning */
    
    /* Track based */
    UFT_PROTECTION_LONG_TRACK,      /**< Oversized tracks */
    UFT_PROTECTION_HALF_TRACK,      /**< Between-track data */
    
    /* Sector based */
    UFT_PROTECTION_BAD_SECTORS,     /**< Intentional bad sectors */
    UFT_PROTECTION_VORTEX,          /**< Vortex Tracker (C64) */
    
    /* Gap based */
    UFT_PROTECTION_GAP_DATA,        /**< Hidden data in gaps */
    UFT_PROTECTION_SYNC_VIOLATION,  /**< Non-standard sync marks */
    
    UFT_PROTECTION_CUSTOM           /**< Unknown/custom */
} uft_protection_type_t;

/**
 * @brief DPM (Data Position Measurement) entry
 * 
 * Measures exact sector position on disk.
 * Deviation from expected position indicates protection.
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    uint8_t sector;
    
    uint32_t expected_bit_pos;      /**< Theoretical position */
    uint32_t actual_bit_pos;        /**< Measured position */
    int32_t deviation_ns;           /**< Timing deviation */
    
    bool is_anomaly;                /**< Significant deviation */
    uint8_t confidence;             /**< 0-100% */
    
} uft_dpm_entry_t;

/**
 * @brief DPM map (complete disk)
 */
typedef struct {
    uft_dpm_entry_t* entries;
    uint32_t entry_count;
    uint32_t anomalies_found;
    
    /* Statistics */
    int32_t min_deviation_ns;
    int32_t max_deviation_ns;
    int32_t avg_deviation_ns;
    
} uft_dpm_map_t;

/**
 * @brief Weak bit detection result
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    uint8_t sector;
    
    /* Multiple read results */
    uint8_t read_count;
    uint16_t crc_values[8];         /**< CRC from each read */
    bool crc_stable;                /**< All CRCs match */
    
    /* Bit-level instability */
    uint32_t unstable_bit_count;
    uint32_t* unstable_bit_positions;
    
    /* Classification */
    bool is_weak_sector;            /**< Intentional weak bits */
    bool is_media_error;            /**< Physical damage */
    uint8_t confidence;
    
} uft_weak_bit_result_t;

/**
 * @brief Gap analysis result
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    
    /* Gap data */
    uint32_t gap_count;
    struct {
        uint32_t start_bit;
        uint32_t length_bits;
        uint8_t* data;              /**< Non-standard data in gap */
        size_t data_size;
        bool has_hidden_data;
    } gaps[256];
    
    /* Sync mark analysis */
    uint32_t sync_violations;
    uint32_t missing_sync_marks;
    
} uft_gap_analysis_t;

/**
 * @brief Variable bitrate detection
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    
    /* Bitrate measurements */
    uint32_t nominal_bitrate;       /**< Expected */
    uint32_t min_bitrate;           /**< Minimum observed */
    uint32_t max_bitrate;           /**< Maximum observed */
    float bitrate_variance;         /**< Percentage */
    
    /* Zone detection */
    bool has_variable_bitrate;
    uint32_t zone_count;
    struct {
        uint32_t start_bit;
        uint32_t length_bits;
        uint32_t bitrate;
    } zones[16];
    
} uft_bitrate_analysis_t;

/**
 * @brief Complete protection analysis result
 */
typedef struct {
    /* Detected schemes */
    uft_protection_type_t protection_type;
    const char* protection_name;
    uint8_t confidence;             /**< 0-100% */
    
    /* Physical signatures */
    uft_dpm_map_t* dpm_map;
    uft_weak_bit_result_t* weak_bits;
    uint32_t weak_bit_count;
    uft_gap_analysis_t* gap_analysis;
    uint32_t gap_analysis_count;
    uft_bitrate_analysis_t* bitrate_analysis;
    uint32_t bitrate_analysis_count;
    
    /* Flux profile reference */
    char flux_profile_id[64];
    bool requires_flux_preservation;
    
    /* Human-readable report */
    char description[1024];
    
} uft_protection_analysis_t;

/**
 * @brief Protection analysis context
 */
typedef struct {
    /* Source */
    void* source_ctx;               /**< Format-specific context */
    
    /* Configuration */
    uint8_t dpm_precision_ns;       /**< DPM timing precision */
    uint8_t weak_bit_reads;         /**< Number of reads for weak bit detection */
    bool analyze_gaps;
    bool measure_bitrate;
    
    /* Results */
    uft_protection_analysis_t* analysis;
    
    /* Progress callback */
    void (*progress_fn)(uint8_t percent, const char* status, void* user_data);
    void* progress_user_data;
    
    /* Error context */
    uft_error_ctx_t error;
    
} uft_protection_ctx_t;

/* ========================================================================
 * DPM (Data Position Measurement)
 * ======================================================================== */

/**
 * @brief Create DPM context
 * 
 * @param[out] ctx Context pointer
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_dpm_create(uft_protection_ctx_t** ctx);

/**
 * @brief Measure sector positions across disk
 * 
 * Reads all sectors and measures exact bit positions.
 * Compares against theoretical positions to detect anomalies.
 * 
 * @param[in] ctx Protection context
 * @param[out] dpm_map Complete DPM map
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_dpm_measure_disk(
    uft_protection_ctx_t* ctx,
    uft_dpm_map_t** dpm_map
);

/**
 * @brief Measure single track DPM
 * 
 * @param[in] ctx Protection context
 * @param track Track number
 * @param head Head number
 * @param[out] entries DPM entries for track
 * @param[out] count Entry count
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_dpm_measure_track(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_dpm_entry_t** entries,
    uint32_t* count
);

/**
 * @brief Free DPM map
 * 
 * @param[in,out] dpm_map DPM map to free
 */
void uft_dpm_free(uft_dpm_map_t** dpm_map);

/* ========================================================================
 * WEAK BIT DETECTION
 * ======================================================================== */

/**
 * @brief Detect weak bits in sector
 * 
 * Reads sector multiple times and compares results.
 * Unstable bits indicate either:
 * - Intentional weak bits (protection)
 * - Media damage
 * 
 * @param[in] ctx Protection context
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @param[out] result Weak bit analysis
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_weak_bit_detect_sector(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_weak_bit_result_t* result
);

/**
 * @brief Scan entire disk for weak bits
 * 
 * @param[in] ctx Protection context
 * @param[out] results Array of weak bit results
 * @param[out] count Result count
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_weak_bit_scan_disk(
    uft_protection_ctx_t* ctx,
    uft_weak_bit_result_t** results,
    uint32_t* count
);

/* ========================================================================
 * GAP ANALYSIS
 * ======================================================================== */

/**
 * @brief Analyze gaps in track
 * 
 * Examines data between sectors for:
 * - Hidden data
 * - Sync mark violations
 * - Non-standard patterns
 * 
 * @param[in] ctx Protection context
 * @param track Track number
 * @param head Head number
 * @param[out] analysis Gap analysis result
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_gap_analyze_track(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_gap_analysis_t* analysis
);

/* ========================================================================
 * VARIABLE BITRATE DETECTION
 * ======================================================================== */

/**
 * @brief Analyze track bitrate
 * 
 * Measures bitrate across track to detect:
 * - Speedlock-style variable density
 * - Zone-based encoding
 * - Timing anomalies
 * 
 * @param[in] ctx Protection context
 * @param track Track number
 * @param head Head number
 * @param[out] analysis Bitrate analysis
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_bitrate_analyze_track(
    uft_protection_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_bitrate_analysis_t* analysis
);

/* ========================================================================
 * PROTECTION SCHEME DETECTION
 * ======================================================================== */

/**
 * @brief Analyze disk for protection schemes
 * 
 * Complete analysis combining:
 * - DPM
 * - Weak bits
 * - Gap analysis
 * - Bitrate analysis
 * 
 * Classifies protection scheme and generates flux profile.
 * 
 * @param[in] ctx Protection context
 * @param[out] analysis Complete analysis result
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_protection_analyze_disk(
    uft_protection_ctx_t* ctx,
    uft_protection_analysis_t** analysis
);

/**
 * @brief Get protection scheme name
 * 
 * @param type Protection type
 * @return Human-readable name
 */
const char* uft_protection_name(uft_protection_type_t type);

/**
 * @brief Free protection analysis
 * 
 * @param[in,out] analysis Analysis to free
 */
void uft_protection_analysis_free(uft_protection_analysis_t** analysis);

/**
 * @brief Destroy protection context
 * 
 * @param[in,out] ctx Context to destroy
 */
void uft_protection_destroy(uft_protection_ctx_t** ctx);

/* ========================================================================
 * FLUX PROFILE EXPORT
 * ======================================================================== */

/**
 * @brief Export analysis to flux profile
 * 
 * Generates flux profile containing:
 * - Timing anomalies
 * - Weak bit positions
 * - Gap data
 * - Bitrate zones
 * 
 * @param[in] analysis Protection analysis
 * @param[out] profile_path Path to save profile
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_protection_export_flux_profile(
    const uft_protection_analysis_t* analysis,
    const char* profile_path
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_ANALYSIS_H */

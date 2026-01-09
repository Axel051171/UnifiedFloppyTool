/**
 * @file uft_decoder_types.h
 * @brief Unified Decoder Type Definitions
 * 
 * Common types used across all decoder modules.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_DECODER_TYPES_H
#define UFT_DECODER_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * ENCODING TYPES
 *============================================================================*/

/**
 * @brief Encoding type enumeration
 */
typedef enum {
    UFT_ENCODING_UNKNOWN = 0,
    UFT_ENCODING_FM,            /**< FM (Frequency Modulation) */
    UFT_ENCODING_MFM,           /**< MFM (Modified FM) */
    UFT_ENCODING_M2FM,          /**< M2FM (Modified MFM) */
    UFT_ENCODING_GCR_COMMODORE, /**< Commodore GCR (4-5 encoding) */
    UFT_ENCODING_GCR_APPLE,     /**< Apple GCR (6-2 encoding) */
    UFT_ENCODING_GCR_VICTOR,    /**< Victor 9000 GCR */
    UFT_ENCODING_AMIGA,         /**< Amiga MFM with special sync */
    UFT_ENCODING_RAW,           /**< Raw flux (no encoding) */
} uft_encoding_t;

/*============================================================================
 * CONFIDENCE TRACKING
 *============================================================================*/

/**
 * @brief Per-bit confidence value (0.0 = no confidence, 1.0 = full confidence)
 */
typedef double uft_conf_t;

/**
 * @brief Confidence array for decoded bits
 */
typedef struct {
    uft_conf_t* values;         /**< Confidence values */
    size_t count;               /**< Number of values */
    double average;             /**< Average confidence */
    double minimum;             /**< Minimum confidence */
} uft_confidence_array_t;

/*============================================================================
 * SYNC DETECTION
 *============================================================================*/

/**
 * @brief Maximum sync candidates per track
 */
#define UFT_MAX_SYNC_CANDIDATES 256

/**
 * @brief Sync pattern candidate
 */
typedef struct {
    size_t bit_offset;          /**< Bit offset in stream */
    uint32_t pattern;           /**< Detected pattern */
    int hamming_distance;       /**< Distance from expected pattern */
    double confidence;          /**< Detection confidence */
    bool is_address_mark;       /**< True if address mark, false if data mark */
} uft_sync_candidate_t;

/*============================================================================
 * PRE-ANALYSIS
 *============================================================================*/

/**
 * @brief Pre-analysis result
 */
typedef struct {
    double cell_time_ns;        /**< Estimated cell time in nanoseconds */
    double cell_time_min_ns;    /**< Minimum observed cell time */
    double cell_time_max_ns;    /**< Maximum observed cell time */
    double rpm;                 /**< Estimated disk RPM */
    double index_to_index_ns;   /**< Time between index pulses */
    int anomaly_count;          /**< Number of timing anomalies */
    int spike_count;            /**< Short pulses (<0.5 cells) */
    int dropout_count;          /**< Long gaps (>max cells) */
    double quality_score;       /**< Overall quality (0.0-1.0) */
    uft_encoding_t detected_encoding; /**< Auto-detected encoding */
} uft_preanalysis_result_t;

/*============================================================================
 * PLL DECODE
 *============================================================================*/

/**
 * @brief PLL decode result for one revolution
 */
typedef struct {
    uint8_t* bits;              /**< Decoded bit stream */
    size_t bit_count;           /**< Number of bits */
    uft_conf_t* confidence;     /**< Per-bit confidence (optional) */
    uint8_t* weak_flags;        /**< Weak bit flags (optional) */
    double average_confidence;  /**< Average confidence */
    int weak_count;             /**< Number of weak bits */
    double phase_error_rms;     /**< RMS phase error */
} uft_pll_decode_result_t;

/*============================================================================
 * MULTI-REV FUSION
 *============================================================================*/

/**
 * @brief Fusion configuration
 */
typedef struct {
    double min_consensus;       /**< Minimum consensus threshold (0.0-1.0) */
    bool detect_weak_bits;      /**< Detect and mark weak bits */
    bool use_weighting;         /**< Use quality weighting */
    int max_revolutions;        /**< Maximum revolutions to process */
    double alignment_tolerance; /**< Alignment tolerance in bits */
} uft_fusion_config_t;

/**
 * @brief Fusion result
 */
typedef struct {
    uint8_t* bits;              /**< Fused bit stream */
    size_t bit_count;           /**< Number of bits */
    uft_conf_t* confidence;     /**< Per-bit confidence */
    uint8_t* weak_bits;         /**< Weak bit map */
    double average_confidence;  /**< Average confidence */
    int weak_count;             /**< Number of weak bits detected */
    int revolutions_used;       /**< Number of revolutions actually used */
} uft_fusion_result_t;

/*============================================================================
 * SECTOR DECODE
 *============================================================================*/

/**
 * @brief Sector decode result
 */
typedef struct {
    int cylinder;               /**< Cylinder/track number */
    int head;                   /**< Head/side number */
    int sector;                 /**< Sector number */
    int size_code;              /**< Size code (0=128, 1=256, 2=512, 3=1024) */
    size_t data_size;           /**< Actual data size in bytes */
    uint8_t* data;              /**< Sector data (caller frees) */
    uint16_t crc_stored;        /**< CRC from disk */
    uint16_t crc_calculated;    /**< CRC we calculated */
    bool crc_ok;                /**< true if CRCs match */
    bool corrected;             /**< true if error correction was applied */
    int corrections_count;      /**< Number of bit corrections */
    double confidence;          /**< Overall sector confidence */
    size_t bit_offset;          /**< Offset in bit stream where sector starts */
} uft_sector_decode_result_t;

/*============================================================================
 * TRACK DECODE
 *============================================================================*/

/**
 * @brief Maximum sectors per track
 */
#define UFT_MAX_SECTORS_PER_TRACK 64

/**
 * @brief Track decode result
 */
typedef struct {
    int cylinder;               /**< Cylinder number */
    int head;                   /**< Head number */
    uft_encoding_t encoding;    /**< Detected/used encoding */
    
    /* Sectors */
    uft_sector_decode_result_t sectors[UFT_MAX_SECTORS_PER_TRACK];
    int sector_count;           /**< Number of sectors found */
    int crc_ok_count;           /**< Sectors with good CRC */
    int crc_error_count;        /**< Sectors with CRC errors */
    int corrected_count;        /**< Sectors that were corrected */
    
    /* Quality metrics */
    double average_confidence;  /**< Average sector confidence */
    double quality_score;       /**< Overall track quality (0.0-1.0) */
    
    /* Raw data (optional) */
    uint8_t* raw_bits;          /**< Raw decoded bits (if kept) */
    size_t raw_bit_count;       /**< Number of raw bits */
} uft_track_decode_result_t;

/*============================================================================
 * DECODER SESSION
 *============================================================================*/

/**
 * @brief Forensic decoder session
 */
typedef struct {
    /* Configuration */
    void* config;               /**< Decoder configuration */
    
    /* Statistics */
    int tracks_processed;
    int sectors_decoded;
    int sectors_recovered;
    int total_corrections;
    
    /* Audit log */
    struct {
        char** entries;
        int count;
        int capacity;
    } audit;
} uft_forensic_decoder_session_t;

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get encoding name as string
 */
static inline const char* uft_encoding_name(uft_encoding_t enc) {
    switch (enc) {
        case UFT_ENCODING_FM:            return "FM";
        case UFT_ENCODING_MFM:           return "MFM";
        case UFT_ENCODING_M2FM:          return "M2FM";
        case UFT_ENCODING_GCR_COMMODORE: return "GCR-Commodore";
        case UFT_ENCODING_GCR_APPLE:     return "GCR-Apple";
        case UFT_ENCODING_GCR_VICTOR:    return "GCR-Victor";
        case UFT_ENCODING_AMIGA:         return "Amiga-MFM";
        case UFT_ENCODING_RAW:           return "Raw";
        default:                         return "Unknown";
    }
}

/**
 * @brief Free preanalysis result
 */
static inline void uft_preanalysis_result_free(uft_preanalysis_result_t* r) {
    (void)r; /* No dynamic allocations */
}

/**
 * @brief Free PLL decode result
 */
void uft_pll_decode_result_free(uft_pll_decode_result_t* r);

/**
 * @brief Free fusion result
 */
void uft_fusion_result_free(uft_fusion_result_t* r);

/**
 * @brief Free sector decode result
 */
void uft_sector_decode_result_free(uft_sector_decode_result_t* r);

/**
 * @brief Free track decode result
 */
void uft_track_decode_result_free(uft_track_decode_result_t* r);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODER_TYPES_H */

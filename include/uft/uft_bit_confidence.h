/**
 * @file uft_bit_confidence.h
 * @brief Per-Bit Confidence System
 * 
 * Implements P0-HW-001: Complete confidence chain from hardware to bits.
 * 
 * This module provides:
 * - Per-bit confidence with full provenance
 * - Multi-source confidence fusion
 * - Confidence propagation through decode pipeline
 * - Audit trail for every bit decision
 * 
 * Design principle: "Kein Bit verloren" - every bit has traceable confidence
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_BIT_CONFIDENCE_H
#define UFT_BIT_CONFIDENCE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Version identifier */
#define UFT_BITCONF_VERSION         0x010000  /* 1.0.0 */

/** Maximum revolutions for multi-read */
#define UFT_BITCONF_MAX_REVOLUTIONS 64

/** Maximum alternative interpretations */
#define UFT_BITCONF_MAX_ALTERNATIVES 8

/** Confidence thresholds */
#define UFT_BITCONF_CERTAIN         100  /**< 100% - definitive */
#define UFT_BITCONF_HIGH            90   /**< 90%+ - very reliable */
#define UFT_BITCONF_GOOD            75   /**< 75%+ - reliable */
#define UFT_BITCONF_MARGINAL        50   /**< 50%+ - marginal */
#define UFT_BITCONF_LOW             25   /**< 25%+ - unreliable */
#define UFT_BITCONF_NONE            0    /**< 0% - unknown/failed */

/** Confidence source flags */
#define UFT_CONFSRC_TIMING          0x0001  /**< From flux timing */
#define UFT_CONFSRC_AMPLITUDE       0x0002  /**< From signal amplitude */
#define UFT_CONFSRC_MULTIREV        0x0004  /**< From multi-revolution vote */
#define UFT_CONFSRC_PLL             0x0008  /**< From PLL lock quality */
#define UFT_CONFSRC_CRC             0x0010  /**< From CRC verification */
#define UFT_CONFSRC_CHECKSUM        0x0020  /**< From checksum verification */
#define UFT_CONFSRC_CONTEXT         0x0040  /**< From data context */
#define UFT_CONFSRC_PATTERN         0x0080  /**< From known pattern match */
#define UFT_CONFSRC_CORRECTION      0x0100  /**< From error correction */
#define UFT_CONFSRC_INFERRED        0x0200  /**< Inferred from neighbors */
#define UFT_CONFSRC_MANUAL          0x0400  /**< Manually specified */

/** Confidence flags */
#define UFT_CONFLAG_WEAK            0x0001  /**< Weak/fuzzy bit detected */
#define UFT_CONFLAG_UNSTABLE        0x0002  /**< Value varies between reads */
#define UFT_CONFLAG_CORRECTED       0x0004  /**< Value was error-corrected */
#define UFT_CONFLAG_INTERPOLATED    0x0008  /**< Value was interpolated */
#define UFT_CONFLAG_AMBIGUOUS       0x0010  /**< Multiple valid interpretations */
#define UFT_CONFLAG_PROTECTED       0x0020  /**< Part of copy protection */
#define UFT_CONFLAG_NO_FLUX         0x0040  /**< No flux transitions */
#define UFT_CONFLAG_TIMING_ANOMALY  0x0080  /**< Timing outside normal range */
#define UFT_CONFLAG_PLL_SLIP        0x0100  /**< PLL slipped during decode */
#define UFT_CONFLAG_BOUNDARY        0x0200  /**< At sector/track boundary */

/*===========================================================================
 * Core Types
 *===========================================================================*/

/**
 * @brief Confidence source record
 * 
 * Records where a confidence value came from.
 */
typedef struct {
    uint16_t source_flags;         /**< UFT_CONFSRC_* flags */
    uint8_t  confidence;           /**< Confidence from this source (0-100) */
    uint8_t  weight;               /**< Weight in fusion (0-255) */
} uft_confidence_source_t;

/**
 * @brief Per-bit confidence record
 * 
 * Complete confidence information for a single bit.
 * This is the core data structure for "kein Bit verloren".
 */
typedef struct {
    /* === Bit Value === */
    uint8_t  value;                /**< Decoded bit value (0 or 1) */
    uint8_t  confidence;           /**< Overall confidence (0-100%) */
    uint16_t flags;                /**< UFT_CONFLAG_* flags */
    
    /* === Position === */
    uint32_t bit_index;            /**< Bit position in track */
    uint32_t byte_index;           /**< Byte position (bit_index / 8) */
    uint8_t  bit_in_byte;          /**< Bit position within byte (0-7) */
    
    /* === Hardware Reference === */
    uint32_t flux_sample;          /**< Primary flux sample index */
    uint16_t flux_offset_ns;       /**< Offset within sample (0-65535 ns) */
    uint16_t flux_duration_ns;     /**< Flux transition duration */
    
    /* === Timing Information === */
    uint16_t timing_ns;            /**< Measured timing in nanoseconds */
    uint16_t expected_ns;          /**< Expected timing (from PLL) */
    int16_t  timing_error_ns;      /**< timing_ns - expected_ns */
    uint8_t  timing_ratio;         /**< (timing_ns / expected_ns) * 100 */
    
    /* === Multi-Revolution Data === */
    uint8_t  revolutions_read;     /**< Number of revolutions with this bit */
    uint8_t  ones_count;           /**< How many reads gave 1 */
    uint8_t  zeros_count;          /**< How many reads gave 0 */
    uint8_t  consistency;          /**< 100 = all same, 0 = 50/50 split */
    
    /* === PLL State === */
    uint8_t  pll_phase;            /**< PLL phase at decode (0-255) */
    uint8_t  pll_frequency;        /**< PLL frequency adjust */
    uint8_t  pll_lock_quality;     /**< PLL lock quality (0-100) */
    uint8_t  pll_status;           /**< PLL status flags */
    
    /* === Confidence Sources === */
    uint16_t source_flags;         /**< Which sources contributed */
    uint8_t  source_count;         /**< Number of sources */
    uft_confidence_source_t sources[4]; /**< Top contributing sources */
    
    /* === Alternative Interpretations === */
    uint8_t  alt_count;            /**< Number of alternatives */
    struct {
        uint8_t value;             /**< Alternative bit value */
        uint8_t confidence;        /**< Confidence for this alternative */
        uint16_t source_flags;     /**< Sources supporting this */
    } alternatives[UFT_BITCONF_MAX_ALTERNATIVES];
    
} uft_bit_confidence_t;

/**
 * @brief Packed per-bit confidence (space-efficient)
 * 
 * 8 bytes per bit for storage-efficient confidence tracking.
 * Use for large track arrays.
 */
typedef struct {
    uint8_t  value       : 1;      /**< Bit value */
    uint8_t  weak        : 1;      /**< Is weak/fuzzy */
    uint8_t  corrected   : 1;      /**< Was corrected */
    uint8_t  ambiguous   : 1;      /**< Has alternatives */
    uint8_t  protected   : 1;      /**< Part of protection */
    uint8_t  reserved    : 3;      /**< Reserved flags */
    
    uint8_t  confidence;           /**< Overall confidence (0-100) */
    uint8_t  consistency;          /**< Multi-rev consistency (0-100) */
    uint8_t  pll_quality;          /**< PLL quality at decode (0-100) */
    
    uint16_t timing_ns;            /**< Measured timing */
    uint16_t source_flags;         /**< Confidence sources */
} uft_bit_confidence_packed_t;

/*===========================================================================
 * Track Confidence
 *===========================================================================*/

/**
 * @brief Per-track confidence array
 */
typedef struct {
    /* Track info */
    uint8_t  track;
    uint8_t  head;
    uint32_t bit_count;
    
    /* Confidence array (one per bit) */
    uft_bit_confidence_packed_t *bits;  /**< Array of bit_count entries */
    
    /* Track-level statistics */
    uint8_t  min_confidence;       /**< Minimum bit confidence */
    uint8_t  max_confidence;       /**< Maximum bit confidence */
    uint8_t  avg_confidence;       /**< Average confidence */
    uint8_t  median_confidence;    /**< Median confidence */
    
    uint32_t weak_bit_count;       /**< Number of weak bits */
    uint32_t corrected_bit_count;  /**< Number of corrected bits */
    uint32_t ambiguous_bit_count;  /**< Number of ambiguous bits */
    
    /* Regions of concern */
    uint16_t low_conf_region_count;
    struct {
        uint32_t start_bit;
        uint32_t end_bit;
        uint8_t  min_confidence;
    } low_conf_regions[64];
    
} uft_track_confidence_t;

/**
 * @brief Sector confidence summary
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    
    uint8_t  overall_confidence;   /**< Overall sector confidence */
    uint8_t  header_confidence;    /**< Header decode confidence */
    uint8_t  data_confidence;      /**< Data decode confidence */
    uint8_t  crc_confidence;       /**< CRC verification confidence */
    
    bool     crc_valid;            /**< CRC check passed */
    bool     has_weak_bits;        /**< Contains weak bits */
    bool     was_corrected;        /**< Error correction applied */
    
    uint16_t weak_bit_count;       /**< Weak bits in sector */
    uint16_t low_conf_bit_count;   /**< Bits below threshold */
    
    uint32_t first_bit;            /**< First bit of sector */
    uint32_t bit_count;            /**< Total bits in sector */
    
} uft_sector_confidence_t;

/*===========================================================================
 * Confidence Calculation
 *===========================================================================*/

/**
 * @brief Confidence calculation parameters
 */
typedef struct {
    /* Weights for fusion (0-255, sum should be ~255) */
    uint8_t  weight_timing;        /**< Weight for timing confidence */
    uint8_t  weight_multirev;      /**< Weight for multi-rev consistency */
    uint8_t  weight_pll;           /**< Weight for PLL quality */
    uint8_t  weight_context;       /**< Weight for context */
    
    /* Thresholds */
    uint8_t  timing_tolerance_pct; /**< Timing tolerance for 100% conf */
    uint8_t  multirev_threshold;   /**< Minimum reads for full conf */
    uint8_t  pll_lock_threshold;   /**< PLL quality for full conf */
    
    /* Penalties */
    uint8_t  weak_penalty;         /**< Confidence reduction for weak bits */
    uint8_t  unstable_penalty;     /**< Penalty for varying reads */
    uint8_t  boundary_penalty;     /**< Penalty at boundaries */
    
} uft_confidence_params_t;

/**
 * @brief Default confidence calculation parameters
 */
static const uft_confidence_params_t UFT_CONFIDENCE_PARAMS_DEFAULT = {
    .weight_timing = 80,
    .weight_multirev = 100,
    .weight_pll = 50,
    .weight_context = 25,
    .timing_tolerance_pct = 10,
    .multirev_threshold = 3,
    .pll_lock_threshold = 80,
    .weak_penalty = 30,
    .unstable_penalty = 25,
    .boundary_penalty = 10
};

/*===========================================================================
 * Core Functions
 *===========================================================================*/

/**
 * @brief Initialize bit confidence record
 * 
 * @param conf Confidence record to initialize
 */
void uft_bitconf_init(uft_bit_confidence_t *conf);

/**
 * @brief Calculate confidence from timing
 * 
 * @param timing_ns Measured timing
 * @param expected_ns Expected timing
 * @param tolerance_pct Tolerance percentage
 * @return Confidence value (0-100)
 */
uint8_t uft_bitconf_from_timing(uint16_t timing_ns, uint16_t expected_ns,
                                 uint8_t tolerance_pct);

/**
 * @brief Calculate confidence from multi-revolution data
 * 
 * @param ones_count Number of reads that gave 1
 * @param zeros_count Number of reads that gave 0
 * @param value Output: best value
 * @return Confidence value (0-100)
 */
uint8_t uft_bitconf_from_multirev(uint8_t ones_count, uint8_t zeros_count,
                                   uint8_t *value);

/**
 * @brief Calculate confidence from PLL state
 * 
 * @param pll_phase PLL phase (0-255)
 * @param pll_lock_quality Lock quality (0-100)
 * @param pll_status Status flags
 * @return Confidence value (0-100)
 */
uint8_t uft_bitconf_from_pll(uint8_t pll_phase, uint8_t pll_lock_quality,
                              uint8_t pll_status);

/**
 * @brief Fuse multiple confidence sources
 * 
 * Combines confidence from multiple sources using weighted fusion.
 * 
 * @param sources Array of confidence sources
 * @param count Number of sources
 * @param params Calculation parameters (NULL for defaults)
 * @return Fused confidence value (0-100)
 */
uint8_t uft_bitconf_fuse(const uft_confidence_source_t *sources, uint8_t count,
                          const uft_confidence_params_t *params);

/**
 * @brief Update confidence record with new information
 * 
 * @param conf Confidence record to update
 * @param source New source information
 * @param params Calculation parameters
 */
void uft_bitconf_update(uft_bit_confidence_t *conf,
                         const uft_confidence_source_t *source,
                         const uft_confidence_params_t *params);

/**
 * @brief Add alternative interpretation
 * 
 * @param conf Confidence record
 * @param value Alternative bit value
 * @param confidence Confidence for alternative
 * @param source_flags Sources supporting alternative
 * @return true if added (space available)
 */
bool uft_bitconf_add_alternative(uft_bit_confidence_t *conf,
                                  uint8_t value,
                                  uint8_t confidence,
                                  uint16_t source_flags);

/*===========================================================================
 * Track Functions
 *===========================================================================*/

/**
 * @brief Allocate track confidence array
 * 
 * @param track Track number
 * @param head Head number
 * @param bit_count Number of bits
 * @return Allocated structure, or NULL on failure
 */
uft_track_confidence_t* uft_trackconf_alloc(uint8_t track, uint8_t head,
                                             uint32_t bit_count);

/**
 * @brief Free track confidence array
 * 
 * @param tconf Track confidence to free
 */
void uft_trackconf_free(uft_track_confidence_t *tconf);

/**
 * @brief Set bit confidence in track array
 * 
 * @param tconf Track confidence
 * @param bit_index Bit position
 * @param conf Packed confidence to set
 * @return 0 on success
 */
int uft_trackconf_set_bit(uft_track_confidence_t *tconf,
                           uint32_t bit_index,
                           const uft_bit_confidence_packed_t *conf);

/**
 * @brief Get bit confidence from track array
 * 
 * @param tconf Track confidence
 * @param bit_index Bit position
 * @param conf Output: packed confidence
 * @return 0 on success
 */
int uft_trackconf_get_bit(const uft_track_confidence_t *tconf,
                           uint32_t bit_index,
                           uft_bit_confidence_packed_t *conf);

/**
 * @brief Calculate track statistics
 * 
 * @param tconf Track confidence (statistics fields updated)
 */
void uft_trackconf_calc_stats(uft_track_confidence_t *tconf);

/**
 * @brief Find low confidence regions
 * 
 * @param tconf Track confidence
 * @param threshold Minimum confidence threshold
 * @param min_length Minimum region length to report
 */
void uft_trackconf_find_regions(uft_track_confidence_t *tconf,
                                 uint8_t threshold,
                                 uint32_t min_length);

/**
 * @brief Get full confidence for specific bit
 * 
 * Expands packed confidence to full record.
 * 
 * @param tconf Track confidence
 * @param bit_index Bit position
 * @param conf Output: full confidence record
 * @return 0 on success
 */
int uft_trackconf_get_full(const uft_track_confidence_t *tconf,
                            uint32_t bit_index,
                            uft_bit_confidence_t *conf);

/*===========================================================================
 * Sector Functions
 *===========================================================================*/

/**
 * @brief Calculate sector confidence from track
 * 
 * @param tconf Track confidence
 * @param start_bit Sector start bit
 * @param bit_count Sector bit count
 * @param header_bits Header bit count
 * @param sector Sector number
 * @param crc_valid CRC verification result
 * @param sconf Output: sector confidence
 */
void uft_sectorconf_calc(const uft_track_confidence_t *tconf,
                          uint32_t start_bit,
                          uint32_t bit_count,
                          uint32_t header_bits,
                          uint8_t sector,
                          bool crc_valid,
                          uft_sector_confidence_t *sconf);

/*===========================================================================
 * Reporting Functions
 *===========================================================================*/

/**
 * @brief Generate confidence report for bit
 * 
 * @param conf Bit confidence
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_bitconf_report(const uft_bit_confidence_t *conf,
                           char *buffer, size_t buffer_size);

/**
 * @brief Generate confidence report for track
 * 
 * @param tconf Track confidence
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_trackconf_report(const uft_track_confidence_t *tconf,
                             char *buffer, size_t buffer_size);

/**
 * @brief Export track confidence to JSON
 * 
 * @param tconf Track confidence
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written
 */
size_t uft_trackconf_export_json(const uft_track_confidence_t *tconf,
                                  char *buffer, size_t buffer_size);

/**
 * @brief Export track confidence heatmap
 * 
 * Creates visual representation of confidence across track.
 * 
 * @param tconf Track confidence
 * @param width Output width (samples)
 * @param heatmap Output array (width entries, 0-255 per entry)
 */
void uft_trackconf_heatmap(const uft_track_confidence_t *tconf,
                            uint32_t width,
                            uint8_t *heatmap);

/*===========================================================================
 * Serialization
 *===========================================================================*/

/**
 * @brief Serialize track confidence to binary
 * 
 * @param tconf Track confidence
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written, or negative on error
 */
int32_t uft_trackconf_serialize(const uft_track_confidence_t *tconf,
                                 uint8_t *buffer, size_t buffer_size);

/**
 * @brief Deserialize track confidence from binary
 * 
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param tconf Output: allocated track confidence
 * @return Bytes consumed, or negative on error
 */
int32_t uft_trackconf_deserialize(const uint8_t *buffer, size_t buffer_size,
                                   uft_track_confidence_t **tconf);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get confidence level name
 */
const char* uft_bitconf_level_name(uint8_t confidence);

/**
 * @brief Get source flag names
 * 
 * @param flags Source flags
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Buffer pointer
 */
char* uft_bitconf_source_names(uint16_t flags, char *buffer, size_t buffer_size);

/**
 * @brief Get confidence flag names
 * 
 * @param flags Confidence flags
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Buffer pointer
 */
char* uft_bitconf_flag_names(uint16_t flags, char *buffer, size_t buffer_size);

/**
 * @brief Pack full confidence to compact form
 * 
 * @param full Full confidence record
 * @param packed Output: packed confidence
 */
void uft_bitconf_pack(const uft_bit_confidence_t *full,
                       uft_bit_confidence_packed_t *packed);

/**
 * @brief Unpack compact confidence to full form
 * 
 * Note: Some information is lost in packing.
 * 
 * @param packed Packed confidence
 * @param full Output: full confidence record
 */
void uft_bitconf_unpack(const uft_bit_confidence_packed_t *packed,
                         uft_bit_confidence_t *full);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BIT_CONFIDENCE_H */

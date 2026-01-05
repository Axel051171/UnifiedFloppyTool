/**
 * @file uft_ir_timing.h
 * @brief Timing Preservation in Intermediate Representation
 * 
 * P0-PR-002: Timing-Preservation-in-IR
 * 
 * Preserves precise timing information in the UFT Intermediate Representation
 * for copy protection analysis and reproduction:
 * 
 * - Per-bit timing deviations
 * - Sector gap timing
 * - Sync pattern timing
 * - Inter-sector timing
 * - Revolution-to-revolution jitter
 * 
 * Critical for:
 * - Speedlock timing protection
 * - Long-track protection analysis
 * - Weak bit timing patterns
 * - Faithful disk reproduction
 */

#ifndef UFT_IR_TIMING_H
#define UFT_IR_TIMING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum timing entries per track */
#define UFT_TIMING_MAX_ENTRIES      65536

/** Timing resolution in nanoseconds */
#define UFT_TIMING_RESOLUTION_NS    25      /**< 25ns resolution */

/** Timing flags */
#define UFT_TIMING_FLAG_NORMAL      0x00
#define UFT_TIMING_FLAG_SYNC        0x01    /**< Sync pattern region */
#define UFT_TIMING_FLAG_GAP         0x02    /**< Gap region */
#define UFT_TIMING_FLAG_ADDRESS     0x04    /**< Address mark */
#define UFT_TIMING_FLAG_DATA        0x08    /**< Data region */
#define UFT_TIMING_FLAG_CRC         0x10    /**< CRC bytes */
#define UFT_TIMING_FLAG_ANOMALY     0x20    /**< Timing anomaly */
#define UFT_TIMING_FLAG_PROTECTED   0x40    /**< Protection-related */
#define UFT_TIMING_FLAG_WEAK        0x80    /**< Weak bit region */

/*===========================================================================
 * Timing Entry Structures
 *===========================================================================*/

/**
 * @brief Compact timing entry (4 bytes)
 * 
 * Stores timing as delta from expected value.
 */
typedef struct {
    uint16_t bit_offset;        /**< Bit offset from region start */
    int8_t   delta_ns;          /**< Timing delta (±127 * resolution) */
    uint8_t  flags;             /**< Region flags */
} uft_timing_entry_compact_t;

/**
 * @brief Full timing entry (8 bytes)
 * 
 * Stores absolute timing values.
 */
typedef struct {
    uint32_t bit_index;         /**< Absolute bit index in track */
    uint16_t actual_ns;         /**< Actual timing in ns */
    uint16_t expected_ns;       /**< Expected timing in ns */
} uft_timing_entry_full_t;

/**
 * @brief Timing region descriptor
 */
typedef struct {
    uint32_t start_bit;         /**< Region start bit */
    uint32_t end_bit;           /**< Region end bit */
    uint8_t  region_type;       /**< Region type (flags) */
    uint16_t expected_cell_ns;  /**< Expected bit cell for region */
    
    /* Statistics */
    int16_t  mean_delta_ns;     /**< Mean timing delta */
    uint16_t variance_ns;       /**< Timing variance */
    uint16_t max_deviation_ns;  /**< Maximum deviation */
} uft_timing_region_t;

/**
 * @brief Sector timing information
 */
typedef struct {
    uint8_t  sector_id;         /**< Sector number */
    
    /* Gap timing (bytes * 8 bits) */
    uint16_t pre_gap_bits;      /**< Pre-sector gap length */
    uint16_t post_gap_bits;     /**< Post-sector gap length */
    uint16_t gap_cell_ns;       /**< Gap bit cell time */
    
    /* Sync timing */
    uint16_t sync_bits;         /**< Sync pattern length */
    uint16_t sync_cell_ns;      /**< Sync bit cell time */
    
    /* Address timing */
    uint16_t addr_bits;         /**< Address field length */
    int8_t   addr_delta_ns;     /**< Address timing delta */
    
    /* Data timing */
    uint16_t data_bits;         /**< Data field length */
    int8_t   data_delta_ns;     /**< Mean data timing delta */
    uint8_t  data_variance;     /**< Data timing variance (scaled) */
    
    /* Anomaly info */
    uint8_t  anomaly_count;     /**< Number of timing anomalies */
    uint16_t first_anomaly_bit; /**< Position of first anomaly */
} uft_sector_timing_t;

/**
 * @brief Track timing information
 */
typedef struct {
    uint16_t track;             /**< Track number */
    uint8_t  side;              /**< Side (0 or 1) */
    uint8_t  flags;             /**< Track-level flags */
    
    /* Revolution info */
    uint32_t revolution_ns;     /**< Total revolution time */
    uint16_t rpm_measured;      /**< Measured RPM (x10) */
    int16_t  rpm_deviation;     /**< RPM deviation from nominal (x100) */
    
    /* Bit cell info */
    uint16_t nominal_cell_ns;   /**< Nominal bit cell time */
    int8_t   mean_cell_delta;   /**< Mean cell time delta */
    uint8_t  cell_variance;     /**< Cell time variance (scaled) */
    
    /* Sector timing array */
    uint8_t  sector_count;      /**< Number of sectors */
    uft_sector_timing_t *sectors; /**< Per-sector timing */
    
    /* Detailed timing entries */
    bool has_detailed_timing;   /**< Detailed entries present */
    uft_timing_entry_compact_t *entries; /**< Compact entries */
    size_t entry_count;         /**< Number of entries */
    
    /* Timing regions */
    uft_timing_region_t *regions; /**< Timing regions */
    size_t region_count;        /**< Number of regions */
    
    /* Protection analysis */
    bool timing_protection;     /**< Timing protection detected */
    uint32_t protection_start;  /**< Protection region start */
    uint32_t protection_length; /**< Protection region length */
    uint8_t  protection_type;   /**< Protection type hint */
} uft_track_timing_t;

/**
 * @brief Multi-revolution timing comparison
 */
typedef struct {
    uint8_t  revolution_count;  /**< Number of revolutions compared */
    uint32_t *revolution_ns;    /**< Revolution times [rev_count] */
    
    /* Cross-revolution analysis */
    double   mean_rev_ns;       /**< Mean revolution time */
    double   std_rev_ns;        /**< Revolution time std dev */
    double   jitter_pct;        /**< Jitter percentage */
    
    /* Per-bit variance (sampled) */
    uint32_t sample_count;      /**< Number of sample points */
    uint16_t *bit_indices;      /**< Sampled bit indices */
    uint16_t *timing_variance;  /**< Variance at each sample */
} uft_multirev_timing_t;

/*===========================================================================
 * Configuration
 *===========================================================================*/

/**
 * @brief Timing preservation configuration
 */
typedef struct {
    bool preserve_all;          /**< Store all timing (memory intensive) */
    bool preserve_anomalies;    /**< Store anomalous timing only */
    bool preserve_regions;      /**< Store region statistics */
    bool preserve_sectors;      /**< Store per-sector timing */
    
    uint8_t anomaly_threshold;  /**< Anomaly threshold (% deviation) */
    uint16_t resolution_ns;     /**< Timing resolution */
    
    bool detect_protection;     /**< Analyze for protection timing */
} uft_timing_config_t;

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize timing config with defaults
 * @param config Configuration to initialize
 */
void uft_timing_config_default(uft_timing_config_t *config);

/**
 * @brief Allocate track timing structure
 * @param track Track number
 * @param side Side
 * @param max_sectors Maximum sectors
 * @param max_entries Maximum timing entries (0 = no detailed timing)
 * @return Allocated structure or NULL
 */
uft_track_timing_t *uft_timing_alloc(uint16_t track, uint8_t side,
                                      uint8_t max_sectors, size_t max_entries);

/**
 * @brief Free track timing structure
 * @param timing Structure to free
 */
void uft_timing_free(uft_track_timing_t *timing);

/*===========================================================================
 * Timing Recording
 *===========================================================================*/

/**
 * @brief Record bit timing
 * @param timing Track timing structure
 * @param bit_index Bit position
 * @param actual_ns Actual timing
 * @param expected_ns Expected timing
 * @param flags Region flags
 * @return 0 on success
 */
int uft_timing_record_bit(uft_track_timing_t *timing, uint32_t bit_index,
                          uint16_t actual_ns, uint16_t expected_ns, uint8_t flags);

/**
 * @brief Record sector timing
 * @param timing Track timing structure
 * @param sector_id Sector number
 * @param sector_timing Sector timing data
 * @return 0 on success
 */
int uft_timing_record_sector(uft_track_timing_t *timing, uint8_t sector_id,
                             const uft_sector_timing_t *sector_timing);

/**
 * @brief Record timing region
 * @param timing Track timing structure
 * @param start_bit Region start
 * @param end_bit Region end
 * @param region_type Region type
 * @param expected_cell_ns Expected cell time
 * @return 0 on success
 */
int uft_timing_record_region(uft_track_timing_t *timing, uint32_t start_bit,
                             uint32_t end_bit, uint8_t region_type,
                             uint16_t expected_cell_ns);

/**
 * @brief Record from flux transitions
 * @param timing Track timing structure
 * @param flux_ns Flux intervals in nanoseconds
 * @param count Number of intervals
 * @param expected_cell_ns Expected cell time
 * @param config Recording configuration
 * @return Number of entries recorded
 */
size_t uft_timing_record_flux(uft_track_timing_t *timing,
                              const uint32_t *flux_ns, size_t count,
                              uint16_t expected_cell_ns,
                              const uft_timing_config_t *config);

/*===========================================================================
 * Analysis Functions
 *===========================================================================*/

/**
 * @brief Calculate timing statistics for track
 * @param timing Track timing (updated with stats)
 */
void uft_timing_calculate_stats(uft_track_timing_t *timing);

/**
 * @brief Detect timing-based protection
 * @param timing Track timing
 * @return true if protection detected
 */
bool uft_timing_detect_protection(uft_track_timing_t *timing);

/**
 * @brief Find timing anomalies
 * @param timing Track timing
 * @param threshold Anomaly threshold (% deviation)
 * @param positions Output array (caller allocates)
 * @param max_count Maximum anomalies
 * @return Number of anomalies found
 */
size_t uft_timing_find_anomalies(const uft_track_timing_t *timing,
                                 uint8_t threshold,
                                 uint32_t *positions, size_t max_count);

/**
 * @brief Compare multi-revolution timing
 * @param revolutions Array of track timings (one per revolution)
 * @param rev_count Number of revolutions
 * @param result Output comparison result
 * @return 0 on success
 */
int uft_timing_compare_revolutions(const uft_track_timing_t **revolutions,
                                   size_t rev_count,
                                   uft_multirev_timing_t *result);

/*===========================================================================
 * Serialization (for IR)
 *===========================================================================*/

/**
 * @brief Serialize track timing to binary
 * @param timing Track timing
 * @param buffer Output buffer (caller allocates)
 * @param buffer_size Buffer size
 * @return Bytes written, or required size if buffer too small
 */
size_t uft_timing_serialize(const uft_track_timing_t *timing,
                            uint8_t *buffer, size_t buffer_size);

/**
 * @brief Deserialize track timing from binary
 * @param buffer Input buffer
 * @param buffer_size Buffer size
 * @param timing Output timing (allocated by this function)
 * @return 0 on success
 */
int uft_timing_deserialize(const uint8_t *buffer, size_t buffer_size,
                           uft_track_timing_t **timing);

/**
 * @brief Export timing to JSON
 * @param timing Track timing
 * @param path Output file path
 * @param include_entries Include detailed entries
 * @return 0 on success
 */
int uft_timing_export_json(const uft_track_timing_t *timing,
                           const char *path, bool include_entries);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Calculate timing delta in resolution units
 * @param actual_ns Actual timing
 * @param expected_ns Expected timing
 * @return Delta in resolution units (clamped to int8_t range)
 */
static inline int8_t uft_timing_delta(uint16_t actual_ns, uint16_t expected_ns)
{
    int32_t delta = ((int32_t)actual_ns - (int32_t)expected_ns) / 
                    UFT_TIMING_RESOLUTION_NS;
    if (delta > 127) return 127;
    if (delta < -128) return -128;
    return (int8_t)delta;
}

/**
 * @brief Reconstruct timing from delta
 * @param expected_ns Expected timing
 * @param delta_units Delta in resolution units
 * @return Reconstructed actual timing
 */
static inline uint16_t uft_timing_reconstruct(uint16_t expected_ns, 
                                               int8_t delta_units)
{
    return expected_ns + (delta_units * UFT_TIMING_RESOLUTION_NS);
}

/**
 * @brief Check if timing is anomalous
 * @param actual_ns Actual timing
 * @param expected_ns Expected timing
 * @param threshold_pct Threshold percentage
 * @return true if anomalous
 */
static inline bool uft_timing_is_anomaly(uint16_t actual_ns, uint16_t expected_ns,
                                          uint8_t threshold_pct)
{
    if (expected_ns == 0) return false;
    int32_t diff = (int32_t)actual_ns - (int32_t)expected_ns;
    if (diff < 0) diff = -diff;
    return (diff * 100) > ((int32_t)expected_ns * threshold_pct);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_IR_TIMING_H */

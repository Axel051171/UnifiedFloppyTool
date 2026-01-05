/**
 * @file uft_latency_tracking.h
 * @brief Per-Bit Latency Tracking for Variable-Density Detection
 * 
 * P0-HW-004: Latency-Tracking
 * 
 * Provides precise timing analysis for:
 * - Variable-density disk detection (Victor 9000, Apple GCR)
 * - Speedlock timing-based protection analysis
 * - Copy protection timing deviation detection
 * - Hardware read-head response analysis
 * 
 * Key Features:
 * - Per-bit latency measurement
 * - Expected vs. actual deviation tracking
 * - Zone-based density detection
 * - Statistical timing analysis
 */

#ifndef UFT_LATENCY_TRACKING_H
#define UFT_LATENCY_TRACKING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum bits to track per track */
#define UFT_LAT_MAX_BITS            100000

/** Maximum zones for variable-density */
#define UFT_LAT_MAX_ZONES           16

/** Timing tolerance thresholds */
#define UFT_LAT_TOLERANCE_NORMAL    10      /**< ±10% normal tolerance */
#define UFT_LAT_TOLERANCE_STRICT    5       /**< ±5% strict tolerance */
#define UFT_LAT_TOLERANCE_LOOSE     20      /**< ±20% loose tolerance */

/** Standard bit cell times (ns) */
#define UFT_LAT_CELL_MFM_DD         4000    /**< MFM DD: 4µs */
#define UFT_LAT_CELL_MFM_HD         2000    /**< MFM HD: 2µs */
#define UFT_LAT_CELL_FM             8000    /**< FM: 8µs */
#define UFT_LAT_CELL_GCR_C64        3692    /**< C64 GCR: ~3.7µs */
#define UFT_LAT_CELL_GCR_APPLE      4000    /**< Apple GCR: 4µs */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Per-bit latency measurement
 */
typedef struct {
    uint32_t bit_index;         /**< Bit position in track */
    uint16_t latency_ns;        /**< Measured latency in ns */
    uint16_t expected_ns;       /**< Expected latency based on encoding */
    int8_t   deviation_pct;     /**< Deviation percentage (-128 to +127) */
    uint8_t  flags;             /**< Status flags */
} uft_bit_latency_t;

/** Latency flags */
#define UFT_LAT_FLAG_NORMAL     0x00    /**< Normal timing */
#define UFT_LAT_FLAG_SHORT      0x01    /**< Shorter than expected */
#define UFT_LAT_FLAG_LONG       0x02    /**< Longer than expected */
#define UFT_LAT_FLAG_ANOMALY    0x04    /**< Significant anomaly */
#define UFT_LAT_FLAG_SYNC       0x08    /**< Sync mark region */
#define UFT_LAT_FLAG_GAP        0x10    /**< Gap region */
#define UFT_LAT_FLAG_WEAK       0x20    /**< Weak bit detected */
#define UFT_LAT_FLAG_PROTECTED  0x40    /**< Protection-related timing */

/**
 * @brief Density zone for variable-density disks
 */
typedef struct {
    uint8_t  zone_id;           /**< Zone identifier (0-15) */
    uint16_t start_track;       /**< Starting track number */
    uint16_t end_track;         /**< Ending track number */
    uint16_t expected_cell_ns;  /**< Expected bit cell time for this zone */
    uint16_t sectors_per_track; /**< Sectors per track in this zone */
    uint32_t data_rate_bps;     /**< Data rate in bits per second */
    char     name[16];          /**< Zone name (e.g., "Zone 0 - Outer") */
} uft_density_zone_t;

/**
 * @brief Variable-density disk profile
 */
typedef struct {
    const char *name;           /**< Profile name (e.g., "Victor 9000") */
    uint8_t zone_count;         /**< Number of zones */
    uft_density_zone_t zones[UFT_LAT_MAX_ZONES];
    double rpm;                 /**< Drive RPM */
    bool big_endian;            /**< Data byte order */
} uft_density_profile_t;

/**
 * @brief Latency statistics for a region
 */
typedef struct {
    uint32_t sample_count;      /**< Number of samples */
    double   mean_ns;           /**< Mean latency */
    double   std_ns;            /**< Standard deviation */
    uint16_t min_ns;            /**< Minimum latency */
    uint16_t max_ns;            /**< Maximum latency */
    double   deviation_mean;    /**< Mean deviation from expected */
    uint32_t anomaly_count;     /**< Number of anomalies */
    double   confidence;        /**< Timing confidence (0-1) */
} uft_latency_stats_t;

/**
 * @brief Track latency analysis result
 */
typedef struct {
    uint16_t track;             /**< Track number */
    uint8_t  side;              /**< Side (0 or 1) */
    
    /* Bit-level latencies */
    uft_bit_latency_t *bits;    /**< Per-bit latency array */
    size_t bit_count;           /**< Number of bits tracked */
    size_t bit_capacity;        /**< Allocated capacity */
    
    /* Zone detection */
    uint8_t detected_zone;      /**< Detected density zone */
    double zone_confidence;     /**< Zone detection confidence */
    
    /* Statistics */
    uft_latency_stats_t stats;  /**< Overall statistics */
    
    /* Anomaly tracking */
    uint32_t *anomaly_positions;/**< Positions of timing anomalies */
    size_t anomaly_count;       /**< Number of anomalies */
    
    /* Protection hints */
    bool timing_protection;     /**< Timing-based protection detected */
    uint32_t protection_start;  /**< Protection region start */
    uint32_t protection_end;    /**< Protection region end */
} uft_track_latency_t;

/**
 * @brief Latency tracking configuration
 */
typedef struct {
    uint16_t expected_cell_ns;  /**< Expected bit cell time */
    uint8_t  tolerance_pct;     /**< Tolerance percentage */
    bool     track_all_bits;    /**< Track all bits (memory intensive) */
    bool     detect_protection; /**< Look for protection timing */
    bool     auto_zone_detect;  /**< Auto-detect density zones */
    const uft_density_profile_t *profile; /**< Variable-density profile */
} uft_latency_config_t;

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize latency configuration with defaults
 * @param config Configuration to initialize
 * @param encoding Encoding type for expected cell time
 */
void uft_lat_config_init(uft_latency_config_t *config, uint8_t encoding);

/**
 * @brief Allocate track latency structure
 * @param track Track number
 * @param side Side (0 or 1)
 * @param max_bits Maximum bits to track
 * @return Allocated structure or NULL
 */
uft_track_latency_t *uft_lat_alloc(uint16_t track, uint8_t side, size_t max_bits);

/**
 * @brief Free track latency structure
 * @param lat Structure to free
 */
void uft_lat_free(uft_track_latency_t *lat);

/*===========================================================================
 * Latency Recording
 *===========================================================================*/

/**
 * @brief Record a single bit latency
 * @param lat Track latency structure
 * @param bit_index Bit position
 * @param latency_ns Measured latency in nanoseconds
 * @param expected_ns Expected latency
 * @return 0 on success
 */
int uft_lat_record(uft_track_latency_t *lat, uint32_t bit_index,
                   uint16_t latency_ns, uint16_t expected_ns);

/**
 * @brief Record latencies from flux transitions
 * @param lat Track latency structure
 * @param flux_intervals Flux interval array (ns)
 * @param count Number of intervals
 * @param expected_cell_ns Expected cell time
 * @return Number of bits recorded
 */
size_t uft_lat_record_flux(uft_track_latency_t *lat,
                           const uint32_t *flux_intervals, size_t count,
                           uint16_t expected_cell_ns);

/**
 * @brief Mark bit with flag
 * @param lat Track latency structure
 * @param bit_index Bit to mark
 * @param flag Flag to add
 */
void uft_lat_mark_bit(uft_track_latency_t *lat, uint32_t bit_index, uint8_t flag);

/*===========================================================================
 * Analysis Functions
 *===========================================================================*/

/**
 * @brief Compute latency statistics
 * @param lat Track latency structure
 * @param stats Output statistics
 */
void uft_lat_compute_stats(const uft_track_latency_t *lat,
                           uft_latency_stats_t *stats);

/**
 * @brief Detect timing anomalies
 * @param lat Track latency structure
 * @param config Latency configuration
 * @return Number of anomalies found
 */
size_t uft_lat_detect_anomalies(uft_track_latency_t *lat,
                                const uft_latency_config_t *config);

/**
 * @brief Detect density zone from latencies
 * @param lat Track latency structure
 * @param profile Variable-density profile
 * @return Detected zone ID or -1
 */
int uft_lat_detect_zone(uft_track_latency_t *lat,
                        const uft_density_profile_t *profile);

/**
 * @brief Detect timing-based protection
 * @param lat Track latency structure
 * @param config Configuration
 * @return true if protection detected
 */
bool uft_lat_detect_protection(uft_track_latency_t *lat,
                               const uft_latency_config_t *config);

/*===========================================================================
 * Variable-Density Profiles
 *===========================================================================*/

/**
 * @brief Get Victor 9000 density profile
 * @return Victor 9000 profile (8 zones)
 */
const uft_density_profile_t *uft_lat_profile_victor9k(void);

/**
 * @brief Get Apple IIgs 3.5" density profile
 * @return Apple IIgs profile (5 zones)
 */
const uft_density_profile_t *uft_lat_profile_apple_35(void);

/**
 * @brief Get Commodore 1541 density profile
 * @return C64 1541 profile (4 zones)
 */
const uft_density_profile_t *uft_lat_profile_c64_1541(void);

/**
 * @brief Create custom density profile
 * @param name Profile name
 * @param zone_count Number of zones
 * @return Allocated profile (caller frees)
 */
uft_density_profile_t *uft_lat_profile_create(const char *name, uint8_t zone_count);

/*===========================================================================
 * Speedlock Analysis
 *===========================================================================*/

/**
 * @brief Speedlock timing signature
 */
typedef struct {
    uint32_t sector;            /**< Sector with timing protection */
    uint16_t expected_gap_ns;   /**< Expected gap timing */
    uint16_t tolerance_ns;      /**< Timing tolerance */
    bool     detected;          /**< Signature detected */
    double   confidence;        /**< Detection confidence */
} uft_speedlock_timing_t;

/**
 * @brief Analyze for Speedlock timing protection
 * @param lat Track latency structure
 * @param result Output Speedlock analysis
 * @return true if Speedlock timing detected
 */
bool uft_lat_analyze_speedlock(const uft_track_latency_t *lat,
                               uft_speedlock_timing_t *result);

/*===========================================================================
 * Export Functions
 *===========================================================================*/

/**
 * @brief Export latency data to JSON
 * @param lat Track latency structure
 * @param path Output file path
 * @return 0 on success
 */
int uft_lat_export_json(const uft_track_latency_t *lat, const char *path);

/**
 * @brief Export latency histogram
 * @param lat Track latency structure
 * @param bins Histogram bins
 * @param histogram Output histogram (caller allocates)
 * @param min_ns Histogram minimum
 * @param max_ns Histogram maximum
 */
void uft_lat_histogram(const uft_track_latency_t *lat, uint16_t bins,
                       uint32_t *histogram, uint16_t min_ns, uint16_t max_ns);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Calculate deviation percentage
 * @param measured Measured value
 * @param expected Expected value
 * @return Deviation percentage
 */
static inline int8_t uft_lat_deviation_pct(uint16_t measured, uint16_t expected)
{
    if (expected == 0) return 0;
    int32_t diff = (int32_t)measured - (int32_t)expected;
    int32_t pct = (diff * 100) / (int32_t)expected;
    if (pct > 127) return 127;
    if (pct < -128) return -128;
    return (int8_t)pct;
}

/**
 * @brief Check if latency is within tolerance
 * @param measured Measured value
 * @param expected Expected value
 * @param tolerance_pct Tolerance percentage
 * @return true if within tolerance
 */
static inline bool uft_lat_in_tolerance(uint16_t measured, uint16_t expected,
                                         uint8_t tolerance_pct)
{
    if (expected == 0) return false;
    uint16_t margin = (expected * tolerance_pct) / 100;
    return (measured >= expected - margin) && (measured <= expected + margin);
}

/**
 * @brief Classify latency deviation
 * @param deviation_pct Deviation percentage
 * @param tolerance_pct Tolerance threshold
 * @return Flag indicating deviation type
 */
static inline uint8_t uft_lat_classify(int8_t deviation_pct, uint8_t tolerance_pct)
{
    if (deviation_pct < -(int8_t)tolerance_pct) {
        return (deviation_pct < -50) ? UFT_LAT_FLAG_ANOMALY : UFT_LAT_FLAG_SHORT;
    }
    if (deviation_pct > (int8_t)tolerance_pct) {
        return (deviation_pct > 50) ? UFT_LAT_FLAG_ANOMALY : UFT_LAT_FLAG_LONG;
    }
    return UFT_LAT_FLAG_NORMAL;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_LATENCY_TRACKING_H */

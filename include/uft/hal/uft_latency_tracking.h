/**
 * @file uft_latency_tracking.h
 * @brief P0-HW-004: Per-Bit Latency Tracking for Variable-Density Detection
 * 
 * This module provides precise timing measurement and analysis for detecting
 * variable-density encoding schemes used in copy protection:
 * 
 * - Speedlock (Atari ST, Amiga): Variable bit-cell timing
 * - Copylock: Long tracks with density variations
 * - V-MAX! (C64): GCR timing variations
 * - Apple II Spiral: Progressive density changes
 * 
 * The latency tracker records per-bit timing deviations from nominal values,
 * enabling detection and faithful reproduction of copy-protected content.
 * 
 * @version 1.0.0
 * @date 2025-01-05
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_LATENCY_TRACKING_H
#define UFT_LATENCY_TRACKING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum bits per track for latency tracking */
#define UFT_LAT_MAX_BITS            100000

/** Maximum latency regions per track */
#define UFT_LAT_MAX_REGIONS         256

/** Deviation threshold for anomaly detection (%) */
#define UFT_LAT_ANOMALY_THRESHOLD   15

/** Minimum region size for detection (bits) */
#define UFT_LAT_MIN_REGION_BITS     32

/*===========================================================================
 * Encoding Timing Constants (nanoseconds)
 *===========================================================================*/

/** MFM nominal bit-cell timings */
#define UFT_LAT_MFM_DD_NS       2000    /**< DD: 500 kbit/s = 2000ns */
#define UFT_LAT_MFM_HD_NS       1000    /**< HD: 1000 kbit/s = 1000ns */
#define UFT_LAT_MFM_ED_NS        500    /**< ED: 2000 kbit/s = 500ns */

/** GCR nominal bit-cell timings */
#define UFT_LAT_GCR_C64_NS      4000    /**< C64: 250 kbit/s = 4000ns (zone 1) */
#define UFT_LAT_GCR_APPLE_NS    4000    /**< Apple II: ~250 kbit/s */

/** FM nominal bit-cell timing */
#define UFT_LAT_FM_DD_NS        4000    /**< FM DD: 250 kbit/s */

/*===========================================================================
 * Latency Types
 *===========================================================================*/

/**
 * @brief Timing deviation type
 */
typedef enum uft_latency_type {
    UFT_LAT_TYPE_NORMAL = 0,    /**< Normal timing */
    UFT_LAT_TYPE_LONG,          /**< Longer than expected (slower) */
    UFT_LAT_TYPE_SHORT,         /**< Shorter than expected (faster) */
    UFT_LAT_TYPE_VARIABLE,      /**< Variable density region */
    UFT_LAT_TYPE_WEAK,          /**< Weak/unstable timing */
    UFT_LAT_TYPE_MISSING,       /**< Missing flux transition */
} uft_latency_type_t;

/**
 * @brief Protection scheme detected via timing
 */
typedef enum uft_latency_protection {
    UFT_LAT_PROT_NONE = 0,
    UFT_LAT_PROT_SPEEDLOCK,     /**< Speedlock variable density */
    UFT_LAT_PROT_COPYLOCK,      /**< Copylock long track */
    UFT_LAT_PROT_VMAX,          /**< V-MAX! GCR timing */
    UFT_LAT_PROT_RAPIDLOK,      /**< RapidLok timing tricks */
    UFT_LAT_PROT_SPIRAL,        /**< Apple spiral protection */
    UFT_LAT_PROT_MACRODOS,      /**< Macrodos (Atari ST) */
    UFT_LAT_PROT_FLASCHEL,      /**< Flaschel FDC exploit */
    UFT_LAT_PROT_GENERIC,       /**< Generic timing anomaly */
} uft_latency_protection_t;

/*===========================================================================
 * Per-Bit Latency Entry
 *===========================================================================*/

/**
 * @brief Single bit latency measurement
 */
typedef struct uft_bit_latency {
    uint32_t    bit_index;      /**< Bit position in track */
    uint16_t    latency_ns;     /**< Measured latency in nanoseconds */
    uint16_t    expected_ns;    /**< Expected latency based on encoding */
    int8_t      deviation_pct;  /**< Deviation from expected (-128 to +127%) */
    uint8_t     confidence;     /**< Measurement confidence (0-255) */
    uint8_t     type;           /**< uft_latency_type_t */
    uint8_t     flags;          /**< Additional flags */
} uft_bit_latency_t;

/** Flags for uft_bit_latency_t */
#define UFT_LAT_FLAG_SYNC       0x01    /**< Part of sync pattern */
#define UFT_LAT_FLAG_HEADER     0x02    /**< Part of sector header */
#define UFT_LAT_FLAG_DATA       0x04    /**< Part of sector data */
#define UFT_LAT_FLAG_GAP        0x08    /**< In gap region */
#define UFT_LAT_FLAG_PROTECTED  0x10    /**< In protection region */
#define UFT_LAT_FLAG_MULTIREV   0x20    /**< Multi-revolution averaged */

/*===========================================================================
 * Latency Region
 *===========================================================================*/

/**
 * @brief Contiguous region with similar timing characteristics
 */
typedef struct uft_latency_region {
    uint32_t    start_bit;          /**< Start bit index */
    uint32_t    end_bit;            /**< End bit index (exclusive) */
    uint16_t    avg_latency_ns;     /**< Average latency in region */
    uint16_t    expected_ns;        /**< Expected latency */
    int16_t     deviation_pct;      /**< Average deviation (%) */
    uint16_t    variance;           /**< Timing variance */
    uint8_t     type;               /**< uft_latency_type_t */
    uint8_t     protection;         /**< uft_latency_protection_t */
    float       density_ratio;      /**< 1.0 = normal, >1.0 = slower */
    uint8_t     confidence;         /**< Detection confidence (0-100) */
    uint8_t     reserved[3];
} uft_latency_region_t;

/*===========================================================================
 * Track Latency Profile
 *===========================================================================*/

/**
 * @brief Complete latency profile for a track
 */
typedef struct uft_track_latency {
    /* Track identification */
    uint8_t     cylinder;           /**< Cylinder number */
    uint8_t     head;               /**< Head number */
    uint8_t     revolution;         /**< Revolution number (0-15) */
    uint8_t     encoding;           /**< Detected encoding */
    
    /* Nominal timing */
    uint16_t    nominal_ns;         /**< Nominal bit-cell time */
    uint16_t    sample_rate_mhz;    /**< Sample rate in MHz */
    
    /* Global statistics */
    uint32_t    total_bits;         /**< Total bits in track */
    uint16_t    avg_latency_ns;     /**< Overall average latency */
    uint16_t    min_latency_ns;     /**< Minimum latency */
    uint16_t    max_latency_ns;     /**< Maximum latency */
    uint16_t    std_deviation_ns;   /**< Standard deviation */
    
    /* Anomaly summary */
    uint32_t    anomaly_count;      /**< Bits with significant deviation */
    uint16_t    long_count;         /**< Bits longer than threshold */
    uint16_t    short_count;        /**< Bits shorter than threshold */
    
    /* Protection detection */
    uint8_t     protection_type;    /**< uft_latency_protection_t */
    uint8_t     protection_confidence;
    uint16_t    protection_region_count;
    
    /* Per-bit data (sparse - only anomalies stored) */
    uft_bit_latency_t *bits;        /**< Array of bit latencies */
    size_t      bit_count;          /**< Number of entries in bits array */
    size_t      bit_capacity;       /**< Allocated capacity */
    
    /* Regions */
    uft_latency_region_t *regions;  /**< Detected regions */
    size_t      region_count;       /**< Number of regions */
    size_t      region_capacity;    /**< Allocated capacity */
    
    /* Histogram (10ns buckets from 0-10240ns) */
    uint32_t    histogram[1024];    /**< Latency histogram */
    
} uft_track_latency_t;

/*===========================================================================
 * Disk Latency Profile
 *===========================================================================*/

/**
 * @brief Latency profiles for entire disk
 */
typedef struct uft_disk_latency {
    uint8_t     cylinders;          /**< Number of cylinders */
    uint8_t     heads;              /**< Number of heads */
    uint8_t     revolutions;        /**< Revolutions per track */
    uint8_t     flags;              /**< Analysis flags */
    
    /* Global statistics */
    uint16_t    avg_latency_ns;     /**< Disk-wide average */
    uint16_t    std_deviation_ns;   /**< Disk-wide std dev */
    
    /* Protection summary */
    uint8_t     protection_type;    /**< Dominant protection type */
    uint8_t     protection_confidence;
    uint16_t    protected_track_count;
    
    /* Track data */
    uft_track_latency_t **tracks;   /**< Array of track profiles */
    size_t      track_count;        /**< Number of tracks */
    
} uft_disk_latency_t;

/*===========================================================================
 * Analysis Configuration
 *===========================================================================*/

/**
 * @brief Configuration for latency analysis
 */
typedef struct uft_latency_config {
    uint16_t    nominal_ns;             /**< Expected bit-cell time (0=auto) */
    uint8_t     anomaly_threshold_pct;  /**< Deviation threshold (default 15%) */
    uint8_t     min_region_bits;        /**< Minimum region size (default 32) */
    bool        store_all_bits;         /**< Store all bits (not just anomalies) */
    bool        build_histogram;        /**< Build latency histogram */
    bool        detect_protection;      /**< Run protection detection */
    bool        multi_rev_average;      /**< Average across revolutions */
} uft_latency_config_t;

/*===========================================================================
 * API: Context Management
 *===========================================================================*/

/**
 * @brief Create track latency profile
 * @return New profile or NULL on error
 */
uft_track_latency_t *uft_lat_track_create(void);

/**
 * @brief Destroy track latency profile
 */
void uft_lat_track_destroy(uft_track_latency_t *lat);

/**
 * @brief Reset track latency for reuse
 */
void uft_lat_track_reset(uft_track_latency_t *lat);

/**
 * @brief Create disk latency profile
 * @param cylinders Number of cylinders
 * @param heads Number of heads
 * @return New profile or NULL
 */
uft_disk_latency_t *uft_lat_disk_create(uint8_t cylinders, uint8_t heads);

/**
 * @brief Destroy disk latency profile
 */
void uft_lat_disk_destroy(uft_disk_latency_t *lat);

/*===========================================================================
 * API: Data Recording
 *===========================================================================*/

/**
 * @brief Record single bit latency
 * @param lat Track latency profile
 * @param bit_index Bit position
 * @param latency_ns Measured latency in nanoseconds
 * @param expected_ns Expected latency
 * @return 0 on success
 */
int uft_lat_record_bit(uft_track_latency_t *lat,
                       uint32_t bit_index,
                       uint16_t latency_ns,
                       uint16_t expected_ns);

/**
 * @brief Record flux transition timing
 * @param lat Track latency profile
 * @param sample_index Flux sample index
 * @param sample_count Samples since last transition
 * @param sample_rate_hz Sample rate
 * @return 0 on success
 */
int uft_lat_record_flux(uft_track_latency_t *lat,
                        uint32_t sample_index,
                        uint32_t sample_count,
                        uint32_t sample_rate_hz);

/**
 * @brief Import timing from flux array
 * @param lat Track latency profile
 * @param flux_times Array of flux transition times
 * @param count Number of transitions
 * @param sample_rate_hz Sample rate in Hz
 * @param nominal_ns Expected bit-cell time
 * @return 0 on success
 */
int uft_lat_import_flux(uft_track_latency_t *lat,
                        const uint32_t *flux_times,
                        size_t count,
                        uint32_t sample_rate_hz,
                        uint16_t nominal_ns);

/*===========================================================================
 * API: Analysis
 *===========================================================================*/

/**
 * @brief Analyze track latency and detect regions
 * @param lat Track latency profile
 * @param config Analysis configuration
 * @return 0 on success
 */
int uft_lat_analyze_track(uft_track_latency_t *lat,
                          const uft_latency_config_t *config);

/**
 * @brief Detect protection scheme from timing
 * @param lat Track latency profile
 * @param protection Output: detected protection type
 * @param confidence Output: confidence (0-100)
 * @return 0 on success
 */
int uft_lat_detect_protection(const uft_track_latency_t *lat,
                              uft_latency_protection_t *protection,
                              uint8_t *confidence);

/**
 * @brief Find variable-density regions
 * @param lat Track latency profile
 * @param regions Output array
 * @param max_regions Maximum regions to return
 * @return Number of regions found
 */
int uft_lat_find_variable_regions(const uft_track_latency_t *lat,
                                  uft_latency_region_t *regions,
                                  size_t max_regions);

/**
 * @brief Calculate density ratio at bit position
 * @param lat Track latency profile
 * @param bit_index Bit position
 * @return Density ratio (1.0 = normal)
 */
float uft_lat_get_density_ratio(const uft_track_latency_t *lat,
                                uint32_t bit_index);

/*===========================================================================
 * API: Statistics
 *===========================================================================*/

/**
 * @brief Get timing statistics for bit range
 * @param lat Track latency profile
 * @param start_bit Start bit index
 * @param end_bit End bit index
 * @param avg_ns Output: average latency
 * @param std_ns Output: standard deviation
 * @param min_ns Output: minimum latency
 * @param max_ns Output: maximum latency
 * @return 0 on success
 */
int uft_lat_get_stats(const uft_track_latency_t *lat,
                      uint32_t start_bit, uint32_t end_bit,
                      uint16_t *avg_ns, uint16_t *std_ns,
                      uint16_t *min_ns, uint16_t *max_ns);

/**
 * @brief Get histogram bucket count
 * @param lat Track latency profile
 * @param latency_ns Latency value
 * @return Count in that bucket
 */
uint32_t uft_lat_histogram_get(const uft_track_latency_t *lat,
                               uint16_t latency_ns);

/**
 * @brief Find histogram peak (most common latency)
 * @param lat Track latency profile
 * @return Peak latency in nanoseconds
 */
uint16_t uft_lat_histogram_peak(const uft_track_latency_t *lat);

/*===========================================================================
 * API: Multi-Revolution
 *===========================================================================*/

/**
 * @brief Merge latency profiles from multiple revolutions
 * @param dst Destination profile
 * @param src Source profiles array
 * @param count Number of source profiles
 * @return 0 on success
 */
int uft_lat_merge_revolutions(uft_track_latency_t *dst,
                              const uft_track_latency_t **src,
                              size_t count);

/**
 * @brief Calculate per-bit variance across revolutions
 * @param profiles Array of track profiles
 * @param count Number of profiles
 * @param bit_index Bit position
 * @return Variance in nanoseconds squared
 */
float uft_lat_revolution_variance(const uft_track_latency_t **profiles,
                                  size_t count,
                                  uint32_t bit_index);

/*===========================================================================
 * API: Export/Report
 *===========================================================================*/

/**
 * @brief Export latency data to JSON
 * @param lat Track latency profile
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written, or -1 on error
 */
int uft_lat_export_json(const uft_track_latency_t *lat,
                        char *buffer, size_t size);

/**
 * @brief Generate human-readable timing report
 * @param lat Track latency profile
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes written
 */
int uft_lat_report(const uft_track_latency_t *lat,
                   char *buffer, size_t size);

/**
 * @brief Export timing for protection reconstruction
 * @param lat Track latency profile
 * @param density_map Output: density ratio per bit
 * @param count Maximum entries
 * @return Number of entries written
 */
int uft_lat_export_density_map(const uft_track_latency_t *lat,
                               float *density_map,
                               size_t count);

/*===========================================================================
 * API: Utilities
 *===========================================================================*/

/**
 * @brief Get protection type name
 */
const char *uft_lat_protection_name(uft_latency_protection_t prot);

/**
 * @brief Get latency type name
 */
const char *uft_lat_type_name(uft_latency_type_t type);

/**
 * @brief Get default configuration
 */
void uft_lat_get_default_config(uft_latency_config_t *config);

/**
 * @brief Calculate expected bit-cell time for encoding
 * @param encoding Encoding type (from uft_ir_encoding_t)
 * @param density Density (DD/HD/ED)
 * @return Expected nanoseconds per bit-cell
 */
uint16_t uft_lat_nominal_timing(uint8_t encoding, uint8_t density);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LATENCY_TRACKING_H */

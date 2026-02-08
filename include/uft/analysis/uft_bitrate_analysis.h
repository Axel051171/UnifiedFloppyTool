/**
 * @file uft_bitrate_analysis.h
 * @brief Bitrate Analysis for Flux Data (SCP, Kryoflux, etc.)
 * 
 * Software-level implementation of bitrate analysis concepts from nibtools IHS.
 * Works with flux timing data instead of requiring SuperCard+ hardware.
 * 
 * Features:
 * - Index hole timing analysis
 * - Bitrate statistics per track
 * - Track alignment reporting
 * - Density zone detection
 * - RPM calculation
 * 
 * Reference: nibtools ihs.c by Pete Rittwage
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_BITRATE_ANALYSIS_H
#define UFT_BITRATE_ANALYSIS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Standard 1541 clock frequency (16 MHz / 16 = 1 MHz) */
#define BITRATE_1541_CLOCK          1000000

/** Standard disk rotation speed (300 RPM) */
#define BITRATE_STANDARD_RPM        300

/** Microseconds per revolution at 300 RPM */
#define BITRATE_US_PER_REV          200000

/** Sample rate for SCP files (25 MHz) */
#define BITRATE_SCP_SAMPLE_RATE     25000000

/** Sample rate for Kryoflux (with index reference) */
#define BITRATE_KRYOFLUX_SCK        ((18432000 * 73) / 14 / 2)

/** Nanoseconds per SCP sample tick (40ns) */
#define BITRATE_SCP_NS_PER_TICK     40

/** Maximum bitrate zones */
#define BITRATE_MAX_ZONES           16

/** Bitrate tolerance percentage */
#define BITRATE_TOLERANCE_PCT       5

/* ============================================================================
 * Standard Bitrates for C64/1541
 * ============================================================================ */

/** Density 0 bitrate (tracks 31-42): ~250 kbit/s */
#define BITRATE_DENSITY_0           250000

/** Density 1 bitrate (tracks 25-30): ~266 kbit/s */
#define BITRATE_DENSITY_1           266667

/** Density 2 bitrate (tracks 18-24): ~285 kbit/s */
#define BITRATE_DENSITY_2           285714

/** Density 3 bitrate (tracks 1-17): ~307 kbit/s */
#define BITRATE_DENSITY_3           307692

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Index hole timing information
 */
typedef struct {
    uint32_t    index_time;         /**< Time of index hole (ns) */
    uint32_t    revolution_time;    /**< Time for one revolution (ns) */
    float       rpm;                /**< Calculated RPM */
    float       rpm_variation;      /**< RPM variation percentage */
    bool        index_detected;     /**< Index hole was detected */
} index_info_t;

/**
 * @brief Bitrate zone information
 */
typedef struct {
    uint32_t    start_pos;          /**< Start position in track */
    uint32_t    end_pos;            /**< End position in track */
    uint32_t    bitrate;            /**< Bitrate in bits/second */
    uint8_t     density;            /**< Detected density (0-3) */
    float       cell_time_ns;       /**< Bit cell time in nanoseconds */
} bitrate_zone_t;

/**
 * @brief Bitrate statistics for a track
 */
typedef struct {
    uint32_t    track;              /**< Track number */
    uint32_t    halftrack;          /**< Halftrack number */
    
    /* Overall statistics */
    uint32_t    avg_bitrate;        /**< Average bitrate */
    uint32_t    min_bitrate;        /**< Minimum bitrate */
    uint32_t    max_bitrate;        /**< Maximum bitrate */
    float       bitrate_std_dev;    /**< Standard deviation */
    
    /* Timing */
    uint32_t    total_bits;         /**< Total bits in track */
    uint32_t    total_time_ns;      /**< Total time in nanoseconds */
    float       rpm;                /**< Calculated RPM */
    
    /* Density detection */
    uint8_t     detected_density;   /**< Most likely density (0-3) */
    float       density_confidence; /**< Confidence in detection (0-100) */
    
    /* Zone analysis */
    int         num_zones;          /**< Number of bitrate zones */
    bitrate_zone_t zones[BITRATE_MAX_ZONES]; /**< Zone details */
    
    /* Sync analysis */
    int         sync_count;         /**< Number of syncs found */
    uint32_t    avg_sync_bitrate;   /**< Average bitrate at sync marks */
    
    /* Quality indicators */
    bool        stable;             /**< Bitrate is stable */
    bool        valid;              /**< Track data appears valid */
    float       quality_score;      /**< Overall quality (0-100) */
} bitrate_stats_t;

/**
 * @brief Deep bitrate analysis result
 */
typedef struct {
    int                 num_tracks;         /**< Number of tracks analyzed */
    bitrate_stats_t     tracks[84];         /**< Per-track statistics */
    
    /* Disk-level statistics */
    float               avg_rpm;            /**< Average RPM */
    float               rpm_stability;      /**< RPM stability (0-100) */
    uint32_t            avg_bitrate;        /**< Average bitrate across disk */
    
    /* Protection indicators */
    bool                variable_density;   /**< Variable density detected */
    bool                non_standard_rpm;   /**< Non-standard RPM detected */
    int                 weak_bit_tracks;    /**< Tracks with weak bits */
    
    /* Report */
    char                summary[512];       /**< Human-readable summary */
} deep_analysis_t;

/**
 * @brief Flux data source type
 */
typedef enum {
    FLUX_SOURCE_UNKNOWN = 0,
    FLUX_SOURCE_SCP,            /**< SuperCard Pro */
    FLUX_SOURCE_KRYOFLUX,       /**< Kryoflux */
    FLUX_SOURCE_HFE,            /**< HxC Floppy Emulator */
    FLUX_SOURCE_RAW,            /**< Raw flux data */
} flux_source_t;

/* ============================================================================
 * API Functions - Bitrate Calculation
 * ============================================================================ */

/**
 * @brief Calculate bitrate from flux timing data
 * @param flux_data Flux timing data (array of time values)
 * @param num_samples Number of flux samples
 * @param sample_rate Sample rate in Hz (e.g., 25MHz for SCP)
 * @return Average bitrate in bits/second
 */
uint32_t bitrate_from_flux(const uint32_t *flux_data, size_t num_samples, 
                           uint32_t sample_rate);

/**
 * @brief Calculate bitrate from raw track data
 * @param track_data Raw GCR track data
 * @param track_length Track length in bytes
 * @param revolution_time_ns Revolution time in nanoseconds
 * @return Bitrate in bits/second
 */
uint32_t bitrate_from_gcr(const uint8_t *track_data, size_t track_length,
                          uint32_t revolution_time_ns);

/**
 * @brief Get expected bitrate for track
 * @param track Track number (1-42)
 * @return Expected bitrate in bits/second
 */
uint32_t bitrate_expected(int track);

/**
 * @brief Get density from bitrate
 * @param bitrate Bitrate in bits/second
 * @return Density (0-3) or -1 if unknown
 */
int bitrate_to_density(uint32_t bitrate);

/**
 * @brief Get bitrate for density
 * @param density Density (0-3)
 * @return Nominal bitrate in bits/second
 */
uint32_t density_to_bitrate(int density);

/* ============================================================================
 * API Functions - Index Hole Analysis
 * ============================================================================ */

/**
 * @brief Analyze index hole timing
 * @param flux_data Flux timing data
 * @param num_samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @param result Output index information
 * @return true if index hole detected
 */
bool analyze_index_timing(const uint32_t *flux_data, size_t num_samples,
                          uint32_t sample_rate, index_info_t *result);

/**
 * @brief Calculate RPM from revolution time
 * @param revolution_time_ns Revolution time in nanoseconds
 * @return RPM value
 */
float calculate_rpm(uint32_t revolution_time_ns);

/**
 * @brief Check if RPM is within standard tolerance
 * @param rpm Measured RPM
 * @return true if within ±3% of 300 RPM
 */
bool rpm_is_standard(float rpm);

/* ============================================================================
 * API Functions - Track Analysis
 * ============================================================================ */

/**
 * @brief Analyze bitrate statistics for a track
 * @param flux_data Flux timing data
 * @param num_samples Number of samples
 * @param sample_rate Sample rate in Hz
 * @param track Track number
 * @param stats Output statistics
 * @return 0 on success, error code otherwise
 */
int analyze_track_bitrate(const uint32_t *flux_data, size_t num_samples,
                          uint32_t sample_rate, int track,
                          bitrate_stats_t *stats);

/**
 * @brief Analyze bitrate from GCR track data
 * @param gcr_data GCR track data
 * @param gcr_length Track length
 * @param revolution_time_ns Revolution time
 * @param track Track number
 * @param stats Output statistics
 * @return 0 on success, error code otherwise
 */
int analyze_gcr_bitrate(const uint8_t *gcr_data, size_t gcr_length,
                        uint32_t revolution_time_ns, int track,
                        bitrate_stats_t *stats);

/**
 * @brief Detect bitrate zones in track
 * @param flux_data Flux timing data
 * @param num_samples Number of samples
 * @param sample_rate Sample rate
 * @param zones Output zone array
 * @param max_zones Maximum zones to detect
 * @return Number of zones found
 */
int detect_bitrate_zones(const uint32_t *flux_data, size_t num_samples,
                         uint32_t sample_rate, bitrate_zone_t *zones,
                         int max_zones);

/* ============================================================================
 * API Functions - Deep Analysis
 * ============================================================================ */

/**
 * @brief Perform deep bitrate analysis on disk image
 * @param track_data Array of track data pointers
 * @param track_lengths Array of track lengths
 * @param track_times Array of revolution times (ns, can be NULL for estimate)
 * @param num_tracks Number of tracks
 * @param source Flux data source type
 * @param result Output analysis result
 * @return 0 on success, error code otherwise
 */
int deep_bitrate_analysis(const uint8_t **track_data, const size_t *track_lengths,
                          const uint32_t *track_times, int num_tracks,
                          flux_source_t source, deep_analysis_t *result);

/**
 * @brief Generate bitrate analysis report
 * @param analysis Analysis result
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Length of report string
 */
size_t generate_bitrate_report(const deep_analysis_t *analysis,
                               char *buffer, size_t buffer_size);

/**
 * @brief Generate track alignment report
 * @param stats Track statistics
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Length of report string
 */
size_t generate_track_report(const bitrate_stats_t *stats,
                             char *buffer, size_t buffer_size);

/* ============================================================================
 * API Functions - Sync Analysis
 * ============================================================================ */

/**
 * @brief Analyze sync bitrate in track
 * @param gcr_data GCR track data
 * @param gcr_length Track length
 * @param revolution_time_ns Revolution time
 * @param sync_bitrate Output: average sync bitrate
 * @param sync_count Output: number of syncs
 * @return 0 on success
 */
int analyze_sync_bitrate(const uint8_t *gcr_data, size_t gcr_length,
                         uint32_t revolution_time_ns,
                         uint32_t *sync_bitrate, int *sync_count);

/**
 * @brief Check for variable density protection
 * @param stats Track statistics
 * @return true if variable density detected
 */
bool detect_variable_density(const bitrate_stats_t *stats);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get flux source name
 * @param source Flux source type
 * @return Human-readable name
 */
const char *flux_source_name(flux_source_t source);

/**
 * @brief Convert SCP timing to nanoseconds
 * @param scp_ticks SCP timing ticks
 * @return Time in nanoseconds
 */
uint32_t scp_ticks_to_ns(uint32_t scp_ticks);

/**
 * @brief Convert Kryoflux timing to nanoseconds
 * @param kf_sample Kryoflux sample value
 * @return Time in nanoseconds
 */
uint32_t kryoflux_to_ns(uint32_t kf_sample);

/**
 * @brief Estimate revolution time from track data
 * @param track Track number
 * @param track_length Track length in bytes
 * @return Estimated revolution time in nanoseconds
 */
uint32_t estimate_revolution_time(int track, size_t track_length);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BITRATE_ANALYSIS_H */

/**
 * @file uft_kryoflux.h
 * @brief KryoFlux Stream File Parser
 * 
 * Source: Aufit (Atari Universal FD Image Tools)
 * Author: Jean Louis-Guerin / Software Preservation Society
 * License: GPL-2.0+
 * 
 * Parses KryoFlux .raw stream files to extract flux timing data.
 */

#ifndef UFT_KRYOFLUX_H
#define UFT_KRYOFLUX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Default sample clock frequency (Hz) */
#define UFT_KF_SAMPLE_CLOCK  24027428.5714285

/** Index clock frequency (sample clock / 8) */
#define UFT_KF_INDEX_CLOCK   (UFT_KF_SAMPLE_CLOCK / 8.0)

/** Maximum flux values per track (typical ~50000) */
#define UFT_KF_MAX_FLUX      200000

/** Maximum indexes per track */
#define UFT_KF_MAX_INDEX     16

/*===========================================================================
 * Stream Block Types
 *===========================================================================*/

/** Block type codes */
typedef enum {
    UFT_KF_FLUX1_MIN    = 0x00,  /**< Flux1 range start */
    UFT_KF_FLUX1_MAX    = 0x07,  /**< Flux1 range end */
    UFT_KF_FLUX2        = 0x08,  /**< 2-byte flux value */
    UFT_KF_FLUX3        = 0x09,  /**< 3-byte flux value */
    UFT_KF_OVERFLOW     = 0x0A,  /**< Add 65536 to next flux */
    UFT_KF_FLUX3_ALT    = 0x0B,  /**< Alternative flux3 */
    UFT_KF_NOP1         = 0x0C,  /**< Skip 1 byte */
    UFT_KF_OOB          = 0x0D,  /**< Out-of-band block */
    UFT_KF_NOP3         = 0x0E,  /**< Skip 3 bytes */
} uft_kf_block_type_t;

/** OOB sub-types */
typedef enum {
    UFT_KF_OOB_INVALID      = 0x00,
    UFT_KF_OOB_STREAM_INFO  = 0x01,
    UFT_KF_OOB_INDEX        = 0x02,
    UFT_KF_OOB_STREAM_END   = 0x03,
    UFT_KF_OOB_KF_INFO      = 0x04,
    UFT_KF_OOB_EOF          = 0x0D,
} uft_kf_oob_type_t;

/** Stream end result codes */
typedef enum {
    UFT_KF_RESULT_OK        = 0x00,
    UFT_KF_RESULT_BUFFERING = 0x01,
    UFT_KF_RESULT_NO_INDEX  = 0x02,
} uft_kf_result_t;

/*===========================================================================
 * Decode Status
 *===========================================================================*/

/** Stream decode status */
typedef enum {
    UFT_KF_STATUS_OK            = 0,
    UFT_KF_STATUS_MISSING_DATA  = -1,
    UFT_KF_STATUS_INVALID_CODE  = -2,
    UFT_KF_STATUS_WRONG_POS     = -3,
    UFT_KF_STATUS_DEV_BUFFER    = -4,
    UFT_KF_STATUS_DEV_INDEX     = -5,
    UFT_KF_STATUS_TRANSFER      = -6,
    UFT_KF_STATUS_INVALID_OOB   = -7,
    UFT_KF_STATUS_MISSING_END   = -8,
    UFT_KF_STATUS_INDEX_REF     = -9,
    UFT_KF_STATUS_MISSING_INDEX = -10,
    UFT_KF_STATUS_READ_ERROR    = -11,
} uft_kf_status_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Index signal information
 */
typedef struct {
    uint32_t flux_position;     /**< Index in flux array */
    uint32_t rotation_time;     /**< Sample clocks since last index */
    uint32_t pre_index_time;    /**< Clocks before index in flux cell */
} uft_kf_index_t;

/**
 * @brief Internal index data (during parsing)
 */
typedef struct {
    uint32_t stream_pos;        /**< Position in stream buffer */
    uint32_t sample_counter;    /**< Sample counter at index */
    uint32_t index_counter;     /**< Index counter value */
} uft_kf_index_internal_t;

/**
 * @brief Stream statistics
 */
typedef struct {
    double avg_rpm;             /**< Average RPM */
    double max_rpm;             /**< Maximum RPM */
    double min_rpm;             /**< Minimum RPM */
    double avg_bps;             /**< Average transfer rate (bytes/sec) */
    uint32_t flux_per_rev;      /**< Average flux per revolution */
    uint32_t min_flux;          /**< Minimum flux value */
    uint32_t max_flux;          /**< Maximum flux value */
} uft_kf_stats_t;

/**
 * @brief KryoFlux stream reader context
 */
typedef struct {
    /* Flux data */
    uint32_t *flux_values;      /**< Array of flux timing values */
    uint32_t *flux_positions;   /**< Stream positions of each flux */
    uint32_t flux_count;        /**< Number of flux values */
    uint32_t flux_capacity;     /**< Allocated capacity */
    
    /* Index data */
    uft_kf_index_t *indexes;    /**< Decoded index information */
    uft_kf_index_internal_t *index_internal;
    uint32_t index_count;       /**< Number of indexes */
    
    /* Hardware info */
    char info_string[256];      /**< KryoFlux info string */
    double sample_clock;        /**< Sample clock frequency */
    double index_clock;         /**< Index clock frequency */
    
    /* Statistics */
    uft_kf_stats_t stats;
    uint32_t data_count;        /**< Transfer data bytes */
    uint32_t data_time;         /**< Transfer time (ms) */
} uft_kf_stream_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize KryoFlux stream context
 * @param stream Context to initialize
 * @return UFT_KF_STATUS_OK on success
 */
uft_kf_status_t uft_kf_init(uft_kf_stream_t *stream);

/**
 * @brief Free KryoFlux stream context resources
 * @param stream Context to free
 */
void uft_kf_free(uft_kf_stream_t *stream);

/**
 * @brief Reset context for reuse
 * @param stream Context to reset
 */
void uft_kf_reset(uft_kf_stream_t *stream);

/**
 * @brief Decode KryoFlux stream from buffer
 * @param stream Context to store results
 * @param data Raw stream data
 * @param len Data length in bytes
 * @return Status code
 */
uft_kf_status_t uft_kf_decode(uft_kf_stream_t *stream,
                              const uint8_t *data, size_t len);

/**
 * @brief Decode KryoFlux stream from file
 * @param stream Context to store results
 * @param filename Path to .raw file
 * @return Status code
 */
uft_kf_status_t uft_kf_decode_file(uft_kf_stream_t *stream,
                                   const char *filename);

/**
 * @brief Calculate stream statistics
 * @param stream Decoded stream context
 */
void uft_kf_calc_stats(uft_kf_stream_t *stream);

/**
 * @brief Convert flux value to microseconds
 * @param stream Stream context (for clock rate)
 * @param flux Flux value in sample clocks
 * @return Time in microseconds
 */
static inline double uft_kf_flux_to_us(const uft_kf_stream_t *stream,
                                       uint32_t flux) {
    return (double)flux * 1000000.0 / stream->sample_clock;
}

/**
 * @brief Convert flux value to nanoseconds
 */
static inline double uft_kf_flux_to_ns(const uft_kf_stream_t *stream,
                                       uint32_t flux) {
    return (double)flux * 1000000000.0 / stream->sample_clock;
}

/**
 * @brief Get revolution time in milliseconds
 * @param stream Stream context
 * @param index_num Revolution number (1 = first complete)
 * @return Revolution time in ms, or 0 if invalid
 */
static inline double uft_kf_revolution_time_ms(const uft_kf_stream_t *stream,
                                               uint32_t index_num) {
    if (index_num == 0 || index_num >= stream->index_count) {
        return 0.0;
    }
    return (double)stream->indexes[index_num].rotation_time * 1000.0 
           / stream->sample_clock;
}

/**
 * @brief Get RPM for a revolution
 */
static inline double uft_kf_revolution_rpm(const uft_kf_stream_t *stream,
                                           uint32_t index_num) {
    double time_ms = uft_kf_revolution_time_ms(stream, index_num);
    if (time_ms <= 0) return 0.0;
    return 60000.0 / time_ms;
}

/*===========================================================================
 * Inline Decoder Implementation
 *===========================================================================*/

/**
 * @brief Extract little-endian 16-bit value
 */
static inline uint16_t uft_kf_read_u16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Extract little-endian 32-bit value
 */
static inline uint32_t uft_kf_read_u32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/*===========================================================================
 * Histogram Analysis
 *===========================================================================*/

/**
 * @brief Build flux timing histogram
 * @param stream Decoded stream
 * @param histogram Output array (must be >= max_flux+1)
 * @param max_value Maximum value to track
 */
void uft_kf_build_histogram(const uft_kf_stream_t *stream,
                            uint32_t *histogram, uint32_t max_value);

/**
 * @brief Find peaks in histogram (bit cell boundaries)
 * @param histogram Histogram data
 * @param len Histogram length
 * @param peaks Output array for peak positions
 * @param max_peaks Maximum peaks to find
 * @return Number of peaks found
 */
int uft_kf_find_histogram_peaks(const uint32_t *histogram, size_t len,
                                uint32_t *peaks, int max_peaks);

#ifdef __cplusplus
}
#endif

#endif /* UFT_KRYOFLUX_H */

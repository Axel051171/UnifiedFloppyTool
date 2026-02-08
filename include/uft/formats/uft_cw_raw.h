/**
 * @file uft_cw_raw.h
 * @brief Catweasel Raw Flux Format Support
 * 
 * Support for raw Catweasel flux data as documented in catweasel-cw.
 * The Catweasel controller outputs raw timing values representing
 * flux transitions on the disk surface.
 * 
 * Format characteristics:
 * - 8-bit timing values at various sample rates
 * - No header, pure flux data
 * - MK3/MK4 have different sample rates
 * - Data can be per-track or full-disk
 * 
 * Based on qbarnes/catweasel-cw documentation.
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#ifndef UFT_CW_RAW_H
#define UFT_CW_RAW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Catweasel MK3 sample clock (14.16168 MHz) */
#define UFT_CW_MK3_CLOCK        14161680

/** Catweasel MK4 sample clock (28.32336 MHz) */
#define UFT_CW_MK4_CLOCK        28323360

/** Default sample clock (MK4) */
#define UFT_CW_DEFAULT_CLOCK    UFT_CW_MK4_CLOCK

/** Maximum track length in bytes */
#define UFT_CW_MAX_TRACK_LEN    65536

/** Overflow marker in raw data */
#define UFT_CW_OVERFLOW         0x00

/*===========================================================================
 * Types
 *===========================================================================*/

/**
 * @brief Catweasel controller model
 */
typedef enum {
    UFT_CW_MODEL_UNKNOWN = 0,
    UFT_CW_MODEL_MK3,           /**< Catweasel MK3 (PCI) */
    UFT_CW_MODEL_MK4,           /**< Catweasel MK4 (PCI) */
    UFT_CW_MODEL_MK4PLUS        /**< Catweasel MK4+ (PCI) */
} uft_cw_model_t;

/**
 * @brief Catweasel raw track data
 */
typedef struct {
    uint8_t *data;              /**< Raw flux data */
    size_t   length;            /**< Data length in bytes */
    int      cylinder;          /**< Physical cylinder */
    int      head;              /**< Physical head (0 or 1) */
    uint32_t sample_clock;      /**< Sample clock in Hz */
    uint32_t index_pos;         /**< Position of index pulse (if any) */
    bool     has_index;         /**< True if index position valid */
} uft_cw_track_t;

/**
 * @brief Catweasel raw disk image
 */
typedef struct {
    uft_cw_model_t model;       /**< Controller model */
    uint32_t sample_clock;      /**< Sample clock in Hz */
    int      cylinders;         /**< Number of cylinders */
    int      heads;             /**< Number of heads (1 or 2) */
    uft_cw_track_t *tracks;     /**< Track data array */
    int      track_count;       /**< Number of tracks */
} uft_cw_image_t;

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

/**
 * @brief Create empty Catweasel image
 */
uft_cw_image_t* uft_cw_create(int cylinders, int heads, uft_cw_model_t model);

/**
 * @brief Free Catweasel image
 */
void uft_cw_free(uft_cw_image_t *img);

/**
 * @brief Initialize track
 */
int uft_cw_track_init(uft_cw_track_t *track, int cyl, int head, size_t max_len);

/**
 * @brief Free track data
 */
void uft_cw_track_free(uft_cw_track_t *track);

/*===========================================================================
 * I/O
 *===========================================================================*/

/**
 * @brief Read raw track file
 * 
 * Reads a single track of raw Catweasel data.
 * 
 * @param filename  Path to raw track file
 * @param track     Track structure to fill
 * @return 0 on success, negative on error
 */
int uft_cw_read_track(const char *filename, uft_cw_track_t *track);

/**
 * @brief Write raw track file
 */
int uft_cw_write_track(const char *filename, const uft_cw_track_t *track);

/**
 * @brief Read multi-track raw file
 * 
 * Some tools produce a single file with all tracks concatenated.
 * This function attempts to detect track boundaries from index pulses.
 * 
 * @param filename    Path to raw file
 * @param img         Image to fill
 * @param tracks      Expected number of tracks (0 for auto-detect)
 * @return 0 on success, negative on error
 */
int uft_cw_read_image(const char *filename, uft_cw_image_t *img, int tracks);

/**
 * @brief Write multi-track raw file
 */
int uft_cw_write_image(const char *filename, const uft_cw_image_t *img);

/*===========================================================================
 * Conversion
 *===========================================================================*/

/**
 * @brief Convert raw timing to flux transitions
 * 
 * Converts 8-bit Catweasel timing values to absolute flux positions.
 * Handles overflow markers (0x00 = add 256 to next value).
 * 
 * @param raw         Raw Catweasel data
 * @param raw_len     Length of raw data
 * @param flux        Output: flux transition times (in sample clocks)
 * @param flux_count  Output: number of flux transitions
 * @param max_flux    Maximum flux values to output
 * @return 0 on success
 */
int uft_cw_raw_to_flux(const uint8_t *raw, size_t raw_len,
                       uint32_t *flux, size_t *flux_count, size_t max_flux);

/**
 * @brief Convert flux transitions to raw timing
 * 
 * @param flux        Flux transition times
 * @param flux_count  Number of transitions
 * @param raw         Output: raw Catweasel data
 * @param raw_len     Output: length of raw data
 * @param max_raw     Maximum raw bytes to output
 * @return 0 on success
 */
int uft_cw_flux_to_raw(const uint32_t *flux, size_t flux_count,
                       uint8_t *raw, size_t *raw_len, size_t max_raw);

/**
 * @brief Convert sample clock timing to nanoseconds
 */
static inline double uft_cw_to_ns(uint32_t ticks, uint32_t clock) {
    return (double)ticks * 1000000000.0 / clock;
}

/**
 * @brief Convert nanoseconds to sample clock timing
 */
static inline uint32_t uft_cw_from_ns(double ns, uint32_t clock) {
    return (uint32_t)(ns * clock / 1000000000.0 + 0.5);
}

/*===========================================================================
 * Analysis
 *===========================================================================*/

/**
 * @brief Detect controller model from timing data
 * 
 * Analyzes flux timing to determine if data is from MK3 or MK4.
 * MK4 has twice the sample rate of MK3.
 */
uft_cw_model_t uft_cw_detect_model(const uft_cw_track_t *track);

/**
 * @brief Find index pulse position in raw data
 * 
 * Catweasel can capture index pulse timing. This function
 * attempts to find the index marker in the data.
 * 
 * @param track  Track to analyze
 * @return Position of index, or 0 if not found
 */
uint32_t uft_cw_find_index(const uft_cw_track_t *track);

/**
 * @brief Calculate track rotation time
 * 
 * @param track  Track data
 * @return Rotation time in microseconds
 */
double uft_cw_rotation_time(const uft_cw_track_t *track);

/**
 * @brief Estimate data rate from flux timing
 * 
 * @param track  Track data
 * @return Estimated data rate in bits per second
 */
uint32_t uft_cw_estimate_datarate(const uft_cw_track_t *track);

/*===========================================================================
 * Integration with UFT formats
 *===========================================================================*/

/**
 * @brief Convert Catweasel track to SCP revolution
 */
int uft_cw_track_to_scp(const uft_cw_track_t *track, 
                        uint16_t *scp_data, size_t *scp_len,
                        uint32_t scp_clock);

/**
 * @brief Convert SCP revolution to Catweasel track
 */
int uft_cw_track_from_scp(uft_cw_track_t *track,
                          const uint16_t *scp_data, size_t scp_len,
                          uint32_t scp_clock);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CW_RAW_H */

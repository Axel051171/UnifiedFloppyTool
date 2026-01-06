/**
 * @file uft_flux.h
 * @brief Raw flux data analysis and processing
 * 
 * This module provides tools for analyzing raw magnetic flux data
 * from floppy disks, as captured by devices like Greaseweazle,
 * FluxEngine, KryoFlux, or SuperCard Pro.
 * 
 * Flux data represents the actual magnetic transitions on the disk,
 * allowing for:
 * - Preservation of copy-protected disks
 * - Analysis of unusual formats
 * - Recovery of damaged data
 * - Forensic disk imaging
 */

#ifndef UFT_FLUX_H
#define UFT_FLUX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Sample clock frequencies (Hz) */
#define UFT_FLUX_CLOCK_GREASEWEAZLE  24000000    /**< Greaseweazle */
#define UFT_FLUX_CLOCK_KRYOFLUX      24027428    /**< KryoFlux */
#define UFT_FLUX_CLOCK_SCP           40000000    /**< SuperCard Pro */
#define UFT_FLUX_CLOCK_FLUXENGINE    72000000    /**< FluxEngine */

/** Standard bit cell times (in nanoseconds) */
#define UFT_FLUX_BITCELL_DD          4000        /**< DD: 250 kbps */
#define UFT_FLUX_BITCELL_HD          2000        /**< HD: 500 kbps */
#define UFT_FLUX_BITCELL_ED          1000        /**< ED: 1000 kbps */

/** MFM timing windows */
#define UFT_MFM_WINDOW_SHORT         1.0f        /**< 1T (01 pattern) */
#define UFT_MFM_WINDOW_MEDIUM        1.5f        /**< 1.5T (001 pattern) */
#define UFT_MFM_WINDOW_LONG          2.0f        /**< 2T (0001 pattern) */

/** Error codes */
typedef enum {
    UFT_FLUX_OK = 0,
    UFT_FLUX_ERR_NO_FLUX,           /**< No flux transitions found */
    UFT_FLUX_ERR_NO_INDEX,          /**< No index pulse found */
    UFT_FLUX_ERR_PLL_FAIL,          /**< PLL failed to lock */
    UFT_FLUX_ERR_DECODE_FAIL,       /**< Decoding failed */
    UFT_FLUX_ERR_BUFFER_TOO_SMALL,  /**< Output buffer too small */
    UFT_FLUX_ERR_INVALID_PARAM,     /**< Invalid parameter */
} uft_flux_error_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Flux sample (time between transitions)
 */
typedef uint32_t uft_flux_sample_t;

/**
 * @brief Raw flux track data
 */
typedef struct {
    uft_flux_sample_t *samples;     /**< Array of flux samples */
    size_t sample_count;            /**< Number of samples */
    uint32_t sample_clock;          /**< Sample clock frequency (Hz) */
    uint32_t index_offset;          /**< Sample offset of index pulse */
    bool has_index;                 /**< Index pulse present */
} uft_flux_track_t;

/**
 * @brief PLL (Phase-Locked Loop) state
 */
typedef struct {
    double clock;                   /**< Current clock period (samples) */
    double phase;                   /**< Current phase (samples) */
    double window;                  /**< Detection window size */
    
    /* PLL tuning parameters */
    double freq_gain;               /**< Frequency adjustment gain */
    double phase_gain;              /**< Phase adjustment gain */
    
    /* Statistics */
    uint32_t total_bits;            /**< Total bits decoded */
    uint32_t errors;                /**< PLL errors (out of window) */
} uft_pll_t;

/**
 * @brief Decoded bit stream
 */
typedef struct {
    uint8_t *data;                  /**< Decoded data bytes */
    size_t data_len;                /**< Length of data */
    size_t bit_count;               /**< Total bits (may not be byte-aligned) */
    
    /* Quality metrics */
    float avg_clock;                /**< Average clock period */
    float clock_variance;           /**< Clock variance */
    uint32_t weak_bits;             /**< Number of weak bits */
} uft_decoded_track_t;

/* ============================================================================
 * Flux Track Operations
 * ============================================================================ */

/**
 * @brief Create a flux track structure
 * 
 * @param capacity Initial sample capacity
 * @return Allocated track, or NULL on failure
 */
uft_flux_track_t* uft_flux_track_create(size_t capacity);

/**
 * @brief Free a flux track structure
 * 
 * @param track Track to free
 */
void uft_flux_track_free(uft_flux_track_t *track);

/**
 * @brief Add a flux sample to the track
 * 
 * @param track Track to modify
 * @param sample Sample to add (time since last transition)
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_track_add_sample(uft_flux_track_t *track,
                                            uft_flux_sample_t sample);

/**
 * @brief Find index pulse in flux data
 * 
 * @param track Track to search
 * @return Index of index pulse, or -1 if not found
 */
ssize_t uft_flux_find_index(const uft_flux_track_t *track);

/**
 * @brief Rotate track to start at index
 * 
 * @param track Track to rotate
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_rotate_to_index(uft_flux_track_t *track);

/* ============================================================================
 * PLL Operations
 * ============================================================================ */

/**
 * @brief Initialize PLL with given parameters
 * 
 * @param pll PLL structure to initialize
 * @param bit_cell Expected bit cell time (samples)
 * @param freq_gain Frequency gain (0.0-1.0, typically 0.01)
 * @param phase_gain Phase gain (0.0-1.0, typically 0.05)
 */
void uft_pll_init(uft_pll_t *pll, double bit_cell,
                  double freq_gain, double phase_gain);

/**
 * @brief Reset PLL to initial state
 * 
 * @param pll PLL to reset
 */
void uft_pll_reset(uft_pll_t *pll);

/**
 * @brief Process a flux transition through the PLL
 * 
 * @param pll PLL state
 * @param interval Time since last transition (samples)
 * @param bits Output: number of bits decoded (0, 1, 2, or 3)
 * @return true if transition was in expected window
 */
bool uft_pll_process(uft_pll_t *pll, double interval, int *bits);

/* ============================================================================
 * Decoding Operations
 * ============================================================================ */

/**
 * @brief Decode MFM flux data to bytes
 * 
 * Uses a software PLL to decode raw flux transitions into
 * MFM-encoded byte stream.
 * 
 * @param track Flux track to decode
 * @param output Output structure for decoded data
 * @param bit_cell Expected bit cell time (samples)
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_decode_mfm(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output,
    double bit_cell
);

/**
 * @brief Decode FM flux data to bytes
 * 
 * @param track Flux track to decode
 * @param output Output structure for decoded data
 * @param bit_cell Expected bit cell time (samples)
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_decode_fm(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output,
    double bit_cell
);

/**
 * @brief Decode GCR flux data (C64/Mac)
 * 
 * @param track Flux track to decode
 * @param output Output structure for decoded data
 * @param bit_cell Expected bit cell time (samples)
 * @param gcr_type GCR type (0 = C64, 1 = Mac)
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_decode_gcr(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output,
    double bit_cell,
    int gcr_type
);

/**
 * @brief Auto-detect encoding and decode
 * 
 * Attempts to automatically determine the encoding scheme
 * and decode the flux data.
 * 
 * @param track Flux track to decode
 * @param output Output structure for decoded data
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_decode_auto(
    const uft_flux_track_t *track,
    uft_decoded_track_t *output
);

/**
 * @brief Free decoded track data
 * 
 * @param decoded Decoded track to free
 */
void uft_decoded_track_free(uft_decoded_track_t *decoded);

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

/**
 * @brief Histogram entry for flux analysis
 */
typedef struct {
    uint32_t min_time;              /**< Minimum time (samples) */
    uint32_t max_time;              /**< Maximum time (samples) */
    uint32_t count;                 /**< Number of samples in bin */
} uft_flux_hist_bin_t;

/**
 * @brief Generate flux timing histogram
 * 
 * Creates a histogram of flux transition times for analysis.
 * 
 * @param track Track to analyze
 * @param bins Output array for histogram bins
 * @param bin_count Number of bins to generate
 * @param min_time Minimum time for histogram
 * @param max_time Maximum time for histogram
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_histogram(
    const uft_flux_track_t *track,
    uft_flux_hist_bin_t *bins,
    size_t bin_count,
    uint32_t min_time,
    uint32_t max_time
);

/**
 * @brief Estimate bit cell time from flux data
 * 
 * Analyzes flux timing histogram to estimate the bit cell period.
 * 
 * @param track Track to analyze
 * @param encoding Expected encoding (0=MFM, 1=FM, 2=GCR)
 * @return Estimated bit cell time in samples, or 0 on failure
 */
double uft_flux_estimate_bitcell(
    const uft_flux_track_t *track,
    int encoding
);

/**
 * @brief Calculate track rotation time
 * 
 * @param track Track with index pulse
 * @return Rotation time in microseconds, or 0 if no index
 */
double uft_flux_rotation_time(const uft_flux_track_t *track);

/**
 * @brief Calculate data rate from flux data
 * 
 * @param track Track to analyze
 * @return Data rate in bits per second
 */
uint32_t uft_flux_data_rate(const uft_flux_track_t *track);

/* ============================================================================
 * Multi-Revolution Analysis
 * ============================================================================ */

/**
 * @brief Compare multiple revolutions for weak bits
 * 
 * Compares flux data from multiple revolutions to find areas
 * that read differently (weak bits, copy protection).
 * 
 * @param revs Array of flux tracks (one per revolution)
 * @param rev_count Number of revolutions
 * @param tolerance Timing tolerance for matching
 * @param diff_map Output bitmap of differing positions
 * @param diff_map_len Length of diff_map in bytes
 * @return Number of differing positions found
 */
size_t uft_flux_compare_revolutions(
    const uft_flux_track_t **revs,
    size_t rev_count,
    double tolerance,
    uint8_t *diff_map,
    size_t diff_map_len
);

/**
 * @brief Merge multiple revolutions
 * 
 * Combines multiple readings of the same track to get
 * the best possible data recovery.
 * 
 * @param revs Array of flux tracks
 * @param rev_count Number of revolutions
 * @param output Output track (merged data)
 * @return UFT_FLUX_OK on success
 */
uft_flux_error_t uft_flux_merge_revolutions(
    const uft_flux_track_t **revs,
    size_t rev_count,
    uft_flux_track_t *output
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert sample time to nanoseconds
 * 
 * @param samples Sample count
 * @param clock Sample clock frequency (Hz)
 * @return Time in nanoseconds
 */
static inline double uft_flux_samples_to_ns(uint32_t samples, uint32_t clock) {
    return (double)samples * 1e9 / (double)clock;
}

/**
 * @brief Convert nanoseconds to sample count
 * 
 * @param ns Time in nanoseconds
 * @param clock Sample clock frequency (Hz)
 * @return Sample count
 */
static inline uint32_t uft_flux_ns_to_samples(double ns, uint32_t clock) {
    return (uint32_t)(ns * (double)clock / 1e9);
}

/**
 * @brief Get expected bit cell for data rate
 * 
 * @param data_rate Data rate in kbps (250, 300, 500, 1000)
 * @return Bit cell time in nanoseconds
 */
static inline uint32_t uft_flux_bitcell_ns(uint32_t data_rate) {
    return 1000000 / data_rate;  /* e.g., 1000000/250 = 4000 ns */
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_H */

/**
 * @file uft_flux_pll.h
 * @brief Flux Stream PLL (Phase-Locked Loop) for UFT
 * 
 * Implements a digital PLL for decoding flux transitions from raw stream data.
 * Based on analysis of HxCFloppyEmulator's fluxStreamAnalyzer.
 * 
 * The PLL consists of:
 * - Histogram-based bitcell detection (peak finding)
 * - Pump-charge phase correction
 * - Fast/slow correction ratios for jitter handling
 * - Multi-revolution analysis with confidence fusion
 * 
 * @copyright UFT Project
 */

#ifndef UFT_FLUX_PLL_H
#define UFT_FLUX_PLL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * PLL Constants
 *============================================================================*/

/** Default PLL parameters */
#define UFT_PLL_DEFAULT_TICK_FREQ       24000000    /**< 24 MHz tick frequency */
#define UFT_PLL_DEFAULT_MIN_MAX_PERCENT 18          /**< ±18% window */
#define UFT_PLL_DEFAULT_MAX_ERROR_NS    680         /**< Max error in ns */

/** Fast/slow correction ratios (numerator/denominator) */
#define UFT_PLL_FAST_CORRECTION_N       1
#define UFT_PLL_FAST_CORRECTION_D       2
#define UFT_PLL_SLOW_CORRECTION_N       3
#define UFT_PLL_SLOW_CORRECTION_D       4

/** Phase correction divisor */
#define UFT_PLL_PHASE_CORRECTION        8

/** Histogram size (65536 for 16-bit timing values) */
#define UFT_PLL_HISTOGRAM_SIZE          65536

/** Maximum detected peaks */
#define UFT_PLL_MAX_PEAKS               8

/** Block time for analysis (microseconds) */
#define UFT_PLL_BLOCK_TIME_US           1000

/** Maximum pulse skew (in 1/256 units) */
#define UFT_PLL_MAX_PULSE_SKEW          25

/*============================================================================
 * Encoding Types
 *============================================================================*/

typedef enum {
    UFT_ENCODING_UNKNOWN    = 0,
    UFT_ENCODING_FM         = 1,    /**< FM (single density) */
    UFT_ENCODING_MFM        = 2,    /**< MFM (double density) */
    UFT_ENCODING_M2FM       = 3,    /**< M2FM (modified MFM) */
    UFT_ENCODING_GCR        = 4,    /**< GCR (group code recording) */
    UFT_ENCODING_APPLE_GCR  = 5,    /**< Apple II GCR */
    UFT_ENCODING_C64_GCR    = 6     /**< Commodore GCR */
} uft_encoding_t;

/*============================================================================
 * Histogram and Peak Detection
 *============================================================================*/

/**
 * @brief Histogram statistics entry
 */
typedef struct {
    uint32_t value;         /**< Timing value */
    uint32_t count;         /**< Number of occurrences */
    float    percent;       /**< Percentage of total */
} uft_histo_entry_t;

/**
 * @brief Detected peak in histogram
 */
typedef struct {
    uint32_t center;        /**< Peak center value */
    uint32_t left;          /**< Left boundary */
    uint32_t right;         /**< Right boundary */
    uint32_t count;         /**< Total samples in peak */
    float    percent;       /**< Percentage of total samples */
    uint8_t  bit_count;     /**< Bits represented (1, 2, 3...) */
} uft_pll_peak_t;

/*============================================================================
 * PLL State Structure
 *============================================================================*/

/**
 * @brief PLL state and configuration
 */
typedef struct {
    /* Configuration */
    uint32_t tick_freq;             /**< Tick frequency (Hz) */
    uint32_t pivot;                 /**< Central bitcell timing (ticks) */
    uint32_t pll_min;               /**< Minimum valid timing */
    uint32_t pll_max;               /**< Maximum valid timing */
    uint8_t  pll_min_max_percent;   /**< Window percentage */
    
    /* Correction ratios (as 16.16 fixed point) */
    int32_t  fast_correction_ratio_n;
    int32_t  fast_correction_ratio_d;
    int32_t  slow_correction_ratio_n;
    int32_t  slow_correction_ratio_d;
    
    /* Error limits */
    float    max_pll_error_ticks;   /**< Maximum error in ticks */
    
    /* PLL state */
    int32_t  pump_charge;           /**< Current pump charge (phase error) */
    int32_t  phase;                 /**< Phase accumulator */
    int32_t  last_error;            /**< Last phase error */
    uint32_t last_pulse_phase;      /**< Phase of last pulse */
    
    /* Detection state */
    uft_encoding_t encoding;        /**< Detected encoding */
    uint8_t  num_peaks;             /**< Number of detected peaks */
    uft_pll_peak_t peaks[UFT_PLL_MAX_PEAKS];  /**< Detected peaks */
    
    /* Statistics */
    uint64_t total_pulses;          /**< Total pulses processed */
    uint64_t error_pulses;          /**< Pulses outside window */
    uint64_t sync_losses;           /**< Number of sync losses */
} uft_pll_state_t;

/*============================================================================
 * Flux Stream Structure
 *============================================================================*/

/**
 * @brief Flux stream data
 */
typedef struct {
    uint32_t* pulses;               /**< Array of pulse timings */
    size_t    num_pulses;           /**< Number of pulses */
    uint32_t  tick_freq;            /**< Tick frequency */
    
    /* Index markers */
    uint32_t* index_offsets;        /**< Pulse indices of index marks */
    size_t    num_indices;          /**< Number of index marks */
    
    /* Revolution data */
    size_t    current_revolution;   /**< Current revolution being processed */
} uft_flux_stream_t;

/*============================================================================
 * Decoded Track Structure
 *============================================================================*/

/**
 * @brief Decoded bitstream
 */
typedef struct {
    uint8_t* data;                  /**< Bit data (packed) */
    size_t   bit_length;            /**< Length in bits */
    size_t   byte_length;           /**< Length in bytes */
    
    /* Timing information */
    uint32_t* timing;               /**< Optional: timing per bit */
    
    /* Weak bit mask */
    uint8_t* weak_mask;             /**< Optional: weak bit positions */
    
    /* Track info */
    uint8_t  track;
    uint8_t  head;
    uint16_t rpm;
} uft_decoded_track_t;

/*============================================================================
 * PLL Initialization and Configuration
 *============================================================================*/

/**
 * @brief Initialize PLL with default parameters
 */
void uft_pll_init(uft_pll_state_t* pll);

/**
 * @brief Reset PLL state (keep configuration)
 */
void uft_pll_reset(uft_pll_state_t* pll);

/**
 * @brief Soft reset (partial reset for new track)
 */
void uft_pll_soft_reset(uft_pll_state_t* pll);

/**
 * @brief Configure PLL for specific bitrate
 * @param pll PLL state
 * @param bitrate_kbps Bitrate in kbps (250, 300, 500, etc.)
 * @param tick_freq Tick frequency in Hz
 */
void uft_pll_configure(uft_pll_state_t* pll, uint32_t bitrate_kbps, 
                       uint32_t tick_freq);

/**
 * @brief Set encoding type
 */
void uft_pll_set_encoding(uft_pll_state_t* pll, uft_encoding_t encoding);

/*============================================================================
 * Histogram and Peak Detection
 *============================================================================*/

/**
 * @brief Compute histogram of pulse timings
 * @param pulses Array of pulse timings
 * @param count Number of pulses
 * @param histogram Output histogram (must be UFT_PLL_HISTOGRAM_SIZE)
 */
void uft_pll_compute_histogram(const uint32_t* pulses, size_t count,
                                uint32_t* histogram);

/**
 * @brief Detect peaks in histogram
 * @param pll PLL state (will store detected peaks)
 * @param histogram Input histogram
 * @return Number of peaks detected
 */
int uft_pll_detect_peaks(uft_pll_state_t* pll, const uint32_t* histogram);

/**
 * @brief Auto-detect encoding from histogram
 * @param pll PLL state
 * @param histogram Input histogram
 * @return Detected encoding type
 */
uft_encoding_t uft_pll_detect_encoding(uft_pll_state_t* pll, 
                                        const uint32_t* histogram);

/*============================================================================
 * PLL Processing
 *============================================================================*/

/**
 * @brief Process single flux pulse
 * 
 * Returns the number of bitcells represented by this pulse.
 * Updates PLL state with phase correction.
 * 
 * @param pll PLL state
 * @param pulse Pulse timing value
 * @param bad_pulse Output: true if pulse outside window
 * @return Number of bitcells (0 = error, 1-4 typical)
 */
int uft_pll_process_pulse(uft_pll_state_t* pll, uint32_t pulse, 
                          bool* bad_pulse);

/**
 * @brief Process flux stream to bitstream
 * 
 * @param pll PLL state
 * @param stream Input flux stream
 * @param output Output decoded track
 * @return 0 on success
 */
int uft_pll_decode_stream(uft_pll_state_t* pll, 
                          const uft_flux_stream_t* stream,
                          uft_decoded_track_t* output);

/**
 * @brief Pre-sync PLL to stream
 * 
 * Processes initial portion of stream to stabilize PLL.
 * 
 * @param pll PLL state
 * @param pulses Pulse timing array
 * @param count Number of pulses
 * @param sync_pulses Number of pulses to use for sync
 */
void uft_pll_presync(uft_pll_state_t* pll, const uint32_t* pulses,
                     size_t count, size_t sync_pulses);

/*============================================================================
 * Multi-Revolution Processing
 *============================================================================*/

/**
 * @brief Revolution alignment info
 */
typedef struct {
    size_t   start_pulse;           /**< Starting pulse index */
    size_t   end_pulse;             /**< Ending pulse index */
    size_t   bit_length;            /**< Decoded bit length */
    uint32_t overlap_offset;        /**< Overlap with next revolution */
    uint32_t overlap_size;          /**< Size of overlap */
    float    confidence;            /**< Decode confidence (0-1) */
} uft_revolution_t;

/**
 * @brief Process multiple revolutions and merge
 * 
 * @param pll PLL state
 * @param stream Input flux stream with multiple revolutions
 * @param output Output merged track
 * @param revolutions Array to receive revolution info
 * @param max_revolutions Maximum revolutions to process
 * @return Number of revolutions processed
 */
int uft_pll_multi_revolution(uft_pll_state_t* pll,
                             const uft_flux_stream_t* stream,
                             uft_decoded_track_t* output,
                             uft_revolution_t* revolutions,
                             size_t max_revolutions);

/**
 * @brief Fuse multiple revolutions with confidence weighting
 * 
 * @param revs Array of decoded revolutions
 * @param num_revs Number of revolutions
 * @param output Output fused track
 * @return 0 on success
 */
int uft_pll_fuse_revolutions(const uft_decoded_track_t* revs, size_t num_revs,
                             uft_decoded_track_t* output);

/*============================================================================
 * Jitter Filtering
 *============================================================================*/

/**
 * @brief Apply jitter filter to flux stream
 * 
 * Smooths out timing variations while preserving signal.
 * 
 * @param pulses Input/output pulse array
 * @param count Number of pulses
 * @param window Filter window size
 */
void uft_pll_jitter_filter(uint32_t* pulses, size_t count, size_t window);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Convert nanoseconds to ticks
 */
static inline uint32_t uft_ns_to_ticks(uint32_t ns, uint32_t tick_freq)
{
    return (uint32_t)(((uint64_t)ns * tick_freq) / 1000000000ULL);
}

/**
 * @brief Convert ticks to nanoseconds
 */
static inline uint32_t uft_ticks_to_ns(uint32_t ticks, uint32_t tick_freq)
{
    return (uint32_t)(((uint64_t)ticks * 1000000000ULL) / tick_freq);
}

/**
 * @brief Get bitrate from bitcell timing
 */
static inline uint32_t uft_bitcell_to_kbps(uint32_t bitcell_ns)
{
    return 1000000 / bitcell_ns;
}

/**
 * @brief Get bitcell timing from bitrate
 */
static inline uint32_t uft_kbps_to_bitcell(uint32_t kbps)
{
    return 1000000 / kbps;
}

/**
 * @brief Print PLL statistics
 */
void uft_pll_print_stats(const uft_pll_state_t* pll);

/**
 * @brief Free decoded track resources
 */
void uft_decoded_track_free(uft_decoded_track_t* track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_PLL_H */

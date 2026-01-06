/**
 * @file uft_hxc_pll_enhanced.h
 * @brief Enhanced PLL algorithms from HxCFloppyEmulator
 * @version 1.0.0
 * @date 2026-01-06
 *
 * Source: HxCFloppyEmulator (Jean-François DEL NERO)
 *
 * Features:
 * - Automatic bitrate detection via histogram peaks
 * - Core PLL cell timing with phase correction
 * - Inter-band rejection for GCR and FM encodings
 * - Victor 9K variable speed band support
 */

#ifndef UFT_HXC_PLL_ENHANCED_H
#define UFT_HXC_PLL_ENHANCED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Histogram size (16-bit timing values) */
#define UFT_HXC_HISTOGRAM_SIZE      65536

/** Maximum band separators */
#define UFT_HXC_MAX_BANDS           16

/** Inter-band rejection modes */
#define UFT_HXC_BAND_NONE           0
#define UFT_HXC_BAND_GCR            1   /**< GCR encoding (reject 3,5 cells) */
#define UFT_HXC_BAND_FM             2   /**< FM encoding (force 2,4 cells) */

/*============================================================================
 * Encoding Types (from bitrate detection)
 *============================================================================*/

typedef enum {
    UFT_HXC_ENC_UNKNOWN     = 0,
    UFT_HXC_ENC_DD_250K     = 1,    /**< Double Density 250 kbit/s (PC DD) */
    UFT_HXC_ENC_DD_300K     = 2,    /**< Double Density 300 kbit/s (Amiga/Atari) */
    UFT_HXC_ENC_HD          = 3,    /**< High Density 500 kbit/s */
    UFT_HXC_ENC_ED          = 4,    /**< Extra Density 1 Mbit/s */
} uft_hxc_encoding_t;

/*============================================================================
 * PLL State Structure
 *============================================================================*/

/**
 * @brief HxC-style PLL state structure
 */
typedef struct {
    /* Core PLL state */
    int32_t pump_charge;            /**< Current cell size */
    int32_t phase;                  /**< Current window phase */
    int32_t lastpulsephase;         /**< Last pulse phase */
    int32_t last_error;             /**< Last PLL error */
    
    /* Cell size limits */
    int32_t pll_min;                /**< Minimum pump_charge */
    int32_t pivot;                  /**< Center value (nominal cell size) */
    int32_t pll_max;                /**< Maximum pump_charge */
    
    /* Configuration */
    int tick_freq;                  /**< Tick frequency (default 24 MHz) */
    int pll_min_max_percent;        /**< Window percentage (default ±18%) */
    
    /* Correction ratios */
    int fast_correction_ratio_n;    /**< Fast correction numerator */
    int fast_correction_ratio_d;    /**< Fast correction denominator */
    int slow_correction_ratio_n;    /**< Slow correction numerator */
    int slow_correction_ratio_d;    /**< Slow correction denominator */
    
    /* Inter-band rejection */
    int inter_band_rejection;       /**< Rejection mode (0=none, 1=GCR, 2=FM) */
    
    /* Band mode (Victor 9K) */
    int band_mode;                  /**< 0=normal, 1=band mode */
    int bands_separators[UFT_HXC_MAX_BANDS];
    
    /* Track info */
    int track;
    int side;
    
} uft_hxc_pll_t;

/*============================================================================
 * Bitrate Detection Result
 *============================================================================*/

/**
 * @brief Result of automatic bitrate detection
 */
typedef struct {
    bool     detected;              /**< Detection successful */
    uint32_t cell_ticks;            /**< Detected cell size in ticks */
    uint32_t bitrate_hz;            /**< Detected bitrate in Hz */
    uft_hxc_encoding_t encoding;    /**< Detected encoding type */
} uft_hxc_bitrate_t;

/*============================================================================
 * PLL Lifecycle
 *============================================================================*/

/**
 * @brief Create new PLL state with defaults
 * @return Allocated PLL state or NULL on error
 */
uft_hxc_pll_t* uft_hxc_pll_create(void);

/**
 * @brief Destroy PLL state
 * @param pll PLL state to destroy
 */
void uft_hxc_pll_destroy(uft_hxc_pll_t* pll);

/**
 * @brief Reset PLL state (keep configuration)
 * @param pll PLL state to reset
 */
void uft_hxc_pll_reset(uft_hxc_pll_t* pll);

/*============================================================================
 * PLL Configuration
 *============================================================================*/

/**
 * @brief Set bitrate and calculate cell size limits
 * @param pll PLL state
 * @param bitrate_hz Target bitrate in Hz
 */
void uft_hxc_pll_set_bitrate(uft_hxc_pll_t* pll, uint32_t bitrate_hz);

/**
 * @brief Set tick frequency
 * @param pll PLL state
 * @param tick_freq Tick frequency in Hz (default 24000000)
 */
void uft_hxc_pll_set_tick_freq(uft_hxc_pll_t* pll, uint32_t tick_freq);

/**
 * @brief Enable band mode for variable speed drives (Victor 9K)
 * @param pll PLL state
 * @param track Track number for band selection
 */
void uft_hxc_pll_set_band_mode(uft_hxc_pll_t* pll, int track);

/**
 * @brief Set inter-band rejection mode
 * @param pll PLL state
 * @param mode UFT_HXC_BAND_NONE, UFT_HXC_BAND_GCR, or UFT_HXC_BAND_FM
 */
void uft_hxc_pll_set_inter_band_rejection(uft_hxc_pll_t* pll, int mode);

/*============================================================================
 * Histogram Analysis
 *============================================================================*/

/**
 * @brief Compute histogram from flux timing data
 * @param indata Input timing data
 * @param size Number of samples
 * @param outdata Output histogram (must be UFT_HXC_HISTOGRAM_SIZE)
 */
void uft_hxc_compute_histogram(const uint32_t* indata, size_t size, 
                                uint32_t* outdata);

/**
 * @brief Detect bitrate peaks in histogram
 * @param pll PLL state (for tick_freq)
 * @param histogram Input histogram
 * @return Cell size in ticks, or 0 on error
 */
uint32_t uft_hxc_detectpeaks(uft_hxc_pll_t* pll, const uint32_t* histogram);

/*============================================================================
 * Core PLL Processing
 *============================================================================*/

/**
 * @brief Process single pulse through PLL
 * @param pll PLL state
 * @param current_pulsevalue Pulse timing value
 * @param badpulse Output: true if pulse was rejected
 * @param overlapval Overlap value for multi-rev analysis
 * @param phasecorrection Enable phase correction
 * @return Number of cells (bits) represented by this pulse
 */
int uft_hxc_get_cell_timing(uft_hxc_pll_t* pll, int current_pulsevalue,
                            bool* badpulse, int overlapval, int phasecorrection);

/*============================================================================
 * High-Level Detection
 *============================================================================*/

/**
 * @brief Detect bitrate from flux data
 * @param flux_data Flux timing data
 * @param num_samples Number of samples
 * @param tick_freq Tick frequency (0 for default 24 MHz)
 * @return Detection result
 */
uft_hxc_bitrate_t uft_hxc_detect_bitrate(const uint32_t* flux_data, 
                                          size_t num_samples,
                                          uint32_t tick_freq);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HXC_PLL_ENHANCED_H */

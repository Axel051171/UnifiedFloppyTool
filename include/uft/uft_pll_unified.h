/**
 * @file uft_pll_unified.h
 * @brief Unified PLL Controller (W-P1-005)
 * 
 * Central PLL management for all flux decoding.
 * Consolidates 18+ PLL implementations into one interface.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_PLL_UNIFIED_H
#define UFT_PLL_UNIFIED_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft_pll_params.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * PLL ALGORITHM TYPES
 *===========================================================================*/

/**
 * @brief Available PLL algorithms
 */
typedef enum {
    UFT_PLL_ALGO_SIMPLE,      /**< Simple threshold-based */
    UFT_PLL_ALGO_DPLL,        /**< Digital PLL (standard) */
    UFT_PLL_ALGO_ADAPTIVE,    /**< Adaptive PLL with auto-tuning */
    UFT_PLL_ALGO_KALMAN,      /**< Kalman filter based */
    UFT_PLL_ALGO_PI,          /**< Proportional-Integral controller */
    UFT_PLL_ALGO_WD1772,      /**< WD1772 emulation */
    UFT_PLL_ALGO_COUNT
} uft_pll_algo_t;

/**
 * @brief Algorithm names for display
 */
static const char* const UFT_PLL_ALGO_NAMES[] = {
    "Simple",
    "DPLL",
    "Adaptive",
    "Kalman",
    "PI Controller",
    "WD1772 Emu"
};

/*===========================================================================
 * PRESET CONFIGURATIONS
 *===========================================================================*/

/**
 * @brief Format-specific PLL presets
 */
typedef enum {
    UFT_PLL_PRESET_AUTO,          /**< Auto-detect from flux */
    UFT_PLL_PRESET_IBM_DD,        /**< IBM PC DD (250 kbps MFM) */
    UFT_PLL_PRESET_IBM_HD,        /**< IBM PC HD (500 kbps MFM) */
    UFT_PLL_PRESET_AMIGA_DD,      /**< Amiga DD (250 kbps MFM) */
    UFT_PLL_PRESET_AMIGA_HD,      /**< Amiga HD (500 kbps MFM) */
    UFT_PLL_PRESET_C64,           /**< C64/1541 (GCR variable) */
    UFT_PLL_PRESET_APPLE2,        /**< Apple II (GCR) */
    UFT_PLL_PRESET_MAC_400K,      /**< Mac 400K (GCR) */
    UFT_PLL_PRESET_MAC_800K,      /**< Mac 800K (GCR) */
    UFT_PLL_PRESET_ATARI_ST,      /**< Atari ST (MFM) */
    UFT_PLL_PRESET_FM_SD,         /**< FM Single Density */
    UFT_PLL_PRESET_COUNT
} uft_pll_preset_t;

/**
 * @brief Preset names for display
 */
static const char* const UFT_PLL_PRESET_NAMES[] = {
    "Auto-Detect",
    "IBM PC DD",
    "IBM PC HD",
    "Amiga DD",
    "Amiga HD",
    "C64/1541",
    "Apple II",
    "Mac 400K",
    "Mac 800K",
    "Atari ST",
    "FM Single Density"
};

/*===========================================================================
 * UNIFIED PLL CONFIGURATION
 *===========================================================================*/

/**
 * @brief Extended PLL configuration
 */
typedef struct {
    /* Base parameters */
    uft_pll_params_t base;
    
    /* Algorithm selection */
    uft_pll_algo_t algorithm;
    
    /* Advanced tuning */
    float gain_p;              /**< Proportional gain (0.0-1.0) */
    float gain_i;              /**< Integral gain (0.0-1.0) */
    float gain_d;              /**< Derivative gain (0.0-1.0) */
    
    /* Noise handling */
    int noise_filter_ns;       /**< Ignore transitions shorter than this */
    int max_zeros;             /**< Max consecutive zeros before resync */
    
    /* Quality tracking */
    bool track_quality;        /**< Enable quality metrics */
    bool adaptive_gain;        /**< Auto-adjust gains */
    
    /* Debug */
    bool debug_output;         /**< Enable debug logging */
} uft_pll_config_t;

/**
 * @brief Default configuration
 */
static const uft_pll_config_t UFT_PLL_CONFIG_DEFAULT = {
    .base = {
        .bitcell_ns = 4000,
        .clock_min_ns = 3400,
        .clock_max_ns = 4600,
        .clock_centre_ns = 4000,
        .pll_adjust_percent = 15,
        .pll_phase_percent = 60,
        .flux_scale_percent = 100,
        .sync_bits_required = 256,
        .jitter_percent = 2,
        .use_fm = false,
        .use_gcr = false
    },
    .algorithm = UFT_PLL_ALGO_DPLL,
    .gain_p = 0.6f,
    .gain_i = 0.1f,
    .gain_d = 0.0f,
    .noise_filter_ns = 100,
    .max_zeros = 32,
    .track_quality = true,
    .adaptive_gain = false,
    .debug_output = false
};

/*===========================================================================
 * PLL STATISTICS
 *===========================================================================*/

/**
 * @brief PLL decode quality metrics
 */
typedef struct {
    /* Bit counts */
    uint64_t bits_decoded;
    uint64_t zeros_decoded;
    uint64_t ones_decoded;
    
    /* Timing */
    double avg_bitcell_ns;
    double min_bitcell_ns;
    double max_bitcell_ns;
    double jitter_ns;
    
    /* Sync */
    int sync_losses;
    int sync_recoveries;
    int current_sync_bits;
    
    /* Quality */
    int quality_score;         /**< 0-100 */
    float signal_quality;      /**< SNR estimate */
    
    /* Phase */
    double phase_error_avg;
    double phase_error_max;
} uft_pll_stats_t;

/*===========================================================================
 * PLL CONTEXT
 *===========================================================================*/

/**
 * @brief Opaque PLL context
 */
typedef struct uft_pll_context uft_pll_context_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create PLL context with config
 */
uft_pll_context_t* uft_pll_create(const uft_pll_config_t *config);

/**
 * @brief Create PLL from preset
 */
uft_pll_context_t* uft_pll_create_preset(uft_pll_preset_t preset);

/**
 * @brief Destroy PLL context
 */
void uft_pll_destroy(uft_pll_context_t *ctx);

/**
 * @brief Reset PLL to initial state
 */
void uft_pll_context_reset(uft_pll_context_t *ctx);

/*===========================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Get current config
 */
const uft_pll_config_t* uft_pll_get_config(const uft_pll_context_t *ctx);

/**
 * @brief Update config
 */
int uft_pll_set_config(uft_pll_context_t *ctx, const uft_pll_config_t *config);

/**
 * @brief Apply preset
 */
int uft_pll_apply_preset(uft_pll_context_t *ctx, uft_pll_preset_t preset);

/**
 * @brief Set algorithm
 */
int uft_pll_set_algorithm(uft_pll_context_t *ctx, uft_pll_algo_t algo);

/**
 * @brief Set bitcell time
 */
int uft_pll_set_bitcell(uft_pll_context_t *ctx, int bitcell_ns);

/*===========================================================================
 * DECODING
 *===========================================================================*/

/**
 * @brief Process single flux transition
 * @param ctx PLL context
 * @param flux_ns Time since last transition in nanoseconds
 * @param bit_out Output: decoded bit (0, 1, or -1 if no bit)
 * @return 0 on success, <0 on error
 */
int uft_pll_process(uft_pll_context_t *ctx, int flux_ns, int *bit_out);

/**
 * @brief Process flux array
 * @param ctx PLL context
 * @param flux_ns Array of flux times
 * @param flux_count Number of flux transitions
 * @param bits_out Output buffer for bits
 * @param bits_capacity Capacity of bits buffer
 * @return Number of bits decoded, <0 on error
 */
int uft_pll_decode_flux(
    uft_pll_context_t *ctx,
    const int *flux_ns,
    size_t flux_count,
    uint8_t *bits_out,
    size_t bits_capacity);

/**
 * @brief Process index pulse
 */
void uft_pll_index(uft_pll_context_t *ctx);

/*===========================================================================
 * QUALITY
 *===========================================================================*/

/**
 * @brief Get current statistics
 */
const uft_pll_stats_t* uft_pll_get_stats(const uft_pll_context_t *ctx);

/**
 * @brief Reset statistics
 */
void uft_pll_reset_stats(uft_pll_context_t *ctx);

/**
 * @brief Check if sync is established
 */
bool uft_pll_is_synced(const uft_pll_context_t *ctx);

/**
 * @brief Get current quality score (0-100)
 */
int uft_pll_get_quality(const uft_pll_context_t *ctx);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Get preset config
 */
const uft_pll_config_t* uft_pll_get_preset_config(uft_pll_preset_t preset);

/**
 * @brief Auto-detect best preset from flux data
 */
uft_pll_preset_t uft_pll_detect_preset(const int *flux_ns, size_t count);

/**
 * @brief Get algorithm name
 */
const char* uft_pll_algo_name(uft_pll_algo_t algo);

/**
 * @brief Get preset name
 */
const char* uft_pll_preset_name(uft_pll_preset_t preset);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLL_UNIFIED_H */

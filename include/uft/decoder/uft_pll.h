/**
 * @file uft_pll.h
 * @brief Kalman PLL (Phase-Locked Loop) API
 */

#ifndef UFT_DECODER_PLL_H
#define UFT_DECODER_PLL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_types.h"  /* für uft_encoding_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PLL configuration
 */
typedef struct {
    double initial_frequency;   /**< Initial frequency (Hz) */
    double frequency_tolerance; /**< Frequency tolerance (0.0-1.0) */
    double phase_gain;          /**< Phase tracking gain */
    double frequency_gain;      /**< Frequency tracking gain */
    double jitter_tolerance;    /**< Jitter tolerance (0.0-1.0) */
    bool adaptive_bandwidth;    /**< Use adaptive bandwidth */
    int lock_threshold;         /**< Lock detection threshold */
} uft_pll_config_t;

/**
 * @brief PLL state information
 */
typedef struct {
    double current_frequency;   /**< Current tracked frequency */
    double current_phase;       /**< Current phase */
    int lock_count;             /**< Lock counter */
    bool is_locked;             /**< Lock status */
    double kalman_gain;         /**< Current Kalman gain */
    double error_covariance;    /**< Error covariance */
    uint64_t total_bits;        /**< Total bits processed */
    uint64_t good_bits;         /**< Good bits */
    double avg_jitter;          /**< Average jitter */
    double confidence;          /**< Decode confidence (0.0-1.0) */
} uft_pll_state_t;

/**
 * @brief PLL context (opaque)
 */
typedef struct uft_pll_s uft_pll_t;

/* Configuration */
void uft_pll_config_default(uft_pll_config_t* config, int encoding);

/* Lifecycle */
uft_pll_t* uft_pll_create(const uft_pll_config_t* config);
void uft_pll_destroy(uft_pll_t* pll);
void uft_pll_reset(uft_pll_t* pll);

/* Processing - signatures match implementation */
int uft_pll_process(uft_pll_t* pll, uint32_t flux_time_ns,
                    uint8_t* bits, int max_bits);

int uft_pll_decode(uft_pll_t* pll, const uint32_t* flux,
                   size_t flux_count, uint8_t* bits, size_t max_bits);

/* State query */
void uft_pll_get_state(const uft_pll_t* pll, uft_pll_state_t* state);
bool uft_pll_is_locked(const uft_pll_t* pll);
double uft_pll_get_ber(const uft_pll_t* pll);

/* Utilities */
double uft_pll_encoding_frequency(int encoding);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODER_PLL_H */

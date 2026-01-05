/**
 * @file uft_pll_pi.h
 * @brief PI Loop Filter PLL for MFM/FM/RLL Decoding
 * 
 * Based on raszpl/sigrok-disk modern PLL implementation.
 * Uses Proportional-Integral control for robust clock recovery.
 * 
 * @copyright GPL-3.0 (derived from sigrok-disk)
 */

#ifndef UFT_PLL_PI_H
#define UFT_PLL_PI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Standard data rates in bits per second */
typedef enum {
    UFT_RATE_FM_DD      = 125000,    /**< FM Double Density */
    UFT_RATE_FM_HD      = 150000,    /**< FM High Density */
    UFT_RATE_MFM_DD     = 250000,    /**< MFM Double Density (floppy) */
    UFT_RATE_MFM_DD_300 = 300000,    /**< MFM DD at 300 RPM */
    UFT_RATE_MFM_HD     = 500000,    /**< MFM High Density (floppy) */
    UFT_RATE_MFM_HDD    = 5000000,   /**< MFM Hard Drive */
    UFT_RATE_RLL_HDD    = 7500000,   /**< RLL 2,7 Hard Drive */
    UFT_RATE_RLL_FAST   = 10000000   /**< Fast RLL */
} uft_data_rate_t;

/** Encoding types */
typedef enum {
    UFT_ENC_FM,
    UFT_ENC_MFM,
    UFT_ENC_RLL_27,         /**< RLL 2,7 */
    UFT_ENC_RLL_17,         /**< RLL 1,7 (Adaptec) */
    UFT_ENC_RLL_SEAGATE,
    UFT_ENC_RLL_WD,
    UFT_ENC_RLL_OMTI,
    UFT_ENC_RLL_ADAPTEC,
    UFT_ENC_CUSTOM
} uft_encoding_t;

/** PLL state */
typedef enum {
    UFT_PLL_SEEKING,        /**< Looking for sync pattern */
    UFT_PLL_SYNCING,        /**< Acquiring lock */
    UFT_PLL_LOCKED,         /**< Stable lock */
    UFT_PLL_TRACKING        /**< Decoding data */
} uft_pll_state_t;

/** Sync tolerance presets */
typedef enum {
    UFT_SYNC_TOL_15 = 15,   /**< 15% - tight */
    UFT_SYNC_TOL_20 = 20,   /**< 20% - moderate */
    UFT_SYNC_TOL_25 = 25,   /**< 25% - default */
    UFT_SYNC_TOL_33 = 33,   /**< 33% - loose */
    UFT_SYNC_TOL_50 = 50    /**< 50% - very loose */
} uft_sync_tolerance_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief PI Loop Filter PLL Configuration
 */
typedef struct {
    double kp;              /**< Proportional constant (default: 0.5) */
    double ki;              /**< Integral constant (default: 0.0005) */
    double sync_tolerance;  /**< Initial sync tolerance (0.0-1.0, default: 0.25) */
    double lock_threshold;  /**< Threshold to declare lock (default: 0.1) */
    uft_encoding_t encoding;
    uint32_t data_rate;     /**< Data rate in bps */
} uft_pll_config_t;

/**
 * @brief PI Loop Filter PLL State
 */
typedef struct {
    /* Configuration */
    uft_pll_config_t config;
    
    /* Timing state */
    double nominal_period;  /**< Nominal bit cell period (ns) */
    double current_period;  /**< Current expected period (ns) */
    double tolerance;       /**< Current tolerance (ns) */
    
    /* PI filter state */
    double integral;        /**< Accumulated error */
    double last_error;      /**< Previous error (for derivative if needed) */
    
    /* Sync state */
    uft_pll_state_t state;
    int sync_count;         /**< Consecutive good transitions */
    int sync_required;      /**< Transitions needed for lock */
    
    /* Bit extraction */
    double accumulated;     /**< Accumulated time since last bit */
    uint32_t shift_reg;     /**< Shift register for pattern matching */
    int bits_pending;       /**< Bits waiting to be output */
    
    /* Statistics */
    uint32_t total_transitions;
    uint32_t good_transitions;
    uint32_t clock_errors;
    uint32_t out_of_tolerance;
    
    /* Debug */
    double min_period_seen;
    double max_period_seen;
    double period_variance;
} uft_pll_t;

/**
 * @brief Decoded bit with metadata
 */
typedef struct {
    uint8_t value;          /**< Bit value (0 or 1) */
    bool is_clock;          /**< True if this is a clock bit (FM/MFM) */
    bool is_sync;           /**< Part of sync pattern */
    bool is_mark;           /**< Part of address mark (clock violation) */
    double timing;          /**< Actual timing (ns) */
    double deviation;       /**< Deviation from expected (ns) */
} uft_pll_bit_t;

/**
 * @brief Byte with metadata
 */
typedef struct {
    uint8_t value;          /**< Decoded byte value */
    uint8_t clock_pattern;  /**< Clock bits for this byte (MFM) */
    bool has_clock_error;   /**< Clock violation detected */
    bool is_sync_mark;      /**< A1/C2 sync mark */
} uft_pll_byte_t;

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Get default configuration
 * @param[out] config Configuration to initialize
 * @param encoding Encoding type
 * @param data_rate Data rate in bps
 */
void uft_pll_config_default(uft_pll_config_t *config,
                            uft_encoding_t encoding,
                            uint32_t data_rate);

/**
 * @brief Set aggressive parameters for problematic disks
 * @param[out] config Configuration to modify
 * 
 * Use for wobbly drives, warped disks, or marginal media.
 */
void uft_pll_config_aggressive(uft_pll_config_t *config);

/**
 * @brief Initialize PLL
 * @param[out] pll PLL state to initialize
 * @param config Configuration (NULL for defaults)
 * @return 0 on success
 */
int uft_pll_init(uft_pll_t *pll, const uft_pll_config_t *config);

/**
 * @brief Reset PLL state (keep configuration)
 * @param pll PLL state
 */
void uft_pll_reset(uft_pll_t *pll);

/* ============================================================================
 * Processing
 * ============================================================================ */

/**
 * @brief Process a single flux transition
 * @param pll PLL state
 * @param delta_ns Time since last transition in nanoseconds
 * @param[out] bits Output bit buffer (caller allocates, size >= 8)
 * @param[out] bit_count Number of bits produced
 * @return 0 on success, UFT_ERR_* on error
 * 
 * May produce 0-8 bits per transition depending on encoding and timing.
 */
int uft_pll_process_transition(uft_pll_t *pll,
                               double delta_ns,
                               uft_pll_bit_t *bits,
                               int *bit_count);

/**
 * @brief Process transition and extract bytes
 * @param pll PLL state
 * @param delta_ns Time since last transition
 * @param[out] byte_out Output byte (if complete)
 * @param[out] byte_ready True if byte is complete
 * @return 0 on success
 */
int uft_pll_process_to_byte(uft_pll_t *pll,
                            double delta_ns,
                            uft_pll_byte_t *byte_out,
                            bool *byte_ready);

/**
 * @brief Process multiple transitions at once
 * @param pll PLL state
 * @param deltas Array of transition times (ns)
 * @param count Number of transitions
 * @param[out] bytes Output buffer (caller allocates)
 * @param max_bytes Size of output buffer
 * @param[out] bytes_decoded Number of bytes produced
 * @return 0 on success
 */
int uft_pll_process_batch(uft_pll_t *pll,
                          const double *deltas,
                          size_t count,
                          uft_pll_byte_t *bytes,
                          size_t max_bytes,
                          size_t *bytes_decoded);

/* ============================================================================
 * Sync Detection
 * ============================================================================ */

/**
 * @brief Check if PLL is locked
 * @param pll PLL state
 * @return true if locked
 */
bool uft_pll_is_locked(const uft_pll_t *pll);

/**
 * @brief Get current sync quality (0.0 - 1.0)
 * @param pll PLL state
 * @return Quality metric (1.0 = perfect)
 */
double uft_pll_sync_quality(const uft_pll_t *pll);

/**
 * @brief Force sync acquisition from known preamble
 * @param pll PLL state
 * @param preamble_deltas Preamble transition times
 * @param count Number of transitions
 * @return 0 on success
 */
int uft_pll_force_sync(uft_pll_t *pll,
                       const double *preamble_deltas,
                       size_t count);

/* ============================================================================
 * Address Mark Detection
 * ============================================================================ */

/** MFM address marks */
#define UFT_MARK_IDAM   0xFE    /**< ID Address Mark */
#define UFT_MARK_DAM    0xFB    /**< Data Address Mark */
#define UFT_MARK_DDAM   0xF8    /**< Deleted Data Address Mark */
#define UFT_MARK_IAM    0xFC    /**< Index Address Mark */

/**
 * @brief Check for sync mark (A1 with clock violation)
 * @param byte Byte with clock info
 * @return true if this is a sync mark
 */
bool uft_pll_is_sync_mark(const uft_pll_byte_t *byte);

/**
 * @brief Check for address mark
 * @param byte Byte value
 * @return Address mark type or 0 if not a mark
 */
int uft_pll_address_mark_type(uint8_t byte);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief PLL statistics
 */
typedef struct {
    uint32_t total_transitions;
    uint32_t good_transitions;
    uint32_t clock_errors;
    uint32_t out_of_tolerance;
    uint32_t sync_losses;
    double min_period_ns;
    double max_period_ns;
    double avg_period_ns;
    double period_stddev_ns;
    double lock_quality;        /**< 0.0 - 1.0 */
} uft_pll_stats_t;

/**
 * @brief Get PLL statistics
 * @param pll PLL state
 * @param[out] stats Statistics structure
 */
void uft_pll_get_stats(const uft_pll_t *pll, uft_pll_stats_t *stats);

/**
 * @brief Reset statistics (keep PLL state)
 * @param pll PLL state
 */
void uft_pll_reset_stats(uft_pll_t *pll);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * @brief Calculate nominal bit period for data rate
 * @param data_rate Data rate in bps
 * @param encoding Encoding type
 * @return Period in nanoseconds
 */
double uft_pll_nominal_period(uint32_t data_rate, uft_encoding_t encoding);

/**
 * @brief Get encoding name
 * @param encoding Encoding type
 * @return Human-readable name
 */
const char *uft_pll_encoding_name(uft_encoding_t encoding);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLL_PI_H */

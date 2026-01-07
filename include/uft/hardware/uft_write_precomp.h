/**
 * @file uft_write_precomp.h
 * @brief Write Precompensation for Floppy Disk Writing
 * 
 * Based on DTC learnings - implements ns-level write precompensation
 * to compensate for bit shift effects during magnetic recording.
 * 
 * Precompensation adjusts timing of flux transitions to counteract
 * the magnetic interference between adjacent bits.
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#ifndef UFT_WRITE_PRECOMP_H
#define UFT_WRITE_PRECOMP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants (based on DTC observations)
 * ============================================================================ */

/** Maximum precompensation time in nanoseconds (DTC: max 1000) */
#define UFT_PRECOMP_TIME_MAX_NS     1000

/** Maximum precompensation window in nanoseconds (DTC: max 10000) */
#define UFT_PRECOMP_WINDOW_MAX_NS   10000

/** Default precompensation for MFM DD */
#define UFT_PRECOMP_MFM_DD_NS       140

/** Default precompensation for MFM HD */
#define UFT_PRECOMP_MFM_HD_NS       70

/** Default precompensation for GCR */
#define UFT_PRECOMP_GCR_NS          0    /* GCR typically doesn't need precomp */

/* Precompensation modes */
#define UFT_PRECOMP_MODE_OFF        0
#define UFT_PRECOMP_MODE_EARLY      1    /* Shift early transitions */
#define UFT_PRECOMP_MODE_LATE       2    /* Shift late transitions */
#define UFT_PRECOMP_MODE_AUTO       3    /* Automatic based on pattern */

/* Write bias modes (DTC -wb) */
#define UFT_WRITE_BIAS_NEUTRAL      0
#define UFT_WRITE_BIAS_OUT          1
#define UFT_WRITE_BIAS_IN           2

/* ============================================================================
 * Data Types
 * ============================================================================ */

/**
 * @brief Write precompensation configuration
 */
typedef struct {
    uint16_t precomp_time_ns;     /**< Precompensation amount (0-1000 ns) */
    uint16_t precomp_window_ns;   /**< Window for pattern detection (0-10000 ns) */
    uint8_t  mode;                /**< Precompensation mode */
    uint8_t  bias;                /**< Write bias mode */
    bool     enabled;             /**< Enable precompensation */
    bool     auto_adjust;         /**< Auto-adjust based on track number */
    
    /* Track-dependent adjustment */
    uint8_t  inner_track_start;   /**< Track where inner adjustment starts */
    uint16_t inner_track_add_ns;  /**< Additional ns for inner tracks */
    
    /* Per-encoding overrides */
    uint16_t mfm_precomp_ns;      /**< MFM-specific override */
    uint16_t fm_precomp_ns;       /**< FM-specific override */
    uint16_t gcr_precomp_ns;      /**< GCR-specific override */
} uft_precomp_config_t;

/**
 * @brief Precompensation state for a write operation
 */
typedef struct {
    uft_precomp_config_t config;
    
    /* Runtime state */
    uint8_t  current_track;
    uint16_t effective_precomp_ns;
    uint64_t bits_processed;
    uint64_t bits_adjusted;
    
    /* Pattern history for auto mode */
    uint8_t  history[8];          /**< Recent bit pattern */
    uint8_t  history_pos;
    
    /* Statistics */
    uint32_t early_shifts;        /**< Count of early shifts applied */
    uint32_t late_shifts;         /**< Count of late shifts applied */
    double   total_shift_ns;      /**< Total shift amount */
} uft_precomp_state_t;

/**
 * @brief Flux transition with precompensation
 */
typedef struct {
    uint32_t original_time;       /**< Original timing in samples */
    uint32_t adjusted_time;       /**< Adjusted timing after precomp */
    int16_t  shift_ns;            /**< Shift amount in ns (+ = early, - = late) */
    uint8_t  bit_pattern;         /**< Local bit pattern */
    bool     was_adjusted;        /**< Whether adjustment was applied */
} uft_precomp_transition_t;

/**
 * @brief Write operation result
 */
typedef struct {
    bool     success;
    uint32_t transitions_written;
    uint32_t transitions_adjusted;
    double   average_shift_ns;
    double   max_shift_ns;
    uint32_t verify_errors;       /**< Errors if verify enabled */
} uft_write_result_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Initialize precompensation config with defaults
 */
void uft_precomp_config_init(uft_precomp_config_t *config);

/**
 * @brief Initialize precompensation config for specific encoding
 * 
 * @param config   Configuration to initialize
 * @param encoding Encoding type ("MFM_DD", "MFM_HD", "FM", "GCR")
 */
void uft_precomp_config_for_encoding(
    uft_precomp_config_t *config,
    const char *encoding
);

/**
 * @brief Initialize precompensation state
 * 
 * @param state    State to initialize
 * @param config   Configuration to use
 */
void uft_precomp_state_init(
    uft_precomp_state_t *state,
    const uft_precomp_config_t *config
);

/**
 * @brief Set current track for track-dependent adjustment
 * 
 * @param state    Precompensation state
 * @param track    Track number (0-based)
 */
void uft_precomp_set_track(uft_precomp_state_t *state, uint8_t track);

/**
 * @brief Calculate precompensation for a single transition
 * 
 * @param state          Precompensation state
 * @param bit_pattern    Recent bit pattern (8 bits, current at LSB)
 * @param original_ns    Original transition time in ns
 * @return Adjusted transition time in ns
 */
double uft_precomp_adjust(
    uft_precomp_state_t *state,
    uint8_t bit_pattern,
    double original_ns
);

/**
 * @brief Apply precompensation to flux transition array
 * 
 * @param state          Precompensation state
 * @param transitions    Array of transitions (modified in place)
 * @param count          Number of transitions
 * @param sample_rate_hz Sample rate for conversion
 * @return Number of transitions adjusted
 */
size_t uft_precomp_apply_array(
    uft_precomp_state_t *state,
    uft_precomp_transition_t *transitions,
    size_t count,
    double sample_rate_hz
);

/**
 * @brief Apply precompensation to raw flux timing data
 * 
 * @param state          Precompensation state
 * @param flux_times     Array of flux intervals (modified in place)
 * @param count          Number of intervals
 * @param sample_rate_hz Sample rate
 * @param bits           Corresponding bit values (for pattern detection)
 * @return Number of intervals adjusted
 */
size_t uft_precomp_apply_flux(
    uft_precomp_state_t *state,
    uint32_t *flux_times,
    size_t count,
    double sample_rate_hz,
    const uint8_t *bits
);

/**
 * @brief Get precompensation statistics
 * 
 * @param state    Precompensation state
 * @param early    Output: count of early shifts
 * @param late     Output: count of late shifts
 * @param avg_ns   Output: average shift in ns
 */
void uft_precomp_get_stats(
    const uft_precomp_state_t *state,
    uint32_t *early,
    uint32_t *late,
    double *avg_ns
);

/**
 * @brief Reset precompensation state for new track
 */
void uft_precomp_reset(uft_precomp_state_t *state);

/* ============================================================================
 * Pattern Analysis
 * ============================================================================ */

/**
 * @brief Bit patterns that typically need early precompensation
 * 
 * Pattern 101 (isolated 1) needs early shift to prevent
 * read-back shift toward the adjacent 1s
 */
#define UFT_PATTERN_EARLY_1     0x05  /* 00000101 - isolated 1 after 1 */
#define UFT_PATTERN_EARLY_2     0x0A  /* 00001010 - 1 before isolated 1 */

/**
 * @brief Bit patterns that typically need late precompensation
 */
#define UFT_PATTERN_LATE_1      0x15  /* 00010101 - multiple isolated 1s */

/**
 * @brief Analyze bit pattern for precompensation need
 * 
 * @param pattern    8-bit pattern (current bit at position 0)
 * @return Suggested shift: positive = early, negative = late, 0 = none
 */
int8_t uft_precomp_analyze_pattern(uint8_t pattern);

/**
 * @brief Check if pattern needs precompensation
 */
bool uft_precomp_pattern_needs_adjust(uint8_t pattern);

/* ============================================================================
 * High-Level Write Functions
 * ============================================================================ */

/**
 * @brief Write track with precompensation
 * 
 * @param hal            Hardware abstraction layer context
 * @param track          Track number
 * @param head           Head number
 * @param flux_data      Flux timing data to write
 * @param flux_count     Number of flux transitions
 * @param config         Precompensation configuration
 * @param verify         Verify after write
 * @param result         Output result
 * @return 0 on success
 */
int uft_write_track_precomp(
    void *hal,
    uint8_t track,
    uint8_t head,
    const uint32_t *flux_data,
    size_t flux_count,
    const uft_precomp_config_t *config,
    bool verify,
    uft_write_result_t *result
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Convert nanoseconds to samples
 */
static inline uint32_t uft_ns_to_samples(double ns, double sample_rate_hz) {
    return (uint32_t)(ns * sample_rate_hz / 1e9 + 0.5);
}

/**
 * @brief Convert samples to nanoseconds
 */
static inline double uft_samples_to_ns(uint32_t samples, double sample_rate_hz) {
    return (double)samples / sample_rate_hz * 1e9;
}

/**
 * @brief Clamp precompensation value to valid range
 */
static inline uint16_t uft_precomp_clamp(int32_t value) {
    if (value < 0) return 0;
    if (value > UFT_PRECOMP_TIME_MAX_NS) return UFT_PRECOMP_TIME_MAX_NS;
    return (uint16_t)value;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_WRITE_PRECOMP_H */

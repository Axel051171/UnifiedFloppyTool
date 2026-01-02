/**
 * @file uft_hw_timing.h
 * @brief Hardware-Level Floppy Timing Constants
 * 
 * Extracted from XCopy Standalone and ZX FDD Emulator.
 * Provides:
 * - Flux transition timing thresholds
 * - MFM bit timing parameters
 * - Drive operation delays
 * - Timer configurations
 * 
 * @version 1.0.0
 */

#ifndef UFT_HW_TIMING_H
#define UFT_HW_TIMING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*============================================================================
 * MFM Timing Constants (from XCopy)
 *============================================================================*/

/**
 * @brief MFM bit transition times
 */
#define UFT_MFM_TRANS_DD_US         1.96    /**< DD transition time (µs) */
#define UFT_MFM_TRANS_HD_US         0.98    /**< HD transition time (µs) */

/**
 * @brief Standard bit cell times
 */
#define UFT_BITCELL_DD_NS           2000    /**< DD bit cell (ns) */
#define UFT_BITCELL_HD_NS           1000    /**< HD bit cell (ns) */
#define UFT_BITCELL_ED_NS           500     /**< ED bit cell (ns) */

/**
 * @brief Track capacity in bytes
 */
#define UFT_GAP_BYTES               1482    /**< Inter-sector gap */
#define UFT_STREAM_SIZE_DD          (12 * 1088 + UFT_GAP_BYTES)  /**< 11 + 1 spare */
#define UFT_STREAM_SIZE_HD          (23 * 1088 + UFT_GAP_BYTES)  /**< 22 + 1 spare */
#define UFT_WRITE_SIZE_DD           (11 * 1088 + UFT_GAP_BYTES)
#define UFT_WRITE_SIZE_HD           (22 * 1088 + UFT_GAP_BYTES)

/*============================================================================
 * Flux Timing Thresholds (from XCopy ISR)
 *============================================================================*/

/**
 * @brief DD disk timing thresholds (timer counts @ 48MHz/2)
 */
typedef struct {
    uint8_t low_threshold;      /**< Minimum valid sample (noise filter) */
    uint8_t high_2us;           /**< 2µs/3µs boundary */
    uint8_t high_3us;           /**< 3µs/4µs boundary */
    uint8_t high_max;           /**< Maximum valid sample */
} uft_flux_thresholds_t;

/** Default DD thresholds */
#define UFT_FLUX_THRESH_DD_LOW      30
#define UFT_FLUX_THRESH_DD_2US      115
#define UFT_FLUX_THRESH_DD_3US      155
#define UFT_FLUX_THRESH_DD_MAX      255

/** Default HD thresholds (half of DD) */
#define UFT_FLUX_THRESH_HD_LOW      15
#define UFT_FLUX_THRESH_HD_2US      57
#define UFT_FLUX_THRESH_HD_3US      77
#define UFT_FLUX_THRESH_HD_MAX      127

/**
 * @brief Get flux thresholds for density
 */
static inline uft_flux_thresholds_t uft_get_flux_thresholds_dd(void) {
    return (uft_flux_thresholds_t){
        .low_threshold = UFT_FLUX_THRESH_DD_LOW,
        .high_2us = UFT_FLUX_THRESH_DD_2US,
        .high_3us = UFT_FLUX_THRESH_DD_3US,
        .high_max = UFT_FLUX_THRESH_DD_MAX
    };
}

static inline uft_flux_thresholds_t uft_get_flux_thresholds_hd(void) {
    return (uft_flux_thresholds_t){
        .low_threshold = UFT_FLUX_THRESH_HD_LOW,
        .high_2us = UFT_FLUX_THRESH_HD_2US,
        .high_3us = UFT_FLUX_THRESH_HD_3US,
        .high_max = UFT_FLUX_THRESH_HD_MAX
    };
}

/*============================================================================
 * Timer Configuration (from XCopy)
 *============================================================================*/

/**
 * @brief Timer mode register values
 */
#define UFT_TIMER_MODE_HD           0x08    /**< Sys clock / 1 */
#define UFT_TIMER_MODE_DD           0x09    /**< Sys clock / 2 */

/**
 * @brief Input filter settings
 * 
 * Filter waits for N cycles of stable input
 * At 48MHz: 4 + 4*val clock cycles
 * val=0: 4 cycles = 83ns
 * val=2: 12 cycles = 250ns
 */
#define UFT_FILTER_DD               0
#define UFT_FILTER_HD               0

/*============================================================================
 * Drive Operation Delays (from XCopy/nibtools)
 *============================================================================*/

/**
 * @brief Drive timing parameters
 */
typedef struct {
    /* Selection */
    uint32_t select_delay_us;       /**< Delay after drive select (100µs) */
    uint32_t deselect_delay_us;     /**< Delay after drive deselect (5µs) */
    
    /* Motor */
    uint32_t spinup_delay_ms;       /**< Motor spinup time (600ms) */
    uint32_t spindown_delay_us;     /**< Motor spindown delay (50µs) */
    
    /* Head movement */
    uint32_t step_pulse_us;         /**< Step pulse duration (2µs) */
    uint32_t step_delay_ms;         /**< Delay between steps (3ms) */
    uint32_t direction_delay_ms;    /**< Delay after direction change (20ms) */
    uint32_t settle_delay_ms;       /**< Head settle time (15ms) */
    
    /* Side selection */
    uint32_t side_delay_ms;         /**< Side select delay (2ms) */
    
    /* Idle */
    uint32_t motor_timeout_sec;     /**< Motor idle timeout (5s) */
} uft_drive_delays_t;

/** Default drive delays */
extern const uft_drive_delays_t UFT_DRIVE_DELAYS_DEFAULT;

/*============================================================================
 * Sync Word Detection
 *============================================================================*/

/**
 * @brief Amiga sync word (in ISR format)
 * 
 * Normal: 0x44894489
 * ISR detected: 0xA4489448 (shifted for byte alignment)
 */
#define UFT_AMIGA_SYNC_RAW          0x44894489
#define UFT_AMIGA_SYNC_ISR          0xA4489448

/**
 * @brief IBM sync byte (MFM encoded 0xA1 with missing clock)
 */
#define UFT_IBM_SYNC_A1             0x4489

/**
 * @brief ID Address Mark (after sync)
 */
#define UFT_IBM_IDAM                0xFE

/**
 * @brief Data Address Mark
 */
#define UFT_IBM_DAM                 0xFB

/**
 * @brief Deleted Data Address Mark
 */
#define UFT_IBM_DDAM                0xF8

/*============================================================================
 * MFM Encoding Tables (from ZX FDD Emulator)
 *============================================================================*/

/**
 * @brief Fast MFM encoding table (inverted for direct output)
 * 
 * Index: 3 bits (prev_bit:current_2bits)
 * Output: MFM encoded pattern
 */
extern const uint8_t UFT_MFM_ENCODE_TABLE[8];

/**
 * @brief Encode byte to MFM with previous bit context
 * 
 * @param byte          Data byte to encode
 * @param prev_bit      Previous bit (0 or 1)
 * @param out           Output buffer (2 bytes minimum)
 * @return New previous bit state
 */
uint8_t uft_mfm_encode_byte(uint8_t byte, uint8_t prev_bit, uint8_t *out);

/*============================================================================
 * Flux Histogram Analysis
 *============================================================================*/

/**
 * @brief Histogram analysis result
 */
typedef struct {
    /* Peaks */
    uint16_t peak_2us;          /**< Center of 2µs peak */
    uint16_t peak_3us;          /**< Center of 3µs peak */
    uint16_t peak_4us;          /**< Center of 4µs peak */
    
    /* Optimal thresholds */
    uint16_t thresh_23;         /**< Optimal 2µs/3µs boundary */
    uint16_t thresh_34;         /**< Optimal 3µs/4µs boundary */
    
    /* Quality metrics */
    bool valid;                 /**< All peaks found */
    float separation;           /**< Peak separation quality (0-1) */
} uft_hist_analysis_t;

/**
 * @brief Analyze flux histogram to find optimal thresholds
 * 
 * @param histogram     256-bin histogram
 * @param result        Output analysis result
 * @return true if valid analysis
 */
bool uft_analyze_flux_histogram(const uint32_t *histogram,
                                uft_hist_analysis_t *result);

/**
 * @brief Find local minimum between two indices
 * 
 * @param histogram     Histogram data
 * @param start         Start index
 * @param end           End index
 * @return Index of minimum
 */
uint16_t uft_find_histogram_minimum(const uint32_t *histogram,
                                    uint16_t start,
                                    uint16_t end);

/*============================================================================
 * RPM and Rotation
 *============================================================================*/

/**
 * @brief Rotation parameters
 */
typedef struct {
    uint32_t period_us;         /**< Rotation period (µs) */
    uint16_t rpm;               /**< RPM */
    double tolerance_pct;       /**< Acceptable variation (%) */
} uft_rotation_params_t;

/** DD disk at 300 RPM */
#define UFT_ROTATION_DD_PERIOD_US   200000
#define UFT_ROTATION_DD_RPM         300

/** HD 5.25" at 360 RPM */
#define UFT_ROTATION_HD_PERIOD_US   166666
#define UFT_ROTATION_HD_RPM         360

/** Acceptable RPM tolerance */
#define UFT_RPM_TOLERANCE_PCT       3.0

/**
 * @brief Measure RPM from index pulse period
 * 
 * @param period_us     Index-to-index period in µs
 * @return RPM value
 */
static inline double uft_period_to_rpm(uint32_t period_us) {
    if (period_us == 0) return 0.0;
    return 60000000.0 / (double)period_us;
}

/**
 * @brief Check if RPM is within tolerance
 */
static inline bool uft_rpm_in_range(double rpm, 
                                    double nominal, 
                                    double tolerance_pct) {
    double low = nominal * (1.0 - tolerance_pct / 100.0);
    double high = nominal * (1.0 + tolerance_pct / 100.0);
    return rpm >= low && rpm <= high;
}

/*============================================================================
 * Precompensation
 *============================================================================*/

/**
 * @brief Write precompensation for inner cylinders
 * 
 * Shifts write timing to compensate for magnetic effects
 * on inner (higher-numbered) cylinders.
 */
#define UFT_PRECOMP_THRESHOLD_CYL   40      /**< Start precomp at cylinder 40 */
#define UFT_PRECOMP_VALUE_NS        140     /**< Precomp shift (ns) */

/**
 * @brief Calculate precompensation for cylinder
 * 
 * @param cylinder      Cylinder number (0-79)
 * @return Precompensation in nanoseconds
 */
static inline uint32_t uft_get_precomp_ns(uint8_t cylinder) {
    return (cylinder >= UFT_PRECOMP_THRESHOLD_CYL) ? UFT_PRECOMP_VALUE_NS : 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_HW_TIMING_H */

/**
 * @file uft_flux_pll.h
 * @brief UnifiedFloppyTool - Flux Stream Processing and PLL Decoding
 * @version 3.1.4.006
 *
 * Comprehensive flux transition processing, PLL (Phase-Locked Loop) decoding,
 * and bitstream extraction. Based on analysis of FluxFox (Rust), BBC-FDC,
 * and other modern flux processing implementations.
 *
 * Sources analyzed:
 * - FluxFox (fluxfox-main): pll.rs, flux_revolution.rs, histogram.rs
 * - dsk2woz2.c: DSK to WOZ2 conversion
 * - DskToWoz2: nibblize_6_2.c, nibblize_5_3.c
 * - DiskBrowser: WozFile.java, ByteTranslator6and2.java
 *
 * Key algorithms:
 * - Software PLL with phase/frequency tracking
 * - Flux classification (short/medium/long)
 * - MFM/FM marker detection
 * - Weak bit detection and handling
 * - Clock rate auto-detection
 */

#ifndef UFT_FLUX_PLL_H
#define UFT_FLUX_PLL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Flux Timing Constants
 *============================================================================*/

/** Base clock period for 250Kbps at 300 RPM (2 µs) */
#define UFT_PLL_BASE_CLOCK          2.0e-6

/** Maximum clock adjustment (20%) */
#define UFT_PLL_MAX_CLOCK_ADJUST    0.20

/** Short flux transition (4 µs for MFM) */
#define UFT_FLUX_SHORT_TIME         4.0e-6

/** Medium flux transition (6 µs for MFM) */
#define UFT_FLUX_MEDIUM_TIME        6.0e-6

/** Long flux transition (8 µs for MFM) */
#define UFT_FLUX_LONG_TIME          8.0e-6

/** Tolerance for flux classification (0.5 µs) */
#define UFT_FLUX_TOLERANCE          0.5e-6

/** Kryoflux default master clock (Hz) */
#define UFT_KFX_DEFAULT_MCK         ((18432000.0 * 73.0) / 14.0 / 2.0)

/** Kryoflux default sample clock (Hz) */
#define UFT_KFX_DEFAULT_SCK         (UFT_KFX_DEFAULT_MCK / 2.0)

/** Kryoflux default index clock (Hz) */
#define UFT_KFX_DEFAULT_ICK         (UFT_KFX_DEFAULT_MCK / 16.0)

/** SCP base capture resolution (25 ns) */
#define UFT_SCP_BASE_CAPTURE_RES    25

/** SCP flux time base (25 ns) */
#define UFT_SCP_FLUX_TIME_BASE      25

/*============================================================================
 * Flux Transition Classification
 *============================================================================*/

/**
 * @brief Flux transition types
 */
typedef enum {
    UFT_FLUX_TOO_SHORT = 0,   /**< Too short (<2 clock ticks) */
    UFT_FLUX_SHORT     = 1,   /**< Short (2 clock ticks, 4µs typical) */
    UFT_FLUX_MEDIUM    = 2,   /**< Medium (3 clock ticks, 6µs typical) */
    UFT_FLUX_LONG      = 3,   /**< Long (4 clock ticks, 8µs typical) */
    UFT_FLUX_TOO_LONG  = 4,   /**< Too long (>4 clock ticks) */
    UFT_FLUX_ABNORMAL  = 5    /**< Abnormal/unclassified */
} uft_flux_type_t;

/**
 * @brief Data encoding types
 */
typedef enum {
    UFT_ENCODING_FM    = 0,   /**< FM (Single Density) */
    UFT_ENCODING_MFM   = 1,   /**< MFM (Double Density) */
    UFT_ENCODING_GCR   = 2,   /**< GCR (Apple/Commodore) */
    UFT_ENCODING_RAW   = 3    /**< Raw bitstream */
} uft_encoding_t;

/**
 * @brief Data rate enum
 */
typedef enum {
    UFT_RATE_125KBPS  = 125000,   /**< 125 Kbps (FM SD) */
    UFT_RATE_250KBPS  = 250000,   /**< 250 Kbps (MFM DD) */
    UFT_RATE_300KBPS  = 300000,   /**< 300 Kbps (MFM DD 360RPM) */
    UFT_RATE_500KBPS  = 500000,   /**< 500 Kbps (MFM HD) */
    UFT_RATE_1000KBPS = 1000000   /**< 1000 Kbps (MFM ED) */
} uft_data_rate_t;

/*============================================================================
 * Flux Statistics
 *============================================================================*/

/**
 * @brief Basic flux transition statistics
 */
typedef struct {
    uint32_t total;           /**< Total flux transitions */
    uint32_t short_count;     /**< Short transitions (2 ticks) */
    uint32_t medium_count;    /**< Medium transitions (3 ticks) */
    uint32_t long_count;      /**< Long transitions (4 ticks) */
    uint32_t too_short;       /**< Too short transitions */
    uint32_t too_long;        /**< Too long transitions */
    uint32_t too_slow_bits;   /**< Extra bits from slow transitions */
    double shortest_flux;     /**< Shortest flux time (seconds) */
    double longest_flux;      /**< Longest flux time (seconds) */
    double short_time_total;  /**< Total time of short fluxes */
} uft_flux_stats_t;

/**
 * @brief PLL decode statistics entry (per flux)
 */
typedef struct {
    double time;              /**< Absolute time position */
    double delta;             /**< Flux delta time */
    double predicted;         /**< Predicted flux time */
    double clock;             /**< Current PLL clock period */
    double window_min;        /**< Window minimum */
    double window_max;        /**< Window maximum */
    double phase_error;       /**< Phase error */
    double phase_integral;    /**< Integrated phase error */
} uft_pll_stat_entry_t;

/**
 * @brief Marker entry (sync word detection)
 */
typedef struct {
    double time;              /**< Time position of marker */
    size_t bitcell;           /**< Bitcell position */
    uint64_t pattern;         /**< Detected pattern */
} uft_pll_marker_t;

/*============================================================================
 * PLL Configuration
 *============================================================================*/

/**
 * @brief PLL configuration flags
 */
typedef enum {
    UFT_PLL_COLLECT_STATS    = 0x0001,  /**< Collect detailed statistics */
    UFT_PLL_COLLECT_ENUMS    = 0x0002,  /**< Collect flux classifications */
    UFT_PLL_DETECT_MARKERS   = 0x0004,  /**< Detect sync markers */
    UFT_PLL_DETECT_WEAK      = 0x0008,  /**< Detect weak bits */
    UFT_PLL_AGGRESSIVE       = 0x0010,  /**< Aggressive clock tracking */
    UFT_PLL_CONSERVATIVE     = 0x0020   /**< Conservative clock tracking */
} uft_pll_flags_t;

/**
 * @brief PLL state structure
 *
 * Software Phase-Locked Loop for flux-to-bitstream conversion.
 * Based on FluxFox's Pll struct with enhancements.
 */
typedef struct {
    /* Clock parameters */
    double default_rate;      /**< Default clock rate (Hz) */
    double current_rate;      /**< Current adjusted rate (Hz) */
    double base_period;       /**< Base clock period (seconds) */
    double working_period;    /**< Current working period (seconds) */
    double period_factor;     /**< Period adjustment factor */
    double max_adjust;        /**< Maximum adjustment (fraction) */
    double density_factor;    /**< Density factor (2.0 for MFM) */
    
    /* PLL gains */
    double clock_gain;        /**< Clock frequency gain (typ. 0.05) */
    double phase_gain;        /**< Phase adjustment gain (typ. 0.65) */
    
    /* Runtime state */
    double time;              /**< Current time position */
    double last_flux_time;    /**< Time of last flux transition */
    double phase_error;       /**< Current phase error */
    double phase_adjust;      /**< Phase adjustment accumulator */
    uint64_t clock_ticks;     /**< Total clock ticks */
    uint64_t ticks_since_flux;/**< Ticks since last flux */
    
    /* Shift register for marker detection */
    uint64_t shift_reg;       /**< 64-bit shift register */
    
    /* Adjustment gate (for gated clock adjustment) */
    int32_t adjust_gate;      /**< Adjustment gate counter */
    
    /* Configuration */
    uft_encoding_t encoding;  /**< Data encoding type */
    uint32_t flags;           /**< PLL flags */
} uft_pll_t;

/*============================================================================
 * Flux Revolution Structure
 *============================================================================*/

/**
 * @brief Single revolution of flux data
 */
typedef struct {
    double *deltas;           /**< Array of flux delta times (seconds) */
    size_t delta_count;       /**< Number of flux transitions */
    double revolution_time;   /**< Total revolution time (seconds) */
    double rpm;               /**< Calculated RPM */
    uint32_t index_time;      /**< Index time (raw format units) */
} uft_flux_revolution_t;

/**
 * @brief Flux stream track (multiple revolutions)
 */
typedef struct {
    uft_flux_revolution_t *revolutions;  /**< Array of revolutions */
    size_t revolution_count;             /**< Number of revolutions */
    uint16_t cylinder;                   /**< Cylinder number */
    uint8_t head;                        /**< Head number */
    double capture_resolution;           /**< Capture resolution (seconds) */
} uft_flux_track_t;

/*============================================================================
 * PLL Decode Result
 *============================================================================*/

/**
 * @brief PLL decode result structure
 */
typedef struct {
    /* Output bitstream */
    uint8_t *bits;            /**< Output bit array (packed bytes) */
    size_t bit_count;         /**< Number of valid bits */
    size_t byte_count;        /**< Number of bytes allocated */
    
    /* Error map */
    uint8_t *error_map;       /**< Error bit positions (packed) */
    
    /* Weak bit mask */
    uint8_t *weak_mask;       /**< Weak bit positions (packed) */
    
    /* Statistics */
    uft_flux_stats_t stats;   /**< Flux statistics */
    
    /* Detailed stats (if collected) */
    uft_pll_stat_entry_t *pll_stats;  /**< Per-flux statistics */
    size_t pll_stat_count;            /**< Number of stat entries */
    
    /* Flux classifications (if collected) */
    uft_flux_type_t *flux_types;      /**< Per-flux classifications */
    size_t flux_type_count;           /**< Number of classifications */
    
    /* Markers (if detected) */
    uft_pll_marker_t *markers;        /**< Detected markers */
    size_t marker_count;              /**< Number of markers */
} uft_pll_result_t;

/*============================================================================
 * MFM Marker Patterns
 *============================================================================*/

/** MFM sync pattern (0x4489 repeated) */
#define UFT_MFM_SYNC_PATTERN        0x4489448944894489ULL

/** MFM sync pattern mask */
#define UFT_MFM_SYNC_MASK           0xFFFFFFFFFFFFFFFFULL

/** MFM half sync + half marker (System34/Amiga detection) */
#define UFT_MFM_HALF_SYNC_MARKER    0x2AAAAAAA44894489ULL

/** MFM marker clock pattern */
#define UFT_MFM_MARKER_CLOCK        0x0220022002200000ULL

/** FM marker pattern (0xF57E) */
#define UFT_FM_MARKER_PATTERN       0xAAAAAAAAAAAAA02AULL

/*============================================================================
 * PLL API Functions
 *============================================================================*/

/**
 * @brief Initialize PLL with default parameters
 * @param pll PLL state to initialize
 * @param rate Data rate (Hz)
 * @param encoding Data encoding type
 */
static inline void uft_pll_init(uft_pll_t *pll, double rate, uft_encoding_t encoding)
{
    pll->default_rate = rate;
    pll->current_rate = rate;
    pll->base_period = 1.0 / rate;
    pll->working_period = pll->base_period;
    pll->period_factor = 1.0;
    pll->max_adjust = UFT_PLL_MAX_CLOCK_ADJUST;
    pll->density_factor = (encoding == UFT_ENCODING_MFM) ? 2.0 : 1.0;
    
    pll->clock_gain = 0.05;
    pll->phase_gain = 0.65;
    
    pll->time = 0.0;
    pll->last_flux_time = 0.0;
    pll->phase_error = 0.0;
    pll->phase_adjust = 0.0;
    pll->clock_ticks = 0;
    pll->ticks_since_flux = 0;
    pll->shift_reg = 0;
    pll->adjust_gate = 0;
    
    pll->encoding = encoding;
    pll->flags = 0;
}

/**
 * @brief Initialize PLL for MFM at 250 Kbps
 * @param pll PLL state to initialize
 */
static inline void uft_pll_init_mfm_250k(uft_pll_t *pll)
{
    uft_pll_init(pll, 500000.0, UFT_ENCODING_MFM);  /* 500KHz bit rate */
}

/**
 * @brief Initialize PLL for FM at 125 Kbps
 * @param pll PLL state to initialize
 */
static inline void uft_pll_init_fm_125k(uft_pll_t *pll)
{
    uft_pll_init(pll, 250000.0, UFT_ENCODING_FM);  /* 250KHz bit rate */
}

/**
 * @brief Reset PLL clock to default
 * @param pll PLL state
 */
static inline void uft_pll_reset_clock(uft_pll_t *pll)
{
    pll->current_rate = pll->default_rate;
    pll->working_period = pll->base_period;
}

/**
 * @brief Adjust PLL clock rate
 * @param pll PLL state
 * @param factor Adjustment factor (e.g., 1.01 for +1%)
 */
static inline void uft_pll_adjust_clock(uft_pll_t *pll, double factor)
{
    pll->current_rate *= factor;
    pll->working_period = 1.0 / pll->current_rate;
}

/**
 * @brief Classify a single flux transition
 * @param pll PLL state
 * @param duration Flux duration in seconds
 * @return Flux type classification
 */
static inline uft_flux_type_t uft_pll_classify_flux(const uft_pll_t *pll, double duration)
{
    if (fabs(duration - UFT_FLUX_SHORT_TIME) <= UFT_FLUX_TOLERANCE) {
        return UFT_FLUX_SHORT;
    } else if (fabs(duration - UFT_FLUX_MEDIUM_TIME) <= UFT_FLUX_TOLERANCE) {
        return UFT_FLUX_MEDIUM;
    } else if (fabs(duration - UFT_FLUX_LONG_TIME) <= UFT_FLUX_TOLERANCE) {
        return UFT_FLUX_LONG;
    } else if (duration < UFT_FLUX_SHORT_TIME - UFT_FLUX_TOLERANCE) {
        return UFT_FLUX_TOO_SHORT;
    } else {
        return UFT_FLUX_TOO_LONG;
    }
}

/**
 * @brief Process single flux transition (MFM)
 * @param pll PLL state
 * @param delta_time Time since last flux (seconds)
 * @param out_bits Output bit buffer
 * @param bit_pos Current bit position (updated)
 * @return Number of bits emitted
 *
 * Core MFM PLL algorithm from FluxFox:
 * 1. Count clock ticks until we pass flux time
 * 2. Emit (flux_length - 1) zeros followed by one 1
 * 3. Calculate phase error from window center
 * 4. Adjust phase and clock based on error
 */
static inline int uft_pll_process_flux_mfm(uft_pll_t *pll, double delta_time,
                                           uint8_t *out_bits, size_t *bit_pos)
{
    double this_flux_time = pll->last_flux_time + delta_time;
    double min_clock = pll->working_period * (1.0 - pll->max_adjust);
    double max_clock = pll->working_period * (1.0 + pll->max_adjust);
    int bits_emitted = 0;
    
    /* Apply phase adjustment and tick clock until we pass flux time */
    pll->time += pll->phase_adjust;
    while (pll->time < this_flux_time) {
        pll->time += pll->working_period;
        pll->ticks_since_flux++;
        pll->clock_ticks++;
    }
    
    uint64_t flux_length = pll->ticks_since_flux;
    
    /* Emit bits: (flux_length - 1) zeros, then one 1 */
    if (flux_length > 0) {
        size_t pos = *bit_pos;
        
        /* Emit zeros */
        for (uint64_t i = 0; i < flux_length - 1; i++) {
            size_t byte_idx = pos / 8;
            size_t bit_idx = 7 - (pos % 8);
            out_bits[byte_idx] &= ~(1 << bit_idx);  /* Clear bit (0) */
            pos++;
            pll->shift_reg <<= 1;
        }
        
        /* Emit 1 for flux transition */
        size_t byte_idx = pos / 8;
        size_t bit_idx = 7 - (pos % 8);
        out_bits[byte_idx] |= (1 << bit_idx);  /* Set bit (1) */
        pos++;
        pll->shift_reg = (pll->shift_reg << 1) | 1;
        
        bits_emitted = (int)flux_length;
        *bit_pos = pos;
    }
    
    /* Calculate phase error */
    double window_max = (pll->time - this_flux_time) + delta_time;
    double window_center = window_max - pll->working_period / 2.0;
    double last_phase_error = pll->phase_error;
    pll->phase_error = delta_time - window_center;
    
    /* Gated adjustment: only adjust clock if error persists */
    if (pll->phase_error < 0.0) {
        pll->adjust_gate = (pll->adjust_gate < 0) ? pll->adjust_gate - 1 : -1;
    } else {
        pll->adjust_gate = (pll->adjust_gate > 0) ? pll->adjust_gate + 1 : 1;
    }
    
    /* Use minimum phase error from last two fluxes for phase adjustment */
    double min_phase_error = (fabs(pll->phase_error) < fabs(last_phase_error)) 
                             ? pll->phase_error : last_phase_error;
    
    /* Phase adjustment */
    pll->phase_adjust = pll->phase_gain * min_phase_error;
    
    /* Clock adjustment (only if gate threshold met) */
    if (abs(pll->adjust_gate) > 1) {
        double clk_adjust = pll->clock_gain * pll->phase_error;
        pll->working_period += clk_adjust;
        
        /* Clamp clock to limits */
        if (pll->working_period < min_clock) pll->working_period = min_clock;
        if (pll->working_period > max_clock) pll->working_period = max_clock;
    }
    
    /* Update state for next flux */
    pll->ticks_since_flux = 0;
    pll->last_flux_time = this_flux_time;
    
    return bits_emitted;
}

/**
 * @brief Check for MFM marker in shift register
 * @param pll PLL state
 * @return true if marker detected
 */
static inline bool uft_pll_check_mfm_marker(const uft_pll_t *pll)
{
    /* Check for System34/Amiga sync pattern */
    return ((pll->shift_reg & ~0x8000000000000000ULL) == UFT_MFM_HALF_SYNC_MARKER);
}

/**
 * @brief Check for FM marker in shift register
 * @param pll PLL state
 * @return true if marker detected
 */
static inline bool uft_pll_check_fm_marker(const uft_pll_t *pll)
{
    return ((pll->shift_reg & 0xFFFFFFFFFFFFFFFFULL) == UFT_FM_MARKER_PATTERN);
}

/*============================================================================
 * Flux Data Conversion
 *============================================================================*/

/**
 * @brief Convert raw SCP flux data to delta times
 * @param raw_data Raw 16-bit big-endian flux values
 * @param count Number of flux values
 * @param resolution Capture resolution in nanoseconds
 * @param out_deltas Output delta array (must be pre-allocated)
 * @return Number of valid deltas
 *
 * Handles SCP overflow (0x0000) values properly.
 */
static inline size_t uft_scp_convert_flux(const uint16_t *raw_data, size_t count,
                                          uint32_t resolution, double *out_deltas)
{
    double res_seconds = resolution * 1e-9;
    size_t out_count = 0;
    uint64_t accumulator = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint16_t val = raw_data[i];
        
        if (val == 0) {
            /* Overflow - add 0xFFFF to accumulator */
            accumulator += 0xFFFF;
        } else {
            /* Valid flux - emit accumulated time */
            out_deltas[out_count++] = ((double)(val + accumulator)) * res_seconds;
            accumulator = 0;
        }
    }
    
    return out_count;
}

/**
 * @brief Convert Kryoflux flux data to delta times
 * @param flux_values Raw flux values (Kryoflux format)
 * @param count Number of values
 * @param sck Sample clock frequency (Hz)
 * @param out_deltas Output delta array
 * @return Number of valid deltas
 */
static inline size_t uft_kfx_convert_flux(const uint32_t *flux_values, size_t count,
                                          double sck, double *out_deltas)
{
    size_t out_count = 0;
    uint32_t overflow = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t val = flux_values[i];
        
        if (val < 0x08) {
            /* Flux1 - single byte value */
            out_deltas[out_count++] = (double)(overflow + val) / sck;
            overflow = 0;
        } else if (val == 0x0A) {
            /* Nop2 - skip */
        } else if (val == 0x0B) {
            /* Nop3 - skip */
        } else if (val == 0x0C) {
            /* Overflow - add to accumulator */
            overflow += 0x10000;
        }
        /* OOB blocks (0x0D) handled elsewhere */
    }
    
    return out_count;
}

/*============================================================================
 * Weak Bit Detection
 *============================================================================*/

/**
 * @brief Detect weak bit regions in bitstream
 * @param bits Packed bit array
 * @param bit_count Number of bits
 * @param run_length Minimum zero run for weak detection (typically 6)
 * @param out_regions Output region array
 * @param max_regions Maximum regions to detect
 * @return Number of weak regions found
 */
static inline size_t uft_detect_weak_bits(const uint8_t *bits, size_t bit_count,
                                          size_t run_length, 
                                          size_t *out_starts, size_t *out_ends,
                                          size_t max_regions)
{
    size_t region_count = 0;
    size_t zero_count = 0;
    size_t region_start = 0;
    bool in_region = false;
    
    for (size_t i = 0; i < bit_count && region_count < max_regions; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = 7 - (i % 8);
        bool bit = (bits[byte_idx] >> bit_idx) & 1;
        
        if (!bit) {
            if (!in_region && zero_count == run_length) {
                region_start = i - run_length;
                in_region = true;
            }
            zero_count++;
        } else {
            if (in_region) {
                out_starts[region_count] = region_start;
                out_ends[region_count] = i - 1;
                region_count++;
                in_region = false;
            }
            zero_count = 0;
        }
    }
    
    return region_count;
}

/**
 * @brief Create weak bit mask from bitstream
 * @param bits Input bitstream
 * @param bit_count Number of bits
 * @param run_length Minimum zero run (typically 6)
 * @param out_mask Output weak mask (same size as bits)
 */
static inline void uft_create_weak_mask(const uint8_t *bits, size_t bit_count,
                                        size_t run_length, uint8_t *out_mask)
{
    size_t zero_count = 0;
    
    for (size_t i = 0; i < bit_count; i++) {
        size_t byte_idx = i / 8;
        size_t bit_idx = 7 - (i % 8);
        bool bit = (bits[byte_idx] >> bit_idx) & 1;
        
        if (!bit) {
            zero_count++;
        } else {
            zero_count = 0;
        }
        
        /* Mark as weak if in a zero run longer than threshold */
        if (zero_count > run_length) {
            out_mask[byte_idx] |= (1 << bit_idx);
        } else {
            out_mask[byte_idx] &= ~(1 << bit_idx);
        }
    }
}

/*============================================================================
 * Clock Rate Detection
 *============================================================================*/

/**
 * @brief Histogram bucket for flux analysis
 */
typedef struct {
    double center;            /**< Bucket center time */
    uint32_t count;           /**< Number of fluxes in bucket */
} uft_histogram_bucket_t;

/**
 * @brief Estimate data rate from flux stream
 * @param deltas Flux delta times (seconds)
 * @param count Number of deltas
 * @return Estimated data rate in Hz, or 0 if unable to determine
 *
 * Uses histogram analysis to find most common flux times
 * and deduce the underlying data rate.
 */
static inline double uft_estimate_data_rate(const double *deltas, size_t count)
{
    if (count < 100) return 0.0;
    
    /* Build simple histogram with 100ns buckets */
    #define HIST_BUCKETS 200
    uint32_t histogram[HIST_BUCKETS] = {0};
    double bucket_size = 100e-9;  /* 100 ns */
    
    for (size_t i = 0; i < count; i++) {
        int bucket = (int)(deltas[i] / bucket_size);
        if (bucket >= 0 && bucket < HIST_BUCKETS) {
            histogram[bucket]++;
        }
    }
    
    /* Find the dominant peak (should be short flux) */
    uint32_t max_count = 0;
    int peak_bucket = 0;
    for (int i = 20; i < 100; i++) {  /* 2-10 µs range */
        if (histogram[i] > max_count) {
            max_count = histogram[i];
            peak_bucket = i;
        }
    }
    
    double peak_time = (peak_bucket + 0.5) * bucket_size;
    
    /* For MFM, short flux is 2 bit cells = 4µs at 250Kbps */
    /* Therefore bit cell time = peak_time / 2 */
    /* And data rate = 1 / (bit_cell_time * 2) for MFM */
    
    if (peak_time > 3.5e-6 && peak_time < 4.5e-6) {
        return 250000.0;  /* 250 Kbps */
    } else if (peak_time > 2.5e-6 && peak_time < 3.5e-6) {
        return 300000.0;  /* 300 Kbps */
    } else if (peak_time > 1.5e-6 && peak_time < 2.5e-6) {
        return 500000.0;  /* 500 Kbps */
    }
    
    /* Fall back to calculation */
    return 1.0 / (peak_time / 2.0);
    
    #undef HIST_BUCKETS
}

/**
 * @brief Calculate RPM from revolution time
 * @param revolution_time Time for one revolution (seconds)
 * @return RPM value
 */
static inline double uft_revolution_to_rpm(double revolution_time)
{
    if (revolution_time <= 0) return 0.0;
    return 60.0 / revolution_time;
}

/**
 * @brief Estimate RPM from flux data
 * @param index_times Array of index times (seconds)
 * @param count Number of index times (need at least 2)
 * @return Estimated RPM
 */
static inline double uft_estimate_rpm(const double *index_times, size_t count)
{
    if (count < 2) return 0.0;
    
    double total_time = 0.0;
    for (size_t i = 1; i < count; i++) {
        total_time += index_times[i] - index_times[i-1];
    }
    
    double avg_revolution = total_time / (count - 1);
    return uft_revolution_to_rpm(avg_revolution);
}

/*============================================================================
 * Memory Management
 *============================================================================*/

/**
 * @brief Allocate PLL result structure
 * @param estimated_bits Estimated number of output bits
 * @param flags PLL flags determining what to allocate
 * @return Allocated result or NULL on failure
 */
uft_pll_result_t *uft_pll_result_alloc(size_t estimated_bits, uint32_t flags);

/**
 * @brief Free PLL result structure
 * @param result Result to free
 */
void uft_pll_result_free(uft_pll_result_t *result);

/**
 * @brief Allocate flux revolution structure
 * @param delta_count Number of flux deltas
 * @return Allocated revolution or NULL
 */
uft_flux_revolution_t *uft_flux_revolution_alloc(size_t delta_count);

/**
 * @brief Free flux revolution structure
 * @param rev Revolution to free
 */
void uft_flux_revolution_free(uft_flux_revolution_t *rev);

/*============================================================================
 * High-Level Decode Functions
 *============================================================================*/

/**
 * @brief Decode flux revolution to bitstream
 * @param pll Initialized PLL state
 * @param rev Flux revolution data
 * @param flags Decode flags
 * @return Decode result (caller must free)
 */
uft_pll_result_t *uft_pll_decode_revolution(uft_pll_t *pll, 
                                             const uft_flux_revolution_t *rev,
                                             uint32_t flags);

/**
 * @brief Decode multiple revolutions and select best
 * @param pll Initialized PLL state
 * @param track Track with multiple revolutions
 * @param flags Decode flags
 * @return Best decode result (caller must free)
 */
uft_pll_result_t *uft_pll_decode_track(uft_pll_t *pll,
                                        const uft_flux_track_t *track,
                                        uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_PLL_H */

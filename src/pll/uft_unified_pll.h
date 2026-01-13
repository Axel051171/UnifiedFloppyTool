/**
 * @file uft_unified_pll.h
 * @brief Unified Phase-Locked Loop Interface
 * 
 * P1-003: PLL Konsolidierung (18 → 1)
 * 
 * Ein PLL-Interface mit konfigurierbaren Presets für alle Formate.
 * Ersetzt: adaptive_pll, kalman_pll, mfm_pll, flux_pll, pi_pll, etc.
 * 
 * USAGE:
 * ```c
 * uft_pll_t pll;
 * uft_pll_init(&pll, UFT_PLL_PRESET_AMIGA);
 * 
 * while (has_flux_data) {
 *     uft_pll_result_t result;
 *     uft_pll_process_transition(&pll, flux_time, &result);
 *     
 *     if (result.bit_valid) {
 *         output_bit(result.bit_value);
 *     }
 * }
 * 
 * uft_pll_stats_t stats;
 * uft_pll_get_stats(&pll, &stats);
 * ```
 */

#ifndef UFT_UNIFIED_PLL_H
#define UFT_UNIFIED_PLL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * PLL Presets (Format-spezifische Konfigurationen)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Amiga / Atari ST MFM */
    UFT_PLL_PRESET_AMIGA_DD = 0,    /* 2µs bit cell, 4µs/6µs/8µs patterns */
    UFT_PLL_PRESET_AMIGA_HD,        /* 1µs bit cell */
    UFT_PLL_PRESET_ATARI_ST,        /* Same as Amiga DD */
    
    /* IBM PC MFM */
    UFT_PLL_PRESET_IBM_DD,          /* 500kbps */
    UFT_PLL_PRESET_IBM_HD,          /* 1000kbps */
    UFT_PLL_PRESET_IBM_ED,          /* 2000kbps (2.88MB) */
    
    /* Commodore GCR */
    UFT_PLL_PRESET_C64_1541,        /* Zone-based, 4 speeds */
    UFT_PLL_PRESET_C64_1571,        /* Double-sided */
    UFT_PLL_PRESET_C128_1581,       /* MFM, 800KB */
    
    /* Apple */
    UFT_PLL_PRESET_APPLE_II_GCR,    /* 5.25" GCR */
    UFT_PLL_PRESET_APPLE_35_GCR,    /* 3.5" Sony GCR */
    UFT_PLL_PRESET_APPLE_35_MFM,    /* 3.5" Superdrive MFM */
    
    /* FM / Single Density */
    UFT_PLL_PRESET_FM_SD,           /* 125kbps FM */
    UFT_PLL_PRESET_FM_DD,           /* 250kbps FM */
    
    /* Specials */
    UFT_PLL_PRESET_PROTECTION,      /* High tolerance for copy protection */
    UFT_PLL_PRESET_DAMAGED,         /* Very high tolerance for damaged disks */
    UFT_PLL_PRESET_CUSTOM,          /* User-defined parameters */
    
    UFT_PLL_PRESET_COUNT
} uft_pll_preset_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * PLL Algorithm Selection
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_PLL_ALGO_SIMPLE,        /* Simple threshold-based */
    UFT_PLL_ALGO_PI,            /* PI Controller (FluxEngine-style) */
    UFT_PLL_ALGO_ADAPTIVE,      /* Adaptive gain */
    UFT_PLL_ALGO_KALMAN,        /* Kalman filter */
    UFT_PLL_ALGO_DPLL,          /* WD1772-style DPLL */
} uft_pll_algo_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * PLL Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Core Parameters */
    double          nominal_bitcell_ns;     /* Nominal bit cell time in ns */
    double          clock_rate_hz;          /* Sample clock rate (e.g. 24MHz) */
    
    /* PI Controller */
    double          phase_gain;             /* Phase correction gain (0.0-1.0) */
    double          freq_gain;              /* Frequency correction gain (0.0-1.0) */
    
    /* Tolerances */
    double          window_tolerance;       /* Window as fraction of bit cell (0.3-0.6) */
    double          bit_error_tolerance;    /* Max acceptable bit error rate */
    
    /* Adaptive Settings */
    bool            adaptive_enabled;       /* Enable adaptive gain adjustment */
    double          adaptive_min_gain;      /* Minimum gain when locked */
    double          adaptive_max_gain;      /* Maximum gain when searching */
    int             lock_threshold;         /* Bits to consider locked */
    
    /* Kalman Filter */
    double          process_noise;          /* Process noise (Q) */
    double          measurement_noise;      /* Measurement noise (R) */
    
    /* Algorithm Selection */
    uft_pll_algo_t  algorithm;
    
    /* Debug/Diagnostics */
    bool            record_history;         /* Record timing history */
    int             max_history;            /* Max history entries */
} uft_pll_config_t;

/* Default configurations */
extern const uft_pll_config_t UFT_PLL_CONFIG_DEFAULT;
extern const uft_pll_config_t UFT_PLL_CONFIG_PRESETS[UFT_PLL_PRESET_COUNT];

/* ═══════════════════════════════════════════════════════════════════════════════
 * PLL State and Results
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Current State */
    double          current_bitcell;        /* Current estimated bit cell time */
    double          phase_error;            /* Current phase error */
    double          freq_error;             /* Current frequency error */
    
    /* Kalman State (if applicable) */
    double          kalman_state;
    double          kalman_covariance;
    
    /* Lock Status */
    bool            is_locked;
    int             bits_since_lock;
    int             bits_since_error;
    
    /* Accumulated */
    double          accumulated_phase;
    uint64_t        total_transitions;
} uft_pll_state_t;

typedef struct {
    bool            bit_valid;              /* A bit was decoded */
    uint8_t         bit_value;              /* 0 or 1 */
    int             bit_count;              /* Number of bits (for long gaps) */
    
    double          phase_error;            /* Phase error for this transition */
    double          confidence;             /* 0.0-1.0 confidence */
    
    bool            is_sync;                /* Could be sync pattern */
    bool            timing_error;           /* Timing outside tolerance */
} uft_pll_result_t;

typedef struct {
    uint64_t        total_bits;
    uint64_t        total_transitions;
    uint64_t        sync_patterns;
    uint64_t        timing_errors;
    
    double          avg_phase_error;
    double          max_phase_error;
    double          min_bitcell_ns;
    double          max_bitcell_ns;
    double          avg_bitcell_ns;
    
    double          bit_error_rate;
    double          lock_percentage;
    
    double          processing_time_ms;
} uft_pll_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main PLL Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_pll_config_t    config;
    uft_pll_state_t     state;
    uft_pll_stats_t     stats;
    
    /* History for debugging */
    struct {
        double          *transitions;
        double          *bitcells;
        double          *errors;
        int             count;
        int             capacity;
    } history;
} uft_pll_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize PLL with preset
 * @param pll PLL structure
 * @param preset Format preset
 * @return 0 on success
 */
int uft_pll_init(uft_pll_t *pll, uft_pll_preset_t preset);

/**
 * @brief Initialize PLL with custom config
 * @param pll PLL structure
 * @param config Custom configuration
 * @return 0 on success
 */
int uft_pll_init_custom(uft_pll_t *pll, const uft_pll_config_t *config);

/**
 * @brief Reset PLL state (keep config)
 */
void uft_pll_reset(uft_pll_t *pll);

/**
 * @brief Free PLL resources
 */
void uft_pll_free(uft_pll_t *pll);

/**
 * @brief Process a flux transition
 * @param pll PLL structure
 * @param flux_time_ns Time since last transition in nanoseconds
 * @param result Output result
 * @return Number of bits decoded (0 or more)
 */
int uft_pll_process_transition(uft_pll_t *pll, double flux_time_ns, 
                                uft_pll_result_t *result);

/**
 * @brief Process array of flux times
 * @param pll PLL structure
 * @param flux_times Array of flux times (ns)
 * @param count Number of times
 * @param bits Output bit buffer
 * @param max_bits Maximum bits to output
 * @return Number of bits decoded
 */
int uft_pll_process_array(uft_pll_t *pll, const double *flux_times, size_t count,
                          uint8_t *bits, size_t max_bits);

/**
 * @brief Get current PLL state
 */
void uft_pll_get_state(const uft_pll_t *pll, uft_pll_state_t *state);

/**
 * @brief Get accumulated statistics
 */
void uft_pll_get_stats(const uft_pll_t *pll, uft_pll_stats_t *stats);

/**
 * @brief Set phase gain (for GUI control)
 */
void uft_pll_set_phase_gain(uft_pll_t *pll, double gain);

/**
 * @brief Set frequency gain (for GUI control)
 */
void uft_pll_set_freq_gain(uft_pll_t *pll, double gain);

/**
 * @brief Set window tolerance (for GUI control)
 */
void uft_pll_set_window(uft_pll_t *pll, double tolerance);

/**
 * @brief Force resync
 */
void uft_pll_resync(uft_pll_t *pll);

/**
 * @brief Check if PLL is locked
 */
bool uft_pll_is_locked(const uft_pll_t *pll);

/**
 * @brief Get preset name
 */
const char* uft_pll_preset_name(uft_pll_preset_t preset);

/**
 * @brief Get algorithm name
 */
const char* uft_pll_algo_name(uft_pll_algo_t algo);

/**
 * @brief Export history for debugging
 */
int uft_pll_export_history(const uft_pll_t *pll, const char *csv_path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Convenience Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Convert microseconds to nanoseconds */
#define UFT_US_TO_NS(us)    ((us) * 1000.0)

/* Standard bit cell times in nanoseconds */
#define UFT_BITCELL_AMIGA_DD    2000.0      /* 2µs = 500kbps */
#define UFT_BITCELL_AMIGA_HD    1000.0      /* 1µs = 1Mbps */
#define UFT_BITCELL_IBM_DD      2000.0      /* 2µs = 500kbps */
#define UFT_BITCELL_IBM_HD      1000.0      /* 1µs = 1Mbps */
#define UFT_BITCELL_IBM_ED       500.0      /* 0.5µs = 2Mbps */
#define UFT_BITCELL_C64_ZONE0   3200.0      /* ~312.5kbps */
#define UFT_BITCELL_C64_ZONE1   2933.0      /* ~341kbps */
#define UFT_BITCELL_C64_ZONE2   2667.0      /* ~375kbps */
#define UFT_BITCELL_C64_ZONE3   2500.0      /* ~400kbps */
#define UFT_BITCELL_FM_SD       4000.0      /* 4µs = 250kbps */

#ifdef __cplusplus
}
#endif

#endif /* UFT_UNIFIED_PLL_H */

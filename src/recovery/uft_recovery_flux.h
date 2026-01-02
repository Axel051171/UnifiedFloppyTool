/**
 * @file uft_recovery_flux.h
 * @brief GOD MODE Flux-Level Recovery Engine
 * 
 * Recovery heißt nicht: "Mach es wieder gut"
 * Sondern: "Finde heraus, was wirklich da ist – und beweise es."
 * 
 * Physische/Flux-Ebene Recovery:
 * - Multi-Revolution-Reads (N-Revs, adaptiv)
 * - Bitweise Mehrheitsentscheidung (Vote pro Bit)
 * - Adaptive PLL (global/pro Track/pro Region)
 * - RPM-Drift-Kompensation
 * - Cell-Width-Histogramme
 * - Dropout-Erkennung
 * - Weak-Bit-Erkennung
 * - Noise-Filter (nicht destruktiv)
 * - Timing-Normalisierung als Hypothese
 * - Flux-Preservation (roh speichern)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_FLUX_H
#define UFT_RECOVERY_FLUX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_MAX_REVOLUTIONS         32      /**< Maximum revolutions to analyze */
#define UFT_FLUX_HISTOGRAM_BINS     256     /**< Histogram resolution */
#define UFT_MIN_CONFIDENCE          50      /**< Minimum confidence threshold */

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Flux transition sample
 */
typedef struct {
    uint32_t time_ns;           /**< Time in nanoseconds from index */
    uint8_t  confidence;        /**< Confidence 0-100 */
    uint8_t  source_rev;        /**< Source revolution */
    uint16_t flags;             /**< Flags (dropout, weak, noise) */
} uft_flux_sample_t;

#define UFT_FLUX_FLAG_DROPOUT       0x0001  /**< Dropout detected */
#define UFT_FLUX_FLAG_WEAK          0x0002  /**< Weak/unstable bit */
#define UFT_FLUX_FLAG_NOISE         0x0004  /**< Noise artifact */
#define UFT_FLUX_FLAG_INTERPOLATED  0x0008  /**< Interpolated value */
#define UFT_FLUX_FLAG_VOTED         0x0010  /**< Result of voting */
#define UFT_FLUX_FLAG_ORIGINAL      0x0020  /**< Original preserved */

/**
 * @brief Single revolution data
 */
typedef struct {
    uft_flux_sample_t *samples;     /**< Flux samples */
    size_t             sample_count; /**< Number of samples */
    uint32_t           index_time;   /**< Index-to-index time (ns) */
    double             rpm;          /**< Measured RPM */
    uint8_t            quality;      /**< Overall quality 0-100 */
    bool               has_index;    /**< Valid index pulse */
} uft_revolution_t;

/**
 * @brief Multi-revolution container
 */
typedef struct {
    uft_revolution_t *revs;         /**< Array of revolutions */
    size_t            rev_count;    /**< Number of revolutions */
    uint8_t           track;        /**< Track number */
    uint8_t           head;         /**< Head/side */
    double            avg_rpm;      /**< Average RPM */
    double            rpm_variance; /**< RPM variance */
} uft_multi_rev_t;

/**
 * @brief Cell width histogram
 */
typedef struct {
    uint32_t bins[UFT_FLUX_HISTOGRAM_BINS];  /**< Histogram bins */
    uint32_t total_samples;         /**< Total samples */
    double   bin_width_ns;          /**< Width per bin in ns */
    double   peak_2t;               /**< 2T peak position */
    double   peak_3t;               /**< 3T peak position */
    double   peak_4t;               /**< 4T peak position */
    double   nominal_cell;          /**< Detected nominal cell width */
    double   cell_variance;         /**< Cell width variance */
} uft_cell_histogram_t;

/**
 * @brief PLL state for adaptive decoding
 */
typedef struct {
    double   clock_period;          /**< Current clock period (ns) */
    double   phase;                 /**< Current phase */
    double   freq_gain;             /**< Frequency gain (Kf) */
    double   phase_gain;            /**< Phase gain (Kp) */
    double   lock_threshold;        /**< Lock detection threshold */
    bool     is_locked;             /**< PLL is locked */
    uint32_t lock_time;             /**< Time to lock (samples) */
    
    /* Adaptive parameters */
    double   min_clock;             /**< Minimum clock period */
    double   max_clock;             /**< Maximum clock period */
    double   adapt_rate;            /**< Adaptation rate */
    
    /* Per-region tracking */
    double  *region_clocks;         /**< Clock per region */
    size_t   region_count;          /**< Number of regions */
} uft_adaptive_pll_t;

/**
 * @brief RPM drift compensation data
 */
typedef struct {
    double  *rpm_profile;           /**< RPM at each sample point */
    size_t   profile_len;           /**< Profile length */
    double   start_rpm;             /**< RPM at start */
    double   end_rpm;               /**< RPM at end */
    double   drift_rate;            /**< Drift rate (RPM/sample) */
    double   max_deviation;         /**< Maximum deviation from nominal */
    bool     compensated;           /**< Compensation applied */
} uft_rpm_drift_t;

/**
 * @brief Dropout region
 */
typedef struct {
    uint32_t start_ns;              /**< Start position (ns) */
    uint32_t end_ns;                /**< End position (ns) */
    uint32_t duration_ns;           /**< Duration (ns) */
    uint8_t  severity;              /**< Severity 0-100 */
    uint8_t  rev_index;             /**< Revolution where detected */
    bool     recovered;             /**< Successfully recovered from other revs */
} uft_dropout_region_t;

/**
 * @brief Weak bit zone
 */
typedef struct {
    uint32_t start_ns;              /**< Start position (ns) */
    uint32_t end_ns;                /**< End position (ns) */
    uint8_t  variability;           /**< How much it varies 0-100 */
    uint8_t  vote_result;           /**< Majority vote result */
    uint8_t  vote_confidence;       /**< Vote confidence */
    bool     is_protection;         /**< Likely copy protection */
} uft_weak_zone_t;

/**
 * @brief Timing hypothesis
 */
typedef struct {
    double   cell_width;            /**< Hypothesized cell width */
    double   score;                 /**< Score for this hypothesis */
    uint32_t sync_matches;          /**< Number of sync pattern matches */
    uint32_t crc_passes;            /**< Number of CRC passes */
    bool     is_best;               /**< Currently best hypothesis */
} uft_timing_hypothesis_t;

/**
 * @brief Flux recovery context
 */
typedef struct {
    /* Input data */
    uft_multi_rev_t      *multi_rev;
    
    /* Analysis results */
    uft_cell_histogram_t  histogram;
    uft_adaptive_pll_t    pll;
    uft_rpm_drift_t       drift;
    
    /* Detected issues */
    uft_dropout_region_t *dropouts;
    size_t                dropout_count;
    uft_weak_zone_t      *weak_zones;
    size_t                weak_zone_count;
    
    /* Hypotheses */
    uft_timing_hypothesis_t *hypotheses;
    size_t                   hypothesis_count;
    
    /* Output */
    uft_flux_sample_t    *recovered;
    size_t                recovered_count;
    
    /* Options */
    bool preserve_original;         /**< Keep original flux data */
    bool mark_uncertain;            /**< Mark uncertain regions */
    uint8_t min_revs_for_vote;      /**< Minimum revs for voting */
} uft_flux_recovery_ctx_t;

/* ============================================================================
 * Multi-Revolution Voting
 * ============================================================================ */

/**
 * @brief Create multi-revolution container
 */
uft_multi_rev_t* uft_multi_rev_create(uint8_t track, uint8_t head, size_t max_revs);

/**
 * @brief Add revolution to container
 */
bool uft_multi_rev_add(uft_multi_rev_t *mr, const uft_flux_sample_t *samples,
                       size_t count, uint32_t index_time);

/**
 * @brief Free multi-revolution container
 */
void uft_multi_rev_free(uft_multi_rev_t *mr);

/**
 * @brief Perform N-rev bit voting
 * 
 * Bitweise Mehrheitsentscheidung über N Revolutionen.
 * Ergebnis: Für jedes Bit der wahrscheinlichste Wert + Confidence.
 * 
 * @param mr            Multi-revolution data
 * @param output        Output: voted flux samples
 * @param confidence    Output: per-sample confidence
 * @return Number of samples in output
 */
size_t uft_flux_vote_bits(const uft_multi_rev_t *mr,
                          uft_flux_sample_t **output,
                          uint8_t **confidence);

/**
 * @brief Adaptive voting with quality weighting
 * 
 * Gewichtet Stimmen nach Revolutionsqualität.
 */
size_t uft_flux_vote_adaptive(const uft_multi_rev_t *mr,
                              uft_flux_sample_t **output,
                              uint8_t **confidence);

/* ============================================================================
 * Cell Width Analysis
 * ============================================================================ */

/**
 * @brief Build cell width histogram
 */
void uft_flux_build_histogram(const uft_flux_sample_t *samples, size_t count,
                              uft_cell_histogram_t *hist);

/**
 * @brief Detect peaks in histogram (2T, 3T, 4T)
 */
void uft_flux_detect_peaks(uft_cell_histogram_t *hist);

/**
 * @brief Calculate nominal cell width from peaks
 */
double uft_flux_calc_nominal_cell(const uft_cell_histogram_t *hist);

/**
 * @brief Analyze cell width variance
 */
double uft_flux_analyze_variance(const uft_cell_histogram_t *hist);

/* ============================================================================
 * Adaptive PLL
 * ============================================================================ */

/**
 * @brief Initialize adaptive PLL
 */
void uft_pll_init(uft_adaptive_pll_t *pll, double nominal_cell,
                  double freq_gain, double phase_gain);

/**
 * @brief Process flux sample through PLL
 * 
 * @param pll           PLL state
 * @param transition_ns Transition time in ns
 * @return Decoded bits (packed) and count in upper byte
 */
uint32_t uft_pll_process(uft_adaptive_pll_t *pll, uint32_t transition_ns);

/**
 * @brief Enable per-region adaptation
 */
void uft_pll_enable_regions(uft_adaptive_pll_t *pll, size_t region_count);

/**
 * @brief Get optimal clock for region
 */
double uft_pll_get_region_clock(const uft_adaptive_pll_t *pll, size_t region);

/**
 * @brief Force PLL resync at position
 */
void uft_pll_force_resync(uft_adaptive_pll_t *pll, double new_clock);

/* ============================================================================
 * RPM Drift Compensation
 * ============================================================================ */

/**
 * @brief Analyze RPM drift across revolution
 */
void uft_flux_analyze_rpm_drift(const uft_revolution_t *rev, uft_rpm_drift_t *drift);

/**
 * @brief Compensate flux times for RPM drift
 * 
 * Passt alle Zeitstempel an, um RPM-Drift zu kompensieren.
 * Original bleibt erhalten wenn preserve_original gesetzt.
 */
void uft_flux_compensate_drift(uft_flux_sample_t *samples, size_t count,
                               const uft_rpm_drift_t *drift);

/**
 * @brief Compare revolutions for drift consistency
 */
double uft_flux_compare_rev_drift(const uft_revolution_t *rev1,
                                  const uft_revolution_t *rev2);

/* ============================================================================
 * Dropout Detection & Recovery
 * ============================================================================ */

/**
 * @brief Detect dropout regions
 * 
 * Ein Dropout ist ein Bereich ohne gültige Flux-Übergänge.
 */
size_t uft_flux_detect_dropouts(const uft_flux_sample_t *samples, size_t count,
                                double nominal_cell,
                                uft_dropout_region_t **dropouts);

/**
 * @brief Attempt to recover dropout from other revolutions
 */
bool uft_flux_recover_dropout(const uft_multi_rev_t *mr,
                              const uft_dropout_region_t *dropout,
                              uft_flux_sample_t **recovered,
                              size_t *recovered_count);

/**
 * @brief Mark dropout regions in output
 */
void uft_flux_mark_dropouts(uft_flux_sample_t *samples, size_t count,
                            const uft_dropout_region_t *dropouts, size_t dropout_count);

/* ============================================================================
 * Weak Bit Detection
 * ============================================================================ */

/**
 * @brief Detect weak bit zones from multi-rev comparison
 */
size_t uft_flux_detect_weak_zones(const uft_multi_rev_t *mr,
                                  uft_weak_zone_t **zones);

/**
 * @brief Classify weak zone (random, protection, damage)
 */
const char* uft_flux_classify_weak_zone(const uft_weak_zone_t *zone);

/**
 * @brief Preserve weak bit zones (don't "fix" them)
 */
void uft_flux_preserve_weak_zones(uft_flux_sample_t *samples, size_t count,
                                  const uft_weak_zone_t *zones, size_t zone_count);

/* ============================================================================
 * Noise Filter (Non-Destructive)
 * ============================================================================ */

/**
 * @brief Detect noise artifacts
 * 
 * Identifiziert Rauschen ohne es zu entfernen.
 * Markiert nur mit UFT_FLUX_FLAG_NOISE.
 */
size_t uft_flux_detect_noise(uft_flux_sample_t *samples, size_t count,
                             double nominal_cell, double threshold);

/**
 * @brief Get noise-filtered view (does not modify original)
 * 
 * Erstellt eine gefilterte Kopie. Original bleibt erhalten!
 */
size_t uft_flux_get_filtered(const uft_flux_sample_t *samples, size_t count,
                             uft_flux_sample_t **filtered);

/* ============================================================================
 * Timing Hypotheses
 * ============================================================================ */

/**
 * @brief Generate timing hypotheses
 * 
 * Erzeugt mehrere Hypothesen für mögliche Timing-Parameter.
 * Keine wird als "richtig" angenommen.
 */
size_t uft_flux_generate_hypotheses(const uft_flux_sample_t *samples, size_t count,
                                    uft_timing_hypothesis_t **hypotheses);

/**
 * @brief Score hypothesis against data
 */
void uft_flux_score_hypothesis(const uft_flux_sample_t *samples, size_t count,
                               uft_timing_hypothesis_t *hypothesis);

/**
 * @brief Get best hypothesis (but keep alternatives!)
 */
const uft_timing_hypothesis_t* uft_flux_get_best_hypothesis(
    const uft_timing_hypothesis_t *hypotheses, size_t count);

/* ============================================================================
 * Flux Preservation
 * ============================================================================ */

/**
 * @brief Create preservation snapshot
 * 
 * Speichert alle Rohdaten bevor irgendwelche Operationen.
 */
typedef struct {
    uft_flux_sample_t *original;    /**< Original flux data */
    size_t             original_count;
    uint64_t           checksum;    /**< Checksum for verification */
    char              *source_desc; /**< Source description */
} uft_flux_preservation_t;

uft_flux_preservation_t* uft_flux_preserve(const uft_flux_sample_t *samples,
                                            size_t count,
                                            const char *source_desc);

/**
 * @brief Verify preservation integrity
 */
bool uft_flux_verify_preservation(const uft_flux_preservation_t *pres);

/**
 * @brief Restore from preservation
 */
void uft_flux_restore(const uft_flux_preservation_t *pres,
                      uft_flux_sample_t **samples, size_t *count);

/**
 * @brief Free preservation
 */
void uft_flux_preservation_free(uft_flux_preservation_t *pres);

/* ============================================================================
 * Full Recovery Context
 * ============================================================================ */

/**
 * @brief Create flux recovery context
 */
uft_flux_recovery_ctx_t* uft_flux_recovery_create(void);

/**
 * @brief Run full flux-level recovery
 * 
 * Führt alle Analysen durch, ändert aber nichts ohne Bestätigung.
 */
void uft_flux_recovery_analyze(uft_flux_recovery_ctx_t *ctx);

/**
 * @brief Generate recovery report
 */
char* uft_flux_recovery_report(const uft_flux_recovery_ctx_t *ctx);

/**
 * @brief Free recovery context
 */
void uft_flux_recovery_free(uft_flux_recovery_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_FLUX_H */

/**
 * @file floppy_otdr.h
 * @brief OTDR-Style Floppy Disk Signal Analysis Module
 *
 * Applies fiber-optic OTDR (Optical Time-Domain Reflectometer) concepts
 * to floppy disk flux-level analysis. Maps timing jitter, signal quality,
 * and anomaly detection onto a position-dependent quality profile.
 *
 * OTDR Analogy:
 *   LWL Fiber              Floppy Disk
 *   ──────────              ───────────
 *   Light pulse         →   Read head scanning track
 *   Distance (km)       →   Position in track (bitcells)
 *   Attenuation (dB)    →   Timing jitter / signal quality
 *   Splice event        →   Sector boundary (PLL re-lock)
 *   Connector reflect.  →   Copy protection anomaly
 *   Fiber break          →   No-flux area / unreadable zone
 *   Macro bend          →   Gradual degradation
 *   Rayleigh scatter    →   Baseline jitter / noise floor
 *   Dead zone           →   Index gap (no data)
 *
 * @author UFT Project
 * @license GPL-3.0
 */

#ifndef FLOPPY_OTDR_H
#define FLOPPY_OTDR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════ */

/** MFM nominal timing intervals (in nanoseconds at 300 RPM, DD) */
#define OTDR_MFM_2US_NS         4000    /**< Short (2T) = 4µs */
#define OTDR_MFM_3US_NS         6000    /**< Medium (3T) = 6µs */
#define OTDR_MFM_4US_NS         8000    /**< Long (4T) = 8µs */

/** MFM nominal timing intervals for HD (300 RPM) */
#define OTDR_MFM_HD_2T_NS       2000    /**< Short (2T) = 2µs */
#define OTDR_MFM_HD_3T_NS       3000    /**< Medium (3T) = 3µs */
#define OTDR_MFM_HD_4T_NS       4000    /**< Long (4T) = 4µs */

/** FM nominal timing intervals */
#define OTDR_FM_SHORT_NS        4000    /**< Clock or data pulse */
#define OTDR_FM_LONG_NS         8000    /**< Clock-to-clock (no data) */

/** Analysis parameters */
#define OTDR_MAX_TRACKS         168     /**< Max tracks (84 cyl × 2 heads) */
#define OTDR_MAX_SECTORS        24      /**< Max sectors per track */
#define OTDR_MAX_EVENTS         256     /**< Max events per track */
#define OTDR_MAX_REVOLUTIONS    16      /**< Max multi-read revolutions */
#define OTDR_WINDOW_SIZE        64      /**< Sliding window for averaging */
#define OTDR_PLL_INITIAL_FREQ   1e6     /**< PLL initial frequency (Hz) */

/** Quality thresholds (percentage deviation from nominal) */
#define OTDR_QUALITY_EXCELLENT  5.0     /**< < 5% jitter = excellent */
#define OTDR_QUALITY_GOOD       10.0    /**< < 10% jitter = good */
#define OTDR_QUALITY_FAIR       15.0    /**< < 15% jitter = fair */
#define OTDR_QUALITY_POOR       25.0    /**< < 25% jitter = poor */
#define OTDR_QUALITY_CRITICAL   40.0    /**< < 40% jitter = critical */
/* > 40% = unreadable */

/** No-flux detection: gap threshold in multiples of nominal period */
#define OTDR_NOFLUX_THRESHOLD   6.0

/** Weak-bit detection: coefficient of variation threshold for multi-read */
#define OTDR_WEAK_BIT_CV        0.15    /**< 15% coefficient of variation */

/* ═══════════════════════════════════════════════════════════════════════
 * Enumerations
 * ═══════════════════════════════════════════════════════════════════════ */

/** Disk encoding type */
typedef enum {
    OTDR_ENC_MFM_DD,        /**< MFM Double Density (Atari ST, PC DD) */
    OTDR_ENC_MFM_HD,        /**< MFM High Density (Atari Falcon, PC HD) */
    OTDR_ENC_FM_SD,         /**< FM Single Density */
    OTDR_ENC_GCR_C64,       /**< GCR (Commodore 64) */
    OTDR_ENC_GCR_APPLE,     /**< GCR (Apple II) */
    OTDR_ENC_AMIGA_DD,      /**< Amiga MFM DD */
    OTDR_ENC_AUTO           /**< Auto-detect from timing histogram */
} otdr_encoding_t;

/** Signal quality level (maps to OTDR dB ranges) */
typedef enum {
    OTDR_QUAL_EXCELLENT = 0,    /**< Pristine signal, like new media */
    OTDR_QUAL_GOOD,             /**< Minor wear, fully readable */
    OTDR_QUAL_FAIR,             /**< Noticeable jitter, still reliable */
    OTDR_QUAL_POOR,             /**< High jitter, read errors possible */
    OTDR_QUAL_CRITICAL,         /**< Marginal, needs multiple retries */
    OTDR_QUAL_UNREADABLE        /**< Signal lost / no flux */
} otdr_quality_t;

/** Event types (analogous to OTDR events) */
typedef enum {
    /* Structural events (like splices/connectors) */
    OTDR_EVT_SECTOR_HEADER,     /**< Sector ID address mark found */
    OTDR_EVT_SECTOR_DATA,       /**< Sector data address mark found */
    OTDR_EVT_INDEX_MARK,        /**< Index pulse position */
    OTDR_EVT_TRACK_GAP,         /**< Inter-sector gap (gap 3) */

    /* Degradation events (like bends/attenuation) */
    OTDR_EVT_JITTER_SPIKE,      /**< Sudden jitter increase */
    OTDR_EVT_JITTER_DRIFT,      /**< Gradual jitter increase over region */
    OTDR_EVT_PLL_RELOCK,        /**< PLL lost and re-acquired lock */
    OTDR_EVT_TIMING_SHIFT,      /**< Systematic timing offset (speed var) */

    /* Anomaly events (like breaks/reflections) */
    OTDR_EVT_CRC_ERROR,         /**< Sector CRC mismatch */
    OTDR_EVT_NOFLUX_AREA,       /**< No flux transitions detected */
    OTDR_EVT_WEAK_BITS,         /**< Unstable bits (multi-read variance) */
    OTDR_EVT_FUZZY_BITS,        /**< Intentional fuzzy bits (copy prot.) */
    OTDR_EVT_EXTRA_SECTOR,      /**< More sectors than expected */
    OTDR_EVT_MISSING_SECTOR,    /**< Expected sector not found */
    OTDR_EVT_ENCODING_ERROR,    /**< Invalid MFM/FM bit pattern */
    OTDR_EVT_DENSITY_CHANGE,    /**< Encoding density changed mid-track */

    /* Copy protection signatures */
    OTDR_EVT_PROT_LONG_TRACK,   /**< Track longer than standard */
    OTDR_EVT_PROT_SHORT_TRACK,  /**< Track shorter than standard */
    OTDR_EVT_PROT_OVERLAP,      /**< Track data wraps past index */
    OTDR_EVT_PROT_DESYNC,       /**< Deliberate desync pattern */
    OTDR_EVT_PROT_SIGNATURE,    /**< Known protection signature matched */
} otdr_event_type_t;

/** Event severity (maps to OTDR loss magnitude) */
typedef enum {
    OTDR_SEV_INFO,          /**< Informational (structural) */
    OTDR_SEV_MINOR,         /**< Minor anomaly, no data loss */
    OTDR_SEV_WARNING,       /**< May cause read failures */
    OTDR_SEV_ERROR,         /**< Data loss / CRC failure */
    OTDR_SEV_CRITICAL       /**< Complete signal loss */
} otdr_severity_t;

/* ═══════════════════════════════════════════════════════════════════════
 * Core Data Structures
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Single flux timing sample with analysis results.
 * Analogous to a single OTDR measurement point.
 */
typedef struct {
    uint32_t    raw_ns;             /**< Raw flux interval (nanoseconds) */
    uint32_t    nominal_ns;         /**< Expected interval from PLL */
    int32_t     deviation_ns;       /**< raw - nominal (signed) */
    float       deviation_pct;      /**< Deviation as percentage */
    float       jitter_rms;         /**< RMS jitter in sliding window */
    float       quality_db;         /**< Quality in "dB" (0=perfect, neg=worse) */
    uint8_t     decoded_pattern;    /**< MFM pattern: 2=2T, 3=3T, 4=4T */
    uint8_t     bitcells;           /**< Number of bitcells this interval spans */
    otdr_quality_t quality;         /**< Quality classification */
    bool        is_stable;          /**< Multi-read: bit is stable */
} otdr_sample_t;

/**
 * Detected event on the track.
 * Analogous to an OTDR event marker.
 */
typedef struct {
    otdr_event_type_t   type;       /**< Event classification */
    otdr_severity_t     severity;   /**< Severity level */
    uint32_t            position;   /**< Bitcell position in track */
    uint32_t            flux_index; /**< Index into flux array */
    uint32_t            length;     /**< Event length in bitcells */
    float               magnitude;  /**< Event magnitude (jitter %) */
    float               loss_db;    /**< "Loss" in dB analogy */
    char                desc[80];   /**< Human-readable description */

    /* Sector context (if applicable) */
    int                 sector_id;  /**< Sector number (-1 if N/A) */
    uint16_t            crc_expected;
    uint16_t            crc_actual;
} otdr_event_t;

/**
 * PLL state tracker — models the read channel PLL.
 * Tracks lock status, phase error, and frequency drift.
 */
typedef struct {
    double      frequency;          /**< Current PLL frequency (Hz) */
    double      phase_error;        /**< Current phase error (ns) */
    double      phase_integral;     /**< Accumulated phase error */
    double      bandwidth;          /**< PLL loop bandwidth (0.0-1.0) */
    double      damping;            /**< PLL damping factor */
    uint32_t    lock_count;         /**< Consecutive locked samples */
    uint32_t    total_samples;      /**< Total samples processed */
    bool        locked;             /**< PLL currently locked */
    uint32_t    lock_lost_count;    /**< Number of lock-lost events */
    uint32_t    last_lock_pos;      /**< Last position where lock was lost */

    /* Adaptive parameters */
    double      freq_min;           /**< Min observed frequency */
    double      freq_max;           /**< Max observed frequency */
    double      freq_drift_rate;    /**< Frequency drift per revolution */
} otdr_pll_state_t;

/**
 * Track-level analysis results.
 * Analogous to an OTDR trace for one fiber segment.
 */
typedef struct {
    /* Track identification */
    uint8_t     cylinder;
    uint8_t     head;
    uint8_t     track_num;          /**< Linear track number */

    /* Encoding */
    otdr_encoding_t encoding;       /**< Detected or specified encoding */
    uint32_t    data_rate;          /**< Bit rate (bits/sec) */

    /* Raw flux data */
    uint32_t   *flux_ns;            /**< Raw flux timings (nanoseconds) */
    uint32_t    flux_count;         /**< Number of flux transitions */
    uint32_t    revolution_ns;      /**< Total revolution time (ns) */

    /* Multi-read data (for weak bit detection) */
    uint32_t   *flux_multi[OTDR_MAX_REVOLUTIONS]; /**< Multiple reads */
    uint32_t    flux_multi_count[OTDR_MAX_REVOLUTIONS];
    uint8_t     num_revolutions;    /**< Number of reads available */

    /* OTDR-style analysis results */
    otdr_sample_t  *samples;        /**< Analyzed samples (1 per flux) */
    uint32_t        sample_count;

    /* Bitcell-level quality profile (the "OTDR trace") */
    float      *quality_profile;    /**< Quality per bitcell (dB) */
    uint32_t    bitcell_count;      /**< Total bitcells in track */

    /* Smoothed profile (sliding window averaged) */
    float      *quality_smoothed;   /**< Smoothed quality profile */

    /* Event list */
    otdr_event_t events[OTDR_MAX_EVENTS];
    uint32_t     event_count;

    /* Sector map */
    struct {
        uint8_t     id;             /**< Sector ID */
        uint32_t    header_pos;     /**< Bitcell position of header */
        uint32_t    data_pos;       /**< Bitcell position of data */
        uint32_t    data_size;      /**< Data size in bytes */
        uint16_t    header_crc;     /**< Header CRC (actual) */
        uint16_t    data_crc;       /**< Data CRC (actual) */
        bool        header_ok;      /**< Header CRC valid */
        bool        data_ok;        /**< Data CRC valid */
        float       avg_quality;    /**< Average quality over this sector */
        otdr_quality_t quality;     /**< Overall sector quality */
    } sectors[OTDR_MAX_SECTORS];
    uint8_t     sector_count;

    /* Track-level statistics */
    struct {
        float   jitter_mean;        /**< Mean jitter (%) */
        float   jitter_rms;         /**< RMS jitter (%) */
        float   jitter_peak;        /**< Peak jitter (%) */
        float   jitter_p95;         /**< 95th percentile jitter */
        float   quality_mean_db;    /**< Mean quality (dB) */
        float   quality_min_db;     /**< Worst quality point (dB) */
        float   snr_estimate;       /**< Estimated SNR (dB) */
        float   speed_variation;    /**< Speed variation (%) */

        uint32_t total_bitcells;
        uint32_t good_bitcells;
        uint32_t weak_bitcells;
        uint32_t bad_bitcells;
        uint32_t noflux_bitcells;

        uint32_t crc_errors;
        uint32_t missing_sectors;
        uint32_t pll_relocks;

        otdr_quality_t overall;     /**< Overall track quality */
    } stats;

    /* PLL state after processing */
    otdr_pll_state_t pll;

    /* Timing histogram (for encoding detection) */
    struct {
        uint32_t bins[256];         /**< Histogram bins (0-25600ns, 100ns/bin) */
        uint32_t peak_2t;           /**< Peak position for 2T */
        uint32_t peak_3t;           /**< Peak position for 3T */
        uint32_t peak_4t;           /**< Peak position for 4T */
        float    peak_separation;   /**< Ratio between peaks */
    } histogram;

} otdr_track_t;

/**
 * Disk-level analysis — the complete "OTDR report".
 * Analogous to a full fiber characterization report.
 */
typedef struct {
    /* Disk identification */
    char            label[64];
    char            source_file[256];

    /* Configuration */
    otdr_encoding_t encoding;
    uint8_t         num_cylinders;
    uint8_t         num_heads;
    uint8_t         expected_sectors;
    uint32_t        rpm;            /**< Rotation speed */

    /* Track analyses */
    otdr_track_t   *tracks;
    uint16_t        track_count;

    /* Disk-level heatmap data */
    float          *heatmap;        /**< track × bitcell quality matrix */
    uint32_t        heatmap_cols;   /**< Bitcell resolution per track */
    uint16_t        heatmap_rows;   /**< Number of tracks */

    /* Disk-level statistics */
    struct {
        float       quality_mean;
        float       quality_worst_track;
        uint8_t     worst_track_num;
        uint32_t    total_sectors;
        uint32_t    good_sectors;
        uint32_t    bad_sectors;
        uint32_t    total_events;
        uint32_t    critical_events;
        otdr_quality_t overall;

        /* Protection analysis */
        bool        has_copy_protection;
        char        protection_type[64];
        uint32_t    protected_tracks;
    } stats;

} otdr_disk_t;

/**
 * Analysis configuration.
 */
typedef struct {
    otdr_encoding_t encoding;       /**< Encoding (or AUTO) */
    uint32_t        rpm;            /**< RPM (300 or 360) */
    uint32_t        expected_sectors; /**< Expected sectors/track (0=auto) */

    /* PLL parameters */
    double          pll_bandwidth;      /**< PLL bandwidth (default 0.04) */
    double          pll_damping;        /**< PLL damping (default 0.707) */
    double          pll_lock_threshold; /**< Lock threshold in % (default 10) */

    /* Analysis options */
    bool            detect_weak_bits;   /**< Enable multi-read comparison */
    bool            detect_protection;  /**< Enable copy protection detect. */
    bool            generate_heatmap;   /**< Generate disk-wide heatmap */
    uint32_t        heatmap_resolution; /**< Heatmap columns (default 1024) */

    /* Smoothing */
    uint32_t        smooth_window;      /**< Sliding window size */
    bool            use_gaussian;       /**< Gaussian vs. box filter */

    /* Thresholds */
    float           noflux_threshold;   /**< No-flux gap multiplier */
    float           weak_bit_cv;        /**< Weak bit CoV threshold */
    float           jitter_spike_threshold; /**< Jitter spike detection */
} otdr_config_t;

/* ═══════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════ */

/* --- Configuration --- */

/** Initialize config with sane defaults for Atari ST DD */
void otdr_config_defaults(otdr_config_t *cfg);

/** Initialize config for specific platform */
void otdr_config_for_platform(otdr_config_t *cfg, const char *platform);

/* --- PLL --- */

/** Initialize PLL state */
void otdr_pll_init(otdr_pll_state_t *pll, double initial_freq,
                   double bandwidth, double damping);

/** Feed one flux interval to PLL, returns decoded bitcells */
uint8_t otdr_pll_feed(otdr_pll_state_t *pll, uint32_t flux_ns,
                      double *phase_error_out);

/** Reset PLL to initial state */
void otdr_pll_reset(otdr_pll_state_t *pll);

/* --- Track Analysis --- */

/** Allocate and initialize a track analysis structure */
otdr_track_t *otdr_track_create(uint8_t cylinder, uint8_t head);

/** Free track analysis structure */
void otdr_track_free(otdr_track_t *track);

/**
 * Load raw flux data into track.
 * @param flux_ns   Array of flux intervals in nanoseconds
 * @param count     Number of flux intervals
 * @param rev       Revolution index (0 for single read, 0-N for multi-read)
 */
void otdr_track_load_flux(otdr_track_t *track, const uint32_t *flux_ns,
                          uint32_t count, uint8_t rev);

/**
 * Run full OTDR-style analysis on a track.
 * Performs: PLL tracking, jitter calculation, event detection,
 * quality profiling, sector mapping, anomaly classification.
 */
int otdr_track_analyze(otdr_track_t *track, const otdr_config_t *cfg);

/** Build timing histogram for the track */
void otdr_track_histogram(otdr_track_t *track);

/** Detect encoding from histogram peaks */
otdr_encoding_t otdr_track_detect_encoding(const otdr_track_t *track);

/** Compute quality profile (the "OTDR trace") */
void otdr_track_quality_profile(otdr_track_t *track,
                                const otdr_config_t *cfg);

/** Smooth quality profile with sliding window */
void otdr_track_smooth_profile(otdr_track_t *track, uint32_t window_size,
                               bool gaussian);

/** Detect events from quality profile */
void otdr_track_detect_events(otdr_track_t *track, const otdr_config_t *cfg);

/** Analyze weak bits from multi-read data */
void otdr_track_weak_bit_analysis(otdr_track_t *track,
                                  const otdr_config_t *cfg);

/** Compute track statistics summary */
void otdr_track_compute_stats(otdr_track_t *track);

/* --- Disk Analysis --- */

/** Allocate disk analysis structure */
otdr_disk_t *otdr_disk_create(uint8_t cylinders, uint8_t heads);

/** Free disk analysis structure */
void otdr_disk_free(otdr_disk_t *disk);

/** Analyze all tracks in disk */
int otdr_disk_analyze(otdr_disk_t *disk, const otdr_config_t *cfg);

/** Generate 2D quality heatmap (track × position) */
void otdr_disk_generate_heatmap(otdr_disk_t *disk, uint32_t resolution);

/** Compute disk-level statistics */
void otdr_disk_compute_stats(otdr_disk_t *disk);

/** Detect copy protection patterns across disk */
void otdr_disk_detect_protection(otdr_disk_t *disk);

/* --- Utility Functions --- */

/** Convert quality percentage to dB scale */
float otdr_quality_to_db(float deviation_pct);

/** Convert dB to quality level enum */
otdr_quality_t otdr_db_to_quality(float db);

/** Get human-readable quality name */
const char *otdr_quality_name(otdr_quality_t q);

/** Get human-readable event type name */
const char *otdr_event_type_name(otdr_event_type_t type);

/** Get event severity name */
const char *otdr_severity_name(otdr_severity_t sev);

/** Get color for quality level (R,G,B as 0-255) */
void otdr_quality_color(otdr_quality_t q, uint8_t *r, uint8_t *g, uint8_t *b);

/* --- Export --- */

/** Export track quality profile as CSV */
int otdr_track_export_csv(const otdr_track_t *track, const char *filename);

/** Export event list as CSV */
int otdr_track_export_events_csv(const otdr_track_t *track,
                                  const char *filename);

/** Export disk heatmap as PGM image */
int otdr_disk_export_heatmap_pgm(const otdr_disk_t *disk,
                                  const char *filename);

/** Export full disk report as text */
int otdr_disk_export_report(const otdr_disk_t *disk, const char *filename);

/* ═══════════════════════════════════════════════════════════════════════
 * TDFC Integration — Matched Filter & Advanced Signal Analysis
 * ═══════════════════════════════════════════════════════════════════════
 *
 * Merged from Time-Domain Flux Characterization (TDFC) library.
 * Adds: template correlation for sync/protection detection,
 *       CUSUM change-point detection, spectral flatness,
 *       and amplitude envelope profiling.
 */

/** Template pattern for matched filtering */
typedef struct {
    float      *pattern;        /**< Normalized template samples */
    uint32_t    length;         /**< Template length in samples */
    char        name[32];       /**< Template name (e.g. "MFM Sync A1") */
    float       threshold;      /**< Correlation threshold for detection */
} otdr_template_t;

/** Matched filter result */
typedef struct {
    float      *correlation;    /**< Normalized correlation at each flux sample */
    uint32_t    corr_count;     /**< Number of correlation values */
    uint32_t   *match_positions;/**< Positions where corr > threshold */
    uint32_t    match_count;    /**< Number of matches found */
    float       peak_corr;      /**< Peak correlation value */
    uint32_t    peak_position;  /**< Position of peak correlation */
} otdr_match_result_t;

/** CUSUM change-point parameters */
typedef struct {
    float       drift_k;        /**< CUSUM drift parameter (default 0.05) */
    float       threshold_h;    /**< CUSUM detection threshold (default 6.0) */
} otdr_cusum_config_t;

/** CUSUM change-point result */
typedef struct {
    uint32_t   *positions;      /**< Detected change-point positions */
    float      *magnitudes;     /**< Magnitude at each change-point */
    uint32_t    count;          /**< Number of change-points detected */
    uint32_t    capacity;
} otdr_changepoints_t;

/** Amplitude envelope result (TDFC-style RMS profiling) */
typedef struct {
    float      *envelope_rms;   /**< RMS envelope per window */
    float      *snr_db;         /**< Local SNR in dB per window */
    uint32_t    n_points;       /**< Number of profile points */
    uint32_t    step;           /**< Step size between points */
    float       global_mean;
    float       global_std;
    int         health_score;   /**< 0-100 media health heuristic */
} otdr_envelope_t;

/* --- Built-in MFM/FM sync templates --- */

/** Create standard MFM sync mark template (A1 with missing clock) */
otdr_template_t otdr_template_mfm_sync_a1(otdr_encoding_t enc);

/** Create MFM IAM (Index Address Mark) template (C2) */
otdr_template_t otdr_template_mfm_iam_c2(otdr_encoding_t enc);

/** Create FM sync template */
otdr_template_t otdr_template_fm_sync(void);

/** Create custom template from flux intervals */
otdr_template_t otdr_template_from_flux(const uint32_t *flux_ns,
                                         uint32_t count,
                                         const char *name,
                                         float threshold);

/** Free template resources */
void otdr_template_free(otdr_template_t *tmpl);

/* --- Matched Filter --- */

/** Run matched filter on track with given template */
int otdr_track_match_template(const otdr_track_t *track,
                               const otdr_template_t *tmpl,
                               otdr_match_result_t *result);

/** Free match result */
void otdr_match_result_free(otdr_match_result_t *r);

/* --- CUSUM Change-Point Detection --- */

/** Initialize CUSUM config with defaults */
otdr_cusum_config_t otdr_cusum_defaults(void);

/** Run CUSUM on track quality profile to find change-points */
int otdr_track_cusum(const otdr_track_t *track,
                      const otdr_cusum_config_t *cfg,
                      otdr_changepoints_t *result);

/** Run CUSUM on arbitrary float series */
int otdr_cusum_analyze(const float *series, uint32_t n,
                        const otdr_cusum_config_t *cfg,
                        otdr_changepoints_t *result);

/** Free changepoints result */
void otdr_changepoints_free(otdr_changepoints_t *cp);

/* --- Amplitude Envelope Profiling (TDFC-style) --- */

/** Compute RMS envelope and SNR profile from raw flux intervals */
int otdr_track_envelope(const otdr_track_t *track,
                         uint32_t window, uint32_t step,
                         otdr_envelope_t *result);

/** Compute media health score from envelope analysis (0-100) */
int otdr_envelope_health_score(const otdr_envelope_t *env);

/** Free envelope result */
void otdr_envelope_free(otdr_envelope_t *env);

/* --- Spectral Flatness (Wiener entropy) --- */

/** Compute spectral flatness of flux intervals in sliding window.
 *  Returns array of flatness values (0=tonal/periodic, 1=noise-like).
 *  Useful for detecting areas with lost magnetic signal. */
int otdr_track_spectral_flatness(const otdr_track_t *track,
                                  uint32_t window,
                                  float **flatness, uint32_t *count);

#ifdef __cplusplus
}
#endif

#endif /* FLOPPY_OTDR_H */

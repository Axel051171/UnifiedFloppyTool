/**
 * @file uft_sector_overlay.h
 * @brief Sector Boundary Detection via FFT/ACF Analysis
 * 
 * Based on FloppyAI overlay_detection.py
 * Ported from Python to C for UFT
 * 
 * Detects sector boundaries in flux data without format knowledge:
 * - MFM media: Uses FFT + Autocorrelation
 * - GCR media: Uses boundary contrast analysis
 * 
 * Applications:
 * - Unknown format analysis
 * - Copy protection detection
 * - Sector timing visualization
 * - Format reverse engineering
 */

#ifndef UFT_SECTOR_OVERLAY_H
#define UFT_SECTOR_OVERLAY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Default angular bins for detection */
#define UFT_OVERLAY_DEFAULT_BINS    360
#define UFT_OVERLAY_HIGH_RES_BINS   1440

/** Minimum sectors to detect */
#define UFT_OVERLAY_MIN_SECTORS     2

/** Maximum sectors to detect */
#define UFT_OVERLAY_MAX_SECTORS     64

/** Confidence thresholds */
#define UFT_OVERLAY_CONF_HIGH       0.7
#define UFT_OVERLAY_CONF_MEDIUM     0.4
#define UFT_OVERLAY_CONF_LOW        0.2

/** Common sector counts for candidate search */
static const int UFT_OVERLAY_MFM_CANDIDATES[] = {
    9, 10, 11, 15, 18, 21, 22  /* PC/Amiga/Atari sectors */
};

static const int UFT_OVERLAY_GCR_CANDIDATES[] = {
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21  /* C64/Apple/Victor */
};

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Overlay detection method
 */
typedef enum {
    UFT_OVERLAY_AUTO,           /**< Auto-detect based on flux patterns */
    UFT_OVERLAY_MFM,            /**< MFM: FFT + Autocorrelation */
    UFT_OVERLAY_GCR             /**< GCR: Boundary contrast */
} uft_overlay_method_t;

/**
 * @brief Sector boundary
 */
typedef struct {
    double angle_deg;           /**< Angular position in degrees (0-360) */
    uint32_t bin_index;         /**< Bin index in histogram */
    double confidence;          /**< Local confidence (0-1) */
    bool refined;               /**< Refined to local maximum */
} uft_sector_boundary_t;

/**
 * @brief Overlay detection result
 */
typedef struct {
    /* Detection status */
    bool detected;              /**< Sectors successfully detected */
    uft_overlay_method_t method;/**< Method used */
    
    /* Sector information */
    uint8_t sector_count;       /**< Number of sectors detected */
    uft_sector_boundary_t *boundaries; /**< Sector boundaries [sector_count] */
    
    /* Quality metrics */
    double confidence;          /**< Overall confidence (0-1) */
    double fft_peak_strength;   /**< FFT dominant peak strength (MFM) */
    double acf_confirmation;    /**< ACF confirmation score (MFM) */
    double boundary_contrast;   /**< Boundary contrast (GCR) */
    
    /* Angular histogram used */
    uint16_t bins;
    double *histogram;          /**< [bins] - caller frees */
} uft_overlay_result_t;

/**
 * @brief Overlay detection configuration
 */
typedef struct {
    uft_overlay_method_t method;/**< Detection method */
    uint16_t angular_bins;      /**< Angular resolution */
    
    /* MFM parameters */
    bool use_autocorrelation;   /**< Confirm FFT with ACF */
    
    /* GCR parameters */
    const int *candidates;      /**< Candidate sector counts */
    size_t candidate_count;     /**< Number of candidates */
    double window_fraction;     /**< Boundary window fraction */
    
    /* Refinement */
    bool refine_to_maxima;      /**< Refine boundaries to local maxima */
    double refine_window;       /**< Refinement window fraction */
    
    /* Multi-file analysis */
    uint8_t max_files;          /**< Maximum files to analyze */
} uft_overlay_config_t;

/*===========================================================================
 * Initialization
 *===========================================================================*/

/**
 * @brief Initialize config with defaults
 * @param config Configuration to initialize
 * @param method Detection method
 */
void uft_overlay_config_init(uft_overlay_config_t *config,
                             uft_overlay_method_t method);

/**
 * @brief Allocate result structure
 * @param max_sectors Maximum sectors
 * @param bins Angular bins
 * @return Allocated result or NULL
 */
uft_overlay_result_t *uft_overlay_alloc(uint8_t max_sectors, uint16_t bins);

/**
 * @brief Free result structure
 * @param result Result to free
 */
void uft_overlay_free(uft_overlay_result_t *result);

/*===========================================================================
 * MFM Detection (FFT + ACF)
 *===========================================================================*/

/**
 * @brief Detect sector overlay for MFM media
 * @param histogram Angular density histogram
 * @param bins Number of bins
 * @param config Detection configuration
 * @param result Output result
 * @return 0 on success
 * 
 * Algorithm:
 * 1. Compute FFT of angular histogram
 * 2. Find dominant frequency (peak in power spectrum)
 * 3. Confirm with autocorrelation
 * 4. Extract phase for boundary positions
 * 5. Refine to local maxima
 */
int uft_overlay_detect_mfm(const double *histogram, uint16_t bins,
                           const uft_overlay_config_t *config,
                           uft_overlay_result_t *result);

/**
 * @brief Compute power spectrum via FFT
 * @param histogram Input histogram
 * @param bins Number of bins
 * @param power Output power spectrum [bins/2+1]
 * @param phase Output phase spectrum [bins/2+1] (optional)
 * @return 0 on success
 */
int uft_overlay_fft_power(const double *histogram, uint16_t bins,
                          double *power, double *phase);

/**
 * @brief Compute autocorrelation
 * @param histogram Input histogram
 * @param bins Number of bins
 * @param acf Output autocorrelation [bins]
 * @return 0 on success
 */
int uft_overlay_autocorrelation(const double *histogram, uint16_t bins,
                                double *acf);

/**
 * @brief Find dominant frequency in power spectrum
 * @param power Power spectrum
 * @param bins Number of bins in original histogram
 * @param min_freq Minimum frequency (sectors)
 * @param max_freq Maximum frequency (sectors)
 * @return Dominant frequency (sector count) or 0
 */
uint8_t uft_overlay_find_dominant_freq(const double *power, uint16_t bins,
                                       uint8_t min_freq, uint8_t max_freq);

/*===========================================================================
 * GCR Detection (Boundary Contrast)
 *===========================================================================*/

/**
 * @brief Detect sector overlay for GCR media
 * @param histogram Angular density histogram
 * @param bins Number of bins
 * @param candidates Candidate sector counts to try
 * @param candidate_count Number of candidates
 * @param config Detection configuration
 * @param result Output result
 * @return 0 on success
 * 
 * Algorithm:
 * 1. For each candidate sector count:
 *    a. Try different phase alignments
 *    b. Compute boundary vs. within-sector contrast
 * 2. Select best (k, phase) pair
 * 3. Refine boundaries to local maxima
 */
int uft_overlay_detect_gcr(const double *histogram, uint16_t bins,
                           const int *candidates, size_t candidate_count,
                           const uft_overlay_config_t *config,
                           uft_overlay_result_t *result);

/**
 * @brief Compute boundary contrast for sector hypothesis
 * @param histogram Angular histogram
 * @param bins Number of bins
 * @param sector_count Hypothesized sector count
 * @param phase_bins Phase offset in bins
 * @param window_bins Boundary window size in bins
 * @return Contrast score (0-1)
 */
double uft_overlay_boundary_contrast(const double *histogram, uint16_t bins,
                                     uint8_t sector_count, uint32_t phase_bins,
                                     uint16_t window_bins);

/*===========================================================================
 * Multi-Revolution Analysis
 *===========================================================================*/

/**
 * @brief Build angular histogram from multiple revolutions
 * @param revolutions Array of revolution data
 * @param rev_counts Size of each revolution
 * @param rev_count Number of revolutions
 * @param bins Output bin count
 * @param histogram Output histogram [bins] (caller allocates)
 * @return 0 on success
 */
int uft_overlay_build_histogram(const uint32_t **revolutions,
                                const size_t *rev_counts, size_t rev_count,
                                uint16_t bins, double *histogram);

/**
 * @brief Detect overlay from multiple files
 * @param file_paths Array of .raw file paths
 * @param file_count Number of files
 * @param config Detection configuration
 * @param result Output result
 * @return 0 on success
 */
int uft_overlay_detect_files(const char **file_paths, size_t file_count,
                             const uft_overlay_config_t *config,
                             uft_overlay_result_t *result);

/*===========================================================================
 * Refinement Functions
 *===========================================================================*/

/**
 * @brief Refine boundary to local maximum
 * @param histogram Angular histogram
 * @param bins Number of bins
 * @param initial_bin Initial boundary bin
 * @param window_bins Search window size
 * @return Refined bin index
 */
uint32_t uft_overlay_refine_boundary(const double *histogram, uint16_t bins,
                                     uint32_t initial_bin, uint16_t window_bins);

/**
 * @brief Smooth histogram for stable maxima detection
 * @param histogram Input/output histogram
 * @param bins Number of bins
 * @param kernel_size Smoothing kernel size (odd)
 */
void uft_overlay_smooth_histogram(double *histogram, uint16_t bins,
                                  uint8_t kernel_size);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Convert bin index to angle
 * @param bin Bin index
 * @param total_bins Total bins
 * @return Angle in degrees (0-360)
 */
static inline double uft_overlay_bin_to_angle(uint32_t bin, uint16_t total_bins)
{
    return ((double)bin / (double)total_bins) * 360.0;
}

/**
 * @brief Convert angle to bin index
 * @param angle_deg Angle in degrees
 * @param total_bins Total bins
 * @return Bin index
 */
static inline uint32_t uft_overlay_angle_to_bin(double angle_deg,
                                                 uint16_t total_bins)
{
    double normalized = angle_deg / 360.0;
    if (normalized < 0) normalized += 1.0;
    if (normalized >= 1.0) normalized -= 1.0;
    return (uint32_t)(normalized * total_bins);
}

/**
 * @brief Get confidence level description
 * @param confidence Confidence value (0-1)
 * @return Description string
 */
static inline const char *uft_overlay_confidence_desc(double confidence)
{
    if (confidence >= UFT_OVERLAY_CONF_HIGH) return "High";
    if (confidence >= UFT_OVERLAY_CONF_MEDIUM) return "Medium";
    if (confidence >= UFT_OVERLAY_CONF_LOW) return "Low";
    return "Very Low";
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_OVERLAY_H */

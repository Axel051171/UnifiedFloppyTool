/**
 * @file uft_recovery_format.h
 * @brief GOD MODE Format-Based Recovery
 * 
 * Format-basierte Recovery (kontrolliert!):
 * - Format-Scoring (Wahrscheinlichkeit)
 * - Alternative Geometry-Hypothesen
 * - Variable Sektoranzahl testen
 * - Abweichende Gap-Layouts zulassen
 * - Encoding-Fallbacks (FM↔MFM↔GCR)
 * - Mixed-Format-Tracks akzeptieren
 * 
 * WICHTIG: Keine automatische Korrektur!
 * Alles sind Hypothesen mit Scoring.
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_FORMAT_H
#define UFT_RECOVERY_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_MAX_FORMAT_HYPOTHESES   16
#define UFT_MAX_GEOMETRY_VARIANTS   8

/* ============================================================================
 * Known Format Types
 * ============================================================================ */

typedef enum {
    UFT_FORMAT_UNKNOWN = 0,
    
    /* IBM PC */
    UFT_FORMAT_IBM_FM,          /**< IBM FM (SD) */
    UFT_FORMAT_IBM_MFM,         /**< IBM MFM (DD/HD) */
    UFT_FORMAT_IBM_360K,        /**< IBM 360KB */
    UFT_FORMAT_IBM_720K,        /**< IBM 720KB */
    UFT_FORMAT_IBM_1200K,       /**< IBM 1.2MB */
    UFT_FORMAT_IBM_1440K,       /**< IBM 1.44MB */
    UFT_FORMAT_IBM_2880K,       /**< IBM 2.88MB */
    
    /* Commodore */
    UFT_FORMAT_C64_GCR,         /**< C64/1541 GCR */
    UFT_FORMAT_C128_MFM,        /**< C128/1571 MFM side */
    UFT_FORMAT_AMIGA_MFM,       /**< Amiga MFM */
    
    /* Apple */
    UFT_FORMAT_APPLE2_GCR,      /**< Apple II GCR */
    UFT_FORMAT_MAC_GCR,         /**< Macintosh GCR */
    UFT_FORMAT_MAC_MFM,         /**< Macintosh MFM (HD) */
    
    /* Atari */
    UFT_FORMAT_ATARI_FM,        /**< Atari 8-bit FM */
    UFT_FORMAT_ATARI_MFM,       /**< Atari 8-bit MFM */
    UFT_FORMAT_ATARI_ST,        /**< Atari ST */
    
    /* Other */
    UFT_FORMAT_BBC_FM,          /**< BBC Micro FM */
    UFT_FORMAT_BBC_MFM,         /**< BBC Micro MFM */
    UFT_FORMAT_MSX,             /**< MSX */
    UFT_FORMAT_PC98,            /**< NEC PC-98 */
    UFT_FORMAT_X68000,          /**< Sharp X68000 */
    UFT_FORMAT_FM_TOWNS,        /**< FM Towns */
    
    UFT_FORMAT_CUSTOM,          /**< Custom/Unknown format */
} uft_format_type_t;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Disk geometry
 */
typedef struct {
    uint8_t  tracks;            /**< Number of tracks */
    uint8_t  heads;             /**< Number of heads */
    uint8_t  sectors;           /**< Sectors per track */
    uint16_t sector_size;       /**< Bytes per sector */
    uint8_t  interleave;        /**< Sector interleave */
    bool     variable_sectors;  /**< Variable sectors per track */
    uint8_t *sectors_per_track; /**< If variable: sectors per track */
} uft_geometry_t;

/**
 * @brief Gap layout
 */
typedef struct {
    uint16_t gap1;              /**< Post-index gap */
    uint16_t gap2;              /**< Post-ID gap */
    uint16_t gap3;              /**< Post-data gap */
    uint16_t gap4a;             /**< Pre-index gap */
    uint8_t  gap_fill;          /**< Gap fill byte */
    bool     is_standard;       /**< Standard gap layout */
} uft_gap_layout_t;

/**
 * @brief Encoding parameters
 */
typedef struct {
    uint8_t  encoding;          /**< FM/MFM/GCR etc. */
    double   data_rate;         /**< Data rate (kbps) */
    double   bit_cell;          /**< Bit cell width (ns) */
    uint32_t sync_pattern;      /**< Sync pattern */
    uint8_t  sync_length;       /**< Sync length (bits) */
    uint8_t  address_mark;      /**< Address mark */
    uint8_t  data_mark;         /**< Data mark */
    uint8_t  deleted_mark;      /**< Deleted data mark */
} uft_encoding_params_t;

/**
 * @brief Format hypothesis
 */
typedef struct {
    uint32_t hyp_id;            /**< Hypothesis ID */
    
    /* Format identification */
    uft_format_type_t format;   /**< Identified format */
    char     format_name[64];   /**< Format name */
    
    /* Geometry */
    uft_geometry_t geometry;    /**< Geometry hypothesis */
    
    /* Gaps */
    uft_gap_layout_t gaps;      /**< Gap layout */
    
    /* Encoding */
    uft_encoding_params_t encoding; /**< Encoding parameters */
    
    /* Scoring */
    double   score;             /**< Format score (0-100) */
    uint32_t sync_matches;      /**< Sync pattern matches */
    uint32_t sectors_found;     /**< Sectors found */
    uint32_t sectors_valid;     /**< Sectors with valid CRC */
    double   timing_match;      /**< Timing accuracy (%) */
    
    /* Confidence breakdown */
    double   geometry_confidence;
    double   encoding_confidence;
    double   timing_confidence;
    double   overall_confidence;
    
    /* Status */
    bool     is_best;           /**< Currently best hypothesis */
    bool     is_rejected;       /**< Explicitly rejected */
    char    *rejection_reason;  /**< Why rejected */
} uft_format_hypothesis_t;

/**
 * @brief Geometry hypothesis
 */
typedef struct {
    uint8_t  tracks;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    double   score;
    uint32_t evidence_count;    /**< Supporting evidence */
} uft_geometry_hypothesis_t;

/**
 * @brief Mixed format track info
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    
    /* Regions with different formats */
    size_t  *region_starts;     /**< Region start offsets */
    size_t  *region_lengths;    /**< Region lengths */
    uft_format_type_t *region_formats; /**< Format per region */
    size_t   region_count;      /**< Number of regions */
    
    /* Analysis */
    bool     is_mixed;          /**< Track has mixed formats */
    double   confidence;        /**< Detection confidence */
} uft_mixed_format_t;

/**
 * @brief Format recovery context
 */
typedef struct {
    /* Track data */
    const uint8_t **track_data;
    size_t *track_lengths;
    uint8_t track_count;
    uint8_t head_count;
    
    /* Format hypotheses */
    uft_format_hypothesis_t hypotheses[UFT_MAX_FORMAT_HYPOTHESES];
    size_t hypothesis_count;
    uft_format_hypothesis_t *best;
    
    /* Geometry hypotheses */
    uft_geometry_hypothesis_t geo_hypotheses[UFT_MAX_GEOMETRY_VARIANTS];
    size_t geo_hypothesis_count;
    
    /* Mixed format detection */
    uft_mixed_format_t *mixed_tracks;
    size_t mixed_track_count;
    
    /* Options */
    bool try_all_formats;       /**< Try all known formats */
    bool allow_mixed;           /**< Allow mixed-format tracks */
    bool allow_nonstandard_gaps;/**< Allow non-standard gaps */
    double min_confidence;      /**< Minimum confidence threshold */
} uft_format_recovery_ctx_t;

/* ============================================================================
 * Format Scoring
 * ============================================================================ */

/**
 * @brief Calculate format score
 * 
 * Berechnet Wahrscheinlichkeit für ein Format.
 */
double uft_format_calc_score(const uint8_t *track_data, size_t len,
                             uft_format_type_t format,
                             const uft_encoding_params_t *encoding);

/**
 * @brief Score all known formats
 */
void uft_format_score_all(const uint8_t *track_data, size_t len,
                          uft_format_hypothesis_t *hypotheses,
                          size_t *count);

/**
 * @brief Get best scoring format
 */
uft_format_type_t uft_format_get_best(const uft_format_hypothesis_t *hypotheses,
                                      size_t count,
                                      double *confidence);

/**
 * @brief Compare format scores
 */
int uft_format_compare_scores(const uft_format_hypothesis_t *a,
                              const uft_format_hypothesis_t *b);

/* ============================================================================
 * Geometry Hypotheses
 * ============================================================================ */

/**
 * @brief Generate geometry hypotheses
 */
size_t uft_format_generate_geometries(const uint8_t **track_data,
                                      const size_t *track_lengths,
                                      size_t track_count,
                                      uft_geometry_hypothesis_t *hypotheses,
                                      size_t max_hypotheses);

/**
 * @brief Score geometry hypothesis
 */
void uft_format_score_geometry(const uint8_t *track_data, size_t len,
                               uft_geometry_hypothesis_t *geo);

/**
 * @brief Validate geometry against track
 */
bool uft_format_validate_geometry(const uint8_t *track_data, size_t len,
                                  const uft_geometry_t *geo,
                                  double *match_score);

/**
 * @brief Infer geometry from data
 */
bool uft_format_infer_geometry(const uint8_t *track_data, size_t len,
                               uft_geometry_t *geo,
                               double *confidence);

/* ============================================================================
 * Variable Sector Count
 * ============================================================================ */

/**
 * @brief Test variable sector counts
 */
bool uft_format_test_variable_sectors(const uint8_t *track_data, size_t len,
                                      uint8_t min_sectors, uint8_t max_sectors,
                                      uint8_t *best_count,
                                      double *score);

/**
 * @brief Detect C64-style variable sectors
 */
bool uft_format_detect_c64_zones(const uint8_t **track_data,
                                 const size_t *track_lengths,
                                 size_t track_count,
                                 uint8_t *sectors_per_track);

/**
 * @brief Detect Apple II variable sectors
 */
bool uft_format_detect_apple_zones(const uint8_t **track_data,
                                   const size_t *track_lengths,
                                   size_t track_count,
                                   uint8_t *sectors_per_track);

/* ============================================================================
 * Gap Layout Analysis
 * ============================================================================ */

/**
 * @brief Analyze gap layout
 */
void uft_format_analyze_gaps(const uint8_t *track_data, size_t len,
                             uft_gap_layout_t *gaps);

/**
 * @brief Check if gaps are standard
 */
bool uft_format_gaps_are_standard(const uft_gap_layout_t *gaps,
                                  uft_format_type_t format);

/**
 * @brief Allow non-standard gaps
 */
bool uft_format_accept_nonstandard_gaps(const uft_gap_layout_t *gaps,
                                        double tolerance);

/**
 * @brief Get standard gaps for format
 */
void uft_format_get_standard_gaps(uft_format_type_t format,
                                  uft_gap_layout_t *gaps);

/* ============================================================================
 * Encoding Fallbacks
 * ============================================================================ */

/**
 * @brief Try encoding fallbacks
 * 
 * Versucht alternative Encodings wenn primäres fehlschlägt.
 */
bool uft_format_try_encoding_fallback(const uint8_t *track_data, size_t len,
                                      uft_encoding_params_t *current,
                                      uft_encoding_params_t *fallback,
                                      double *improvement);

/**
 * @brief Get encoding fallback chain
 */
size_t uft_format_get_fallback_chain(uft_format_type_t format,
                                     uft_encoding_params_t *fallbacks,
                                     size_t max_fallbacks);

/**
 * @brief FM to MFM fallback
 */
bool uft_format_fm_to_mfm(const uint8_t *fm_data, size_t len,
                          uint8_t **mfm_data, size_t *mfm_len);

/**
 * @brief MFM to FM fallback
 */
bool uft_format_mfm_to_fm(const uint8_t *mfm_data, size_t len,
                          uint8_t **fm_data, size_t *fm_len);

/**
 * @brief GCR fallback (different GCR variants)
 */
bool uft_format_gcr_fallback(const uint8_t *gcr_data, size_t len,
                             uint8_t gcr_type,
                             uint8_t **decoded, size_t *decoded_len);

/* ============================================================================
 * Mixed-Format Tracks
 * ============================================================================ */

/**
 * @brief Detect mixed-format track
 */
bool uft_format_detect_mixed(const uint8_t *track_data, size_t len,
                             uft_mixed_format_t *result);

/**
 * @brief Analyze mixed-format regions
 */
void uft_format_analyze_mixed_regions(const uint8_t *track_data, size_t len,
                                      uft_mixed_format_t *mixed);

/**
 * @brief Decode mixed-format track
 */
bool uft_format_decode_mixed(const uint8_t *track_data, size_t len,
                             const uft_mixed_format_t *mixed,
                             uint8_t **decoded, size_t *decoded_len);

/**
 * @brief Accept mixed-format track (don't try to "fix" it)
 */
void uft_format_accept_mixed(uft_mixed_format_t *mixed);

/* ============================================================================
 * Format Database
 * ============================================================================ */

/**
 * @brief Get format parameters
 */
bool uft_format_get_params(uft_format_type_t format,
                           uft_geometry_t *geometry,
                           uft_encoding_params_t *encoding,
                           uft_gap_layout_t *gaps);

/**
 * @brief Get format name
 */
const char* uft_format_get_name(uft_format_type_t format);

/**
 * @brief Identify format from signature
 */
uft_format_type_t uft_format_identify(const uint8_t *track_data, size_t len);

/**
 * @brief Get all formats for platform
 */
size_t uft_format_get_for_platform(const char *platform,
                                   uft_format_type_t *formats,
                                   size_t max_formats);

/* ============================================================================
 * Full Format Recovery
 * ============================================================================ */

/**
 * @brief Create format recovery context
 */
uft_format_recovery_ctx_t* uft_format_recovery_create(void);

/**
 * @brief Add track data
 */
bool uft_format_recovery_add_track(uft_format_recovery_ctx_t *ctx,
                                   uint8_t track, uint8_t head,
                                   const uint8_t *data, size_t len);

/**
 * @brief Run format analysis
 */
void uft_format_recovery_analyze(uft_format_recovery_ctx_t *ctx);

/**
 * @brief Get best format hypothesis
 */
const uft_format_hypothesis_t* uft_format_recovery_get_best(
    const uft_format_recovery_ctx_t *ctx);

/**
 * @brief Get all hypotheses (sorted by score)
 */
size_t uft_format_recovery_get_all(const uft_format_recovery_ctx_t *ctx,
                                   const uft_format_hypothesis_t **hypotheses);

/**
 * @brief Generate report
 */
char* uft_format_recovery_report(const uft_format_recovery_ctx_t *ctx);

/**
 * @brief Free context
 */
void uft_format_recovery_free(uft_format_recovery_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_FORMAT_H */

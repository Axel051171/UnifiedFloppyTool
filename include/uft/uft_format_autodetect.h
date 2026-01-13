/**
 * @file uft_format_autodetect.h
 * @brief Score-based Format Auto-Detection Engine
 * 
 * P1-008: Format Auto-Detection mit Confidence Scoring
 * 
 * Features:
 * - Multi-Heuristik Scoring System
 * - Magic Bytes Detection
 * - Geometry Validation
 * - Boot Sector Analysis
 * - Confidence Levels (0-100%)
 */

#ifndef UFT_FORMAT_AUTODETECT_H
#define UFT_FORMAT_AUTODETECT_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Score Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_DETECT_SCORE_MAX        100
#define UFT_DETECT_SCORE_HIGH       80
#define UFT_DETECT_SCORE_MEDIUM     60
#define UFT_DETECT_SCORE_LOW        40
#define UFT_DETECT_SCORE_UNCERTAIN  20

#define UFT_DETECT_MAX_CANDIDATES   16
#define UFT_DETECT_MAX_WARNINGS     8
#define UFT_DETECT_WARNING_LEN      128

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Heuristics
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_HEURISTIC_MAGIC_BYTES     = (1 << 0),  /* File header magic */
    UFT_HEURISTIC_EXTENSION       = (1 << 1),  /* File extension */
    UFT_HEURISTIC_FILE_SIZE       = (1 << 2),  /* Expected file size */
    UFT_HEURISTIC_BOOT_SECTOR     = (1 << 3),  /* Boot sector analysis */
    UFT_HEURISTIC_GEOMETRY        = (1 << 4),  /* Track/sector layout */
    UFT_HEURISTIC_ENCODING        = (1 << 5),  /* MFM/FM/GCR patterns */
    UFT_HEURISTIC_FILESYSTEM      = (1 << 6),  /* FAT/OFS/FFS/etc. */
    UFT_HEURISTIC_FLUX_TIMING     = (1 << 7),  /* Flux timing analysis */
    UFT_HEURISTIC_ALL             = 0xFF
} uft_heuristic_flags_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_format_t        format;             /* Detected format */
    int                 score;              /* Confidence 0-100 */
    uint32_t            heuristics_matched; /* Which heuristics matched */
    const char          *format_name;       /* Human-readable name */
    const char          *format_desc;       /* Description */
} uft_detect_candidate_t;

typedef struct {
    char                text[UFT_DETECT_WARNING_LEN];
    int                 severity;           /* 0=info, 1=warn, 2=error */
} uft_detect_warning_t;

typedef struct {
    /* Results */
    uft_detect_candidate_t  candidates[UFT_DETECT_MAX_CANDIDATES];
    int                     candidate_count;
    
    /* Best match */
    uft_format_t            best_format;
    int                     best_score;
    const char              *best_name;
    
    /* Detected properties */
    uft_encoding_t          detected_encoding;
    uft_geometry_t          detected_geometry;
    
    /* Warnings */
    uft_detect_warning_t    warnings[UFT_DETECT_MAX_WARNINGS];
    int                     warning_count;
    
    /* Metadata */
    size_t                  file_size;
    uint32_t                heuristics_used;
    double                  detection_time_ms;
} uft_detect_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Options
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint32_t            heuristics;         /* Which heuristics to use */
    bool                analyze_flux;       /* Analyze flux timing */
    bool                deep_scan;          /* Scan entire file */
    int                 max_candidates;     /* Max results to return */
    const char          *hint_extension;    /* File extension hint */
    uft_format_t        hint_format;        /* Expected format hint */
} uft_detect_options_t;

#define UFT_DETECT_OPTIONS_DEFAULT { \
    .heuristics = UFT_HEURISTIC_ALL, \
    .analyze_flux = false, \
    .deep_scan = false, \
    .max_candidates = 5, \
    .hint_extension = NULL, \
    .hint_format = UFT_FORMAT_UNKNOWN \
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Magic Byte Definitions
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_format_t    format;
    const uint8_t   *magic;
    size_t          magic_len;
    size_t          offset;
    int             score_boost;
    const char      *description;
} uft_magic_entry_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect format from file
 * @param path File path
 * @param options Detection options (NULL for defaults)
 * @param result Output result
 * @return UFT_OK on success
 */
uft_error_t uft_detect_format_file(const char *path,
                                    const uft_detect_options_t *options,
                                    uft_detect_result_t *result);

/**
 * @brief Detect format from memory buffer
 * @param data Buffer containing file data
 * @param size Buffer size
 * @param options Detection options
 * @param result Output result
 * @return UFT_OK on success
 */
uft_error_t uft_detect_format_buffer(const uint8_t *data, size_t size,
                                      const uft_detect_options_t *options,
                                      uft_detect_result_t *result);

/**
 * @brief Detect format from first N bytes (quick check)
 * @param header First bytes of file
 * @param header_size Size of header (recommend 512+)
 * @param file_size Total file size
 * @param extension File extension (optional)
 * @param result Output result
 * @return UFT_OK on success
 */
uft_error_t uft_detect_format_quick(const uint8_t *header, size_t header_size,
                                     size_t file_size, const char *extension,
                                     uft_detect_result_t *result);

/**
 * @brief Initialize result structure
 */
void uft_detect_result_init(uft_detect_result_t *result);

/**
 * @brief Free any allocated resources in result
 */
void uft_detect_result_free(uft_detect_result_t *result);

/**
 * @brief Get confidence level string
 */
const char* uft_detect_confidence_str(int score);

/**
 * @brief Get all supported magic entries
 */
const uft_magic_entry_t* uft_get_magic_entries(size_t *count);

/**
 * @brief Check if format is flux-based
 */
bool uft_format_is_flux(uft_format_t format);

/**
 * @brief Check if format is sector-based
 */
bool uft_format_is_sector(uft_format_t format);

/**
 * @brief Get expected file size for format (0 if variable)
 */
size_t uft_format_expected_size(uft_format_t format);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_AUTODETECT_H */

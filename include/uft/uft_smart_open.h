/**
 * @file uft_smart_open.h
 * @brief UFT Smart Pipeline - Automatic Feature Integration
 * @version 3.8.0
 * 
 * "Bei uns geht kein Bit verloren" - UFT Preservation Philosophy
 * 
 * This module provides automatic integration of all UFT features:
 * - Bayesian format detection with confidence scoring
 * - Automatic v3 parser selection when available
 * - Protection detection on load
 * - God-mode algorithms for damaged/difficult disks
 */

#ifndef UFT_SMART_OPEN_H
#define UFT_SMART_OPEN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Smart open options */
typedef struct {
    bool use_bayesian_detect;       /**< Use Bayesian format detection (default: true) */
    bool prefer_v3_parsers;         /**< Prefer v3 parsers when available (default: true) */
    bool auto_detect_protection;    /**< Detect protection on load (default: true) */
    bool enable_god_mode;           /**< Enable god-mode for difficult disks (default: false) */
    bool enable_multi_rev_fusion;   /**< Enable multi-revolution fusion (default: true) */
    bool enable_crc_correction;     /**< Try CRC error correction (default: true) */
    bool strict_mode;               /**< Strict mode: don't guess, only report (default: false) */
    int  min_confidence;            /**< Minimum detection confidence 0-100 (default: 70) */
    void (*progress_cb)(int percent, const char* msg, void* user);
    void* user_data;
} uft_smart_options_t;

/** Quality assessment levels */
typedef enum {
    UFT_QUALITY_PERFECT   = 100,    /**< No errors detected */
    UFT_QUALITY_EXCELLENT = 90,     /**< Minor issues, fully readable */
    UFT_QUALITY_GOOD      = 75,     /**< Some errors, mostly readable */
    UFT_QUALITY_FAIR      = 50,     /**< Significant errors, partial data */
    UFT_QUALITY_POOR      = 25,     /**< Heavy damage, limited recovery */
    UFT_QUALITY_UNREADABLE = 0      /**< Cannot decode */
} uft_quality_level_t;

/** Detection result */
typedef struct {
    int format_id;                  /**< Detected format ID */
    const char* format_name;        /**< Format name */
    int confidence;                 /**< Detection confidence 0-100 */
    bool using_v3_parser;           /**< True if v3 parser is active */
} uft_detection_result_t;

/** Protection result */
typedef struct {
    bool detected;                  /**< True if protection found */
    char scheme_name[64];           /**< Protection scheme name */
    char platform[32];              /**< Platform (C64, Amiga, etc.) */
    int confidence;                 /**< Detection confidence */
    int indicator_count;            /**< Number of indicators found */
} uft_protection_result_t;

/** Quality analysis result */
typedef struct {
    uft_quality_level_t level;      /**< Overall quality level */
    int readable_sectors;           /**< Number of readable sectors */
    int total_sectors;              /**< Total sectors expected */
    int crc_errors;                 /**< CRC errors found */
    int crc_corrected;              /**< CRC errors corrected */
    int weak_bits_found;            /**< Weak/fuzzy bits detected */
    int weak_bits_resolved;         /**< Weak bits resolved via fusion */
    double bit_error_rate;          /**< Estimated BER */
    bool god_mode_used;             /**< True if god-mode was needed */
} uft_quality_result_t;

/** Complete smart open result */
typedef struct {
    void* handle;                   /**< Opaque disk handle */
    uft_detection_result_t detection;
    uft_protection_result_t protection;
    uft_quality_result_t quality;
    char warnings[1024];            /**< Accumulated warnings */
    char error[256];                /**< Error message if failed */
} uft_smart_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize smart options with defaults
 */
void uft_smart_options_init(uft_smart_options_t* opts);

/**
 * @brief Smart open - full automatic pipeline
 * @param path Path to disk image
 * @param opts Options (NULL for defaults)
 * @param result Output result structure
 * @return 0 on success, error code on failure
 */
int uft_smart_open(const char* path, const uft_smart_options_t* opts,
                   uft_smart_result_t* result);

/**
 * @brief Close smart-opened disk
 */
void uft_smart_close(uft_smart_result_t* result);

/**
 * @brief Re-analyze with different options
 */
int uft_smart_reanalyze(uft_smart_result_t* result, const uft_smart_options_t* opts);

/**
 * @brief Get quality level name
 */
const char* uft_quality_level_name(uft_quality_level_t level);

/**
 * @brief Generate human-readable report
 */
char* uft_smart_report(const uft_smart_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SMART_OPEN_H */

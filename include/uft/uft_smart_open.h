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

/* Declared at file scope: a struct tag first seen inside a prototype has
 * prototype scope and would be a different type (MF-433). */
struct uft_format_handler;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

/** Smart open options */
typedef struct {
    bool use_bayesian_detect;       /**< Use Bayesian format detection (default: true) */
    bool prefer_v3_parsers;         /**< Prefer v3 parsers when available (default: true) */
    bool auto_detect_protection;    /**< Detect protection on load (default: true) */
    bool enable_multi_rev_fusion;   /**< Enable multi-revolution fusion (default: true) */
    bool enable_crc_correction;     /**< Try CRC error correction (default: true) */
    bool strict_mode;               /**< Strict mode: don't guess, only report (default: false) */
    int  min_confidence;            /**< Minimum detection confidence 0-100 (default: 70) */
    void (*progress_cb)(int percent, const char* msg, void* user);
    void* user_data;
} uft_smart_options_t;

/** Quality assessment levels */
typedef enum {
    /* MF-443: a report must be able to say "I did not measure this".
     * Without it the only way to express "unknown" was to pick a level, and
     * the code picked GOOD — see analyze_quality(). Every count in
     * uft_quality_result_t uses UFT_QUALITY_NOT_DETERMINED as its sentinel
     * for the same reason: 0 readable sectors and "not counted" are very
     * different statements about a disk. */
    UFT_QUALITY_NOT_DETERMINED = -1, /**< Not measured — do not report a value */
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

    /* MF-448 (ARCH-13): how alone the winner was.
     *
     * `equally_ranked` counts the plugins that answered with EXACTLY the
     * winner's confidence, the winner included. 1 means identified. Anything
     * above 1 means the winner was picked by registration order, and the
     * confidence above describes a claim several formats make just as
     * strongly — a report that prints only the number would be stating
     * certainty that was not measured. */
    size_t equally_ranked;          /**< 1 = eindeutig, >1 = Gleichstand */
    size_t claimants;               /**< Plugins, die die Datei überhaupt beanspruchen */
    const char* runner_up_name;     /**< Bestes Plugin darunter, oder NULL */
    int runner_up_confidence;
} uft_detection_result_t;

/** Protection result */
#ifndef UFT_PROTECTION_RESULT_T_DEFINED
#define UFT_PROTECTION_RESULT_T_DEFINED
typedef struct {
    bool detected;                  /**< True if protection found */
    char scheme_name[64];           /**< Protection scheme name */
    char platform[32];              /**< Platform (C64, Amiga, etc.) */
    int confidence;                 /**< Detection confidence */
    int indicator_count;            /**< Number of indicators found */
} uft_protection_result_t;
#endif /* UFT_PROTECTION_RESULT_T_DEFINED */

/** Quality analysis result.
 *
 * Counts are -1 (UFT_QUALITY_NOT_DETERMINED) when the corresponding analysis
 * did not run. Consumers must render that as "not determined", never as a
 * number — this is a forensic report, and an unmeasured field claiming a value
 * is the failure mode the first principle exists to prevent (MF-443). */
typedef struct {
    uft_quality_level_t level;      /**< Overall quality, or NOT_DETERMINED */
    int readable_sectors;           /**< Sectors read OK, or -1 if not counted */
    int total_sectors;              /**< Sectors expected, or -1 if not counted */
    int crc_errors;                 /**< CRC failures, or -1 if not counted */
    int crc_corrected;              /**< CRC errors corrected, or -1 */
    int weak_bits_found;            /**< Weak/fuzzy bits, or -1 if not analysed */
    int weak_bits_resolved;         /**< Weak bits resolved via fusion, or -1 */
    double bit_error_rate;          /**< Estimated BER, or -1.0 if not measured */
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
/**
 * @brief Register an optional v3 parser handler for a format.
 *
 * Protection detection uses the v3 parsers. They are NOT linked in by this
 * module (MF-444): a consumer that wants protection detection registers the
 * handler it wants, and one that does not is not forced to link several
 * thousand lines of parser it never calls.
 *
 * @param format_id One of the FMT_* ids reported in uft_detection_result_t
 * @param handler   The v3 handler, or NULL to ignore the call
 */
typedef bool (*uft_smart_protection_fn)(void* handle, char* name, size_t name_size,
                                        float* confidence);

void uft_smart_register_v3_handler(int format_id, struct uft_format_handler* handler,
                                   uft_smart_protection_fn detect_protection);

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

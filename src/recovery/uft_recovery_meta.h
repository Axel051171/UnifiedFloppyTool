/**
 * @file uft_recovery_meta.h
 * @brief GOD MODE Meta/Decision Recovery & Forensic Tracking
 * 
 * Entscheidungs- & Meta-Recovery:
 * - Confidence-Score pro Bit/Sektor/Track
 * - Quellen-Tracking (welcher Rev, welcher Algo)
 * - Reversibilität jeder Entscheidung
 * - Alternativ-Hypothesen speichern
 * - Warnflags für Writer setzen
 * - Forensik-Logs erzeugen
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_META_H
#define UFT_RECOVERY_META_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_META_MAX_HYPOTHESES     16      /**< Max hypotheses per decision */
#define UFT_META_MAX_SOURCES        32      /**< Max sources tracked */
#define UFT_META_LOG_MAX_ENTRIES    10000   /**< Max log entries */

/* ============================================================================
 * Source Tracking
 * ============================================================================ */

/**
 * @brief Data source type
 */
typedef enum {
    UFT_SOURCE_RAW_FLUX = 0,    /**< Raw flux data */
    UFT_SOURCE_REVOLUTION,      /**< Specific revolution */
    UFT_SOURCE_VOTING,          /**< Multi-rev voting result */
    UFT_SOURCE_INTERPOLATION,   /**< Interpolated */
    UFT_SOURCE_HYPOTHESIS,      /**< From hypothesis */
    UFT_SOURCE_INFERENCE,       /**< Inferred from context */
    UFT_SOURCE_USER_OVERRIDE,   /**< User manual override */
    UFT_SOURCE_CROSS_TRACK,     /**< From cross-track comparison */
    UFT_SOURCE_DIRECTORY,       /**< From directory hints */
} uft_source_type_t;

/**
 * @brief Source info
 */
typedef struct {
    uft_source_type_t type;     /**< Source type */
    uint8_t  rev_index;         /**< Revolution index (if applicable) */
    uint16_t algo_id;           /**< Algorithm ID used */
    char     algo_name[32];     /**< Algorithm name */
    double   algo_param[4];     /**< Algorithm parameters */
    uint8_t  confidence;        /**< Confidence from this source */
    time_t   timestamp;         /**< When obtained */
} uft_source_info_t;

/**
 * @brief Source tracking for a data element
 */
typedef struct {
    uft_source_info_t *sources; /**< Array of sources */
    size_t   source_count;      /**< Number of sources */
    uint8_t  primary_source;    /**< Index of primary source */
    uint8_t  overall_confidence;/**< Combined confidence */
} uft_source_tracking_t;

/* ============================================================================
 * Confidence Scoring
 * ============================================================================ */

/**
 * @brief Confidence level
 */
typedef enum {
    UFT_CONFIDENCE_NONE = 0,    /**< No confidence (guess) */
    UFT_CONFIDENCE_LOW = 25,    /**< Low confidence */
    UFT_CONFIDENCE_MEDIUM = 50, /**< Medium confidence */
    UFT_CONFIDENCE_HIGH = 75,   /**< High confidence */
    UFT_CONFIDENCE_CERTAIN = 100,/**< Certain (CRC OK etc.) */
} uft_confidence_level_t;

/**
 * @brief Confidence breakdown
 */
typedef struct {
    uint8_t  raw_confidence;    /**< From raw data quality */
    uint8_t  crc_confidence;    /**< From CRC validation */
    uint8_t  pattern_confidence;/**< From pattern matching */
    uint8_t  cross_confidence;  /**< From cross-validation */
    uint8_t  combined;          /**< Combined confidence */
} uft_confidence_breakdown_t;

/* ============================================================================
 * Hypothesis Management
 * ============================================================================ */

/**
 * @brief Decision hypothesis
 */
typedef struct {
    uint32_t hyp_id;            /**< Unique hypothesis ID */
    char     description[128];  /**< Description */
    
    /* The hypothesis data */
    uint8_t *data;              /**< Resulting data */
    size_t   data_len;          /**< Data length */
    
    /* Scoring */
    double   score;             /**< Hypothesis score */
    uint8_t  confidence;        /**< Confidence */
    
    /* Validation */
    bool     crc_valid;         /**< CRC validates */
    uint32_t sync_matches;      /**< Sync pattern matches */
    
    /* Status */
    bool     is_selected;       /**< Currently selected */
    bool     is_rejected;       /**< Explicitly rejected */
    char    *rejection_reason;  /**< Why rejected */
} uft_hypothesis_t;

/**
 * @brief Hypothesis set for a decision point
 */
typedef struct {
    uint32_t decision_id;       /**< Decision point ID */
    char     context[64];       /**< Context (e.g., "Track 5 Sector 3") */
    
    uft_hypothesis_t *hypotheses;
    size_t   hypothesis_count;
    
    uft_hypothesis_t *selected; /**< Currently selected hypothesis */
    
    bool     is_finalized;      /**< Decision finalized */
    bool     is_reversible;     /**< Can be reversed */
} uft_hypothesis_set_t;

/* ============================================================================
 * Reversibility
 * ============================================================================ */

/**
 * @brief Undo record
 */
typedef struct {
    uint32_t action_id;         /**< Action ID */
    char     description[128];  /**< What was done */
    time_t   timestamp;         /**< When */
    
    /* What to restore */
    uint8_t *original_data;     /**< Original data */
    size_t   original_len;      /**< Original length */
    uint8_t *modified_data;     /**< Modified data */
    size_t   modified_len;      /**< Modified length */
    
    /* Location */
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    size_t   offset;
    
    /* Status */
    bool     can_undo;          /**< Can be undone */
    bool     was_undone;        /**< Was undone */
} uft_undo_record_t;

/**
 * @brief Undo stack
 */
typedef struct {
    uft_undo_record_t *records; /**< Undo records */
    size_t   record_count;      /**< Number of records */
    size_t   max_records;       /**< Maximum records */
    size_t   current_position;  /**< Current position */
} uft_undo_stack_t;

/* ============================================================================
 * Warning Flags for Writer
 * ============================================================================ */

/**
 * @brief Writer warning
 */
typedef struct {
    uint32_t flag;              /**< Warning flag */
    char     message[256];      /**< Warning message */
    uint8_t  track;             /**< Affected track */
    uint8_t  head;              /**< Affected head */
    uint8_t  sector;            /**< Affected sector (-1 for track-level) */
    uint8_t  severity;          /**< Severity 0-100 */
} uft_writer_warning_t;

/* Writer warning flags */
#define UFT_WARN_CRC_INTENTIONAL    0x0001  /**< Intentional CRC error */
#define UFT_WARN_WEAK_BITS          0x0002  /**< Contains weak bits */
#define UFT_WARN_DUPLICATE_ID       0x0004  /**< Duplicate sector ID */
#define UFT_WARN_NON_STANDARD_SYNC  0x0008  /**< Non-standard sync */
#define UFT_WARN_LONG_TRACK         0x0010  /**< Long track */
#define UFT_WARN_SHORT_TRACK        0x0020  /**< Short track */
#define UFT_WARN_TIMING_CRITICAL    0x0040  /**< Timing-critical data */
#define UFT_WARN_PROTECTION         0x0080  /**< Copy protection present */
#define UFT_WARN_DATA_UNCERTAIN     0x0100  /**< Data uncertain */
#define UFT_WARN_RECONSTRUCTED      0x0200  /**< Data reconstructed */

/* ============================================================================
 * Forensic Logging
 * ============================================================================ */

/**
 * @brief Log entry type
 */
typedef enum {
    UFT_LOG_INFO = 0,
    UFT_LOG_WARNING,
    UFT_LOG_ERROR,
    UFT_LOG_DECISION,
    UFT_LOG_HYPOTHESIS,
    UFT_LOG_RECOVERY,
    UFT_LOG_PROTECTION,
} uft_log_type_t;

/**
 * @brief Forensic log entry
 */
typedef struct {
    uint32_t entry_id;          /**< Entry ID */
    uft_log_type_t type;        /**< Entry type */
    time_t   timestamp;         /**< Timestamp */
    
    /* Location */
    int8_t   track;             /**< Track (-1 for global) */
    int8_t   head;              /**< Head */
    int8_t   sector;            /**< Sector */
    
    /* Content */
    char     message[512];      /**< Log message */
    
    /* Additional data */
    uint8_t *data;              /**< Associated data */
    size_t   data_len;          /**< Data length */
    
    /* Source */
    uft_source_info_t source;   /**< Where this came from */
} uft_log_entry_t;

/**
 * @brief Forensic log
 */
typedef struct {
    uft_log_entry_t *entries;   /**< Log entries */
    size_t   entry_count;       /**< Number of entries */
    size_t   max_entries;       /**< Maximum entries */
    
    /* Filtering */
    uft_log_type_t min_level;   /**< Minimum log level */
    bool     log_decisions;     /**< Log decisions */
    bool     log_hypotheses;    /**< Log hypotheses */
    
    /* Output */
    FILE    *log_file;          /**< Log file (optional) */
} uft_forensic_log_t;

/* ============================================================================
 * Meta Recovery Context
 * ============================================================================ */

/**
 * @brief Meta recovery context
 */
typedef struct {
    /* Source tracking */
    uft_source_tracking_t **source_map; /**< Source tracking per element */
    size_t   source_map_size;
    
    /* Hypotheses */
    uft_hypothesis_set_t *hypothesis_sets;
    size_t   hypothesis_set_count;
    
    /* Undo */
    uft_undo_stack_t undo_stack;
    
    /* Writer warnings */
    uft_writer_warning_t *warnings;
    size_t   warning_count;
    
    /* Forensic log */
    uft_forensic_log_t log;
    
    /* Options */
    bool     track_sources;     /**< Track data sources */
    bool     keep_hypotheses;   /**< Keep all hypotheses */
    bool     enable_undo;       /**< Enable undo */
    uint8_t  max_undo_depth;    /**< Maximum undo depth */
} uft_meta_ctx_t;

/* ============================================================================
 * Source Tracking Functions
 * ============================================================================ */

/**
 * @brief Create source tracking
 */
uft_source_tracking_t* uft_meta_source_create(void);

/**
 * @brief Add source to tracking
 */
bool uft_meta_source_add(uft_source_tracking_t *tracking,
                         const uft_source_info_t *source);

/**
 * @brief Get primary source
 */
const uft_source_info_t* uft_meta_source_get_primary(
    const uft_source_tracking_t *tracking);

/**
 * @brief Set primary source
 */
void uft_meta_source_set_primary(uft_source_tracking_t *tracking,
                                 size_t source_index);

/**
 * @brief Free source tracking
 */
void uft_meta_source_free(uft_source_tracking_t *tracking);

/* ============================================================================
 * Confidence Functions
 * ============================================================================ */

/**
 * @brief Calculate combined confidence
 */
uint8_t uft_meta_calc_confidence(const uft_confidence_breakdown_t *breakdown);

/**
 * @brief Get confidence level
 */
uft_confidence_level_t uft_meta_get_level(uint8_t confidence);

/**
 * @brief Describe confidence level
 */
const char* uft_meta_describe_confidence(uft_confidence_level_t level);

/* ============================================================================
 * Hypothesis Functions
 * ============================================================================ */

/**
 * @brief Create hypothesis set
 */
uft_hypothesis_set_t* uft_meta_hyp_create(const char *context);

/**
 * @brief Add hypothesis to set
 */
bool uft_meta_hyp_add(uft_hypothesis_set_t *set,
                      const uft_hypothesis_t *hyp);

/**
 * @brief Select hypothesis
 */
bool uft_meta_hyp_select(uft_hypothesis_set_t *set, uint32_t hyp_id);

/**
 * @brief Reject hypothesis
 */
void uft_meta_hyp_reject(uft_hypothesis_set_t *set, uint32_t hyp_id,
                         const char *reason);

/**
 * @brief Get selected hypothesis
 */
const uft_hypothesis_t* uft_meta_hyp_get_selected(
    const uft_hypothesis_set_t *set);

/**
 * @brief Get all non-rejected hypotheses
 */
size_t uft_meta_hyp_get_valid(const uft_hypothesis_set_t *set,
                              const uft_hypothesis_t ***hypotheses);

/**
 * @brief Free hypothesis set
 */
void uft_meta_hyp_free(uft_hypothesis_set_t *set);

/* ============================================================================
 * Undo Functions
 * ============================================================================ */

/**
 * @brief Create undo stack
 */
uft_undo_stack_t* uft_meta_undo_create(size_t max_records);

/**
 * @brief Record action for undo
 */
bool uft_meta_undo_record(uft_undo_stack_t *stack,
                          const char *description,
                          uint8_t track, uint8_t head, uint8_t sector,
                          const uint8_t *original, size_t orig_len,
                          const uint8_t *modified, size_t mod_len);

/**
 * @brief Undo last action
 */
bool uft_meta_undo(uft_undo_stack_t *stack,
                   uint8_t **restored, size_t *restored_len);

/**
 * @brief Redo undone action
 */
bool uft_meta_redo(uft_undo_stack_t *stack,
                   uint8_t **result, size_t *result_len);

/**
 * @brief Check if can undo
 */
bool uft_meta_can_undo(const uft_undo_stack_t *stack);

/**
 * @brief Check if can redo
 */
bool uft_meta_can_redo(const uft_undo_stack_t *stack);

/**
 * @brief Free undo stack
 */
void uft_meta_undo_free(uft_undo_stack_t *stack);

/* ============================================================================
 * Writer Warning Functions
 * ============================================================================ */

/**
 * @brief Add writer warning
 */
void uft_meta_warn_add(uft_meta_ctx_t *ctx,
                       uint32_t flag, uint8_t severity,
                       uint8_t track, uint8_t head, uint8_t sector,
                       const char *message);

/**
 * @brief Get warnings for track/sector
 */
size_t uft_meta_warn_get(const uft_meta_ctx_t *ctx,
                         uint8_t track, uint8_t head, uint8_t sector,
                         uft_writer_warning_t **warnings);

/**
 * @brief Check if has specific warning
 */
bool uft_meta_warn_has(const uft_meta_ctx_t *ctx,
                       uint8_t track, uint8_t head, uint8_t sector,
                       uint32_t flag);

/**
 * @brief Generate writer warning report
 */
char* uft_meta_warn_report(const uft_meta_ctx_t *ctx);

/* ============================================================================
 * Forensic Log Functions
 * ============================================================================ */

/**
 * @brief Create forensic log
 */
uft_forensic_log_t* uft_meta_log_create(size_t max_entries);

/**
 * @brief Add log entry
 */
void uft_meta_log_add(uft_forensic_log_t *log,
                      uft_log_type_t type,
                      int8_t track, int8_t head, int8_t sector,
                      const char *message);

/**
 * @brief Add log entry with data
 */
void uft_meta_log_add_data(uft_forensic_log_t *log,
                           uft_log_type_t type,
                           int8_t track, int8_t head, int8_t sector,
                           const char *message,
                           const uint8_t *data, size_t len);

/**
 * @brief Set log file
 */
void uft_meta_log_set_file(uft_forensic_log_t *log, FILE *file);

/**
 * @brief Export log to text
 */
char* uft_meta_log_export(const uft_forensic_log_t *log);

/**
 * @brief Free forensic log
 */
void uft_meta_log_free(uft_forensic_log_t *log);

/* ============================================================================
 * Full Meta Context
 * ============================================================================ */

/**
 * @brief Create meta context
 */
uft_meta_ctx_t* uft_meta_create(void);

/**
 * @brief Enable/disable features
 */
void uft_meta_configure(uft_meta_ctx_t *ctx,
                        bool track_sources,
                        bool keep_hypotheses,
                        bool enable_undo,
                        uint8_t max_undo);

/**
 * @brief Generate full forensic report
 */
char* uft_meta_full_report(const uft_meta_ctx_t *ctx);

/**
 * @brief Free meta context
 */
void uft_meta_free(uft_meta_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_META_H */

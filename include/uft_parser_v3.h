/**
 * @file uft_parser_v3.h
 * @brief GOD MODE Parser Interface v3 - Vollständige Spezifikation
 * 
 * Dieser Header definiert den VOLLSTÄNDIGEN Parser-Standard für UFT.
 * Jeder Parser MUSS dieses Interface implementieren.
 * 
 * Features:
 * - Multi-Rev Read mit Bit-Level Voting
 * - Kopierschutz-Erkennung und -Preservation
 * - Vollständige Read/Write Pipeline
 * - Track-Level Diagnose mit Erklärungen
 * - Scoring-System pro Sektor
 * - Adaptive PLL mit konfigurierbaren Parametern
 * - Verify-After-Write
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#ifndef UFT_PARSER_V3_H
#define UFT_PARSER_V3_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * FORWARD DECLARATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct uft_parser_v3 uft_parser_v3_t;
typedef struct uft_disk_v3 uft_disk_v3_t;
typedef struct uft_track_v3 uft_track_v3_t;
typedef struct uft_sector_v3 uft_sector_v3_t;
typedef struct uft_revolution_v3 uft_revolution_v3_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_V3_MAX_TRACKS           168
#define UFT_V3_MAX_SECTORS          64
#define UFT_V3_MAX_REVOLUTIONS      32
#define UFT_V3_MAX_DIAGNOSIS_LEN    1024
#define UFT_V3_MAX_FORMAT_NAME      64

/* ═══════════════════════════════════════════════════════════════════════════
 * 1) RETRY / READ STRATEGY PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Revolution control */
    uint8_t revolutions;              /* Number of revolutions to read (1-32) */
    uint8_t min_revolutions;          /* Minimum revs before accepting */
    uint8_t max_revolutions;          /* Maximum revs to try */
    
    /* Retry control */
    uint8_t sector_retries;           /* Retries per sector on failure */
    uint8_t track_retries;            /* Retries per track on failure */
    bool retry_on_crc;                /* Retry if CRC fails */
    bool retry_on_missing_id;         /* Retry if sector ID not found */
    bool retry_on_no_sync;            /* Retry if sync not found */
    
    /* Adaptive mode */
    bool adaptive_mode;               /* Auto-increase revs on error */
    uint8_t adaptive_step;            /* Revs to add on failure */
    uint8_t adaptive_max;             /* Max revs in adaptive mode */
    
    /* Revolution selection */
    enum {
        UFT_REV_FIRST,                /* Use first revolution only */
        UFT_REV_BEST,                 /* Use best quality revolution */
        UFT_REV_VOTE,                 /* Bit-level voting across revs */
        UFT_REV_MERGE,                /* Merge best sectors from all revs */
        UFT_REV_ALL                   /* Keep all revolutions */
    } rev_selection;
    
    /* Merge strategy */
    enum {
        UFT_MERGE_MAJORITY,           /* Majority voting per bit */
        UFT_MERGE_BEST_CRC,           /* Take sector with valid CRC */
        UFT_MERGE_HIGHEST_SCORE,      /* Take highest scored sector */
        UFT_MERGE_WEIGHTED            /* Weighted by confidence */
    } merge_strategy;
    
} uft_retry_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 2) SPEED / TIMING CONTROL PARAMETERS
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* RPM control */
    uint16_t rpm_target;              /* Target RPM (300, 360, etc.) */
    uint8_t rpm_tolerance_percent;    /* Acceptable deviation % */
    bool rpm_auto_detect;             /* Auto-detect from index pulses */
    
    /* Data rate */
    uint32_t data_rate;               /* Data rate in bps (250000, 300000, 500000) */
    bool data_rate_auto;              /* Auto-detect data rate */
    
    /* PLL parameters */
    enum {
        UFT_PLL_AGGRESSIVE,           /* Fast lock, less stable */
        UFT_PLL_SMOOTH,               /* Slow lock, more stable */
        UFT_PLL_ADAPTIVE,             /* Adjusts based on quality */
        UFT_PLL_KALMAN                /* Kalman filter based */
    } pll_mode;
    
    float pll_bandwidth;              /* PLL bandwidth (0.01 - 1.0) */
    float pll_gain;                   /* PLL gain factor */
    uint8_t pll_lock_threshold;       /* Bits needed to consider locked */
    
    /* Bitcell timing */
    uint32_t bitcell_time_ns;         /* Nominal bitcell time in nanoseconds */
    uint8_t bitcell_tolerance_percent;/* Acceptable deviation % */
    
    /* Clock recovery */
    bool clock_recovery_enabled;      /* Enable clock recovery */
    uint16_t clock_window_bits;       /* Window size for averaging */
    
} uft_timing_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 3) ERROR HANDLING / THRESHOLDS
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* CRC handling */
    bool accept_bad_crc;              /* Store sectors even with CRC errors */
    bool attempt_crc_correction;      /* Try to fix 1-2 bit errors */
    uint8_t max_correction_bits;      /* Max bits to try correcting */
    
    /* Sector limits */
    uint8_t max_bad_sectors_track;    /* Max bad sectors per track */
    uint16_t max_bad_sectors_total;   /* Max bad sectors total */
    bool abort_on_limit;              /* Abort if limit reached */
    
    /* Recovery modes */
    enum {
        UFT_ERR_STRICT,               /* Fail on any error */
        UFT_ERR_NORMAL,               /* Standard error handling */
        UFT_ERR_SALVAGE,              /* Try everything to recover */
        UFT_ERR_FORENSIC              /* Preserve errors for analysis */
    } error_mode;
    
    /* Fill pattern for unrecoverable */
    uint8_t fill_pattern;             /* Fill byte for unrecoverable data */
    bool mark_filled;                 /* Flag filled sectors */
    
    /* Logging */
    bool log_all_errors;              /* Log every error */
    bool log_to_file;                 /* Write error log to file */
    char error_log_path[256];         /* Path for error log */
    
} uft_error_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 4) JITTER / QUALITY METRICS
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Flux statistics */
    bool dump_flux_stats;             /* Generate flux statistics */
    bool histogram_enabled;           /* Generate timing histogram */
    uint16_t histogram_bins;          /* Number of histogram bins */
    
    /* Jitter detection */
    uint16_t jitter_threshold_ns;     /* Jitter threshold in ns */
    bool flag_high_jitter;            /* Mark high-jitter sectors */
    
    /* Weak bit detection */
    bool weakbit_detect;              /* Detect weak/fuzzy bits */
    uint8_t weakbit_threshold;        /* Revs with different value */
    bool preserve_weakbits;           /* Store weak bit locations */
    
    /* Confidence reporting */
    bool confidence_report;           /* Generate confidence per sector */
    float min_confidence;             /* Minimum acceptable confidence */
    
    /* Quality thresholds */
    float quality_good;               /* Threshold for "good" (0-1) */
    float quality_marginal;           /* Threshold for "marginal" (0-1) */
    
} uft_quality_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 5) RAW vs COOKED MODE
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Output mode */
    enum {
        UFT_MODE_COOKED,              /* Decoded sectors only */
        UFT_MODE_RAW_BITS,            /* Raw bitstream */
        UFT_MODE_RAW_FLUX,            /* Raw flux transitions */
        UFT_MODE_HYBRID               /* Both cooked and raw */
    } output_mode;
    
    /* Raw options */
    bool preserve_sync;               /* Keep sync patterns */
    bool preserve_gaps;               /* Keep inter-sector gaps */
    bool preserve_weak;               /* Keep weak bit masks */
    bool preserve_timing;             /* Keep timing information */
    
    /* Flux options */
    uint32_t flux_resolution_ns;      /* Flux timing resolution */
    bool flux_compression;            /* Compress flux data */
    
} uft_mode_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 6) OFFSET / ALIGNMENT / SYNC
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Index alignment */
    bool index_align;                 /* Align to index pulse */
    bool ignore_index;                /* Ignore index pulse completely */
    int32_t index_offset_ns;          /* Offset from index in ns */
    
    /* Sync detection */
    uint16_t sync_window_bits;        /* Window to search for sync */
    uint8_t sync_min_bits;            /* Minimum sync bits required */
    bool sync_tolerant;               /* Accept partial/weak sync */
    uint8_t sync_patterns[16];        /* Custom sync patterns */
    uint8_t sync_pattern_count;       /* Number of patterns */
    
    /* Track length */
    uint32_t track_length_hint;       /* Expected track length in bits */
    bool auto_detect_length;          /* Auto-detect track length */
    
    /* Write splice */
    enum {
        UFT_SPLICE_AUTO,              /* Auto-detect best position */
        UFT_SPLICE_INDEX,             /* At index pulse */
        UFT_SPLICE_GAP,               /* In largest gap */
        UFT_SPLICE_FIXED              /* Fixed position */
    } splice_mode;
    int32_t splice_offset;            /* Offset for fixed splice */
    
} uft_alignment_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * 7) VERIFY AFTER WRITE
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    bool verify_enabled;              /* Enable verify after write */
    
    enum {
        UFT_VERIFY_SECTOR,            /* Verify at sector level */
        UFT_VERIFY_BITSTREAM,         /* Verify at bitstream level */
        UFT_VERIFY_FLUX               /* Verify at flux level */
    } verify_mode;
    
    /* Tolerances */
    uint8_t verify_retries;           /* Retries for verify */
    float timing_tolerance_percent;   /* Acceptable timing difference */
    bool allow_weak_mismatch;         /* Allow weak bits to differ */
    
    /* Actions */
    bool rewrite_on_fail;             /* Retry write if verify fails */
    uint8_t max_rewrites;             /* Maximum rewrite attempts */
    
} uft_verify_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * COMPLETE PARAMETER SET
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_retry_params_t retry;
    uft_timing_params_t timing;
    uft_error_params_t error;
    uft_quality_params_t quality;
    uft_mode_params_t mode;
    uft_alignment_params_t alignment;
    uft_verify_params_t verify;
    
    /* Format-specific extension */
    void* format_specific;
    size_t format_specific_size;
    
} uft_params_v3_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SCORING SYSTEM
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    float overall;                    /* Overall confidence (0-1) */
    
    /* Component scores */
    float crc_score;                  /* CRC validity score */
    float id_score;                   /* Sector ID plausibility */
    float timing_score;               /* Timing consistency */
    float sequence_score;             /* Sector sequence validity */
    float sync_score;                 /* Sync quality */
    float jitter_score;               /* Jitter level (lower = better) */
    
    /* Flags */
    bool crc_valid;
    bool id_valid;
    bool timing_ok;
    bool has_weak_bits;
    bool has_errors;
    bool recovered;                   /* Was recovered from errors */
    
    /* Details */
    uint8_t revolutions_used;         /* Revs used to decode */
    uint8_t best_revolution;          /* Which rev was best */
    uint16_t bit_errors_corrected;    /* Bits corrected */
    
} uft_score_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS SYSTEM
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_DIAG_OK = 0,
    
    /* Sync issues */
    UFT_DIAG_NO_SYNC,
    UFT_DIAG_WEAK_SYNC,
    UFT_DIAG_PARTIAL_SYNC,
    
    /* ID field issues */
    UFT_DIAG_MISSING_ID,
    UFT_DIAG_ID_CRC_ERROR,
    UFT_DIAG_BAD_TRACK_ID,
    UFT_DIAG_BAD_SECTOR_ID,
    UFT_DIAG_DUPLICATE_ID,
    
    /* Data issues */
    UFT_DIAG_MISSING_DAM,
    UFT_DIAG_DATA_CRC_ERROR,
    UFT_DIAG_DATA_SHORT,
    UFT_DIAG_DATA_LONG,
    
    /* Timing issues */
    UFT_DIAG_TIMING_DRIFT,
    UFT_DIAG_HIGH_JITTER,
    UFT_DIAG_SPEED_ERROR,
    UFT_DIAG_BITCELL_VARIANCE,
    
    /* Structure issues */
    UFT_DIAG_WRONG_SECTOR_COUNT,
    UFT_DIAG_MISSING_SECTOR,
    UFT_DIAG_EXTRA_SECTOR,
    UFT_DIAG_BAD_INTERLEAVE,
    UFT_DIAG_TRUNCATED_TRACK,
    
    /* Copy protection */
    UFT_DIAG_WEAK_BITS,
    UFT_DIAG_NON_STANDARD_TIMING,
    UFT_DIAG_FUZZY_BITS,
    UFT_DIAG_LONG_TRACK,
    UFT_DIAG_EXTRA_DATA,
    
    /* Hardware */
    UFT_DIAG_INDEX_MISSING,
    UFT_DIAG_WRITE_SPLICE_BAD,
    
} uft_diagnosis_code_t;

typedef struct {
    uft_diagnosis_code_t code;
    uint8_t track;
    uint8_t side;
    uint8_t sector;                   /* 0xFF if track-level */
    uint32_t bit_position;            /* Position in track */
    char message[256];                /* Human-readable explanation */
    char suggestion[256];             /* What to do about it */
    uft_score_t score;                /* Associated scores */
} uft_diagnosis_t;

typedef struct {
    uft_diagnosis_t* items;
    size_t count;
    size_t capacity;
    
    /* Summary */
    uint16_t error_count;
    uint16_t warning_count;
    uint16_t info_count;
    float overall_quality;
    
} uft_diagnosis_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SECTOR STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_sector_v3 {
    /* Identity */
    uint8_t logical_track;            /* Track from ID field */
    uint8_t logical_side;             /* Side from ID field */
    uint8_t logical_sector;           /* Sector from ID field */
    uint8_t size_code;                /* Size code from ID */
    
    /* Data */
    uint8_t* data;                    /* Sector data */
    size_t data_size;                 /* Actual data size */
    
    /* CRC */
    uint16_t id_crc;                  /* ID field CRC */
    uint16_t data_crc;                /* Data field CRC */
    uint16_t calculated_id_crc;       /* Calculated ID CRC */
    uint16_t calculated_data_crc;     /* Calculated data CRC */
    
    /* Position */
    uint32_t bit_offset;              /* Position in track */
    uint32_t byte_offset;             /* Byte position */
    
    /* Status */
    bool id_crc_valid;
    bool data_crc_valid;
    bool deleted;                     /* Deleted data mark */
    bool has_data;                    /* Data field present */
    
    /* Scoring & Diagnosis */
    uft_score_t score;
    uft_diagnosis_t diagnosis;
    
    /* Multi-rev data */
    uint8_t** rev_data;               /* Data from each revolution */
    uint8_t* rev_crc_valid;           /* CRC status per revolution */
    uint8_t rev_count;                /* Number of revolutions */
    uint8_t best_rev;                 /* Best revolution index */
    
    /* Weak bits */
    uint8_t* weak_mask;               /* Mask of weak bits */
    uint16_t weak_bit_count;          /* Number of weak bits */
    
    /* Raw data (if preserved) */
    uint8_t* raw_id;                  /* Raw ID field */
    size_t raw_id_size;
    uint8_t* raw_data_block;          /* Raw data block */
    size_t raw_data_block_size;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_track_v3 {
    /* Identity */
    uint8_t physical_track;           /* Physical track number */
    uint8_t physical_side;            /* Physical side */
    uint8_t track_index;              /* Linear index */
    
    /* Geometry */
    uint8_t expected_sectors;         /* Expected sector count */
    uint8_t found_sectors;            /* Actually found */
    uint8_t valid_sectors;            /* With valid CRC */
    uint8_t error_sectors;            /* With errors */
    
    /* Sectors */
    uft_sector_v3_t* sectors;
    size_t sector_count;
    size_t sector_capacity;
    
    /* Raw track data */
    uint8_t* raw_bits;                /* Raw bitstream */
    size_t raw_bit_count;
    uint8_t* raw_flux;                /* Raw flux (if available) */
    size_t raw_flux_count;
    
    /* Timing */
    uint32_t rotation_time_ns;        /* Track rotation time */
    uint16_t* bit_timing;             /* Per-bit timing */
    size_t bit_timing_count;
    
    /* Revolutions */
    uft_revolution_v3_t* revolutions;
    uint8_t revolution_count;
    uint8_t best_revolution;
    
    /* Scoring & Diagnosis */
    uft_score_t score;
    uft_diagnosis_list_t diagnosis;
    
    /* Protection info */
    bool has_weak_bits;
    bool has_non_standard_timing;
    bool has_extra_data;
    bool is_protected;
    
    /* For writing back */
    bool modified;
    bool needs_rewrite;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * REVOLUTION STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_revolution_v3 {
    uint8_t index;
    
    /* Flux data */
    uint32_t* flux_transitions;       /* Flux timing data */
    size_t flux_count;
    uint32_t total_time;              /* Total revolution time */
    
    /* Decoded data */
    uint8_t* bitstream;               /* Decoded bits */
    size_t bitstream_length;
    
    /* Quality */
    uft_score_t score;
    bool is_best;
    
    /* Statistics */
    uint32_t min_flux;
    uint32_t max_flux;
    double mean_flux;
    double stddev_flux;
    uint16_t jitter_count;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DISK STRUCTURE
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_disk_v3 {
    /* Format info */
    char format_name[UFT_V3_MAX_FORMAT_NAME];
    char format_variant[UFT_V3_MAX_FORMAT_NAME];
    uint32_t format_flags;
    
    /* Geometry */
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;        /* If fixed */
    uint16_t sector_size;             /* If fixed */
    bool variable_geometry;           /* Zones or variable */
    
    /* Track data */
    uft_track_v3_t* track_data[UFT_V3_MAX_TRACKS];
    
    /* Metadata */
    char disk_name[64];
    char disk_id[32];
    uint8_t dos_version;
    
    /* File system info (if applicable) */
    uint32_t free_blocks;
    uint32_t used_blocks;
    uint32_t total_blocks;
    
    /* Overall scoring */
    uft_score_t score;
    uft_diagnosis_list_t diagnosis;
    
    /* Protection */
    bool has_protection;
    char protection_type[64];
    
    /* Source info */
    char source_file[256];
    size_t source_size;
    uint32_t source_checksum;
    
    /* Parameters used */
    uft_params_v3_t params;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSER INTERFACE
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parser operation result
 */
typedef struct {
    bool success;
    int error_code;
    char error_message[256];
    uft_diagnosis_list_t* diagnosis;
} uft_result_t;

/**
 * @brief Parser callbacks for progress and logging
 */
typedef struct {
    void (*on_progress)(uint8_t track, uint8_t side, float percent, void* ctx);
    void (*on_track_complete)(uint8_t track, uint8_t side, uft_track_v3_t* track_data, void* ctx);
    void (*on_error)(uft_diagnosis_t* diag, void* ctx);
    void (*on_log)(int level, const char* message, void* ctx);
    void* context;
} uft_callbacks_t;

/**
 * @brief Parser interface - ALL parsers must implement this
 */
struct uft_parser_v3 {
    /* === IDENTIFICATION === */
    const char* name;                 /* Parser name */
    const char* description;          /* Parser description */
    const char* version;              /* Parser version */
    const char* extensions;           /* Supported extensions (comma-separated) */
    uint32_t format_id;               /* Unique format ID */
    
    /* === CAPABILITY FLAGS === */
    struct {
        bool can_read;
        bool can_write;
        bool can_analyze;
        bool supports_multi_rev;
        bool supports_protection;
        bool supports_weak_bits;
        bool supports_timing;
        bool supports_raw_flux;
        bool supports_verify;
    } capabilities;
    
    /* === PROBE (Does this parser handle this file?) === */
    int (*probe)(const uint8_t* data, size_t size);  /* Returns confidence 0-100 */
    
    /* === READ OPERATIONS === */
    uft_result_t (*read)(
        const uint8_t* data,
        size_t size,
        const uft_params_v3_t* params,
        uft_disk_v3_t* disk,
        uft_callbacks_t* callbacks
    );
    
    /* === WRITE OPERATIONS === */
    uft_result_t (*write)(
        const uft_disk_v3_t* disk,
        const uft_params_v3_t* params,
        uint8_t** out_data,
        size_t* out_size,
        uft_callbacks_t* callbacks
    );
    
    /* === ANALYZE (Read + detailed diagnosis) === */
    uft_result_t (*analyze)(
        const uint8_t* data,
        size_t size,
        const uft_params_v3_t* params,
        uft_disk_v3_t* disk,
        uft_diagnosis_list_t* diagnosis,
        uft_callbacks_t* callbacks
    );
    
    /* === VERIFY === */
    uft_result_t (*verify)(
        const uint8_t* original,
        size_t original_size,
        const uint8_t* written,
        size_t written_size,
        const uft_params_v3_t* params,
        uft_diagnosis_list_t* differences
    );
    
    /* === REPAIR === */
    uft_result_t (*repair)(
        uft_disk_v3_t* disk,
        const uft_params_v3_t* params,
        uft_diagnosis_list_t* changes
    );
    
    /* === CONVERT === */
    uft_result_t (*convert_to)(
        const uft_disk_v3_t* disk,
        uint32_t target_format_id,
        uft_disk_v3_t* output
    );
    
    /* === TRACK-LEVEL OPERATIONS === */
    uft_result_t (*read_track)(
        const uint8_t* data,
        size_t size,
        uint8_t track,
        uint8_t side,
        const uft_params_v3_t* params,
        uft_track_v3_t* track_data
    );
    
    uft_result_t (*write_track)(
        uft_track_v3_t* track_data,
        const uft_params_v3_t* params,
        uint8_t** out_data,
        size_t* out_size
    );
    
    uft_result_t (*diagnose_track)(
        uft_track_v3_t* track_data,
        uft_diagnosis_list_t* diagnosis
    );
    
    /* === SECTOR-LEVEL OPERATIONS === */
    uft_result_t (*read_sector)(
        const uint8_t* track_data,
        size_t track_size,
        uint8_t sector,
        const uft_params_v3_t* params,
        uft_sector_v3_t* sector_data
    );
    
    uft_result_t (*write_sector)(
        uft_track_v3_t* track,
        uft_sector_v3_t* sector,
        const uft_params_v3_t* params
    );
    
    /* === MULTI-REV OPERATIONS === */
    uft_result_t (*merge_revolutions)(
        uft_revolution_v3_t* revs,
        size_t rev_count,
        const uft_params_v3_t* params,
        uft_track_v3_t* output
    );
    
    uft_result_t (*select_best_revolution)(
        uft_revolution_v3_t* revs,
        size_t rev_count,
        uint8_t* best_index,
        uft_score_t* score
    );
    
    /* === PROTECTION OPERATIONS === */
    uft_result_t (*detect_protection)(
        const uft_disk_v3_t* disk,
        char* protection_name,
        size_t name_size,
        uft_diagnosis_list_t* details
    );
    
    uft_result_t (*preserve_protection)(
        const uft_disk_v3_t* source,
        uft_disk_v3_t* target
    );
    
    /* === PARAMETER MANAGEMENT === */
    void (*get_default_params)(uft_params_v3_t* params);
    bool (*validate_params)(const uft_params_v3_t* params, char* error, size_t error_size);
    
    /* === CLEANUP === */
    void (*free_disk)(uft_disk_v3_t* disk);
    void (*free_track)(uft_track_v3_t* track);
    void (*free_sector)(uft_sector_v3_t* sector);
    
    /* === INTERNAL STATE === */
    void* private_data;
};

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize default parameters
 */
void uft_params_v3_init(uft_params_v3_t* params);

/**
 * @brief Create empty disk structure
 */
uft_disk_v3_t* uft_disk_v3_create(void);

/**
 * @brief Free disk structure
 */
void uft_disk_v3_free(uft_disk_v3_t* disk);

/**
 * @brief Add diagnosis entry
 */
void uft_diagnosis_add(
    uft_diagnosis_list_t* list,
    uft_diagnosis_code_t code,
    uint8_t track,
    uint8_t side,
    uint8_t sector,
    const char* message,
    const char* suggestion
);

/**
 * @brief Generate diagnosis report as text
 */
char* uft_diagnosis_to_text(const uft_diagnosis_list_t* list);

/**
 * @brief Calculate score from sector
 */
void uft_score_sector(uft_sector_v3_t* sector);

/**
 * @brief Calculate score from track
 */
void uft_score_track(uft_track_v3_t* track);

/**
 * @brief Calculate score from disk
 */
void uft_score_disk(uft_disk_v3_t* disk);

/**
 * @brief Get diagnosis code name
 */
const char* uft_diagnosis_code_name(uft_diagnosis_code_t code);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARSER_V3_H */

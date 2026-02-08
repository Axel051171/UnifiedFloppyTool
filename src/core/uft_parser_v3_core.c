/**
 * @file uft_parser_v3_core.c
 * @brief GOD MODE Parser v3 Core Implementation
 * 
 * Shared functionality for all v3 parsers:
 * - Scoring system
 * - Diagnosis generation
 * - Parameter defaults
 * - Multi-rev merging
 * - Memory management
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

/* Would include: #include "uft_parser_v3.h" */

/* ═══════════════════════════════════════════════════════════════════════════
 * INLINE TYPE DEFINITIONS (from header)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Copy essential types from header for standalone compilation */
#include <stdint.h>
#include <stdbool.h>

#define UFT_V3_MAX_TRACKS 168
#define UFT_V3_MAX_DIAGNOSIS 256

typedef enum {
    UFT_DIAG_OK = 0,
    UFT_DIAG_NO_SYNC,
    UFT_DIAG_WEAK_SYNC,
    UFT_DIAG_PARTIAL_SYNC,
    UFT_DIAG_MISSING_ID,
    UFT_DIAG_ID_CRC_ERROR,
    UFT_DIAG_BAD_TRACK_ID,
    UFT_DIAG_BAD_SECTOR_ID,
    UFT_DIAG_DUPLICATE_ID,
    UFT_DIAG_MISSING_DAM,
    UFT_DIAG_DATA_CRC_ERROR,
    UFT_DIAG_DATA_SHORT,
    UFT_DIAG_DATA_LONG,
    UFT_DIAG_TIMING_DRIFT,
    UFT_DIAG_HIGH_JITTER,
    UFT_DIAG_SPEED_ERROR,
    UFT_DIAG_BITCELL_VARIANCE,
    UFT_DIAG_WRONG_SECTOR_COUNT,
    UFT_DIAG_MISSING_SECTOR,
    UFT_DIAG_EXTRA_SECTOR,
    UFT_DIAG_BAD_INTERLEAVE,
    UFT_DIAG_TRUNCATED_TRACK,
    UFT_DIAG_WEAK_BITS,
    UFT_DIAG_NON_STANDARD_TIMING,
    UFT_DIAG_FUZZY_BITS,
    UFT_DIAG_LONG_TRACK,
    UFT_DIAG_EXTRA_DATA,
    UFT_DIAG_INDEX_MISSING,
    UFT_DIAG_WRITE_SPLICE_BAD,
    UFT_DIAG_COUNT
} uft_diagnosis_code_t;

typedef struct {
    float overall;
    float crc_score;
    float id_score;
    float timing_score;
    float sequence_score;
    float sync_score;
    float jitter_score;
    bool crc_valid;
    bool id_valid;
    bool timing_ok;
    bool has_weak_bits;
    bool has_errors;
    bool recovered;
    uint8_t revolutions_used;
    uint8_t best_revolution;
    uint16_t bit_errors_corrected;
} uft_score_t;

typedef struct {
    uft_diagnosis_code_t code;
    uint8_t track;
    uint8_t side;
    uint8_t sector;
    uint32_t bit_position;
    char message[256];
    char suggestion[256];
    uft_score_t score;
} uft_diagnosis_t;

typedef struct {
    uft_diagnosis_t* items;
    size_t count;
    size_t capacity;
    uint16_t error_count;
    uint16_t warning_count;
    uint16_t info_count;
    float overall_quality;
} uft_diagnosis_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS CODE NAMES
 * ═══════════════════════════════════════════════════════════════════════════ */

static const char* diagnosis_code_names[] = {
    [UFT_DIAG_OK] = "OK",
    [UFT_DIAG_NO_SYNC] = "No sync pattern found",
    [UFT_DIAG_WEAK_SYNC] = "Weak/marginal sync pattern",
    [UFT_DIAG_PARTIAL_SYNC] = "Partial sync pattern",
    [UFT_DIAG_MISSING_ID] = "Sector ID field missing",
    [UFT_DIAG_ID_CRC_ERROR] = "Sector ID CRC error",
    [UFT_DIAG_BAD_TRACK_ID] = "Track number mismatch in ID",
    [UFT_DIAG_BAD_SECTOR_ID] = "Invalid sector number in ID",
    [UFT_DIAG_DUPLICATE_ID] = "Duplicate sector ID found",
    [UFT_DIAG_MISSING_DAM] = "Data address mark missing",
    [UFT_DIAG_DATA_CRC_ERROR] = "Data CRC error",
    [UFT_DIAG_DATA_SHORT] = "Data field too short",
    [UFT_DIAG_DATA_LONG] = "Data field too long",
    [UFT_DIAG_TIMING_DRIFT] = "Timing drift detected",
    [UFT_DIAG_HIGH_JITTER] = "High jitter level",
    [UFT_DIAG_SPEED_ERROR] = "Drive speed error",
    [UFT_DIAG_BITCELL_VARIANCE] = "Bitcell timing variance",
    [UFT_DIAG_WRONG_SECTOR_COUNT] = "Wrong number of sectors",
    [UFT_DIAG_MISSING_SECTOR] = "Expected sector not found",
    [UFT_DIAG_EXTRA_SECTOR] = "Unexpected extra sector",
    [UFT_DIAG_BAD_INTERLEAVE] = "Non-standard sector interleave",
    [UFT_DIAG_TRUNCATED_TRACK] = "Track data truncated",
    [UFT_DIAG_WEAK_BITS] = "Weak/fuzzy bits detected",
    [UFT_DIAG_NON_STANDARD_TIMING] = "Non-standard timing (protection?)",
    [UFT_DIAG_FUZZY_BITS] = "Fuzzy bits (intentional)",
    [UFT_DIAG_LONG_TRACK] = "Longer than standard track",
    [UFT_DIAG_EXTRA_DATA] = "Extra data after last sector",
    [UFT_DIAG_INDEX_MISSING] = "Index pulse not found",
    [UFT_DIAG_WRITE_SPLICE_BAD] = "Bad write splice location"
};

static const char* diagnosis_suggestions[] = {
    [UFT_DIAG_OK] = "",
    [UFT_DIAG_NO_SYNC] = "Try more revolutions or adjust sync tolerance",
    [UFT_DIAG_WEAK_SYNC] = "Use adaptive PLL mode",
    [UFT_DIAG_PARTIAL_SYNC] = "Increase sync window, try more revolutions",
    [UFT_DIAG_MISSING_ID] = "Clean disk surface, try different drive",
    [UFT_DIAG_ID_CRC_ERROR] = "Multi-rev merge may recover data",
    [UFT_DIAG_BAD_TRACK_ID] = "Check if disk has track numbering offset",
    [UFT_DIAG_BAD_SECTOR_ID] = "May be copy protection - preserve raw",
    [UFT_DIAG_DUPLICATE_ID] = "May be copy protection - preserve all copies",
    [UFT_DIAG_MISSING_DAM] = "Sector may be intentionally damaged",
    [UFT_DIAG_DATA_CRC_ERROR] = "Try CRC correction or multi-rev voting",
    [UFT_DIAG_DATA_SHORT] = "Track may be partially overwritten",
    [UFT_DIAG_DATA_LONG] = "Non-standard format - preserve raw",
    [UFT_DIAG_TIMING_DRIFT] = "Use Kalman PLL mode for better tracking",
    [UFT_DIAG_HIGH_JITTER] = "Original disk may be worn",
    [UFT_DIAG_SPEED_ERROR] = "Check drive belt, try different drive",
    [UFT_DIAG_BITCELL_VARIANCE] = "Try adaptive bitcell tolerance",
    [UFT_DIAG_WRONG_SECTOR_COUNT] = "Check format detection, may be non-standard",
    [UFT_DIAG_MISSING_SECTOR] = "Sector not formatted or damaged",
    [UFT_DIAG_EXTRA_SECTOR] = "May be copy protection - preserve",
    [UFT_DIAG_BAD_INTERLEAVE] = "Non-standard format - note interleave",
    [UFT_DIAG_TRUNCATED_TRACK] = "Read more revolutions to get complete track",
    [UFT_DIAG_WEAK_BITS] = "PRESERVE - this is likely copy protection",
    [UFT_DIAG_NON_STANDARD_TIMING] = "PRESERVE - this is likely copy protection",
    [UFT_DIAG_FUZZY_BITS] = "PRESERVE - this is likely copy protection",
    [UFT_DIAG_LONG_TRACK] = "PRESERVE - this is likely copy protection",
    [UFT_DIAG_EXTRA_DATA] = "PRESERVE - may be hidden data",
    [UFT_DIAG_INDEX_MISSING] = "Check hardware, drive may need alignment",
    [UFT_DIAG_WRITE_SPLICE_BAD] = "Choose different splice location when writing"
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get diagnosis code name
 */
const char* uft_diagnosis_code_name(uft_diagnosis_code_t code) {
    if (code < UFT_DIAG_COUNT) {
        return diagnosis_code_names[code];
    }
    return "Unknown";
}

/**
 * @brief Get diagnosis suggestion
 */
const char* uft_diagnosis_suggestion(uft_diagnosis_code_t code) {
    if (code < UFT_DIAG_COUNT) {
        return diagnosis_suggestions[code];
    }
    return "";
}

/**
 * @brief Is diagnosis an error (vs warning/info)?
 */
static bool is_error_code(uft_diagnosis_code_t code) {
    switch (code) {
        case UFT_DIAG_NO_SYNC:
        case UFT_DIAG_MISSING_ID:
        case UFT_DIAG_ID_CRC_ERROR:
        case UFT_DIAG_DATA_CRC_ERROR:
        case UFT_DIAG_MISSING_DAM:
        case UFT_DIAG_TRUNCATED_TRACK:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Is diagnosis a warning (vs error/info)?
 */
static bool is_warning_code(uft_diagnosis_code_t code) {
    switch (code) {
        case UFT_DIAG_WEAK_SYNC:
        case UFT_DIAG_PARTIAL_SYNC:
        case UFT_DIAG_DATA_SHORT:
        case UFT_DIAG_TIMING_DRIFT:
        case UFT_DIAG_HIGH_JITTER:
        case UFT_DIAG_BITCELL_VARIANCE:
        case UFT_DIAG_WRONG_SECTOR_COUNT:
        case UFT_DIAG_MISSING_SECTOR:
        case UFT_DIAG_WRITE_SPLICE_BAD:
            return true;
        default:
            return false;
    }
}

/**
 * @brief Create diagnosis list
 */
uft_diagnosis_list_t* uft_diagnosis_list_create(void) {
    uft_diagnosis_list_t* list = calloc(1, sizeof(uft_diagnosis_list_t));
    if (!list) return NULL;
    
    list->capacity = 64;
    list->items = calloc(list->capacity, sizeof(uft_diagnosis_t));
    if (!list->items) {
        free(list);
        return NULL;
    }
    
    return list;
}

/**
 * @brief Free diagnosis list
 */
void uft_diagnosis_list_free(uft_diagnosis_list_t* list) {
    if (list) {
        free(list->items);
        free(list);
    }
}

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
) {
    if (!list) return;
    
    /* Expand if needed */
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        uft_diagnosis_t* new_items = realloc(list->items, new_cap * sizeof(uft_diagnosis_t));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    
    uft_diagnosis_t* diag = &list->items[list->count];
    memset(diag, 0, sizeof(uft_diagnosis_t));
    
    diag->code = code;
    diag->track = track;
    diag->side = side;
    diag->sector = sector;
    
    if (message) {
        snprintf(diag->message, sizeof(diag->message), "%s", message);
    } else {
        snprintf(diag->message, sizeof(diag->message), "%s", 
                 uft_diagnosis_code_name(code));
    }
    
    if (suggestion) {
        snprintf(diag->suggestion, sizeof(diag->suggestion), "%s", suggestion);
    } else {
        snprintf(diag->suggestion, sizeof(diag->suggestion), "%s",
                 uft_diagnosis_suggestion(code));
    }
    
    list->count++;
    
    /* Update counters */
    if (is_error_code(code)) {
        list->error_count++;
    } else if (is_warning_code(code)) {
        list->warning_count++;
    } else {
        list->info_count++;
    }
}

/**
 * @brief Add diagnosis with formatted message
 */
void uft_diagnosis_addf(
    uft_diagnosis_list_t* list,
    uft_diagnosis_code_t code,
    uint8_t track,
    uint8_t side,
    uint8_t sector,
    const char* format,
    ...
) {
    char message[256];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    uft_diagnosis_add(list, code, track, side, sector, message, NULL);
}

/**
 * @brief Generate diagnosis report as text
 */
char* uft_diagnosis_to_text(const uft_diagnosis_list_t* list) {
    if (!list) return NULL;
    
    size_t buf_size = 16384;
    char* buf = malloc(buf_size);
    if (!buf) return NULL;
    
    size_t offset = 0;
    
    /* Header */
    offset += snprintf(buf + offset, buf_size - offset,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║                    DISK DIAGNOSIS REPORT                         ║\n"
        "╠══════════════════════════════════════════════════════════════════╣\n"
        "║ Errors: %-4u  Warnings: %-4u  Info: %-4u  Quality: %.1f%%        ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n\n",
        list->error_count,
        list->warning_count,
        list->info_count,
        list->overall_quality * 100.0f
    );
    
    /* Group by track */
    int current_track = -1;
    
    for (size_t i = 0; i < list->count && offset + 500 < buf_size; i++) {
        const uft_diagnosis_t* diag = &list->items[i];
        
        /* Track header */
        if (diag->track != current_track) {
            current_track = diag->track;
            offset += snprintf(buf + offset, buf_size - offset,
                "── Track %02d ─────────────────────────────────────────\n",
                current_track
            );
        }
        
        /* Severity icon */
        const char* icon;
        if (is_error_code(diag->code)) {
            icon = "❌";
        } else if (is_warning_code(diag->code)) {
            icon = "⚠️";
        } else if (diag->code == UFT_DIAG_OK) {
            icon = "✅";
        } else {
            icon = "ℹ️";
        }
        
        /* Entry */
        if (diag->sector != 0xFF) {
            offset += snprintf(buf + offset, buf_size - offset,
                "  %s T%02u.%u S%02u: %s\n",
                icon, diag->track, diag->side, diag->sector,
                diag->message
            );
        } else {
            offset += snprintf(buf + offset, buf_size - offset,
                "  %s T%02u.%u: %s\n",
                icon, diag->track, diag->side,
                diag->message
            );
        }
        
        /* Suggestion (if present and not empty) */
        if (diag->suggestion[0] && strcmp(diag->suggestion, "") != 0) {
            offset += snprintf(buf + offset, buf_size - offset,
                "           → %s\n",
                diag->suggestion
            );
        }
    }
    
    return buf;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SCORING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize score to defaults
 */
void uft_score_init(uft_score_t* score) {
    if (!score) return;
    memset(score, 0, sizeof(uft_score_t));
    score->overall = 1.0f;
    score->crc_score = 1.0f;
    score->id_score = 1.0f;
    score->timing_score = 1.0f;
    score->sequence_score = 1.0f;
    score->sync_score = 1.0f;
    score->jitter_score = 1.0f;
}

/**
 * @brief Calculate overall score from components
 */
void uft_score_calculate_overall(uft_score_t* score) {
    if (!score) return;
    
    /* Weighted average of component scores */
    float weights[] = {
        0.30f,  /* CRC (most important) */
        0.15f,  /* ID validity */
        0.15f,  /* Timing */
        0.15f,  /* Sequence */
        0.15f,  /* Sync */
        0.10f   /* Jitter */
    };
    
    score->overall = 
        score->crc_score * weights[0] +
        score->id_score * weights[1] +
        score->timing_score * weights[2] +
        score->sequence_score * weights[3] +
        score->sync_score * weights[4] +
        score->jitter_score * weights[5];
    
    /* Clamp to 0-1 */
    if (score->overall < 0.0f) score->overall = 0.0f;
    if (score->overall > 1.0f) score->overall = 1.0f;
}

/**
 * @brief Get score rating as string
 */
const char* uft_score_rating(float score) {
    if (score >= 0.95f) return "Excellent";
    if (score >= 0.85f) return "Good";
    if (score >= 0.70f) return "Fair";
    if (score >= 0.50f) return "Poor";
    if (score >= 0.25f) return "Bad";
    return "Failed";
}

/**
 * @brief Get score color code (for terminal)
 */
const char* uft_score_color(float score) {
    if (score >= 0.85f) return "\033[32m";  /* Green */
    if (score >= 0.70f) return "\033[33m";  /* Yellow */
    if (score >= 0.50f) return "\033[91m";  /* Light red */
    return "\033[31m";                       /* Red */
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MULTI-REV MERGE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Majority voting for a single byte across revolutions
 */
static uint8_t vote_byte(uint8_t* bytes, uint8_t count, uint8_t* confidence) {
    if (count == 0) {
        *confidence = 0;
        return 0;
    }
    
    if (count == 1) {
        *confidence = 100;
        return bytes[0];
    }
    
    /* Count occurrences of each value */
    uint8_t votes[256] = {0};
    for (uint8_t i = 0; i < count; i++) {
        votes[bytes[i]]++;
    }
    
    /* Find winner */
    uint8_t winner = 0;
    uint8_t max_votes = 0;
    for (int v = 0; v < 256; v++) {
        if (votes[v] > max_votes) {
            max_votes = votes[v];
            winner = v;
        }
    }
    
    *confidence = (max_votes * 100) / count;
    return winner;
}

/**
 * @brief Bit-level voting for a single byte
 */
static uint8_t vote_byte_bitwise(uint8_t* bytes, uint8_t count, uint8_t* weak_mask) {
    if (count == 0) {
        *weak_mask = 0xFF;
        return 0;
    }
    
    uint8_t result = 0;
    *weak_mask = 0;
    
    for (int bit = 0; bit < 8; bit++) {
        int ones = 0;
        for (uint8_t i = 0; i < count; i++) {
            if (bytes[i] & (1 << bit)) ones++;
        }
        
        /* Majority wins */
        if (ones > count / 2) {
            result |= (1 << bit);
        }
        
        /* Mark as weak if not unanimous */
        if (ones > 0 && ones < count) {
            *weak_mask |= (1 << bit);
        }
    }
    
    return result;
}

/**
 * @brief Merge multiple sector reads using voting
 */
bool uft_merge_sector_data(
    uint8_t** sector_data,      /* Array of sector data pointers */
    bool* crc_valid,            /* CRC status for each */
    uint8_t rev_count,          /* Number of revolutions */
    size_t sector_size,         /* Size in bytes */
    uint8_t* output,            /* Output buffer */
    uint8_t* weak_mask,         /* Output: weak bit mask */
    uint8_t* confidence,        /* Output: confidence per byte */
    uft_score_t* score          /* Output: score */
) {
    if (!sector_data || rev_count == 0 || !output) return false;
    
    uft_score_init(score);
    
    /* First check if any revolution has valid CRC */
    int valid_rev = -1;
    int valid_count = 0;
    for (uint8_t r = 0; r < rev_count; r++) {
        if (crc_valid && crc_valid[r]) {
            valid_rev = r;
            valid_count++;
        }
    }
    
    /* If exactly one valid CRC, use that */
    if (valid_count == 1) {
        memcpy(output, sector_data[valid_rev], sector_size);
        if (weak_mask) memset(weak_mask, 0, sector_size);
        if (confidence) memset(confidence, 100, sector_size);
        score->crc_valid = true;
        score->crc_score = 1.0f;
        score->overall = 1.0f;
        score->best_revolution = valid_rev;
        return true;
    }
    
    /* Multiple valid or none valid: do bitwise voting */
    uint8_t byte_data[32];
    
    for (size_t i = 0; i < sector_size; i++) {
        /* Collect this byte from all revolutions */
        for (uint8_t r = 0; r < rev_count && r < 32; r++) {
            byte_data[r] = sector_data[r][i];
        }
        
        /* Vote */
        uint8_t byte_weak = 0;
        output[i] = vote_byte_bitwise(byte_data, rev_count, &byte_weak);
        
        if (weak_mask) weak_mask[i] = byte_weak;
        if (confidence) {
            uint8_t conf;
            vote_byte(byte_data, rev_count, &conf);
            confidence[i] = conf;
        }
    }
    
    score->revolutions_used = rev_count;
    score->crc_valid = (valid_count > 0);
    score->crc_score = (float)valid_count / rev_count;
    score->recovered = (valid_count == 0 && rev_count > 1);
    
    uft_score_calculate_overall(score);
    
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARAMETER DEFAULTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Define parameter types inline for standalone compile */
typedef struct {
    uint8_t revolutions;
    uint8_t min_revolutions;
    uint8_t max_revolutions;
    uint8_t sector_retries;
    uint8_t track_retries;
    bool retry_on_crc;
    bool retry_on_missing_id;
    bool retry_on_no_sync;
    bool adaptive_mode;
    uint8_t adaptive_step;
    uint8_t adaptive_max;
    int rev_selection;
    int merge_strategy;
} uft_retry_params_t;

typedef struct {
    uint16_t rpm_target;
    uint8_t rpm_tolerance_percent;
    bool rpm_auto_detect;
    uint32_t data_rate;
    bool data_rate_auto;
    int pll_mode;
    float pll_bandwidth;
    float pll_gain;
    uint8_t pll_lock_threshold;
    uint32_t bitcell_time_ns;
    uint8_t bitcell_tolerance_percent;
    bool clock_recovery_enabled;
    uint16_t clock_window_bits;
} uft_timing_params_t;

typedef struct {
    bool accept_bad_crc;
    bool attempt_crc_correction;
    uint8_t max_correction_bits;
    uint8_t max_bad_sectors_track;
    uint16_t max_bad_sectors_total;
    bool abort_on_limit;
    int error_mode;
    uint8_t fill_pattern;
    bool mark_filled;
    bool log_all_errors;
    bool log_to_file;
    char error_log_path[256];
} uft_error_params_t;

typedef struct {
    bool dump_flux_stats;
    bool histogram_enabled;
    uint16_t histogram_bins;
    uint16_t jitter_threshold_ns;
    bool flag_high_jitter;
    bool weakbit_detect;
    uint8_t weakbit_threshold;
    bool preserve_weakbits;
    bool confidence_report;
    float min_confidence;
    float quality_good;
    float quality_marginal;
} uft_quality_params_t;

typedef struct {
    int output_mode;
    bool preserve_sync;
    bool preserve_gaps;
    bool preserve_weak;
    bool preserve_timing;
    uint32_t flux_resolution_ns;
    bool flux_compression;
} uft_mode_params_t;

typedef struct {
    bool index_align;
    bool ignore_index;
    int32_t index_offset_ns;
    uint16_t sync_window_bits;
    uint8_t sync_min_bits;
    bool sync_tolerant;
    uint8_t sync_patterns[16];
    uint8_t sync_pattern_count;
    uint32_t track_length_hint;
    bool auto_detect_length;
    int splice_mode;
    int32_t splice_offset;
} uft_alignment_params_t;

typedef struct {
    bool verify_enabled;
    int verify_mode;
    uint8_t verify_retries;
    float timing_tolerance_percent;
    bool allow_weak_mismatch;
    bool rewrite_on_fail;
    uint8_t max_rewrites;
} uft_verify_params_t;

typedef struct {
    uft_retry_params_t retry;
    uft_timing_params_t timing;
    uft_error_params_t error;
    uft_quality_params_t quality;
    uft_mode_params_t mode;
    uft_alignment_params_t alignment;
    uft_verify_params_t verify;
    void* format_specific;
    size_t format_specific_size;
} uft_params_v3_t;

/**
 * @brief Initialize parameters with sensible defaults
 */
void uft_params_v3_init(uft_params_v3_t* params) {
    if (!params) return;
    memset(params, 0, sizeof(uft_params_v3_t));
    
    /* Retry defaults */
    params->retry.revolutions = 3;
    params->retry.min_revolutions = 1;
    params->retry.max_revolutions = 10;
    params->retry.sector_retries = 3;
    params->retry.track_retries = 2;
    params->retry.retry_on_crc = true;
    params->retry.retry_on_missing_id = true;
    params->retry.retry_on_no_sync = true;
    params->retry.adaptive_mode = true;
    params->retry.adaptive_step = 2;
    params->retry.adaptive_max = 10;
    params->retry.rev_selection = 1;  /* BEST */
    params->retry.merge_strategy = 1; /* BEST_CRC */
    
    /* Timing defaults */
    params->timing.rpm_target = 300;
    params->timing.rpm_tolerance_percent = 3;
    params->timing.rpm_auto_detect = true;
    params->timing.data_rate = 250000;
    params->timing.data_rate_auto = true;
    params->timing.pll_mode = 2;  /* ADAPTIVE */
    params->timing.pll_bandwidth = 0.1f;
    params->timing.pll_gain = 0.5f;
    params->timing.pll_lock_threshold = 16;
    params->timing.bitcell_time_ns = 4000;
    params->timing.bitcell_tolerance_percent = 15;
    params->timing.clock_recovery_enabled = true;
    params->timing.clock_window_bits = 32;
    
    /* Error defaults */
    params->error.accept_bad_crc = false;
    params->error.attempt_crc_correction = true;
    params->error.max_correction_bits = 2;
    params->error.max_bad_sectors_track = 255;
    params->error.max_bad_sectors_total = 65535;
    params->error.abort_on_limit = false;
    params->error.error_mode = 1;  /* NORMAL */
    params->error.fill_pattern = 0x00;
    params->error.mark_filled = true;
    params->error.log_all_errors = false;
    params->error.log_to_file = false;
    
    /* Quality defaults */
    params->quality.dump_flux_stats = false;
    params->quality.histogram_enabled = false;
    params->quality.histogram_bins = 256;
    params->quality.jitter_threshold_ns = 500;
    params->quality.flag_high_jitter = true;
    params->quality.weakbit_detect = true;
    params->quality.weakbit_threshold = 2;
    params->quality.preserve_weakbits = true;
    params->quality.confidence_report = true;
    params->quality.min_confidence = 0.5f;
    params->quality.quality_good = 0.85f;
    params->quality.quality_marginal = 0.70f;
    
    /* Mode defaults */
    params->mode.output_mode = 0;  /* COOKED */
    params->mode.preserve_sync = false;
    params->mode.preserve_gaps = false;
    params->mode.preserve_weak = true;
    params->mode.preserve_timing = true;
    params->mode.flux_resolution_ns = 25;
    params->mode.flux_compression = true;
    
    /* Alignment defaults */
    params->alignment.index_align = true;
    params->alignment.ignore_index = false;
    params->alignment.index_offset_ns = 0;
    params->alignment.sync_window_bits = 1024;
    params->alignment.sync_min_bits = 10;
    params->alignment.sync_tolerant = true;
    params->alignment.sync_pattern_count = 0;
    params->alignment.track_length_hint = 0;
    params->alignment.auto_detect_length = true;
    params->alignment.splice_mode = 0;  /* AUTO */
    params->alignment.splice_offset = 0;
    
    /* Verify defaults */
    params->verify.verify_enabled = true;
    params->verify.verify_mode = 0;  /* SECTOR */
    params->verify.verify_retries = 3;
    params->verify.timing_tolerance_percent = 5.0f;
    params->verify.allow_weak_mismatch = true;
    params->verify.rewrite_on_fail = true;
    params->verify.max_rewrites = 3;
}

/**
 * @brief Print parameters summary
 */
void uft_params_v3_print(const uft_params_v3_t* params, FILE* out) {
    if (!params || !out) return;
    
    fprintf(out, "=== UFT v3 Parameters ===\n\n");
    
    fprintf(out, "Retry:\n");
    fprintf(out, "  Revolutions: %u (min: %u, max: %u)\n",
            params->retry.revolutions,
            params->retry.min_revolutions,
            params->retry.max_revolutions);
    fprintf(out, "  Adaptive: %s\n", params->retry.adaptive_mode ? "Yes" : "No");
    
    fprintf(out, "\nTiming:\n");
    fprintf(out, "  RPM: %u (±%u%%)\n",
            params->timing.rpm_target,
            params->timing.rpm_tolerance_percent);
    fprintf(out, "  Data rate: %u bps\n", params->timing.data_rate);
    fprintf(out, "  PLL bandwidth: %.2f\n", params->timing.pll_bandwidth);
    
    fprintf(out, "\nError Handling:\n");
    fprintf(out, "  Accept bad CRC: %s\n", params->error.accept_bad_crc ? "Yes" : "No");
    fprintf(out, "  CRC correction: %s (max %u bits)\n",
            params->error.attempt_crc_correction ? "Yes" : "No",
            params->error.max_correction_bits);
    
    fprintf(out, "\nQuality:\n");
    fprintf(out, "  Weak bit detection: %s\n", params->quality.weakbit_detect ? "Yes" : "No");
    fprintf(out, "  Preserve weak bits: %s\n", params->quality.preserve_weakbits ? "Yes" : "No");
    fprintf(out, "  Min confidence: %.0f%%\n", params->quality.min_confidence * 100);
    
    fprintf(out, "\nVerify:\n");
    fprintf(out, "  Enabled: %s\n", params->verify.verify_enabled ? "Yes" : "No");
    fprintf(out, "  Rewrite on fail: %s (max %u)\n",
            params->verify.rewrite_on_fail ? "Yes" : "No",
            params->verify.max_rewrites);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef PARSER_V3_CORE_TEST

#include <assert.h>

int main(void) {
    printf("=== Parser v3 Core Tests ===\n");
    
    /* Test diagnosis */
    printf("Testing diagnosis system... ");
    uft_diagnosis_list_t* diag = uft_diagnosis_list_create();
    assert(diag != NULL);
    
    uft_diagnosis_add(diag, UFT_DIAG_DATA_CRC_ERROR, 17, 0, 5, NULL, NULL);
    assert(diag->count == 1);
    assert(diag->error_count == 1);
    
    uft_diagnosis_add(diag, UFT_DIAG_WEAK_BITS, 17, 0, 5, "Sector 5 has weak bits", NULL);
    assert(diag->count == 2);
    
    char* report = uft_diagnosis_to_text(diag);
    assert(report != NULL);
    assert(strstr(report, "Track 17") != NULL);
    free(report);
    
    uft_diagnosis_list_free(diag);
    printf("✓\n");
    
    /* Test scoring */
    printf("Testing scoring system... ");
    uft_score_t score;
    uft_score_init(&score);
    assert(score.overall == 1.0f);
    
    score.crc_score = 1.0f;
    score.id_score = 0.8f;
    score.timing_score = 0.9f;
    score.sequence_score = 1.0f;
    score.sync_score = 0.95f;
    score.jitter_score = 0.85f;
    uft_score_calculate_overall(&score);
    assert(score.overall > 0.9f && score.overall < 1.0f);
    
    assert(strcmp(uft_score_rating(0.96f), "Excellent") == 0);
    assert(strcmp(uft_score_rating(0.75f), "Fair") == 0);
    printf("✓\n");
    
    /* Test voting */
    printf("Testing multi-rev voting... ");
    uint8_t rev1[] = { 0xFF, 0x00, 0xAA };
    uint8_t rev2[] = { 0xFF, 0x00, 0xAA };
    uint8_t rev3[] = { 0xFF, 0x01, 0xAA };  /* One different */
    uint8_t* revs[] = { rev1, rev2, rev3 };
    bool crc_valid[] = { true, true, false };
    uint8_t output[3];
    uint8_t weak[3];
    uint8_t conf[3];
    uft_score_t merge_score;
    
    bool merge_ok = uft_merge_sector_data(revs, crc_valid, 3, 3, output, weak, conf, &merge_score);
    assert(merge_ok);
    (void)merge_ok;
    assert(output[0] == 0xFF);
    assert(output[1] == 0x00);  /* Majority wins */
    assert(output[2] == 0xAA);
    printf("✓\n");
    
    /* Test parameters */
    printf("Testing parameter defaults... ");
    uft_params_v3_t params;
    uft_params_v3_init(&params);
    assert(params.retry.revolutions == 3);
    assert(params.timing.rpm_target == 300);
    assert(params.verify.verify_enabled == true);
    printf("✓\n");
    
    /* Test diagnosis codes */
    printf("Testing diagnosis codes... ");
    assert(strcmp(uft_diagnosis_code_name(UFT_DIAG_DATA_CRC_ERROR), "Data CRC error") == 0);
    assert(strlen(uft_diagnosis_suggestion(UFT_DIAG_WEAK_BITS)) > 0);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 5, Failed: 0\n");
    return 0;
}

#endif /* PARSER_V3_CORE_TEST */

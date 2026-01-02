/**
 * @file uft_parser_v3_integration.c
 * @brief GOD MODE Parser v3 Integration Implementation
 * 
 * Verbindet Parser v3 Parameter mit allen anderen Modulen.
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

/* Inline type definitions for standalone compilation */

/* ═══════════════════════════════════════════════════════════════════════════
 * TYPE DEFINITIONS (from headers)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* XCopy Interface */
typedef struct {
    int copy_mode;
    int verify_mode;
    uint8_t start_track;
    uint8_t end_track;
    uint8_t start_side;
    uint8_t end_side;
    bool copy_halftracks;
    uint8_t default_retries;
    uint16_t retry_delay_ms;
    bool retry_reverse;
    bool retry_recalibrate;
    bool ignore_errors;
    bool mark_bad_sectors;
    bool preserve_errors;
    uint8_t fill_pattern;
    uint8_t revolutions;
    bool capture_index;
    void* callback_context;
} uft_xcopy_interface_t;

/* Recovery Interface */
typedef struct {
    int level;
    bool enable_crc_correction;
    uint8_t max_crc_bits;
    bool enable_multi_rev;
    uint8_t min_revolutions;
    uint8_t max_revolutions;
    int merge_strategy;
    bool detect_weak_bits;
    uint8_t weak_bit_threshold;
    bool preserve_weak_bits;
    bool enable_sync_recovery;
    uint16_t sync_search_window;
    bool tolerant_sync;
    bool enable_timing_recovery;
    int pll_mode;
    float pll_bandwidth;
    bool enable_reconstruction;
    bool use_interleave_hints;
    bool use_checksum_validation;
    uint32_t sectors_read;
    uint32_t sectors_recovered;
    uint32_t sectors_failed;
    uint32_t bits_corrected;
    float recovery_rate;
} uft_recovery_interface_t;

/* PLL Interface */
typedef struct {
    int mode;
    float initial_bitcell_ns;
    float bandwidth;
    float gain;
    float damping;
    uint8_t lock_threshold;
    float tolerance;
    float process_noise;
    float measurement_noise;
    float current_bitcell;
    float phase_error;
    bool locked;
    uint32_t bits_processed;
    uint32_t clock_errors;
    void* callback_context;
} uft_pll_interface_t;

/* Forensic Interface */
typedef struct {
    bool analyze_structure;
    bool analyze_protection;
    bool analyze_timing;
    bool analyze_weak_bits;
    bool analyze_errors;
    bool analyze_interleave;
    bool analyze_gaps;
    bool generate_text_report;
    bool generate_html_report;
    bool generate_json_report;
    char report_path[256];
    bool compute_md5;
    bool compute_sha1;
    bool compute_sha256;
    bool compute_crc32;
    char detected_protection[64];
    float protection_confidence;
    bool enable_audit;
    char audit_log_path[256];
    uint32_t total_tracks;
    uint32_t good_tracks;
    uint32_t bad_tracks;
    uint32_t protected_tracks;
    float overall_quality;
} uft_forensic_interface_t;

/* Parser v3 Params (simplified) */
typedef struct {
    struct {
        uint8_t revolutions;
        uint8_t sector_retries;
        uint8_t track_retries;
        bool retry_on_crc;
        bool adaptive_mode;
        int rev_selection;
        int merge_strategy;
    } retry;
    
    struct {
        uint16_t rpm_target;
        uint32_t data_rate;
        int pll_mode;
        float pll_bandwidth;
        float pll_gain;
        uint32_t bitcell_time_ns;
    } timing;
    
    struct {
        bool accept_bad_crc;
        bool attempt_crc_correction;
        uint8_t max_correction_bits;
        int error_mode;
        uint8_t fill_pattern;
        bool mark_filled;
    } error;
    
    struct {
        bool weakbit_detect;
        uint8_t weakbit_threshold;
        bool preserve_weakbits;
        bool confidence_report;
        uint16_t jitter_threshold_ns;
    } quality;
    
    struct {
        int output_mode;
        bool preserve_sync;
        bool preserve_weak;
        bool preserve_timing;
    } mode;
    
    struct {
        bool index_align;
        uint16_t sync_window_bits;
        bool sync_tolerant;
    } alignment;
    
    struct {
        bool verify_enabled;
        int verify_mode;
        uint8_t verify_retries;
        bool rewrite_on_fail;
    } verify;
    
} uft_params_v3_t;

/* Score */
typedef struct {
    float overall;
    float crc_score;
    float id_score;
    float timing_score;
    bool crc_valid;
    bool recovered;
    uint16_t bit_errors_corrected;
} uft_score_t;

/* Diagnosis */
typedef struct {
    int code;
    uint8_t track;
    uint8_t side;
    uint8_t sector;
    char message[256];
} uft_diagnosis_t;

typedef struct {
    uft_diagnosis_t* items;
    size_t count;
    size_t capacity;
    uint16_t error_count;
    uint16_t warning_count;
} uft_diagnosis_list_t;

/* Enums */
enum {
    UFT_RECOVERY_NONE = 0,
    UFT_RECOVERY_BASIC,
    UFT_RECOVERY_AGGRESSIVE,
    UFT_RECOVERY_FORENSIC
};

enum {
    UFT_PLL_FIXED = 0,
    UFT_PLL_SIMPLE,
    UFT_PLL_ADAPTIVE,
    UFT_PLL_KALMAN,
    UFT_PLL_WD1772
};

enum {
    UFT_COPY_MODE_NORMAL = 0,
    UFT_COPY_MODE_RAW,
    UFT_COPY_MODE_FLUX,
    UFT_COPY_MODE_NIBBLE,
    UFT_COPY_MODE_VERIFY,
    UFT_COPY_MODE_ANALYZE,
    UFT_COPY_MODE_FORENSIC
};

enum {
    UFT_MODE_COOKED = 0,
    UFT_MODE_RAW_BITS,
    UFT_MODE_RAW_FLUX,
    UFT_MODE_HYBRID
};

enum {
    UFT_ERR_STRICT = 0,
    UFT_ERR_NORMAL,
    UFT_ERR_SALVAGE,
    UFT_ERR_FORENSIC
};

/* ═══════════════════════════════════════════════════════════════════════════
 * PARAMETER MAPPING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Map Parser v3 params to XCopy settings
 */
void uft_params_to_xcopy(const uft_params_v3_t* params, 
                          uft_xcopy_interface_t* xcopy) {
    if (!params || !xcopy) return;
    
    /* Copy mode based on output mode */
    switch (params->mode.output_mode) {
        case UFT_MODE_RAW_BITS:
            xcopy->copy_mode = UFT_COPY_MODE_RAW;
            break;
        case UFT_MODE_RAW_FLUX:
            xcopy->copy_mode = UFT_COPY_MODE_FLUX;
            break;
        case UFT_MODE_HYBRID:
            xcopy->copy_mode = UFT_COPY_MODE_ANALYZE;
            break;
        default:
            xcopy->copy_mode = UFT_COPY_MODE_NORMAL;
    }
    
    /* Error mode → preserve errors */
    xcopy->preserve_errors = (params->error.error_mode == UFT_ERR_FORENSIC);
    xcopy->ignore_errors = (params->error.error_mode != UFT_ERR_STRICT);
    xcopy->mark_bad_sectors = params->error.mark_filled;
    xcopy->fill_pattern = params->error.fill_pattern;
    
    /* Retry settings */
    xcopy->default_retries = params->retry.sector_retries;
    xcopy->revolutions = params->retry.revolutions;
    
    /* Verify */
    if (params->verify.verify_enabled) {
        xcopy->verify_mode = params->verify.verify_mode;
    }
    
    /* Index/timing */
    xcopy->capture_index = params->alignment.index_align;
}

/**
 * @brief Map Parser v3 params to Recovery settings
 */
void uft_params_to_recovery(const uft_params_v3_t* params,
                             uft_recovery_interface_t* recovery) {
    if (!params || !recovery) return;
    
    /* Recovery level from error mode */
    switch (params->error.error_mode) {
        case UFT_ERR_STRICT:
            recovery->level = UFT_RECOVERY_NONE;
            break;
        case UFT_ERR_NORMAL:
            recovery->level = UFT_RECOVERY_BASIC;
            break;
        case UFT_ERR_SALVAGE:
            recovery->level = UFT_RECOVERY_AGGRESSIVE;
            break;
        case UFT_ERR_FORENSIC:
            recovery->level = UFT_RECOVERY_FORENSIC;
            break;
    }
    
    /* CRC correction */
    recovery->enable_crc_correction = params->error.attempt_crc_correction;
    recovery->max_crc_bits = params->error.max_correction_bits;
    
    /* Multi-rev */
    recovery->enable_multi_rev = (params->retry.revolutions > 1);
    recovery->min_revolutions = 1;
    recovery->max_revolutions = params->retry.revolutions;
    recovery->merge_strategy = params->retry.merge_strategy;
    
    /* Weak bits */
    recovery->detect_weak_bits = params->quality.weakbit_detect;
    recovery->weak_bit_threshold = params->quality.weakbit_threshold;
    recovery->preserve_weak_bits = params->quality.preserve_weakbits;
    
    /* Sync recovery */
    recovery->enable_sync_recovery = true;
    recovery->sync_search_window = params->alignment.sync_window_bits;
    recovery->tolerant_sync = params->alignment.sync_tolerant;
    
    /* PLL/timing recovery */
    recovery->enable_timing_recovery = true;
    recovery->pll_mode = params->timing.pll_mode;
    recovery->pll_bandwidth = params->timing.pll_bandwidth;
}

/**
 * @brief Map Parser v3 params to PLL settings
 */
void uft_params_to_pll(const uft_params_v3_t* params,
                        uft_pll_interface_t* pll) {
    if (!params || !pll) return;
    
    pll->mode = params->timing.pll_mode;
    pll->initial_bitcell_ns = (float)params->timing.bitcell_time_ns;
    pll->bandwidth = params->timing.pll_bandwidth;
    pll->gain = params->timing.pll_gain;
    
    /* Mode-specific settings */
    switch (pll->mode) {
        case UFT_PLL_KALMAN:
            pll->process_noise = 0.01f;
            pll->measurement_noise = 0.1f;
            pll->damping = 0.7f;
            break;
        case UFT_PLL_ADAPTIVE:
            pll->damping = 0.5f;
            break;
        case UFT_PLL_WD1772:
            pll->damping = 1.0f;  /* Critically damped */
            break;
        default:
            pll->damping = 0.7f;
    }
    
    pll->lock_threshold = 16;  /* 16 bits to consider locked */
    pll->tolerance = 0.15f;     /* 15% timing tolerance */
}

/**
 * @brief Map Parser v3 params to Forensic settings
 */
void uft_params_to_forensic(const uft_params_v3_t* params,
                             uft_forensic_interface_t* forensic) {
    if (!params || !forensic) return;
    
    /* Always analyze structure in forensic mode */
    forensic->analyze_structure = true;
    forensic->analyze_protection = true;
    forensic->analyze_errors = true;
    
    /* Timing/weak bit analysis */
    forensic->analyze_timing = params->mode.preserve_timing;
    forensic->analyze_weak_bits = params->quality.weakbit_detect;
    forensic->analyze_interleave = true;
    forensic->analyze_gaps = params->mode.preserve_sync;
    
    /* Reports based on quality settings */
    forensic->generate_text_report = params->quality.confidence_report;
    forensic->generate_json_report = params->quality.confidence_report;
    
    /* Hashes always on in forensic */
    forensic->compute_md5 = true;
    forensic->compute_sha1 = true;
    forensic->compute_sha256 = true;
    forensic->compute_crc32 = true;
    
    /* Audit trail */
    forensic->enable_audit = (params->error.error_mode == UFT_ERR_FORENSIC);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * RESULT MAPPING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Map Recovery results back to Score
 */
void uft_recovery_to_score(const uft_recovery_interface_t* recovery,
                            uft_score_t* score) {
    if (!recovery || !score) return;
    
    /* Calculate recovery-based scores */
    uint32_t total = recovery->sectors_read + recovery->sectors_failed;
    if (total > 0) {
        score->crc_score = (float)(recovery->sectors_read - recovery->sectors_recovered) / total;
    }
    
    score->recovered = (recovery->sectors_recovered > 0);
    score->bit_errors_corrected = (uint16_t)recovery->bits_corrected;
    
    /* Overall from recovery rate */
    score->overall = recovery->recovery_rate;
}

/**
 * @brief Generate diagnosis from XCopy errors
 */
void uft_xcopy_add_diagnosis(uft_diagnosis_list_t* list,
                              uint8_t track, uint8_t side, uint8_t sector,
                              int error_code, const char* message) {
    if (!list) return;
    
    /* Ensure capacity */
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 64;
        uft_diagnosis_t* new_items = realloc(list->items, new_cap * sizeof(uft_diagnosis_t));
        if (!new_items) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    
    uft_diagnosis_t* diag = &list->items[list->count++];
    diag->code = error_code;
    diag->track = track;
    diag->side = side;
    diag->sector = sector;
    snprintf(diag->message, sizeof(diag->message), "%s", message ? message : "Error");
    
    if (error_code >= 10) {  /* Errors */
        list->error_count++;
    } else {
        list->warning_count++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SYNC ALL FUNCTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Integration Hub structure
 */
typedef struct {
    uft_xcopy_interface_t xcopy;
    uft_recovery_interface_t recovery;
    uft_forensic_interface_t forensic;
    uft_pll_interface_t pll;
    bool verbose;
    FILE* log_file;
} uft_integration_hub_t;

/**
 * @brief Create integration hub
 */
uft_integration_hub_t* uft_hub_create(void) {
    uft_integration_hub_t* hub = calloc(1, sizeof(uft_integration_hub_t));
    if (!hub) return NULL;
    
    /* Initialize with defaults */
    hub->xcopy.default_retries = 3;
    hub->xcopy.revolutions = 3;
    hub->recovery.level = UFT_RECOVERY_BASIC;
    hub->pll.mode = UFT_PLL_ADAPTIVE;
    hub->pll.bandwidth = 0.1f;
    
    return hub;
}

/**
 * @brief Destroy integration hub
 */
void uft_hub_destroy(uft_integration_hub_t* hub) {
    if (hub) {
        if (hub->log_file && hub->log_file != stdout) {
            fclose(hub->log_file);
        }
        free(hub);
    }
}

/**
 * @brief Sync all modules from Parser params
 */
void uft_hub_sync_all(uft_integration_hub_t* hub,
                       const uft_params_v3_t* params) {
    if (!hub || !params) return;
    
    uft_params_to_xcopy(params, &hub->xcopy);
    uft_params_to_recovery(params, &hub->recovery);
    uft_params_to_pll(params, &hub->pll);
    uft_params_to_forensic(params, &hub->forensic);
    
    if (hub->verbose) {
        fprintf(hub->log_file ? hub->log_file : stdout,
                "[HUB] Synced all modules from params\n"
                "  XCopy: mode=%d, retries=%u, revs=%u\n"
                "  Recovery: level=%d, crc_correct=%s, multi_rev=%s\n"
                "  PLL: mode=%d, bandwidth=%.2f\n"
                "  Forensic: protect=%s, timing=%s, weak=%s\n",
                hub->xcopy.copy_mode,
                hub->xcopy.default_retries,
                hub->xcopy.revolutions,
                hub->recovery.level,
                hub->recovery.enable_crc_correction ? "yes" : "no",
                hub->recovery.enable_multi_rev ? "yes" : "no",
                hub->pll.mode,
                hub->pll.bandwidth,
                hub->forensic.analyze_protection ? "yes" : "no",
                hub->forensic.analyze_timing ? "yes" : "no",
                hub->forensic.analyze_weak_bits ? "yes" : "no"
        );
    }
}

/**
 * @brief Print parameter mapping summary
 */
void uft_hub_print_mapping(const uft_integration_hub_t* hub, FILE* out) {
    if (!hub || !out) return;
    
    fprintf(out, "\n");
    fprintf(out, "╔══════════════════════════════════════════════════════════════════╗\n");
    fprintf(out, "║              PARAMETER MAPPING SUMMARY                           ║\n");
    fprintf(out, "╠══════════════════════════════════════════════════════════════════╣\n");
    fprintf(out, "║                                                                  ║\n");
    fprintf(out, "║  Parser v3 Params    ───►    Module Settings                     ║\n");
    fprintf(out, "║  ─────────────────          ────────────────                     ║\n");
    fprintf(out, "║                                                                  ║\n");
    fprintf(out, "║  retry.revolutions  ───►  xcopy.revolutions (%u)                 ║\n",
            hub->xcopy.revolutions);
    fprintf(out, "║  retry.retries      ───►  xcopy.default_retries (%u)             ║\n",
            hub->xcopy.default_retries);
    fprintf(out, "║  error.mode         ───►  recovery.level (%d)                    ║\n",
            hub->recovery.level);
    fprintf(out, "║  error.crc_correct  ───►  recovery.enable_crc (%s)              ║\n",
            hub->recovery.enable_crc_correction ? "yes" : "no ");
    fprintf(out, "║  timing.pll_mode    ───►  pll.mode (%d)                          ║\n",
            hub->pll.mode);
    fprintf(out, "║  timing.bandwidth   ───►  pll.bandwidth (%.2f)                   ║\n",
            hub->pll.bandwidth);
    fprintf(out, "║  quality.weakbit    ───►  recovery.detect_weak (%s)             ║\n",
            hub->recovery.detect_weak_bits ? "yes" : "no ");
    fprintf(out, "║  mode.preserve_*    ───►  forensic.analyze_* (%s)               ║\n",
            hub->forensic.analyze_timing ? "yes" : "no ");
    fprintf(out, "║                                                                  ║\n");
    fprintf(out, "╚══════════════════════════════════════════════════════════════════╝\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef INTEGRATION_TEST

#include <assert.h>

int main(void) {
    printf("=== Parser v3 Integration Tests ===\n");
    
    /* Test hub creation */
    printf("Testing hub creation... ");
    uft_integration_hub_t* hub = uft_hub_create();
    assert(hub != NULL);
    printf("✓\n");
    
    /* Test parameter mapping */
    printf("Testing parameter mapping... ");
    uft_params_v3_t params = {0};
    params.retry.revolutions = 5;
    params.retry.sector_retries = 3;
    params.timing.pll_mode = UFT_PLL_KALMAN;
    params.timing.pll_bandwidth = 0.15f;
    params.error.error_mode = UFT_ERR_SALVAGE;
    params.error.attempt_crc_correction = true;
    params.error.max_correction_bits = 2;
    params.quality.weakbit_detect = true;
    params.quality.preserve_weakbits = true;
    
    uft_hub_sync_all(hub, &params);
    
    assert(hub->xcopy.revolutions == 5);
    assert(hub->xcopy.default_retries == 3);
    assert(hub->recovery.level == UFT_RECOVERY_AGGRESSIVE);
    assert(hub->recovery.enable_crc_correction == true);
    assert(hub->recovery.detect_weak_bits == true);
    assert(hub->pll.mode == UFT_PLL_KALMAN);
    printf("✓\n");
    
    /* Test XCopy mapping */
    printf("Testing XCopy mapping... ");
    uft_xcopy_interface_t xcopy = {0};
    params.mode.output_mode = UFT_MODE_RAW_FLUX;
    uft_params_to_xcopy(&params, &xcopy);
    assert(xcopy.copy_mode == UFT_COPY_MODE_FLUX);
    printf("✓\n");
    
    /* Test PLL mapping */
    printf("Testing PLL mapping... ");
    uft_pll_interface_t pll = {0};
    params.timing.pll_mode = UFT_PLL_WD1772;
    params.timing.bitcell_time_ns = 4000;
    uft_params_to_pll(&params, &pll);
    assert(pll.mode == UFT_PLL_WD1772);
    assert(pll.initial_bitcell_ns == 4000.0f);
    printf("✓\n");
    
    /* Test score mapping */
    printf("Testing score mapping... ");
    uft_recovery_interface_t recovery = {0};
    recovery.sectors_read = 90;
    recovery.sectors_recovered = 5;
    recovery.sectors_failed = 10;
    recovery.bits_corrected = 15;
    recovery.recovery_rate = 0.9f;
    
    uft_score_t score = {0};
    uft_recovery_to_score(&recovery, &score);
    assert(score.recovered == true);
    assert(score.bit_errors_corrected == 15);
    printf("✓\n");
    
    /* Print mapping summary */
    printf("\nMapping summary:\n");
    hub->verbose = true;
    uft_hub_sync_all(hub, &params);
    uft_hub_print_mapping(hub, stdout);
    
    uft_hub_destroy(hub);
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 5, Failed: 0\n");
    return 0;
}

#endif /* INTEGRATION_TEST */

/**
 * @file uft_parser_v3_integration.h
 * @brief GOD MODE Parser v3 Integration Hub
 * 
 * Verbindet Parser v3 mit allen anderen Modulen:
 * - XCopy (Disk-to-Disk Kopieren)
 * - Recovery (Datenrettung)
 * - Forensic (Analyse & Reports)
 * - Nibble/GCR (Low-Level Decoding)
 * - PLL (Clock Recovery)
 * - Flux (Raw Flux Processing)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#ifndef UFT_PARSER_V3_INTEGRATION_H
#define UFT_PARSER_V3_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * FORWARD DECLARATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Parser v3 Types */
typedef struct uft_parser_v3 uft_parser_v3_t;
typedef struct uft_disk_v3 uft_disk_v3_t;
typedef struct uft_track_v3 uft_track_v3_t;
typedef struct uft_sector_v3 uft_sector_v3_t;
typedef struct uft_params_v3 uft_params_v3_t;
typedef struct uft_score uft_score_t;
typedef struct uft_diagnosis_list uft_diagnosis_list_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MODULE INTERFACES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief XCopy Module Interface
 * Disk-to-Disk und Image-to-Disk Operationen
 */
typedef struct {
    /* Profile Settings (von XCopy übernommen) */
    int copy_mode;              /* NORMAL, RAW, FLUX, NIBBLE, FORENSIC */
    int verify_mode;            /* NONE, COMPARE, HASH */
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
    
    /* Callbacks */
    void (*on_track_start)(uint8_t track, uint8_t side, void* ctx);
    void (*on_track_complete)(uint8_t track, uint8_t side, int status, void* ctx);
    void (*on_sector_read)(uint8_t track, uint8_t side, uint8_t sector, 
                           bool ok, void* ctx);
    void (*on_error)(uint8_t track, uint8_t side, uint8_t sector,
                     int error_code, const char* message, void* ctx);
    void* callback_context;
    
} uft_xcopy_interface_t;

/**
 * @brief Recovery Module Interface
 * Datenrettung und Fehlerkorrektur
 */
typedef struct {
    /* Recovery Level */
    enum {
        UFT_RECOVERY_NONE = 0,      /* Keine Rettung, nur lesen */
        UFT_RECOVERY_BASIC,          /* CRC-Korrektur, Multi-Rev */
        UFT_RECOVERY_AGGRESSIVE,     /* Alles versuchen */
        UFT_RECOVERY_FORENSIC        /* Preserve + Analyse */
    } level;
    
    /* CRC Correction */
    bool enable_crc_correction;
    uint8_t max_crc_bits;           /* Max Bits to flip */
    
    /* Multi-Rev Settings */
    bool enable_multi_rev;
    uint8_t min_revolutions;
    uint8_t max_revolutions;
    int merge_strategy;             /* VOTING, BEST_CRC, WEIGHTED */
    
    /* Weak Bit Handling */
    bool detect_weak_bits;
    uint8_t weak_bit_threshold;
    bool preserve_weak_bits;
    
    /* Gap/Sync Recovery */
    bool enable_sync_recovery;
    uint16_t sync_search_window;
    bool tolerant_sync;
    
    /* Timing Recovery */
    bool enable_timing_recovery;
    int pll_mode;                   /* SMOOTH, AGGRESSIVE, KALMAN */
    float pll_bandwidth;
    
    /* Sector Reconstruction */
    bool enable_reconstruction;
    bool use_interleave_hints;
    bool use_checksum_validation;
    
    /* Statistics Output */
    uint32_t sectors_read;
    uint32_t sectors_recovered;
    uint32_t sectors_failed;
    uint32_t bits_corrected;
    float recovery_rate;
    
} uft_recovery_interface_t;

/**
 * @brief Forensic Module Interface
 * Disk-Analyse und Report-Generierung
 */
typedef struct {
    /* Analysis Options */
    bool analyze_structure;         /* Filesystem structure */
    bool analyze_protection;        /* Copy protection */
    bool analyze_timing;            /* Timing anomalies */
    bool analyze_weak_bits;         /* Weak/fuzzy bits */
    bool analyze_errors;            /* Error patterns */
    bool analyze_interleave;        /* Sector interleave */
    bool analyze_gaps;              /* Inter-sector gaps */
    
    /* Report Options */
    bool generate_text_report;
    bool generate_html_report;
    bool generate_json_report;
    char report_path[256];
    
    /* Hash Options */
    bool compute_md5;
    bool compute_sha1;
    bool compute_sha256;
    bool compute_crc32;
    
    /* Protection Detection */
    char detected_protection[64];
    float protection_confidence;
    
    /* Audit Trail */
    bool enable_audit;
    char audit_log_path[256];
    
    /* Statistics */
    uint32_t total_tracks;
    uint32_t good_tracks;
    uint32_t bad_tracks;
    uint32_t protected_tracks;
    float overall_quality;
    
} uft_forensic_interface_t;

/**
 * @brief Nibble/GCR Module Interface
 * Low-Level Bit/Nibble Verarbeitung
 */
typedef struct {
    /* Encoding Type */
    enum {
        UFT_ENCODING_MFM = 0,
        UFT_ENCODING_FM,
        UFT_ENCODING_GCR_CBM,       /* Commodore GCR */
        UFT_ENCODING_GCR_APPLE,     /* Apple 6-and-2 */
        UFT_ENCODING_GCR_APPLE53,   /* Apple 5-and-3 */
        UFT_ENCODING_AMIGA_MFM,     /* Amiga-specific MFM */
        UFT_ENCODING_RAW            /* Raw bits, no encoding */
    } encoding;
    
    /* Decode Tables (provided by module) */
    const uint8_t* gcr_decode_table;
    const uint8_t* gcr_encode_table;
    size_t gcr_table_size;
    
    /* Sync Patterns */
    uint8_t sync_pattern[8];
    uint8_t sync_pattern_len;
    uint8_t sync_min_bits;
    
    /* Address Mark */
    uint8_t address_mark[4];
    uint8_t address_mark_len;
    
    /* Data Mark */
    uint8_t data_mark[4];
    uint8_t data_mark_len;
    
    /* Sector Layout */
    uint16_t id_field_size;
    uint16_t gap1_size;
    uint16_t gap2_size;
    uint16_t gap3_size;
    uint16_t data_field_size;
    
    /* Callbacks for decode */
    bool (*decode_id)(const uint8_t* raw, size_t len, 
                      uint8_t* track, uint8_t* side, uint8_t* sector,
                      uint8_t* size_code, uint16_t* crc);
    bool (*decode_data)(const uint8_t* raw, size_t len,
                        uint8_t* data, size_t data_size,
                        uint16_t* crc);
    bool (*encode_id)(uint8_t track, uint8_t side, uint8_t sector,
                      uint8_t size_code, uint8_t* raw, size_t* len);
    bool (*encode_data)(const uint8_t* data, size_t data_size,
                        uint8_t* raw, size_t* len);
    
} uft_nibble_interface_t;

/**
 * @brief PLL Module Interface
 * Clock Recovery und Timing
 */
typedef struct {
    /* PLL Mode */
    enum {
        UFT_PLL_FIXED = 0,          /* Fixed bitcell time */
        UFT_PLL_SIMPLE,             /* Simple tracking */
        UFT_PLL_ADAPTIVE,           /* Adaptive bandwidth */
        UFT_PLL_KALMAN,             /* Kalman filter */
        UFT_PLL_WD1772              /* WD1772 emulation */
    } mode;
    
    /* Parameters */
    float initial_bitcell_ns;       /* Starting bitcell time */
    float bandwidth;                /* PLL bandwidth (0.01-1.0) */
    float gain;                     /* PLL gain */
    float damping;                  /* Damping factor */
    uint8_t lock_threshold;         /* Bits for lock */
    float tolerance;                /* Timing tolerance */
    
    /* Kalman-specific */
    float process_noise;
    float measurement_noise;
    
    /* State (managed by PLL module) */
    float current_bitcell;
    float phase_error;
    bool locked;
    uint32_t bits_processed;
    uint32_t clock_errors;
    
    /* Callbacks */
    void (*on_bit)(uint8_t bit, float timing, void* ctx);
    void (*on_sync)(uint32_t position, void* ctx);
    void (*on_lock_change)(bool locked, void* ctx);
    void* callback_context;
    
    /* Process function */
    int (*process_flux)(const uint32_t* flux, size_t count,
                        uint8_t* bits, size_t* bit_count);
    
} uft_pll_interface_t;

/**
 * @brief Flux Module Interface
 * Raw Flux Processing
 */
typedef struct {
    /* Flux Data */
    uint32_t* transitions;          /* Flux timing array */
    size_t transition_count;
    uint32_t sample_rate;           /* ns per sample */
    uint32_t index_time;            /* Time at index pulse */
    
    /* Revolution Data */
    struct {
        size_t start_index;
        size_t end_index;
        uint32_t duration;
    } revolutions[32];
    uint8_t revolution_count;
    
    /* Statistics */
    uint32_t min_flux;
    uint32_t max_flux;
    double mean_flux;
    double stddev_flux;
    uint32_t short_count;           /* Below threshold */
    uint32_t long_count;            /* Above threshold */
    
    /* Processing Options */
    bool filter_glitches;
    uint32_t glitch_threshold;
    bool normalize_timing;
    bool detect_index;
    
    /* Callbacks */
    void (*on_revolution)(uint8_t rev, uint32_t duration, void* ctx);
    void (*on_anomaly)(uint32_t position, int type, void* ctx);
    void* callback_context;
    
} uft_flux_interface_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * INTEGRATION HUB
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Integration Hub - verbindet alle Module
 */
typedef struct {
    /* Active Parser */
    uft_parser_v3_t* parser;
    
    /* Module Interfaces */
    uft_xcopy_interface_t xcopy;
    uft_recovery_interface_t recovery;
    uft_forensic_interface_t forensic;
    uft_nibble_interface_t nibble;
    uft_pll_interface_t pll;
    uft_flux_interface_t flux;
    
    /* Current Disk */
    uft_disk_v3_t* disk;
    
    /* Global Settings */
    bool verbose;
    bool dry_run;
    FILE* log_file;
    
    /* Statistics */
    uint32_t operations_count;
    uint32_t errors_count;
    double total_time_ms;
    
} uft_integration_hub_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * INTEGRATION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create integration hub
 */
uft_integration_hub_t* uft_hub_create(void);

/**
 * @brief Destroy integration hub
 */
void uft_hub_destroy(uft_integration_hub_t* hub);

/**
 * @brief Set active parser
 */
bool uft_hub_set_parser(uft_integration_hub_t* hub, uft_parser_v3_t* parser);

/**
 * @brief Configure XCopy from Parser params
 */
void uft_hub_sync_xcopy_from_params(uft_integration_hub_t* hub,
                                     const uft_params_v3_t* params);

/**
 * @brief Configure Recovery from Parser params
 */
void uft_hub_sync_recovery_from_params(uft_integration_hub_t* hub,
                                        const uft_params_v3_t* params);

/**
 * @brief Configure PLL from Parser params
 */
void uft_hub_sync_pll_from_params(uft_integration_hub_t* hub,
                                   const uft_params_v3_t* params);

/**
 * @brief Full sync: Parser params → all modules
 */
void uft_hub_sync_all(uft_integration_hub_t* hub,
                       const uft_params_v3_t* params);

/* ═══════════════════════════════════════════════════════════════════════════
 * HIGH-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read disk using all modules
 * 
 * Pipeline: Flux → PLL → Nibble → Parser → Recovery → Forensic
 */
typedef struct {
    bool success;
    uft_disk_v3_t* disk;
    uft_diagnosis_list_t* diagnosis;
    char forensic_report[4096];
    float quality_score;
    
    /* Statistics */
    uint32_t tracks_read;
    uint32_t tracks_recovered;
    uint32_t tracks_failed;
    uint32_t sectors_total;
    uint32_t sectors_good;
    uint32_t sectors_recovered;
    uint32_t sectors_bad;
    
} uft_read_result_t;

uft_read_result_t uft_hub_read_disk(
    uft_integration_hub_t* hub,
    const char* source,             /* File or device */
    const uft_params_v3_t* params
);

/**
 * @brief Write disk using all modules
 * 
 * Pipeline: Parser → Nibble → PLL → Flux → Verify
 */
typedef struct {
    bool success;
    bool verified;
    uft_diagnosis_list_t* diagnosis;
    
    uint32_t tracks_written;
    uint32_t tracks_verified;
    uint32_t tracks_failed;
    uint8_t rewrite_count;
    
} uft_write_result_t;

uft_write_result_t uft_hub_write_disk(
    uft_integration_hub_t* hub,
    const uft_disk_v3_t* disk,
    const char* destination,        /* File or device */
    const uft_params_v3_t* params
);

/**
 * @brief Copy disk using XCopy pipeline
 * 
 * Pipeline: Read → Transform → Write → Verify
 */
typedef struct {
    bool success;
    uft_read_result_t read_result;
    uft_write_result_t write_result;
    
    uint32_t tracks_copied;
    uint32_t sectors_copied;
    float copy_quality;
    
} uft_copy_result_t;

uft_copy_result_t uft_hub_copy_disk(
    uft_integration_hub_t* hub,
    const char* source,
    const char* destination,
    const uft_params_v3_t* params
);

/**
 * @brief Analyze disk (forensic mode)
 */
typedef struct {
    bool success;
    uft_disk_v3_t* disk;
    uft_diagnosis_list_t* diagnosis;
    
    char* text_report;
    char* html_report;
    char* json_report;
    
    char protection_name[64];
    float protection_confidence;
    
    char hashes[4][65];             /* MD5, SHA1, SHA256, CRC32 */
    
} uft_analyze_result_t;

uft_analyze_result_t uft_hub_analyze_disk(
    uft_integration_hub_t* hub,
    const char* source,
    const uft_params_v3_t* params
);

/**
 * @brief Recover disk (aggressive recovery)
 */
typedef struct {
    bool success;
    uft_disk_v3_t* original;
    uft_disk_v3_t* recovered;
    uft_diagnosis_list_t* changes;
    
    uint32_t sectors_recovered;
    uint32_t bits_corrected;
    float recovery_rate;
    
} uft_recover_result_t;

uft_recover_result_t uft_hub_recover_disk(
    uft_integration_hub_t* hub,
    const char* source,
    const char* destination,
    const uft_params_v3_t* params
);

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read single track with full pipeline
 */
uft_track_v3_t* uft_hub_read_track(
    uft_integration_hub_t* hub,
    const uint8_t* raw_data,        /* Raw track/flux data */
    size_t raw_size,
    uint8_t track,
    uint8_t side,
    const uft_params_v3_t* params
);

/**
 * @brief Write single track with full pipeline
 */
bool uft_hub_write_track(
    uft_integration_hub_t* hub,
    const uft_track_v3_t* track,
    uint8_t** raw_data,
    size_t* raw_size,
    const uft_params_v3_t* params
);

/**
 * @brief Diagnose single track
 */
void uft_hub_diagnose_track(
    uft_integration_hub_t* hub,
    uft_track_v3_t* track,
    uft_diagnosis_list_t* diagnosis
);

/**
 * @brief Recover single track
 */
bool uft_hub_recover_track(
    uft_integration_hub_t* hub,
    uft_track_v3_t* track,
    const uft_params_v3_t* params
);

/* ═══════════════════════════════════════════════════════════════════════════
 * PARAMETER MAPPING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Map Parser v3 params to XCopy profile
 */
void uft_params_to_xcopy(const uft_params_v3_t* params, 
                          uft_xcopy_interface_t* xcopy);

/**
 * @brief Map Parser v3 params to Recovery settings
 */
void uft_params_to_recovery(const uft_params_v3_t* params,
                             uft_recovery_interface_t* recovery);

/**
 * @brief Map Parser v3 params to PLL settings
 */
void uft_params_to_pll(const uft_params_v3_t* params,
                        uft_pll_interface_t* pll);

/**
 * @brief Map Parser v3 params to Forensic settings
 */
void uft_params_to_forensic(const uft_params_v3_t* params,
                             uft_forensic_interface_t* forensic);

/**
 * @brief Map XCopy results back to Parser diagnosis
 */
void uft_xcopy_to_diagnosis(const uft_xcopy_interface_t* xcopy,
                             uft_diagnosis_list_t* diagnosis);

/**
 * @brief Map Recovery results back to Parser score
 */
void uft_recovery_to_score(const uft_recovery_interface_t* recovery,
                            uft_score_t* score);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARSER_V3_INTEGRATION_H */

/**
 * @file uft_recovery_params.h
 * @brief Recovery Parameter Management with Presets
 * 
 * Part of UFT God Mode - Unified parameter system for data recovery
 */

#ifndef UFT_RECOVERY_PARAMS_H
#define UFT_RECOVERY_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_recovery.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Parameter Version & Flags
 * ============================================================================ */

#define UFT_RECOVERY_PARAMS_VERSION  1

/** Recovery strategy flags */
typedef enum {
    UFT_REC_FLAG_NONE           = 0x0000,
    UFT_REC_FLAG_CRC_SINGLE     = 0x0001,   /**< Try single-bit CRC fix */
    UFT_REC_FLAG_CRC_DOUBLE     = 0x0002,   /**< Try double-bit CRC fix */
    UFT_REC_FLAG_MULTI_READ     = 0x0004,   /**< Use multiple reads */
    UFT_REC_FLAG_MAJORITY_VOTE  = 0x0008,   /**< Majority voting */
    UFT_REC_FLAG_WEAK_BIT       = 0x0010,   /**< Weak bit detection */
    UFT_REC_FLAG_TRACK_ALIGN    = 0x0020,   /**< Track alignment correction */
    UFT_REC_FLAG_PLL_RESYNC     = 0x0040,   /**< PLL re-synchronization */
    UFT_REC_FLAG_SECTOR_SPLICE  = 0x0080,   /**< Splice sectors from reads */
    UFT_REC_FLAG_FORENSIC_LOG   = 0x0100,   /**< Detailed forensic logging */
    UFT_REC_FLAG_PRESERVE_ORIG  = 0x0200,   /**< Keep original on failure */
} uft_recovery_flags_t;

/* ============================================================================
 * Recovery Parameters
 * ============================================================================ */

/**
 * @brief Complete recovery parameter set
 */
typedef struct {
    /* Version for compatibility */
    uint32_t version;
    uint32_t flags;
    
    /* Retry strategy */
    int32_t max_retries_per_sector;     /**< Max retries per sector (1-20) */
    int32_t max_retries_per_track;      /**< Max retries per track (1-10) */
    int32_t retry_delay_ms;             /**< Delay between retries (0-1000) */
    
    /* Multi-read settings */
    int32_t min_reads_for_vote;         /**< Min reads for majority (3-10) */
    int32_t max_reads_for_vote;         /**< Max reads for majority (3-20) */
    float   vote_threshold;             /**< Confidence threshold (0.5-1.0) */
    
    /* CRC correction */
    bool    enable_crc_single_fix;      /**< Allow single-bit CRC fix */
    bool    enable_crc_double_fix;      /**< Allow double-bit CRC fix */
    int32_t crc_fix_max_bytes;          /**< Max bytes to scan for CRC fix */
    
    /* Weak bit handling */
    float   weak_bit_threshold;         /**< Timing variance threshold */
    int32_t weak_bit_min_variance;      /**< Min reads showing variance */
    bool    stabilize_weak_bits;        /**< Attempt to stabilize weak bits */
    
    /* Track alignment */
    float   alignment_tolerance;        /**< Alignment tolerance (0.01-0.1) */
    bool    auto_align_tracks;          /**< Auto-adjust track alignment */
    
    /* PLL recovery */
    bool    pll_resync_on_error;        /**< Re-sync PLL on errors */
    int32_t pll_resync_bits;            /**< Bits to re-sync (16-128) */
    
    /* Sector reconstruction */
    bool    enable_sector_splice;       /**< Splice best parts from reads */
    bool    enable_header_recovery;     /**< Recover damaged headers */
    
    /* Output control */
    bool    mark_recovered_sectors;     /**< Flag recovered sectors */
    bool    generate_recovery_log;      /**< Create detailed log */
    bool    preserve_original_on_fail;  /**< Keep original if recovery fails */
    
    /* Metadata */
    char name[32];
    char description[128];
    
    /* Validation */
    bool validated;
    char error_msg[256];
    
} uft_recovery_params_t;

/* ============================================================================
 * Preset IDs
 * ============================================================================ */

typedef enum {
    UFT_REC_PRESET_DEFAULT = 0,
    UFT_REC_PRESET_QUICK,           /**< Fast, minimal recovery */
    UFT_REC_PRESET_STANDARD,        /**< Balanced recovery */
    UFT_REC_PRESET_THOROUGH,        /**< Comprehensive recovery */
    UFT_REC_PRESET_FORENSIC,        /**< Maximum recovery, full logging */
    UFT_REC_PRESET_WEAK_BIT,        /**< Focus on weak bit recovery */
    UFT_REC_PRESET_CRC_FOCUS,       /**< Focus on CRC correction */
    UFT_REC_PRESET_COUNT
} uft_recovery_preset_id_t;

/* ============================================================================
 * Preset Definitions
 * ============================================================================ */

/** Default balanced preset */
#define UFT_RECOVERY_PARAMS_DEFAULT { \
    .version = UFT_RECOVERY_PARAMS_VERSION, \
    .flags = UFT_REC_FLAG_CRC_SINGLE | UFT_REC_FLAG_MULTI_READ | \
             UFT_REC_FLAG_MAJORITY_VOTE, \
    .max_retries_per_sector = 5, \
    .max_retries_per_track = 3, \
    .retry_delay_ms = 0, \
    .min_reads_for_vote = 3, \
    .max_reads_for_vote = 5, \
    .vote_threshold = 0.6f, \
    .enable_crc_single_fix = true, \
    .enable_crc_double_fix = false, \
    .crc_fix_max_bytes = 512, \
    .weak_bit_threshold = 0.15f, \
    .weak_bit_min_variance = 2, \
    .stabilize_weak_bits = false, \
    .alignment_tolerance = 0.05f, \
    .auto_align_tracks = false, \
    .pll_resync_on_error = true, \
    .pll_resync_bits = 32, \
    .enable_sector_splice = false, \
    .enable_header_recovery = false, \
    .mark_recovered_sectors = true, \
    .generate_recovery_log = false, \
    .preserve_original_on_fail = true, \
    .name = "Default", \
    .description = "Balanced recovery settings", \
    .validated = true, \
    .error_msg = "" \
}

/** Quick recovery - minimal attempts */
#define UFT_RECOVERY_PARAMS_QUICK { \
    .version = UFT_RECOVERY_PARAMS_VERSION, \
    .flags = UFT_REC_FLAG_CRC_SINGLE, \
    .max_retries_per_sector = 2, \
    .max_retries_per_track = 1, \
    .retry_delay_ms = 0, \
    .min_reads_for_vote = 2, \
    .max_reads_for_vote = 3, \
    .vote_threshold = 0.7f, \
    .enable_crc_single_fix = true, \
    .enable_crc_double_fix = false, \
    .crc_fix_max_bytes = 256, \
    .weak_bit_threshold = 0.10f, \
    .weak_bit_min_variance = 2, \
    .stabilize_weak_bits = false, \
    .alignment_tolerance = 0.03f, \
    .auto_align_tracks = false, \
    .pll_resync_on_error = false, \
    .pll_resync_bits = 16, \
    .enable_sector_splice = false, \
    .enable_header_recovery = false, \
    .mark_recovered_sectors = false, \
    .generate_recovery_log = false, \
    .preserve_original_on_fail = true, \
    .name = "Quick", \
    .description = "Fast recovery with minimal attempts", \
    .validated = true, \
    .error_msg = "" \
}

/** Forensic recovery - maximum effort */
#define UFT_RECOVERY_PARAMS_FORENSIC { \
    .version = UFT_RECOVERY_PARAMS_VERSION, \
    .flags = UFT_REC_FLAG_CRC_SINGLE | UFT_REC_FLAG_CRC_DOUBLE | \
             UFT_REC_FLAG_MULTI_READ | UFT_REC_FLAG_MAJORITY_VOTE | \
             UFT_REC_FLAG_WEAK_BIT | UFT_REC_FLAG_TRACK_ALIGN | \
             UFT_REC_FLAG_PLL_RESYNC | UFT_REC_FLAG_SECTOR_SPLICE | \
             UFT_REC_FLAG_FORENSIC_LOG | UFT_REC_FLAG_PRESERVE_ORIG, \
    .max_retries_per_sector = 20, \
    .max_retries_per_track = 10, \
    .retry_delay_ms = 100, \
    .min_reads_for_vote = 5, \
    .max_reads_for_vote = 15, \
    .vote_threshold = 0.5f, \
    .enable_crc_single_fix = true, \
    .enable_crc_double_fix = true, \
    .crc_fix_max_bytes = 1024, \
    .weak_bit_threshold = 0.20f, \
    .weak_bit_min_variance = 3, \
    .stabilize_weak_bits = true, \
    .alignment_tolerance = 0.08f, \
    .auto_align_tracks = true, \
    .pll_resync_on_error = true, \
    .pll_resync_bits = 64, \
    .enable_sector_splice = true, \
    .enable_header_recovery = true, \
    .mark_recovered_sectors = true, \
    .generate_recovery_log = true, \
    .preserve_original_on_fail = true, \
    .name = "Forensic", \
    .description = "Maximum recovery effort with full logging", \
    .validated = true, \
    .error_msg = "" \
}

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * @brief Get default parameters
 */
uft_recovery_params_t uft_recovery_params_default(void);

/**
 * @brief Get preset by ID
 */
uft_recovery_params_t uft_recovery_params_preset(uft_recovery_preset_id_t preset);

/**
 * @brief Get preset name
 */
const char* uft_recovery_preset_name(uft_recovery_preset_id_t preset);

/**
 * @brief Validate parameters
 * @return true if valid, false if errors (check error_msg)
 */
bool uft_recovery_params_validate(uft_recovery_params_t* params);

/**
 * @brief Convert parameters to JSON string
 * @return Allocated string (caller must free) or NULL on error
 */
char* uft_recovery_params_to_json(const uft_recovery_params_t* params);

/**
 * @brief Parse parameters from JSON
 */
int uft_recovery_params_from_json(const char* json, uft_recovery_params_t* params);

/**
 * @brief Copy parameters
 */
void uft_recovery_params_copy(uft_recovery_params_t* dst, 
                               const uft_recovery_params_t* src);

/**
 * @brief Apply recovery params to stats tracking
 */
void uft_recovery_stats_init(uft_recovery_stats_t* stats);

/**
 * @brief Generate recovery report
 */
char* uft_recovery_generate_report(const uft_recovery_stats_t* stats,
                                    const uft_recovery_params_t* params);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_PARAMS_H */

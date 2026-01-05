/**
 * @file uft_pll_params.h
 * @brief PLL Parameter Management with Presets and JSON Support
 * 
 * Part of UFT God Mode - Unified parameter system for PLL tuning
 */

#ifndef UFT_PLL_PARAMS_H
#define UFT_PLL_PARAMS_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_pll_pi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Parameter Version & Flags
 * ============================================================================ */

#define UFT_PLL_PARAMS_VERSION  1

/** Parameter flags */
typedef enum {
    UFT_PLL_FLAG_NONE           = 0x0000,
    UFT_PLL_FLAG_ADAPTIVE       = 0x0001,   /**< Auto-adjust during decode */
    UFT_PLL_FLAG_AGGRESSIVE     = 0x0002,   /**< Fast lock, may overshoot */
    UFT_PLL_FLAG_CONSERVATIVE   = 0x0004,   /**< Slow lock, more stable */
    UFT_PLL_FLAG_MULTI_REV      = 0x0008,   /**< Use multi-revolution data */
    UFT_PLL_FLAG_WEAK_BIT_AWARE = 0x0010,   /**< Handle weak bit regions */
    UFT_PLL_FLAG_JITTER_FILTER  = 0x0020,   /**< Extra jitter filtering */
} uft_pll_flags_t;

/* ============================================================================
 * Extended Parameter Structure
 * ============================================================================ */

/**
 * @brief Complete PLL parameter set with metadata
 */
typedef struct {
    /* Version for future compatibility */
    uint32_t version;
    uint32_t flags;
    
    /* Core PI parameters */
    double kp;                  /**< Proportional gain (0.1 - 1.0) */
    double ki;                  /**< Integral gain (0.0001 - 0.01) */
    double kd;                  /**< Derivative gain (optional, usually 0) */
    
    /* Sync parameters */
    double sync_tolerance;      /**< Initial sync window (0.15 - 0.50) */
    double lock_tolerance;      /**< Locked tracking window (0.05 - 0.25) */
    double unlock_threshold;    /**< Error to lose lock (0.3 - 0.6) */
    int32_t sync_bits_required; /**< Bits needed to declare sync (8 - 64) */
    
    /* Timing parameters */
    double cell_adjust_rate;    /**< Max cell adjustment per bit (0.01 - 0.1) */
    double rpm_tolerance;       /**< RPM variation tolerance (0.01 - 0.05) */
    
    /* Encoding specific */
    uft_encoding_t encoding;
    uint32_t data_rate;
    uint32_t sample_rate;       /**< Flux sample rate (Hz) */
    
    /* Weak bit handling */
    double weak_bit_threshold;  /**< Timing variance for weak detection */
    int32_t weak_bit_min_count; /**< Min samples for weak confirmation */
    
    /* Metadata */
    char name[32];              /**< Preset name */
    char description[128];      /**< Description */
    
    /* Validation */
    bool validated;
    char error_msg[256];
    
} uft_pll_params_t;

/* ============================================================================
 * Preset IDs
 * ============================================================================ */

typedef enum {
    UFT_PLL_PRESET_DEFAULT = 0,
    
    /* By use case */
    UFT_PLL_PRESET_CLEAN_DISK,      /**< Good quality disk */
    UFT_PLL_PRESET_DIRTY_DISK,      /**< Marginal/damaged disk */
    UFT_PLL_PRESET_COPY_PROTECTED,  /**< Known protection */
    UFT_PLL_PRESET_FORENSIC,        /**< Maximum recovery */
    
    /* By platform */
    UFT_PLL_PRESET_IBM_PC_DD,
    UFT_PLL_PRESET_IBM_PC_HD,
    UFT_PLL_PRESET_AMIGA_DD,
    UFT_PLL_PRESET_AMIGA_HD,
    UFT_PLL_PRESET_ATARI_ST,
    UFT_PLL_PRESET_C64,
    UFT_PLL_PRESET_APPLE_II,
    UFT_PLL_PRESET_MAC_GCR,
    
    /* By hardware */
    UFT_PLL_PRESET_GREASEWEAZLE,
    UFT_PLL_PRESET_KRYOFLUX,
    UFT_PLL_PRESET_FLUXENGINE,
    UFT_PLL_PRESET_SCP,
    
    UFT_PLL_PRESET_COUNT
} uft_pll_preset_id_t;

/* ============================================================================
 * Preset Definitions
 * ============================================================================ */

/** Default balanced preset */
#define UFT_PLL_PARAMS_DEFAULT { \
    .version = UFT_PLL_PARAMS_VERSION, \
    .flags = UFT_PLL_FLAG_ADAPTIVE, \
    .kp = 0.5, \
    .ki = 0.0005, \
    .kd = 0.0, \
    .sync_tolerance = 0.25, \
    .lock_tolerance = 0.10, \
    .unlock_threshold = 0.40, \
    .sync_bits_required = 16, \
    .cell_adjust_rate = 0.05, \
    .rpm_tolerance = 0.03, \
    .encoding = UFT_ENC_MFM, \
    .data_rate = 250000, \
    .sample_rate = 24000000, \
    .weak_bit_threshold = 0.15, \
    .weak_bit_min_count = 3, \
    .name = "Default", \
    .description = "Balanced settings for most disks", \
    .validated = true, \
    .error_msg = "" \
}

/** Aggressive preset for clean disks */
#define UFT_PLL_PARAMS_AGGRESSIVE { \
    .version = UFT_PLL_PARAMS_VERSION, \
    .flags = UFT_PLL_FLAG_AGGRESSIVE, \
    .kp = 0.7, \
    .ki = 0.001, \
    .kd = 0.0, \
    .sync_tolerance = 0.15, \
    .lock_tolerance = 0.08, \
    .unlock_threshold = 0.30, \
    .sync_bits_required = 8, \
    .cell_adjust_rate = 0.08, \
    .rpm_tolerance = 0.02, \
    .encoding = UFT_ENC_MFM, \
    .data_rate = 250000, \
    .sample_rate = 24000000, \
    .weak_bit_threshold = 0.10, \
    .weak_bit_min_count = 2, \
    .name = "Aggressive", \
    .description = "Fast lock for clean disks", \
    .validated = true, \
    .error_msg = "" \
}

/** Conservative preset for damaged disks */
#define UFT_PLL_PARAMS_CONSERVATIVE { \
    .version = UFT_PLL_PARAMS_VERSION, \
    .flags = UFT_PLL_FLAG_CONSERVATIVE | UFT_PLL_FLAG_JITTER_FILTER, \
    .kp = 0.3, \
    .ki = 0.0003, \
    .kd = 0.0, \
    .sync_tolerance = 0.35, \
    .lock_tolerance = 0.15, \
    .unlock_threshold = 0.50, \
    .sync_bits_required = 32, \
    .cell_adjust_rate = 0.03, \
    .rpm_tolerance = 0.05, \
    .encoding = UFT_ENC_MFM, \
    .data_rate = 250000, \
    .sample_rate = 24000000, \
    .weak_bit_threshold = 0.20, \
    .weak_bit_min_count = 5, \
    .name = "Conservative", \
    .description = "Stable tracking for damaged disks", \
    .validated = true, \
    .error_msg = "" \
}

/** Forensic preset - maximum recovery */
#define UFT_PLL_PARAMS_FORENSIC { \
    .version = UFT_PLL_PARAMS_VERSION, \
    .flags = UFT_PLL_FLAG_ADAPTIVE | UFT_PLL_FLAG_MULTI_REV | \
             UFT_PLL_FLAG_WEAK_BIT_AWARE | UFT_PLL_FLAG_JITTER_FILTER, \
    .kp = 0.4, \
    .ki = 0.0004, \
    .kd = 0.0, \
    .sync_tolerance = 0.33, \
    .lock_tolerance = 0.12, \
    .unlock_threshold = 0.45, \
    .sync_bits_required = 24, \
    .cell_adjust_rate = 0.04, \
    .rpm_tolerance = 0.04, \
    .encoding = UFT_ENC_MFM, \
    .data_rate = 250000, \
    .sample_rate = 24000000, \
    .weak_bit_threshold = 0.18, \
    .weak_bit_min_count = 4, \
    .name = "Forensic", \
    .description = "Maximum recovery with weak bit detection", \
    .validated = true, \
    .error_msg = "" \
}

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

/**
 * @brief Get default parameters
 */
uft_pll_params_t uft_pll_params_default(void);

/**
 * @brief Get preset by ID
 */
uft_pll_params_t uft_pll_params_preset(uft_pll_preset_id_t preset);

/**
 * @brief Get preset name
 */
const char* uft_pll_preset_name(uft_pll_preset_id_t preset);

/**
 * @brief Validate parameters
 * @return true if valid, false if errors (check error_msg)
 */
bool uft_pll_params_validate(uft_pll_params_t* params);

/**
 * @brief Convert parameters to JSON string
 * @return Allocated string (caller must free) or NULL on error
 */
char* uft_pll_params_to_json(const uft_pll_params_t* params);

/**
 * @brief Parse parameters from JSON
 * @return UFT_OK or error code
 */
int uft_pll_params_from_json(const char* json, uft_pll_params_t* params);

/**
 * @brief Copy parameters
 */
void uft_pll_params_copy(uft_pll_params_t* dst, const uft_pll_params_t* src);

/**
 * @brief Convert to core PLL config (for uft_pll_pi)
 */
uft_pll_config_t uft_pll_params_to_config(const uft_pll_params_t* params);

/**
 * @brief Create params from core config
 */
uft_pll_params_t uft_pll_params_from_config(const uft_pll_config_t* config);

/**
 * @brief Adjust params for specific platform
 */
void uft_pll_params_adjust_for_platform(uft_pll_params_t* params, 
                                         const char* platform);

/**
 * @brief Adjust params for specific hardware
 */
void uft_pll_params_adjust_for_hardware(uft_pll_params_t* params,
                                         const char* hardware);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLL_PARAMS_H */

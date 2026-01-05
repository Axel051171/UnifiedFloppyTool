/**
 * @file uft_protection_params.h
 * @brief Copy Protection Detection Parameters with Presets
 * 
 * Part of UFT God Mode - Unified parameter system for protection detection
 */

#ifndef UFT_PROTECTION_PARAMS_H
#define UFT_PROTECTION_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Protection Types
 * ============================================================================ */

typedef enum {
    UFT_PROT_NONE               = 0x0000,
    UFT_PROT_FUZZY_BITS         = 0x0001,   /**< Fuzzy/weak bits */
    UFT_PROT_LONG_TRACK         = 0x0002,   /**< Extended track length */
    UFT_PROT_SHORT_TRACK        = 0x0004,   /**< Shortened track */
    UFT_PROT_INVALID_CRC        = 0x0008,   /**< Intentional CRC errors */
    UFT_PROT_DUPLICATE_SECTOR   = 0x0010,   /**< Duplicate sector IDs */
    UFT_PROT_MISSING_SECTOR     = 0x0020,   /**< Missing sector data */
    UFT_PROT_HALF_TRACK         = 0x0040,   /**< Half-track data */
    UFT_PROT_DENSITY_VAR        = 0x0080,   /**< Density variations */
    UFT_PROT_NON_STANDARD_GAP   = 0x0100,   /**< Non-standard gaps */
    UFT_PROT_TIMING_BASED       = 0x0200,   /**< Timing-based protection */
    UFT_PROT_SECTOR_247         = 0x0400,   /**< Dungeon Master sector 247 */
    UFT_PROT_COPYLOCK           = 0x0800,   /**< Copylock protection */
    UFT_PROT_SPEEDLOCK          = 0x1000,   /**< Speedlock protection */
    UFT_PROT_CUSTOM             = 0x8000,   /**< Custom/unknown */
} uft_protection_type_t;

/* ============================================================================
 * Parameter Version & Flags
 * ============================================================================ */

#define UFT_PROTECTION_PARAMS_VERSION  1

typedef enum {
    UFT_PROT_FLAG_NONE          = 0x0000,
    UFT_PROT_FLAG_DETECT_ALL    = 0x0001,   /**< Detect all protection types */
    UFT_PROT_FLAG_STRICT        = 0x0002,   /**< Strict detection (fewer FP) */
    UFT_PROT_FLAG_LOOSE         = 0x0004,   /**< Loose detection (fewer FN) */
    UFT_PROT_FLAG_MULTI_REV     = 0x0008,   /**< Use multi-revolution data */
    UFT_PROT_FLAG_REPORT        = 0x0010,   /**< Generate detailed report */
    UFT_PROT_FLAG_PRESERVE      = 0x0020,   /**< Preserve protection in output */
} uft_protection_flags_t;

/* ============================================================================
 * Protection Parameters
 * ============================================================================ */

typedef struct {
    /* Version */
    uint32_t version;
    uint32_t flags;
    
    /* Detection enables */
    uint32_t detect_types;              /**< Bitmask of types to detect */
    
    /* Fuzzy bit detection */
    float fuzzy_timing_min_us;          /**< Min timing for fuzzy (4.3) */
    float fuzzy_timing_max_us;          /**< Max timing for fuzzy (5.7) */
    int32_t fuzzy_min_reads;            /**< Min reads for confirmation */
    float fuzzy_variance_threshold;     /**< Variance threshold */
    
    /* Track length detection */
    float long_track_threshold;         /**< % over nominal for long (1.02) */
    float short_track_threshold;        /**< % under nominal for short (0.98) */
    
    /* CRC detection */
    int32_t min_crc_errors_for_protect; /**< Min CRC errors for protection */
    bool ignore_random_crc_errors;      /**< Ignore isolated CRC errors */
    
    /* Duplicate sector detection */
    bool detect_duplicate_ids;          /**< Detect duplicate sector IDs */
    int32_t duplicate_read_variance;    /**< Expected variance in dups */
    
    /* Half-track detection */
    bool detect_half_tracks;            /**< Look for half-track data */
    float half_track_signal_threshold;  /**< Signal level threshold */
    
    /* Density detection */
    float density_variance_threshold;   /**< Density variation threshold */
    
    /* Timing-based */
    float timing_variance_threshold;    /**< Timing variance threshold */
    int32_t timing_sample_bits;         /**< Bits to sample for timing */
    
    /* Platform-specific presets */
    bool enable_atari_st_checks;        /**< Atari ST specific (fuzzy) */
    bool enable_amiga_checks;           /**< Amiga specific (long track) */
    bool enable_c64_checks;             /**< C64 specific (density) */
    bool enable_apple_checks;           /**< Apple specific (half track) */
    
    /* Output */
    int32_t confidence_threshold;       /**< Min confidence to report (0-100) */
    
    /* Metadata */
    char name[32];
    char description[128];
    bool validated;
    char error_msg[256];
    
} uft_protection_params_t;

/* ============================================================================
 * Detection Result
 * ============================================================================ */

typedef struct {
    /* What was found */
    uint32_t detected_types;            /**< Bitmask of detected types */
    int32_t confidence;                 /**< Overall confidence (0-100) */
    
    /* Details per type */
    struct {
        uft_protection_type_t type;
        int32_t confidence;             /**< Confidence for this type */
        int32_t track;                  /**< Track where found (-1 = multiple) */
        int32_t sector;                 /**< Sector where found (-1 = multiple) */
        int32_t count;                  /**< Number of instances */
        char description[128];          /**< Human-readable description */
    } details[16];
    int32_t detail_count;
    
    /* Summary */
    char summary[512];
    
} uft_protection_result_t;

/* ============================================================================
 * Preset IDs
 * ============================================================================ */

typedef enum {
    UFT_PROT_PRESET_DEFAULT = 0,
    UFT_PROT_PRESET_QUICK,              /**< Fast scan */
    UFT_PROT_PRESET_THOROUGH,           /**< Complete scan */
    UFT_PROT_PRESET_ATARI_ST,           /**< Atari ST focused */
    UFT_PROT_PRESET_AMIGA,              /**< Amiga focused */
    UFT_PROT_PRESET_C64,                /**< C64 focused */
    UFT_PROT_PRESET_APPLE,              /**< Apple focused */
    UFT_PROT_PRESET_COUNT
} uft_protection_preset_id_t;

/* ============================================================================
 * Preset Definitions
 * ============================================================================ */

#define UFT_PROTECTION_PARAMS_DEFAULT { \
    .version = UFT_PROTECTION_PARAMS_VERSION, \
    .flags = UFT_PROT_FLAG_DETECT_ALL, \
    .detect_types = 0xFFFF, \
    .fuzzy_timing_min_us = 4.3f, \
    .fuzzy_timing_max_us = 5.7f, \
    .fuzzy_min_reads = 3, \
    .fuzzy_variance_threshold = 0.15f, \
    .long_track_threshold = 1.02f, \
    .short_track_threshold = 0.98f, \
    .min_crc_errors_for_protect = 3, \
    .ignore_random_crc_errors = true, \
    .detect_duplicate_ids = true, \
    .duplicate_read_variance = 2, \
    .detect_half_tracks = false, \
    .half_track_signal_threshold = 0.3f, \
    .density_variance_threshold = 0.1f, \
    .timing_variance_threshold = 0.15f, \
    .timing_sample_bits = 1000, \
    .enable_atari_st_checks = true, \
    .enable_amiga_checks = true, \
    .enable_c64_checks = true, \
    .enable_apple_checks = true, \
    .confidence_threshold = 50, \
    .name = "Default", \
    .description = "Balanced protection detection", \
    .validated = true, \
    .error_msg = "" \
}

#define UFT_PROTECTION_PARAMS_ATARI_ST { \
    .version = UFT_PROTECTION_PARAMS_VERSION, \
    .flags = UFT_PROT_FLAG_DETECT_ALL | UFT_PROT_FLAG_MULTI_REV, \
    .detect_types = UFT_PROT_FUZZY_BITS | UFT_PROT_SECTOR_247 | \
                    UFT_PROT_COPYLOCK | UFT_PROT_SPEEDLOCK | \
                    UFT_PROT_LONG_TRACK, \
    .fuzzy_timing_min_us = 4.3f, \
    .fuzzy_timing_max_us = 5.7f, \
    .fuzzy_min_reads = 5, \
    .fuzzy_variance_threshold = 0.12f, \
    .long_track_threshold = 1.01f, \
    .short_track_threshold = 0.99f, \
    .min_crc_errors_for_protect = 2, \
    .ignore_random_crc_errors = false, \
    .detect_duplicate_ids = true, \
    .duplicate_read_variance = 3, \
    .detect_half_tracks = false, \
    .half_track_signal_threshold = 0.3f, \
    .density_variance_threshold = 0.1f, \
    .timing_variance_threshold = 0.12f, \
    .timing_sample_bits = 2000, \
    .enable_atari_st_checks = true, \
    .enable_amiga_checks = false, \
    .enable_c64_checks = false, \
    .enable_apple_checks = false, \
    .confidence_threshold = 40, \
    .name = "Atari ST", \
    .description = "Optimized for Atari ST protection schemes", \
    .validated = true, \
    .error_msg = "" \
}

/* ============================================================================
 * Function Prototypes
 * ============================================================================ */

uft_protection_params_t uft_protection_params_default(void);
uft_protection_params_t uft_protection_params_preset(uft_protection_preset_id_t preset);
const char* uft_protection_preset_name(uft_protection_preset_id_t preset);
bool uft_protection_params_validate(uft_protection_params_t* params);
char* uft_protection_params_to_json(const uft_protection_params_t* params);
void uft_protection_result_init(uft_protection_result_t* result);
const char* uft_protection_type_name(uft_protection_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROTECTION_PARAMS_H */

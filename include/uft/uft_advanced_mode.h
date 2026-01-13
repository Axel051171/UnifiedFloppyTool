/**
 * @file uft_advanced_mode.h
 * @brief UFT Advanced Mode - Automatic v3 Parser & God-Mode Integration
 * @version 3.8.0
 * 
 * "Bei uns geht kein Bit verloren" - UFT Preservation Philosophy
 * 
 * Advanced Mode provides:
 * - Automatic v3 parser selection for supported formats
 * - Automatic protection detection on open
 * - God-Mode algorithms for weak/damaged tracks
 * - Bayesian format detection with confidence scoring
 */

#ifndef UFT_ADVANCED_MODE_H
#define UFT_ADVANCED_MODE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADVANCED MODE CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Advanced mode feature flags
 */
typedef enum {
    UFT_ADV_NONE              = 0,
    UFT_ADV_USE_V3_PARSERS    = (1 << 0),  /**< Use v3 parsers when available */
    UFT_ADV_AUTO_PROTECTION   = (1 << 1),  /**< Auto-detect protection on open */
    UFT_ADV_GOD_MODE          = (1 << 2),  /**< Use God-Mode for weak tracks */
    UFT_ADV_BAYESIAN_DETECT   = (1 << 3),  /**< Use Bayesian format detection */
    UFT_ADV_MULTI_REV_FUSION  = (1 << 4),  /**< Auto-fuse multiple revolutions */
    UFT_ADV_CRC_CORRECTION    = (1 << 5),  /**< Attempt CRC error correction */
    UFT_ADV_KALMAN_PLL        = (1 << 6),  /**< Use Kalman PLL for timing */
    UFT_ADV_VITERBI_DECODE    = (1 << 7),  /**< Use Viterbi for GCR decode */
    
    /** All features enabled */
    UFT_ADV_ALL = 0xFF
} uft_advanced_flags_t;

/**
 * @brief Advanced mode configuration
 */
typedef struct {
    uint32_t flags;                 /**< Feature flags (uft_advanced_flags_t) */
    int quality_threshold;          /**< Track quality threshold for God-Mode (0-100) */
    int bayesian_min_confidence;    /**< Minimum Bayesian confidence (0-100) */
    int max_crc_corrections;        /**< Maximum CRC corrections per sector */
    bool verbose_logging;           /**< Enable verbose logging */
} uft_advanced_config_t;

/**
 * @brief Advanced mode disk handle
 */
typedef struct {
    void* handle;                   /**< Underlying format handle */
    void* v3_handle;                /**< v3 parser handle (if used) */
    int format_id;                  /**< Detected format */
    int detection_confidence;       /**< Detection confidence 0-100 */
    char protection_name[64];       /**< Detected protection (if any) */
    bool protection_detected;       /**< True if protection found */
    bool using_v3;                  /**< True if using v3 parser */
    bool god_mode_active;           /**< True if God-Mode engaged */
    int weak_track_count;           /**< Number of weak tracks */
    int recovered_sector_count;     /**< Sectors recovered by God-Mode */
} uft_advanced_handle_t;

/**
 * @brief Track quality info
 */
typedef struct {
    int cylinder;
    int head;
    double quality;                 /**< Quality 0.0-1.0 */
    bool is_weak;                   /**< Has weak bits */
    bool has_errors;                /**< Has CRC errors */
    int error_count;                /**< Number of errors */
    bool god_mode_used;             /**< God-Mode was applied */
    int recovered_bits;             /**< Bits recovered by God-Mode */
} uft_track_quality_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * GLOBAL CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize global advanced mode with default settings
 */
void uft_advanced_init(void);

/**
 * @brief Set global advanced mode configuration
 */
void uft_advanced_set_config(const uft_advanced_config_t* config);

/**
 * @brief Get current configuration
 */
const uft_advanced_config_t* uft_advanced_get_config(void);

/**
 * @brief Enable/disable advanced mode globally
 */
void uft_advanced_enable(bool enable);

/**
 * @brief Check if advanced mode is enabled
 */
bool uft_advanced_is_enabled(void);

/**
 * @brief Enable specific feature flag
 */
void uft_advanced_enable_feature(uft_advanced_flags_t flag);

/**
 * @brief Disable specific feature flag
 */
void uft_advanced_disable_feature(uft_advanced_flags_t flag);

/**
 * @brief Check if feature is enabled
 */
bool uft_advanced_has_feature(uft_advanced_flags_t flag);

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADVANCED OPEN/CLOSE
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open disk image with advanced mode
 * 
 * Automatically:
 * - Detects format (Bayesian if enabled)
 * - Uses v3 parser if available and enabled
 * - Detects protection if enabled
 * - Prepares God-Mode for weak tracks
 * 
 * @param path Path to disk image
 * @param handle Output handle
 * @return UFT_OK on success
 */
uft_error_t uft_advanced_open(const char* path, uft_advanced_handle_t** handle);

/**
 * @brief Close advanced handle
 */
void uft_advanced_close(uft_advanced_handle_t* handle);

/* ═══════════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS WITH GOD-MODE
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read track with automatic God-Mode for weak data
 * 
 * If track quality is below threshold:
 * - Applies Kalman PLL for timing recovery
 * - Uses Viterbi decoder for GCR/MFM
 * - Attempts CRC correction
 * - Fuses multiple revolutions if available
 */
uft_error_t uft_advanced_read_track(uft_advanced_handle_t* handle,
                                    int cylinder, int head,
                                    uint8_t* buffer, size_t* size,
                                    uft_track_quality_t* quality);

/**
 * @brief Read sector with automatic error recovery
 */
uft_error_t uft_advanced_read_sector(uft_advanced_handle_t* handle,
                                     int cylinder, int head, int sector,
                                     uint8_t* buffer, size_t* size);

/**
 * @brief Get track quality without reading full data
 */
uft_error_t uft_advanced_get_track_quality(uft_advanced_handle_t* handle,
                                           int cylinder, int head,
                                           uft_track_quality_t* quality);

/**
 * @brief Get summary of all track qualities
 */
uft_error_t uft_advanced_analyze_disk(uft_advanced_handle_t* handle,
                                      uft_track_quality_t* qualities,
                                      int* track_count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONVENIENCE FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Quick format detection with confidence
 */
int uft_advanced_detect_format(const char* path, int* confidence);

/**
 * @brief Quick protection detection
 */
bool uft_advanced_detect_protection(const char* path, char* name, size_t name_size);

/**
 * @brief Get statistics about God-Mode usage
 */
typedef struct {
    int total_tracks;
    int weak_tracks;
    int recovered_tracks;
    int total_sectors;
    int error_sectors;
    int recovered_sectors;
    int crc_corrections;
    double average_quality;
} uft_advanced_stats_t;

void uft_advanced_get_stats(uft_advanced_handle_t* handle, uft_advanced_stats_t* stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADVANCED_MODE_H */

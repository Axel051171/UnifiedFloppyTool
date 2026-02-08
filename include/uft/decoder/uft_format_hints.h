/**
 * @file uft_format_hints.h
 * @brief Format-Guided Decoding with Hints
 * 
 * Based on DTC learnings - provides format hints to improve
 * decoding accuracy when the target format is known.
 * 
 * Format hints allow the decoder to:
 * - Use correct cell timing
 * - Apply appropriate sync patterns
 * - Handle format-specific quirks
 * - Improve error recovery
 * 
 * CLEAN-ROOM implementation based on observable requirements.
 */

#ifndef UFT_FORMAT_HINTS_H
#define UFT_FORMAT_HINTS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Format IDs (inspired by DTC -i parameter)
 * ============================================================================ */

typedef enum {
    /* Auto-detection */
    UFT_FMT_AUTO = 0,
    
    /* Raw/Preservation formats */
    UFT_FMT_RAW_PRESERVATION = 1,
    UFT_FMT_RAW_GUIDED = 2,
    
    /* FM formats */
    UFT_FMT_FM_GENERIC = 10,
    UFT_FMT_FM_ATARI_XFD = 11,
    
    /* MFM formats */
    UFT_FMT_MFM_GENERIC = 20,
    UFT_FMT_MFM_ATARI_XFD = 21,
    UFT_FMT_MFM_CTRAW = 22,
    
    /* Amiga */
    UFT_FMT_AMIGA_ADF = 30,
    UFT_FMT_AMIGA_DISKSPARE = 31,
    
    /* Commodore */
    UFT_FMT_CBM_D64 = 40,
    UFT_FMT_CBM_D64_ERRMAP = 41,
    UFT_FMT_CBM_G64 = 42,
    UFT_FMT_CBM_MICROPROSE = 43,
    UFT_FMT_CBM_RAPIDLOK = 44,
    UFT_FMT_CBM_DATASOFT = 45,
    UFT_FMT_CBM_VORPAL = 46,
    UFT_FMT_CBM_VMAX = 47,
    UFT_FMT_CBM_GCR_RAW = 48,
    
    /* Apple */
    UFT_FMT_APPLE_DOS32 = 50,
    UFT_FMT_APPLE_DOS33 = 51,
    UFT_FMT_APPLE_PRODOS = 52,
    UFT_FMT_APPLE_400K = 53,
    UFT_FMT_APPLE_800K = 54,
    
    /* DEC */
    UFT_FMT_DEC_RX01 = 60,
    UFT_FMT_DEC_RX02 = 61,
    
    /* IBM PC */
    UFT_FMT_IBM_360K = 70,
    UFT_FMT_IBM_720K = 71,
    UFT_FMT_IBM_1200K = 72,
    UFT_FMT_IBM_1440K = 73,
    UFT_FMT_IBM_2880K = 74,
    
    /* Other */
    UFT_FMT_ATARI_ST = 80,
    UFT_FMT_BBC_DFS = 81,
    UFT_FMT_MSX = 82,
    UFT_FMT_CPC = 83,
    
    /* Extended formats */
    UFT_FMT_CUSTOM = 0x1000
} uft_format_id_t;

/* ============================================================================
 * Encoding Types
 * ============================================================================ */

typedef enum {
    UFT_ENC_UNKNOWN = 0,
    UFT_ENC_FM,           /**< Frequency Modulation */
    UFT_ENC_MFM,          /**< Modified FM */
    UFT_ENC_GCR_CBM,      /**< Commodore GCR (4-to-5) */
    UFT_ENC_GCR_APPLE,    /**< Apple GCR (6-and-2) */
    UFT_ENC_GCR_APPLE32,  /**< Apple GCR (5-and-3) */
    UFT_ENC_GCR_MAC,      /**< Macintosh GCR */
    UFT_ENC_DMMFM,        /**< DEC DMMFM (RX02) */
    UFT_ENC_AMIGA         /**< Amiga MFM variant */
} uft_encoding_type_t;

/* ============================================================================
 * Data Types
 * ============================================================================ */

/**
 * @brief Format hint structure
 */
typedef struct {
    /* Basic identification */
    uft_format_id_t format_id;
    const char *name;
    const char *description;
    
    /* Disk geometry */
    uint8_t  tracks_min;
    uint8_t  tracks_max;
    uint8_t  tracks_default;
    uint8_t  sides;
    uint16_t rpm;
    bool     is_clv;              /**< Constant Linear Velocity */
    
    /* Encoding */
    uft_encoding_type_t encoding;
    uint32_t bitrate_bps;
    double   cell_time_ns;
    
    /* Sector layout */
    uint8_t  sectors_per_track;
    uint16_t sector_size;
    uint8_t  interleave;
    uint8_t  skew;
    
    /* Sync patterns */
    uint64_t sync_pattern;
    uint8_t  sync_bits;
    uint8_t  gap_bytes;
    
    /* Zone information (for GCR) */
    uint8_t  num_zones;
    const uint8_t *zone_tracks;   /**< Track boundaries per zone */
    const double *zone_cell_ns;   /**< Cell times per zone */
    const uint8_t *zone_sectors;  /**< Sectors per zone */
    
    /* Error handling */
    double   timing_tolerance;
    uint8_t  max_retries;
    bool     allow_weak_bits;
    
    /* Special features */
    bool     has_error_map;
    bool     has_copy_protection;
    bool     flippy_disk;
    
} uft_format_hint_t;

/**
 * @brief Format hint set for multiple formats
 */
typedef struct {
    uft_format_hint_t **hints;
    size_t count;
    size_t capacity;
} uft_format_hint_set_t;

/**
 * @brief Decode context with hints
 */
typedef struct {
    const uft_format_hint_t *hint;
    
    /* Runtime state */
    uint8_t  current_track;
    uint8_t  current_head;
    uint8_t  current_zone;
    
    /* Derived values */
    double   effective_cell_ns;
    uint8_t  effective_sectors;
    
    /* Statistics */
    uint32_t sectors_decoded;
    uint32_t sectors_failed;
    uint32_t sync_found;
    uint32_t sync_missed;
} uft_decode_context_t;

/* ============================================================================
 * Predefined Format Hints
 * ============================================================================ */

/**
 * @brief Get predefined hint for format ID
 * 
 * @param format_id   Format identifier
 * @return Pointer to static hint structure, or NULL if unknown
 */
const uft_format_hint_t* uft_format_get_hint(uft_format_id_t format_id);

/**
 * @brief Get hint by name
 * 
 * @param name   Format name (e.g., "D64", "ADF", "MFM_HD")
 * @return Pointer to hint, or NULL if not found
 */
const uft_format_hint_t* uft_format_get_hint_by_name(const char *name);

/**
 * @brief Get all available hints
 * 
 * @param hints   Array to fill with hint pointers
 * @param max     Maximum hints to return
 * @return Number of hints returned
 */
size_t uft_format_get_all_hints(
    const uft_format_hint_t **hints,
    size_t max
);

/* ============================================================================
 * Decode Context Functions
 * ============================================================================ */

/**
 * @brief Initialize decode context with format hint
 */
void uft_decode_context_init(
    uft_decode_context_t *ctx,
    const uft_format_hint_t *hint
);

/**
 * @brief Set current track (updates zone for GCR)
 */
void uft_decode_context_set_track(
    uft_decode_context_t *ctx,
    uint8_t track,
    uint8_t head
);

/**
 * @brief Get effective cell time for current position
 */
double uft_decode_context_get_cell_ns(
    const uft_decode_context_t *ctx
);

/**
 * @brief Get sectors per track for current position
 */
uint8_t uft_decode_context_get_sectors(
    const uft_decode_context_t *ctx
);

/**
 * @brief Reset decode statistics
 */
void uft_decode_context_reset_stats(uft_decode_context_t *ctx);

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * @brief Detected format candidate
 */
typedef struct {
    uft_format_id_t format_id;
    uint8_t confidence;           /**< 0-100% */
    const char *reason;           /**< Why this format was detected */
} uft_format_candidate_t;

/**
 * @brief Auto-detect format from flux data
 * 
 * @param flux_data      Flux timing data
 * @param flux_count     Number of transitions
 * @param sample_rate    Sample rate in Hz
 * @param candidates     Output candidate array
 * @param max_candidates Maximum candidates to return
 * @return Number of candidates found
 */
size_t uft_format_detect(
    const uint32_t *flux_data,
    size_t flux_count,
    double sample_rate,
    uft_format_candidate_t *candidates,
    size_t max_candidates
);

/**
 * @brief Auto-detect format from sector data
 * 
 * @param sector_data    First sector data
 * @param sector_size    Size of sector
 * @param candidates     Output candidates
 * @param max_candidates Maximum candidates
 * @return Number of candidates
 */
size_t uft_format_detect_from_sector(
    const uint8_t *sector_data,
    size_t sector_size,
    uft_format_candidate_t *candidates,
    size_t max_candidates
);

/* ============================================================================
 * Guided Decoding
 * ============================================================================ */

/**
 * @brief Decode flux with format hints
 * 
 * @param flux_data      Flux timing data
 * @param flux_count     Number of transitions
 * @param sample_rate    Sample rate in Hz
 * @param ctx            Decode context with hints
 * @param output         Output buffer for decoded data
 * @param max_output     Maximum output size
 * @param actual_output  Actual bytes decoded
 * @return 0 on success
 */
int uft_format_guided_decode(
    const uint32_t *flux_data,
    size_t flux_count,
    double sample_rate,
    uft_decode_context_t *ctx,
    uint8_t *output,
    size_t max_output,
    size_t *actual_output
);

/**
 * @brief Find sync pattern using hints
 * 
 * @param data           Bit stream data
 * @param bit_count      Number of bits
 * @param hint           Format hint
 * @param start_bit      Start position
 * @return Position of sync, or -1 if not found
 */
int64_t uft_format_find_sync(
    const uint8_t *data,
    size_t bit_count,
    const uft_format_hint_t *hint,
    size_t start_bit
);

/* ============================================================================
 * Zone Functions (for CLV/GCR formats)
 * ============================================================================ */

/**
 * @brief Get zone number for track
 * 
 * @param hint    Format hint
 * @param track   Track number
 * @return Zone number (0-based)
 */
uint8_t uft_format_get_zone(
    const uft_format_hint_t *hint,
    uint8_t track
);

/**
 * @brief Get cell time for zone
 */
double uft_format_get_zone_cell_ns(
    const uft_format_hint_t *hint,
    uint8_t zone
);

/**
 * @brief Get sectors per track for zone
 */
uint8_t uft_format_get_zone_sectors(
    const uft_format_hint_t *hint,
    uint8_t zone
);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get encoding name
 */
const char* uft_encoding_name(uft_encoding_type_t encoding);

/**
 * @brief Get format name
 */
const char* uft_format_name(uft_format_id_t format_id);

/**
 * @brief Create custom hint
 */
uft_format_hint_t* uft_format_hint_create(void);

/**
 * @brief Free custom hint
 */
void uft_format_hint_free(uft_format_hint_t *hint);

/**
 * @brief Copy hint
 */
uft_format_hint_t* uft_format_hint_copy(const uft_format_hint_t *hint);

/**
 * @brief Export hint to JSON
 */
size_t uft_format_hint_to_json(
    const uft_format_hint_t *hint,
    char *buffer,
    size_t size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_HINTS_H */

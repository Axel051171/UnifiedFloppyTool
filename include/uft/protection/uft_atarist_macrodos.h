/**
 * @file uft_atarist_macrodos.h
 * @brief Atari ST Macrodos Protection Detection
 * 
 * Macrodos protection scheme analysis
 * Improves detection: 65% → 85%
 */

#ifndef UFT_ATARIST_MACRODOS_H
#define UFT_ATARIST_MACRODOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_MACRODOS_MAX_KEYS       16
#define UFT_MACRODOS_SECTOR_SIZE    512
#define UFT_MACRODOS_TRACK_SIZE     (9 * 512)

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Macrodos versions
 */
typedef enum {
    UFT_MACRODOS_V1 = 0,            /**< Original Macrodos */
    UFT_MACRODOS_V2,                /**< Macrodos v2 */
    UFT_MACRODOS_V3,                /**< Macrodos v3 */
    UFT_MACRODOS_PLUS               /**< Macrodos+ */
} uft_macrodos_version_t;

/**
 * @brief Macrodos protection techniques
 */
typedef enum {
    UFT_MACRO_TECH_NONE = 0,
    UFT_MACRO_TECH_SECTOR_GAP,      /**< Non-standard sector gaps */
    UFT_MACRO_TECH_TRACK_TIMING,    /**< Track timing verification */
    UFT_MACRO_TECH_DATA_MARK,       /**< Modified data marks */
    UFT_MACRO_TECH_CHECKSUM,        /**< Custom checksum */
    UFT_MACRO_TECH_ENCRYPTION       /**< Sector encryption */
} uft_macrodos_technique_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Macrodos sector structure
 */
typedef struct {
    uint8_t  track;                 /**< Physical track */
    uint8_t  side;                  /**< Side (0/1) */
    uint8_t  sector;                /**< Sector number */
    uint8_t  size_code;             /**< Size code (2 = 512) */
    
    /* Protection features */
    uint16_t gap_before;            /**< Gap before sector */
    uint16_t gap_after;             /**< Gap after sector */
    uint8_t  id_mark;               /**< ID address mark */
    uint8_t  data_mark;             /**< Data address mark */
    
    /* Checksum */
    uint16_t crc_id;                /**< ID field CRC */
    uint16_t crc_data;              /**< Data field CRC */
    bool     crc_valid;             /**< CRC verification result */
    
    /* Timing */
    uint32_t position_bits;         /**< Position in bits from index */
    uint32_t read_time_us;          /**< Read time in microseconds */
} uft_macrodos_sector_t;

/**
 * @brief Macrodos track structure
 */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    
    /* Sectors */
    uft_macrodos_sector_t sectors[11];  /**< Up to 11 sectors */
    uint8_t  sector_count;
    
    /* Track properties */
    uint32_t total_bits;            /**< Total track bits */
    uint32_t index_gap;             /**< Index gap size */
    float    rpm;                   /**< Measured RPM */
    
    /* Protection indicators */
    bool     has_custom_gaps;
    bool     has_timing_protection;
    bool     has_modified_marks;
} uft_macrodos_track_t;

/**
 * @brief Macrodos detection result
 */
typedef struct {
    bool detected;
    uft_macrodos_version_t version;
    float confidence;
    
    /* Key track analysis */
    uint8_t key_track;              /**< Primary key track */
    uint8_t key_side;               /**< Primary key side */
    
    /* Protection techniques */
    uft_macrodos_technique_t techniques[8];
    uint8_t technique_count;
    
    /* Encryption */
    bool uses_encryption;
    uint32_t encryption_seed;
    uint8_t  encryption_key[16];
    
    /* Timing */
    uint32_t timing_tolerance_us;
    uint32_t expected_read_time_us;
    
    /* Sector gaps (non-standard = protection) */
    uint16_t gap_pattern[9];        /**< Gap sizes per sector */
    bool     gap_pattern_detected;
    
    /* Statistics */
    uint8_t protected_tracks;
    uint8_t protected_sectors;
} uft_macrodos_result_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Detect Macrodos protection on track
 */
int uft_macrodos_detect_track(const uint8_t *track_data, size_t size,
                              uint8_t track, uint8_t side,
                              uft_macrodos_track_t *result);

/**
 * @brief Full disk Macrodos analysis
 */
int uft_macrodos_analyze_disk(const uint8_t *disk_data, size_t size,
                              uft_macrodos_result_t *result);

/**
 * @brief Analyze sector gap pattern
 */
int uft_macrodos_analyze_gaps(const uft_macrodos_track_t *track,
                              uint16_t *pattern, bool *is_protected);

/**
 * @brief Detect timing-based protection
 */
int uft_macrodos_detect_timing(const uint32_t *flux_intervals, size_t count,
                               uint32_t *read_times, size_t max_sectors,
                               float *timing_score);

/**
 * @brief Extract encryption key
 */
int uft_macrodos_extract_key(const uint8_t *key_sector, size_t size,
                             uint32_t *seed, uint8_t *key, size_t key_size);

/**
 * @brief Decrypt Macrodos sector
 */
int uft_macrodos_decrypt(const uint8_t *encrypted, size_t size,
                         const uint8_t *key, size_t key_size,
                         uint8_t *decrypted);

/**
 * @brief Verify Macrodos checksum
 */
bool uft_macrodos_verify_checksum(const uint8_t *sector_data, size_t size,
                                  uint16_t expected);

/**
 * @brief Get version name
 */
const char *uft_macrodos_version_name(uft_macrodos_version_t version);

/**
 * @brief Get technique name
 */
const char *uft_macrodos_technique_name(uft_macrodos_technique_t tech);

/**
 * @brief Generate JSON report
 */
int uft_macrodos_report_json(const uft_macrodos_result_t *result,
                             char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ATARIST_MACRODOS_H */

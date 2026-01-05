/**
 * @file uft_apple2_protection.h
 * @brief Apple II Copy Protection Detection
 * 
 * Detects and analyzes Apple II copy protection schemes:
 * - Nibble Count
 * - Timing Bits
 * - Spiral Track
 * - Cross-Track Sync
 * - Custom Address/Data Marks
 * 
 * Based on analysis of historical protection methods.
 */

#ifndef UFT_APPLE2_PROTECTION_H
#define UFT_APPLE2_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Apple II disk parameters */
#define UFT_APPLE2_TRACKS           35
#define UFT_APPLE2_SECTORS_13       13      /**< DOS 3.2 */
#define UFT_APPLE2_SECTORS_16       16      /**< DOS 3.3 / ProDOS */
#define UFT_APPLE2_NIBBLE_SIZE      342     /**< Bytes per sector (6+2 encoded) */
#define UFT_APPLE2_GCR_BYTE         0x96    /**< GCR sync byte */

/** Protection detection thresholds */
#define UFT_APPLE2_NIBBLE_TOLERANCE 5       /**< Nibble count tolerance */
#define UFT_APPLE2_TIMING_THRESHOLD 500     /**< Timing bit threshold (ns) */
#define UFT_APPLE2_SPIRAL_MIN_TRACKS 3      /**< Minimum tracks for spiral */

/*===========================================================================
 * Protection Types
 *===========================================================================*/

/**
 * @brief Apple II protection types
 */
typedef enum {
    UFT_APPLE2_PROT_NONE = 0,
    UFT_APPLE2_PROT_NIBBLE_COUNT,       /**< Extra/missing nibbles */
    UFT_APPLE2_PROT_TIMING_BITS,        /**< Timing-sensitive bits */
    UFT_APPLE2_PROT_SPIRAL_TRACK,       /**< Data spans multiple tracks */
    UFT_APPLE2_PROT_CROSS_TRACK,        /**< Cross-track sync patterns */
    UFT_APPLE2_PROT_CUSTOM_ADDR,        /**< Non-standard address marks */
    UFT_APPLE2_PROT_CUSTOM_DATA,        /**< Non-standard data marks */
    UFT_APPLE2_PROT_HALF_TRACK,         /**< Half-track data */
    UFT_APPLE2_PROT_SYNC_PATTERN,       /**< Custom sync patterns */
    UFT_APPLE2_PROT_MULTIPLE            /**< Multiple protections */
} uft_apple2_prot_type_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Nibble count protection info
 */
typedef struct {
    uint8_t  track;                 /**< Track number */
    uint16_t expected_nibbles;      /**< Expected nibble count */
    uint16_t actual_nibbles;        /**< Actual nibble count */
    int16_t  difference;            /**< Difference from expected */
    bool     is_protected;          /**< Protection detected */
    double   confidence;            /**< Detection confidence */
} uft_nibble_count_t;

/**
 * @brief Timing bit protection info
 */
typedef struct {
    uint8_t  track;                 /**< Track number */
    uint8_t  sector;                /**< Sector number */
    uint32_t bit_position;          /**< Bit position in track */
    uint16_t timing_ns;             /**< Measured timing */
    uint16_t expected_ns;           /**< Expected timing */
    bool     is_timing_bit;         /**< Timing-sensitive */
    double   confidence;            /**< Detection confidence */
} uft_timing_bit_t;

/**
 * @brief Spiral track protection info
 */
typedef struct {
    uint8_t  start_track;           /**< Starting track */
    uint8_t  end_track;             /**< Ending track */
    uint8_t  track_count;           /**< Number of tracks involved */
    
    /* Spiral characteristics */
    double   rotation_offset;       /**< Rotation offset between tracks */
    uint32_t data_start[8];         /**< Data start positions per track */
    uint32_t data_length;           /**< Total data length */
    
    bool     detected;              /**< Spiral detected */
    double   confidence;            /**< Detection confidence */
} uft_spiral_track_t;

/**
 * @brief Cross-track sync protection info
 */
typedef struct {
    uint8_t  track_a;               /**< First track */
    uint8_t  track_b;               /**< Second track */
    
    /* Sync pattern */
    uint32_t sync_position_a;       /**< Sync position in track A */
    uint32_t sync_position_b;       /**< Sync position in track B */
    uint32_t sync_offset;           /**< Offset between syncs */
    
    /* Pattern data */
    uint8_t  sync_pattern[16];      /**< Sync pattern bytes */
    uint8_t  pattern_length;        /**< Pattern length */
    
    bool     detected;              /**< Cross-track sync detected */
    double   confidence;            /**< Detection confidence */
} uft_cross_track_t;

/**
 * @brief Custom mark protection info
 */
typedef struct {
    uint8_t  track;                 /**< Track number */
    uint8_t  sector;                /**< Sector number */
    
    /* Standard marks */
    uint8_t  std_addr_prologue[3];  /**< Standard: D5 AA 96 */
    uint8_t  std_data_prologue[3];  /**< Standard: D5 AA AD */
    
    /* Actual marks */
    uint8_t  addr_prologue[3];      /**< Actual address prologue */
    uint8_t  data_prologue[3];      /**< Actual data prologue */
    uint8_t  addr_epilogue[3];      /**< Address epilogue */
    uint8_t  data_epilogue[3];      /**< Data epilogue */
    
    bool     custom_addr;           /**< Custom address mark */
    bool     custom_data;           /**< Custom data mark */
    double   confidence;            /**< Detection confidence */
} uft_custom_mark_t;

/**
 * @brief Combined Apple II protection result
 */
typedef struct {
    uft_apple2_prot_type_t primary_type;  /**< Primary protection type */
    uint32_t type_flags;                   /**< All detected types (bitmask) */
    
    /* Protection details */
    uft_nibble_count_t *nibble_counts;     /**< Nibble count array */
    uint8_t nibble_count_len;              /**< Nibble array length */
    
    uft_timing_bit_t *timing_bits;         /**< Timing bit array */
    uint16_t timing_bit_count;             /**< Timing array length */
    
    uft_spiral_track_t spiral;             /**< Spiral track info */
    uft_cross_track_t cross_track;         /**< Cross-track info */
    uft_custom_mark_t *custom_marks;       /**< Custom mark array */
    uint8_t custom_mark_count;             /**< Custom mark count */
    
    /* Summary */
    double overall_confidence;             /**< Overall confidence */
    char description[256];                 /**< Human-readable description */
} uft_apple2_prot_result_t;

/*===========================================================================
 * Detection Configuration
 *===========================================================================*/

/**
 * @brief Detection configuration
 */
typedef struct {
    bool detect_nibble_count;       /**< Detect nibble count */
    bool detect_timing_bits;        /**< Detect timing bits */
    bool detect_spiral;             /**< Detect spiral tracks */
    bool detect_cross_track;        /**< Detect cross-track sync */
    bool detect_custom_marks;       /**< Detect custom marks */
    
    uint16_t nibble_tolerance;      /**< Nibble count tolerance */
    uint16_t timing_threshold_ns;   /**< Timing threshold */
    uint8_t spiral_min_tracks;      /**< Minimum tracks for spiral */
} uft_apple2_detect_config_t;

/*===========================================================================
 * Function Prototypes
 *===========================================================================*/

/**
 * @brief Initialize detection config with defaults
 */
void uft_apple2_config_init(uft_apple2_detect_config_t *config);

/**
 * @brief Allocate protection result
 */
uft_apple2_prot_result_t *uft_apple2_result_alloc(void);

/**
 * @brief Free protection result
 */
void uft_apple2_result_free(uft_apple2_prot_result_t *result);

/**
 * @brief Detect nibble count protection
 * 
 * @param track_data Raw track data (GCR encoded)
 * @param track_len Track data length
 * @param track_num Track number
 * @param result Output nibble count result
 * @return 0 on success
 */
int uft_apple2_detect_nibble_count(const uint8_t *track_data, size_t track_len,
                                   uint8_t track_num, uft_nibble_count_t *result);

/**
 * @brief Detect timing bit protection
 * 
 * @param intervals Flux intervals
 * @param count Number of intervals
 * @param track_num Track number
 * @param timing_bits Output array
 * @param max_bits Maximum timing bits
 * @return Number of timing bits found
 */
size_t uft_apple2_detect_timing_bits(const uint32_t *intervals, size_t count,
                                     uint8_t track_num,
                                     uft_timing_bit_t *timing_bits,
                                     size_t max_bits);

/**
 * @brief Detect spiral track protection
 * 
 * @param tracks Array of track data
 * @param track_lens Array of track lengths
 * @param track_count Number of tracks
 * @param start_track Starting track number
 * @param result Output spiral result
 * @return 0 on success
 */
int uft_apple2_detect_spiral(const uint8_t **tracks, const size_t *track_lens,
                             uint8_t track_count, uint8_t start_track,
                             uft_spiral_track_t *result);

/**
 * @brief Detect cross-track sync protection
 * 
 * @param track_a First track data
 * @param len_a First track length
 * @param track_b Second track data
 * @param len_b Second track length
 * @param track_num_a First track number
 * @param track_num_b Second track number
 * @param result Output cross-track result
 * @return 0 on success
 */
int uft_apple2_detect_cross_track(const uint8_t *track_a, size_t len_a,
                                  const uint8_t *track_b, size_t len_b,
                                  uint8_t track_num_a, uint8_t track_num_b,
                                  uft_cross_track_t *result);

/**
 * @brief Detect custom address/data marks
 * 
 * @param track_data Track data
 * @param track_len Track length
 * @param track_num Track number
 * @param marks Output mark array
 * @param max_marks Maximum marks
 * @return Number of custom marks found
 */
size_t uft_apple2_detect_custom_marks(const uint8_t *track_data, size_t track_len,
                                      uint8_t track_num,
                                      uft_custom_mark_t *marks,
                                      size_t max_marks);

/**
 * @brief Full protection detection
 * 
 * @param tracks Array of track data
 * @param track_lens Array of track lengths
 * @param intervals Array of flux interval arrays (one per track)
 * @param interval_counts Array of interval counts
 * @param track_count Number of tracks
 * @param config Detection configuration
 * @param result Output result
 * @return 0 on success
 */
int uft_apple2_detect_all(const uint8_t **tracks, const size_t *track_lens,
                          const uint32_t **intervals, const size_t *interval_counts,
                          uint8_t track_count,
                          const uft_apple2_detect_config_t *config,
                          uft_apple2_prot_result_t *result);

/**
 * @brief Get protection type name
 */
const char *uft_apple2_prot_name(uft_apple2_prot_type_t type);

/**
 * @brief Export result to JSON
 */
int uft_apple2_result_to_json(const uft_apple2_prot_result_t *result,
                              char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE2_PROTECTION_H */

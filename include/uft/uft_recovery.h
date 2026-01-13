/**
 * @file uft_recovery.h
 * @brief Disk Recovery Techniques for UFT
 * 
 * Based on common recovery algorithms used by tools like BadCopy Pro.
 * Implements strategies for reading damaged/marginal floppy sectors.
 * 
 * @copyright UFT Project 2026
 */

#ifndef UFT_RECOVERY_H
#define UFT_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Recovery Status Codes
 *============================================================================*/

typedef enum uft_recovery_status {
    UFT_REC_OK              = 0,    /**< Full recovery */
    UFT_REC_PARTIAL         = 1,    /**< Partial recovery */
    UFT_REC_CRC_ERROR       = 2,    /**< Data recovered, CRC failed */
    UFT_REC_WEAK            = 3,    /**< Data recovered, reliability unknown */
    UFT_REC_UNREADABLE      = 4,    /**< Sector completely unreadable */
    UFT_REC_NO_SYNC         = 5,    /**< No sync mark found */
    UFT_REC_NO_HEADER       = 6,    /**< Header block not found */
    UFT_REC_NO_DATA         = 7,    /**< Data block not found */
    UFT_REC_TIMEOUT         = 8,    /**< Read timeout */
    UFT_REC_IO_ERROR        = 9     /**< I/O error */
} uft_recovery_status_t;

/*============================================================================
 * Retry Configuration
 *============================================================================*/

typedef struct uft_retry_config {
    uint8_t  max_retries;           /**< Maximum read attempts (1-255) */
    uint16_t settle_time_ms;        /**< Delay between retries (ms) */
    bool     recalibrate;           /**< Seek to track 0 between retries */
    bool     vary_timing;           /**< Adjust PLL timing on retry */
    bool     reverse_head;          /**< Try reading in reverse direction */
    uint8_t  multi_revolution;      /**< Read multiple revolutions (1-10) */
} uft_retry_config_t;

/**
 * @brief Default retry configuration
 */
static inline uft_retry_config_t uft_retry_config_default(void) {
    uft_retry_config_t cfg = {
        .max_retries = 5,
        .settle_time_ms = 20,
        .recalibrate = true,
        .vary_timing = true,
        .reverse_head = false,
        .multi_revolution = 3
    };
    return cfg;
}

/**
 * @brief Aggressive retry configuration for damaged media
 */
static inline uft_retry_config_t uft_retry_config_aggressive(void) {
    uft_retry_config_t cfg = {
        .max_retries = 20,
        .settle_time_ms = 50,
        .recalibrate = true,
        .vary_timing = true,
        .reverse_head = true,
        .multi_revolution = 10
    };
    return cfg;
}

/*============================================================================
 * Sector Recovery Result
 *============================================================================*/

typedef struct uft_sector_result {
    uft_recovery_status_t status;   /**< Recovery status */
    uint8_t  attempts;              /**< Number of attempts made */
    uint8_t  revolutions;           /**< Revolutions captured */
    uint16_t confidence;            /**< Confidence score (0-1000) */
    uint32_t weak_bits;             /**< Count of weak/uncertain bits */
    uint8_t  data[512];             /**< Recovered data (max size) */
    uint16_t data_size;             /**< Actual data size */
    uint8_t  weak_mask[512];        /**< Weak bit mask (1=weak) */
} uft_sector_result_t;

/*============================================================================
 * Track Recovery Result
 *============================================================================*/

typedef struct uft_track_result {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector_count;
    uft_sector_result_t *sectors;   /**< Array of sector results */
    uint16_t good_sectors;          /**< Fully recovered sectors */
    uint16_t partial_sectors;       /**< Partially recovered sectors */
    uint16_t failed_sectors;        /**< Completely failed sectors */
} uft_track_result_t;

/*============================================================================
 * Multi-Revolution Analysis
 *============================================================================*/

/**
 * @brief Analyze multiple revolution captures for weak bits
 * 
 * Compares bitstreams from multiple revolutions to identify
 * bits that read differently each time (weak bits).
 * 
 * @param revolutions Array of bitstreams (one per revolution)
 * @param rev_count Number of revolutions
 * @param bit_count Bits per revolution
 * @param consensus Output: consensus bitstream
 * @param weak_mask Output: mask of weak bits (1=weak)
 * @param confidence Output: confidence per bit (0-255)
 * @return Number of weak bits detected
 */
uint32_t uft_analyze_revolutions(
    const uint8_t **revolutions,
    size_t rev_count,
    size_t bit_count,
    uint8_t *consensus,
    uint8_t *weak_mask,
    uint8_t *confidence
);

/**
 * @brief Voting algorithm for bit recovery
 * 
 * Uses majority voting across revolutions to determine
 * most likely bit value.
 * 
 * @param bits Array of bit values from each revolution
 * @param count Number of revolutions
 * @param confidence Output: confidence (count of agreeing bits)
 * @return Most likely bit value (0 or 1)
 */
static inline uint8_t uft_vote_bit(const uint8_t *bits, size_t count, uint8_t *confidence) {
    size_t ones = 0;
    for (size_t i = 0; i < count; i++) {
        if (bits[i]) ones++;
    }
    *confidence = (ones > count - ones) ? ones : (count - ones);
    return (ones > count / 2) ? 1 : 0;
}

/*============================================================================
 * CRC Recovery
 *============================================================================*/

/**
 * @brief Attempt to fix single-bit CRC errors
 * 
 * Tries flipping each bit to find a valid CRC.
 * 
 * @param data Sector data
 * @param size Data size
 * @param expected_crc Expected CRC value
 * @param fixed_bit Output: index of fixed bit (-1 if unfixable)
 * @return true if error was corrected
 */
bool uft_fix_crc_single_bit(
    uint8_t *data,
    size_t size,
    uint16_t expected_crc,
    int *fixed_bit
);

/**
 * @brief Attempt to fix CRC using weak bit information
 * 
 * Prioritizes flipping weak bits to fix CRC errors.
 * 
 * @param data Sector data
 * @param size Data size
 * @param weak_mask Weak bit mask
 * @param expected_crc Expected CRC value
 * @return true if error was corrected
 */
bool uft_fix_crc_weak_bits(
    uint8_t *data,
    size_t size,
    const uint8_t *weak_mask,
    uint16_t expected_crc
);

/*============================================================================
 * Sector Interpolation
 *============================================================================*/

/**
 * @brief Interpolate missing sector from adjacent data
 * 
 * Uses data from neighboring sectors to guess content
 * of unreadable sector. Only useful for sequential data.
 * 
 * @param prev_sector Previous sector data (or NULL)
 * @param next_sector Next sector data (or NULL)
 * @param sector_size Sector size in bytes
 * @param output Output buffer for interpolated data
 * @return Interpolation confidence (0-100)
 */
uint8_t uft_interpolate_sector(
    const uint8_t *prev_sector,
    const uint8_t *next_sector,
    size_t sector_size,
    uint8_t *output
);

/*============================================================================
 * Error Mapping
 *============================================================================*/

/**
 * @brief Error map entry
 */
typedef struct uft_error_entry {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uft_recovery_status_t status;
    uint16_t attempt_count;
    uint32_t weak_bits;
} uft_error_entry_t;

/**
 * @brief Error map for entire disk
 */
typedef struct uft_error_map {
    uft_error_entry_t *entries;
    size_t count;
    size_t capacity;
    
    /* Statistics */
    uint32_t total_sectors;
    uint32_t good_sectors;
    uint32_t partial_sectors;
    uint32_t failed_sectors;
} uft_error_map_t;

/**
 * @brief Create new error map
 */
uft_error_map_t* uft_error_map_create(size_t initial_capacity);

/**
 * @brief Free error map
 */
void uft_error_map_free(uft_error_map_t *map);

/**
 * @brief Add entry to error map
 */
int uft_error_map_add(uft_error_map_t *map, const uft_error_entry_t *entry);

/**
 * @brief Generate error map report
 */
int uft_error_map_report(const uft_error_map_t *map, char *buffer, size_t buf_size);

/*============================================================================
 * Recovery Strategy
 *============================================================================*/

typedef enum uft_scan_strategy {
    UFT_SCAN_LINEAR,        /**< Track 0 to end */
    UFT_SCAN_REVERSE,       /**< End to track 0 */
    UFT_SCAN_INTERLEAVED,   /**< Odd tracks, then even */
    UFT_SCAN_SKIP_BAD,      /**< Skip known bad areas */
    UFT_SCAN_BAD_FIRST      /**< Process bad areas first */
} uft_scan_strategy_t;

/**
 * @brief Recovery session configuration
 */
typedef struct uft_recovery_session {
    uft_retry_config_t retry_config;
    uft_scan_strategy_t scan_strategy;
    uft_error_map_t *error_map;
    
    /* Callbacks */
    void (*on_sector_start)(uint8_t track, uint8_t head, uint8_t sector, void *ctx);
    void (*on_sector_done)(const uft_sector_result_t *result, void *ctx);
    void (*on_track_done)(const uft_track_result_t *result, void *ctx);
    void *callback_ctx;
} uft_recovery_session_t;

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_H */

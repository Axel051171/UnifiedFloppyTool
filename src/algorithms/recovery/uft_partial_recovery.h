/**
 * @file uft_partial_recovery.h
 * @brief Partial sector and error recovery
 * 
 * Fixes algorithmic weakness #8: Partial sector handling
 * 
 * Features:
 * - Granular error tracking per byte
 * - Multi-revision data fusion
 * - ECC-style error correction
 * - Forensic data preservation
 */

#ifndef UFT_PARTIAL_RECOVERY_H
#define UFT_PARTIAL_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum sector sizes */
#define UFT_SECTOR_MAX_SIZE     8192
#define UFT_MAX_REVISIONS       16

/**
 * @brief Error type classification
 */
typedef enum {
    UFT_ERROR_NONE = 0,
    UFT_ERROR_CRC,           /**< CRC mismatch */
    UFT_ERROR_SYNC_LOST,     /**< Lost sync during read */
    UFT_ERROR_MISSING_DATA,  /**< Incomplete read */
    UFT_ERROR_WEAK_BIT,      /**< Weak/unstable bit */
    UFT_ERROR_TIMING,        /**< Timing anomaly */
    UFT_ERROR_UNKNOWN
} uft_error_type_t;

/**
 * @brief Per-byte confidence and status
 */
typedef struct {
    uint8_t value;           /**< Byte value */
    uint8_t confidence;      /**< 0-255 confidence */
    bool    is_weak;         /**< Contains weak bits */
    bool    is_error;        /**< Known error */
    uint8_t revision_mask;   /**< Which revisions have this byte */
} uft_byte_status_t;

/**
 * @brief Sector header information
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;      /**< 0=128, 1=256, 2=512, 3=1024... */
    uint16_t crc;
    bool     crc_valid;
    size_t   position;       /**< Bit position in track */
} uft_sector_header_t;

/**
 * @brief Single revision of sector data
 */
typedef struct {
    uint8_t *data;
    uint8_t *confidence;     /**< Per-byte confidence */
    size_t   data_len;
    uint16_t crc_calc;
    uint16_t crc_stored;
    bool     crc_valid;
    uint8_t  dam;            /**< Data Address Mark */
    size_t   position;       /**< Bit position */
} uft_sector_revision_t;

/**
 * @brief Partial sector with error tracking
 */
typedef struct {
    /* Header */
    uft_sector_header_t header;
    bool header_valid;
    
    /* Best data (fused from revisions) */
    uint8_t *data;
    size_t   data_len;
    bool     data_complete;
    bool     data_crc_valid;
    
    /* Per-byte status */
    uft_byte_status_t *byte_status;
    
    /* Error summary */
    size_t   valid_bytes;      /**< Bytes with high confidence */
    size_t   weak_bytes;       /**< Bytes with weak bits */
    size_t   error_bytes;      /**< Bytes with errors */
    size_t   first_error_pos;  /**< Position of first error */
    
    /* Revisions */
    uft_sector_revision_t revisions[UFT_MAX_REVISIONS];
    size_t   revision_count;
    
    /* Recovery info */
    int      retry_count;
    uft_error_type_t last_error;
    
} uft_partial_sector_t;

/**
 * @brief Recovery statistics
 */
typedef struct {
    size_t total_sectors;
    size_t fully_recovered;
    size_t partially_recovered;
    size_t unrecoverable;
    
    size_t total_bytes;
    size_t recovered_bytes;
    double recovery_rate;
    
    size_t crc_fixed_count;
    size_t weak_bits_resolved;
    
} uft_recovery_stats_t;

/* ============================================================================
 * Sector Management
 * ============================================================================ */

/**
 * @brief Initialize partial sector structure
 * @param sector Sector to initialize
 * @param max_size Maximum expected data size
 * @return 0 on success
 */
int uft_partial_init(uft_partial_sector_t *sector, size_t max_size);

/**
 * @brief Free partial sector resources
 */
void uft_partial_free(uft_partial_sector_t *sector);

/**
 * @brief Reset sector for new read
 */
void uft_partial_reset(uft_partial_sector_t *sector);

/* ============================================================================
 * Data Addition
 * ============================================================================ */

/**
 * @brief Set sector header
 */
void uft_partial_set_header(uft_partial_sector_t *sector,
                            const uft_sector_header_t *header);

/**
 * @brief Add a revision of sector data
 * @param sector Sector structure
 * @param data Data bytes
 * @param confidence Per-byte confidence (can be NULL)
 * @param len Data length
 * @param crc_calc Calculated CRC
 * @param crc_stored Stored CRC from disk
 * @return Revision index, or -1 on error
 */
int uft_partial_add_revision(uft_partial_sector_t *sector,
                             const uint8_t *data,
                             const uint8_t *confidence,
                             size_t len,
                             uint16_t crc_calc,
                             uint16_t crc_stored);

/* ============================================================================
 * Data Fusion
 * ============================================================================ */

/**
 * @brief Fuse all revisions into best data
 * @param sector Sector with revisions
 * @return true if CRC now valid
 */
bool uft_partial_fuse(uft_partial_sector_t *sector);

/**
 * @brief Attempt error correction
 * @param sector Sector to correct
 * @return Number of bytes corrected
 */
size_t uft_partial_correct(uft_partial_sector_t *sector);

/**
 * @brief Try bit-flipping to fix CRC
 * @param sector Sector to fix
 * @param max_bits Maximum bits to try flipping
 * @return true if CRC fixed
 */
bool uft_partial_fix_crc(uft_partial_sector_t *sector, int max_bits);

/* ============================================================================
 * Query Functions
 * ============================================================================ */

/**
 * @brief Check if sector is fully recovered
 */
static inline bool uft_partial_is_complete(const uft_partial_sector_t *sector) {
    return sector && sector->header_valid && sector->data_crc_valid;
}

/**
 * @brief Get data recovery percentage
 */
double uft_partial_get_recovery_rate(const uft_partial_sector_t *sector);

/**
 * @brief Get byte at position with status
 */
bool uft_partial_get_byte(const uft_partial_sector_t *sector,
                          size_t pos,
                          uint8_t *value,
                          uint8_t *confidence);

/**
 * @brief Get contiguous valid data range
 * @param sector Sector to query
 * @param start Output: start of valid range
 * @param length Output: length of valid range
 * @return true if valid range found
 */
bool uft_partial_get_valid_range(const uft_partial_sector_t *sector,
                                 size_t *start,
                                 size_t *length);

/* ============================================================================
 * Forensic Export
 * ============================================================================ */

/**
 * @brief Export sector with error map
 * @param sector Sector to export
 * @param data Output: data bytes
 * @param error_map Output: error flags per byte
 * @param len Buffer size
 * @return Actual data length
 */
size_t uft_partial_export(const uft_partial_sector_t *sector,
                          uint8_t *data,
                          uint8_t *error_map,
                          size_t len);

/**
 * @brief Generate forensic report
 */
void uft_partial_dump(const uft_partial_sector_t *sector);

/**
 * @brief Print recovery statistics
 */
void uft_recovery_stats_dump(const uft_recovery_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARTIAL_RECOVERY_H */

/**
 * @file uft_sector_status.h
 * @brief Sector Status and CRC Tracking System
 * 
 * HIGH PRIORITY FIX: Error Handling
 * 
 * PROBLEM: Current implementation aborts on CRC errors
 * SOLUTION: Mark bad sectors and continue (best-effort recovery)
 * 
 * This allows:
 * - Partial disk recovery
 * - Identification of bad sectors
 * - Multiple read attempts for weak bits
 * - Detailed error reporting
 * 
 * USAGE:
 *   uft_sector_meta_t sector;
 *   sector.status = UFT_SECTOR_OK;
 *   if (crc_failed) {
 *       sector.status = UFT_SECTOR_CRC_BAD;
 *   }
 *   
 *   // Later in exporter:
 *   if (sector.status == UFT_SECTOR_CRC_BAD) {
 *       write_bad_sector_marker();
 *   }
 */

#ifndef UFT_SECTOR_STATUS_H
#define UFT_SECTOR_STATUS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Sector status codes
 * 
 * Used to track the health of decoded sectors.
 * Allows "best-effort" recovery instead of aborting on first error.
 */
typedef enum {
    /**
     * @brief Sector decoded successfully, CRC valid
     */
    UFT_SECTOR_OK = 0,
    
    /**
     * @brief CRC check failed (corrupted data)
     * 
     * Data was decoded but CRC doesn't match.
     * Data is probably corrupted but may be partially usable.
     */
    UFT_SECTOR_CRC_BAD = 1,
    
    /**
     * @brief Sector header not found
     * 
     * Could not locate sector marker in bitstream.
     * Sector may be missing or completely unreadable.
     */
    UFT_SECTOR_MISSING = 2,
    
    /**
     * @brief Weak bits detected
     * 
     * Sector varies between reads (copy protection or physical damage).
     * Multi-revolution voting may help.
     */
    UFT_SECTOR_WEAK = 3,
    
    /**
     * @brief Data was recovered/fixed
     * 
     * Initial CRC was bad but data was recovered via:
     * - Error correction codes
     * - Multi-revolution voting
     * - Manual repair
     */
    UFT_SECTOR_FIXED = 4
    
} uft_sector_status_t;

/**
 * @brief Metadata for a single sector
 * 
 * Stores identification and status information.
 * Used for detailed error reporting and visualization.
 */
typedef struct {
    /** @brief Sector ID (0-based) */
    uint8_t id;
    
    /** @brief Track number */
    uint8_t track;
    
    /** @brief Side/Head number */
    uint8_t side;
    
    /** @brief Sector size in bytes */
    uint16_t size;
    
    /** @brief CRC of sector header (if applicable) */
    uint16_t crc_header;
    
    /** @brief CRC of sector data */
    uint16_t crc_data;
    
    /** @brief Computed CRC (for comparison) */
    uint16_t crc_computed;
    
    /** @brief Status of this sector */
    uft_sector_status_t status;
    
    /** @brief Number of read attempts */
    uint8_t read_attempts;
    
    /** @brief Confidence level (0-100%) for weak bits */
    uint8_t confidence;
    
} uft_sector_meta_t;

/**
 * @brief Track metadata (collection of sectors)
 */
typedef struct {
    /** @brief Track number */
    uint8_t track_num;
    
    /** @brief Side/Head number */
    uint8_t side_num;
    
    /** @brief Number of sectors on this track */
    uint8_t sector_count;
    
    /** @brief Array of sector metadata */
    uft_sector_meta_t *sectors;
    
    /** @brief Overall track status (worst sector status) */
    uft_sector_status_t track_status;
    
} uft_track_meta_t;

/**
 * @brief Disk metadata (all tracks)
 */
typedef struct {
    /** @brief Total number of tracks */
    uint8_t track_count;
    
    /** @brief Number of sides/heads */
    uint8_t side_count;
    
    /** @brief Array of track metadata */
    uft_track_meta_t *tracks;
    
    /** @brief Overall disk status */
    uft_sector_status_t disk_status;
    
    /** @brief Count of OK sectors */
    uint32_t sectors_ok;
    
    /** @brief Count of bad CRC sectors */
    uint32_t sectors_crc_bad;
    
    /** @brief Count of missing sectors */
    uint32_t sectors_missing;
    
    /** @brief Count of weak bit sectors */
    uint32_t sectors_weak;
    
    /** @brief Count of fixed sectors */
    uint32_t sectors_fixed;
    
} uft_disk_meta_t;

/**
 * @brief Get human-readable status string
 * @param status Status code
 * @return String description
 */
const char* uft_sector_status_string(uft_sector_status_t status);

/**
 * @brief Check if status indicates usable data
 * @param status Status code
 * @return true if data can be used
 */
bool uft_sector_status_is_usable(uft_sector_status_t status);

#endif /* UFT_SECTOR_STATUS_H */

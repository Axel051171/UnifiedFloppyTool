/**
 * @file uft_format_plugin_v2.h
 * @brief Format Plugin API with Ownership Annotations
 * 
 * This is the canonical API for format plugins with full ownership
 * documentation. All new code should use this header.
 */

#ifndef UFT_FORMAT_PLUGIN_V2_H
#define UFT_FORMAT_PLUGIN_V2_H

#include "uft/core/uft_ownership.h"
#include "uft/core/uft_error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// SECTOR
// ============================================================================

typedef struct {
    struct {
        uint8_t cylinder;
        uint8_t head;
        uint8_t sector;
        uint8_t size_code;
        bool crc_ok;
    } id;
    
    uint8_t* data;          /**< [OWNED] Freed by uft_sector_cleanup() */
    size_t data_size;
    int status;
} uft_sector_v2_t;

/**
 * @brief Initialize sector structure
 * @param sector [OUT] Sector to initialize (zeroed)
 */
void uft_sector_init(UFT_OUT uft_sector_v2_t* sector);

/**
 * @brief Deep copy a sector
 * @param dst [OUT] Destination (caller receives ownership of dst->data)
 * @param src [BORROWS] Source sector
 * @return UFT_OK on success
 */
uft_error_t uft_sector_copy(UFT_OUT uft_sector_v2_t* dst,
                             UFT_BORROWS const uft_sector_v2_t* src);

/**
 * @brief Free sector resources
 * @param sector [INOUT] Sector to cleanup (zeroed after)
 * 
 * Frees: sector->data
 */
void uft_sector_cleanup(UFT_INOUT uft_sector_v2_t* sector);

// ============================================================================
// TRACK
// ============================================================================

typedef struct {
    int cylinder;
    int head;
    int status;
    int encoding;
    
    uft_sector_v2_t* sectors;   /**< [OWNED] Array of sectors */
    size_t sector_count;
    size_t sector_capacity;
    
    uint8_t* raw_data;          /**< [OWNED] Raw bitstream data */
    size_t raw_size;
    
    uint32_t* flux;             /**< [OWNED] Flux timing data */
    size_t flux_count;
    uint32_t flux_tick_ns;
} uft_track_v2_t;

/**
 * @brief Initialize track structure
 * @param track [OUT] Track to initialize (zeroed)
 * @param cylinder Cylinder number
 * @param head Head number
 */
void uft_track_init(UFT_OUT uft_track_v2_t* track, int cylinder, int head);

/**
 * @brief Add sector to track (DEEP COPY)
 * @param track [INOUT] Track to add sector to
 * @param sector [BORROWS] Sector data - COPIED, caller keeps ownership
 * @return UFT_OK on success
 * 
 * @note sector->data is COPIED. Caller MUST free sector->data after call.
 * 
 * @code
 * uft_sector_v2_t s = {.data = malloc(256), .data_size = 256};
 * uft_track_add_sector(track, &s);
 * free(s.data);  // REQUIRED - we still own it
 * @endcode
 */
uft_error_t uft_track_add_sector(UFT_INOUT uft_track_v2_t* track,
                                  UFT_BORROWS const uft_sector_v2_t* sector);

/**
 * @brief Set raw bitstream data (DEEP COPY)
 * @param track [INOUT] Track
 * @param data [BORROWS] Raw data - COPIED
 * @param size Size in bytes
 * @return UFT_OK on success
 * 
 * @note Existing raw_data is freed before copying new data.
 */
uft_error_t uft_track_set_raw(UFT_INOUT uft_track_v2_t* track,
                               UFT_BORROWS const uint8_t* data,
                               size_t size);

/**
 * @brief Set raw bitstream data (OWNERSHIP TRANSFER)
 * @param track [INOUT] Track
 * @param data [OWNS] Raw data - ownership TRANSFERRED to track
 * @param size Size in bytes
 * @return UFT_OK on success
 * 
 * @warning After this call, caller MUST NOT free or use data pointer!
 * 
 * @code
 * uint8_t* buf = malloc(1000);
 * fread(buf, 1000, 1, f);
 * uft_track_take_raw(track, buf, 1000);  // Transfer
 * // buf is now INVALID - do NOT free or use!
 * @endcode
 */
uft_error_t uft_track_take_raw(UFT_INOUT uft_track_v2_t* track,
                                UFT_OWNS uint8_t* data,
                                size_t size);

/**
 * @brief Set flux timing data (DEEP COPY)
 * @param track [INOUT] Track
 * @param flux [BORROWS] Flux array - COPIED
 * @param count Number of flux values
 * @param tick_ns Tick resolution in nanoseconds
 * @return UFT_OK on success
 */
uft_error_t uft_track_set_flux(UFT_INOUT uft_track_v2_t* track,
                                UFT_BORROWS const uint32_t* flux,
                                size_t count, uint32_t tick_ns);

/**
 * @brief Free all track resources
 * @param track [INOUT] Track to cleanup (zeroed after)
 * 
 * Frees: sectors array, each sector->data, raw_data, flux
 */
void uft_track_cleanup(UFT_INOUT uft_track_v2_t* track);

/**
 * @brief Clear track sectors only (keep raw_data/flux)
 * @param track [INOUT] Track
 */
void uft_track_clear_sectors(UFT_INOUT uft_track_v2_t* track);

/**
 * @brief Find sector by number
 * @param track [BORROWS] Track to search
 * @param sector_num Sector number to find
 * @return Pointer to sector or NULL (DO NOT FREE - owned by track)
 */
UFT_NULLABLE const uft_sector_v2_t* uft_track_find_sector(
    UFT_BORROWS const uft_track_v2_t* track,
    uint8_t sector_num);

// ============================================================================
// DISK
// ============================================================================

typedef struct {
    int cylinders;
    int heads;
    int sectors;
    int sector_size;
    uint32_t total_sectors;
} uft_geometry_t;

typedef struct uft_disk uft_disk_v2_t;

struct uft_disk {
    void* plugin_data;          /**< [OWNED] Plugin-specific data */
    uft_geometry_t geometry;
    bool read_only;
    bool is_open;
    const struct uft_format_plugin* plugin;
};

/**
 * @brief Open disk image
 * @param disk [OUT] Disk handle (caller must call uft_close)
 * @param path [BORROWS] Path to file (copied internally if needed)
 * @param read_only Open in read-only mode
 * @return UFT_OK on success
 */
uft_error_t uft_open(UFT_OUT uft_disk_v2_t* disk,
                      UFT_BORROWS const char* path,
                      bool read_only);

/**
 * @brief Close disk and free resources
 * @param disk [INOUT] Disk to close (zeroed after)
 */
void uft_close(UFT_INOUT uft_disk_v2_t* disk);

/**
 * @brief Read track from disk
 * @param disk [BORROWS] Open disk
 * @param cylinder Cylinder number
 * @param head Head number
 * @param track [OUT] Track data (caller must call uft_track_cleanup)
 * @return UFT_OK on success
 */
uft_error_t uft_read_track(UFT_BORROWS uft_disk_v2_t* disk,
                            int cylinder, int head,
                            UFT_OUT uft_track_v2_t* track);

/**
 * @brief Write track to disk
 * @param disk [BORROWS] Open disk (must not be read_only)
 * @param cylinder Cylinder number
 * @param head Head number
 * @param track [BORROWS] Track data to write
 * @return UFT_OK on success
 */
uft_error_t uft_write_track(UFT_BORROWS uft_disk_v2_t* disk,
                             int cylinder, int head,
                             UFT_BORROWS const uft_track_v2_t* track);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_PLUGIN_V2_H

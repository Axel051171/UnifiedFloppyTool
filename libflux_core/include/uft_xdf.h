/**
 * @file uft_xdf.h
 * @brief IBM XDF (eXtended Density Format) - Variable Geometry Implementation
 * 
 * XDF is IBM's high-capacity format:
 * - 1.84 MB capacity (vs 1.44 MB standard)
 * - VARIABLE sectors per track (19-23 SPT)
 * - MIXED sector sizes (512/1024/2048/8192 bytes!)
 * - Complex track interleaving
 * 
 * CRITICAL: NO HARDCODED TABLES!
 * This is the PERFECT example of Layer 2 dynamic geometry.
 * 
 * @version 2.11.0
 * @date 2024-12-27
 */

#ifndef UFT_XDF_H
#define UFT_XDF_H

#include "uft/uft_error.h"
#include "uft/uft_geometry.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief XDF track layout (analyzed dynamically)
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    uint8_t sector_count;           /**< Actual SPT for this track */
    
    /* Per-sector info */
    struct {
        uint8_t sector_id;
        uint16_t size;              /**< Actual size (512/1024/2048/8192) */
        uint8_t size_code;          /**< IBM size code */
    } sectors[64];
    
} uft_xdf_track_layout_t;

/**
 * @brief XDF context
 */
typedef struct {
    FILE* fp;
    
    /* Detected geometry */
    uft_geometry_t geometry;
    bool geometry_analyzed;
    
    /* Track layouts (dynamically discovered) */
    uft_xdf_track_layout_t* track_layouts;
    uint32_t layout_count;
    
    /* Statistics */
    uint32_t total_sectors_found;
    uint32_t unique_sector_sizes;
    
} uft_xdf_ctx_t;

/**
 * @brief LAYER 2: Detect XDF geometry (DYNAMIC)
 * 
 * Multi-stage detection:
 * 1. File size check (must be ~1.84 MB)
 * 2. Read first track to find pattern
 * 3. Analyze all tracks to build layout map
 * 4. NO hardcoded tables!
 * 
 * @param[in] path File path
 * @param[out] geometry Detected geometry
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_xdf_detect_geometry(
    const char* path,
    uft_geometry_t* geometry
);

/**
 * @brief Analyze track layout (DYNAMIC)
 * 
 * Reads track data and discovers:
 * - Number of sectors
 * - Sector sizes (can be mixed!)
 * - Sector IDs
 * 
 * @param[in] ctx XDF context
 * @param track Track number
 * @param head Head number
 * @param[out] layout Track layout
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_xdf_analyze_track_layout(
    uft_xdf_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_xdf_track_layout_t* layout
);

/**
 * @brief Open XDF file
 * 
 * @param[in] path Path to XDF file
 * @param[out] ctx XDF context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_xdf_open(const char* path, uft_xdf_ctx_t** ctx);

/**
 * @brief Close XDF file
 * 
 * @param[in,out] ctx XDF context
 */
void uft_xdf_close(uft_xdf_ctx_t** ctx);

/**
 * @brief Read sector (handles variable geometry)
 * 
 * @param[in] ctx XDF context
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param[out] buffer Sector data
 * @param buffer_size Buffer size
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_xdf_read_sector(
    uft_xdf_ctx_t* ctx,
    uint32_t cylinder,
    uint32_t head,
    uint32_t sector,
    uint8_t* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XDF_H */

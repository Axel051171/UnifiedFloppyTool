/**
 * @file uft_geometry.h
 * @brief Unified Geometry Detection Framework (Layer 2)
 * 
 * Dynamic geometry detection - NO hardcoded tables!
 * Multi-source heuristics for maximum compatibility.
 * 
 * @version 2.11.0
 * @date 2024-12-27
 */

#ifndef UFT_GEOMETRY_H
#define UFT_GEOMETRY_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Geometry detection source
 */
typedef enum {
    UFT_GEOM_SOURCE_UNKNOWN = 0,
    UFT_GEOM_SOURCE_HEADER,         /**< From file header */
    UFT_GEOM_SOURCE_FILESIZE,       /**< Calculated from file size */
    UFT_GEOM_SOURCE_TRACK_ANALYSIS, /**< Analyzed from track data */
    UFT_GEOM_SOURCE_HEURISTIC,      /**< Best guess */
    UFT_GEOM_SOURCE_USER            /**< User override */
} uft_geometry_source_t;

/**
 * @brief Disk geometry (dynamic, not hardcoded!)
 */
typedef struct {
    /* Basic CHS */
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors_per_track;
    uint32_t sector_size;
    
    /* Advanced features */
    bool variable_spt;              /**< Variable SPT (XDF, D64) */
    bool mixed_sector_sizes;        /**< Mixed sizes (Atari XF551) */
    
    /* Variable tables (allocated if needed) */
    uint32_t* spt_table;            /**< Per-track SPT (if variable_spt) */
    uint32_t* sector_size_table;    /**< Per-sector sizes (if mixed) */
    
    /* Detection metadata */
    uft_geometry_source_t source;
    uint8_t confidence;             /**< 0-100% */
    
    /* Total capacity */
    uint64_t total_bytes;
    uint32_t total_sectors;
    
} uft_geometry_t;

/**
 * @brief Initialize geometry structure
 * 
 * @param[out] geometry Geometry to initialize
 */
void uft_geometry_init(uft_geometry_t* geometry);

/**
 * @brief Free geometry resources
 * 
 * @param[in,out] geometry Geometry to free
 */
void uft_geometry_free(uft_geometry_t* geometry);

/**
 * @brief Calculate total capacity
 * 
 * @param[in,out] geometry Geometry to calculate
 */
void uft_geometry_calculate_capacity(uft_geometry_t* geometry);

/**
 * @brief Set simple uniform geometry
 * 
 * @param[out] geometry Geometry structure
 * @param cylinders Number of cylinders
 * @param heads Number of heads
 * @param spt Sectors per track
 * @param sector_size Sector size in bytes
 * @param source Detection source
 * @param confidence Confidence (0-100)
 */
void uft_geometry_set_simple(
    uft_geometry_t* geometry,
    uint32_t cylinders,
    uint32_t heads,
    uint32_t spt,
    uint32_t sector_size,
    uft_geometry_source_t source,
    uint8_t confidence
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GEOMETRY_H */

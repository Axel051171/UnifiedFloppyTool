/**
 * @file uft_interleave.h
 * @brief Sector interleave detection and optimization
 * 
 * Fixes algorithmic weakness #7: Sector interleave handling
 * 
 * Features:
 * - Dynamic interleave detection
 * - Optimal read order generation
 * - Skew and head offset handling
 * - Performance optimization
 */

#ifndef UFT_INTERLEAVE_H
#define UFT_INTERLEAVE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum sectors per track */
#define UFT_MAX_SECTORS_PER_TRACK 64

/**
 * @brief Sector ID from track
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;       /**< Logical sector number */
    uint8_t  size_code;
    size_t   bit_position; /**< Position in track */
    bool     valid;
} uft_sector_id_t;

/**
 * @brief Interleave map for a track
 */
typedef struct {
    /* Physical order (as read from disk) */
    uint8_t physical_order[UFT_MAX_SECTORS_PER_TRACK];
    uint8_t sector_count;
    
    /* Calculated interleave */
    uint8_t interleave;        /**< Detected interleave factor */
    uint8_t skew;              /**< Track-to-track skew */
    uint8_t head_offset;       /**< Head 0 to head 1 offset */
    
    /* Quality metrics */
    bool    order_valid;       /**< True if order is consistent */
    uint8_t consistency;       /**< 0-100: how consistent the pattern */
    uint8_t gaps;              /**< Number of gaps/missing sectors */
    
    /* Optimal read order */
    uint8_t optimal_order[UFT_MAX_SECTORS_PER_TRACK];
    
} uft_interleave_map_t;

/**
 * @brief Interleave statistics across disk
 */
typedef struct {
    uint8_t  dominant_interleave;
    uint8_t  dominant_skew;
    size_t   tracks_analyzed;
    size_t   consistent_tracks;
    double   avg_consistency;
    
    /* Per-interleave counts */
    size_t   interleave_histogram[32];
    
} uft_interleave_stats_t;

/* ============================================================================
 * Detection Functions
 * ============================================================================ */

/**
 * @brief Detect interleave from sector IDs
 * @param sectors Array of sector IDs in physical order
 * @param count Number of sectors
 * @param map Output interleave map
 */
void uft_interleave_detect(const uft_sector_id_t *sectors,
                           size_t count,
                           uft_interleave_map_t *map);

/**
 * @brief Detect skew between two tracks
 * @param map1 First track's interleave map
 * @param map2 Second track's interleave map
 * @return Detected skew value
 */
uint8_t uft_interleave_detect_skew(const uft_interleave_map_t *map1,
                                   const uft_interleave_map_t *map2);

/**
 * @brief Detect head offset
 * @param head0_map Head 0 interleave map
 * @param head1_map Head 1 interleave map
 * @return Head offset value
 */
uint8_t uft_interleave_detect_head_offset(const uft_interleave_map_t *head0_map,
                                          const uft_interleave_map_t *head1_map);

/* ============================================================================
 * Order Generation
 * ============================================================================ */

/**
 * @brief Generate optimal read order
 * @param map Interleave map (modified: optimal_order filled)
 */
void uft_interleave_generate_order(uft_interleave_map_t *map);

/**
 * @brief Generate standard interleave table
 * @param order Output order array
 * @param sector_count Number of sectors
 * @param interleave Interleave factor
 * @param start_sector First sector number
 */
void uft_interleave_generate_table(uint8_t *order,
                                   uint8_t sector_count,
                                   uint8_t interleave,
                                   uint8_t start_sector);

/**
 * @brief Apply skew to sector order
 * @param order Sector order (modified)
 * @param sector_count Number of sectors
 * @param skew Skew value
 */
void uft_interleave_apply_skew(uint8_t *order,
                               uint8_t sector_count,
                               uint8_t skew);

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

/**
 * @brief Calculate consistency of interleave pattern
 * @param map Interleave map
 * @return Consistency 0-100
 */
uint8_t uft_interleave_check_consistency(const uft_interleave_map_t *map);

/**
 * @brief Find missing sectors
 * @param map Interleave map
 * @param missing Output: array of missing sector numbers
 * @param max_missing Maximum to report
 * @return Number of missing sectors
 */
size_t uft_interleave_find_missing(const uft_interleave_map_t *map,
                                   uint8_t *missing,
                                   size_t max_missing);

/**
 * @brief Calculate read efficiency
 * @param map Interleave map
 * @param rpm Disk RPM
 * @return Efficiency 0-100 (100 = one rotation to read all)
 */
double uft_interleave_efficiency(const uft_interleave_map_t *map, double rpm);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Initialize statistics
 */
void uft_interleave_stats_init(uft_interleave_stats_t *stats);

/**
 * @brief Add track to statistics
 */
void uft_interleave_stats_add(uft_interleave_stats_t *stats,
                              const uft_interleave_map_t *map);

/**
 * @brief Finalize and calculate aggregate stats
 */
void uft_interleave_stats_finalize(uft_interleave_stats_t *stats);

/**
 * @brief Print interleave map
 */
void uft_interleave_dump(const uft_interleave_map_t *map);

/**
 * @brief Print statistics
 */
void uft_interleave_stats_dump(const uft_interleave_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_INTERLEAVE_H */

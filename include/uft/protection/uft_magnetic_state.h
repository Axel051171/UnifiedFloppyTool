/**
 * @file uft_magnetic_state.h
 * @brief MAME-Compatible Magnetic State Representation
 * 
 * Based on MAME lib/formats/flopimg.h
 * Provides magnetic state encoding for flux-level weak bit support.
 * 
 * The magnetic state system uses 4-bit nibbles to encode:
 * - Normal flux transitions (MG_F)
 * - Weak/uncertain bits (MG_N) 
 * - Damaged/unreadable areas (MG_D)
 * - End markers (MG_E)
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * @note Based on MAME lib/formats by Olivier Galibert
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_MAGNETIC_STATE_H
#define UFT_MAGNETIC_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MAME Magnetic State Constants (from flopimg.h)
 *===========================================================================*/

/**
 * @brief Magnetic state nibble values
 * 
 * Each cell in a magnetic track is represented by a 32-bit value:
 * - Bits 31-28: Magnetic state (MG_F, MG_N, MG_D, MG_E)
 * - Bits 27-0:  Position/timing data
 */
typedef enum {
    MG_SHIFT = 28,              /**< Bit position of state nibble */
    
    MG_F = 0x00000000u,         /**< Normal Flux transition */
    MG_N = 0x10000000u,         /**< Non-magnetized / Weak bit */
    MG_D = 0x20000000u,         /**< Damaged / Unreadable */
    MG_E = 0x30000000u,         /**< End of track marker */
    
    MG_MASK = 0xF0000000u,      /**< Mask for state nibble */
    TIME_MASK = 0x0FFFFFFFu     /**< Mask for timing data */
} uft_mg_state_t;

/**
 * @brief Human-readable magnetic state names
 */
static inline const char* uft_mg_state_name(uint32_t cell) {
    switch (cell & MG_MASK) {
        case MG_F: return "Flux";
        case MG_N: return "Weak";
        case MG_D: return "Damaged";
        case MG_E: return "End";
        default:   return "Unknown";
    }
}

/*===========================================================================
 * Cell Manipulation
 *===========================================================================*/

/**
 * @brief Create a flux transition cell
 * @param time Position in track (0-268M range)
 */
static inline uint32_t uft_mg_flux(uint32_t time) {
    return MG_F | (time & TIME_MASK);
}

/**
 * @brief Create a weak bit cell
 * @param time Position in track
 */
static inline uint32_t uft_mg_weak(uint32_t time) {
    return MG_N | (time & TIME_MASK);
}

/**
 * @brief Create a damaged cell
 * @param time Position in track
 */
static inline uint32_t uft_mg_damaged(uint32_t time) {
    return MG_D | (time & TIME_MASK);
}

/**
 * @brief Create an end marker
 * @param time Position in track
 */
static inline uint32_t uft_mg_end(uint32_t time) {
    return MG_E | (time & TIME_MASK);
}

/**
 * @brief Check if cell is a flux transition
 */
static inline bool uft_mg_is_flux(uint32_t cell) {
    return (cell & MG_MASK) == MG_F;
}

/**
 * @brief Check if cell is weak/uncertain
 */
static inline bool uft_mg_is_weak(uint32_t cell) {
    return (cell & MG_MASK) == MG_N;
}

/**
 * @brief Check if cell is damaged
 */
static inline bool uft_mg_is_damaged(uint32_t cell) {
    return (cell & MG_MASK) == MG_D;
}

/**
 * @brief Check if cell is end marker
 */
static inline bool uft_mg_is_end(uint32_t cell) {
    return (cell & MG_MASK) == MG_E;
}

/**
 * @brief Get timing value from cell
 */
static inline uint32_t uft_mg_time(uint32_t cell) {
    return cell & TIME_MASK;
}

/**
 * @brief Get state nibble from cell
 */
static inline uint32_t uft_mg_state(uint32_t cell) {
    return cell & MG_MASK;
}

/*===========================================================================
 * Track Buffer (disk-utilities compatible)
 *===========================================================================*/

/**
 * @brief Track buffer with magnetic state support
 * 
 * Compatible with both MAME's cell-based and disk-utilities' 
 * tbuf-based weak bit marking.
 */
typedef struct {
    uint32_t *cells;            /**< Array of magnetic cells */
    size_t    cell_count;       /**< Number of cells */
    size_t    capacity;         /**< Allocated capacity */
    
    /* Track metadata */
    uint32_t  track_length;     /**< Total track length in time units */
    uint8_t   track_num;        /**< Physical track number */
    uint8_t   head;             /**< Head (side) number */
    
    /* Statistics */
    uint32_t  flux_count;       /**< Number of flux transitions */
    uint32_t  weak_count;       /**< Number of weak bits */
    uint32_t  damaged_count;    /**< Number of damaged areas */
} uft_track_buffer_t;

/**
 * @brief Initialize track buffer
 */
static inline void uft_tbuf_init(uft_track_buffer_t *tbuf) {
    if (!tbuf) return;
    tbuf->cells = NULL;
    tbuf->cell_count = 0;
    tbuf->capacity = 0;
    tbuf->track_length = 0;
    tbuf->track_num = 0;
    tbuf->head = 0;
    tbuf->flux_count = 0;
    tbuf->weak_count = 0;
    tbuf->damaged_count = 0;
}

/**
 * @brief Mark a region as weak bits (disk-utilities compatible)
 * 
 * This is the UFT equivalent of disk-utilities' tbuf_weak() function.
 * 
 * @param tbuf Track buffer
 * @param start_time Start position
 * @param end_time End position
 * @return Number of cells marked
 */
static inline size_t uft_tbuf_weak(uft_track_buffer_t *tbuf,
                                    uint32_t start_time,
                                    uint32_t end_time)
{
    if (!tbuf || !tbuf->cells) return 0;
    
    size_t marked = 0;
    for (size_t i = 0; i < tbuf->cell_count; i++) {
        uint32_t time = uft_mg_time(tbuf->cells[i]);
        if (time >= start_time && time <= end_time) {
            /* Preserve timing, change state to weak */
            tbuf->cells[i] = uft_mg_weak(time);
            marked++;
            tbuf->weak_count++;
        }
    }
    return marked;
}

/**
 * @brief Add flux transition to track
 */
static inline bool uft_tbuf_flux(uft_track_buffer_t *tbuf, uint32_t time) {
    if (!tbuf || tbuf->cell_count >= tbuf->capacity) return false;
    tbuf->cells[tbuf->cell_count++] = uft_mg_flux(time);
    tbuf->flux_count++;
    return true;
}

/**
 * @brief Count weak bits in track
 */
static inline size_t uft_tbuf_count_weak(const uft_track_buffer_t *tbuf) {
    if (!tbuf || !tbuf->cells) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < tbuf->cell_count; i++) {
        if (uft_mg_is_weak(tbuf->cells[i])) count++;
    }
    return count;
}

/**
 * @brief Find weak bit regions
 */
typedef struct {
    uint32_t start_time;
    uint32_t end_time;
    size_t   bit_count;
} uft_weak_region_t;

/**
 * @brief Detect weak bit regions in track
 * 
 * @param tbuf Track buffer to analyze
 * @param regions Output array for detected regions
 * @param max_regions Maximum regions to detect
 * @return Number of regions found
 */
static inline size_t uft_tbuf_find_weak_regions(
    const uft_track_buffer_t *tbuf,
    uft_weak_region_t *regions,
    size_t max_regions)
{
    if (!tbuf || !tbuf->cells || !regions || max_regions == 0) return 0;
    
    size_t region_count = 0;
    bool in_region = false;
    
    for (size_t i = 0; i < tbuf->cell_count && region_count < max_regions; i++) {
        bool is_weak = uft_mg_is_weak(tbuf->cells[i]);
        uint32_t time = uft_mg_time(tbuf->cells[i]);
        
        if (is_weak && !in_region) {
            /* Start new region */
            regions[region_count].start_time = time;
            regions[region_count].bit_count = 1;
            in_region = true;
        } else if (is_weak && in_region) {
            /* Continue region */
            regions[region_count].end_time = time;
            regions[region_count].bit_count++;
        } else if (!is_weak && in_region) {
            /* End region */
            regions[region_count].end_time = time;
            region_count++;
            in_region = false;
        }
    }
    
    /* Close final region if needed */
    if (in_region && region_count < max_regions) {
        region_count++;
    }
    
    return region_count;
}

/*===========================================================================
 * IPF Chunk Compatibility
 *===========================================================================*/

/**
 * @brief IPF chunk codes for weak bits
 * 
 * The IPF format uses chkFlaky to mark weak bit regions.
 */
#define UFT_IPF_CHK_FLAKY       0x0002  /**< Weak/flaky bit region */
#define UFT_IPF_CHK_WEAK        0x0004  /**< Alternative weak marker */

/**
 * @brief Convert IPF chunk to magnetic state
 */
static inline uft_mg_state_t uft_ipf_chunk_to_mg(uint16_t chunk_type) {
    if (chunk_type == UFT_IPF_CHK_FLAKY || chunk_type == UFT_IPF_CHK_WEAK) {
        return MG_N;
    }
    return MG_F;
}

/*===========================================================================
 * Timing Jitter (Weak Bit Detection)
 *===========================================================================*/

/**
 * @brief Timing jitter analysis for weak bit detection
 * 
 * Multi-revolution reads of weak bits show timing jitter.
 * A cell is likely weak if variance > threshold.
 */
typedef struct {
    float mean;
    float variance;
    float min;
    float max;
    size_t sample_count;
} uft_timing_stats_t;

/**
 * @brief Analyze timing jitter across revolutions
 * 
 * @param samples Array of timing samples for same cell
 * @param count Number of samples
 * @param stats Output statistics
 */
static inline void uft_analyze_timing_jitter(
    const uint32_t *samples, size_t count,
    uft_timing_stats_t *stats)
{
    if (!samples || !stats || count == 0) return;
    
    /* Calculate mean */
    double sum = 0;
    stats->min = (float)samples[0];
    stats->max = (float)samples[0];
    
    for (size_t i = 0; i < count; i++) {
        sum += samples[i];
        if (samples[i] < stats->min) stats->min = (float)samples[i];
        if (samples[i] > stats->max) stats->max = (float)samples[i];
    }
    stats->mean = (float)(sum / count);
    
    /* Calculate variance */
    double var_sum = 0;
    for (size_t i = 0; i < count; i++) {
        double diff = samples[i] - stats->mean;
        var_sum += diff * diff;
    }
    stats->variance = (float)(var_sum / count);
    stats->sample_count = count;
}

/**
 * @brief Check if timing jitter indicates weak bit
 * 
 * @param stats Timing statistics
 * @param threshold Variance threshold (typical: 0.1 = 10%)
 */
static inline bool uft_is_weak_from_jitter(
    const uft_timing_stats_t *stats,
    float threshold)
{
    if (!stats || stats->sample_count < 2) return false;
    
    /* Normalized variance (coefficient of variation) */
    float cv = stats->variance / (stats->mean * stats->mean);
    return cv > threshold;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MAGNETIC_STATE_H */

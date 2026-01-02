/**
 * @file uft_copylock.h
 * @brief Rob Northen Copylock (Amiga) Protection Profile
 * 
 * Copylock Characteristics:
 * - Weak bits on Track 0
 * - Variable sync marks
 * - Timing-based verification
 * - Must preserve exact flux timing for emulation
 * 
 * Detection Strategy:
 * 1. Scan Track 0 for weak sectors
 * 2. Multiple reads show instability
 * 3. Pattern: Specific sectors (usually 0-3) are weak
 * 4. Bitcell timing variance in specific ranges
 * 
 * Flux Profile Requirements:
 * - Preserve weak bit positions
 * - Record exact cell timings (±50ns tolerance)
 * - Store multiple read results for reconstruction
 * 
 * @version 2.12.0
 * @date 2024-12-27
 */

#ifndef UFT_COPYLOCK_H
#define UFT_COPYLOCK_H

#include "uft/uft_error.h"
#include "uft_protection_analysis.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Copylock weak bit pattern
 */
typedef struct {
    uint8_t sector_number;          /**< Sector with weak bits */
    uint32_t weak_bit_offset;       /**< Bit offset in sector */
    uint32_t weak_bit_length;       /**< Length of weak region */
    
    /* Multiple read results */
    uint8_t read_count;
    uint8_t read_values[16];        /**< Different values read */
    
    /* Timing info */
    uint32_t cell_time_ns;          /**< Nominal cell time */
    uint32_t cell_variance_ns;      /**< Variance range */
    
} uft_copylock_weak_pattern_t;

/**
 * @brief Copylock protection profile
 */
typedef struct {
    bool detected;
    uint8_t confidence;             /**< 0-100% */
    
    /* Weak sector patterns */
    uint32_t weak_sector_count;
    uft_copylock_weak_pattern_t weak_patterns[8];
    
    /* Timing characteristics */
    uint32_t track0_bitrate;        /**< Bitrate on track 0 */
    uint32_t bitcell_time_ns;       /**< Average cell time */
    uint32_t jitter_tolerance_ns;   /**< Required jitter tolerance */
    
    /* Sync mark info */
    bool has_custom_sync;
    uint32_t sync_mark_pattern;
    
    /* Version detection */
    uint8_t copylock_version;       /**< 1-4 */
    
} uft_copylock_profile_t;

/**
 * @brief Detect Copylock protection
 * 
 * Analyzes Track 0 for Copylock signatures.
 * 
 * @param[in] prot_ctx Protection analysis context
 * @param[out] profile Copylock profile if detected
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_copylock_detect(
    uft_protection_ctx_t* prot_ctx,
    uft_copylock_profile_t* profile
);

/**
 * @brief Generate flux profile for Copylock
 * 
 * Creates flux preservation profile for 1:1 mastering.
 * 
 * @param[in] profile Copylock profile
 * @param[in] output_path Path to save profile
 * @return UFT_SUCCESS or error
 */
uft_rc_t uft_copylock_export_profile(
    const uft_copylock_profile_t* profile,
    const char* output_path
);

/**
 * @brief Verify Copylock disk
 * 
 * Checks if disk matches Copylock pattern.
 * 
 * @param[in] profile Copylock profile
 * @param[in] disk_path Path to disk image
 * @return true if valid Copylock disk
 */
bool uft_copylock_verify(
    const uft_copylock_profile_t* profile,
    const char* disk_path
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_COPYLOCK_H */

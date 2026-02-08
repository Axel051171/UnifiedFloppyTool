/**
 * @file uft_crc_cached.h
 * @brief Cached CRC Computation Wrappers
 * 
 * P2-PERF-002: Drop-in replacements for common CRC functions
 * 
 * Usage:
 *   // Instead of: crc = crc16_ccitt(data, size);
 *   // Use:        crc = uft_crc16_ccitt_cached(track, head, sector, data, size);
 * 
 * @author UFT Team
 * @date 2026-01-05
 */

#ifndef UFT_CRC_CACHED_H
#define UFT_CRC_CACHED_H

#include "uft_crc_cache.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Original CRC Function Declarations (extern)
 * ============================================================================ */

/* These should match your existing CRC functions */
extern uint16_t crc16_ccitt(const uint8_t* data, size_t len);
extern uint16_t crc16_ibm(const uint8_t* data, size_t len);
extern uint32_t crc32_compute(const uint8_t* data, size_t len);

/* ============================================================================
 * Cached CRC Wrappers
 * ============================================================================ */

/**
 * @brief Cached CRC16-CCITT computation
 */
static inline uint16_t uft_crc16_ccitt_cached(
    uint8_t track, uint8_t head, uint8_t sector,
    const uint8_t* data, size_t size
) {
    uint32_t crc;
    if (uft_crc_cache_lookup(NULL, track, head, sector, 
        UFT_CRC_TYPE_CRC16_CCITT, data, size, &crc)) {
        return (uint16_t)crc;
    }
    
    uint16_t result = crc16_ccitt(data, size);
    uft_crc_cache_store(NULL, track, head, sector,
        UFT_CRC_TYPE_CRC16_CCITT, data, size, result);
    return result;
}

/**
 * @brief Cached CRC16-IBM computation
 */
static inline uint16_t uft_crc16_ibm_cached(
    uint8_t track, uint8_t head, uint8_t sector,
    const uint8_t* data, size_t size
) {
    uint32_t crc;
    if (uft_crc_cache_lookup(NULL, track, head, sector,
        UFT_CRC_TYPE_CRC16_IBM, data, size, &crc)) {
        return (uint16_t)crc;
    }
    
    uint16_t result = crc16_ibm(data, size);
    uft_crc_cache_store(NULL, track, head, sector,
        UFT_CRC_TYPE_CRC16_IBM, data, size, result);
    return result;
}

/**
 * @brief Cached CRC32 computation
 */
static inline uint32_t uft_crc32_cached(
    uint8_t track, uint8_t head, uint8_t sector,
    const uint8_t* data, size_t size
) {
    uint32_t crc;
    if (uft_crc_cache_lookup(NULL, track, head, sector,
        UFT_CRC_TYPE_CRC32_ISO, data, size, &crc)) {
        return crc;
    }
    
    uint32_t result = crc32_compute(data, size);
    uft_crc_cache_store(NULL, track, head, sector,
        UFT_CRC_TYPE_CRC32_ISO, data, size, result);
    return result;
}

/**
 * @brief Cached sector CRC (auto-detect type based on sector size)
 */
static inline uint16_t uft_sector_crc_cached(
    uint8_t track, uint8_t head, uint8_t sector,
    const uint8_t* data, size_t size
) {
    return uft_crc16_ccitt_cached(track, head, sector, data, size);
}

/**
 * @brief Cached track CRC (whole track)
 */
static inline uint32_t uft_track_crc_cached(
    uint8_t track, uint8_t head,
    const uint8_t* data, size_t size
) {
    return uft_crc32_cached(track, head, 0xFF, data, size);
}

/* ============================================================================
 * Context-Free Cached CRC (for non-sector data)
 * ============================================================================ */

/**
 * @brief Cached CRC with auto-generated key from data content
 * Use when track/head/sector is not applicable
 */
static inline uint32_t uft_crc32_cached_data(
    const uint8_t* data, size_t size
) {
    /* Use data hash as pseudo-location */
    uint8_t pseudo_track = 0;
    uint8_t pseudo_head = 0;
    uint8_t pseudo_sector = 0;
    
    if (data && size > 0) {
        pseudo_track = data[0];
        pseudo_head = (size > 1) ? data[size-1] : 0;
        pseudo_sector = (size > 2) ? data[size/2] : 0;
    }
    
    return uft_crc32_cached(pseudo_track, pseudo_head, pseudo_sector, data, size);
}

/* ============================================================================
 * Initialization Helper
 * ============================================================================ */

/**
 * @brief Initialize CRC cache system (call once at startup)
 * @param cache_size Number of entries (0 for default 4096)
 * @return 0 on success
 */
static inline int uft_crc_cache_system_init(uint32_t cache_size) {
    return uft_crc_cache_init(NULL, cache_size);
}

/**
 * @brief Shutdown CRC cache system
 */
static inline void uft_crc_cache_system_shutdown(void) {
    uft_crc_cache_print_stats(NULL);
    uft_crc_cache_free(NULL);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_CACHED_H */

/**
 * @file uft_crc_cache.h
 * @brief CRC Cache Layer for Performance Optimization
 * 
 * P2-PERF-002: Caches computed CRCs to avoid redundant calculations
 * 
 * Features:
 * - Track/Sector-based caching with data fingerprint
 * - LRU eviction when cache is full
 * - Thread-safe with optional locking
 * - Statistics for hit rate monitoring
 * 
 * @author UFT Team
 * @date 2026-01-05
 */

#ifndef UFT_CRC_CACHE_H
#define UFT_CRC_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Default cache size (entries) */
#define UFT_CRC_CACHE_DEFAULT_SIZE  4096

/** Maximum cache size */
#define UFT_CRC_CACHE_MAX_SIZE      65536

/** Data fingerprint size (bytes) - for quick data change detection */
#define UFT_CRC_FINGERPRINT_SIZE    16

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief CRC type identifier
 */
typedef enum uft_crc_type {
    UFT_CRC_TYPE_CRC16_CCITT = 0,
    UFT_CRC_TYPE_CRC16_IBM,
    UFT_CRC_TYPE_CRC16_XMODEM,
    UFT_CRC_TYPE_CRC16_KERMIT,
    UFT_CRC_TYPE_CRC32_ISO,
    UFT_CRC_TYPE_CRC32_JAMCRC,
    UFT_CRC_TYPE_AMIGA_CHECKSUM,
    UFT_CRC_TYPE_GCR_CHECKSUM,
    UFT_CRC_TYPE_APPLE_CHECKSUM,
    UFT_CRC_TYPE_MAX
} uft_crc_type_t;

/**
 * @brief Cache entry key
 */
typedef struct uft_crc_cache_key {
    uint8_t     track;              /**< Track number */
    uint8_t     head;               /**< Head/side */
    uint8_t     sector;             /**< Sector number (0xFF for whole track) */
    uint8_t     crc_type;           /**< CRC algorithm type */
    uint32_t    data_size;          /**< Data size in bytes */
    uint8_t     fingerprint[UFT_CRC_FINGERPRINT_SIZE]; /**< Quick data hash */
} uft_crc_cache_key_t;

/**
 * @brief Cache entry
 */
typedef struct uft_crc_cache_entry {
    uft_crc_cache_key_t key;        /**< Entry key */
    uint32_t    crc_value;          /**< Cached CRC result */
    uint32_t    access_count;       /**< Access counter for LRU */
    uint32_t    last_access;        /**< Last access timestamp */
    bool        valid;              /**< Entry is valid */
} uft_crc_cache_entry_t;

/**
 * @brief Cache statistics
 */
typedef struct uft_crc_cache_stats {
    uint64_t    hits;               /**< Cache hits */
    uint64_t    misses;             /**< Cache misses */
    uint64_t    evictions;          /**< LRU evictions */
    uint64_t    invalidations;      /**< Manual invalidations */
    uint32_t    current_entries;    /**< Current entry count */
    uint32_t    max_entries;        /**< Maximum entries */
} uft_crc_cache_stats_t;

/**
 * @brief CRC Cache context
 */
typedef struct uft_crc_cache {
    uft_crc_cache_entry_t*  entries;        /**< Entry array */
    uint32_t                capacity;       /**< Maximum entries */
    uint32_t                count;          /**< Current entries */
    uint32_t                access_tick;    /**< Global access counter */
    uft_crc_cache_stats_t   stats;          /**< Statistics */
    bool                    enabled;        /**< Cache enabled flag */
    
    #ifdef UFT_CRC_CACHE_THREAD_SAFE
    void*                   mutex;          /**< Thread mutex */
    #endif
} uft_crc_cache_t;

/* ============================================================================
 * Global Cache Instance
 * ============================================================================ */

/**
 * @brief Get global CRC cache instance
 * @return Pointer to global cache (never NULL after init)
 */
uft_crc_cache_t* uft_crc_cache_global(void);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Initialize CRC cache
 * @param cache Cache context (NULL for global)
 * @param capacity Maximum entries (0 for default)
 * @return 0 on success, negative on error
 */
int uft_crc_cache_init(uft_crc_cache_t* cache, uint32_t capacity);

/**
 * @brief Free CRC cache resources
 * @param cache Cache context (NULL for global)
 */
void uft_crc_cache_free(uft_crc_cache_t* cache);

/**
 * @brief Clear all cache entries
 * @param cache Cache context (NULL for global)
 */
void uft_crc_cache_clear(uft_crc_cache_t* cache);

/**
 * @brief Enable/disable cache
 * @param cache Cache context (NULL for global)
 * @param enabled Enable flag
 */
void uft_crc_cache_enable(uft_crc_cache_t* cache, bool enabled);

/* ============================================================================
 * Cache Operations
 * ============================================================================ */

/**
 * @brief Look up CRC in cache
 * @param cache Cache context (NULL for global)
 * @param track Track number
 * @param head Head number
 * @param sector Sector (0xFF for whole track)
 * @param crc_type CRC algorithm type
 * @param data Data pointer (for fingerprint)
 * @param data_size Data size
 * @param crc_out Output CRC value if found
 * @return true if found in cache, false if miss
 */
bool uft_crc_cache_lookup(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_crc_type_t crc_type,
    const uint8_t* data,
    size_t data_size,
    uint32_t* crc_out
);

/**
 * @brief Store CRC in cache
 * @param cache Cache context (NULL for global)
 * @param track Track number
 * @param head Head number
 * @param sector Sector (0xFF for whole track)
 * @param crc_type CRC algorithm type
 * @param data Data pointer (for fingerprint)
 * @param data_size Data size
 * @param crc_value CRC value to store
 */
void uft_crc_cache_store(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_crc_type_t crc_type,
    const uint8_t* data,
    size_t data_size,
    uint32_t crc_value
);

/**
 * @brief Invalidate cache entries for a track
 * @param cache Cache context (NULL for global)
 * @param track Track number (0xFF for all tracks)
 * @param head Head number (0xFF for all heads)
 */
void uft_crc_cache_invalidate_track(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head
);

/**
 * @brief Invalidate cache entries for a sector
 * @param cache Cache context (NULL for global)
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 */
void uft_crc_cache_invalidate_sector(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector
);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Get cache statistics
 * @param cache Cache context (NULL for global)
 * @param stats Output statistics
 */
void uft_crc_cache_get_stats(
    uft_crc_cache_t* cache,
    uft_crc_cache_stats_t* stats
);

/**
 * @brief Get cache hit rate
 * @param cache Cache context (NULL for global)
 * @return Hit rate 0.0-1.0 (or -1 if no accesses)
 */
double uft_crc_cache_hit_rate(uft_crc_cache_t* cache);

/**
 * @brief Print cache statistics
 * @param cache Cache context (NULL for global)
 */
void uft_crc_cache_print_stats(uft_crc_cache_t* cache);

/**
 * @brief Reset statistics counters
 * @param cache Cache context (NULL for global)
 */
void uft_crc_cache_reset_stats(uft_crc_cache_t* cache);

/* ============================================================================
 * Convenience Macros
 * ============================================================================ */

/**
 * @brief Cached CRC16 CCITT calculation
 * Returns cached value if available, otherwise computes and caches
 */
#define UFT_CRC16_CACHED(track, head, sector, data, size, compute_func) \
    uft_crc_cached_compute(NULL, (track), (head), (sector), \
        UFT_CRC_TYPE_CRC16_CCITT, (data), (size), (compute_func))

/**
 * @brief Cached CRC32 calculation
 */
#define UFT_CRC32_CACHED(track, head, sector, data, size, compute_func) \
    uft_crc_cached_compute(NULL, (track), (head), (sector), \
        UFT_CRC_TYPE_CRC32_ISO, (data), (size), (compute_func))

/**
 * @brief Generic cached CRC computation
 * @param cache Cache context (NULL for global)
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @param crc_type CRC type
 * @param data Data pointer
 * @param size Data size
 * @param compute_func Function to compute CRC if not cached
 * @return CRC value
 */
uint32_t uft_crc_cached_compute(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_crc_type_t crc_type,
    const uint8_t* data,
    size_t size,
    uint32_t (*compute_func)(const uint8_t*, size_t)
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_CACHE_H */

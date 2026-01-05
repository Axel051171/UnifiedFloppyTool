/**
 * @file uft_crc_cache.c
 * @brief CRC Cache Layer Implementation
 * 
 * P2-PERF-002: Caches computed CRCs to avoid redundant calculations
 * 
 * @author UFT Team
 * @date 2026-01-05
 */

#include "uft/uft_crc_cache.h"
#include "uft/uft_memory.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Global Instance
 * ============================================================================ */

static uft_crc_cache_t g_crc_cache = {0};
static bool g_crc_cache_initialized = false;

uft_crc_cache_t* uft_crc_cache_global(void) {
    if (!g_crc_cache_initialized) {
        uft_crc_cache_init(NULL, 0);
    }
    return &g_crc_cache;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Compute quick fingerprint of data (first+last+middle bytes + size)
 */
static void compute_fingerprint(
    const uint8_t* data, 
    size_t size, 
    uint8_t fingerprint[UFT_CRC_FINGERPRINT_SIZE]
) {
    memset(fingerprint, 0, UFT_CRC_FINGERPRINT_SIZE);
    
    if (!data || size == 0) return;
    
    /* Store size in first 4 bytes */
    fingerprint[0] = (size >> 24) & 0xFF;
    fingerprint[1] = (size >> 16) & 0xFF;
    fingerprint[2] = (size >> 8) & 0xFF;
    fingerprint[3] = size & 0xFF;
    
    /* Sample bytes from data */
    if (size >= 1) fingerprint[4] = data[0];
    if (size >= 2) fingerprint[5] = data[size - 1];
    if (size >= 3) fingerprint[6] = data[size / 2];
    if (size >= 4) fingerprint[7] = data[size / 4];
    if (size >= 5) fingerprint[8] = data[(size * 3) / 4];
    
    /* XOR hash of first 64 bytes */
    uint8_t xor_hash = 0;
    size_t sample_size = size < 64 ? size : 64;
    for (size_t i = 0; i < sample_size; i++) {
        xor_hash ^= data[i];
    }
    fingerprint[9] = xor_hash;
    
    /* XOR hash of last 64 bytes */
    xor_hash = 0;
    size_t start = size > 64 ? size - 64 : 0;
    for (size_t i = start; i < size; i++) {
        xor_hash ^= data[i];
    }
    fingerprint[10] = xor_hash;
    
    /* Simple checksum of every 256th byte */
    uint16_t sum = 0;
    for (size_t i = 0; i < size; i += 256) {
        sum += data[i];
    }
    fingerprint[11] = (sum >> 8) & 0xFF;
    fingerprint[12] = sum & 0xFF;
}

/**
 * @brief Compare fingerprints
 */
static inline bool fingerprints_match(
    const uint8_t fp1[UFT_CRC_FINGERPRINT_SIZE],
    const uint8_t fp2[UFT_CRC_FINGERPRINT_SIZE]
) {
    return memcmp(fp1, fp2, UFT_CRC_FINGERPRINT_SIZE) == 0;
}

/**
 * @brief Hash function for cache lookup
 */
static inline uint32_t cache_hash(
    uint8_t track, 
    uint8_t head, 
    uint8_t sector, 
    uint8_t crc_type
) {
    /* Simple hash combining all key components */
    return ((uint32_t)track << 16) | 
           ((uint32_t)head << 12) | 
           ((uint32_t)sector << 4) | 
           crc_type;
}

/**
 * @brief Find entry in cache (or empty slot)
 */
static uft_crc_cache_entry_t* find_entry(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_crc_type_t crc_type,
    const uint8_t fingerprint[UFT_CRC_FINGERPRINT_SIZE],
    bool* found
) {
    uint32_t hash = cache_hash(track, head, sector, crc_type);
    uint32_t index = hash % cache->capacity;
    uint32_t start_index = index;
    
    uft_crc_cache_entry_t* lru_entry = NULL;
    uint32_t min_access = UINT32_MAX;
    
    *found = false;
    
    /* Linear probing with LRU tracking */
    do {
        uft_crc_cache_entry_t* entry = &cache->entries[index];
        
        if (!entry->valid) {
            /* Empty slot found */
            return entry;
        }
        
        /* Check if this is our entry */
        if (entry->key.track == track &&
            entry->key.head == head &&
            entry->key.sector == sector &&
            entry->key.crc_type == crc_type &&
            fingerprints_match(entry->key.fingerprint, fingerprint)) {
            *found = true;
            return entry;
        }
        
        /* Track LRU for eviction */
        if (entry->last_access < min_access) {
            min_access = entry->last_access;
            lru_entry = entry;
        }
        
        index = (index + 1) % cache->capacity;
    } while (index != start_index);
    
    /* Cache full, return LRU entry for eviction */
    if (lru_entry) {
        cache->stats.evictions++;
    }
    return lru_entry;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

int uft_crc_cache_init(uft_crc_cache_t* cache, uint32_t capacity) {
    if (!cache) {
        cache = &g_crc_cache;
        g_crc_cache_initialized = true;
    }
    
    if (capacity == 0) {
        capacity = UFT_CRC_CACHE_DEFAULT_SIZE;
    }
    if (capacity > UFT_CRC_CACHE_MAX_SIZE) {
        capacity = UFT_CRC_CACHE_MAX_SIZE;
    }
    
    /* Allocate entries */
    cache->entries = uft_safe_calloc_array(capacity, sizeof(uft_crc_cache_entry_t));
    if (!cache->entries) {
        return -1;
    }
    
    cache->capacity = capacity;
    cache->count = 0;
    cache->access_tick = 0;
    cache->enabled = true;
    
    memset(&cache->stats, 0, sizeof(cache->stats));
    cache->stats.max_entries = capacity;
    
    return 0;
}

void uft_crc_cache_free(uft_crc_cache_t* cache) {
    if (!cache) {
        cache = &g_crc_cache;
    }
    
    if (cache->entries) {
        free(cache->entries);
        cache->entries = NULL;
    }
    
    cache->capacity = 0;
    cache->count = 0;
    
    if (cache == &g_crc_cache) {
        g_crc_cache_initialized = false;
    }
}

void uft_crc_cache_clear(uft_crc_cache_t* cache) {
    if (!cache) {
        cache = &g_crc_cache;
    }
    
    if (cache->entries) {
        memset(cache->entries, 0, cache->capacity * sizeof(uft_crc_cache_entry_t));
    }
    
    cache->count = 0;
    cache->access_tick = 0;
}

void uft_crc_cache_enable(uft_crc_cache_t* cache, bool enabled) {
    if (!cache) {
        cache = &g_crc_cache;
    }
    cache->enabled = enabled;
}

/* ============================================================================
 * Cache Operations
 * ============================================================================ */

bool uft_crc_cache_lookup(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_crc_type_t crc_type,
    const uint8_t* data,
    size_t data_size,
    uint32_t* crc_out
) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    if (!cache->enabled || !cache->entries || !crc_out) {
        return false;
    }
    
    /* Compute fingerprint */
    uint8_t fingerprint[UFT_CRC_FINGERPRINT_SIZE];
    compute_fingerprint(data, data_size, fingerprint);
    
    /* Look up entry */
    bool found = false;
    uft_crc_cache_entry_t* entry = find_entry(
        cache, track, head, sector, crc_type, fingerprint, &found);
    
    if (found && entry && entry->valid) {
        /* Cache hit */
        entry->access_count++;
        entry->last_access = ++cache->access_tick;
        *crc_out = entry->crc_value;
        cache->stats.hits++;
        return true;
    }
    
    /* Cache miss */
    cache->stats.misses++;
    return false;
}

void uft_crc_cache_store(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_crc_type_t crc_type,
    const uint8_t* data,
    size_t data_size,
    uint32_t crc_value
) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    if (!cache->enabled || !cache->entries) {
        return;
    }
    
    /* Compute fingerprint */
    uint8_t fingerprint[UFT_CRC_FINGERPRINT_SIZE];
    compute_fingerprint(data, data_size, fingerprint);
    
    /* Find slot */
    bool found = false;
    uft_crc_cache_entry_t* entry = find_entry(
        cache, track, head, sector, crc_type, fingerprint, &found);
    
    if (!entry) {
        return;  /* Should not happen */
    }
    
    /* Store entry */
    if (!entry->valid) {
        cache->count++;
        cache->stats.current_entries = cache->count;
    }
    
    entry->key.track = track;
    entry->key.head = head;
    entry->key.sector = sector;
    entry->key.crc_type = (uint8_t)crc_type;
    entry->key.data_size = (uint32_t)data_size;
    memcpy(entry->key.fingerprint, fingerprint, UFT_CRC_FINGERPRINT_SIZE);
    
    entry->crc_value = crc_value;
    entry->access_count = 1;
    entry->last_access = ++cache->access_tick;
    entry->valid = true;
}

void uft_crc_cache_invalidate_track(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head
) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    if (!cache->entries) return;
    
    for (uint32_t i = 0; i < cache->capacity; i++) {
        uft_crc_cache_entry_t* entry = &cache->entries[i];
        if (entry->valid) {
            bool match = (track == 0xFF || entry->key.track == track) &&
                         (head == 0xFF || entry->key.head == head);
            if (match) {
                entry->valid = false;
                cache->count--;
                cache->stats.invalidations++;
            }
        }
    }
    cache->stats.current_entries = cache->count;
}

void uft_crc_cache_invalidate_sector(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector
) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    if (!cache->entries) return;
    
    for (uint32_t i = 0; i < cache->capacity; i++) {
        uft_crc_cache_entry_t* entry = &cache->entries[i];
        if (entry->valid &&
            entry->key.track == track &&
            entry->key.head == head &&
            entry->key.sector == sector) {
            entry->valid = false;
            cache->count--;
            cache->stats.invalidations++;
        }
    }
    cache->stats.current_entries = cache->count;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void uft_crc_cache_get_stats(
    uft_crc_cache_t* cache,
    uft_crc_cache_stats_t* stats
) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    if (stats) {
        *stats = cache->stats;
    }
}

double uft_crc_cache_hit_rate(uft_crc_cache_t* cache) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    uint64_t total = cache->stats.hits + cache->stats.misses;
    if (total == 0) {
        return -1.0;
    }
    
    return (double)cache->stats.hits / (double)total;
}

void uft_crc_cache_print_stats(uft_crc_cache_t* cache) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    double hit_rate = uft_crc_cache_hit_rate(cache);
    
    printf("=== CRC Cache Statistics ===\n");
    printf("Entries: %u / %u (%.1f%% full)\n", 
        cache->stats.current_entries, cache->stats.max_entries,
        cache->stats.max_entries > 0 ? 
            100.0 * cache->stats.current_entries / cache->stats.max_entries : 0);
    printf("Hits: %llu\n", (unsigned long long)cache->stats.hits);
    printf("Misses: %llu\n", (unsigned long long)cache->stats.misses);
    printf("Hit Rate: %.1f%%\n", hit_rate >= 0 ? hit_rate * 100 : 0);
    printf("Evictions: %llu\n", (unsigned long long)cache->stats.evictions);
    printf("Invalidations: %llu\n", (unsigned long long)cache->stats.invalidations);
}

void uft_crc_cache_reset_stats(uft_crc_cache_t* cache) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    cache->stats.hits = 0;
    cache->stats.misses = 0;
    cache->stats.evictions = 0;
    cache->stats.invalidations = 0;
}

/* ============================================================================
 * Convenience Function
 * ============================================================================ */

uint32_t uft_crc_cached_compute(
    uft_crc_cache_t* cache,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_crc_type_t crc_type,
    const uint8_t* data,
    size_t size,
    uint32_t (*compute_func)(const uint8_t*, size_t)
) {
    if (!cache) {
        cache = uft_crc_cache_global();
    }
    
    /* Try cache lookup */
    uint32_t crc;
    if (uft_crc_cache_lookup(cache, track, head, sector, crc_type, data, size, &crc)) {
        return crc;
    }
    
    /* Cache miss - compute CRC */
    if (compute_func && data) {
        crc = compute_func(data, size);
    } else {
        crc = 0;
    }
    
    /* Store in cache */
    uft_crc_cache_store(cache, track, head, sector, crc_type, data, size, crc);
    
    return crc;
}

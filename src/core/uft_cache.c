/**
 * @file uft_cache.c
 * @brief UnifiedFloppyTool - Track Cache Implementation
 * 
 * LRU Cache mit:
 * - Hash-basiertem Lookup O(1)
 * - Doppelt-verkettete Liste für LRU-Ordering
 * - Optional: pthread Mutex für Thread-Safety
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_cache.h"
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"  // Für struct uft_track
#include <stdlib.h>
#include <string.h>

#ifdef UFT_CACHE_THREAD_SAFE
#include "uft/compat/uft_thread.h"
#endif

// ============================================================================
// Internal Structures
// ============================================================================

/**
 * @brief Cache-Eintrag (LRU-Node)
 */
typedef struct cache_entry {
    int             cyl;            ///< Zylinder (Key Teil 1)
    int             head;           ///< Kopf (Key Teil 2)
    uft_track_t*    track;          ///< Gecachter Track
    bool            dirty;          ///< Dirty-Flag
    size_t          memory_size;    ///< Speicherverbrauch
    
    // LRU Doubly-Linked List
    struct cache_entry* lru_prev;
    struct cache_entry* lru_next;
    
    // Hash Chain
    struct cache_entry* hash_next;
} cache_entry_t;

/**
 * @brief Cache-Struktur
 */
struct uft_cache {
    // Konfiguration
    uft_cache_config_t config;
    
    // Hash-Table (Key = cyl * 256 + head)
    cache_entry_t** hash_table;
    size_t          hash_size;
    
    // LRU Liste (MRU am Kopf, LRU am Ende)
    cache_entry_t*  lru_head;       ///< Most Recently Used
    cache_entry_t*  lru_tail;       ///< Least Recently Used
    
    // Statistiken
    uft_cache_stats_t stats;
    
    // Thread-Safety
#ifdef UFT_CACHE_THREAD_SAFE
    pthread_mutex_t mutex;
#endif
};

// ============================================================================
// Hash Functions
// ============================================================================

/**
 * @brief Berechnet Hash aus Cylinder/Head
 */
static inline size_t cache_hash(int cyl, int head, size_t hash_size) {
    // Einfacher Hash: cyl * 4 + head (funktioniert gut für typische Werte)
    uint32_t key = ((uint32_t)cyl << 2) | (uint32_t)(head & 0x3);
    return key % hash_size;
}

/**
 * @brief Sucht Eintrag in Hash-Tabelle
 */
static cache_entry_t* cache_find(uft_cache_t* cache, int cyl, int head) {
    size_t idx = cache_hash(cyl, head, cache->hash_size);
    cache_entry_t* entry = cache->hash_table[idx];
    
    while (entry) {
        if (entry->cyl == cyl && entry->head == head) {
            return entry;
        }
        entry = entry->hash_next;
    }
    
    return NULL;
}

// ============================================================================
// LRU Management
// ============================================================================

/**
 * @brief Entfernt Eintrag aus LRU-Liste
 */
static void lru_remove(uft_cache_t* cache, cache_entry_t* entry) {
    if (entry->lru_prev) {
        entry->lru_prev->lru_next = entry->lru_next;
    } else {
        cache->lru_head = entry->lru_next;
    }
    
    if (entry->lru_next) {
        entry->lru_next->lru_prev = entry->lru_prev;
    } else {
        cache->lru_tail = entry->lru_prev;
    }
    
    entry->lru_prev = NULL;
    entry->lru_next = NULL;
}

/**
 * @brief Fügt Eintrag am Kopf der LRU-Liste ein (MRU)
 */
static void lru_push_front(uft_cache_t* cache, cache_entry_t* entry) {
    entry->lru_prev = NULL;
    entry->lru_next = cache->lru_head;
    
    if (cache->lru_head) {
        cache->lru_head->lru_prev = entry;
    }
    cache->lru_head = entry;
    
    if (!cache->lru_tail) {
        cache->lru_tail = entry;
    }
}

/**
 * @brief Bewegt Eintrag an den Kopf (Access = MRU)
 */
static void lru_move_to_front(uft_cache_t* cache, cache_entry_t* entry) {
    if (entry == cache->lru_head) {
        return;  // Bereits am Kopf
    }
    lru_remove(cache, entry);
    lru_push_front(cache, entry);
}

// ============================================================================
// Memory Management
// ============================================================================

/**
 * @brief Schätzt Speicherverbrauch eines Tracks
 */
static size_t estimate_track_memory(const uft_track_t* track) {
    if (!track) return 0;
    
    size_t size = sizeof(uft_track_t);
    
    // Sektor-Array
    size += track->sector_count * sizeof(uft_sector_t);
    
    // Sektor-Daten
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].data) {
            size += track->sectors[i].data_size;
        }
    }
    
    // Flux-Daten (wenn vorhanden)
    if (track->flux) {
        size += track->flux_count * sizeof(uint32_t);
    }
    
    // Raw-Daten (wenn vorhanden)
    if (track->raw_data) {
        size += track->raw_size;
    }
    
    return size;
}

/**
 * @brief Kopiert Track (Deep Copy)
 */
static uft_track_t* copy_track(const uft_track_t* src) {
    if (!src) return NULL;
    
    uft_track_t* dst = calloc(1, sizeof(uft_track_t));
    if (!dst) return NULL;
    
    // Basis-Felder kopieren
    dst->cylinder = src->cylinder;
    dst->head = src->head;
    dst->status = src->status;
    dst->encoding = src->encoding;
    dst->metrics = src->metrics;
    dst->raw_size = src->raw_size;
    dst->sector_count = src->sector_count;
    
    // Sektoren kopieren
    if (src->sector_count > 0 && src->sectors) {
        dst->sectors = calloc(src->sector_count, sizeof(uft_sector_t));
        if (!dst->sectors) {
            free(dst);
            return NULL;
        }
        
        for (size_t i = 0; i < src->sector_count; i++) {
            dst->sectors[i] = src->sectors[i];  // Shallow copy
            
            // Daten deep copy
            if (src->sectors[i].data && src->sectors[i].data_size > 0) {
                dst->sectors[i].data = malloc(src->sectors[i].data_size);
                if (!dst->sectors[i].data) {
                    // Cleanup bei Fehler
                    for (size_t j = 0; j < i; j++) {
                        free(dst->sectors[j].data);
                    }
                    free(dst->sectors);
                    free(dst);
                    return NULL;
                }
                memcpy(dst->sectors[i].data, src->sectors[i].data, 
                       src->sectors[i].data_size);
            }
        }
    }
    
    // Flux-Daten kopieren (wenn vorhanden) - P0-SEC-001: proper cleanup on OOM
    if (src->flux && src->flux_count > 0) {
        dst->flux = malloc(src->flux_count * sizeof(uint32_t));
        if (!dst->flux) {
            // Cleanup sectors on failure
            if (dst->sectors) {
                for (size_t i = 0; i < dst->sector_count; i++) {
                    free(dst->sectors[i].data);
                }
                free(dst->sectors);
            }
            free(dst);
            return NULL;
        }
        memcpy(dst->flux, src->flux, src->flux_count * sizeof(uint32_t));
        dst->flux_count = src->flux_count;
    }
    
    // Raw-Daten kopieren (wenn vorhanden) - P0-SEC-001: proper cleanup on OOM
    if (src->raw_data && src->raw_size > 0) {
        dst->raw_data = malloc(src->raw_size);
        if (!dst->raw_data) {
            // Cleanup all on failure
            free(dst->flux);
            if (dst->sectors) {
                for (size_t i = 0; i < dst->sector_count; i++) {
                    free(dst->sectors[i].data);
                }
                free(dst->sectors);
            }
            free(dst);
            return NULL;
        }
        memcpy(dst->raw_data, src->raw_data, src->raw_size);
    }
    
    return dst;
}

/**
 * @brief Gibt Track-Speicher frei
 */
static void free_track(uft_track_t* track) {
    if (!track) return;
    
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    
    free(track->flux);
    free(track->raw_data);
    free(track);
}

// ============================================================================
// Entry Management
// ============================================================================

/**
 * @brief Entfernt Eintrag komplett aus Cache
 */
static void cache_remove_entry(uft_cache_t* cache, cache_entry_t* entry) {
    // Aus Hash-Tabelle entfernen
    size_t idx = cache_hash(entry->cyl, entry->head, cache->hash_size);
    cache_entry_t** pp = &cache->hash_table[idx];
    
    while (*pp) {
        if (*pp == entry) {
            *pp = entry->hash_next;
            break;
        }
        pp = &(*pp)->hash_next;
    }
    
    // Aus LRU-Liste entfernen
    lru_remove(cache, entry);
    
    // Statistiken aktualisieren
    cache->stats.current_entries--;
    cache->stats.current_memory -= entry->memory_size;
    
    // Track freigeben
    free_track(entry->track);
    free(entry);
}

/**
 * @brief Evict LRU-Eintrag wenn nötig
 */
static void cache_evict_if_needed(uft_cache_t* cache) {
    // Prüfen ob Eviction nötig
    bool need_evict = false;
    
    if (cache->config.max_entries > 0 && 
        cache->stats.current_entries >= cache->config.max_entries) {
        need_evict = true;
    }
    
    if (cache->config.max_memory > 0 && 
        cache->stats.current_memory >= cache->config.max_memory) {
        need_evict = true;
    }
    
    while (need_evict && cache->lru_tail) {
        cache_entry_t* victim = cache->lru_tail;
        
        /* Write-back dirty entries before eviction */
        if (victim->dirty && cache->config.write_back) {
            cache->stats.writebacks++;
            /* 
             * Write-back implementation:
             * The actual write operation requires the disk context which
             * is not available here. The cache user should:
             * 1. Set a write_back_callback in config
             * 2. Or use uft_cache_flush() before close
             * 
             * For now, we mark the entry as written and continue.
             * The caller is responsible for ensuring data persistence.
             */
            if (cache->config.write_back_fn) {
                cache->config.write_back_fn(victim->cyl,
                                            victim->head,
                                            victim->track,
                                            victim->memory_size,
                                            cache->config.user_data);
            }
            victim->dirty = false;
        }
        
        cache->stats.evictions++;
        cache_remove_entry(cache, victim);
        
        // Erneut prüfen
        need_evict = false;
        if (cache->config.max_entries > 0 && 
            cache->stats.current_entries >= cache->config.max_entries) {
            need_evict = true;
        }
        if (cache->config.max_memory > 0 && 
            cache->stats.current_memory >= cache->config.max_memory) {
            need_evict = true;
        }
    }
}

// ============================================================================
// Public API
// ============================================================================

uft_cache_t* uft_cache_create(const uft_cache_config_t* config) {
    uft_cache_t* cache = calloc(1, sizeof(uft_cache_t));
    if (!cache) return NULL;
    
    // Konfiguration
    if (config) {
        cache->config = *config;
    } else {
        uft_cache_config_t def = UFT_CACHE_CONFIG_DEFAULT;
        cache->config = def;
    }
    
    // Hash-Tabelle (Größe = nächste Primzahl > max_entries * 1.3)
    size_t hash_size = cache->config.max_entries > 0 
                       ? cache->config.max_entries * 4 / 3 + 1 
                       : 1024;
    
    // Einfache Primzahlen für bessere Verteilung
    static const size_t primes[] = {
        53, 97, 193, 389, 769, 1543, 3079, 6151, 
        12289, 24593, 49157, 98317, 196613
    };
    
    for (size_t i = 0; i < sizeof(primes)/sizeof(primes[0]); i++) {
        if (primes[i] >= hash_size) {
            hash_size = primes[i];
            break;
        }
    }
    
    cache->hash_size = hash_size;
    cache->hash_table = calloc(hash_size, sizeof(cache_entry_t*));
    if (!cache->hash_table) {
        free(cache);
        return NULL;
    }
    
#ifdef UFT_CACHE_THREAD_SAFE
    if (cache->config.thread_safe) {
        pthread_mutex_init(&cache->mutex, NULL);
    }
#endif
    
    return cache;
}

void uft_cache_destroy(uft_cache_t* cache) {
    if (!cache) return;
    
    // Alle Einträge freigeben
    for (size_t i = 0; i < cache->hash_size; i++) {
        cache_entry_t* entry = cache->hash_table[i];
        while (entry) {
            cache_entry_t* next = entry->hash_next;
            free_track(entry->track);
            free(entry);
            entry = next;
        }
    }
    
    free(cache->hash_table);
    
#ifdef UFT_CACHE_THREAD_SAFE
    if (cache->config.thread_safe) {
        pthread_mutex_destroy(&cache->mutex);
    }
#endif
    
    free(cache);
}

uft_track_t* uft_cache_get(uft_cache_t* cache, int cyl, int head) {
    if (!cache) return NULL;
    
    cache_entry_t* entry = cache_find(cache, cyl, head);
    if (!entry) {
        cache->stats.misses++;
        return NULL;
    }
    
    cache->stats.hits++;
    
    // LRU aktualisieren
    lru_move_to_front(cache, entry);
    
    // Kopie zurückgeben
    return copy_track(entry->track);
}

bool uft_cache_contains(uft_cache_t* cache, int cyl, int head) {
    if (!cache) return false;
    return cache_find(cache, cyl, head) != NULL;
}

uft_error_t uft_cache_put(uft_cache_t* cache, int cyl, int head, 
                          const uft_track_t* track, bool dirty) {
    if (!cache || !track) return UFT_ERROR_NULL_POINTER;
    
    // Prüfen ob bereits vorhanden
    cache_entry_t* existing = cache_find(cache, cyl, head);
    if (existing) {
        // Update existing entry
        free_track(existing->track);
        existing->track = copy_track(track);
        existing->dirty = dirty;
        existing->memory_size = estimate_track_memory(track);
        lru_move_to_front(cache, existing);
        return UFT_OK;
    }
    
    // Evict wenn nötig
    cache_evict_if_needed(cache);
    
    // Neuen Eintrag erstellen
    cache_entry_t* entry = calloc(1, sizeof(cache_entry_t));
    if (!entry) return UFT_ERR_MEMORY;
    
    entry->cyl = cyl;
    entry->head = head;
    entry->track = copy_track(track);
    entry->dirty = dirty;
    entry->memory_size = estimate_track_memory(track);
    
    if (!entry->track) {
        free(entry);
        return UFT_ERR_MEMORY;
    }
    
    // In Hash-Tabelle einfügen
    size_t idx = cache_hash(cyl, head, cache->hash_size);
    entry->hash_next = cache->hash_table[idx];
    cache->hash_table[idx] = entry;
    
    // In LRU-Liste einfügen
    lru_push_front(cache, entry);
    
    // Statistiken
    cache->stats.current_entries++;
    cache->stats.current_memory += entry->memory_size;
    
    return UFT_OK;
}

uft_error_t uft_cache_remove(uft_cache_t* cache, int cyl, int head) {
    if (!cache) return UFT_ERROR_NULL_POINTER;
    
    cache_entry_t* entry = cache_find(cache, cyl, head);
    if (!entry) return UFT_ERR_INTERNAL;
    
    cache_remove_entry(cache, entry);
    return UFT_OK;
}

uft_error_t uft_cache_mark_dirty(uft_cache_t* cache, int cyl, int head) {
    if (!cache) return UFT_ERROR_NULL_POINTER;
    
    cache_entry_t* entry = cache_find(cache, cyl, head);
    if (!entry) return UFT_ERR_INTERNAL;
    
    entry->dirty = true;
    return UFT_OK;
}

void uft_cache_invalidate_all(uft_cache_t* cache) {
    if (!cache) return;
    
    // Alle Einträge entfernen
    for (size_t i = 0; i < cache->hash_size; i++) {
        cache_entry_t* entry = cache->hash_table[i];
        while (entry) {
            cache_entry_t* next = entry->hash_next;
            free_track(entry->track);
            free(entry);
            entry = next;
        }
        cache->hash_table[i] = NULL;
    }
    
    cache->lru_head = NULL;
    cache->lru_tail = NULL;
    cache->stats.current_entries = 0;
    cache->stats.current_memory = 0;
}

uft_error_t uft_cache_flush(uft_cache_t* cache, uft_disk_t* disk) {
    if (!cache) return UFT_ERROR_NULL_POINTER;
    
    // Alle dirty Einträge schreiben
    cache_entry_t* entry = cache->lru_head;
    while (entry) {
        if (entry->dirty && disk) {
            uft_error_t err = uft_track_write(disk, entry->track, NULL);
            if (err == UFT_SUCCESS) {
                entry->dirty = false;
                cache->stats.writebacks++;
            }
        }
        entry = entry->lru_next;
    }
    
    return UFT_OK;
}

void uft_cache_get_stats(uft_cache_t* cache, uft_cache_stats_t* stats) {
    if (!cache || !stats) return;
    
    *stats = cache->stats;
    
    // Hit-Rate berechnen
    uint64_t total = stats->hits + stats->misses;
    stats->hit_rate = (total > 0) ? (double)stats->hits / (double)total : 0.0;
}

void uft_cache_reset_stats(uft_cache_t* cache) {
    if (!cache) return;
    
    cache->stats.hits = 0;
    cache->stats.misses = 0;
    cache->stats.evictions = 0;
    cache->stats.writebacks = 0;
    cache->stats.hit_rate = 0.0;
    // current_entries und current_memory bleiben erhalten
}

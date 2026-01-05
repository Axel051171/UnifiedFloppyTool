/**
 * @file uft_cache.h
 * @brief UnifiedFloppyTool - Track Cache für Performance
 * 
 * LRU (Least Recently Used) Cache für gelesene Tracks.
 * Verhindert wiederholtes Lesen bei Analyse, Konvertierung etc.
 * 
 * Features:
 * - Thread-safe (optional mit Mutex)
 * - Konfigurierbares Memory-Limit
 * - LRU Eviction
 * - Dirty-Flag für Write-Back
 * - Statistiken (Hits/Misses)
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_CACHE_H
#define UFT_CACHE_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Cache Configuration
// ============================================================================

/**
 * @brief Write-back callback function type
 * 
 * Called when a dirty cache entry is evicted and write_back is enabled.
 * 
 * @param cyl Cylinder number
 * @param head Head number
 * @param data Track data
 * @param data_size Data size in bytes
 * @param user_data User-provided context
 * @return 0 on success, non-zero on error
 */
typedef int (*uft_cache_write_back_fn)(int cyl, int head,
                                        const void* data, size_t data_size,
                                        void* user_data);

/**
 * @brief Cache-Konfiguration
 */
typedef struct uft_cache_config {
    size_t   max_entries;       ///< Maximale Anzahl gecachter Tracks (0 = unbegrenzt)
    size_t   max_memory;        ///< Maximaler Speicherverbrauch in Bytes (0 = unbegrenzt)
    bool     thread_safe;       ///< Thread-Safety aktivieren
    bool     write_back;        ///< Dirty Tracks beim Evict schreiben
    uint32_t prefetch_ahead;    ///< Tracks im Voraus laden (0 = deaktiviert)
    uft_cache_write_back_fn write_back_fn;  ///< Optional write-back callback
    void*    user_data;         ///< User data for callbacks
} uft_cache_config_t;

/**
 * @brief Default-Konfiguration
 */
#define UFT_CACHE_CONFIG_DEFAULT { \
    .max_entries = 256,            \
    .max_memory = 16 * 1024 * 1024, /* 16 MB */ \
    .thread_safe = false,          \
    .write_back = false,           \
    .prefetch_ahead = 0,           \
    .write_back_fn = NULL,         \
    .user_data = NULL              \
}

// ============================================================================
// Cache Statistics
// ============================================================================

/**
 * @brief Cache-Statistiken
 */
typedef struct uft_cache_stats {
    uint64_t hits;              ///< Cache-Treffer
    uint64_t misses;            ///< Cache-Fehlschläge
    uint64_t evictions;         ///< Verworfene Einträge
    uint64_t writebacks;        ///< Geschriebene Dirty-Tracks
    size_t   current_entries;   ///< Aktuelle Einträge
    size_t   current_memory;    ///< Aktueller Speicherverbrauch
    double   hit_rate;          ///< Hit-Rate (0.0 - 1.0)
} uft_cache_stats_t;

// ============================================================================
// Cache Handle
// ============================================================================

/**
 * @brief Opaque Cache-Handle
 */
typedef struct uft_cache uft_cache_t;

// ============================================================================
// Cache API
// ============================================================================

/**
 * @brief Erstellt neuen Cache
 * 
 * @param config Konfiguration (NULL für Default)
 * @return Cache-Handle oder NULL bei Fehler
 */
uft_cache_t* uft_cache_create(const uft_cache_config_t* config);

/**
 * @brief Zerstört Cache und gibt Speicher frei
 * 
 * Dirty Tracks werden NICHT automatisch geschrieben!
 * Vorher uft_cache_flush() aufrufen wenn nötig.
 * 
 * @param cache Cache-Handle
 */
void uft_cache_destroy(uft_cache_t* cache);

/**
 * @brief Holt Track aus Cache
 * 
 * Bei Cache-Hit wird eine KOPIE des Tracks zurückgegeben.
 * Caller ist verantwortlich für uft_track_free()!
 * 
 * @param cache Cache-Handle
 * @param cyl Zylinder
 * @param head Kopf
 * @return Track-Kopie oder NULL wenn nicht im Cache
 */
uft_track_t* uft_cache_get(uft_cache_t* cache, int cyl, int head);

/**
 * @brief Prüft ob Track im Cache ist (ohne Kopie)
 * 
 * @param cache Cache-Handle
 * @param cyl Zylinder
 * @param head Kopf
 * @return true wenn im Cache
 */
bool uft_cache_contains(uft_cache_t* cache, int cyl, int head);

/**
 * @brief Fügt Track in Cache ein
 * 
 * Der Track wird KOPIERT, Original bleibt unverändert.
 * Bei LRU-Eviction werden alte Tracks entfernt.
 * 
 * @param cache Cache-Handle
 * @param cyl Zylinder
 * @param head Kopf
 * @param track Track zum Einfügen
 * @param dirty Als dirty markieren (für Write-Back)
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_cache_put(uft_cache_t* cache, int cyl, int head, 
                          const uft_track_t* track, bool dirty);

/**
 * @brief Entfernt Track aus Cache
 * 
 * @param cache Cache-Handle
 * @param cyl Zylinder
 * @param head Kopf
 * @return UFT_OK wenn entfernt, UFT_ERROR wenn nicht gefunden
 */
uft_error_t uft_cache_remove(uft_cache_t* cache, int cyl, int head);

/**
 * @brief Markiert Track als dirty
 * 
 * @param cache Cache-Handle
 * @param cyl Zylinder
 * @param head Kopf
 * @return UFT_OK wenn gefunden und markiert
 */
uft_error_t uft_cache_mark_dirty(uft_cache_t* cache, int cyl, int head);

/**
 * @brief Invalidiert alle Einträge
 * 
 * Dirty Tracks werden NICHT geschrieben!
 * 
 * @param cache Cache-Handle
 */
void uft_cache_invalidate_all(uft_cache_t* cache);

/**
 * @brief Schreibt alle Dirty-Tracks (wenn Write-Back aktiv)
 * 
 * @param cache Cache-Handle
 * @param disk Ziel-Disk für Write-Back
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_cache_flush(uft_cache_t* cache, uft_disk_t* disk);

/**
 * @brief Holt Cache-Statistiken
 * 
 * @param cache Cache-Handle
 * @param stats Output für Statistiken
 */
void uft_cache_get_stats(uft_cache_t* cache, uft_cache_stats_t* stats);

/**
 * @brief Setzt Statistiken zurück
 * 
 * @param cache Cache-Handle
 */
void uft_cache_reset_stats(uft_cache_t* cache);

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Erstellt einfachen Cache mit Standard-Einstellungen
 * 
 * @param max_tracks Maximale Track-Anzahl
 * @return Cache-Handle
 */
static inline uft_cache_t* uft_cache_create_simple(size_t max_tracks) {
    uft_cache_config_t config = UFT_CACHE_CONFIG_DEFAULT;
    config.max_entries = max_tracks;
    return uft_cache_create(&config);
}

/**
 * @brief Hit-Rate als Prozent
 */
static inline double uft_cache_hit_rate_percent(const uft_cache_stats_t* stats) {
    if (stats->hits + stats->misses == 0) return 0.0;
    return (double)stats->hits * 100.0 / (double)(stats->hits + stats->misses);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CACHE_H */

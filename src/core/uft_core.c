/**
 * @file uft_core.c
 * @brief UnifiedFloppyTool - Core API Implementation
 * 
 * Implementiert die zentrale Disk/Track/Sector Abstraktion.
 * Dies ist das Herzstück von UFT.
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_decoder_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================================
// Global State
// ============================================================================

static bool g_initialized = false;
static uft_log_fn g_log_handler = NULL;
static void* g_log_user = NULL;

// Default Options (modifizierbar)
static uft_read_options_t g_default_read_options;
static uft_write_options_t g_default_write_options;

// ============================================================================
// Logging
// ============================================================================

static void log_message(uft_log_level_t level, const char* fmt, ...) {
    if (!g_log_handler) return;
    
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    g_log_handler(level, buffer, g_log_user);
}

#define LOG_DEBUG(...)  log_message(UFT_LOG_DEBUG, __VA_ARGS__)
#define LOG_INFO(...)   log_message(UFT_LOG_INFO, __VA_ARGS__)
#define LOG_WARN(...)   log_message(UFT_LOG_WARN, __VA_ARGS__)
#define LOG_ERROR(...)  log_message(UFT_LOG_ERROR, __VA_ARGS__)

// ============================================================================
// Initialization
// ============================================================================

uft_error_t uft_init(void) {
    if (g_initialized) {
        return UFT_OK;  // Bereits initialisiert
    }
    
    LOG_INFO("UFT initializing (version %s)", UFT_VERSION_STRING);
    
    // Default-Optionen setzen
    g_default_read_options = uft_default_read_options();
    g_default_write_options = uft_default_write_options();
    
    // Built-in Plugins registrieren
    uft_error_t err = uft_register_builtin_format_plugins();
    if (UFT_FAILED(err)) {
        LOG_ERROR("Failed to register format plugins: %s", uft_error_string(err));
        return err;
    }
    
    err = uft_register_builtin_decoder_plugins();
    if (UFT_FAILED(err)) {
        LOG_ERROR("Failed to register decoder plugins: %s", uft_error_string(err));
        return err;
    }
    
    g_initialized = true;
    LOG_INFO("UFT initialized successfully");
    
    return UFT_OK;
}

void uft_shutdown(void) {
    if (!g_initialized) return;
    
    LOG_INFO("UFT shutting down");
    
    // Hier würden wir Plugins entladen, Caches freigeben, etc.
    
    g_initialized = false;
    g_log_handler = NULL;
    g_log_user = NULL;
}

const char* uft_version(void) {
    return UFT_VERSION_STRING;
}

void uft_set_log_handler(uft_log_fn handler, void* user) {
    g_log_handler = handler;
    g_log_user = user;
}

uft_read_options_t* uft_get_default_read_options(void) {
    return &g_default_read_options;
}

uft_write_options_t* uft_get_default_write_options(void) {
    return &g_default_write_options;
}

// ============================================================================
// Disk Memory Management
// ============================================================================

/**
 * @brief Allokiert neue Disk-Struktur
 */
static uft_disk_t* disk_alloc(void) {
    uft_disk_t* disk = calloc(1, sizeof(uft_disk_t));
    if (!disk) return NULL;
    
    disk->format = UFT_FORMAT_UNKNOWN;
    disk->read_only = true;
    
    return disk;
}

/**
 * @brief Gibt Disk-Struktur frei
 */
static void disk_free(uft_disk_t* disk) {
    if (!disk) return;
    
    // Track-Cache freigeben
    if (disk->track_cache) {
        for (uint32_t i = 0; i < disk->cache_size; i++) {
            if (disk->track_cache[i]) {
                uft_track_free(disk->track_cache[i]);
            }
        }
        free(disk->track_cache);
    }
    
    // Pfad freigeben
    free(disk->path);
    
    // Plugin-Daten werden vom Plugin freigegeben
    
    free(disk);
}

// ============================================================================
// Disk Operations
// ============================================================================

uft_disk_t* uft_disk_open(const char* path, bool read_only) {
    return uft_disk_open_format(path, UFT_FORMAT_UNKNOWN, read_only);
}

uft_disk_t* uft_disk_open_format(const char* path, uft_format_t format, bool read_only) {
    if (!path) {
        uft_error_set_context(__FILE__, __LINE__, __func__, "path is NULL");
        return NULL;
    }
    
    if (!g_initialized) {
        uft_error_t err = uft_init();
        if (UFT_FAILED(err)) {
            return NULL;
        }
    }
    
    LOG_INFO("Opening disk: %s (format=%d, read_only=%d)", path, format, read_only);
    
    // Format ermitteln wenn nicht angegeben
    const uft_format_plugin_t* plugin = NULL;
    
    if (format == UFT_FORMAT_UNKNOWN) {
        // Auto-detect
        plugin = uft_find_format_plugin_for_file(path);
        if (!plugin) {
            LOG_ERROR("Cannot detect format for: %s", path);
            uft_error_set_context(__FILE__, __LINE__, __func__, "Format auto-detection failed");
            return NULL;
        }
        format = plugin->format;
        LOG_INFO("Auto-detected format: %s", plugin->name);
    } else {
        plugin = uft_get_format_plugin(format);
        if (!plugin) {
            LOG_ERROR("No plugin for format: %d", format);
            uft_error_set_context(__FILE__, __LINE__, __func__, "No plugin for format");
            return NULL;
        }
    }
    
    // Disk-Struktur allokieren
    uft_disk_t* disk = disk_alloc();
    if (!disk) {
        uft_error_set_context(__FILE__, __LINE__, __func__, "Out of memory");
        return NULL;
    }
    
    // H2 FIX: strdup() kann NULL zurückgeben
    disk->path = strdup(path);
    if (!disk->path) {
        disk_free(disk);
        uft_error_set_context(__FILE__, __LINE__, __func__, "strdup failed");
        return NULL;
    }
    disk->format = format;
    disk->read_only = read_only;
    
    // Plugin öffnen lassen
    uft_error_t err = plugin->open(disk, path, read_only);
    if (UFT_FAILED(err)) {
        LOG_ERROR("Plugin failed to open: %s (%s)", path, uft_error_string(err));
        disk_free(disk);
        return NULL;
    }
    
    // Track-Cache initialisieren - H4 FIX: calloc NULL Check
    size_t cache_size = (size_t)disk->geometry.cylinders * (size_t)disk->geometry.heads;
    if (cache_size > 0 && cache_size < 65536) {  // Sanity check
        disk->track_cache = calloc(cache_size, sizeof(uft_track_t*));
        if (disk->track_cache) {
            disk->cache_size = cache_size;
        } else {
            disk->cache_size = 0;  // Kein Cache, aber kein fataler Fehler
            LOG_WARN("Failed to allocate track cache (%zu entries)", cache_size);
        }
    } else {
        disk->cache_size = 0;
    }
    
    LOG_INFO("Disk opened successfully: %s (%s, %dx%dx%d)",
        path, plugin->name,
        disk->geometry.cylinders, disk->geometry.heads, disk->geometry.sectors);
    
    return disk;
}

uft_disk_t* uft_disk_create(const char* path, uft_format_t format,
                             const uft_geometry_t* geometry) {
    if (!path || !geometry) {
        uft_error_set_context(__FILE__, __LINE__, __func__, "Invalid arguments");
        return NULL;
    }
    
    if (!g_initialized) {
        uft_init();
    }
    
    const uft_format_plugin_t* plugin = uft_get_format_plugin(format);
    if (!plugin) {
        LOG_ERROR("No plugin for format: %d", format);
        return NULL;
    }
    
    if (!(plugin->capabilities & UFT_FORMAT_CAP_CREATE)) {
        LOG_ERROR("Plugin %s does not support creation", plugin->name);
        return NULL;
    }
    
    // Disk-Struktur allokieren
    uft_disk_t* disk = disk_alloc();
    if (!disk) return NULL;
    
    disk->path = strdup(path);
    disk->format = format;
    disk->geometry = *geometry;
    disk->read_only = false;
    
    // Plugin erstellen lassen
    uft_error_t err = plugin->create(disk, path, geometry);
    if (UFT_FAILED(err)) {
        LOG_ERROR("Plugin failed to create: %s", uft_error_string(err));
        disk_free(disk);
        return NULL;
    }
    
    // Track-Cache initialisieren
    size_t cache_size = geometry->cylinders * geometry->heads;
    disk->track_cache = calloc(cache_size, sizeof(uft_track_t*));
    disk->cache_size = cache_size;
    
    LOG_INFO("Disk created: %s (%s)", path, plugin->name);
    
    return disk;
}

void uft_disk_close(uft_disk_t* disk) {
    if (!disk) return;
    
    LOG_INFO("Closing disk: %s", disk->path ? disk->path : "(unknown)");
    
    // Plugin schließen lassen
    const uft_format_plugin_t* plugin = uft_get_format_plugin(disk->format);
    if (plugin && plugin->close) {
        plugin->close(disk);
    }
    
    disk_free(disk);
}

uft_format_t uft_disk_get_format(const uft_disk_t* disk) {
    return disk ? disk->format : UFT_FORMAT_UNKNOWN;
}

uft_error_t uft_disk_get_geometry(const uft_disk_t* disk, uft_geometry_t* geometry) {
    if (!disk || !geometry) {
        return UFT_ERROR_NULL_POINTER;
    }
    *geometry = disk->geometry;
    return UFT_OK;
}

const char* uft_disk_get_path(const uft_disk_t* disk) {
    return disk ? disk->path : NULL;
}

bool uft_disk_is_modified(const uft_disk_t* disk) {
    return disk ? disk->modified : false;
}

uft_error_t uft_disk_save(uft_disk_t* disk) {
    if (!disk) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    if (!disk->modified) return UFT_OK;
    
    const uft_format_plugin_t* plugin = uft_get_format_plugin(disk->format);
    if (!plugin || !plugin->flush) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    uft_error_t err = plugin->flush(disk);
    if (UFT_SUCCEEDED(err)) {
        disk->modified = false;
    }
    
    return err;
}

uft_error_t uft_disk_save_as(uft_disk_t* disk, const char* path, uft_format_t format) {
    if (!disk || !path) return UFT_ERROR_NULL_POINTER;
    
    // Neues Disk-Image erstellen
    uft_disk_t* new_disk = uft_disk_create(path, format, &disk->geometry);
    if (!new_disk) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Alle Tracks kopieren
    for (int cyl = 0; cyl < disk->geometry.cylinders; cyl++) {
        for (int head = 0; head < disk->geometry.heads; head++) {
            uft_track_t* track = uft_track_read(disk, cyl, head, NULL);
            if (track) {
                uft_track_write(new_disk, track, NULL);
                uft_track_free(track);
            }
        }
    }
    
    uft_disk_close(new_disk);
    return UFT_OK;
}

// ============================================================================
// Track Operations
// ============================================================================

/**
 * @brief Allokiert neue Track-Struktur
 */
static uft_track_t* track_alloc(int cylinder, int head) {
    uft_track_t* track = calloc(1, sizeof(uft_track_t));
    if (!track) return NULL;
    
    track->cylinder = cylinder;
    track->head = head;
    
    return track;
}

uft_track_t* uft_track_read(uft_disk_t* disk, int cylinder, int head,
                             const uft_read_options_t* options) {
    if (!disk) return NULL;
    
    // Bounds check
    if (cylinder < 0 || cylinder >= disk->geometry.cylinders ||
        head < 0 || head >= disk->geometry.heads) {
        uft_error_set_context(__FILE__, __LINE__, __func__, "Track out of range");
        return NULL;
    }
    
    /* Check LRU cache first (using proper cache API if available) */
    size_t cache_idx = (size_t)cylinder * (size_t)disk->geometry.heads + (size_t)head;
    if (disk->track_cache && cache_idx < disk->cache_size) {
        if (disk->track_cache[cache_idx]) {
            /* Cache hit - return copy of cached track */
            uft_track_t* cached = disk->track_cache[cache_idx];
            uft_track_t* copy = track_alloc(cylinder, head);
            if (copy) {
                copy->disk = disk;
                copy->cylinder = cached->cylinder;
                copy->head = cached->head;
                copy->status = cached->status;
                copy->encoding = cached->encoding;
                
                /* Copy sector array if present */
                if (cached->sector_count > 0 && cached->sectors) {
                    copy->sectors = (uft_sector_t*)calloc(cached->sector_count, sizeof(uft_sector_t));
                    if (copy->sectors) {
                        memcpy(copy->sectors, cached->sectors, 
                               cached->sector_count * sizeof(uft_sector_t));
                        copy->sector_count = cached->sector_count;
                    }
                }
                
                /* Copy raw data if present */
                if (cached->raw_size > 0 && cached->raw_data) {
                    copy->raw_data = (uint8_t*)malloc(cached->raw_size);
                    if (copy->raw_data) {
                        memcpy(copy->raw_data, cached->raw_data, cached->raw_size);
                        copy->raw_size = cached->raw_size;
                    }
                }
                
                return copy;
            }
            /* Fall through if copy failed */
        }
    }
    
    // Plugin lesen lassen
    const uft_format_plugin_t* plugin = uft_get_format_plugin(disk->format);
    if (!plugin || !plugin->read_track) {
        uft_error_set_context(__FILE__, __LINE__, __func__, "Plugin cannot read tracks");
        return NULL;
    }
    
    uft_track_t* track = track_alloc(cylinder, head);
    if (!track) return NULL;
    
    track->disk = disk;
    
    uft_error_t err = plugin->read_track(disk, cylinder, head, track);
    if (UFT_FAILED(err)) {
        LOG_WARN("Read error at C%d H%d: %s", cylinder, head, uft_error_string(err));
        track->status |= UFT_TRACK_READ_ERROR;
        // Wir geben den Track trotzdem zurück - mit Error-Status
    }
    
    /* Store in cache for future reads (if no error) */
    if (UFT_SUCCEEDED(err) && disk->track_cache && cache_idx < disk->cache_size) {
        /* Free old entry if present */
        if (disk->track_cache[cache_idx]) {
            uft_track_free(disk->track_cache[cache_idx]);
        }
        
        /* Create copy for cache */
        uft_track_t* cache_copy = track_alloc(cylinder, head);
        if (cache_copy) {
            cache_copy->disk = disk;
            cache_copy->cylinder = track->cylinder;
            cache_copy->head = track->head;
            cache_copy->status = track->status;
            cache_copy->encoding = track->encoding;
            
            if (track->sector_count > 0 && track->sectors) {
                cache_copy->sectors = (uft_sector_t*)calloc(track->sector_count, sizeof(uft_sector_t));
                if (cache_copy->sectors) {
                    memcpy(cache_copy->sectors, track->sectors,
                           track->sector_count * sizeof(uft_sector_t));
                    cache_copy->sector_count = track->sector_count;
                }
            }
            
            if (track->raw_size > 0 && track->raw_data) {
                cache_copy->raw_data = (uint8_t*)malloc(track->raw_size);
                if (cache_copy->raw_data) {
                    memcpy(cache_copy->raw_data, track->raw_data, track->raw_size);
                    cache_copy->raw_size = track->raw_size;
                }
            }
            
            disk->track_cache[cache_idx] = cache_copy;
        }
    }
    
    return track;
}

uft_error_t uft_track_write(uft_disk_t* disk, const uft_track_t* track,
                             const uft_write_options_t* options) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    
    const uft_format_plugin_t* plugin = uft_get_format_plugin(disk->format);
    if (!plugin || !plugin->write_track) {
        return UFT_ERROR_NOT_SUPPORTED;
    }
    
    uft_error_t err = plugin->write_track(disk, track->cylinder, track->head, track);
    if (UFT_SUCCEEDED(err)) {
        disk->modified = true;
        
        // Cache invalidieren
        size_t cache_idx = track->cylinder * disk->geometry.heads + track->head;
        if (disk->track_cache && cache_idx < disk->cache_size) {
            if (disk->track_cache[cache_idx]) {
                uft_track_free(disk->track_cache[cache_idx]);
                disk->track_cache[cache_idx] = NULL;
            }
        }
    }
    
    return err;
}

void uft_track_free(uft_track_t* track) {
    if (!track) return;
    
    // Sektoren freigeben
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    
    // Flux-Daten freigeben
    free(track->flux);
    
    // Raw-Daten freigeben
    free(track->raw_data);
    
    // Plugin-Daten
    free(track->plugin_data);
    
    free(track);
}

void uft_track_get_position(const uft_track_t* track, int* cylinder, int* head) {
    if (!track) return;
    if (cylinder) *cylinder = track->cylinder;
    if (head) *head = track->head;
}

size_t uft_track_get_sector_count(const uft_track_t* track) {
    return track ? track->sector_count : 0;
}

const uft_sector_t* uft_track_get_sector(const uft_track_t* track, size_t index) {
    if (!track || index >= track->sector_count) return NULL;
    return &track->sectors[index];
}

const uft_sector_t* uft_track_find_sector(const uft_track_t* track, int sector) {
    if (!track) return NULL;
    
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].id.sector == sector) {
            return &track->sectors[i];
        }
    }
    
    return NULL;
}

uint32_t uft_track_get_status(const uft_track_t* track) {
    return track ? track->status : 0;
}

uft_error_t uft_track_get_metrics(const uft_track_t* track, uft_track_metrics_t* metrics) {
    if (!track || !metrics) return UFT_ERROR_NULL_POINTER;
    *metrics = track->metrics;
    return UFT_OK;
}

bool uft_track_has_flux(const uft_track_t* track) {
    return track && track->flux && track->flux_count > 0;
}

uft_error_t uft_track_get_flux(const uft_track_t* track, 
                                uint32_t** flux_out, size_t* count_out) {
    if (!track || !flux_out || !count_out) return UFT_ERROR_NULL_POINTER;
    if (!track->flux || track->flux_count == 0) return UFT_ERROR_NOT_SUPPORTED;
    
    // Kopie erstellen
    uint32_t* copy = malloc(track->flux_count * sizeof(uint32_t));
    if (!copy) return UFT_ERROR_NO_MEMORY;
    
    memcpy(copy, track->flux, track->flux_count * sizeof(uint32_t));
    
    *flux_out = copy;
    *count_out = track->flux_count;
    
    return UFT_OK;
}

// ============================================================================
// Sector Operations
// ============================================================================

int uft_sector_read(uft_disk_t* disk, int cylinder, int head, int sector,
                    uint8_t* buffer, size_t buffer_size) {
    if (!disk || !buffer) return UFT_ERROR_NULL_POINTER;
    
    uft_track_t* track = uft_track_read(disk, cylinder, head, NULL);
    if (!track) return UFT_ERROR_TRACK_NOT_FOUND;
    
    const uft_sector_t* sec = uft_track_find_sector(track, sector);
    if (!sec) {
        uft_track_free(track);
        return UFT_ERROR_SECTOR_NOT_FOUND;
    }
    
    size_t copy_size = sec->data_size;
    if (copy_size > buffer_size) copy_size = buffer_size;
    
    memcpy(buffer, sec->data, copy_size);
    
    int result = (sec->status & UFT_SECTOR_CRC_ERROR) ? 
                 UFT_ERROR_CRC_ERROR : (int)copy_size;
    
    uft_track_free(track);
    return result;
}

uft_error_t uft_sector_write(uft_disk_t* disk, int cylinder, int head, int sector,
                              const uint8_t* data, size_t data_size) {
    if (!disk || !data) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    
    // Track lesen
    uft_track_t* track = uft_track_read(disk, cylinder, head, NULL);
    if (!track) return UFT_ERROR_TRACK_NOT_FOUND;
    
    // Sektor finden und modifizieren
    bool found = false;
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].id.sector == sector) {
            // Daten ersetzen
            if (track->sectors[i].data_size >= data_size) {
                memcpy(track->sectors[i].data, data, data_size);
            } else {
                free(track->sectors[i].data);
                track->sectors[i].data = malloc(data_size);
                if (!track->sectors[i].data) {
                    uft_track_free(track);
                    return UFT_ERROR_NO_MEMORY;
                }
                memcpy(track->sectors[i].data, data, data_size);
                track->sectors[i].data_size = data_size;
            }
            track->sectors[i].status &= ~UFT_SECTOR_CRC_ERROR;
            found = true;
            break;
        }
    }
    
    if (!found) {
        uft_track_free(track);
        return UFT_ERROR_SECTOR_NOT_FOUND;
    }
    
    // Track zurückschreiben
    uft_error_t err = uft_track_write(disk, track, NULL);
    uft_track_free(track);
    
    return err;
}

// ============================================================================
// Block Operations (LBA)
// ============================================================================

uint32_t uft_chs_to_lba(const uft_geometry_t* geo, int cyl, int head, int sector) {
    if (!geo) return 0;
    return (cyl * geo->heads + head) * geo->sectors + sector;
}

void uft_lba_to_chs(const uft_geometry_t* geo, uint32_t lba,
                    int* cyl, int* head, int* sector) {
    // K5 FIX: Output-Pointer prüfen und initialisieren
    if (!cyl || !head || !sector) return;
    
    // Default-Werte bei Fehler
    *cyl = 0;
    *head = 0;
    *sector = 0;
    
    if (!geo || geo->sectors == 0 || geo->heads == 0) return;
    
    *sector = lba % geo->sectors;
    uint32_t temp = lba / geo->sectors;
    *head = temp % geo->heads;
    *cyl = temp / geo->heads;
}

int uft_block_read(uft_disk_t* disk, uint32_t lba, uint8_t* buffer, size_t buffer_size) {
    if (!disk) return UFT_ERROR_NULL_POINTER;
    
    int cyl, head, sector;
    uft_lba_to_chs(&disk->geometry, lba, &cyl, &head, &sector);
    
    return uft_sector_read(disk, cyl, head, sector, buffer, buffer_size);
}

uft_error_t uft_block_write(uft_disk_t* disk, uint32_t lba, 
                             const uint8_t* data, size_t data_size) {
    if (!disk) return UFT_ERROR_NULL_POINTER;
    
    int cyl, head, sector;
    uft_lba_to_chs(&disk->geometry, lba, &cyl, &head, &sector);
    
    return uft_sector_write(disk, cyl, head, sector, data, data_size);
}

// ============================================================================
// Analysis
// ============================================================================

uft_error_t uft_analyze(uft_disk_t* disk, uft_analysis_t* analysis,
                         uft_progress_fn progress, void* user) {
    if (!disk || !analysis) return UFT_ERROR_NULL_POINTER;
    
    memset(analysis, 0, sizeof(*analysis));
    analysis->detected_format = disk->format;
    analysis->detected_geometry = disk->geometry;
    
    // K4 FIX: Division-by-Zero Check
    if (disk->geometry.cylinders == 0 || disk->geometry.heads == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    uint32_t total_tracks = (uint32_t)disk->geometry.cylinders * (uint32_t)disk->geometry.heads;
    if (total_tracks == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    uint32_t readable_tracks = 0;
    uint32_t total_sectors = 0;
    uint32_t readable_sectors = 0;
    uint32_t crc_errors = 0;
    
    for (int cyl = 0; cyl < disk->geometry.cylinders; cyl++) {
        for (int head = 0; head < disk->geometry.heads; head++) {
            // Progress
            if (progress) {
                int pct = (cyl * disk->geometry.heads + head) * 100 / total_tracks;
                if (!progress(cyl, head, pct, "Analyzing...", user)) {
                    return UFT_ERROR_CANCELLED;
                }
            }
            
            uft_track_t* track = uft_track_read(disk, cyl, head, NULL);
            if (track) {
                if (!(track->status & UFT_TRACK_READ_ERROR)) {
                    readable_tracks++;
                }
                
                total_sectors += track->sector_count;
                
                for (size_t i = 0; i < track->sector_count; i++) {
                    if (!(track->sectors[i].status & UFT_SECTOR_CRC_ERROR)) {
                        readable_sectors++;
                    } else {
                        crc_errors++;
                    }
                    
                    if (track->sectors[i].status & UFT_SECTOR_WEAK) {
                        analysis->has_weak_bits = true;
                    }
                }
                
                if (track->status & UFT_TRACK_PROTECTED) {
                    analysis->has_copy_protection = true;
                }
                
                uft_track_free(track);
            }
        }
    }
    
    analysis->total_tracks = total_tracks;
    analysis->readable_tracks = readable_tracks;
    analysis->total_sectors = total_sectors;
    analysis->readable_sectors = readable_sectors;
    analysis->crc_errors = crc_errors;
    
    // Uniform check
    analysis->is_uniform = (readable_tracks == total_tracks) && (crc_errors == 0);
    
    if (progress) {
        progress(0, 0, 100, "Analysis complete", user);
    }
    
    return UFT_OK;
}

uft_format_t uft_detect_format(const char* path, int* confidence) {
    if (!path) return UFT_FORMAT_UNKNOWN;
    
    const uft_format_plugin_t* plugin = uft_find_format_plugin_for_file(path);
    if (plugin) {
        if (confidence) *confidence = 100;  // Plugin hat sich gemeldet
        return plugin->format;
    }
    
    if (confidence) *confidence = 0;
    return UFT_FORMAT_UNKNOWN;
}

// ============================================================================
// Conversion
// ============================================================================

bool uft_can_convert(uft_format_t from, uft_format_t to) {
    // Grundregel: Wenn wir lesen und schreiben können, können wir konvertieren
    const uft_format_plugin_t* src = uft_get_format_plugin(from);
    const uft_format_plugin_t* dst = uft_get_format_plugin(to);
    
    if (!src || !dst) return false;
    if (!(src->capabilities & UFT_FORMAT_CAP_READ)) return false;
    if (!(dst->capabilities & UFT_FORMAT_CAP_WRITE)) return false;
    
    // Flux → Sector verliert Information
    // Sector → Flux geht nicht (ohne Encoder)
    // Beides Flux oder beides Sector: OK
    
    return true;
}

uft_error_t uft_convert(uft_disk_t* src, const char* dst_path,
                         const uft_convert_options_t* options) {
    if (!src || !dst_path || !options) return UFT_ERROR_NULL_POINTER;
    
    // Ziel erstellen
    uft_geometry_t geo = options->target_geometry;
    if (geo.cylinders == 0) {
        geo = src->geometry;  // Geometrie übernehmen
    }
    
    // Sanity check
    if (geo.cylinders == 0 || geo.heads == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    uft_disk_t* dst = uft_disk_create(dst_path, options->target_format, &geo);
    if (!dst) return UFT_ERROR_FILE_OPEN;
    
    uint32_t total = (uint32_t)geo.cylinders * (uint32_t)geo.heads;
    uint32_t write_errors = 0;
    
    for (int cyl = 0; cyl < geo.cylinders; cyl++) {
        for (int head = 0; head < geo.heads; head++) {
            if (options->progress) {
                int pct = (cyl * geo.heads + head) * 100 / total;
                if (!options->progress(cyl, head, pct, "Converting...", 
                                        options->progress_user)) {
                    uft_disk_close(dst);
                    return UFT_ERROR_CANCELLED;
                }
            }
            
            uft_track_t* track = uft_track_read(src, cyl, head, NULL);
            if (track) {
                // H3 FIX: Write-Fehler prüfen
                uft_error_t write_err = uft_track_write(dst, track, NULL);
                if (UFT_FAILED(write_err)) {
                    write_errors++;
                    LOG_WARN("Write error at C%d H%d: %s", 
                             cyl, head, uft_error_string(write_err));
                }
                uft_track_free(track);
            }
        }
    }
    
    uft_disk_close(dst);
    
    // Bei zu vielen Fehlern abbrechen
    if (write_errors > total / 4) {  // >25% Fehler = Fehlschlag
        LOG_ERROR("Conversion failed: %u write errors", write_errors);
        return UFT_ERROR_FILE_WRITE;
    }
    
    if (options->progress) {
        options->progress(0, 0, 100, "Conversion complete", options->progress_user);
    }
    
    return UFT_OK;
}

// ============================================================================
// Format Registry (Forwards to plugin system)
// ============================================================================

size_t uft_list_formats(const uft_format_info_t** formats, size_t max) {
    if (!formats || max == 0) return 0;
    
    size_t count = 0;
    for (int i = 1; i < UFT_FORMAT_MAX && count < max; i++) {
        const uft_format_plugin_t* plugin = uft_get_format_plugin(i);
        if (plugin) {
            formats[count++] = uft_format_get_info(i);
        }
    }
    
    return count;
}

uft_format_t uft_format_for_extension(const char* ext) {
    return uft_format_from_extension(ext);
}

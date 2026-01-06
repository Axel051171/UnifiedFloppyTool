/**
 * @file uft_format_plugin.c
 * @brief UnifiedFloppyTool - Format Plugin Registry
 * 
 * Verwaltet Format-Plugins und bietet Auto-Detection.
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Plugin Registry
// ============================================================================

#define MAX_FORMAT_PLUGINS 32

static const uft_format_plugin_t* g_format_plugins[MAX_FORMAT_PLUGINS] = {0};
static size_t g_format_plugin_count = 0;

// ============================================================================
// Registration
// ============================================================================

uft_error_t uft_register_format_plugin(const uft_format_plugin_t* plugin) {
    if (!plugin) return UFT_ERROR_NULL_POINTER;
    if (!plugin->name) return UFT_ERROR_INVALID_ARG;
    
    // Duplikate prüfen
    for (size_t i = 0; i < g_format_plugin_count; i++) {
        if (g_format_plugins[i]->format == plugin->format) {
            // Plugin für dieses Format bereits registriert
            return UFT_ERROR_PLUGIN_LOAD;
        }
    }
    
    if (g_format_plugin_count >= MAX_FORMAT_PLUGINS) {
        return UFT_ERROR_BUFFER_TOO_SMALL;
    }
    
    // Plugin initialisieren
    if (plugin->init) {
        uft_error_t err = plugin->init();
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    g_format_plugins[g_format_plugin_count++] = plugin;
    
    return UFT_OK;
}

uft_error_t uft_unregister_format_plugin(uft_format_t format) {
    for (size_t i = 0; i < g_format_plugin_count; i++) {
        if (g_format_plugins[i]->format == format) {
            // Plugin shutdown
            if (g_format_plugins[i]->shutdown) {
                g_format_plugins[i]->shutdown();
            }
            
            // Lücke schließen
            for (size_t j = i; j < g_format_plugin_count - 1; j++) {
                g_format_plugins[j] = g_format_plugins[j + 1];
            }
            g_format_plugin_count--;
            g_format_plugins[g_format_plugin_count] = NULL;
            
            return UFT_OK;
        }
    }
    
    return UFT_ERROR_PLUGIN_NOT_FOUND;
}

// ============================================================================
// Lookup
// ============================================================================

const uft_format_plugin_t* uft_get_format_plugin(uft_format_t format) {
    for (size_t i = 0; i < g_format_plugin_count; i++) {
        if (g_format_plugins[i]->format == format) {
            return g_format_plugins[i];
        }
    }
    return NULL;
}

const uft_format_plugin_t* uft_find_format_plugin_by_extension(const char* ext) {
    if (!ext || !*ext) return NULL;
    
    // Punkt überspringen
    if (*ext == '.') ext++;
    
    for (size_t i = 0; i < g_format_plugin_count; i++) {
        const char* exts = g_format_plugins[i]->extensions;
        if (!exts) continue;
        
        // Extensions parsen (semikolon-getrennt)
        char buffer[256];
        strncpy(buffer, exts, sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        
        char* token = strtok(buffer, ";");
        while (token) {
            if (strcasecmp(token, ext) == 0) {
                return g_format_plugins[i];
            }
            token = strtok(NULL, ";");
        }
    }
    
    return NULL;
}

const uft_format_plugin_t* uft_find_format_plugin_for_file(const char* path) {
    if (!path) return NULL;
    
    // Datei öffnen und erste Bytes lesen
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    // Dateigröße ermitteln
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    size_t file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    // Header lesen (mind. 512 Bytes für die meisten Formate)
    uint8_t header[4096];
    size_t header_size = fread(header, 1, sizeof(header), f);
    fclose(f);
    
    if (header_size < 16) {
        // Zu wenig Daten
        return NULL;
    }
    
    // Alle Plugins proben
    const uft_format_plugin_t* best_plugin = NULL;
    int best_confidence = 0;
    
    for (size_t i = 0; i < g_format_plugin_count; i++) {
        if (!g_format_plugins[i]->probe) continue;
        
        int confidence = 0;
        bool matches = g_format_plugins[i]->probe(header, header_size, 
                                                   file_size, &confidence);
        
        if (matches && confidence > best_confidence) {
            best_plugin = g_format_plugins[i];
            best_confidence = confidence;
        }
    }
    
    // Fallback auf Extension
    if (!best_plugin) {
        const char* ext = strrchr(path, '.');
        if (ext) {
            best_plugin = uft_find_format_plugin_by_extension(ext);
        }
    }
    
    return best_plugin;
}

size_t uft_list_format_plugins(const uft_format_plugin_t** plugins, size_t max) {
    if (!plugins || max == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < g_format_plugin_count && count < max; i++) {
        plugins[count++] = g_format_plugins[i];
    }
    
    return count;
}

// ============================================================================
// Track Helpers
// ============================================================================

void uft_track_init(uft_track_t* track, int cylinder, int head) {
    if (!track) return;
    
    memset(track, 0, sizeof(*track));
    track->cylinder = cylinder;
    track->head = head;
}

uft_error_t uft_track_add_sector(uft_track_t* track, const uft_sector_t* sector) {
    if (!track || !sector) return UFT_ERROR_NULL_POINTER;
    
    // Kapazität prüfen/erweitern
    if (track->sector_count >= track->sector_capacity) {
        size_t new_cap = track->sector_capacity ? track->sector_capacity * 2 : 32;
        uft_sector_t* new_sectors = realloc(track->sectors, 
                                             new_cap * sizeof(uft_sector_t));
        if (!new_sectors) return UFT_ERROR_NO_MEMORY;
        
        track->sectors = new_sectors;
        track->sector_capacity = new_cap;
    }
    
    // Sektor kopieren
    uft_sector_t* dst = &track->sectors[track->sector_count];
    *dst = *sector;
    
    // Daten kopieren
    if (sector->data && sector->data_size > 0) {
        dst->data = malloc(sector->data_size);
        if (!dst->data) return UFT_ERROR_NO_MEMORY;
        memcpy(dst->data, sector->data, sector->data_size);
    }
    
    track->sector_count++;
    return UFT_OK;
}

uft_error_t uft_track_set_flux(uft_track_t* track, const uint32_t* flux, 
                                size_t count, uint32_t tick_ns) {
    if (!track) return UFT_ERROR_NULL_POINTER;
    
    // Alte Daten freigeben
    free(track->flux);
    track->flux = NULL;
    track->flux_count = 0;
    
    if (flux && count > 0) {
        track->flux = malloc(count * sizeof(uint32_t));
        if (!track->flux) return UFT_ERROR_NO_MEMORY;
        
        memcpy(track->flux, flux, count * sizeof(uint32_t));
        track->flux_count = count;
        track->flux_tick_ns = tick_ns;
    }
    
    return UFT_OK;
}

void uft_track_cleanup(uft_track_t* track) {
    if (!track) return;
    
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    
    free(track->flux);
    free(track->raw_data);
    
    memset(track, 0, sizeof(*track));
}

uft_error_t uft_sector_copy(uft_sector_t* dst, const uft_sector_t* src) {
    if (!dst || !src) return UFT_ERROR_NULL_POINTER;
    
    *dst = *src;
    dst->data = NULL;
    
    if (src->data && src->data_size > 0) {
        dst->data = malloc(src->data_size);
        if (!dst->data) return UFT_ERROR_NO_MEMORY;
        memcpy(dst->data, src->data, src->data_size);
    }
    
    return UFT_OK;
}

void uft_sector_cleanup(uft_sector_t* sector) {
    if (!sector) return;
    free(sector->data);
    memset(sector, 0, sizeof(*sector));
}

// ============================================================================
// Built-in Plugin Registration
// ============================================================================

// Forward declarations für Built-in Plugins
extern const uft_format_plugin_t uft_format_plugin_adf;
extern const uft_format_plugin_t uft_format_plugin_img;
extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_g64;
extern const uft_format_plugin_t uft_format_plugin_hfe;
extern const uft_format_plugin_t uft_format_plugin_scp;

uft_error_t uft_register_builtin_format_plugins(void) {
    uft_error_t err;
    
    // ADF Plugin
    err = uft_register_format_plugin(&uft_format_plugin_adf);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        return err;
    }
    
    // IMG Plugin
    err = uft_register_format_plugin(&uft_format_plugin_img);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        return err;
    }
    
    // D64 Plugin (Commodore 64)
    err = uft_register_format_plugin(&uft_format_plugin_d64);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        return err;
    }
    
    // G64 Plugin (Commodore 64 GCR Raw)
    err = uft_register_format_plugin(&uft_format_plugin_g64);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        return err;
    }
    
    // HFE Plugin (HxC Floppy Emulator)
    err = uft_register_format_plugin(&uft_format_plugin_hfe);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        return err;
    }
    
    // SCP Plugin (SuperCard Pro Flux)
    err = uft_register_format_plugin(&uft_format_plugin_scp);
    if (UFT_FAILED(err) && err != UFT_ERROR_PLUGIN_LOAD) {
        return err;
    }
    
    return UFT_OK;
}

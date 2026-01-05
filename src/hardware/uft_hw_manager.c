/**
 * @file uft_hw_manager.c
 * @brief UnifiedFloppyTool - Hardware Backend Manager
 * 
 * Verwaltet die Registrierung und Aktivierung von Hardware-Backends.
 * Ermöglicht das Ein- und Ausschalten einzelner Backends zur Laufzeit.
 * 
 * FEATURES:
 * - Dynamische Backend-Registrierung
 * - Backend aktivieren/deaktivieren
 * - Konfigurations-Persistenz (optional)
 * - Backend-Prioritäten
 * 
 * BEISPIEL:
 *   // Nibtools deaktivieren
 *   uft_hw_backend_set_enabled(UFT_HW_XUM1541, false);
 *   
 *   // Nur Flux-Hardware aktivieren
 *   uft_hw_backend_disable_all();
 *   uft_hw_backend_set_enabled(UFT_HW_GREASEWEAZLE, true);
 *   uft_hw_backend_set_enabled(UFT_HW_KRYOFLUX, true);
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_hardware.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Backend Registry
// ============================================================================

#define MAX_BACKENDS 32

typedef struct {
    const uft_hw_backend_t* backend;
    bool                    enabled;
    bool                    initialized;
    int                     priority;       // Höher = bevorzugt
} backend_entry_t;

static backend_entry_t g_backends[MAX_BACKENDS] = {0};
static size_t g_backend_count = 0;
static bool g_manager_initialized = false;

// ============================================================================
// Externe Backend-Deklarationen
// ============================================================================

// Diese werden in den jeweiligen Backend-Dateien definiert
extern const uft_hw_backend_t uft_hw_backend_greaseweazle;
extern const uft_hw_backend_t uft_hw_backend_opencbm;
extern const uft_hw_backend_t uft_hw_backend_fc5025;
extern const uft_hw_backend_t uft_hw_backend_kryoflux;
extern const uft_hw_backend_t uft_hw_backend_supercard;

// ============================================================================
// Default Backend Configuration
// ============================================================================

typedef struct {
    uft_hw_type_t   type;
    bool            default_enabled;
    int             priority;
    const char*     name;
} backend_default_t;

static const backend_default_t g_default_backends[] = {
    // Flux-Hardware (höchste Priorität)
    { UFT_HW_KRYOFLUX,      true,   100,    "KryoFlux" },
    { UFT_HW_SUPERCARD_PRO, true,   90,     "SuperCard Pro" },
    { UFT_HW_GREASEWEAZLE,  true,   80,     "Greaseweazle" },
    { UFT_HW_FLUXENGINE,    true,   70,     "FluxEngine" },
    
    // Spezial-Hardware (mittlere Priorität)
    { UFT_HW_FC5025,        true,   50,     "FC5025" },
    { UFT_HW_XUM1541,       true,   40,     "OpenCBM/Nibtools" },
    { UFT_HW_ZOOMFLOPPY,    true,   45,     "ZoomFloppy" },
    
    // Legacy (niedrige Priorität)
    { UFT_HW_CATWEASEL,     false,  10,     "CatWeasel" },
    
    // Ende-Marker
    { UFT_HW_UNKNOWN,       false,  0,      NULL }
};

// ============================================================================
// Helper Functions
// ============================================================================

static backend_entry_t* find_backend_entry(uft_hw_type_t type) {
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i].backend && g_backends[i].backend->type == type) {
            return &g_backends[i];
        }
    }
    return NULL;
}

static int get_default_priority(uft_hw_type_t type) {
    for (const backend_default_t* def = g_default_backends; def->name; def++) {
        if (def->type == type) {
            return def->priority;
        }
    }
    return 0;
}

static bool get_default_enabled(uft_hw_type_t type) {
    for (const backend_default_t* def = g_default_backends; def->name; def++) {
        if (def->type == type) {
            return def->default_enabled;
        }
    }
    return true;  // Default: aktiviert
}

// ============================================================================
// Public API - Backend Management
// ============================================================================

/**
 * @brief Hardware Manager initialisieren
 * 
 * Registriert alle verfügbaren Backends mit Default-Einstellungen.
 */
uft_error_t uft_hw_manager_init(void) {
    if (g_manager_initialized) {
        return UFT_OK;
    }
    
    memset(g_backends, 0, sizeof(g_backends));
    g_backend_count = 0;
    
    // Builtin Backends registrieren
    // (werden später bei Bedarf geladen)
    
    g_manager_initialized = true;
    return UFT_OK;
}

/**
 * @brief Hardware Manager herunterfahren
 */
void uft_hw_manager_shutdown(void) {
    // Alle initialisierten Backends herunterfahren
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i].initialized && g_backends[i].backend->shutdown) {
            g_backends[i].backend->shutdown();
            g_backends[i].initialized = false;
        }
    }
    
    g_backend_count = 0;
    g_manager_initialized = false;
}

/**
 * @brief Backend registrieren
 */
uft_error_t uft_hw_manager_register(const uft_hw_backend_t* backend) {
    if (!backend) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (!g_manager_initialized) {
        uft_hw_manager_init();
    }
    
    // Duplikat prüfen
    if (find_backend_entry(backend->type)) {
        return UFT_OK;  // Bereits registriert
    }
    
    if (g_backend_count >= MAX_BACKENDS) {
        return UFT_ERROR_BUFFER_TOO_SMALL;
    }
    
    backend_entry_t* entry = &g_backends[g_backend_count++];
    entry->backend = backend;
    entry->enabled = get_default_enabled(backend->type);
    entry->initialized = false;
    entry->priority = get_default_priority(backend->type);
    
    return UFT_OK;
}

/**
 * @brief Backend aktivieren/deaktivieren
 * 
 * @param type Hardware-Typ
 * @param enabled true = aktivieren, false = deaktivieren
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_hw_backend_set_enabled(uft_hw_type_t type, bool enabled) {
    backend_entry_t* entry = find_backend_entry(type);
    if (!entry) {
        return UFT_ERROR_PLUGIN_NOT_FOUND;
    }
    
    // Wenn deaktiviert und initialisiert -> herunterfahren
    if (!enabled && entry->initialized) {
        if (entry->backend->shutdown) {
            entry->backend->shutdown();
        }
        entry->initialized = false;
    }
    
    entry->enabled = enabled;
    return UFT_OK;
}

/**
 * @brief Prüft ob Backend aktiviert ist
 */
bool uft_hw_backend_is_enabled(uft_hw_type_t type) {
    backend_entry_t* entry = find_backend_entry(type);
    return entry ? entry->enabled : false;
}

/**
 * @brief Alle Backends deaktivieren
 */
void uft_hw_backend_disable_all(void) {
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i].initialized && g_backends[i].backend->shutdown) {
            g_backends[i].backend->shutdown();
            g_backends[i].initialized = false;
        }
        g_backends[i].enabled = false;
    }
}

/**
 * @brief Alle Backends aktivieren
 */
void uft_hw_backend_enable_all(void) {
    for (size_t i = 0; i < g_backend_count; i++) {
        g_backends[i].enabled = true;
    }
}

/**
 * @brief Backend-Priorität setzen
 * 
 * Höhere Priorität = wird bei enumerate() zuerst aufgelistet
 */
uft_error_t uft_hw_backend_set_priority(uft_hw_type_t type, int priority) {
    backend_entry_t* entry = find_backend_entry(type);
    if (!entry) {
        return UFT_ERROR_PLUGIN_NOT_FOUND;
    }
    
    entry->priority = priority;
    return UFT_OK;
}

/**
 * @brief Liste aller registrierten Backends
 */
size_t uft_hw_backend_list(uft_hw_type_t* types, bool* enabled, 
                           size_t max_count) {
    size_t count = 0;
    
    for (size_t i = 0; i < g_backend_count && count < max_count; i++) {
        if (g_backends[i].backend) {
            if (types) types[count] = g_backends[i].backend->type;
            if (enabled) enabled[count] = g_backends[i].enabled;
            count++;
        }
    }
    
    return count;
}

// ============================================================================
// Device Enumeration (nur aktivierte Backends)
// ============================================================================

/**
 * @brief Geräte auflisten (nur aktivierte Backends)
 */
uft_error_t uft_hw_manager_enumerate(uft_hw_info_t* devices, size_t max_devices,
                                      size_t* found) {
    if (!devices || !found) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    *found = 0;
    
    // Nach Priorität sortieren (höhere zuerst)
    backend_entry_t* sorted[MAX_BACKENDS];
    size_t sorted_count = 0;
    
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i].enabled && g_backends[i].backend) {
            sorted[sorted_count++] = &g_backends[i];
        }
    }
    
    // Simple Bubble Sort (wenige Einträge)
    for (size_t i = 0; i < sorted_count; i++) {
        for (size_t j = i + 1; j < sorted_count; j++) {
            if (sorted[j]->priority > sorted[i]->priority) {
                backend_entry_t* tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }
    
    // Backends durchgehen
    for (size_t i = 0; i < sorted_count && *found < max_devices; i++) {
        backend_entry_t* entry = sorted[i];
        
        // Initialisieren falls nötig
        if (!entry->initialized) {
            if (entry->backend->init) {
                if (UFT_OK == entry->backend->init()) {
                    entry->initialized = true;
                } else {
                    continue;  // Backend konnte nicht initialisiert werden
                }
            } else {
                entry->initialized = true;
            }
        }
        
        // Enumerate
        if (entry->backend->enumerate) {
            size_t backend_found = 0;
            entry->backend->enumerate(&devices[*found], 
                                      max_devices - *found,
                                      &backend_found);
            *found += backend_found;
        }
    }
    
    return UFT_OK;
}

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 * @brief Nur Nibtools/OpenCBM aktivieren
 */
void uft_hw_use_nibtools_only(void) {
    uft_hw_backend_disable_all();
    uft_hw_backend_set_enabled(UFT_HW_XUM1541, true);
    uft_hw_backend_set_enabled(UFT_HW_ZOOMFLOPPY, true);
}

/**
 * @brief Nur Flux-Hardware aktivieren
 */
void uft_hw_use_flux_only(void) {
    uft_hw_backend_disable_all();
    uft_hw_backend_set_enabled(UFT_HW_GREASEWEAZLE, true);
    uft_hw_backend_set_enabled(UFT_HW_KRYOFLUX, true);
    uft_hw_backend_set_enabled(UFT_HW_SUPERCARD_PRO, true);
    uft_hw_backend_set_enabled(UFT_HW_FLUXENGINE, true);
}

/**
 * @brief Alle Standard-Backends aktivieren
 */
void uft_hw_use_all(void) {
    uft_hw_backend_enable_all();
}

/**
 * @brief Nibtools ein/ausschalten (Convenience)
 */
void uft_hw_nibtools_enable(bool enable) {
    uft_hw_backend_set_enabled(UFT_HW_XUM1541, enable);
    uft_hw_backend_set_enabled(UFT_HW_ZOOMFLOPPY, enable);
    uft_hw_backend_set_enabled(UFT_HW_XU1541, enable);
    uft_hw_backend_set_enabled(UFT_HW_XA1541, enable);
}

// ============================================================================
// Configuration Persistence
// ============================================================================

/**
 * @brief Konfiguration speichern
 */
uft_error_t uft_hw_config_save(const char* path) {
    if (!path) return UFT_ERROR_NULL_POINTER;
    
    FILE* f = fopen(path, "w");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    fprintf(f, "# UFT Hardware Backend Configuration\n\n");
    
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i].backend) {
            fprintf(f, "%s = %s  # priority=%d\n",
                    g_backends[i].backend->name,
                    g_backends[i].enabled ? "enabled" : "disabled",
                    g_backends[i].priority);
        }
    }
    
    fclose(f);
    return UFT_OK;
}

/**
 * @brief Konfiguration laden
 */
uft_error_t uft_hw_config_load(const char* path) {
    if (!path) return UFT_ERROR_NULL_POINTER;
    
    FILE* f = fopen(path, "r");
    if (!f) return UFT_ERROR_FILE_OPEN;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        // Kommentare und Leerzeilen überspringen
        if (line[0] == '#' || line[0] == '\n') continue;
        
        char name[64];
        char value[32];
        
        if (sscanf(line, "%63s = %31s", name, value) == 2) {
            // Backend finden
            for (size_t i = 0; i < g_backend_count; i++) {
                if (g_backends[i].backend && 
                    strcmp(g_backends[i].backend->name, name) == 0) {
                    g_backends[i].enabled = (strcmp(value, "enabled") == 0);
                    break;
                }
            }
        }
    }
    
    fclose(f);
    return UFT_OK;
}

// ============================================================================
// Builtin Backend Registration
// ============================================================================

/**
 * @brief Registriert alle Builtin-Backends
 * 
 * Wird automatisch beim ersten Zugriff aufgerufen.
 */
uft_error_t uft_hw_register_builtin_backends(void) {
    uft_hw_manager_init();
    
    // Registriere verfügbare Backends
    // (Weak Symbols oder bedingte Kompilierung möglich)
    
#ifdef UFT_HW_ENABLE_GREASEWEAZLE
    uft_hw_manager_register(&uft_hw_backend_greaseweazle);
#endif

#ifdef UFT_HW_ENABLE_KRYOFLUX
    uft_hw_manager_register(&uft_hw_backend_kryoflux);
#endif

#ifdef UFT_HW_ENABLE_SUPERCARD
    uft_hw_manager_register(&uft_hw_backend_supercard);
#endif

#ifdef UFT_HW_ENABLE_FC5025
    uft_hw_manager_register(&uft_hw_backend_fc5025);
#endif

#ifdef UFT_HW_ENABLE_OPENCBM
    uft_hw_manager_register(&uft_hw_backend_opencbm);
#endif

    return UFT_OK;
}

// ============================================================================
// Debug/Info
// ============================================================================

/**
 * @brief Gibt Backend-Status auf stdout aus
 */
void uft_hw_print_backends(void) {
    printf("UFT Hardware Backends:\n");
    printf("======================\n\n");
    
    for (size_t i = 0; i < g_backend_count; i++) {
        if (g_backends[i].backend) {
            printf("  %-20s %s (priority=%d, %s)\n",
                   g_backends[i].backend->name,
                   g_backends[i].enabled ? "[ENABLED]" : "[disabled]",
                   g_backends[i].priority,
                   g_backends[i].initialized ? "init" : "not init");
        }
    }
    
    printf("\n");
}

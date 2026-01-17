/**
 * @file uft_format_plugin.h
 * @brief UnifiedFloppyTool - Format Plugin Interface
 * 
 * Definiert das Interface für Format-Plugins (ADF, SCP, HFE, etc.)
 */

#ifndef UFT_FORMAT_PLUGIN_H
#define UFT_FORMAT_PLUGIN_H

#include "uft_types.h"
#include "uft_error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Plugin Capabilities - Use central definition from format registry
// ============================================================================

#include "uft/core/uft_format_registry.h"  /* For uft_format_caps_t */

/* Legacy aliases for backwards compatibility */
#define UFT_FORMAT_CAP_READ      UFT_FCAP_READ
#define UFT_FORMAT_CAP_WRITE     UFT_FCAP_WRITE
#define UFT_FORMAT_CAP_CREATE    UFT_FCAP_CREATE
#define UFT_FORMAT_CAP_FLUX      UFT_FCAP_FLUX
#define UFT_FORMAT_CAP_TIMING    UFT_FCAP_TIMING
#define UFT_FORMAT_CAP_WEAK_BITS UFT_FCAP_WEAK_BITS
#define UFT_FORMAT_CAP_MULTI_REV UFT_FCAP_MULTI_REV
#define UFT_FORMAT_CAP_STREAMING (1 << 14)  /* Not in registry, add here */

// ============================================================================
// Internal Disk Structure (für Plugins)
// ============================================================================

/**
 * @brief Interne Disk-Struktur
 * 
 * Diese Struktur wird von Plugins befüllt/gelesen.
 */
struct uft_disk {
    // Identifikation
    char*               path;
    uft_format_t        format;
    uint32_t            flags;
    
    // Geometrie
    uft_geometry_t      geometry;
    
    // State
    bool                read_only;
    bool                modified;
    void*               file;          ///< FILE* oder Handle
    
    // Plugin-spezifisch
    void*               plugin_data;   ///< Private Daten des Plugins
    
    // Track Cache
    uft_track_t**       track_cache;   ///< [cyl * heads + head]
    uint32_t            cache_size;
    
    // Callback
    uft_log_fn          log_fn;
    void*               log_user;
};

/**
 * @brief Interne Track-Struktur
 * Note: If uft_track.h is included first, use its definition
 */
#ifndef UFT_TRACK_T_DEFINED
#define UFT_TRACK_T_DEFINED
struct uft_track {
    // Position
    int                 cylinder;
    int                 head;
    
    // Dekodierte Sektoren
    uft_sector_t*       sectors;
    size_t              sector_count;
    size_t              sector_capacity;
    
    // Flux-Daten (optional)
    uint32_t*           flux;          ///< Flux-Zeiten (ns oder ticks)
    size_t              flux_count;
    size_t              flux_capacity;
    uint32_t            flux_tick_ns;  ///< Tick-Dauer in ns
    
    // Encoding und Metriken
    uft_encoding_t      encoding;      ///< Disk-Encoding (MFM/FM/GCR)
    uft_track_metrics_t metrics;
    uint32_t            status;
    
    // Raw-Daten (für einige Formate)
    uint8_t*            raw_data;
    size_t              raw_size;
    
    // Owner
    uft_disk_t*         disk;
    void*               plugin_data;
};
typedef struct uft_track uft_track_t;
#endif /* UFT_TRACK_T_DEFINED */

// ============================================================================
// Format Plugin Interface
// ============================================================================

/**
 * @brief Format Plugin Struktur
 * 
 * Jedes Format-Plugin muss diese Struktur implementieren und registrieren.
 */
typedef struct uft_format_plugin {
    // === Identifikation ===
    const char*         name;          ///< Plugin-Name ("SCP", "ADF", etc.)
    const char*         description;   ///< Beschreibung
    const char*         extensions;    ///< Extensions (";"-getrennt)
    uint32_t            version;       ///< Plugin-Version
    uft_format_t        format;        ///< Format-Typ
    uint32_t            capabilities;  ///< uft_format_caps_t Flags
    
    // === Probe (Format erkennen) ===
    
    /**
     * @brief Prüft ob Daten diesem Format entsprechen
     * 
     * @param data Erste Bytes der Datei
     * @param size Größe der Daten (min. 512)
     * @param file_size Gesamtgröße der Datei
     * @param confidence [out] Confidence 0-100
     * @return true wenn erkannt
     */
    bool (*probe)(const uint8_t* data, size_t size, size_t file_size, 
                  int* confidence);
    
    // === Disk Operations ===
    
    /**
     * @brief Disk-Image öffnen
     * 
     * @param disk Vor-allokierte Disk-Struktur
     * @param path Dateipfad
     * @param read_only Read-Only Modus
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*open)(uft_disk_t* disk, const char* path, bool read_only);
    
    /**
     * @brief Disk-Image schließen
     */
    void (*close)(uft_disk_t* disk);
    
    /**
     * @brief Neues Disk-Image erstellen
     * 
     * @param disk Vor-allokierte Disk-Struktur
     * @param path Ziel-Pfad
     * @param geometry Gewünschte Geometrie
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*create)(uft_disk_t* disk, const char* path,
                           const uft_geometry_t* geometry);
    
    /**
     * @brief Änderungen speichern
     */
    uft_error_t (*flush)(uft_disk_t* disk);
    
    // === Track Operations ===
    
    /**
     * @brief Track lesen
     * 
     * @param disk Disk-Handle
     * @param cylinder Zylinder
     * @param head Seite
     * @param track [out] Track-Struktur (wird befüllt)
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*read_track)(uft_disk_t* disk, int cylinder, int head,
                               uft_track_t* track);
    
    /**
     * @brief Track schreiben
     */
    uft_error_t (*write_track)(uft_disk_t* disk, int cylinder, int head,
                                const uft_track_t* track);
    
    // === Optionale Erweiterungen ===
    
    /**
     * @brief Geometrie aus Datei ermitteln
     */
    uft_error_t (*detect_geometry)(uft_disk_t* disk, uft_geometry_t* geometry);
    
    /**
     * @brief Meta-Daten lesen (Volume-Name etc.)
     */
    uft_error_t (*read_metadata)(uft_disk_t* disk, const char* key, 
                                  char* value, size_t max_len);
    
    /**
     * @brief Meta-Daten schreiben
     */
    uft_error_t (*write_metadata)(uft_disk_t* disk, const char* key,
                                   const char* value);
    
    // === Plugin Lifecycle ===
    
    /**
     * @brief Plugin initialisieren
     */
    uft_error_t (*init)(void);
    
    /**
     * @brief Plugin beenden
     */
    void (*shutdown)(void);
    
    // === Private ===
    void*               private_data;
    
} uft_format_plugin_t;

// ============================================================================
// Plugin Registry
// ============================================================================

/**
 * @brief Format-Plugin registrieren
 * 
 * @param plugin Plugin-Struktur (muss statisch/global sein)
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_register_format_plugin(const uft_format_plugin_t* plugin);

/**
 * @brief Format-Plugin deregistrieren
 */
uft_error_t uft_unregister_format_plugin(uft_format_t format);

/**
 * @brief Plugin für Format holen
 */
const uft_format_plugin_t* uft_get_format_plugin(uft_format_t format);

/**
 * @brief Plugin für Extension finden
 */
const uft_format_plugin_t* uft_find_format_plugin_by_extension(const char* ext);

/**
 * @brief Bestes Plugin für Datei finden (probe)
 */
const uft_format_plugin_t* uft_find_format_plugin_for_file(const char* path);

/**
 * @brief Alle Plugins auflisten
 * 
 * @param plugins Array für Pointer
 * @param max Maximale Anzahl
 * @return Anzahl der Plugins
 */
size_t uft_list_format_plugins(const uft_format_plugin_t** plugins, size_t max);

// ============================================================================
// Helper für Plugin-Implementierung
// ============================================================================

/**
 * @brief Track-Struktur initialisieren
 */
void uft_track_init(uft_track_t* track, int cylinder, int head);

/**
 * @brief Sektor zu Track hinzufügen
 */
uft_error_t uft_track_add_sector(uft_track_t* track, const uft_sector_t* sector);

/**
 * @brief Flux-Daten zu Track hinzufügen
 */
uft_error_t uft_track_set_flux(uft_track_t* track, const uint32_t* flux, 
                                size_t count, uint32_t tick_ns);

/**
 * @brief Track-Ressourcen freigeben
 */
void uft_track_cleanup(uft_track_t* track);

/**
 * @brief Sektor nach ID finden (für Format-Plugins)
 */
const uft_sector_t* uft_track_find_sector(const uft_track_t* track, int sector);

/**
 * @brief Sektor-Daten kopieren
 */
uft_error_t uft_sector_copy_plugin(uft_sector_t* dst, const uft_sector_t* src);

/**
 * @brief Sektor-Ressourcen freigeben
 */
void uft_sector_cleanup(uft_sector_t* sector);

// ============================================================================
// Built-in Format Plugin Deklarationen
// ============================================================================

/// RAW Format Plugin (einfacher Sector-Dump)
extern const uft_format_plugin_t uft_format_plugin_raw;

/// IMG/IMA Format Plugin (PC-kompatibel)
extern const uft_format_plugin_t uft_format_plugin_img;

/// ADF Format Plugin (Amiga)
extern const uft_format_plugin_t uft_format_plugin_adf;

/// SCP Format Plugin (SuperCard Pro)
extern const uft_format_plugin_t uft_format_plugin_scp;

/// HFE Format Plugin (HxC)
extern const uft_format_plugin_t uft_format_plugin_hfe;

/// D64 Format Plugin (C64)
extern const uft_format_plugin_t uft_format_plugin_d64;

/// G64 Format Plugin (C64 GCR)
extern const uft_format_plugin_t uft_format_plugin_g64;

/// CQM Format Plugin (CopyQM)
extern const uft_format_plugin_t uft_format_plugin_cqm;

/// ApriDisk Format Plugin
extern const uft_format_plugin_t uft_format_plugin_apridisk;

/// NanoWasp Format Plugin (Microbee)
extern const uft_format_plugin_t uft_format_plugin_nanowasp;

/// QRST Format Plugin (Compaq)
extern const uft_format_plugin_t uft_format_plugin_qrst;

/// CFI Format Plugin (Compressed Floppy Image)
extern const uft_format_plugin_t uft_format_plugin_cfi;

/// YDSK Format Plugin (YAZE CP/M)
extern const uft_format_plugin_t uft_format_plugin_ydsk;

/// SIMH Format Plugin (SIMH Simulator)
extern const uft_format_plugin_t uft_format_plugin_simh;

/// Logical Format Plugin
extern const uft_format_plugin_t uft_format_plugin_logical;

/// CP/M Format Plugin
extern const uft_format_plugin_t uft_format_plugin_cpm;

/// X68000 Format Plugin (XDF/DIM)
extern const uft_format_plugin_t uft_format_plugin_x68k;

/// Hard-Sector Format Plugin (8"/5.25")
extern const uft_format_plugin_t uft_format_plugin_hardsector;

/// POSIX Format Plugin (raw + geometry file)
extern const uft_format_plugin_t uft_format_plugin_posix;

/// OPUS Format Plugin (ZX Spectrum OPUS Discovery)
extern const uft_format_plugin_t uft_format_plugin_opus;

/// MGT Format Plugin (ZX Spectrum +D/DISCiPLE)
extern const uft_format_plugin_t uft_format_plugin_mgt;

/// LDBS Format Plugin (LibDsk Block Store)
extern const uft_format_plugin_t uft_format_plugin_ldbs;

/// RCPMFS Format Plugin (Remote CP/M File System)
extern const uft_format_plugin_t uft_format_plugin_rcpmfs;

/// DFI Format Plugin (DiscFerret Flux Image)
extern const uft_format_plugin_t uft_format_plugin_dfi;

/// MYZ80 Format Plugin (MYZ80 CP/M Emulator)
extern const uft_format_plugin_t uft_format_plugin_myz80;

// ============================================================================
// Alle Built-in Plugins registrieren
// ============================================================================

/**
 * @brief Alle eingebauten Format-Plugins registrieren
 */
uft_error_t uft_register_builtin_format_plugins(void);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_PLUGIN_H

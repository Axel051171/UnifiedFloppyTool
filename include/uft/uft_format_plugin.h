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
// Plugin Capabilities
// ============================================================================

/**
 * @brief Format-Plugin Fähigkeiten
 */
#ifndef UFT_FORMAT_CAPS_DEFINED
#define UFT_FORMAT_CAPS_DEFINED
typedef enum uft_format_caps {
    UFT_FORMAT_CAP_READ         = (1 << 0),  ///< Kann lesen
    UFT_FORMAT_CAP_WRITE        = (1 << 1),  ///< Kann schreiben
    UFT_FORMAT_CAP_CREATE       = (1 << 2),  ///< Kann neue erstellen
    UFT_FORMAT_CAP_FLUX         = (1 << 3),  ///< Hat Flux-Daten
    UFT_FORMAT_CAP_TIMING       = (1 << 4),  ///< Erhält Timing
    UFT_FORMAT_CAP_WEAK_BITS    = (1 << 5),  ///< Unterstützt Weak Bits
    UFT_FORMAT_CAP_MULTI_REV    = (1 << 6),  ///< Multiple Revolutions
    UFT_FORMAT_CAP_STREAMING    = (1 << 7),  ///< Streaming I/O
    UFT_FORMAT_CAP_VERIFY       = (1 << 8),  ///< Kann verify_track
} uft_format_caps_t;
#endif /* UFT_FORMAT_CAPS_DEFINED */

// ============================================================================
// Internal Disk Structure (für Plugins)
// ============================================================================

/**
 * @brief Interne Disk-Struktur
 * 
 * Diese Struktur wird von Plugins befüllt/gelesen.
 */
/**
 * @brief CANONICAL uft_disk — superset of all historical variants
 *
 * This is THE ONE definition of uft_disk. Previously there were 4 parallel
 * definitions in uft_disk.h, uft_format_plugin_v2.h, uft_plugin_bridge.h,
 * uft_format_plugin.h — all with different fields. GUI/Plugins never shared
 * the same struct layout. Now consolidated here.
 *
 * Field groups:
 *   Identity       — path, format, encoding, flags
 *   Geometry       — geometry struct
 *   State          — read_only/is_readonly, modified/is_modified, is_open
 *   Plugin slot    — plugin_data (owned by plugin)
 *   Backends       — reader_backend, writer_backend, hw_provider
 *   Tracks         — track_cache, tracks[], image_data
 *   File handle    — file (FILE* or opaque)
 *   Callbacks      — log, progress
 */
struct uft_disk {
    /* Identity */
    char                path_buf[512];  ///< Path buffer (internal storage)
    char*               path;           ///< Path pointer — can reference path_buf or external
    uft_format_t        format;
    uft_encoding_t      encoding;
    uint32_t            flags;

    /* Geometry */
    uft_geometry_t      geometry;

    /* State */
    bool                read_only;
    bool                modified;
    bool                is_open;

    /* Aliases for legacy API compat (point to same bytes) */
    #define is_readonly  read_only
    #define is_modified  modified

    /* File handle */
    void*               file;           ///< FILE* or opaque handle

    /* Plugin-specific */
    void*               plugin_data;    ///< Owned by plugin

    /* Legacy backends */
    void*               reader_backend;
    void*               writer_backend;
    void*               hw_provider;

    /* Track storage (legacy GUI usage) */
    uft_track_t**       tracks;         ///< Track array (legacy)
    int                 track_count;

    /* Image buffer (legacy disk-image-in-memory) */
    uint8_t*            image_data;
    size_t              image_size;

    /* Plugin-B Track Cache */
    uft_track_t**       track_cache;    ///< [cyl * heads + head]
    uint32_t            cache_size;

    /* Callbacks */
    uft_log_fn          log_fn;
    void*               log_user;

    /* Metadata (Key-Value + Annotations, set via uft_meta_* API) */
    void*               meta;          ///< Opaque uft_meta_store_t*
};

/* Forward declarations for layer types (full defs in uft_track.h) */
struct uft_flux_layer;
struct uft_bitstream_layer;
struct uft_sector_layer;

/**
 * @brief Unified Track Structure (CANONICAL — superset of all variants)
 *
 * This is the ONE definition of uft_track_t used across the entire project.
 * All fields from uft_format_plugin.h, uft_track.h, uft_io_abstraction.h,
 * and uft_unified_types.h are merged here.
 *
 * Memory ownership:
 * - uft_track_alloc() creates track, owns all internal memory
 * - uft_track_free() releases track and all internal memory
 * - Never manually free internal pointers!
 */
#ifndef UFT_TRACK_T_DEFINED
#define UFT_TRACK_T_DEFINED
struct uft_track {
    /* ═══ Identity ═══ */
    int                 cylinder;               ///< Physical cylinder
    int                 head;                   ///< Head/side
    uint16_t            track_num;              ///< Linear track index (cyl*heads+head)
    uint8_t             side;                   ///< Side (alias for head, backward compat)
    int8_t              quarter_offset;         ///< Quarter-track offset (-2 to +2)
    bool                is_half_track;          ///< True for half-track

    /* ═══ Encoding ═══ */
    uft_encoding_t      encoding;               ///< Disk-Encoding (MFM/FM/GCR)
    uint32_t            bitrate;                ///< Bit rate in bps
    uint32_t            rpm;                    ///< Drive RPM
    double              nominal_bit_rate_kbps;  ///< 250, 300, 500 kbps
    double              nominal_rpm;            ///< 300.0 or 360.0
    double              data_rate;              ///< Data rate in bits/sec

    /* ═══ Status ═══ */
    uint32_t            status;                 ///< uft_track_status_t flags
    uint32_t            available_layers;       ///< uft_layer_flags_t
    bool                decoded;                ///< true if sectors decoded
    int                 errors;                 ///< error count
    float               quality_score;          ///< 0.0-1.0 quality
    uint8_t             quality;                ///< Track quality enum (backward compat)
    bool                complete;               ///< All sectors found
    bool                copy_protected;         ///< Copy protection detected

    /* ═══ Metrics ═══ */
    uft_track_metrics_t metrics;
    /* Inline quality metrics (from uft_track_quality_t) */
    double              avg_bit_cell_ns;        ///< Average bit cell time (ns)
    double              jitter_ns;              ///< Timing jitter (ns)
    double              jitter_percent;         ///< Jitter as % of bit cell
    int                 decode_errors;          ///< PLL/decode errors
    float               detection_confidence;   ///< Detection confidence 0.0-1.0
    int                 signal_strength;        ///< 0-100

    /* ═══ Decoded Sectors ═══ */
    uft_sector_t*       sectors;                ///< Dynamic sector array
    size_t              sector_count;
    size_t              sector_capacity;
    uft_sector_t        sectors_fixed[64];      ///< Legacy fixed array (UFT_MAX_SECTORS)
    int                 sector_count_legacy;    ///< Legacy sector count

    /* ═══ Data Layers (extended) ═══ */
    struct uft_flux_layer*      flux_layer;     ///< Layer 0: Flux (optional)
    struct uft_bitstream_layer* bitstream;      ///< Layer 1: Bits (optional)
    struct uft_sector_layer*    sector_layer;   ///< Layer 2: Sectors (optional)

    /* ═══ Flux Data ═══ */
    uint32_t*           flux;                   ///< Flux timing (ns or ticks)
    size_t              flux_count;
    size_t              flux_capacity;
    uint32_t            flux_tick_ns;           ///< Tick duration in ns
    uint32_t*           flux_data;              ///< Legacy flux pointer
    double*             flux_times;             ///< Flux transition times (ns)

    /* ═══ Raw Data (bitstream) ═══ */
    uint8_t*            raw_data;               ///< Raw track data
    size_t              raw_size;               ///< Raw data size (bytes)
    size_t              raw_len;                ///< Raw data length (alias)
    size_t              raw_bits;               ///< Number of bits
    size_t              raw_capacity;           ///< Allocated bytes

    /* ═══ Multi-Revision Data ═══ */
    struct {
        uint8_t *data;
        size_t   bits;
        uint8_t  quality;                       ///< 0-100 quality score
    } *revisions;
    size_t              revision_count;

    /* ═══ Quality Maps ═══ */
    uint8_t*            confidence;             ///< Per-bit confidence
    bool*               weak_mask;              ///< Per-bit weak flags

    /* ═══ Timing ═══ */
    uint32_t            track_time_ns;          ///< Total track time
    uint32_t            write_splice_ns;        ///< Write splice location
    uint64_t            rotation_ns;            ///< Rotation time

    /* ═══ Error Info ═══ */
    int                 error;                  ///< Primary error code (uft_error_t compatible)

    /* ═══ Owner / User Data ═══ */
    uft_disk_t*         disk;                   ///< Owning disk
    void*               plugin_data;            ///< Plugin-specific data
    void*               user_data;              ///< Application-specific
    bool                owns_data;              ///< True = free on destroy

    /* ═══ Internal ═══ */
    uint32_t            _magic;                 ///< UFT_TRACK_MAGIC
    uint32_t            _version;               ///< UFT_TRACK_VERSION
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

    /**
     * @brief Track verifizieren
     *
     * Vergleicht den Disk-Inhalt mit einer Referenz. Formatspezifische
     * Implementierung (z.B. Weak-Bit-Toleranz) oder uft_generic_verify_track.
     *
     * @return UFT_OK wenn übereinstimmend, UFT_ERROR_VERIFY_FAILED sonst
     */
    uft_error_t (*verify_track)(uft_disk_t* disk, int cylinder, int head,
                                 const uft_track_t* reference);

    /**
     * @brief Einzel-Sektor verifizieren (optional)
     */
    uft_error_t (*verify_sector)(uft_disk_t* disk, int cyl, int head,
                                  uint8_t sector_id,
                                  const uint8_t* expected_data,
                                  size_t expected_len);

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
 * @brief Generische Verify-Implementierung
 *
 * Default-Implementierung für verify_track. Liest Track, vergleicht
 * Sektor-für-Sektor mit der Referenz. Für Standard-Sector-Formate geeignet.
 * Formate mit Weak-Bits/Timing brauchen eigene verify_track-Funktion.
 */
uft_error_t uft_generic_verify_track(uft_disk_t *disk, int cyl, int head,
                                      const uft_track_t *reference);

/**
 * @brief Weak-Bit-tolerante Verify-Implementierung (ATX/STX/PRO)
 *
 * Vergleicht Sektor-Daten byteweise, ignoriert aber Bytes die in weak_mask
 * als weak markiert sind. Sektoren mit weak-Flag aber ohne Mask werden
 * übersprungen (nicht reproduzierbar).
 */
uft_error_t uft_weak_bit_verify_track(uft_disk_t *disk, int cyl, int head,
                                       const uft_track_t *reference);

/**
 * @brief Flux-Level Verify-Implementierung (WOZ/IPF/KFX/MFI/PRI/UDI)
 *
 * Vergleicht primär raw_data/raw_bits (Bitstream-Ebene). Fällt auf
 * Sektor-Vergleich zurück wenn raw_data nicht verfügbar. Weak-Sektoren
 * werden toleriert (Flux kann nicht reproduzierbar dekodiert werden).
 */
uft_error_t uft_flux_verify_track(uft_disk_t *disk, int cyl, int head,
                                   const uft_track_t *reference);

/**
 * @brief Format-Gruppen-Metadaten (CBM, ATARI, APPLE, ...)
 *
 * Ordnet Format-Plugins semantisch zu Plattformen. Nützlich für GUI/CLI
 * die Formate nach Plattform präsentieren (z.B. "alle Commodore-Formate").
 */
typedef struct {
    const char *name;                          /* "CBM", "ATARI", "APPLE", ... */
    const char *description;                   /* "Commodore 8-bit computers" */
    const uft_format_plugin_t *const *plugins; /* Array of plugin pointers */
    size_t plugin_count;
} uft_format_group_t;

/**
 * @brief Alle Format-Gruppen abrufen
 * @param count [out] Anzahl der Gruppen
 * @return Array von Gruppen (konstant, nicht freigeben)
 */
const uft_format_group_t* uft_format_get_groups(size_t *count);

/**
 * @brief Gruppe nach Namen suchen
 * @param name Gruppen-Name ("CBM", "ATARI", ...)
 * @return Gruppe oder NULL wenn nicht gefunden
 */
const uft_format_group_t* uft_format_find_group(const char *name);

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
 * @brief Plugin für Buffer-Daten finden (via probe-Chain)
 *
 * Probiert alle registrierten Plugins, wählt das mit höchster Confidence.
 */
const uft_format_plugin_t* uft_probe_buffer_format(const uint8_t *data,
                                                    size_t size,
                                                    size_t file_size);

/**
 * @brief Plugin für Datei finden (liest Anfang + probed)
 */
const uft_format_plugin_t* uft_probe_file_format(const char *path);

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
#define UFT_TRACK_INIT_DECLARED
void uft_track_init(uft_track_t* track, int cylinder, int head);

/**
 * @brief Sektor zu Track hinzufügen
 */
uft_error_t uft_track_add_sector(uft_track_t* track, const uft_sector_t* sector);

/**
 * @brief Flux-Daten zu Track hinzufügen
 */
#define UFT_TRACK_SET_FLUX_DECLARED
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

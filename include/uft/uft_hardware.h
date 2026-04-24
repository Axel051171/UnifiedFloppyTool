/**
 * @file uft_hardware.h
 * @brief UnifiedFloppyTool - Hardware Abstraction Layer
 * 
 * Einheitliche API für verschiedene Floppy-Hardware:
 * - FC5025 (Device Side Industries) - 5.25"/8" FM/MFM
 * - SuperCard Pro - Flux-Level, High-Resolution
 * - Applesauce - Apple II GCR
 * 
 * ARCHITEKTUR:
 * 
 *   Application Layer (uft_disk_t)
 *           │
 *           ▼
 *   ┌─────────────────────────────┐
 *   │    UFT Hardware API        │
 *   │  uft_hw_device_t           │
 *   └─────────────────────────────┘
 *           │
 *   ┌───────┴───────┬───────────────┬────────────────┐
 *   ▼               ▼               ▼                ▼
 * ┌─────────┐  ┌─────────┐   ┌──────────┐   ┌──────────┐
 * │Backend  │  │ Backend │   │ Backend  │   │ Backend  │
 * └─────────┘  └─────────┘   └──────────┘   └──────────┘
 * 
 * @author UFT Team
 * @date 2025
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Root-level API
 *
 * This header declares 43 public functions; 36 are NOT implemented
 * in the source tree (only 7 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_HARDWARE_H
#define UFT_HARDWARE_H

#include "uft_types.h"
#include "uft_error.h"
#include "uft_decoder_plugin.h"
#include "uft_progress.h"   /* uft_unified_progress_fn for batch */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Hardware Types
// ============================================================================

/**
 * @brief Unterstützte Hardware-Typen
 */
typedef enum uft_hw_type {
    UFT_HW_UNKNOWN          = 0,
    
    // === Commodore (GCR) ===
    UFT_HW_XUM1541          = 1,    ///< XUM1541 USB Adapter
    UFT_HW_ZOOMFLOPPY       = 2,    ///< ZoomFloppy (XU1541 + Parallel)
    UFT_HW_XU1541           = 3,    ///< XU1541 (nur seriell)
    UFT_HW_XA1541           = 4,    ///< XA1541 (Active)
    
    // === FC5025 (MFM/FM) ===
    UFT_HW_FC5025           = 10,   ///< FC5025 USB Controller
    
    // === Flux-Level Hardware ===
    UFT_HW_GREASEWEAZLE     = 20,   ///< Greaseweazle
    UFT_HW_FLUXENGINE       = 21,   ///< FluxEngine
    UFT_HW_SUPERCARD_PRO    = 22,   ///< SuperCard Pro
    UFT_HW_KRYOFLUX         = 23,   ///< KryoFlux
    UFT_HW_APPLESAUCE       = 24,   ///< Applesauce
    UFT_HW_PAULINE          = 25,   ///< Pauline
    
    // === Legacy ===
    UFT_HW_CATWEASEL        = 30,   ///< CatWeasel PCI/MK4
    
    // === Emulation/Virtual ===
    UFT_HW_VIRTUAL          = 100,  ///< Virtual Device (für Tests)
    
} uft_hw_type_t;

/**
 * @brief Hardware-Capabilities
 */
typedef enum uft_hw_caps {
    UFT_HW_CAP_READ         = (1 << 0),   ///< Kann lesen
    UFT_HW_CAP_WRITE        = (1 << 1),   ///< Kann schreiben
    UFT_HW_CAP_FLUX         = (1 << 2),   ///< Flux-Level Zugriff
    UFT_HW_CAP_INDEX        = (1 << 3),   ///< Index-Pulse Erkennung
    UFT_HW_CAP_MULTI_REV    = (1 << 4),   ///< Multiple Revolutions
    UFT_HW_CAP_DENSITY      = (1 << 5),   ///< Density Select
    UFT_HW_CAP_SIDE         = (1 << 6),   ///< Side Select
    UFT_HW_CAP_MOTOR        = (1 << 7),   ///< Motor Control
    UFT_HW_CAP_EJECT        = (1 << 8),   ///< Eject Control
    UFT_HW_CAP_TIMING       = (1 << 9),   ///< Präzises Timing
    UFT_HW_CAP_WEAK_BITS    = (1 << 10),  ///< Weak Bit Detection
} uft_hw_caps_t;

/**
 * @brief Drive-Typen
 */
typedef enum uft_drive_type {
    UFT_DRIVE_UNKNOWN       = 0,
    
    // Commodore
    UFT_DRIVE_1541          = 1,
    UFT_DRIVE_1571          = 2,
    UFT_DRIVE_1581          = 3,
    
    // PC
    UFT_DRIVE_PC_525_DD     = 10,   ///< 5.25" 360KB
    UFT_DRIVE_PC_525_HD     = 11,   ///< 5.25" 1.2MB
    UFT_DRIVE_PC_35_DD      = 12,   ///< 3.5" 720KB
    UFT_DRIVE_PC_35_HD      = 13,   ///< 3.5" 1.44MB
    UFT_DRIVE_PC_35_ED      = 14,   ///< 3.5" 2.88MB
    
    // 8 Zoll
    UFT_DRIVE_8_SSSD        = 20,   ///< 8" Single Sided Single Density
    UFT_DRIVE_8_DSDD        = 21,   ///< 8" Double Sided Double Density
    
    // Apple
    UFT_DRIVE_APPLE_525     = 30,   ///< Apple II 5.25"
    UFT_DRIVE_APPLE_35      = 31,   ///< Mac 3.5"
    
    // Andere
    UFT_DRIVE_AMIGA_DD      = 40,   ///< Amiga DD
    UFT_DRIVE_AMIGA_HD      = 41,   ///< Amiga HD
    UFT_DRIVE_ATARI_ST      = 50,   ///< Atari ST
    
} uft_drive_type_t;

// ============================================================================
// Device Info
// ============================================================================

/**
 * @brief Hardware-Device Informationen
 */
typedef struct uft_hw_info {
    uft_hw_type_t       type;               ///< Hardware-Typ
    char                name[64];           ///< Gerätename
    char                serial[32];         ///< Seriennummer
    char                firmware[32];       ///< Firmware-Version
    uint32_t            capabilities;       ///< uft_hw_caps_t Flags
    
    // USB-Info
    uint16_t            usb_vid;            ///< USB Vendor ID
    uint16_t            usb_pid;            ///< USB Product ID
    char                usb_path[128];      ///< USB Device Path
    
    // Timing
    uint32_t            sample_rate_hz;     ///< Sample Rate (Flux)
    uint32_t            resolution_ns;      ///< Timing Resolution
    
} uft_hw_info_t;

/**
 * @brief Drive-Status
 */
typedef struct uft_drive_status {
    bool                connected;          ///< Drive verbunden
    bool                disk_present;       ///< Disk eingelegt
    bool                write_protected;    ///< Schreibschutz aktiv
    bool                motor_on;           ///< Motor läuft
    bool                ready;              ///< Drive bereit
    
    uint8_t             current_track;      ///< Aktuelle Track-Position
    uint8_t             current_head;       ///< Aktuelle Seite
    
    double              rpm;                ///< Gemessene RPM
    double              index_time_us;      ///< Zeit zwischen Index-Pulsen
    
} uft_drive_status_t;

// ============================================================================
// Device Handle
// ============================================================================

/**
 * @brief Opaque Device Handle
 */
typedef struct uft_hw_device uft_hw_device_t;

// ============================================================================
// Hardware Backend Interface
// ============================================================================

/**
 * @brief Backend-Funktionen für einen Hardware-Typ
 * 
 * Jedes Backend implementiert diese Funktionen
 */
typedef struct uft_hw_backend {
    const char*         name;               ///< Backend-Name
    uft_hw_type_t       type;               ///< Hardware-Typ
    
    // === Lifecycle ===
    
    /**
     * @brief Backend initialisieren
     */
    uft_error_t (*init)(void);
    
    /**
     * @brief Backend herunterfahren
     */
    void (*shutdown)(void);
    
    // === Discovery ===
    
    /**
     * @brief Sucht nach Geräten
     * 
     * @param devices Array für gefundene Geräte
     * @param max_devices Maximale Anzahl
     * @param found [out] Anzahl gefundener Geräte
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*enumerate)(uft_hw_info_t* devices, size_t max_devices, 
                             size_t* found);
    
    // === Connection ===
    
    /**
     * @brief Gerät öffnen
     * 
     * @param info Geräte-Info aus enumerate()
     * @param device [out] Device Handle
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*open)(const uft_hw_info_t* info, uft_hw_device_t** device);
    
    /**
     * @brief Gerät schließen
     */
    void (*close)(uft_hw_device_t* device);
    
    // === Drive Control ===
    
    /**
     * @brief Drive-Status abfragen
     */
    uft_error_t (*get_status)(uft_hw_device_t* device, uft_drive_status_t* status);
    
    /**
     * @brief Motor ein/ausschalten
     */
    uft_error_t (*motor)(uft_hw_device_t* device, bool on);
    
    /**
     * @brief Zu Track bewegen
     */
    uft_error_t (*seek)(uft_hw_device_t* device, uint8_t track);
    
    /**
     * @brief Seite wählen
     */
    uft_error_t (*select_head)(uft_hw_device_t* device, uint8_t head);
    
    /**
     * @brief Density wählen (DD/HD)
     */
    uft_error_t (*select_density)(uft_hw_device_t* device, bool high_density);
    
    // === Track I/O ===
    
    /**
     * @brief Track lesen (dekodiert)
     * 
     * @param device Device Handle
     * @param track Track-Daten
     * @param revolutions Anzahl Umdrehungen (0 = 1)
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*read_track)(uft_hw_device_t* device, uft_track_t* track,
                              uint8_t revolutions);
    
    /**
     * @brief Track schreiben
     */
    uft_error_t (*write_track)(uft_hw_device_t* device, const uft_track_t* track);
    
    // === Flux I/O (optional) ===
    
    /**
     * @brief Rohe Flux-Daten lesen
     * 
     * @param device Device Handle
     * @param flux [out] Flux-Zeiten in Nanosekunden
     * @param max_flux Maximale Anzahl
     * @param flux_count [out] Anzahl gelesener Flux
     * @param revolutions Anzahl Umdrehungen
     * @return UFT_OK bei Erfolg
     */
    uft_error_t (*read_flux)(uft_hw_device_t* device, uint32_t* flux,
                             size_t max_flux, size_t* flux_count,
                             uint8_t revolutions);
    
    /**
     * @brief Rohe Flux-Daten schreiben
     */
    uft_error_t (*write_flux)(uft_hw_device_t* device, const uint32_t* flux,
                              size_t flux_count);
    
    // === Commodore-spezifisch (Nibtools) ===
    
    /**
     * @brief Parallel-Port Daten senden (für 1541)
     */
    uft_error_t (*parallel_write)(uft_hw_device_t* device, 
                                  const uint8_t* data, size_t len);
    
    /**
     * @brief Parallel-Port Daten empfangen
     */
    uft_error_t (*parallel_read)(uft_hw_device_t* device,
                                 uint8_t* data, size_t len, size_t* read);
    
    /**
     * @brief IEC-Bus Befehl senden
     */
    uft_error_t (*iec_command)(uft_hw_device_t* device,
                               uint8_t device_num, uint8_t command,
                               const uint8_t* data, size_t len);

    // === Optional: Batch-Reads ===

    /**
     * @brief Optional: Mehrere Tracks am Stück lesen.
     *
     * Backends mit Streaming-Architektur (KryoFlux, Greaseweazle Bulk-USB)
     * implementieren das für deutlich bessere Performance bei komplettem
     * Disk-Capture. Backends ohne internen Batch-Vorteil setzen diesen
     * Pointer auf NULL — die Capture-API fällt dann auf eine Schleife
     * über read_track() zurück.
     */
    uft_error_t (*read_tracks_batch)(uft_hw_device_t* device,
                                       int cyl_start, int cyl_end,
                                       int head_mask,
                                       uft_track_t* tracks_out,
                                       uint8_t revolutions,
                                       uft_unified_progress_fn progress,
                                       void* user_data);

    // === Capabilities Bitmap (ORed uft_hw_capability_t) ===
    uint32_t            capabilities;

    // === Private ===
    void*               private_data;

} uft_hw_backend_t;

// ============================================================================
// Hardware Capabilities
// ============================================================================

typedef enum {
    UFT_HW_CAP_READ_TRACK      = (1 << 0),   /* Pflicht */
    UFT_HW_CAP_WRITE_TRACK     = (1 << 1),   /* Pflicht */
    UFT_HW_CAP_READ_FLUX       = (1 << 2),   /* Optional */
    UFT_HW_CAP_WRITE_FLUX      = (1 << 3),   /* Optional */
    UFT_HW_CAP_BATCH_READ      = (1 << 4),   /* read_tracks_batch vorhanden */
    UFT_HW_CAP_PARALLEL_PORT   = (1 << 5),   /* CBM 1541 parallel */
    UFT_HW_CAP_IEC_BUS         = (1 << 6),   /* CBM serial */
    UFT_HW_CAP_DENSITY_SELECT  = (1 << 7),   /* DD/HD wechselbar */
    UFT_HW_CAP_MULTI_DRIVE     = (1 << 8),   /* Mehrere Drives am gleichen Controller */
} uft_hw_capability_t;

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Hardware-Subsystem initialisieren
 */
uft_error_t uft_hw_init(void);

/**
 * @brief Hardware-Subsystem herunterfahren
 */
void uft_hw_shutdown(void);

/**
 * @brief Backend registrieren
 */
uft_error_t uft_hw_register_backend(const uft_hw_backend_t* backend);

/**
 * @brief Liest mehrere Tracks. Nutzt Backend-Batch wenn verfügbar, sonst Fallback.
 *
 * Das ist die API die Anwendungen (Capture, GUI) aufrufen sollen. Sie wählt
 * selbst ob der native Batch-Hook des Backends oder der Schleifen-Fallback
 * genutzt wird.
 *
 * @param device      geöffnetes Device
 * @param cyl_start   inklusiv
 * @param cyl_end     exklusiv
 * @param head_mask   Bitmaske: 0x1=head0, 0x2=head1, 0x3=beide
 * @param tracks_out  pre-allocated Array mit (cyl_end-cyl_start)*popcount(head_mask) Einträgen
 * @param revolutions Umdrehungen pro Track (typ. 1-5)
 * @param progress    optional, NULL = kein Progress
 * @param user_data   an progress() weitergereicht
 */
uft_error_t uft_hw_read_tracks(uft_hw_device_t *device,
                                 int cyl_start, int cyl_end,
                                 int head_mask,
                                 uft_track_t *tracks_out,
                                 uint8_t revolutions,
                                 uft_unified_progress_fn progress,
                                 void *user_data);

/**
 * @brief Generischer Fallback — nutzt backend->read_track in Schleife.
 *
 * Wird automatisch von uft_hw_read_tracks genutzt wenn das Backend
 * keinen nativen Batch-Hook hat.
 */
uft_error_t uft_hw_read_tracks_batch_default(uft_hw_device_t *device,
                                                int cyl_start, int cyl_end,
                                                int head_mask,
                                                uft_track_t *tracks_out,
                                                uint8_t revolutions,
                                                uft_unified_progress_fn progress,
                                                void *user_data);

/**
 * @brief Alle verfügbaren Geräte auflisten
 * 
 * @param devices Array für Ergebnisse
 * @param max_devices Maximale Anzahl
 * @param found [out] Anzahl gefundener Geräte
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_hw_enumerate(uft_hw_info_t* devices, size_t max_devices,
                             size_t* found);

/**
 * @brief Gerät öffnen
 * 
 * @param info Geräte-Info aus enumerate()
 * @param device [out] Device Handle
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_hw_open(const uft_hw_info_t* info, uft_hw_device_t** device);

/**
 * @brief Gerät schließen
 */
void uft_hw_close(uft_hw_device_t* device);

/**
 * @brief Device-Info abfragen
 */
uft_error_t uft_hw_get_info(uft_hw_device_t* device, uft_hw_info_t* info);

/**
 * @brief Drive-Status abfragen
 */
uft_error_t uft_hw_get_status(uft_hw_device_t* device, uft_drive_status_t* status);

// === Motor/Seek ===

uft_error_t uft_hw_motor_on(uft_hw_device_t* device);
uft_error_t uft_hw_motor_off(uft_hw_device_t* device);
uft_error_t uft_hw_seek(uft_hw_device_t* device, uint8_t track);
uft_error_t uft_hw_select_head(uft_hw_device_t* device, uint8_t head);
uft_error_t uft_hw_recalibrate(uft_hw_device_t* device);

// === Track I/O ===

/**
 * @brief Track lesen
 * 
 * @param device Device Handle
 * @param cylinder Zylinder
 * @param head Seite
 * @param track [out] Track-Daten
 * @param options Lese-Optionen (NULL = defaults)
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_hw_read_track(uft_hw_device_t* device, 
                              uint8_t cylinder, uint8_t head,
                              uft_track_t* track,
                              const uft_decode_options_t* options);

/**
 * @brief Track schreiben
 */
uft_error_t uft_hw_write_track(uft_hw_device_t* device,
                               uint8_t cylinder, uint8_t head,
                               const uft_track_t* track,
                               const uft_encode_options_t* options);

// === Flux I/O ===

/**
 * @brief Rohe Flux-Daten lesen
 */
uft_error_t uft_hw_read_flux(uft_hw_device_t* device,
                             uint8_t cylinder, uint8_t head,
                             uint32_t** flux, size_t* flux_count,
                             uint8_t revolutions);

/**
 * @brief Rohe Flux-Daten schreiben
 */
uft_error_t uft_hw_write_flux(uft_hw_device_t* device,
                              uint8_t cylinder, uint8_t head,
                              const uint32_t* flux, size_t flux_count);

// === Disk-Level Operations ===

/**
 * @brief Komplette Disk lesen und als Image speichern
 * 
 * @param device Device Handle
 * @param path Ziel-Pfad
 * @param format Ziel-Format
 * @param progress Progress-Callback (optional)
 * @param user_data User-Data für Callback
 * @return UFT_OK bei Erfolg
 */
typedef void (*uft_hw_progress_fn)(int track, int total, void* user_data);

uft_error_t uft_hw_read_disk(uft_hw_device_t* device,
                             const char* path,
                             uft_format_t format,
                             const uft_geometry_t* geometry,
                             uft_hw_progress_fn progress,
                             void* user_data);

/**
 * @brief Image auf Disk schreiben
 */
uft_error_t uft_hw_write_disk(uft_hw_device_t* device,
                              const char* path,
                              uft_hw_progress_fn progress,
                              void* user_data);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Hardware-Typ zu String
 */
const char* uft_hw_type_name(uft_hw_type_t type);

/**
 * @brief Drive-Typ zu String
 */
const char* uft_drive_type_name(uft_drive_type_t type);

/**
 * @brief Prüft ob Hardware Flux-Level Zugriff unterstützt
 */
bool uft_hw_supports_flux(uft_hw_type_t type);

/**
 * @brief Empfohlenes Output-Format für Hardware
 */
uft_format_t uft_hw_recommended_format(uft_hw_type_t type, uft_drive_type_t drive);

// ============================================================================
// Backend Manager API
// ============================================================================

/**
 * @brief Hardware Manager initialisieren
 */
uft_error_t uft_hw_manager_init(void);

/**
 * @brief Hardware Manager herunterfahren
 */
void uft_hw_manager_shutdown(void);

/**
 * @brief Backend registrieren
 */
uft_error_t uft_hw_manager_register(const uft_hw_backend_t* backend);

/**
 * @brief Backend aktivieren/deaktivieren
 * 
 * @param type Hardware-Typ (z.B. UFT_HW_XUM1541 für Nibtools)
 * @param enabled true = aktivieren, false = deaktivieren
 * @return UFT_OK bei Erfolg
 * 
 * BEISPIEL - Nibtools ausschalten:
 *   uft_hw_backend_set_enabled(UFT_HW_XUM1541, false);
 */
uft_error_t uft_hw_backend_set_enabled(uft_hw_type_t type, bool enabled);

/**
 * @brief Prüft ob Backend aktiviert ist
 */
bool uft_hw_backend_is_enabled(uft_hw_type_t type);

/**
 * @brief Alle Backends deaktivieren
 */
void uft_hw_backend_disable_all(void);

/**
 * @brief Alle Backends aktivieren
 */
void uft_hw_backend_enable_all(void);

/**
 * @brief Backend-Priorität setzen
 */
uft_error_t uft_hw_backend_set_priority(uft_hw_type_t type, int priority);

/**
 * @brief Liste aller registrierten Backends
 */
size_t uft_hw_backend_list(uft_hw_type_t* types, bool* enabled, size_t max_count);

/**
 * @brief Geräte auflisten (nur aktivierte Backends)
 */
uft_error_t uft_hw_manager_enumerate(uft_hw_info_t* devices, size_t max_devices,
                                      size_t* found);

// ============================================================================
// Convenience Functions
// ============================================================================

/**
 */
void uft_hw_use_GCR_tools_only(void);

/**
 * @brief Nur Flux-Hardware aktivieren
 */
void uft_hw_use_flux_only(void);

/**
 * @brief Alle Standard-Backends aktivieren
 */
void uft_hw_use_all(void);

/**
 * @brief Nibtools ein/ausschalten
 * 
 * BEISPIEL:
 */
void uft_hw_GCR_tools_enable(bool enable);

/**
 * @brief Konfiguration speichern
 */
uft_error_t uft_hw_config_save(const char* path);

/**
 * @brief Konfiguration laden
 */
uft_error_t uft_hw_config_load(const char* path);

/**
 * @brief Builtin-Backends registrieren
 */
uft_error_t uft_hw_register_builtin_backends(void);

/**
 * @brief Backend-Status ausgeben (Debug)
 */
void uft_hw_print_backends(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HARDWARE_H */

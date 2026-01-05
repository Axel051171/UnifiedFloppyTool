# UnifiedFloppyTool - Architektur-Refactoring-Plan

**Version:** 2.0  
**Datum:** Januar 2025  
**Status:** ELITE QA Architektur-Analyse

---

## 1. ZIEL-PIPELINE

```
┌─────────────────────────────────────────────────────────────────────────────────────────────────────┐
│                                    SAUBERE PIPELINE                                                 │
├─────────────────────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                                     │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    │
│   │ HARDWARE │───▶│  FLUX    │───▶│ DECODER  │───▶│  IMAGE   │───▶│  EXPORT  │───▶│   GUI    │    │
│   │ ADAPTER  │    │ BUFFER   │    │ LAYER    │    │  MODEL   │    │  LAYER   │    │  LAYER   │    │
│   └──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘    │
│        │               │               │               │               │               │          │
│        ▼               ▼               ▼               ▼               ▼               ▼          │
│   ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐    │
│   │Greaseweaz│    │FluxStream│    │MFM/FM/GCR│    │ Unified  │    │ Format   │    │ ViewModel│    │
│   │KryoFlux  │    │RawBits   │    │ PLL      │    │  Image   │    │ Writers  │    │ Bindings │    │
│   │FluxEngin │    │Revolutions│   │ DPLL     │    │Container │    │ Tools    │    │ Signals  │    │
│   │SuperCard │    │IndexPulse│    │ Adaptive │    │          │    │          │    │          │    │
│   └──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘    └──────────┘    │
│                                                                                                     │
└─────────────────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. FEHLENDE KERN-MODULE

### 2.1 Aktuelle Lücken

| Modul | Status | Problem |
|-------|--------|---------|
| **Unified Image Model** | ❌ FEHLT | `uft_disk_t` und `flux_disk_t` sind getrennt |
| **Flux Buffer Layer** | ⚠️ FRAGMENTIERT | Flux-Daten direkt in Track, keine Abstraktion |
| **Decoder Registry** | ⚠️ VERSTREUT | MFM/FM/GCR Decoder ohne einheitliches Interface |
| **Export Pipeline** | ⚠️ AD-HOC | Direkte Format-Konvertierung ohne Zwischenschicht |
| **Tool Adapter Interface** | ⚠️ INKONSISTENT | nibtools/fdutils haben keine einheitliche API |

### 2.2 Neue Module (zu implementieren)

```c
// ═══════════════════════════════════════════════════════════════════════════════
// MODUL 1: Unified Image Container
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @file uft_image.h
 * @brief Einheitlicher Container für alle Image-Typen
 */

typedef enum uft_image_layer {
    UFT_LAYER_FLUX,      // Raw flux transitions (SCP, KryoFlux, GW)
    UFT_LAYER_BITSTREAM, // Decoded bits (MFM/FM/GCR encoded)
    UFT_LAYER_SECTOR,    // Decoded sectors
    UFT_LAYER_BLOCK,     // Logical blocks (für Filesysteme)
} uft_image_layer_t;

typedef struct uft_image {
    // Identifikation
    char*               path;
    uint32_t            flags;
    
    // Geometrie (gemeinsam für alle Layer)
    uft_geometry_t      geometry;
    
    // Layer-Daten (lazy loaded)
    struct {
        bool            available;
        bool            dirty;
        void*           data;
        size_t          size;
    } layers[4];
    
    // Flux-spezifisch (wenn UFT_LAYER_FLUX verfügbar)
    struct {
        uint32_t        sample_rate_hz;
        uint32_t        index_count;
        double          rpm_measured;
    } flux_info;
    
    // Sektor-spezifisch (wenn UFT_LAYER_SECTOR verfügbar)
    struct {
        uft_encoding_t  encoding;
        uft_format_t    detected_format;
        int             confidence;
    } sector_info;
    
    // Provider (welches Plugin/Backend)
    const void*         provider;
    void*               provider_data;
    
    // Callbacks
    uft_log_fn          log_fn;
    void*               log_user;
} uft_image_t;

// API
uft_image_t* uft_image_create(void);
void         uft_image_destroy(uft_image_t* img);

// Layer-Zugriff
bool         uft_image_has_layer(const uft_image_t* img, uft_image_layer_t layer);
uft_error_t  uft_image_decode_to_layer(uft_image_t* img, uft_image_layer_t target);
uft_error_t  uft_image_encode_from_layer(uft_image_t* img, uft_image_layer_t source);

// Track-Zugriff (polymorphisch je nach Layer)
uft_error_t  uft_image_get_flux_track(uft_image_t* img, int cyl, int head,
                                       uft_flux_track_t* flux);
uft_error_t  uft_image_get_sector_track(uft_image_t* img, int cyl, int head,
                                         uft_track_t* track);

// ═══════════════════════════════════════════════════════════════════════════════
// MODUL 2: Flux Buffer Layer
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @file uft_flux_buffer.h
 * @brief Abstraktion für Flux-Daten (Hardware-unabhängig)
 */

typedef struct uft_flux_sample {
    uint32_t    time_ns;        // Zeit seit letzter Transition
    uint8_t     flags;          // Index, Weak, etc.
} uft_flux_sample_t;

typedef struct uft_flux_revolution {
    uft_flux_sample_t*  samples;
    size_t              sample_count;
    uint64_t            total_time_ns;
    uint32_t            index_offset;   // Sample-Index des Index-Pulses
} uft_flux_revolution_t;

typedef struct uft_flux_track {
    int                     cylinder;
    int                     head;
    
    // Multi-Revolution Support
    uft_flux_revolution_t*  revolutions;
    size_t                  revolution_count;
    
    // Statistik
    double                  avg_rpm;
    double                  rpm_variance;
    uint32_t                weak_bit_count;
    
    // Quelle
    enum {
        UFT_FLUX_SOURCE_HARDWARE,
        UFT_FLUX_SOURCE_SCP,
        UFT_FLUX_SOURCE_KRYOFLUX,
        UFT_FLUX_SOURCE_A2R,
        UFT_FLUX_SOURCE_SYNTHETIC
    } source;
    
    uint32_t                source_sample_rate;
} uft_flux_track_t;

// API
uft_flux_track_t* uft_flux_track_create(int cyl, int head, size_t max_revolutions);
void              uft_flux_track_destroy(uft_flux_track_t* track);
uft_error_t       uft_flux_track_add_revolution(uft_flux_track_t* track,
                                                 const uint32_t* samples,
                                                 size_t count,
                                                 uint32_t sample_rate_hz);
uft_error_t       uft_flux_track_normalize(uft_flux_track_t* track,
                                            uint32_t target_rate_hz);

// ═══════════════════════════════════════════════════════════════════════════════
// MODUL 3: Decoder Registry
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @file uft_decoder_registry.h
 * @brief Einheitliches Decoder-Interface
 */

typedef struct uft_decoder_ops {
    const char*     name;
    uft_encoding_t  encoding;       // MFM, FM, GCR_C64, GCR_APPLE, etc.
    
    // Probe: Kann dieser Decoder die Daten verarbeiten?
    int (*probe)(const uft_flux_track_t* flux, int* confidence);
    
    // Decode: Flux → Sectors
    uft_error_t (*decode_track)(const uft_flux_track_t* flux,
                                 uft_track_t* sectors,
                                 const uft_decode_options_t* opts);
    
    // Encode: Sectors → Flux (optional)
    uft_error_t (*encode_track)(const uft_track_t* sectors,
                                 uft_flux_track_t* flux,
                                 const uft_encode_options_t* opts);
    
    // PLL-Parameter
    const uft_pll_params_t* default_pll_params;
} uft_decoder_ops_t;

// Registry API
uft_error_t uft_decoder_register(const uft_decoder_ops_t* decoder);
const uft_decoder_ops_t* uft_decoder_find_by_encoding(uft_encoding_t enc);
const uft_decoder_ops_t* uft_decoder_auto_detect(const uft_flux_track_t* flux);

// ═══════════════════════════════════════════════════════════════════════════════
// MODUL 4: Tool Adapter Interface
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @file uft_tool_adapter.h
 * @brief Einheitliches Interface für externe Tools (nibtools, fdutils, etc.)
 */

typedef struct uft_tool_adapter {
    const char*     name;           // "nibtools", "fdutils", "gw"
    const char*     version;
    
    // Capabilities
    uint32_t        supported_formats;  // Bitmaske von uft_format_t
    bool            can_read;
    bool            can_write;
    bool            requires_hardware;
    
    // Lifecycle
    uft_error_t (*init)(void** context);
    void        (*cleanup)(void* context);
    
    // Operations
    uft_error_t (*read_disk)(void* context,
                              const uft_tool_read_params_t* params,
                              uft_image_t* output);
    
    uft_error_t (*write_disk)(void* context,
                               const uft_tool_write_params_t* params,
                               const uft_image_t* input);
    
    uft_error_t (*convert)(void* context,
                            const char* input_path,
                            const char* output_path,
                            uft_format_t output_format);
    
    // Progress callback
    void (*set_progress_callback)(void* context, uft_progress_fn fn, void* user);
} uft_tool_adapter_t;

// Registry
uft_error_t uft_tool_register(const uft_tool_adapter_t* adapter);
const uft_tool_adapter_t* uft_tool_find(const char* name);
const uft_tool_adapter_t* uft_tool_find_for_format(uft_format_t format);

// ═══════════════════════════════════════════════════════════════════════════════
// MODUL 5: Export Pipeline
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @file uft_export.h
 * @brief Export-Pipeline mit Transformation-Chain
 */

typedef struct uft_export_step {
    enum {
        UFT_EXPORT_DECODE,      // Flux → Sectors
        UFT_EXPORT_ENCODE,      // Sectors → Flux
        UFT_EXPORT_CONVERT,     // Format A → Format B
        UFT_EXPORT_FILTER,      // Filterung (z.B. nur bestimmte Tracks)
        UFT_EXPORT_VERIFY       // Verifikation nach Schreiben
    } type;
    
    union {
        struct { uft_encoding_t encoding; } decode;
        struct { uft_encoding_t encoding; } encode;
        struct { uft_format_t target; } convert;
        struct { int start_track, end_track, start_head, end_head; } filter;
        struct { bool byte_by_byte; } verify;
    } params;
} uft_export_step_t;

typedef struct uft_export_pipeline {
    uft_export_step_t*  steps;
    size_t              step_count;
    
    uft_progress_fn     progress_fn;
    void*               progress_user;
    
    uft_log_fn          log_fn;
    void*               log_user;
} uft_export_pipeline_t;

// API
uft_export_pipeline_t* uft_export_create(void);
void                   uft_export_destroy(uft_export_pipeline_t* pipe);
uft_error_t            uft_export_add_step(uft_export_pipeline_t* pipe,
                                            const uft_export_step_t* step);
uft_error_t            uft_export_execute(uft_export_pipeline_t* pipe,
                                           const uft_image_t* input,
                                           const char* output_path);
```

---

## 3. IMPLIZITE KOPPLUNGEN UND TRENNUNG

### 3.1 Aktuelle Probleme

```
╔═══════════════════════════════════════════════════════════════════════════════════════════╗
║                              IMPLIZITE KOPPLUNGEN (IST)                                   ║
╠═══════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                           ║
║   GUI ──────────────────────────────────────────────────────────────────────────────┐    ║
║     │                                                                                │    ║
║     │ PROBLEM: GUI-Header kennen Hardware-Konstanten                                │    ║
║     │ uft_gui_kryoflux_style.h → KryoFlux-spezifische UI-Definitionen              │    ║
║     │ uft_gw_official_params.h → Greaseweazle Parameter-Strukturen                 │    ║
║     │                                                                                │    ║
║     ▼                                                                                │    ║
║   Hardware ─────────────────────────────────────────────────────────────────────────┤    ║
║     │                                                                                │    ║
║     │ PROBLEM: Hardware kennt Format-Empfehlungen                                   │    ║
║     │ uft_hardware.c:654 → uft_hw_recommended_format()                              │    ║
║     │ Direkte Referenzen auf UFT_FORMAT_D64, UFT_FORMAT_ADF, etc.                   │    ║
║     │                                                                                │    ║
║     ▼                                                                                │    ║
║   Decoder ──────────────────────────────────────────────────────────────────────────┤    ║
║     │                                                                                │    ║
║     │ PROBLEM: Decoder sind nicht registriert, sondern direkt aufgerufen            │    ║
║     │ Kein einheitliches Interface für MFM/FM/GCR                                   │    ║
║     │                                                                                │    ║
║     ▼                                                                                │    ║
║   Format ────────────────────────────────────────────────────────────────────────────┘    ║
║                                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════════════════════╝
```

### 3.2 Trennungsstrategie

```c
// ═══════════════════════════════════════════════════════════════════════════════
// LAYER SEPARATION: Interfaces definieren
// ═══════════════════════════════════════════════════════════════════════════════

// SCHICHT 1: GUI kennt nur abstrakte "Device" und "Image"
// ─────────────────────────────────────────────────────────
// include/uft/gui/uft_gui_device.h

typedef struct uft_gui_device_info {
    char        name[64];
    char        port[32];
    char        firmware_version[16];
    uint32_t    capabilities;       // Abstrakte Flags, keine GW/KF-spezifisch
    bool        connected;
} uft_gui_device_info_t;

// GUI fragt NIEMALS nach Hardware-Details
// Stattdessen: Observer-Pattern für Status-Updates

typedef void (*uft_gui_device_callback_t)(
    void* user,
    const uft_gui_device_info_t* device,
    uft_gui_event_t event
);

// ─────────────────────────────────────────────────────────
// SCHICHT 2: Core vermittelt zwischen GUI und Hardware
// ─────────────────────────────────────────────────────────
// include/uft/uft_device_manager.h

typedef struct uft_device_manager uft_device_manager_t;

uft_device_manager_t* uft_device_manager_create(void);
void uft_device_manager_destroy(uft_device_manager_t* mgr);

// GUI registriert sich für Updates
void uft_device_manager_set_callback(uft_device_manager_t* mgr,
                                      uft_gui_device_callback_t cb,
                                      void* user);

// GUI fordert Scan an (asynchron)
uft_error_t uft_device_manager_scan_async(uft_device_manager_t* mgr);

// GUI wählt Device (abstrakt, nicht "Greaseweazle auf COM3")
uft_error_t uft_device_manager_select(uft_device_manager_t* mgr,
                                       int device_index);

// ─────────────────────────────────────────────────────────
// SCHICHT 3: Hardware ist vollständig gekapselt
// ─────────────────────────────────────────────────────────
// Hardware-spezifische Details sind PRIVATE (nicht in Public Headers)

// FALSCH (aktuell):
// #include "uft_greaseweazle.h"  // In GUI-Code!

// RICHTIG:
// GUI inkludiert nur uft_device_manager.h
// Device Manager inkludiert intern uft_hardware_internal.h
// Hardware-Backends sind Plugins, nicht statisch gelinkt

// ═══════════════════════════════════════════════════════════════════════════════
// FORMAT-EMPFEHLUNGEN: Von Hardware in Core verschieben
// ═══════════════════════════════════════════════════════════════════════════════

// FALSCH (aktuell in hardware.c):
uft_format_t uft_hw_recommended_format(uft_hw_type_t hw_type, ...);

// RICHTIG: Format-Logik gehört in Core
// include/uft/uft_format_advisor.h

typedef struct uft_format_advice {
    uft_format_t    recommended_format;
    uft_format_t    alternatives[8];
    int             alternative_count;
    const char*     reason;
} uft_format_advice_t;

uft_error_t uft_get_format_advice(
    const uft_image_t* image,           // Was haben wir?
    uft_format_advice_t* advice         // Was empfehlen wir?
);

// Hardware stellt nur fest: "Ich kann Flux lesen"
// Core entscheidet: "Bei Commodore-Drive → G64 empfohlen"
```

---

## 4. STABILE APIs FÜR TOOL-AUSTAUSCHBARKEIT

### 4.1 Anforderungen

```
╔═══════════════════════════════════════════════════════════════════════════════════════════╗
║                    TOOL-INTEROPERABILITÄT: REQUIREMENTS                                   ║
╠═══════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                           ║
║   TOOLS:                          MUSS UNTERSTÜTZEN:                                      ║
║   ──────                          ──────────────────                                      ║
║   Greaseweazle CLI (gw)           Flux read/write, SCP/HFE export                        ║
║   FluxEngine CLI                  Flux read/write, Multi-Format                          ║
║   KryoFlux DTC                    Stream read, RAW export                                ║
║   nibtools                        G64/D64 read/write, Commodore-spezifisch               ║
║   fdutils (setfdprm, etc.)        Linux FD access, IMG/DSK                               ║
║   adftools                        ADF read/write, Amiga-spezifisch                       ║
║   hxcfe                           Universal converter                                     ║
║   disk-analyse                    Analyse, keine Schreib-Ops                             ║
║                                                                                           ║
║   GEMEINSAME API MUSS:                                                                    ║
║   ────────────────────                                                                    ║
║   1. Flux-Daten abstrahieren (Sample-Rate-unabhängig)                                    ║
║   2. Sektor-Daten abstrahieren (Encoding-unabhängig)                                     ║
║   3. Progress/Cancel unterstützen                                                        ║
║   4. Fehler einheitlich zurückgeben                                                      ║
║   5. Keine Tool-spezifischen Datentypen in Public API                                    ║
║                                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════════════════════╝
```

### 4.2 Stabile API-Definitionen

```c
// ═══════════════════════════════════════════════════════════════════════════════
// STABILE API: Tool-Unabhängige Operationen
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @file uft_operations.h
 * @brief Tool-unabhängige High-Level Operationen
 * 
 * Diese API ist STABIL und darf nicht gebrochen werden!
 * Version: 1.0
 */

#ifndef UFT_OPERATIONS_H
#define UFT_OPERATIONS_H

#include "uft_image.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Version und Kompatibilität
// ─────────────────────────────────────────────────────────────────────────────

#define UFT_API_VERSION_MAJOR   1
#define UFT_API_VERSION_MINOR   0
#define UFT_API_VERSION         ((UFT_API_VERSION_MAJOR << 16) | UFT_API_VERSION_MINOR)

/**
 * @brief Prüft API-Kompatibilität
 * @return true wenn kompatibel
 */
bool uft_api_compatible(uint32_t client_version);

// ─────────────────────────────────────────────────────────────────────────────
// Disk-Operationen (Tool-unabhängig)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Liest eine Disk von Hardware
 * 
 * Wählt automatisch das beste verfügbare Tool/Backend.
 * 
 * @param device_id  Device-Selektor (0 = Auto, oder spezifischer Index)
 * @param params     Lese-Parameter (NULL = Defaults)
 * @param output     Ausgabe-Image
 * @return UFT_OK bei Erfolg
 */
uft_error_t uft_read_disk(
    int device_id,
    const uft_read_params_t* params,
    uft_image_t* output
);

/**
 * @brief Schreibt eine Disk auf Hardware
 */
uft_error_t uft_write_disk(
    int device_id,
    const uft_write_params_t* params,
    const uft_image_t* input
);

/**
 * @brief Öffnet ein Disk-Image aus Datei
 * 
 * Format wird automatisch erkannt.
 */
uft_error_t uft_open_image(
    const char* path,
    uft_image_t* output
);

/**
 * @brief Speichert ein Disk-Image
 * 
 * @param format UFT_FORMAT_AUTO = behalte Original, sonst konvertiere
 */
uft_error_t uft_save_image(
    const uft_image_t* image,
    const char* path,
    uft_format_t format
);

/**
 * @brief Konvertiert zwischen Formaten
 * 
 * Automatische Layer-Konvertierung:
 * - Flux → Sector wenn nötig (via beste verfügbare Decoder)
 * - Sector → Flux wenn nötig (via beste verfügbare Encoder)
 */
uft_error_t uft_convert_image(
    const uft_image_t* input,
    uft_format_t target_format,
    uft_image_t* output
);

// ─────────────────────────────────────────────────────────────────────────────
// Parameter-Strukturen (ABI-stabil)
// ─────────────────────────────────────────────────────────────────────────────

typedef struct uft_read_params {
    uint32_t    struct_size;        // Für Versionierung: sizeof(uft_read_params_t)
    
    // Track-Bereich
    int         start_track;        // -1 = Auto
    int         end_track;          // -1 = Auto
    int         start_head;         // -1 = Alle
    int         end_head;
    
    // Qualität
    int         retries;            // 0 = Default
    int         revolutions;        // Für Flux: Anzahl Umdrehungen
    
    // Callbacks
    uft_progress_fn progress_fn;
    void*           progress_user;
    
    // Reserviert für zukünftige Erweiterungen
    uint8_t     reserved[64];
} uft_read_params_t;

typedef struct uft_write_params {
    uint32_t    struct_size;
    
    bool        verify_after_write;
    bool        format_before_write;
    bool        erase_empty_tracks;
    
    uft_progress_fn progress_fn;
    void*           progress_user;
    
    uint8_t     reserved[64];
} uft_write_params_t;

// ─────────────────────────────────────────────────────────────────────────────
// Tool-Auswahl (optional, normalerweise Auto)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Listet verfügbare Tools/Backends
 */
uft_error_t uft_list_tools(
    uft_tool_info_t* tools,
    size_t max_tools,
    size_t* actual_count
);

/**
 * @brief Erzwingt Verwendung eines bestimmten Tools
 * 
 * @param tool_name NULL = Auto, sonst "gw", "nibtools", etc.
 */
uft_error_t uft_set_preferred_tool(const char* tool_name);

#ifdef __cplusplus
}
#endif

#endif // UFT_OPERATIONS_H
```

---

## 5. UNIFIED IMAGE MODEL

### 5.1 Design-Prinzipien

```
╔═══════════════════════════════════════════════════════════════════════════════════════════╗
║                         UNIFIED IMAGE MODEL DESIGN                                        ║
╠═══════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                           ║
║   ANFORDERUNG: Ein Container für ALLE Disk-Typen                                          ║
║                                                                                           ║
║   ┌────────────────────────────────────────────────────────────────────────────────────┐  ║
║   │                           uft_image_t                                              │  ║
║   │                                                                                    │  ║
║   │   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐   ┌─────────────┐          │  ║
║   │   │ FLUX LAYER  │   │  BITSTREAM  │   │   SECTOR    │   │   BLOCK     │          │  ║
║   │   │             │   │   LAYER     │   │   LAYER     │   │   LAYER     │          │  ║
║   │   │ • SCP       │   │             │   │             │   │             │          │  ║
║   │   │ • KryoFlux  │──▶│ • MFM bits  │──▶│ • ID fields │──▶│ • LBA       │          │  ║
║   │   │ • A2R       │   │ • FM bits   │   │ • Data      │   │ • Clusters  │          │  ║
║   │   │ • GW flux   │   │ • GCR bits  │   │ • CRC       │   │ • Files     │          │  ║
║   │   │             │   │             │   │             │   │             │          │  ║
║   │   └─────────────┘   └─────────────┘   └─────────────┘   └─────────────┘          │  ║
║   │         ▲                 ▲                 ▲                 ▲                   │  ║
║   │         │                 │                 │                 │                   │  ║
║   │         └────────────┬────┴─────────┬───────┴─────────┬───────┘                   │  ║
║   │                      │              │                 │                           │  ║
║   │              ┌───────▼──────┐ ┌─────▼─────┐ ┌─────────▼─────────┐                │  ║
║   │              │   DECODER    │ │  FORMAT   │ │    FILESYSTEM     │                │  ║
║   │              │   PLUGINS    │ │  PLUGINS  │ │      PLUGINS      │                │  ║
║   │              └──────────────┘ └───────────┘ └───────────────────┘                │  ║
║   │                                                                                    │  ║
║   └────────────────────────────────────────────────────────────────────────────────────┘  ║
║                                                                                           ║
║   LAZY LOADING:                                                                           ║
║   • Layer werden erst bei Bedarf berechnet                                               ║
║   • Flux → Bitstream → Sector on-demand                                                  ║
║   • Caching für wiederholte Zugriffe                                                     ║
║                                                                                           ║
║   BI-DIREKTIONAL:                                                                        ║
║   • Sector → Bitstream → Flux für Schreiben                                              ║
║   • Encoder-Plugins symmetrisch zu Decoder-Plugins                                       ║
║                                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════════════════════╝
```

### 5.2 Implementierung

```c
// ═══════════════════════════════════════════════════════════════════════════════
// UNIFIED IMAGE MODEL: Vollständige Implementierung
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @file uft_unified_image.h
 * @brief Einheitliches Image-Modell für Sektor- und Flux-Daten
 */

#ifndef UFT_UNIFIED_IMAGE_H
#define UFT_UNIFIED_IMAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "uft_types.h"
#include "uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Layer-Definitionen
// ─────────────────────────────────────────────────────────────────────────────

typedef enum uft_layer {
    UFT_LAYER_NONE      = 0,
    UFT_LAYER_FLUX      = (1 << 0),  // Raw flux transitions
    UFT_LAYER_BITSTREAM = (1 << 1),  // Decoded bits (vor Sektor-Parsing)
    UFT_LAYER_SECTOR    = (1 << 2),  // Dekodierte Sektoren
    UFT_LAYER_BLOCK     = (1 << 3),  // Logische Blöcke (nach Interleave)
    UFT_LAYER_FILE      = (1 << 4),  // Filesystem-Ebene
} uft_layer_t;

// ─────────────────────────────────────────────────────────────────────────────
// Flux Layer Strukturen
// ─────────────────────────────────────────────────────────────────────────────

#define UFT_FLUX_FLAG_INDEX     (1 << 0)  // Index-Puls an dieser Position
#define UFT_FLUX_FLAG_WEAK      (1 << 1)  // Schwaches/variables Signal
#define UFT_FLUX_FLAG_MISSING   (1 << 2)  // Fehlende Transition (Sync)

typedef struct uft_flux_transition {
    uint32_t    delta_ns;   // Zeit seit letzter Transition in Nanosekunden
    uint8_t     flags;
} uft_flux_transition_t;

typedef struct uft_flux_revolution {
    uft_flux_transition_t*  transitions;
    size_t                  transition_count;
    size_t                  transition_capacity;
    
    uint64_t                total_time_ns;
    uint32_t                index_position;     // Transition-Index des Index-Pulses
    double                  rpm;                // Berechnete RPM
} uft_flux_revolution_t;

typedef struct uft_flux_track_data {
    int                     cylinder;
    int                     head;
    
    uft_flux_revolution_t*  revolutions;
    size_t                  revolution_count;
    size_t                  revolution_capacity;
    
    // Statistik (berechnet beim Hinzufügen)
    double                  avg_rpm;
    double                  rpm_stddev;
    uint32_t                total_transitions;
    
    // Quell-Information
    uint32_t                source_sample_rate_hz;
    const char*             source_format;      // "SCP", "KryoFlux", etc.
} uft_flux_track_data_t;

// ─────────────────────────────────────────────────────────────────────────────
// Bitstream Layer Strukturen
// ─────────────────────────────────────────────────────────────────────────────

typedef struct uft_bitstream_track {
    int                 cylinder;
    int                 head;
    
    uint8_t*            bits;           // Gepackte Bits (MSB first)
    size_t              bit_count;
    size_t              byte_capacity;
    
    uft_encoding_t      encoding;       // MFM, FM, GCR_CBM, GCR_APPLE
    uint32_t            data_rate_bps;  // Bits pro Sekunde (250000 für DD MFM)
    
    // Sync-Positionen (für schnelles Sektor-Finden)
    uint32_t*           sync_positions;
    size_t              sync_count;
} uft_bitstream_track_t;

// ─────────────────────────────────────────────────────────────────────────────
// Sector Layer (existiert bereits als uft_track_t)
// ─────────────────────────────────────────────────────────────────────────────

// Verwende bestehende uft_track_t und uft_sector_t

// ─────────────────────────────────────────────────────────────────────────────
// Unified Track Container
// ─────────────────────────────────────────────────────────────────────────────

typedef struct uft_unified_track {
    int                     cylinder;
    int                     head;
    
    // Verfügbare Layer (Bitmaske)
    uint32_t                available_layers;
    
    // Layer-Daten (NULL wenn nicht verfügbar/geladen)
    uft_flux_track_data_t*  flux;
    uft_bitstream_track_t*  bitstream;
    uft_track_t*            sectors;
    
    // Dirty-Flags (für Write-Back)
    uint32_t                dirty_layers;
    
    // Ursprünglicher Layer (woher kamen die Daten?)
    uft_layer_t             source_layer;
} uft_unified_track_t;

// ─────────────────────────────────────────────────────────────────────────────
// Unified Image Container
// ─────────────────────────────────────────────────────────────────────────────

typedef struct uft_unified_image {
    // ──── Metadaten ────
    char*                   path;
    uft_format_t            source_format;
    uft_format_t            detected_format;
    int                     detection_confidence;
    
    // ──── Geometrie ────
    uft_geometry_t          geometry;
    
    // ──── Track-Array ────
    uft_unified_track_t**   tracks;         // [cyl * heads + head]
    size_t                  track_count;
    
    // ──── Globale Layer-Info ────
    uint32_t                available_layers;   // OR aller Track-Layer
    uft_layer_t             primary_layer;      // Ursprünglicher Daten-Layer
    
    // ──── Flux-Metadaten (wenn Flux verfügbar) ────
    struct {
        uint32_t            sample_rate_hz;
        double              avg_rpm;
        const char*         capture_tool;       // "Greaseweazle", "KryoFlux"
        char*               capture_date;
    } flux_meta;
    
    // ──── Sektor-Metadaten (wenn Sectors verfügbar) ────
    struct {
        uft_encoding_t      encoding;
        uint32_t            total_sectors;
        uint32_t            bad_sectors;
        uint32_t            weak_sectors;
    } sector_meta;
    
    // ──── State ────
    bool                    read_only;
    bool                    modified;
    
    // ──── Provider (Format-Plugin oder Hardware-Backend) ────
    const void*             provider;
    void*                   provider_data;
    
    // ──── Callbacks ────
    uft_log_fn              log_fn;
    void*                   log_user;
} uft_unified_image_t;

// ─────────────────────────────────────────────────────────────────────────────
// API: Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Erstellt ein leeres Image
 */
uft_unified_image_t* uft_image_create(void);

/**
 * @brief Zerstört ein Image und gibt alle Ressourcen frei
 */
void uft_image_destroy(uft_unified_image_t* img);

/**
 * @brief Öffnet ein Image aus Datei
 * 
 * Format wird automatisch erkannt.
 * Layer werden lazy geladen.
 */
uft_error_t uft_image_open(uft_unified_image_t* img, const char* path);

/**
 * @brief Speichert ein Image
 * 
 * @param format Ziel-Format (UFT_FORMAT_AUTO = behalte Original)
 */
uft_error_t uft_image_save(uft_unified_image_t* img, 
                            const char* path, 
                            uft_format_t format);

// ─────────────────────────────────────────────────────────────────────────────
// API: Layer-Management
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Prüft ob ein Layer verfügbar ist
 */
bool uft_image_has_layer(const uft_unified_image_t* img, uft_layer_t layer);

/**
 * @brief Erzeugt einen Layer durch Dekodierung/Enkodierung
 * 
 * Beispiele:
 * - FLUX → BITSTREAM: PLL-Dekodierung
 * - BITSTREAM → SECTOR: Sektor-Parsing
 * - SECTOR → BITSTREAM: Enkodierung
 * - BITSTREAM → FLUX: Flux-Synthese
 */
uft_error_t uft_image_ensure_layer(uft_unified_image_t* img, uft_layer_t layer);

/**
 * @brief Entfernt einen Layer (um Speicher zu sparen)
 * 
 * Layer muss neu berechnet werden wenn wieder benötigt.
 */
void uft_image_drop_layer(uft_unified_image_t* img, uft_layer_t layer);

// ─────────────────────────────────────────────────────────────────────────────
// API: Track-Zugriff
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Holt einen Track (lädt bei Bedarf)
 */
uft_unified_track_t* uft_image_get_track(uft_unified_image_t* img, 
                                          int cyl, int head);

/**
 * @brief Holt Flux-Daten für einen Track
 * 
 * Lädt/berechnet bei Bedarf.
 */
uft_error_t uft_image_get_flux_track(uft_unified_image_t* img,
                                      int cyl, int head,
                                      uft_flux_track_data_t** flux);

/**
 * @brief Holt Sektor-Daten für einen Track
 */
uft_error_t uft_image_get_sector_track(uft_unified_image_t* img,
                                        int cyl, int head,
                                        uft_track_t** sectors);

// ─────────────────────────────────────────────────────────────────────────────
// API: Konvertierung
// ─────────────────────────────────────────────────────────────────────────────

/**
 * @brief Konvertiert Image zu anderem Format
 * 
 * Erstellt eine Kopie, Original bleibt unverändert.
 */
uft_error_t uft_image_convert(const uft_unified_image_t* src,
                               uft_format_t target_format,
                               uft_unified_image_t* dst);

/**
 * @brief Prüft ob Konvertierung möglich ist
 * 
 * @param loss_info Beschreibung was verloren geht (kann NULL sein)
 */
bool uft_image_can_convert(const uft_unified_image_t* src,
                            uft_format_t target_format,
                            char* loss_info, size_t loss_info_size);

#ifdef __cplusplus
}
#endif

#endif // UFT_UNIFIED_IMAGE_H
```

---

## 6. REFACTORING-ROADMAP

### Phase 1: Foundation (1 Woche)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 1: FOUNDATION                                                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 1.1 Unified Image Header erstellen                                          │
│     → include/uft/uft_unified_image.h                                       │
│     → include/uft/uft_flux_buffer.h                                         │
│                                                                             │
│ 1.2 Layer-Abstraktionen implementieren                                      │
│     → src/core/uft_unified_image.c                                          │
│     → src/core/uft_flux_buffer.c                                            │
│                                                                             │
│ 1.3 Bestehende Strukturen als Legacy markieren                              │
│     → uft_disk_t → UFT_DEPRECATED                                           │
│     → flux_disk_t → UFT_DEPRECATED                                          │
│                                                                             │
│ 1.4 Migration-Adapter erstellen                                             │
│     → uft_disk_to_unified()                                                 │
│     → uft_unified_to_disk()                                                 │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Phase 2: Decoder Registry (1 Woche)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 2: DECODER REGISTRY                                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 2.1 Decoder-Interface definieren                                            │
│     → include/uft/uft_decoder_registry.h                                    │
│                                                                             │
│ 2.2 Bestehende Decoder migrieren                                            │
│     → MFM Decoder → uft_decoder_mfm                                         │
│     → FM Decoder → uft_decoder_fm                                           │
│     → GCR Decoder (C64) → uft_decoder_gcr_cbm                              │
│     → GCR Decoder (Apple) → uft_decoder_gcr_apple                          │
│                                                                             │
│ 2.3 Auto-Detection implementieren                                           │
│     → Probe-Funktionen pro Decoder                                          │
│     → Score-basierte Auswahl                                                │
│                                                                             │
│ 2.4 PLL-Parameter konsolidieren                                             │
│     → Ein PLL-Config pro Encoding-Typ                                       │
│     → Default-Werte dokumentieren                                           │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Phase 3: Layer Separation (1 Woche)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 3: LAYER SEPARATION                                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 3.1 GUI-Abstraktion                                                         │
│     → include/uft/gui/uft_gui_types.h (nur abstrakte Typen)                │
│     → Entfernen aller Hardware-Referenzen aus GUI-Headers                   │
│                                                                             │
│ 3.2 Device Manager                                                          │
│     → src/core/uft_device_manager.c                                         │
│     → Observer-Pattern für GUI-Updates                                      │
│                                                                             │
│ 3.3 Hardware Isolation                                                      │
│     → Hardware-Details nur in src/hardware/                                 │
│     → Public API nur über uft_operations.h                                  │
│                                                                             │
│ 3.4 Format-Advisor (aus Hardware entfernen)                                 │
│     → src/core/uft_format_advisor.c                                         │
│     → Format-Empfehlungen basierend auf Image-Inhalt                        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Phase 4: Tool Adapters (1 Woche)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ PHASE 4: TOOL ADAPTERS                                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│ 4.1 Tool Adapter Interface                                                  │
│     → include/uft/uft_tool_adapter.h                                        │
│                                                                             │
│ 4.2 Adapter implementieren                                                  │
│     → src/tools/uft_tool_gw.c (Greaseweazle CLI)                           │
│     → src/tools/uft_tool_nibtools.c                                         │
│     → src/tools/uft_tool_hxcfe.c                                            │
│     → src/tools/uft_tool_fdutils.c                                          │
│                                                                             │
│ 4.3 Tool Registry                                                           │
│     → Automatische Tool-Detection                                           │
│     → Fallback-Kette bei Fehler                                             │
│                                                                             │
│ 4.4 Einheitliche Progress/Cancel                                            │
│     → Thread-safe Callbacks                                                 │
│     → Cleanup bei Cancel                                                    │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 7. ZUSAMMENFASSUNG

```
╔═══════════════════════════════════════════════════════════════════════════════════════════╗
║                           ARCHITEKTUR-REFACTORING SUMMARY                                 ║
╠═══════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                           ║
║   FEHLENDE KERN-MODULE:                                                                   ║
║   ─────────────────────                                                                   ║
║   ✗ Unified Image Model     → NEU: uft_unified_image_t                                   ║
║   ✗ Flux Buffer Layer       → NEU: uft_flux_track_data_t                                 ║
║   ✗ Decoder Registry        → NEU: uft_decoder_ops_t + Registry                          ║
║   ✗ Tool Adapter Interface  → NEU: uft_tool_adapter_t                                    ║
║   ✗ Export Pipeline         → NEU: uft_export_pipeline_t                                 ║
║                                                                                           ║
║   IMPLIZITE KOPPLUNGEN:                                                                   ║
║   ─────────────────────                                                                   ║
║   ✗ GUI → Hardware          → FIX: Device Manager als Vermittler                         ║
║   ✗ Hardware → Format       → FIX: Format Advisor in Core                                ║
║   ✗ Direkte Decoder-Aufrufe → FIX: Decoder Registry                                      ║
║                                                                                           ║
║   STABILE APIs:                                                                           ║
║   ─────────────                                                                           ║
║   → uft_operations.h        : High-Level API (Tool-unabhängig)                           ║
║   → uft_unified_image.h     : Image Container API                                        ║
║   → uft_tool_adapter.h      : Tool Integration API                                       ║
║   → uft_decoder_registry.h  : Decoder Plugin API                                         ║
║                                                                                           ║
║   UNIFIED IMAGE MODEL:                                                                    ║
║   ────────────────────                                                                    ║
║   → Layer-basiert: FLUX → BITSTREAM → SECTOR → BLOCK                                     ║
║   → Lazy Loading für Performance                                                         ║
║   → Bi-direktional (Decode + Encode)                                                     ║
║   → Unterstützt: SCP, KryoFlux, A2R, D64, G64, ADF, IMG, ...                            ║
║                                                                                           ║
║   GESCHÄTZTE ZEIT: 4 Wochen                                                              ║
║                                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════════════════════╝
```

---

*ELITE QA Architektur-Analyse - Januar 2025*

# UFT v3.7.0 - Modul- und Abhängigkeitskarte

**Erstellt:** 2026-01-09  
**Version:** 3.7.0  
**Build-Targets:** 65+ Libraries, 15+ Executables

---

## PHASE 1: Modul-Inventur

### A) Modul-Liste mit Klassifikation

#### 1. CORE (Kern-Infrastruktur)

| Modul | Library | Zweck | Public Headers |
|-------|---------|-------|----------------|
| **uft_core** | `libuft_core.a` | Grundlegende Typen, Error-Handling, Logging | `uft_types.h`, `uft_error.h`, `uft_version.h` |
| **uft_crc** | `libuft_crc.a` | CRC16/32 Berechnungen | `uft_crc.h` |
| **uft_encoding** | `libuft_encoding.a` | MFM/FM/GCR Encoding | `uft_encoding.h`, `encoding_detector.h` |
| **uft_sector** | `libuft_sector.a` | Sektor-Parsing & Validierung | `uft_sector_parser.h` |

#### 2. HAL (Hardware Abstraction Layer)

| Modul | Library | Zweck | Controller |
|-------|---------|-------|------------|
| **uft_hal** | `libuft_hal.a` | Hardware-Abstraktion | GW, FluxEngine, Kryoflux, FC5025, XUM1541 |
| **uft_greaseweazle** | (in uft_hal) | Greaseweazle-Protokoll | F1/F7 |
| **uft_kryoflux** | (in uft_hal) | KryoFlux DTC | USB |
| **uft_supercard** | (in uft_hal) | SuperCard Pro | SCP |
| **uft_nibtools** | (in uft_hal) | C64 Nibbler | XUM1541 |

#### 3. ANALYSIS (Track-Analyse & Profile)

| Modul | Library | Zweck | Profiles |
|-------|---------|-------|----------|
| **uft_track_analysis** | `libuft_track_analysis.a` | XCopy-Stil Analyse | 13 Built-in |
| **uft_profiles** | `libuft_profiles.a` | Plattform-Profile | 53 gesamt |
| **uft_amiga_protection** | `libuft_amiga_protection.a` | Kopierschutz-Erkennung | Rob Northen, etc. |

#### 4. FORMATS (Format-Parser/Writer)

| Modul | Library | Formate |
|-------|---------|---------|
| **uft_formats** | `libuft_formats.a` | D64, ADF, IMG, SCP, HFE, G64, ATR... |
| **uft_format_amiga_ext** | Erweiterungen | DMS, ADZ, ADFS |
| **uft_format_apple** | Apple | DO, PO, NIB, WOZ, 2IMG |
| **uft_format_atari** | Atari | ATR, XFD, ATX, STX |
| **uft_xdf** | `libuft_xdf.a` | IBM XDF, XXDF, DMF |

#### 5. DECODERS (Flux → Bitstream → Sectors)

| Modul | Library | Zweck |
|-------|---------|-------|
| **uft_decoder** | `libuft_decoder.a` | Single-Decoder Adapter |
| **uft_decoders** | `libuft_decoders.a` | Multi-Format Decoder |
| **uft_pll** | `libuft_pll.a` | PLL Flux-Dekodierung |
| **uft_fdc_bitstream** | `libuft_fdc_bitstream.a` | FDC Bitstream |

#### 6. TOOLS (Spezialisierte Werkzeuge)

| Modul | Library | Zweck |
|-------|---------|-------|
| **uft_tools** | `libuft_tools.a` | Allgemeine Tools |
| **uft_recovery** | `libuft_recovery.a` | Datenrettung |
| **uft_protection** | `libuft_protection.a` | Kopierschutz-Analyse |
| **uft_forensic** | `libuft_forensic.a` | Forensik-Features |
| **uft_formatid** | `libuft_formatid.a` | Format-Identifikation |

#### 7. PLATFORM-SPEZIFISCH

| Modul | Library | Plattform |
|-------|---------|-----------|
| **uft_c64** | `libuft_c64.a` | Commodore 64/128/VIC-20 |
| **uft_cbm** | OBJECT | CBM DOS |
| **uft_whdload** | OBJECT | Amiga WHDLoad |

#### 8. GUI (Qt6)

| Modul | Executable | Zweck |
|-------|------------|-------|
| **uft_gui** | `UnifiedFloppyTool` | Haupt-GUI |
| **XCopyPanel** | (Widget) | Disk-Kopier-Settings |
| **AnalyzerToolbar** | (Widget) | Track-Analyse Toolbar |
| **TrackAnalyzerWidget** | (Widget) | Heatmap-Visualisierung |
| **TrackAnalyzerBackend** | (C++ Wrapper) | C ↔ Qt Bridge |

#### 9. TESTS

| Modul | Executable | Coverage |
|-------|------------|----------|
| **smoke_version** | Test | Version API |
| **smoke_workflow** | Test | Load→Analyze→Convert |
| **test_xdf_xcopy** | Test | XDF ↔ XCopy Integration |
| **test_sector_parser** | Test | Sektor-Parsing |
| **test_c64_*** | Tests (4) | C64 Module |

---

## PHASE 2: Abhängigkeitsmatrix

### B) Modul → nutzt → Modul (über Header/API)

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                           DEPENDENCY MATRIX                                  │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  GUI Layer:                                                                  │
│  ┌─────────────────┐                                                         │
│  │ uft_gui         │──┬──► uft_track_analysis                               │
│  │ (Qt6 Widgets)   │  ├──► uft_profiles                                     │
│  │                 │  ├──► uft_formats                                       │
│  │ XCopyPanel      │  ├──► uft_xdf (für requires_track_copy)                │
│  │ AnalyzerToolbar │  ├──► uft_hal (optional, für Hardware)                 │
│  │ TrackAnalyzer   │  └──► uft_core                                         │
│  │ Backend         │                                                         │
│  └─────────────────┘                                                         │
│         │                                                                    │
│         ▼                                                                    │
│  Analysis Layer:                                                             │
│  ┌─────────────────┐   ┌─────────────────┐                                  │
│  │ uft_track_      │◄──│ uft_profiles    │ (53 Profile)                     │
│  │ analysis        │   │ - Japanese (5)  │                                  │
│  │                 │   │ - UK (12)       │                                  │
│  │ Sync-Detection  │   │ - US (10)       │                                  │
│  │ Type-Erkennung  │   │ - Misc (13)     │                                  │
│  │ Confidence      │   │ - XDF/DMF (3)   │                                  │
│  └────────┬────────┘   └─────────────────┘                                  │
│           │                                                                  │
│           ├──► uft_amiga_protection (Kopierschutz)                          │
│           └──► uft_encoding (MFM/GCR Detection)                             │
│                                                                              │
│  Format Layer:                                                               │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐            │
│  │ uft_formats     │   │ uft_xdf         │   │ uft_formatid    │            │
│  │ - D64, ADF, IMG │   │ - XDF (1.86MB)  │   │ - Auto-Detect   │            │
│  │ - SCP, HFE, G64 │   │ - XXDF (2M)     │   │ - FAT BPB       │            │
│  │ - TD0, IMD, DMK │   │ - DMF (1.68MB)  │   │ - Magic Bytes   │            │
│  └────────┬────────┘   └────────┬────────┘   └────────┬────────┘            │
│           │                     │                     │                      │
│           └─────────────────────┴─────────────────────┘                      │
│                                 │                                            │
│                                 ▼                                            │
│  Decoder Layer:                                                              │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐            │
│  │ uft_decoder     │◄──│ uft_pll         │◄──│ uft_fdc_        │            │
│  │                 │   │ - Phase-Lock    │   │ bitstream       │            │
│  │ Adapter API     │   │ - Weak-Bit Det  │   │                 │            │
│  └────────┬────────┘   └─────────────────┘   └─────────────────┘            │
│           │                                                                  │
│           ▼                                                                  │
│  HAL Layer:                                                                  │
│  ┌─────────────────────────────────────────────────────────────────────────┐│
│  │ uft_hal                                                                  ││
│  │ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐       ││
│  │ │Grease-   │ │KryoFlux  │ │SuperCard │ │FC5025    │ │XUM1541   │       ││
│  │ │weazle    │ │DTC       │ │Pro       │ │          │ │          │       ││
│  │ └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘       ││
│  └─────────────────────────────────────────────────────────────────────────┘│
│                                 │                                            │
│                                 ▼                                            │
│  Core Layer:                                                                 │
│  ┌─────────────────┐   ┌─────────────────┐   ┌─────────────────┐            │
│  │ uft_core        │◄──│ uft_crc         │   │ uft_sector      │            │
│  │ - Types         │   │ - CRC16/32      │   │ - ID Parsing    │            │
│  │ - Error         │   │ - Commodore     │   │ - DAM Parsing   │            │
│  │ - Version       │   │ - CCITT         │   │ - Validation    │            │
│  └─────────────────┘   └─────────────────┘   └─────────────────┘            │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

### C) Datenflussdiagramm (Pipeline)

```
┌────────────────────────────────────────────────────────────────────────────────┐
│                        UFT DATA FLOW PIPELINE                                  │
├────────────────────────────────────────────────────────────────────────────────┤
│                                                                                │
│  ┌─────────┐         ┌─────────┐         ┌─────────┐         ┌─────────┐     │
│  │ SOURCE  │         │ READ    │         │ ANALYZE │         │ DECIDE  │     │
│  └────┬────┘         └────┬────┘         └────┬────┘         └────┬────┘     │
│       │                   │                   │                   │           │
│       ▼                   ▼                   ▼                   ▼           │
│  ┌─────────┐         ┌─────────┐         ┌─────────┐         ┌─────────┐     │
│  │Hardware │         │Format   │         │Track    │         │Copy     │     │
│  │GW/KF/SCP│         │Parser   │         │Analysis │         │Mode     │     │
│  │         │         │D64/ADF/ │         │XDF/Vic/ │         │Selection│     │
│  │ --or--  │         │SCP/IMG  │         │Profile  │         │Sector/  │     │
│  │Image    │         │         │         │Match    │         │Track/   │     │
│  │File     │         │         │         │         │         │Nibble/  │     │
│  └────┬────┘         └────┬────┘         └────┬────┘         │Flux     │     │
│       │                   │                   │               └────┬────┘     │
│       │                   │                   │                    │           │
│       ▼                   ▼                   ▼                    ▼           │
│  ┌─────────────────────────────────────────────────────────────────────┐     │
│  │                        HUB FORMAT                                    │     │
│  │                     uft_raw_track_t                                  │     │
│  │  ┌────────────────────────────────────────────────────────────────┐ │     │
│  │  │ cylinder, head, encoding, bit_rate, rpm                        │ │     │
│  │  │ bits[] (packed), bit_count                                     │ │     │
│  │  │ timing[] (optional, per-bit ns)                                │ │     │
│  │  │ weak_mask[] (optional)                                         │ │     │
│  │  │ index_positions[], revolution                                  │ │     │
│  │  │ quality: avg_bit_cell_ns, jitter_ns, decode_errors             │ │     │
│  │  └────────────────────────────────────────────────────────────────┘ │     │
│  └──────────────────────────────┬──────────────────────────────────────┘     │
│                                 │                                             │
│       ┌─────────────────────────┴─────────────────────────┐                  │
│       │                                                   │                  │
│       ▼                                                   ▼                  │
│  ┌─────────┐         ┌─────────┐         ┌─────────┐     ┌─────────┐        │
│  │PRESERVE │         │CONVERT  │         │WRITE    │     │VERIFY   │        │
│  └────┬────┘         └────┬────┘         └────┬────┘     └────┬────┘        │
│       │                   │                   │               │              │
│       ▼                   ▼                   ▼               ▼              │
│  ┌─────────┐         ┌─────────┐         ┌─────────┐     ┌─────────┐        │
│  │Flux     │         │Format   │         │Hardware │     │Read-    │        │
│  │Archive  │         │Encoder  │         │Writer   │     │back     │        │
│  │SCP/RAW  │         │D64/ADF/ │         │GW/KF/   │     │Compare  │        │
│  │         │         │IMG      │         │SCP      │     │         │        │
│  └─────────┘         └─────────┘         └─────────┘     └─────────┘        │
│                                                                              │
└────────────────────────────────────────────────────────────────────────────────┘

ÜBERGABEPUNKTE:

1. Hardware → Format Parser
   - RAW Flux Data (timing arrays)
   - Index Hole Positions
   - Quality Metrics

2. Format Parser → Hub Format
   - Decoded Bitstream
   - Encoding Type
   - Weak Bits Mask

3. Hub Format → Analysis
   - Full Track Data
   - Multi-Revolution Data
   - Timing Information

4. Analysis → Decision
   - Platform Match (Confidence)
   - Protection Detection
   - Copy Mode Recommendation

5. Hub Format → Preservation
   - Complete Flux Archive
   - No Data Loss

6. Hub Format → Conversion
   - Sector Data Only
   - Or Full Track Data (XDF)
```

---

## PHASE 3: Schnittstellenanalyse

### D) API-/Datamodel-Matrix

| Verbindung | API | Typen | Ownership | Error-Model | Probleme |
|------------|-----|-------|-----------|-------------|----------|
| GUI → Analysis | `uft_analyze_track()` | `uft_track_analysis_t` | Caller owns result | Return code | ✅ Konsistent |
| GUI → Profiles | `uft_find_profile_by_name()` | `uft_platform_profile_t*` | Static, no free | NULL = not found | ✅ Konsistent |
| Analysis → Profiles | Includes | `uft_platform_profile_t` | Shared | N/A | ✅ Konsistent |
| XCopy → XDF | `uft_format_requires_track_copy()` | `bool` | N/A | Return value | ✅ NEU, Konsistent |
| Formats → Core | `uft_error_t` | `UFT_ERR_*` | N/A | Return code | ✅ Konsistent |
| HAL → Decoder | `uft_raw_track_t` | Hub Format | Caller alloc | Return code | ⚠️ Nur 2 Nutzer |
| Sector → Core | CRC Functions | `uint16_t` | N/A | Return value | ✅ Konsistent |

### Zentrale Datenmodelle

#### 1. Track-Modelle (FRAGMENTIERT ⚠️)

| Location | Struct | Usage |
|----------|--------|-------|
| `uft_decoder_adapter.h` | `uft_raw_track_t` | Hub-Format (Ideal) |
| `uft_track_analysis.h` | `uft_track_analysis_t` | Analysis Result |
| `uft_types.h` | `uft_track_t` | Forward decl only |
| `uft_track.h` | `uft_track_t` | Incomplete |
| Diverse | `HXCFE_TRACK*` | Legacy HxC |

**Problem:** 7 verschiedene Track-Definitionen, keine einheitliche.

#### 2. Sector-Modelle (GUT ✅)

| Location | Struct | Usage |
|----------|--------|-------|
| `uft_sector_parser.h` | `uft_parsed_sector_t` | Unified Parsing |
| `uft_sector_parser.h` | `uft_sector_config_t` | Config |
| `uft_sector_parser.h` | `uft_sector_result_t` | Result |

#### 3. Profile-Modelle (GUT ✅)

| Location | Struct | Usage |
|----------|--------|-------|
| `uft_track_analysis.h` | `uft_platform_profile_t` | 53 Profile |
| `uft_profiles_all.h` | Lookup-API | Registry |

---

## PHASE 4: Bewertung - Ergänzen vs. Behindern

### E) Sinnvolle Kopplungen (Behalten/Ausbauen)

| Kopplung | Grund | Benefit |
|----------|-------|---------|
| **Analysis ↔ Profiles** | Klare Schnittstelle | Sauber getrennt, 53 Profile |
| **XDF ↔ XCopy** | Automatische Mode-Erkennung | Variable Sektoren → Track Copy |
| **GUI ↔ Backend** | Qt/C Bridge | Thread-sicher, Signal/Slot |
| **Sector ↔ CRC** | Konsistente Validierung | Einheitliche CRC-API |
| **HAL ↔ Hub-Format** | N+M statt N×M | Reduktion Konverter |
| **C64 Module** | Eigenständig | 55 Tests, keine Deps |

### F) Behindernde Kopplungen (Konkret + Fixstrategie)

| Kopplung | Problem | Konfliktgrad | Fixstrategie |
|----------|---------|--------------|--------------|
| **Track-Definitionen** | 7 verschiedene Structs | HOCH | Konsolidierung auf `uft_raw_track_t` |
| **HAL → Core** | Direkter Zugriff auf Internals | MITTEL | HAL nur über öffentliche API |
| **Decoder → Formats** | Zirkuläre Includes möglich | MITTEL | Clear Layering Rules |
| **Legacy HxC Code** | `HXCFE_*` Typen | HOCH | Wrapper/Adapter Pattern |
| **GUI → HAL** | Direkter Hardware-Zugriff | NIEDRIG | Nur über Core/Backend |
| **Format Registry** | Mehrere Implementierungen | MITTEL | Konsolidieren |

#### Detailanalyse kritischer Konflikte:

##### 1. Track-Definition Chaos (HOCH)

**Problem:**
```c
// Definition 1: uft_decoder_adapter.h
typedef struct uft_raw_track {
    int cylinder, head;
    uint8_t* bits;
    size_t bit_count;
    uint16_t* timing;
    // ... 15 weitere Felder
} uft_raw_track_t;

// Definition 2: uft_types.h
typedef struct uft_track uft_track_t;  // Forward only

// Definition 3: HXCFE (Legacy)
typedef struct { /* HxC specifics */ } HXCFE_TRACK;
```

**Risiko:** 
- Daten gehen verloren bei Konvertierung
- Timing/Weak-Bits nicht übertragen
- API-Inkompatibilität

**Fix:**
```c
// Neue zentrale Definition: include/uft/uft_track.h
typedef struct uft_track {
    /* Identity */
    int cylinder, head;
    bool is_half_track;
    
    /* Encoding */
    uft_encoding_t encoding;
    double bit_rate_kbps;
    double rpm;
    
    /* Data */
    uint8_t *bits;
    size_t bit_count;
    
    /* Optional: Timing (für Flux) */
    uint16_t *timing_ns;
    
    /* Optional: Weak bits */
    uint8_t *weak_mask;
    
    /* Multi-Rev */
    int revolution;
    int total_revolutions;
    
    /* Quality */
    uft_track_quality_t quality;
} uft_track_t;

// Converter von Legacy:
int uft_track_from_hxc(uft_track_t *dst, const HXCFE_TRACK *src);
int uft_track_to_hxc(HXCFE_TRACK *dst, const uft_track_t *src);
```

##### 2. Format Registry Duplikation (MITTEL)

**Problem:**
- `src/formats/format_registry/`
- `src/registry/`
- `src/detection/`

Drei verschiedene Ansätze zur Format-Erkennung.

**Fix:** Konsolidieren zu einer Registry mit Score-basierter Detection.

---

## PHASE 5: Architekturentscheidungen & Refactor-Plan

### G) Refactor-Plan

#### Schritt 1: Track-Konsolidierung (P1-4)

**Dateien:**
- `include/uft/uft_track.h` (NEU - zentrale Definition)
- `include/uft/uft_track_compat.h` (NEU - Legacy Converter)
- `src/core/uft_track.c` (NEU - Implementation)

**API:**
```c
/* include/uft/uft_track.h */

#ifndef UFT_TRACK_H
#define UFT_TRACK_H

#include "uft_types.h"

typedef struct uft_track {
    /* Identity */
    int cylinder;
    int head;
    bool is_half_track;
    
    /* Encoding info */
    uft_encoding_t encoding;
    double bit_rate_kbps;
    double rpm;
    
    /* Bitstream data (packed, MSB first) */
    uint8_t *bits;
    size_t bit_count;
    
    /* Optional timing (per-bit, nanoseconds) */
    uint16_t *timing_ns;
    size_t timing_count;
    
    /* Optional weak bits mask */
    uint8_t *weak_mask;
    
    /* Index hole positions (bit offsets) */
    size_t *index_positions;
    int index_count;
    
    /* Revolution info */
    int revolution;
    int total_revolutions;
    
    /* Quality metrics */
    double avg_bit_cell_ns;
    double jitter_ns;
    int decode_errors;
} uft_track_t;

/* Lifecycle */
uft_track_t* uft_track_alloc(size_t bit_count, bool with_timing);
void uft_track_free(uft_track_t *track);
uft_track_t* uft_track_clone(const uft_track_t *src);

/* Comparison */
int uft_track_compare(const uft_track_t *a, const uft_track_t *b);
bool uft_track_equals(const uft_track_t *a, const uft_track_t *b);

/* Conversion */
int uft_track_from_bits(uft_track_t *track, const uint8_t *data, size_t bytes);
int uft_track_to_bits(const uft_track_t *track, uint8_t *data, size_t *bytes);

#endif /* UFT_TRACK_H */
```

#### Schritt 2: Format Registry Konsolidierung (P2)

**Dateien:**
- `include/uft/uft_format_registry.h` (Überarbeitet)
- `src/formats/format_registry/uft_format_registry_v3.c` (NEU)

**API:**
```c
/* Score-basierte Format-Detection */
typedef struct uft_format_detection {
    uft_format_t format;
    int confidence;          /* 0-100 */
    const char *reason;
    bool requires_track_copy;
} uft_format_detection_t;

int uft_detect_format(const uint8_t *data, size_t size, 
                      const char *filename,
                      uft_format_detection_t *results, 
                      int max_results);
```

#### Schritt 3: HAL Layer Separation (P2)

**Regel:** Formate dürfen HAL nicht direkt kennen.

```
VORHER:
  Format → HAL → Hardware

NACHHER:
  Format → Core → HAL → Hardware
```

### H) Integrationsregeln (Projekt-Contract)

```
╔════════════════════════════════════════════════════════════════════════════════╗
║                         UFT INTEGRATION CONTRACT                               ║
╠════════════════════════════════════════════════════════════════════════════════╣
║                                                                                ║
║  REGEL 1: Schichten-Isolation                                                  ║
║  ─────────────────────────────────────────────────────────────────────────────║
║  GUI → Public APIs only (kein direkter Core-Zugriff)                          ║
║  Tools → Core-Interfaces (nicht HAL direkt)                                   ║
║  Formats → Core (nicht HAL direkt)                                            ║
║  HAL → Core Types (uft_track_t, uft_error_t)                                  ║
║                                                                                ║
║  REGEL 2: Hub-Format                                                           ║
║  ─────────────────────────────────────────────────────────────────────────────║
║  Alle Track-Daten fließen durch uft_track_t                                   ║
║  N Formate + M Outputs = N+M Converter (nicht N×M)                            ║
║  Timing und Weak-Bits werden IMMER mitgeführt                                 ║
║                                                                                ║
║  REGEL 3: Error-Modell                                                         ║
║  ─────────────────────────────────────────────────────────────────────────────║
║  Return: 0 = OK, negativ = Fehler                                             ║
║  Alle Module nutzen UFT_ERR_* Konstanten                                      ║
║  Fehlertext via uft_error_str(code)                                           ║
║                                                                                ║
║  REGEL 4: Memory Ownership                                                     ║
║  ─────────────────────────────────────────────────────────────────────────────║
║  Caller-owns-buffer (Default)                                                 ║
║  Explicit _alloc()/_free() Paare dokumentiert                                 ║
║  Keine impliziten Allokationen in Out-Parametern                              ║
║                                                                                ║
║  REGEL 5: Copy-Mode Integration                                                ║
║  ─────────────────────────────────────────────────────────────────────────────║
║  Variable Sektoren → uft_format_requires_track_copy() == true                 ║
║  XCopy-Panel respektiert Empfehlung automatisch                               ║
║  Flux-Copy für geschützte Medien                                              ║
║                                                                                ║
║  REGEL 6: Test-Coverage                                                        ║
║  ─────────────────────────────────────────────────────────────────────────────║
║  Jedes neue Modul braucht mindestens 1 Unit-Test                              ║
║  Integrations-Tests für kritische Pfade                                       ║
║  Smoke-Test für CI                                                            ║
║                                                                                ║
╚════════════════════════════════════════════════════════════════════════════════╝
```

---

## PHASE 6: Aktualisierte TODO.md

```markdown
# UnifiedFloppyTool v3.7.0 - TODO (Architektur-Update)

**Stand:** 2026-01-09  
**Module:** 65+ Libraries, 53 Profile  
**Tests:** 3/3 CTest + weitere

---

## ✅ ERLEDIGT (Session 2026-01-09)

### P1 Tasks
- [x] P1-1: Smoke-Test Basis-Workflow (5 Test-Suiten)
- [x] P1-2: test_sector_parser API-Fix
- [x] P1-3: CI-Verification Script
- [x] XDF → XCopy Integration (3 Profile, requires_track_copy)

### Architektur
- [x] XDF Profile (UFT_PROFILE_IBM_XDF, XXDF, DMF)
- [x] uft_format_requires_track_copy() API
- [x] TrackAnalyzerBackend (Qt/C Bridge)

---

## 🔴 P0 - Blocker

**Keine offenen P0!** ✅

---

## 🟠 P1 - Kritisch

### P1-4: uft_track_t Zentralisierung
**Aufwand:** L (4-8h)  
**Beschreibung:** 7 Track-Definitionen → 1 zentrale  
**Akzeptanz:**
- [ ] `include/uft/uft_track.h` mit vollständiger Definition
- [ ] `uft_track_alloc()/free()` API
- [ ] Legacy-Converter für HxC
- [ ] Alle Module verwenden zentrale Definition
- [ ] Tests grün auf 3 OS
**Abhängigkeit:** Blockt P2-Architektur

### P1-5: FAT BPB Probe
**Aufwand:** M (2-4h)  
**Beschreibung:** FAT12/16/32 Erkennung mit Confidence  
**Akzeptanz:**
- [ ] FAT-Images werden erkannt
- [ ] Confidence-Score 0-100
- [ ] Keine False Positives bei D64/ADF

---

## 🟡 P2 - Architektur

### P2-15: Format Registry Konsolidierung
**Aufwand:** L (4-8h)  
**Beschreibung:** 3 Registries → 1 mit Score-Detection  
**Akzeptanz:**
- [ ] Einheitliche `uft_detect_format()` API
- [ ] Score-basierte Multi-Result Detection
- [ ] requires_track_copy integriert
**Abhängigkeit:** Nach P1-4

### P2-16: HAL Layer Separation
**Aufwand:** M (2-4h)  
**Beschreibung:** Formate sprechen nicht mehr direkt mit HAL  
**Akzeptanz:**
- [ ] Keine HAL-Includes in Format-Modulen
- [ ] Core als Zwischenschicht
- [ ] Keine Regression

### P2-17: Legacy HxC Wrapper
**Aufwand:** L (4-8h)  
**Beschreibung:** HXCFE_* Types kapseln  
**Akzeptanz:**
- [ ] `uft_track_from_hxc()` / `uft_track_to_hxc()`
- [ ] Timing bleibt erhalten
- [ ] Weak-Bits bleiben erhalten

---

## 🟢 P3 - Polish

### P3-1: Doku-Update
**Aufwand:** M  
**Beschreibung:** Module/API Dokumentation  
**Akzeptanz:**
- [ ] README.md aktualisiert
- [ ] API-Docs für Public Headers
- [ ] Integration Contract dokumentiert

### P3-2: CI Dashboard
**Aufwand:** S  
**Beschreibung:** Test-Coverage Report  
**Akzeptanz:**
- [ ] Coverage > 60%
- [ ] Dashboard in GitHub Actions

---

## Architektur-Schulden (Technische Schulden)

| ID | Beschreibung | Aufwand | Prio |
|----|--------------|---------|------|
| TD-1 | 7 Track-Definitionen | L | P1 |
| TD-2 | 3 Format-Registries | M | P2 |
| TD-3 | Legacy HxC Code | L | P2 |
| TD-4 | Zirkuläre Includes möglich | S | P2 |
| TD-5 | GUI ↔ HAL direkt | S | P3 |

---

## Modul-Integritätsprüfung

| Modul | Status | Tests | Deps OK |
|-------|--------|-------|---------|
| uft_core | ✅ | smoke | ✅ |
| uft_crc | ✅ | sector_parser | ✅ |
| uft_track_analysis | ✅ | smoke_workflow | ✅ |
| uft_profiles | ✅ | smoke_workflow | ✅ |
| uft_xdf | ✅ | xdf_xcopy | ✅ |
| uft_c64 | ✅ | 4 Tests | ✅ |
| uft_sector | ✅ | sector_parser | ✅ |
| uft_hal | ⚠️ | - | TD-5 |
| uft_formats | ⚠️ | - | TD-2 |

---

## Nächste Schritte (Priorität)

1. **P1-4**: Track-Konsolidierung (blockt Architektur)
2. **P1-5**: FAT BPB Detection
3. **P2-15**: Registry Konsolidierung
4. **P2-16**: HAL Separation
5. **P2-17**: HxC Wrapper
```

---

## Zusammenfassung

### Was gut läuft ✅
- Analysis ↔ Profiles: Saubere Trennung
- XDF ↔ XCopy: Vollständig integriert
- C64 Module: Eigenständig, gut getestet
- GUI ↔ Backend: Qt/C Bridge funktioniert
- Error-Modell: Konsistent

### Was Arbeit braucht ⚠️
- **Track-Definitionen**: 7 verschiedene → 1 zentrale (KRITISCH)
- **Format-Registries**: 3 verschiedene → konsolidieren
- **Legacy HxC**: Wrapper benötigt
- **HAL Separation**: Formate sollten nicht direkt zugreifen

### Konkrete Fixes
1. `uft_track.h` mit zentraler Definition erstellen
2. Legacy-Converter für HxC implementieren
3. Format-Registry mit Score-Detection vereinheitlichen
4. Integrationsregeln durchsetzen

**"Bei uns geht kein Bit verloren"** - aber die Architektur braucht Konsolidierung!

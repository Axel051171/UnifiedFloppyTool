# UFT Development TODO - God Mode Roadmap

## Legende
- 🔴 **MUST** - Kritisch für Stabilität/Funktion
- 🟡 **SHOULD** - Wichtig für Qualität
- 🟢 **COULD** - Nice-to-have
- ⏱️ Aufwand: S(mall) / M(edium) / L(arge) / XL
- ⚡ Priorität: 1-5 (1=höchste)

---

## 1️⃣ BUILD INTEGRATION (Woche 1)

### 🔴 MUST: CMake für neue Module
| Task | Aufwand | Prio | Status |
|------|---------|------|--------|
| `src/encoding/CMakeLists.txt` erstellen | S | ⚡1 | ☐ |
| `src/flux/fdc_bitstream/CMakeLists.txt` | S | ⚡1 | ☐ |
| `src/formats/apple/CMakeLists.txt` | S | ⚡1 | ☐ |
| `src/formats/atari/CMakeLists.txt` | S | ⚡1 | ☐ |
| `src/formats/amiga_ext/CMakeLists.txt` | S | ⚡1 | ☐ |
| `src/formats/misc_hxc/CMakeLists.txt` | S | ⚡1 | ☐ |
| `src/crc/CMakeLists.txt` | S | ⚡1 | ☐ |
| Haupt-CMakeLists.txt: `add_subdirectory()` | S | ⚡1 | ☐ |

**Ziel:** Alle neuen Module kompilieren ohne Fehler

---

## 2️⃣ PARSER DEVELOPMENT (Woche 2-3)

### 🔴 MUST: Core Parser vervollständigen
| Parser | Was fehlt | Aufwand | Prio |
|--------|-----------|---------|------|
| **SCP v3** | Multi-Revolution Support | M | ⚡1 |
| **HFE v3** | HDDD A2 Variante | M | ⚡1 |
| **IPF** | CAPS Library Integration | L | ⚡2 |
| **A2R** | Apple II Flux Format | M | ⚡2 |

### 🟡 SHOULD: Neue Parser aus extrahiertem Code
| Parser | Source | Aufwand | Prio |
|--------|--------|---------|------|
| Apple 2MG | `src/formats/apple/` | M | ⚡2 |
| Apple NIB | `src/formats/apple/` | M | ⚡2 |
| Atari STX | `src/formats/atari/` | M | ⚡3 |
| Atari ATX | `src/formats/atari/` | M | ⚡3 |
| CPC EDSK | `src/formats/misc_hxc/` | M | ⚡3 |

### Parser Template (Copy & Adapt)
```c
// src/formats/XXX/uft_XXX_parser_v3.c
#include "uft_XXX_parser_v3.h"

uft_error_t uft_XXX_parse(const uint8_t* data, size_t size, uft_XXX_t* out) {
    if (!data || !out || size < XXX_MIN_SIZE) 
        return UFT_ERR_INVALID_PARAM;
    
    // 1. Magic/Signature prüfen
    // 2. Header parsen
    // 3. Geometrie extrahieren
    // 4. Tracks/Sektoren lesen
    // 5. Checksums validieren
    
    return UFT_OK;
}
```

---

## 3️⃣ PARAMETER SYSTEM (Woche 3-4)

### 🔴 MUST: Parameter für neue Module
| Modul | Parameter-Struct | Aufwand | Prio |
|-------|------------------|---------|------|
| PLL PI | `uft_pll_pi_params_t` | M | ⚡1 |
| Recovery | `uft_recovery_params_t` | M | ⚡1 |
| Fuzzy Bits | `uft_fuzzy_params_t` | S | ⚡2 |
| FDC Bitstream | `uft_fdc_params_t` | M | ⚡2 |

### Parameter-Struktur (Standard)
```c
typedef struct {
    // Identification
    uint32_t version;
    uint32_t flags;
    
    // Core Settings
    // ...
    
    // Validation
    bool validated;
    char error_msg[256];
} uft_XXX_params_t;

// Factory functions
uft_XXX_params_t uft_XXX_params_default(void);
uft_error_t uft_XXX_params_validate(uft_XXX_params_t* p);
uft_error_t uft_XXX_params_from_json(const char* json, uft_XXX_params_t* p);
char* uft_XXX_params_to_json(const uft_XXX_params_t* p);
```

### 🟡 SHOULD: Preset-System erweitern
| Preset-Kategorie | Anzahl | Status |
|------------------|--------|--------|
| Commodore (D64/G64/D81) | 5 | ✓ vorhanden |
| Amiga (ADF/ExtADF) | 4 | ✓ vorhanden |
| Apple (DO/PO/NIB/2MG) | 4 | ☐ NEU |
| Atari (ST/STX/ATR/ATX) | 4 | ☐ NEU |
| PC (IMG/IMA/DMK/IMD) | 5 | ☐ NEU |
| Flux (SCP/KF/HFE/A2R) | 6 | ☐ erweitern |

---

## 4️⃣ RECOVERY PIPELINE (Woche 4-5)

### 🔴 MUST: Recovery-Funktionen implementieren
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Multi-Read │───▶│  Majority   │───▶│  CRC Fix    │───▶│  Verify     │
│  Capture    │    │  Vote       │    │  (1-2 bit)  │    │  & Report   │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

| Funktion | Datei | Aufwand | Prio |
|----------|-------|---------|------|
| `uft_recovery_multi_read()` | `src/recovery/` | L | ⚡1 |
| `uft_recovery_majority_vote()` | `src/recovery/` | M | ⚡1 |
| `uft_recovery_fix_crc_single()` | vorhanden | S | ⚡2 |
| `uft_recovery_fix_crc_double()` | `src/recovery/` | M | ⚡2 |
| `uft_recovery_report_generate()` | `src/recovery/` | M | ⚡3 |

### 🟡 SHOULD: Confidence Scoring
```c
typedef struct {
    uint8_t data[512];
    float confidence[512];      // 0.0-1.0 pro Byte
    uint8_t read_count;
    uint8_t agreement_count[512];
} uft_sector_confidence_t;
```

---

## 5️⃣ FLUX DECODER PIPELINE (Woche 5-6)

### 🔴 MUST: PLL Integration
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│  Flux       │───▶│  PLL        │───▶│  Bitstream  │───▶│  Decoder    │
│  Timing     │    │  (PI/VFO)   │    │  Extract    │    │  (MFM/GCR)  │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

| Task | Module | Aufwand | Prio |
|------|--------|---------|------|
| PI-PLL an UFT-API anpassen | `src/flux/pll/` | M | ⚡1 |
| VFO-Module vereinheitlichen | `src/flux/fdc_bitstream/` | L | ⚡2 |
| Flux→Bitstream Adapter | `src/flux/` | M | ⚡1 |
| MFM Decoder optimieren | `src/encoding/` | M | ⚡2 |
| GCR Decoder testen | `src/encoding/gcr/` | M | ⚡2 |

### PLL-Parameter Presets
```c
// Conservative (für beschädigte Disks)
uft_pll_pi_params_t pll_conservative = {
    .kp = 0.3, .ki = 0.0003, .sync_tolerance = 0.33
};

// Aggressive (für saubere Disks)  
uft_pll_pi_params_t pll_aggressive = {
    .kp = 0.7, .ki = 0.001, .sync_tolerance = 0.15
};

// Adaptive (automatisch)
uft_pll_pi_params_t pll_adaptive = {
    .kp = 0.5, .ki = 0.0005, .sync_tolerance = 0.25,
    .flags = UFT_PLL_FLAG_ADAPTIVE
};
```

---

## 6️⃣ COPY PROTECTION (Woche 6-7)

### 🟡 SHOULD: Protection Detection
| Protection | Plattform | Modul | Aufwand |
|------------|-----------|-------|---------|
| Fuzzy Bits | Atari ST | ✓ vorhanden | - |
| Weak Sectors | Amiga | `src/protection/` | M |
| Long Tracks | Amiga/C64 | `src/protection/` | M |
| Invalid CRC | Multi | `src/protection/` | S |
| Half Tracks | Apple II | `src/protection/` | M |
| Density Variations | C64 | `src/protection/` | L |

### Detection API
```c
typedef struct {
    bool has_protection;
    uft_protection_type_t types[16];
    uint8_t type_count;
    char description[256];
    uint8_t confidence;         // 0-100%
    
    // Details
    uint8_t fuzzy_sectors[64];
    uint8_t fuzzy_count;
    uint8_t weak_tracks[84];
    uint8_t weak_count;
} uft_protection_result_t;

uft_error_t uft_protection_detect(
    const uft_disk_t* disk,
    uft_protection_result_t* result
);
```

---

## 7️⃣ GUI INTEGRATION (Woche 7-8)

### 🟡 SHOULD: Neue Panels
| Panel | Für Modul | Aufwand | Prio |
|-------|-----------|---------|------|
| `uft_pll_panel.cpp` | PLL Tuning | M | ⚡2 |
| `uft_recovery_panel.cpp` | Recovery Settings | M | ⚡2 |
| `uft_track_grid_widget.cpp` | Track Heatmap | L | ⚡3 |
| `uft_flux_view_widget.cpp` | Flux Visualizer | XL | ⚡4 |

### TrackGrid Widget Spec
```
┌────────────────────────────────────────┐
│  Track 0  │ ■ ■ ■ ■ ■ ■ ■ ■ ■ │ Side 0 │
│  Track 1  │ ■ ■ □ ■ ■ ■ ■ ■ ■ │ Side 0 │
│  Track 2  │ ■ ■ ■ ■ ■ ■ ■ ■ ■ │ Side 0 │
│  ...      │                   │        │
│  Track 79 │ ■ ■ ■ ■ □ □ ■ ■ ■ │ Side 1 │
└────────────────────────────────────────┘
■ = OK (grün)  □ = CRC Error (rot)  ◊ = Recovered (gelb)
```

---

## 8️⃣ TESTING & VALIDATION (Ongoing)

### 🔴 MUST: Unit Tests
| Test-Bereich | Dateien | Status |
|--------------|---------|--------|
| Parser Tests | `tests/parser/` | ☐ |
| CRC Tests | `tests/crc/` | ☐ |
| PLL Tests | `tests/pll/` | ☐ |
| Recovery Tests | `tests/recovery/` | ☐ |

### Golden Tests (Referenz-Images)
```
tests/golden/
├── d64/
│   ├── standard.d64      → expected_hash.txt
│   └── protected.d64     → expected_protection.json
├── adf/
│   ├── ofs_880k.adf
│   └── ffs_880k.adf
├── flux/
│   ├── clean.scp
│   └── damaged.scp       → expected_recovery.json
```

---

## 9️⃣ DOKUMENTATION (Ongoing)

### 🟢 COULD: Developer Docs
| Dokument | Beschreibung | Status |
|----------|--------------|--------|
| `ARCHITECTURE.md` | Modul-Übersicht | ☐ |
| `PARSER_GUIDE.md` | Wie man Parser schreibt | ☐ |
| `API_REFERENCE.md` | Alle öffentlichen Funktionen | ☐ |
| `FORMATS.md` | Unterstützte Formate | ☐ |

---

## 📅 TIMELINE ÜBERSICHT

```
Woche 1:  ████░░░░░░  CMake Integration
Woche 2:  ████████░░  Parser Development
Woche 3:  ████████░░  Parser + Parameter
Woche 4:  ██████░░░░  Parameter + Recovery
Woche 5:  ████████░░  Recovery + Flux
Woche 6:  ██████░░░░  Flux + Protection
Woche 7:  ████░░░░░░  Protection + GUI
Woche 8:  ████████░░  GUI + Testing
```

---

## 🎯 QUICK WINS (Sofort machbar)

1. **CMake für `src/encoding/`** - 15 min
2. **PLL Parameter Struct** - 30 min
3. **Recovery Header finalisieren** - 20 min
4. **Protection Detect Stub** - 30 min
5. **Preset für Apple Formate** - 45 min

---

## ⚠️ RISIKEN & ABHÄNGIGKEITEN

| Risiko | Impact | Mitigation |
|--------|--------|------------|
| HxCFE-Code GPL-Lizenz | Legal | Lizenz-Header prüfen |
| C++ in FDC Bitstream | Build | Wrapper oder Port nach C |
| Fehlende Test-Images | Quality | Community fragen |
| API-Brüche | Stability | Versionierung |

---

## 📝 NOTIZEN

- **Nächster Fokus:** CMake Integration (Woche 1)
- **Feedback sammeln:** Recovery-API Design
- **Community:** Test-Images für exotische Formate

---

*Erstellt: 2026-01-03*
*Version: 1.0*

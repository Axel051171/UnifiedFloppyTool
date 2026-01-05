# Parallel Development Merge Report

**Datum:** Januar 2026  
**Version:** 3.9.5  
**Quellen:** UFT_GUI_v1.3.0 (amiga-stuff), UFT_GUI_v1.4.0 (diagnostics)

---

## Zusammenfassung

Aus der parallelen Entwicklung wurden folgende wertvolle Komponenten übernommen:

| Komponente | Zeilen | Priorität | Status |
|------------|--------|-----------|--------|
| **tab_diagnostics.ui** | 745 | KRITISCH | ✅ Übernommen |
| **gw_drift_jitter_test.py** | 128 | HOCH | ✅ Übernommen |
| **gw_stats.py** | 124 | HOCH | ✅ Übernommen |
| **parameter_registry.json** | 1672 | KRITISCH | ✅ Übernommen |
| **FLUXFOX_FEATURES_EXTRACTION.md** | 557 | HOCH | ✅ Übernommen |
| **fdutils_reference.md** | 59 | MITTEL | ✅ Übernommen |
| **batch3_fdutils_gw_analysis.h** | 553 | HOCH | ✅ Übernommen |

**Gesamt:** ~3838 Zeilen neuer/aktualisierter Code

---

## 1. Hardware-Diagnostik (NEU)

### 1.1 Diagnostics Tab (Qt UI)

**Datei:** `forms/tab_diagnostics.ui`

Enthält:
- **RPM-Messung** (fdutils/floppymeter basiert)
  - Messzyklen (100-10000)
  - Warmup-Rotationen (10-500)
  - Sliding-Window (5-50)
  - Test-Zylinder (0-84)
  - Ergebnisanzeige: Rotationszeit, RPM, Abweichung (PPM), Track-Kapazität

- **Drift/Jitter-Test** (gw_drift_jitter_test.py basiert)
  - Umdrehungen pro Pass (10-200)
  - Anzahl Durchläufe (1-20)
  - Ergebnisanzeige: Jitter pk-pk, Jitter stdev, Drift gesamt, Bewertung

- **Greaseweazle Live-Stats** (gw_stats.py basiert)
  - Fehler-Statistik: bad_cmd, no_idx, no_trk0, wrprot, overflow, underflow
  - Index-Filter: accepted, masked, glitch
  - Index-Write Timing: starts, min, max, avg

- **CMOS Drive-Type Referenz-Tabelle**

### 1.2 Python Diagnostik-Scripts

**Dateien:** `scripts/diagnostics/`

| Script | Funktion |
|--------|----------|
| `gw_drift_jitter_test.py` | Misst Jitter (Variation innerhalb Capture) und Drift (über Zeit) |
| `gw_stats.py` | Liest Greaseweazle Firmware-Statistiken via CDC-ACM |

**Verwendung:**
```bash
# Drift/Jitter Test
python3 gw_drift_jitter_test.py --port /dev/ttyACM0 --track 0 --revs 50 --passes 5

# Live-Stats
python3 gw_stats.py /dev/ttyACM0 --watch 0.5
```

### 1.3 C-Header für Diagnostik

**Datei:** `include/uft/hal/uft_diagnostics.h`

Enthält:
- CMOS Drive-Type Tabelle (7 Typen)
- Datenraten-Konstanten (4 Raten)
- FDC Status Register Bits (ST1, ST2)
- Format-Konstanten (fdutils)
- RPM-Messung Strukturen
- Jitter/Drift Test Strukturen
- Greaseweazle Stats Strukturen
- Hardware-Capabilities Tabelle

---

## 2. Standard-Format Registry (NEU)

**Datei:** `include/uft/formats/uft_standard_formats.h`

### 2.1 Format-Deskriptoren

| Format | Geometrie | Größe |
|--------|-----------|-------|
| PC 160KB | 40×1×8×512 | 163,840 |
| PC 180KB | 40×1×9×512 | 184,320 |
| PC 320KB | 40×2×8×512 | 327,680 |
| PC 360KB | 40×2×9×512 | 368,640 |
| PC 720KB | 80×2×9×512 | 737,280 |
| PC 1.2MB | 80×2×15×512 | 1,228,800 |
| PC 1.44MB | 80×2×18×512 | 1,474,560 |
| PC 2.88MB | 80×2×36×512 | 2,949,120 |
| Amiga 880KB | 80×2×11×512 | 901,120 |
| Amiga 1.76MB | 80×2×22×512 | 1,802,240 |
| C64 D64 | 35 Tracks | 174,848 |
| Atari 90KB | 40×1×18×128 | 92,160 |
| Apple 140KB | 35×1×16×256 | 143,360 |

### 2.2 BPB Parser

- Validierung nach DOS 2.x/3.x Standard
- Auto-Detection via total_sectors und media_descriptor
- Vollständige Typprüfung

### 2.3 Kopierschutz-Signaturen

**PC/Amiga:**
- Formaster CopyLock
- Softguard Superlok
- EA Interlock
- RNC CopyLock
- Speedlock
- Psygnosis
- Factor 5

**C64/CBM:**
- RapidLok
- Vorpal
- EA Fat Track

**Generisch:**
- Weak Bits
- Long/Short Track
- Half Track
- Illegal Encoding

---

## 3. Parameter Registry (AKTUALISIERT)

**Datei:** `config/parameter_registry.json`

### 3.1 Statistiken

- **155 Parameter-Definitionen**
- **4 Modi:** Simple, Flux, Recovery, Protection
- **6 Kategorien:** Format, PLL, Capture, Recovery, Protection, Output

### 3.2 Wichtige neue Parameter

| Parameter | Quelle | Beschreibung |
|-----------|--------|--------------|
| `dpll_fast_divisor` | disktools | DPLL Fast-Mode Clock-Divisor |
| `dpll_fast_count` | disktools | Bits im Fast-Training-Modus |
| `dpll_fast_tolerance` | disktools | Toleranz im Fast-Training |
| `dpll_medium_divisor` | disktools | DPLL Medium-Mode Divisor |
| `dpll_slow_divisor` | disktools | DPLL Slow-Mode (Normal) |
| `dpll_phase_table` | US Patent 4808884 | Phase Adjustment Lookup |

### 3.3 Amiga Protection Database

- **196 Kopierschutz-Decoder** aus disk-utilities
- Familien: RNC, Speedlock, Psygnosis, Factor 5, Rainbow Arts, etc.
- CopyLock Sync-Wörter
- Long Track Lengths

### 3.4 Hardware Timing

- Step Delay: 3ms (DD/HD)
- Head Settle: 15ms
- Motor Spinup: 1250ms (DD), 2000ms (HD)
- Precompensation: 140ns ab Zylinder 40

---

## 4. Integration in bestehendes Projekt

### 4.1 Neue Dateien

```
scripts/
└── diagnostics/
    ├── gw_drift_jitter_test.py    # Drift/Jitter Messung
    └── gw_stats.py                 # GW Firmware Stats

forms/
└── tab_diagnostics.ui              # Qt Designer UI

config/
└── parameter_registry.json         # 155 Parameter

include/uft/
├── hal/
│   ├── uft_diagnostics.h           # Diagnostik-Konstanten
│   └── batch3_fdutils_gw_analysis.h
└── formats/
    └── uft_standard_formats.h      # Format Registry

docs/reference/
├── FLUXFOX_FEATURES_EXTRACTION.md
└── fdutils_reference.md
```

### 4.2 Empfohlene nächste Schritte

1. **Qt Integration:** Tab_diagnostics in MainWindow einbinden
2. **Python Wrapper:** C++ Wrapper für Python-Scripts erstellen
3. **Format Registry:** In uft_format_detect.c integrieren
4. **Protection Detection:** In analyze_protection() einbauen
5. **Parameter System:** parameter_registry.json in GUI laden

---

## 5. Quellen und Lizenzen

| Quelle | Lizenz | Verwendung |
|--------|--------|------------|
| fdutils-5.6 | GPL | Konstanten, Algorithmen |
| FluxFox | MIT | Format Registry, BPB Parser |
| disk-utilities | Public Domain | Kopierschutz-Datenbank |
| gw_scripts | MIT | Diagnostik-Scripts |
| amiga-stuff | Public Domain | Timing-Konstanten |

---

## Changelog

### v3.9.5 (Parallel Dev Merge)

**Hinzugefügt:**
- Hardware-Diagnostik Tab (745 Zeilen UI)
- Greaseweazle Scripts (252 Zeilen Python)
- Standard Format Registry (18 Formate)
- BPB Parser und Auto-Detection
- Kopierschutz-Signaturen (20+ Typen)
- CMOS Drive-Type Tabelle
- Parameter Registry (155 Parameter)

**Aktualisiert:**
- DPLL-Parameter aus disktools/US Patent
- Hardware-Capabilities Tabelle
- FDC Status Register Definitionen

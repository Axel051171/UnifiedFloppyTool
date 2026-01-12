# UFT Integration Pack Analyse

**Datum:** 2026-01-09  
**Pakete:** TransWarp v2 + FormatID/FAT/UFI  
**Rolle:** R5 (Integration & Extraction Engineer)

---

## Executive Summary

| Paket | Module | Qualität | Empfehlung |
|-------|--------|----------|------------|
| **TransWarp v2** | Serial Stream, ADF, Blockdev | ✅ Sehr gut | **MERGE** |
| **FormatID** | FAT BPB, Geometry Guess, UFI HAL | ✅ Sehr gut | **MERGE** |

**Keine Konflikte.** Beide Pakete sind komplementär und erweitern UFT um wichtige Funktionen.

---

## 1. TransWarp Integration Pack v2

### 1.1 Inhalt

| Datei | Funktion | LOC |
|-------|----------|-----|
| `link/serial_stream.{h,c}` | Cross-Platform Serial I/O (POSIX/Win32) | ~200 |
| `format/adf.{h,c}` | ADF Container Helpers (DD Geometry) | ~150 |
| `hal/blockdev.h` | Blockdevice Interface (read/write/flush) | ~50 |

### 1.2 API-Qualität

```c
// Saubere Cross-Platform Abstraktion
typedef struct uft_serial {
#ifdef _WIN32
    void *h; /* HANDLE */
#else
    int fd;
#endif
    int is_open;
} uft_serial_t;

// Klare Optionen
typedef struct uft_serial_opts {
    uint32_t baud;
    uint8_t databits;  // 8
    uint8_t stopbits;  // 1
    char parity;       // 'N','E','O'
    int rtscts;        // 0/1
    uint32_t read_timeout_ms;
} uft_serial_opts_t;
```

### 1.3 Bewertung

| Kriterium | Score | Kommentar |
|-----------|-------|-----------|
| Code-Qualität | 9/10 | C11, sauber, dokumentiert |
| Cross-Platform | 10/10 | Win32/POSIX Abstraktion |
| Error-Handling | 9/10 | uft_rc_t + uft_diag_t |
| Tests | 7/10 | Header-Compile, Smoke TODO |
| Dokumentation | 9/10 | README + TODO klar |

**Use-Cases:**
- Amiga Serial Disk Capture (TransWarp-kompatibel)
- Generische Serial I/O für weitere Controller

---

## 2. FormatID/FAT/UFI Pack

### 2.1 Inhalt

| Datei | Funktion | LOC |
|-------|----------|-----|
| `fs/fat_bpb.{h,c}` | FAT BPB Parser + Layout Calculator | ~250 |
| `formatid/guess.{h,c}` | Format/Geometry Guesser | ~100 |
| `hal/ufi.{h,c}` | USB Floppy Interface HAL | ~150 |

### 2.2 API-Qualität

```c
// FAT-Erkennung mit Confidence
typedef struct uft_guess_result {
    uft_guess_kind_t kind;      // FAT, RAW_GEOMETRY, UNKNOWN
    int confidence;             // 0..100
    uft_geometry_hint_t geom;
    char label[64];             // "DOS FAT12", "PC 3.5\" HD 1.44M"
} uft_guess_result_t;

// Geometry-Tabelle (8 Standard-PC-Formate)
static const geom_entry k_common[] = {
    { 160*1024,  {40,1,8,512},   "PC 5.25\" SS 160K" },
    { 720*1024,  {80,2,9,512},   "PC 3.5\" DS 720K" },
    { 1440*1024, {80,2,18,512},  "PC 3.5\" HD 1.44M" },
    // ...
};
```

### 2.3 Bewertung

| Kriterium | Score | Kommentar |
|-----------|-------|-----------|
| Code-Qualität | 10/10 | Perfekte FAT-Validierung |
| Confidence-System | 10/10 | Scoring wie gewünscht |
| Error-Handling | 10/10 | Detaillierte Diagnose |
| Tests | 7/10 | Header-Compile, Unit TODO |
| Dokumentation | 9/10 | Fremdquellen-Report |

**Use-Cases:**
- Auto-Detection für raw .img Dateien
- FAT-Validierung für Recovery
- GUI: "Erkanntes Format: DOS FAT12 (Confidence: 85%)"

---

## 3. Kompatibilität mit UFT

### 3.1 uft_common.h Differenz

```diff
  UFT_ENOT_IMPLEMENTED = -7,
+ UFT_ETIMEOUT = -8        // Nur in TransWarp
```

**Aktion:** `UFT_ETIMEOUT` in zentrale uft_common.h übernehmen.

### 3.2 Namespace

Beide Pakete nutzen `uft_*` Prefix - **kompatibel** mit bestehendem UFT.

### 3.3 Dependencies

| Paket | Dependencies | Status |
|-------|--------------|--------|
| TransWarp | stdio, string, uft_common | ✅ OK |
| FormatID | stdio, string, uft_common | ✅ OK |
| Cross-Deps | TransWarp ↔ FormatID: KEINE | ✅ OK |

---

## 4. Integration in UFT

### 4.1 Zielstruktur

```
src/
├── link/
│   └── serial_stream.c     (NEU: TransWarp)
├── fs/
│   └── fat_bpb.c           (NEU: FormatID)
├── formatid/
│   └── guess.c             (NEU: FormatID)
├── hal/
│   ├── blockdev.h          (NEU: TransWarp)
│   └── ufi.c               (NEU: FormatID)
└── formats/amiga/
    └── adf.c               (NEU: TransWarp - ergänzt bestehend)

include/uft/
├── link/serial_stream.h    (NEU)
├── fs/fat_bpb.h            (NEU)
├── formatid/guess.h        (NEU)
└── hal/
    ├── blockdev.h          (NEU)
    └── ufi.h               (NEU)
```

### 4.2 CMakeLists.txt Ergänzungen

```cmake
# Serial Link (TransWarp)
add_library(uft_link STATIC src/link/serial_stream.c)
target_include_directories(uft_link PUBLIC include)

# FormatID (FAT + Guess)
add_library(uft_formatid STATIC
    src/fs/fat_bpb.c
    src/formatid/guess.c
)
target_include_directories(uft_formatid PUBLIC include)

# UFI HAL
add_library(uft_ufi STATIC src/hal/ufi.c)
target_include_directories(uft_ufi PUBLIC include)
```

---

## 5. TODO Updates (aus Paketen)

### Neue P1 Tasks

| ID | Task | Aufwand | Akzeptanz |
|----|------|---------|-----------|
| P1-5 | FAT BPB Probe in Format-Detection | S | FAT12/16/32 erkannt, keine false-success |

### Neue P2 Tasks

| ID | Task | Aufwand | Akzeptanz |
|----|------|---------|-----------|
| P2-6 | Geometry Guess Table | S | raw .img mit typischen Größen erkannt |
| P2-7 | UFI HAL pro OS | L | Linux SG_IO, Windows DeviceIoControl |
| P2-8 | Serial Framing + CRC | M | Resync bei Bitflip/Drop |

---

## 6. Empfehlung

**BEIDE PAKETE MERGEN** ✅

1. Keine Konflikte mit bestehendem Code
2. Erweitern wichtige Funktionen:
   - Format-Erkennung (P1-5: FAT Probe)
   - Serial I/O (P1-6: Amiga Capture)
   - HAL-Abstraktion (Blockdev, UFI)
3. Saubere Code-Qualität (C11, documented)
4. Confidence-basierte Erkennung (wie gefordert)

---

*Report: R5 Integration Engineer*

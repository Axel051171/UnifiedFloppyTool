# DD-Module Analyse für UnifiedFloppyTool

## Analysierte Quellen

| Tool | Version | Zeilen | Lizenz | Spezialisierung |
|------|---------|--------|--------|-----------------|
| **GNU dd** | coreutils 2025 | 2,531 | GPL-3 | Standard-Kopieren |
| **dd_rescue** | 1.99.22 | 3,829 | GPL-2/3 | Recovery, Reverse, Adaptive |
| **DC3DD** | debian | 4,500+ | GPL-3 | Forensik, Hashing, Wipe |
| **dcfldd** | master | ~2,000 | GPL-2 | Multi-Output, Hash-on-copy |

---

## Übernommene Features

### Von dd_rescue (Recovery-Spezialist)

```
✅ Reverse Reading (--reverse)
   - Liest Disk rückwärts
   - Kritisch bei Head-Crashes am Anfang
   - UFT: recovery.reverse = true

✅ Adaptive Block Size
   - Große Blöcke (softbs) für Speed
   - Kleine Blöcke (hardbs) bei Fehlern
   - Automatische Anpassung
   - UFT: blocksize.auto_adjust = true

✅ Sparse Output
   - Leere Bereiche nicht schreiben
   - Spart Platz bei Partial Reads
   - UFT: recovery.sparse = true

✅ Error Handling
   - max_errors: Limit vor Abbruch
   - retry_count: Wiederholungen pro Sektor
   - retry_delay: Wartezeit zwischen Retries
   - fill_on_error: Pattern für unlesbare Sektoren

✅ Sync Control
   - sync_writes: Nach jedem Write
   - sync_frequency: Alle N Blöcke
```

### Von DC3DD (Forensik-Spezialist)

```
✅ Multi-Algorithm Hashing
   - MD5, SHA-1, SHA-256, SHA-512
   - Gleichzeitig Input und Output
   - UFT: hash.algorithms Bitmask

✅ Verification
   - Hash-Vergleich nach Kopie
   - Re-Read Verification
   - UFT: hash.verify_after = true

✅ Wipe Patterns
   - Zero, One, Random
   - DoD 5220.22-M (3/7-pass)
   - Gutmann 35-pass
   - UFT: wipe.pattern Enum

✅ Forensic Logging
   - Timestamps
   - Error details
   - Hash results
   - Chain of custody
```

### Von dcfldd (Multi-Output)

```
✅ Split Output
   - Teile in mehrere Dateien
   - Konfigurierbare Größe
   - UFT: output.split_size

✅ Progress Display
   - Bytes, MB, Prozent
   - Geschwindigkeit
   - ETA
   - UFT: dd_status_t Struktur

✅ Pattern Input
   - Schreiben ohne Input-Datei
   - Für Disk-Wipe/Fill
   - UFT: Via wipe.pattern
```

---

## NEU: Floppy-Ausgabe

Speziell für UFT entwickelt - direktes Schreiben auf Floppy:

```c
typedef struct {
    bool enabled;               /* Floppy-Modus aktiv */
    floppy_type_t type;         /* RAW_DEVICE, USB, Greaseweazle, etc */
    const char *device;         /* /dev/fd0 oder \\.\A: */
    
    /* Geometrie */
    int tracks;                 /* 40-85 */
    int heads;                  /* 1-2 */
    int sectors_per_track;      /* 1-21 */
    int sector_size;            /* 128-1024 */
    
    /* Write Options */
    bool format_before;         /* Vorher formatieren */
    bool verify_sectors;        /* Jeden Sektor verifizieren */
    int write_retries;          /* Wiederholungen bei Fehler */
    bool skip_bad_sectors;      /* Weitermachen bei Fehler */
    
    /* Timing (für Hardware-Controller) */
    int step_delay_ms;          /* Head Step Delay */
    int settle_delay_ms;        /* Head Settle Delay */
    int motor_delay_ms;         /* Motor Spin-up */
} dd_floppy_t;
```

### Unterstützte Formate:

| Format | Tracks | Heads | SPT | Kapazität |
|--------|--------|-------|-----|-----------|
| DD 720K | 80 | 2 | 9 | 737,280 |
| HD 1.44M | 80 | 2 | 18 | 1,474,560 |
| DD 360K | 40 | 2 | 9 | 368,640 |
| HD 1.2M | 80 | 2 | 15 | 1,228,800 |
| Amiga DD | 80 | 2 | 11 | 901,120 |
| Amiga HD | 80 | 2 | 22 | 1,802,240 |

---

## GUI-Parameter (Qt Widget)

### Tab 1: Input/Output

| Widget | Parameter | Range | Default |
|--------|-----------|-------|---------|
| LineEdit | input_file | File path | - |
| LineEdit | output_file | File path | - |
| SpinBox | skip_bytes | 0-∞ | 0 |
| SpinBox | seek_bytes | 0-∞ | 0 |
| SpinBox | max_bytes | 0-∞ | 0 (all) |

### Tab 2: Block Size

| Widget | Parameter | Range | Default |
|--------|-----------|-------|---------|
| ComboBox | soft_blocksize | 512B-1MB | 128KB |
| ComboBox | hard_blocksize | 512B-4KB | 512B |
| CheckBox | auto_adjust | - | true |
| CheckBox | direct_io | - | false |
| CheckBox | sync_writes | - | false |
| SpinBox | sync_frequency | 0-10000 | 0 |

### Tab 3: Recovery

| Widget | Parameter | Range | Default |
|--------|-----------|-------|---------|
| CheckBox | enabled | - | true |
| CheckBox | reverse | - | false |
| CheckBox | sparse | - | false |
| CheckBox | continue_on_error | - | true |
| CheckBox | fill_on_error | - | true |
| SpinBox | fill_pattern | 0x00-0xFF | 0x00 |
| SpinBox | max_errors | 0-100000 | 0 |
| SpinBox | retry_count | 0-100 | 3 |
| SpinBox | retry_delay | 0-10000ms | 100 |

### Tab 4: Hashing

| Widget | Parameter | Default |
|--------|-----------|---------|
| CheckBox | MD5 | false |
| CheckBox | SHA-1 | false |
| CheckBox | SHA-256 | false |
| CheckBox | SHA-512 | false |
| CheckBox | hash_input | false |
| CheckBox | hash_output | false |
| CheckBox | verify_after | false |
| LineEdit | expected_hash | - |

### Tab 5: Floppy Output

| Widget | Parameter | Range | Default |
|--------|-----------|-------|---------|
| CheckBox | enabled | - | false |
| ComboBox | device | System | /dev/fd0 |
| ComboBox | format | Presets | Auto |
| SpinBox | tracks | 40-85 | 80 |
| SpinBox | heads | 1-2 | 2 |
| SpinBox | sectors_per_track | 1-21 | 18 |
| SpinBox | sector_size | 128-1024 | 512 |
| CheckBox | format_before | - | false |
| CheckBox | verify_sectors | - | true |
| SpinBox | write_retries | 0-20 | 3 |
| CheckBox | skip_bad_sectors | - | false |
| SpinBox | step_delay | 1-50ms | 3 |
| SpinBox | settle_delay | 5-100ms | 15 |
| SpinBox | motor_delay | 100-2000ms | 500 |

---

## Flux-Kompatibilität

### Integration mit Flux-Workflow

```
┌─────────────┐     ┌──────────────┐     ┌─────────────┐
│ Flux Capture│────►│ UFT Decoder  │────►│ Sector Data │
│ (.scp/.raw) │     │ (MFM/GCR/PLL)│     │ (Memory)    │
└─────────────┘     └──────────────┘     └──────┬──────┘
                                                │
                         ┌──────────────────────┼──────────────────────┐
                         ▼                      ▼                      ▼
                  ┌─────────────┐       ┌─────────────┐       ┌─────────────┐
                  │ Image File  │       │ Floppy Write│       │ Analysis    │
                  │ (.adf/.img) │       │ (dd_floppy) │       │ (Histogram) │
                  └─────────────┘       └─────────────┘       └─────────────┘
```

### Anwendungsfälle

1. **Flux → Image → Floppy**
   - Flux-Capture dekodieren
   - Sektoren zu Image zusammenbauen
   - Image auf physische Floppy schreiben

2. **Flux → Direct Floppy**
   - Dekodierte Sektoren direkt schreiben
   - Kein Zwischen-Image nötig
   - Schneller für Massenproduktion

3. **Recovery mit dd_rescue Algorithmen**
   - Adaptive Thresholds für Timing
   - Retries bei schwachen Signalen
   - Partial Recovery möglich

---

## Ideen für Weiterentwicklung

### 1. Bidirektionale Floppy-Operationen
```c
// Nicht nur Schreiben, auch Lesen mit dd_floppy
dd_floppy_read_image() → Image
dd_floppy_write_image() ← Image

// Vergleich: Floppy ↔ Image
dd_floppy_verify(floppy, image) → Differences
```

### 2. Flux-Aware Recovery
```c
// Bei Sektorfehlern: Neue Flux-Captures machen
if (sector_error) {
    flux_capture_multiple(track, head, 5);  // 5 Captures
    error_correction_multi_capture();        // Best bits extrahieren
}
```

### 3. Hardware-Controller Integration
```c
// Greaseweazle/FluxEngine/KryoFlux als Floppy-Backend
typedef enum {
    FLOPPY_RAW_DEVICE,      // OS Treiber
    FLOPPY_GREASEWEAZLE,    // Via USB
    FLOPPY_FLUXENGINE,      // Via USB
    FLOPPY_KRYOFLUX,        // Via USB
    FLOPPY_SUPERCARD_PRO    // Via USB
} floppy_backend_t;
```

### 4. Interaktiver Recovery-Modus
```c
// GUI zeigt jeden Fehler
// User entscheidet: Skip/Retry/Abort/Use-Pattern
recovery_action_t on_error_callback(sector_info_t *sector);
```

### 5. Copy Protection Handling
```c
// Spezielle Sektoren erkennen und korrekt schreiben
// z.B. Weak bits, Long tracks, Non-standard formats
copy_protection_t detect_protection(flux_data);
write_protected_track(floppy, protection, data);
```

---

## Dateien erstellt

| Datei | Zeilen | Beschreibung |
|-------|--------|--------------|
| `include/uft/uft_dd.h` | ~400 | Header mit allen Strukturen |
| `libflux_format/src/uft_dd.c` | ~600 | Core Implementation |
| `src/gui/DDParamsWidget.h` | ~140 | Qt Widget Header |
| `src/gui/DDParamsWidget.cpp` | ~650 | Qt Widget Implementation |
| **TOTAL** | **~1790** | Neue DD-Funktionalität |

---

## Zusammenfassung

Das DD-Modul kombiniert die besten Features aus vier verschiedenen dd-Varianten:

- **Recovery-Algorithmen** von dd_rescue
- **Forensik-Features** von DC3DD  
- **Multi-Output** von dcfldd
- **Standard-Kompatibilität** von GNU dd

Plus **neu entwickelte Floppy-Ausgabe** speziell für UFT.

Die GUI-Parameter ermöglichen volle Kontrolle über alle Aspekte des Kopiervorgangs, von Block-Größen über Recovery-Optionen bis hin zu Floppy-Timing.

# XDF ↔ XCopy Integration

## Überblick

Die XDF-API ist vollständig in das XCopy-System integriert:

```
┌─────────────────────────────────────────────────────────────────┐
│                    XDF → XCopy Workflow                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. Disk Image öffnen (*.xdf, *.img)                           │
│     └─► uft_detect_profile_by_size(1915904)                    │
│         └─► UFT_PROFILE_IBM_XDF                                │
│                                                                 │
│  2. AnalyzerToolbar Quick Scan                                 │
│     └─► uft_format_requires_track_copy("XDF") = true           │
│     └─► uft_xdf_recommended_copy_mode(false) = 2 (Track)       │
│                                                                 │
│  3. XCopy Panel empfängt:                                      │
│     └─► CopyMode::Track (automatisch gesetzt)                  │
│     └─► Warnung: "Variable Sektoren - Sector Copy nicht mögl." │
│                                                                 │
│  4. Bei Protection:                                             │
│     └─► uft_xdf_recommended_copy_mode(true) = 3 (Flux)         │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

## Warum XDF Sector Copy nicht funktioniert

```
Standard 1.44MB Disk:          XDF Disk (~1.86MB):
┌────────────────────┐         ┌────────────────────┐
│ Track 0:           │         │ Track 0:           │
│  18 × 512B = 9KB   │         │  1×8KB + 2KB + 1KB │
│                    │         │  + 512B = 11.5KB   │
│ Track 1-79:        │         │                    │
│  18 × 512B = 9KB   │         │ Track 1-79:        │
│                    │         │  2×8KB + 2KB + 1KB │
│ Total: 1.44MB      │         │  + 512B = 19.5KB   │
└────────────────────┘         │                    │
                               │ Total: ~1.86MB     │
Sector Copy: ✓ WORKS           └────────────────────┘
                               Sector Copy: ✗ FAILS
                               Track Copy:  ✓ WORKS
```

## API-Funktionen

### 1. Format-Erkennung

```c
// Automatisch bei Dateiöffnung
const uft_platform_profile_t *profile = uft_detect_profile_by_size(file_size);

// Erkannte XDF-Größen:
// 1915904 → IBM XDF (Standard)
// 1884160 → IBM XDF (Variante)
// 1720320 → Microsoft DMF (1.68MB)
```

### 2. Copy-Mode Entscheidung

```c
// Im AnalyzerToolbar/Backend:
if (uft_format_requires_track_copy(profile->name)) {
    // Sector Copy deaktivieren, Track Copy empfehlen
    int mode = uft_xdf_recommended_copy_mode(is_protected);
    // mode = 2 (Track) oder 3 (Flux)
    emit suggestMode(static_cast<CopyMode>(mode));
}
```

### 3. XCopy Panel Reaktion

```cpp
// In uft_xcopy_panel.cpp:
void XCopyPanel::suggestMode(CopyMode mode)
{
    // Automatisch umschalten
    m_modeCombo->setCurrentIndex(static_cast<int>(mode));
    
    // Warnung anzeigen wenn nötig
    if (mode >= CopyMode::Track) {
        m_statusLabel->setText(tr("Variable sectors detected - using %1 mode")
            .arg(mode == CopyMode::Track ? "Track" : "Flux"));
    }
}
```

### 4. Variable Sektoren pro Track

```c
// Für XDF-spezifische Operationen:
int sectors = uft_xdf_sectors_for_track(track);
// Track 0: 4 Sektoren (8KB + 2KB + 1KB + 512B)
// Track 1-79: 5 Sektoren (8KB + 8KB + 2KB + 1KB + 512B)

// Einzelne Sektorgröße:
int size = uft_xdf_sector_size(track, sector_index);
// Gibt 8192, 2048, 1024 oder 512 zurück
```

## Integration in AnalyzerToolbar

```cpp
// Nach Quick Scan:
void AnalyzerToolbar::onQuickScanComplete(const QuickScanSummary &summary)
{
    // Prüfen ob Track Copy nötig
    if (uft_format_requires_track_copy(summary.platform.toUtf8().constData())) {
        // Mode-Button auf Track setzen
        m_modeCombo->setCurrentIndex(2);  // Track
        
        // Apply-Button mit Warnung
        m_applyButton->setToolTip(
            tr("XDF requires Track Copy (variable sector sizes)"));
    }
    
    // Confidence-Bar aktualisieren
    m_confidenceBar->setValue(summary.confidence);
}
```

## Unterstützte Formate

| Format | Größe | Sektoren/Track | Copy Mode |
|--------|-------|----------------|-----------|
| XDF | 1.86MB | Variable (4-5) | Track |
| XXDF (2M) | ~1.8MB | Variable | Track |
| DMF | 1.68MB | 21 × 512B | Track* |
| Victor 9000 | Variable | 11-19 | Track |
| Apple II | 140KB | GCR | Track |
| C64 | 170KB | GCR | Track |

*DMF hat einheitliche Sektoren, braucht aber Track Copy wegen speziellem Timing

## Test

```c
// XDF-Erkennung testen:
const uft_platform_profile_t *p = uft_detect_profile_by_size(1915904);
assert(strcmp(p->name, "IBM XDF (Extended Density)") == 0);

// Track Copy erforderlich:
assert(uft_format_requires_track_copy("XDF") == true);
assert(uft_format_requires_track_copy("IBM PC HD") == false);

// Empfohlener Mode:
assert(uft_xdf_recommended_copy_mode(false) == 2);  // Track
assert(uft_xdf_recommended_copy_mode(true) == 3);   // Flux
```

## Zusammenfassung

**Ja, XCopy funktioniert mit XDF!**

Die Integration erfolgt automatisch:
1. ✅ XDF wird an Dateigröße erkannt (1.86MB, 1.68MB, etc.)
2. ✅ `uft_format_requires_track_copy()` erkennt variable Sektoren
3. ✅ XCopy Panel schaltet automatisch auf Track/Flux Mode
4. ✅ Sector Copy wird blockiert (würde fehlschlagen)
5. ✅ Per-Track Sektorinfo via `uft_xdf_sectors_for_track()`

"Bei uns geht kein Bit verloren" - auch bei XDF! 🖫

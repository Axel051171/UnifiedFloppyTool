# XCopy-Algorithmen: Format-Übertragung & GUI-Integration

**Datum:** 2026-01-09  
**Status:** Analyse + Konzept  
**Basis:** XCopy Pro (1989-2011) Algorithmen in `src/analysis/uft_track_analysis.{h,c}`

---

## 1. Aktueller Stand

### Bereits unterstützte Plattformen (13 Profile)

| Plattform | Encoding | Sync-Pattern | Sektoren/Track |
|-----------|----------|--------------|----------------|
| **Amiga DD** | MFM | 0x4489 + Specials | 11 |
| **Amiga HD** | MFM | 0x4489 + Specials | 22 |
| **Atari ST DD** | MFM | 0x4489 | 9 |
| **Atari ST HD** | MFM | 0x4489 | 18 |
| **IBM PC DD** | MFM | 0x4489 | 9 |
| **IBM PC HD** | MFM | 0x4489 | 18 |
| **Apple II DOS 3.3** | GCR 6&2 | 0xD5AA96 (24-bit) | 16 |
| **Apple II ProDOS** | GCR 6&2 | 0xD5AAAD (24-bit) | 16 |
| **C64** | GCR | 0x52 (8-bit) | 17-21 (zonenabhängig) |
| **BBC DFS** | FM | 0xFE | 10 |
| **BBC ADFS** | MFM | 0x4489 | 16 |
| **MSX** | MFM | 0x4489 | 9 |
| **Amstrad CPC** | MFM | 0x4489 | 9 |

### Generische Algorithmen (XCopy Pro)

```c
// 1. Sync-Suche mit Bit-Rotation (alle Plattformen)
int uft_find_syncs_rotated(data, size, patterns, count, bits, result);

// 2. Track-Typ-Klassifikation
uft_track_type_t:
    STANDARD, PROTECTED, LONG, SHORT, WEAK_BITS, 
    FUZZY, NO_SYNC, UNFORMATTED, DAMAGED, UNKNOWN

// 3. Sektor-Analyse (Längen-Distribution, GAP-Erkennung)
uft_sector_analysis_t

// 4. Protection-Detection (Breakpoints, Long-Tracks)
uft_track_analysis_t.is_protected, has_weak_bits, breakpoint_positions[]
```

---

## 2. Erweiterung auf weitere Formate

### 2.1 Neue Profile hinzufügen (Schritt-für-Schritt)

```c
// In uft_track_analysis.c:

// 1. Sync-Patterns definieren
static const uint32_t PC98_SYNCS[] = {
    0x4489,     // Standard MFM
    0x5224,     // PC-98 spezifisch
};

// 2. Profile erstellen
const uft_platform_profile_t UFT_PROFILE_PC98 = {
    .platform = PLATFORM_PC98,
    .encoding = ENCODING_MFM,
    .name = "NEC PC-98",
    .sync_patterns = PC98_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    .track_length_min = 10000,
    .track_length_max = 13000,
    .track_length_nominal = 12500,
    .long_track_threshold = 12800,
    .sectors_per_track = 8,     // oder 15 für HD
    .sector_size = 1024,        // PC-98 typisch
    .sector_mfm_size = 1200,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 360.0                // PC-98 spezifisch
};

// 3. In uft_get_profile_for_platform() registrieren
case PLATFORM_PC98: return &UFT_PROFILE_PC98;
```

### 2.2 Geplante Profile (TODO)

| Plattform | Prio | Aufwand | Besonderheit |
|-----------|------|---------|--------------|
| **PC-98** | P2 | S | 1024-byte Sektoren, 360 RPM |
| **X68000** | P2 | S | 1024-byte Sektoren |
| **FM-Towns** | P3 | M | Varianten DD/HD |
| **Victor 9000** | P3 | M | Eigenes GCR-Encoding |
| **SAM Coupé** | P3 | S | MGT Format |
| **Spectrum +3** | P3 | S | +3DOS Format |
| **Archimedes** | P2 | M | ADFS Varianten |
| **TI-99/4A** | P3 | S | FM/MFM Mix |

---

## 3. GUI-Integration Konzept

### 3.1 Aktuelles Widget (TrackAnalyzerWidget)

```
┌─────────────────────────────────────────────────────────────────┐
│ 🔍 Track Analyzer                                               │
├─────────────────────────────────────────────────────────────────┤
│ Platform: [Auto-Detect ▼] □ Auto-Detect                         │
│ Detected: Amiga DD (Confidence: 95%)                            │
├─────────────────────────────────────────────────────────────────┤
│ Quick Scan Results:                                             │
│   Platform: Amiga DD        Encoding: MFM                       │
│   Sectors: 11/track         Protection: Copylock                │
│   Recommended: Flux Copy    Confidence: ████████░░ 85%          │
├─────────────────────────────────────────────────────────────────┤
│ Track Heatmap:                                                  │
│   Track│ Type │Prot│WeakBits│Long│ Mode                         │
│   T00.0│ STD  │ ░░ │   ░░   │ ░░ │ Normal                       │
│   T00.1│ STD  │ ░░ │   ░░   │ ░░ │ Normal                       │
│   T01.0│ PROT │ ██ │   ░░   │ ░░ │ Nibble                       │
│   ...                                                           │
├─────────────────────────────────────────────────────────────────┤
│ [Quick Scan] [Full Analysis] [Export JSON] [Export Report]      │
│ [Apply to XCopy Panel]                                          │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Verbesserte Integration

#### A) Automatische Format-Erkennung + Analyse

```cpp
// In MainWindow beim Öffnen einer Datei:
void MainWindow::onFileOpened(const QString &path) {
    // 1. Format laden
    auto format = m_formatRegistry->detect(path);
    
    // 2. Track-Daten extrahieren
    QByteArray trackData = format->getRawTrackData();
    
    // 3. Analyse starten
    m_trackAnalyzer->setTrackData(trackData, format->trackCount(), format->sides());
    m_trackAnalyzer->runQuickScan();
}

// Quick-Scan Ergebnis verarbeiten
void MainWindow::onQuickScanComplete(const QuickScanResult &result) {
    // Status-Bar aktualisieren
    statusBar()->showMessage(
        QString("Detected: %1 | %2 sectors | %3")
        .arg(result.platformName)
        .arg(result.sectorsPerTrack)
        .arg(result.protectionDetected ? result.protectionName : "No protection")
    );
    
    // XCopy-Panel vorschlagen
    if (result.protectionDetected) {
        m_xcopyPanel->suggestMode(result.recommendedMode);
        showNotification("Protection detected - recommended mode: " + 
                        getCopyModeName(result.recommendedMode));
    }
}
```

#### B) XCopy-Panel Integration

```cpp
// In XCopyPanel Signals/Slots erweitern:
class XCopyPanel : public QWidget {
    Q_OBJECT
    
public slots:
    // Analyse-Ergebnisse übernehmen
    void applyAnalysisResults(CopyModeRecommendation overall,
                              const QVector<CopyModeRecommendation> &perTrack);
    
    // Per-Track Modi setzen
    void setTrackMode(int track, int side, CopyModeRecommendation mode);
    
    // Vorschlag anzeigen
    void suggestMode(CopyModeRecommendation mode);
    
signals:
    // Analyse anfordern
    void requestAnalysis();
    void requestQuickScan();
};
```

#### C) Neue Tab-Struktur im Hauptfenster

```
┌─────────────────────────────────────────────────────────────────┐
│ File  Edit  View  Tools  Help                                   │
├─────────────────────────────────────────────────────────────────┤
│ [📁 Browser] [🔍 Analyzer] [📊 XCopy] [💾 Write] [📋 Log]       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Tab-Inhalt je nach Auswahl                                     │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.3 Workflow-Integration

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  File Open   │────▶│ Quick Scan   │────▶│ Show Result  │
└──────────────┘     └──────────────┘     └──────────────┘
                            │
                            ▼
                     ┌──────────────┐
                     │ Protection?  │
                     └──────────────┘
                       │         │
                      Yes        No
                       │         │
                       ▼         ▼
              ┌──────────────┐  ┌──────────────┐
              │ Full Analyse │  │ Standard Copy │
              └──────────────┘  └──────────────┘
                       │
                       ▼
              ┌──────────────┐
              │ Per-Track    │
              │ Mode Map     │
              └──────────────┘
                       │
                       ▼
              ┌──────────────┐
              │ XCopy Panel  │
              │ Auto-Config  │
              └──────────────┘
```

---

## 4. Konkrete Implementierung

### 4.1 Format-Registry Integration

```cpp
// include/uft/format_registry_analysis.h

/**
 * @brief Format mit integrierter Analyse-Unterstützung
 */
class AnalyzableFormat : public DiskFormat {
public:
    /**
     * @brief Gibt das passende Analysis-Profil zurück
     */
    virtual const uft_platform_profile_t* getAnalysisProfile() const = 0;
    
    /**
     * @brief Raw Track-Daten für Analyse
     */
    virtual QByteArray getRawTrackData(int track, int side) const = 0;
    
    /**
     * @brief Vollständige Track-Analyse durchführen
     */
    virtual uft_track_analysis_t analyzeTrack(int track, int side) const;
};

// Beispiel: ADF Format
class AdfFormat : public AnalyzableFormat {
public:
    const uft_platform_profile_t* getAnalysisProfile() const override {
        return &UFT_PROFILE_AMIGA_DD;  // oder HD je nach Größe
    }
};
```

### 4.2 GUI-Erweiterung: Analyzer Toolbar

```cpp
// gui/AnalyzerToolbar.h

class AnalyzerToolbar : public QToolBar {
    Q_OBJECT
    
public:
    explicit AnalyzerToolbar(QWidget *parent = nullptr);
    
    void setAnalysisResult(const QuickScanResult &result);
    void showRecommendation(CopyModeRecommendation mode);
    
signals:
    void quickScanRequested();
    void fullAnalysisRequested();
    void applyToXCopyRequested();
    
private:
    QLabel *m_platformLabel;
    QLabel *m_protectionLabel;
    QLabel *m_modeLabel;
    QProgressBar *m_confidenceBar;
    QPushButton *m_analyzeBtn;
    QPushButton *m_applyBtn;
};
```

### 4.3 Beispiel: C64 D64 mit Analyse

```cpp
// Format-spezifische Analyse
class D64Format : public AnalyzableFormat {
public:
    const uft_platform_profile_t* getAnalysisProfile() const override {
        return &UFT_PROFILE_C64;
    }
    
    // C64-spezifisch: Zonen-abhängige Sektor-Anzahl
    int getSectorsForTrack(int track) const {
        if (track < 17) return 21;
        if (track < 24) return 19;
        if (track < 30) return 18;
        return 17;
    }
    
    uft_track_analysis_t analyzeTrack(int track, int side) const override {
        uft_track_analysis_t result;
        
        // Custom Analysis Config für C64
        uft_analysis_config_t cfg = {
            .track_data = getRawTrackData(track, side).constData(),
            .track_size = getRawTrackData(track, side).size(),
            .profile = &UFT_PROFILE_C64,
            .auto_detect_platform = false,
            .detect_protection = true,  // C64 hatte viele Kopierschutz-Systeme
            .detect_breakpoints = true,
        };
        
        uft_analyze_track_ex(&cfg, &result);
        return result;
    }
};
```

---

## 5. CMake-Integration

```cmake
# src/analysis/CMakeLists.txt (erweitert)

set(TRACK_ANALYSIS_SOURCES
    uft_track_analysis.c
    # Neue Profile
    profiles/uft_profile_pc98.c
    profiles/uft_profile_x68000.c
    profiles/uft_profile_archimedes.c
)

# GUI-Widget (wenn Qt verfügbar)
if(Qt6_FOUND)
    add_library(uft_track_analysis_gui
        TrackAnalyzerWidget.cpp
        AnalyzerToolbar.cpp
    )
    target_link_libraries(uft_track_analysis_gui
        PRIVATE uft_track_analysis Qt6::Widgets
    )
endif()
```

---

## 6. TODO Updates

### Neue P2 Tasks

| ID | Task | Aufwand | Beschreibung |
|----|------|---------|--------------|
| P2-9 | PC-98 Profile | S | 1024-byte Sektoren, 360 RPM |
| P2-10 | X68000 Profile | S | Sharp X68000 Support |
| P2-11 | Archimedes Profile | M | ADFS Varianten |
| P2-12 | Analyzer Toolbar | M | Kompakte Status-Anzeige |
| P2-13 | Format-Registry Analyse | M | AnalyzableFormat Interface |

### Empfohlene Reihenfolge

1. **P2-12: Analyzer Toolbar** (schnellste sichtbare Verbesserung)
2. **P2-13: Format-Registry Analyse** (Architektur-Basis)
3. **P2-9: PC-98 Profile** (häufig nachgefragt)

---

## 7. Quick-Start: Neues Profil in 5 Minuten

```c
// 1. In uft_track_analysis.c hinzufügen:

static const uint32_t MY_SYNCS[] = { 0x4489 };

const uft_platform_profile_t UFT_PROFILE_MYFORMAT = {
    .platform = PLATFORM_CUSTOM,
    .encoding = ENCODING_MFM,
    .name = "My Format",
    .sync_patterns = MY_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_nominal = 12500,
    .sectors_per_track = 9,
    .sector_size = 512,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

// 2. Header deklarieren:
extern const uft_platform_profile_t UFT_PROFILE_MYFORMAT;

// 3. Verwenden:
uft_track_analysis_t result;
uft_analyze_track_profile(track_data, track_size, &UFT_PROFILE_MYFORMAT, &result);
```

---

*Konzept: R4 (Format Engineer) + R3 (Architecture)*  
*"Bei uns geht kein Bit verloren"*

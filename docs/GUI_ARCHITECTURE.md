# UnifiedFloppyTool - Qt GUI Architecture

## Version: 3.1.4.010

## 1. Architektur-Übersicht

```
┌─────────────────────────────────────────────────────────────────┐
│                        Qt Application                            │
├─────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐    Signals    ┌──────────────────────────┐   │
│  │  MainWindow  │◄─────────────►│    UFTController         │   │
│  │   (View)     │               │   (ViewModel)            │   │
│  └──────┬───────┘               └───────────┬──────────────┘   │
│         │                                    │                  │
│         │ UI Events                          │ QThread          │
│         ▼                                    ▼                  │
│  ┌──────────────┐               ┌──────────────────────────┐   │
│  │ Custom       │               │     UFTWorker            │   │
│  │ Widgets      │               │  (Background Thread)     │   │
│  │ - TrackGrid  │               └───────────┬──────────────┘   │
│  │ - FluxVis    │                           │                  │
│  └──────────────┘                           │ C-API Calls      │
│                                             ▼                  │
├─────────────────────────────────────────────────────────────────┤
│                         C Core Library                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────────────────┐ │
│  │ GUI Params  │  │  Format     │  │  Forensic Imaging       │ │
│  │ Extended    │  │  Detection  │  │  (SIMD optimized)       │ │
│  └─────────────┘  └─────────────┘  └─────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
```

## 2. Threading-Modell

### 2.1 Warum QThread?

Die C-Core-Operationen (Disk-Lesen, Forensic Imaging, PLL-Dekodierung) sind 
**rechenintensiv** und würden die UI blockieren. Wir verwenden:

- **UFTWorker**: QObject in eigenem QThread
- **Signals/Slots**: Thread-sichere Kommunikation
- **QueuedConnection**: Automatische Event-Queue-Integration

### 2.2 Worker Pattern

```cpp
// Controller erstellt Worker und Thread
m_workerThread = new QThread(this);
m_worker = new UFTWorker();
m_worker->moveToThread(m_workerThread);

// Connections für Thread-Kommunikation
connect(m_worker, &UFTWorker::progress, 
        this, &UFTController::onWorkerProgress,
        Qt::QueuedConnection);  // Automatisch für Cross-Thread

// Starten einer Operation
QMetaObject::invokeMethod(m_worker, "process", Qt::QueuedConnection);
```

### 2.3 Cancellation

```cpp
// Atomic flag für sichere Cancellation
std::atomic<bool> m_cancelRequested{false};

void UFTWorker::process() {
    for (int track = 0; track < 80 && !m_cancelRequested; ++track) {
        // ... work ...
        emit progress(percent, status);
    }
}
```

## 3. Signal/Slot Best Practices

### 3.1 Modern Functor Syntax (Qt5+)

```cpp
// ✅ Richtig: Compile-Time Type Checking
connect(ui->sldRateOfChange, &QSlider::valueChanged,
        this, &MainWindow::onRateOfChangeChanged);

// ❌ Vermeiden: String-basierte Syntax
connect(ui->sldRateOfChange, SIGNAL(valueChanged(int)),
        this, SLOT(onRateOfChangeChanged(int)));
```

### 3.2 Lambda für einfache Updates

```cpp
connect(ui->sldPhaseCorrection, &QSlider::valueChanged,
        this, [this](int value) {
            float pct = value / 128.0f * 100.0f;
            ui->lblPhasePct->setText(QString("%1%").arg(pct, 0, 'f', 1));
        });
```

### 3.3 QSignalBlocker für Batch-Updates

```cpp
void MainWindow::syncSettingsToUI() {
    // Verhindert Signal-Kaskaden während Update
    QSignalBlocker b1(ui->sldRateOfChange);
    QSignalBlocker b2(ui->spnLowpass);
    QSignalBlocker b3(ui->cmbPlatform);
    
    ui->sldRateOfChange->setValue(settings.rate_of_change * 10);
    ui->spnLowpass->setValue(settings.lowpass_radius);
    ui->cmbPlatform->setCurrentIndex(settings.platform);
}
```

## 4. Memory Management

### 4.1 Qt Parent-System

```cpp
// Parent übernimmt Ownership - automatische Deletion
auto* label = new QLabel("Text", this);  // 'this' ist Parent

// Für Heap-Objekte ohne Parent
QScopedPointer<UFTWorker> m_worker;
```

### 4.2 Smart Pointers

```cpp
class MainWindow {
    QScopedPointer<Ui::MainWindow> ui;      // UI-Formular
    QScopedPointer<UFTController> m_controller;
    // ...
};
```

### 4.3 Thread Cleanup

```cpp
MainWindow::~MainWindow() {
    // Warte auf Worker-Thread bevor Destruction
    m_workerThread->quit();
    m_workerThread->wait();
}
```

## 5. Styling (QSS)

### 5.1 Dark Mode Farbpalette (Catppuccin Mocha)

```css
/* Basis-Farben */
--base:     #1e1e2e;  /* Hintergrund */
--surface0: #313244;  /* Erhöhte Elemente */
--surface1: #45475a;  /* Hover/Borders */
--text:     #cdd6f4;  /* Primärtext */
--subtext:  #6c7086;  /* Sekundärtext */

/* Akzent-Farben */
--blue:   #89b4fa;  /* Primary Actions */
--green:  #a6e3a1;  /* Success/OK */
--red:    #f38ba8;  /* Error/Danger */
--yellow: #f9e2af;  /* Warning */
--peach:  #fab387;  /* Secondary Warning */
```

### 5.2 Widget-Spezifische Styles

```css
/* Slider mit gefülltem Track */
QSlider::sub-page:horizontal {
    background-color: #89b4fa;
    border-radius: 3px;
}

/* Primary Button */
QPushButton#btnStart {
    background-color: #89b4fa;
    color: #1e1e2e;
}

/* Custom Properties für Status */
QWidget[sectorStatus="ok"] { 
    background-color: #a6e3a1; 
}
```

## 6. Custom Widgets

### 6.1 TrackGridWidget

Effizienter Sektor-Status-Visualizer mit:

- **QPainter**: Hardware-beschleunigte Renderung
- **Partial Updates**: Nur geänderte Bereiche neu zeichnen
- **Hover Effects**: Tooltips und Highlighting
- **Click Events**: Sektor-Details anzeigen

```cpp
void TrackGridWidget::updateSector(int track, int head, int sector, int status) {
    m_data[track][head][sector] = static_cast<SectorStatus>(status);
    
    // Nur betroffene Region neu zeichnen
    update(sectorRect(track, head, sector).adjusted(-2, -2, 2, 2));
}
```

### 6.2 FluxVisualizerWidget (TODO)

Für Flux-Daten-Darstellung:
- Waveform-Anzeige
- Zoom/Pan
- Anomaly-Highlighting

## 7. C-Core Integration

### 7.1 Header-Einbindung

```cpp
// In .cpp Dateien
extern "C" {
#include "uft/uft_gui_params_extended.h"
#include "uft/uft_forensic_imaging.h"
}
```

### 7.2 Callbacks für Progress

```c
// C-Seite: Callback-Typedef
typedef void (*uft_progress_cb)(void* user, int percent, const char* msg);

// C-Seite: Callback aufrufen
void uft_fi_set_progress_callback(uft_progress_cb cb, void* user);

// Qt-Seite: Wrapper
static void progressWrapper(void* user, int pct, const char* msg) {
    auto* worker = static_cast<UFTWorker*>(user);
    emit worker->progress(pct, QString::fromUtf8(msg));
}
```

## 8. Build-System

### 8.1 qmake (.pro)

```bash
qmake UnifiedFloppyTool.pro
make -j$(nproc)
```

### 8.2 CMake (Alternative)

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(UnifiedFloppyTool
    main.cpp
    gui/mainwindow.cpp
    gui/widgets/trackgridwidget.cpp
    # ... C sources
)

target_link_libraries(UnifiedFloppyTool PRIVATE Qt6::Widgets)
```

## 9. Dateistruktur

```
gui/
├── forms/
│   └── mainwindow.ui          # Qt Designer UI
├── widgets/
│   ├── trackgridwidget.h
│   ├── trackgridwidget.cpp
│   └── fluxvisualizerwidget.*
├── resources/
│   ├── resources.qrc
│   ├── icons/*.svg
│   └── fonts/*.ttf
├── styles/
│   └── darkmode.qss
├── mainwindow.h
└── mainwindow.cpp
```

## 10. Performance-Tipps

1. **Lazy Loading**: Tabs erst bei Aktivierung initialisieren
2. **Batch Updates**: Mehrere UI-Änderungen zusammenfassen
3. **Double Buffering**: Für Custom Widgets automatisch via QPainter
4. **Implicit Sharing**: QString, QVector etc. kopieren billig
5. **Avoid Blocking**: Alle I/O im Worker-Thread

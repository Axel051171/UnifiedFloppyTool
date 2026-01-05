# Sovox Amiga Disk Master - Code Analysis Report

## Übersicht

**Typ:** PyInstaller-gepackte Python 3.13 Tkinter GUI
**Hauptzweck:** Greaseweazle-Frontend für Amiga-Disketten schreiben
**Autor:** Sovox (mit Gemini AI Unterstützung)

## Extrahierte Architektur

### Klassenstruktur

```
GreaseweazleXCopyWriterApp (Hauptklasse)
├── __init__(master, instance_id, initial_x/y, width, height)
├── Track Grid System
│   ├── _setup_track_grid_ui_elements()      # 80 Track Grid (10x8)
│   ├── _update_track_cell_direct(idx, status)
│   ├── _update_track_cell_from_queue(idx, status)
│   └── _clear_track_display()
├── Timer System
│   ├── _start_timer()
│   ├── _stop_timer()
│   ├── _tick_timer()
│   ├── _reset_timer()
│   └── _update_timer_display()
├── Greaseweazle Interface
│   ├── execute_gw_command_threaded()        # Async GW-Ausführung
│   ├── stream_reader(stream, name)          # stdout/stderr Parsing
│   ├── monitor_gw_process()                 # Process-Überwachung
│   ├── start_write_thread()                 # Thread-Start
│   └── stop_gw_command()                    # Abbruch
├── Configuration
│   ├── load_app_config()
│   ├── save_app_config()
│   ├── load_translations(lang_code)
│   └── apply_app_settings()
└── UI
    ├── _initialize_fonts()
    ├── _update_ui_texts()
    ├── browse_source_file()
    └── handle_drop(event)                   # Drag & Drop

SettingsWindow
├── Font-Konfiguration für alle UI-Elemente
├── COM-Port / Drive-Auswahl pro Instanz
├── Multi-Instance-Unterstützung (bis zu 10)
└── Sprachauswahl (EN/IT)

AboutWindow
└── Info-Dialog mit Website-Link
```

### Konstanten und Defaults

```python
NUM_TRACKS_TO_PROCESS = 80       # Amiga DD: 80 Tracks
TRACK_COLS = 10                  # Grid-Spalten
TRACK_ROWS_DISPLAY = 9           # Grid-Zeilen (8 + Header)
NUM_DRAWN_CELLS = 80             # Total Zellen
MAX_INSTANCES_ALLOWED = 10       # Multi-Instance Limit

# Font Defaults
DEFAULT_UI_FONT_FAMILY = "Arial"
DEFAULT_GRID_FONT_FAMILY = "Consolas"  # Monospace für Grid
DEFAULT_BIG_DISPLAY_FONT_SIZE = 48     # Große Track-Nummer
DEFAULT_TIMER_FONT_SIZE = 24
```

## Interessante Implementierungsdetails

### 1. Track Grid X-Copy Style

Die Track-Anzeige folgt dem klassischen X-Copy Stil:
- 10x8 Grid für 80 Tracks
- Status-Farben: Pending (grau), Writing (gelb), OK (grün), Error (rot)
- Große numerische Track-Anzeige (48pt Font)
- Timer-Display für Schreibdauer

```
┌──────────────────────────────────────┐
│  0  1  2  3  4  5  6  7  8  9       │  ← Spalten 0-9
├──────────────────────────────────────┤
│  O  O  O  W  .  .  .  .  .  .  | 0  │  ← Tracks 0-9
│  .  .  .  .  .  .  .  .  .  .  | 1  │  ← Tracks 10-19
│  .  .  .  .  .  .  .  .  .  .  | 2  │
│  ...                           | 7  │  ← Tracks 70-79
└──────────────────────────────────────┘
```

### 2. Asynchrones Greaseweazle-Management

```python
# Threading-Architektur
def start_write_thread(self):
    self.stop_event.clear()
    self._start_timer()
    # Deaktiviere Start-Button, aktiviere Stop-Button
    threading.Thread(target=execute_gw_command_threaded).start()

def execute_gw_command_threaded(self):
    # Baue GW-Kommando
    command = [gw_exe, "write", "--drive", drive, source_file]
    
    # Starte Subprocess
    self.gw_process = subprocess.Popen(
        command,
        stdout=PIPE,
        stderr=PIPE,
        creationflags=CREATE_NO_WINDOW
    )
    
    # Starte Stream-Reader Threads
    Thread(target=stream_reader, args=(stdout, "stdout")).start()
    Thread(target=stream_reader, args=(stderr, "stderr")).start()
    Thread(target=monitor_gw_process).start()

def stream_reader(self, stream, stream_name):
    while not self.stop_event.is_set():
        line = stream.readline()
        if line.startswith(b"T"):  # Track-Status
            self.output_queue.put(("track_update", track_num, status))

def monitor_gw_process(self):
    rc = self.gw_process.wait()
    if rc == 0:
        self.output_queue.put(("completed", "success"))
    else:
        self.output_queue.put(("error", rc))
```

### 3. Output Queue Pattern

Wichtig für Thread-sichere UI-Updates:

```python
def process_output_queue(self):
    """Wird via after() im Main-Thread aufgerufen."""
    try:
        while True:
            msg = self.output_queue.get_nowait()
            if msg[0] == "track_update":
                self._update_track_cell_from_queue(msg[1], msg[2])
            elif msg[0] == "completed":
                self.reset_buttons_after_process()
            elif msg[0] == "log_message":
                self.add_log_message(msg[1])
    except queue.Empty:
        pass
    
    # Reschedule
    self._schedule_after(100, self.process_output_queue)
```

### 4. Multi-Instance Support

Die Anwendung unterstützt mehrere Instanzen gleichzeitig:
- Jede Instanz hat eigene COM-Port / Drive Konfiguration
- Launcher-Modus startet mehrere Fenster
- Separate Config-Dateien pro Instanz

```json
// config/gw_instance_0_config.json
{
    "com_port": "COM10",
    "drive_letter": "A"
}
```

### 5. Internationalisierung (i18n)

```python
def load_translations(self, lang_code):
    path = os.path.join(TRANSLATIONS_DIR, f"translations_{lang_code}.json")
    with open(path, 'r') as f:
        self.translations = json.load(f)

def _update_ui_texts(self):
    self.com_label.config(text=self.translations['com_label'])
    self.start_button.config(text=self.translations['start_button'])
```

## Relevante Konzepte für UnifiedFloppyTool

### 1. Track Grid Widget

**ÜBERNEHMEN:** Das X-Copy-Style Track Grid ist ein bewährtes UI-Pattern.
Wir haben bereits `TrackGridWidget` in Qt - können aber die visuelle
Darstellung verbessern:

- Größere Track-Nummer-Anzeige (48pt)
- Timer-Display für Operationsdauer
- Status-Farben konsistent mit X-Copy Tradition

### 2. Async Process Management

**ÜBERNEHMEN:** Das Queue-basierte Pattern für Subprocess-Kommunikation:

```cpp
// Qt-Äquivalent für UFTWorker
void UFTWorker::processReadDisk() {
    QProcess gw;
    gw.start("gw", {"read", "--drive", drive, output});
    
    while (gw.waitForReadyRead()) {
        QString line = gw.readLine();
        if (line.startsWith("T")) {
            int track = parseTrack(line);
            emit trackCompleted(track, status);
        }
    }
}
```

### 3. Multi-Instance Architecture

**INSPIRATION:** Für Multi-Drive-Support könnten wir:
- Mehrere UFTWorker parallel laufen lassen
- Jeder Worker hat eigene Hardware-Konfiguration
- Zentrale Koordination über Controller

### 4. Timer Integration

**ÜBERNEHMEN:** Operationsdauer-Timer ist nützlich für Forensik-Imaging:

```cpp
// In MainWindow
QElapsedTimer m_operationTimer;
QTimer* m_displayTimer;

void MainWindow::onOperationStarted() {
    m_operationTimer.start();
    m_displayTimer->start(1000);  // Update every second
}

void MainWindow::updateTimerDisplay() {
    qint64 elapsed = m_operationTimer.elapsed() / 1000;
    int min = elapsed / 60;
    int sec = elapsed % 60;
    ui->lblTimer->setText(QString("TIME: %1:%2")
        .arg(min, 2, 10, QChar('0'))
        .arg(sec, 2, 10, QChar('0')));
}
```

### 5. Drag & Drop für Disk Images

**ÜBERNEHMEN:** Bereits implementiert, aber Dateityp-Validierung verbessern:

```cpp
void MainWindow::dropEvent(QDropEvent* event) {
    QStringList supportedExts = {"adf", "ipf", "scp", "img", "hfe", "raw"};
    
    for (const QUrl& url : event->mimeData()->urls()) {
        QString path = url.toLocalFile();
        QString ext = QFileInfo(path).suffix().toLower();
        
        if (!supportedExts.contains(ext)) {
            QMessageBox::warning(this, tr("Unsupported File"),
                tr("File '%1' is not a supported disk image format.")
                .arg(QFileInfo(path).fileName()));
            continue;
        }
        
        loadDiskImage(path);
    }
}
```

### 6. COM-Port Detection

**ÜBERNEHMEN:** Automatische Erkennung von Greaseweazle-Geräten:

```cpp
// Verwende QSerialPortInfo
QStringList MainWindow::detectGreaseweazleDevices() {
    QStringList devices;
    
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        // Greaseweazle hat spezifische VID/PID
        if (port.vendorIdentifier() == 0x1209 && 
            port.productIdentifier() == 0x4d69) {
            devices << port.portName();
        }
    }
    
    return devices;
}
```

## Zusammenfassung: Was wir übernehmen können

| Feature | Sovox | UFT Status | Aktion |
|---------|-------|------------|--------|
| X-Copy Track Grid | ✅ Tkinter | ✅ TrackGridWidget | Verbessern: Timer + große Track-Nr |
| Async GW Control | ✅ Threading | ✅ QThread | OK |
| Multi-Instance | ✅ Launcher | ❌ | Optional: Später |
| i18n System | ✅ JSON | ❌ | Qt Translation Files |
| Drag & Drop | ✅ tkinterdnd2 | ✅ QDropEvent | OK |
| COM Detection | ✅ pyserial | ❌ | QSerialPortInfo |
| Timer Display | ✅ | ❌ | ÜBERNEHMEN |
| Font Customization | ✅ Extensive | Partial | QSettings |

## Neue Features für UFT v3.1.4.012

1. **Operation Timer** - Zeigt Dauer der aktuellen Operation
2. **Große Track-Nummer-Anzeige** - 48pt Font für aktuelle Track
3. **COM-Port Auto-Detection** - Greaseweazle automatisch erkennen
4. **Multi-Drive Support** - Mehrere Laufwerke parallel (später)
5. **Verbesserte Status-Farben** - Konsistent mit X-Copy Tradition

## Zusätzliche Code-Erkenntnisse (Deep Analysis)

### 1. GW Output Parsing - Regex Patterns

```python
# Track-Status Pattern (aus stream_reader)
pattern_traccia_gw = r'^T(\d{1,2})\.(\d)'  # Matches: T00.0, T01.1, etc.

# Track-Prefix-Bereinigung
clean_pattern = r'^T\d{1,2}\.\d\s*([:*\-]+\s*)?'
```

### 2. Error Detection Keywords

```python
# Aus stream_reader extrahiert
error_keywords = (
    'error', 'fail', 'failed', 
    'verify failure', 'verification failed',
    'cannot', 'unable', 'invalid',
    'no index', 'no data', 'crc'
)

# Aus process_output_queue (mehrsprachig)
status_keywords = (
    'error', 'successo', 'completato', 'terminato',
    'interrotta', 'fallito', 'eccezione',
    'success', 'completed', 'terminated',
    'interrupted', 'failed', 'exception'
)
```

### 3. GW Command Structure

```python
# execute_gw_command_threaded
command = [
    gw_exe,           # Pfad zu gw.exe
    'write',          # Operation
    '--drive=' + drive,    # Drive-Buchstabe (A, B, 0, 1, 2)
    '--device=' + com_port, # COM-Port
    source_file       # Disk-Image Pfad
]

# Subprocess-Optionen
subprocess.Popen(
    command,
    stdout=PIPE,
    stderr=PIPE,
    creationflags=CREATE_NO_WINDOW  # Windows: kein Konsolenfenster
)
```

### 4. Unterstützte Disk-Image Formate

```python
# Aus handle_drop
valid_extensions = ('.adf', '.ipf', '.scp', '.img', '.hfe')
```

### 5. Drag & Drop Pattern

```python
# Regex für Windows-Style Dateiliste
pattern = r'\{([^{}]+)\}|([^\s]+)'
# Matcht: {C:\path with spaces\file.adf} oder C:\path\file.adf
```

### 6. Track-Cell Status-Zustände

```python
# Aus _update_track_cell_direct
status_mapping = {
    'pending': ('gray', ''),      # Noch nicht bearbeitet
    'extra_pending': ('gray', ''),
    'writing': ('yellow', 'W'),   # Wird gerade geschrieben
    'written': ('green', 'O'),    # Erfolgreich geschrieben (OK)
    'error': ('red', 'E'),        # Fehler
}
```

### 7. Farb-Konstanten (X-Copy Style)

```python
# Aus __init__
COLOR_OK = '#32CD32'        # LimeGreen - Erfolgreich
COLOR_WRITING = '#FFFF99'   # LightYellow - In Bearbeitung
COLOR_GRID_BG = '#404040'   # DarkGray - Hintergrund
COLOR_GRID_FG = '#555555'   # Gray - Gitternetz
```

### 8. UI Dimensionen

```python
# Default Fenster
DEFAULT_WIDTH = 380
DEFAULT_HEIGHT = 680

# Track-Grid
TRACK_CELL_MIN = 28  # Minimale Zellgröße
GRID_COLS = 10       # 10 Tracks pro Zeile
GRID_ROWS = 8        # 8 Zeilen = 80 Tracks

# About-Dialog
ABOUT_WIDTH = 600
ABOUT_HEIGHT = 500
```

### 9. Queue-basierte Kommunikation

```python
# Message-Typen in output_queue
MESSAGE_TYPES = {
    'track_writing': (track_idx,),           # Track wird geschrieben
    'track_written_final': (track_idx,),     # Track fertig
    'track_error': (track_idx, error_msg),   # Track-Fehler
    'general_status': (status_msg,),         # Status-Update
    'log_message': (log_msg,),               # Log-Eintrag
    'clear_display': (),                     # Grid zurücksetzen
    '---PROCESS_COMPLETED_SIGNAL---': (),    # Operation beendet
}
```

## Implementierte UFT-Komponenten (basierend auf Sovox-Analyse)

| Komponente | Datei | Beschreibung |
|------------|-------|--------------|
| GW Output Parser | `gw_output_parser.h/.cpp` | Regex-basiertes Parsing |
| Device Detector | `gw_device_detector.h/.cpp` | USB VID/PID + String-Match |
| Image Validator | `disk_image_validator.h/.cpp` | Format-Erkennung + Magic |
| TrackGridWidget | Erweitert | Writing/Verifying Status |

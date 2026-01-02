# UFT GUI Guidelines

**Version:** 1.0  
**Status:** VERBINDLICH

---

## 1. Threading-Regeln (Frage 17)

### 1.1 Goldene Regel

> **Der UI-Thread darf NIEMALS länger als 16ms blockieren.**

16ms = 60 FPS. Jede längere Blockierung führt zu "Rucklern" oder "Einfrieren".

### 1.2 Was darf im UI-Thread laufen?

| Operation | UI-Thread | Worker-Thread |
|-----------|-----------|---------------|
| Widget-Updates | ✅ | ❌ |
| Signal-Emit | ✅ | ✅ (queued) |
| Layout-Berechnung | ✅ | ❌ |
| File-Dialog öffnen | ✅ | ❌ |
| Config lesen (klein) | ✅ | ⚠️ |
| Disk-Image öffnen | ❌ | ✅ |
| Track lesen | ❌ | ✅ |
| Track schreiben | ❌ | ✅ |
| Konvertierung | ❌ | ✅ |
| Format-Detection | ❌ | ✅ |
| Hardware-Zugriff | ❌ | ✅ |

### 1.3 Worker-Thread Architektur

```cpp
// gui/workers/DiskWorker.h

class DiskWorker : public QThread {
    Q_OBJECT
    
public:
    enum Operation { ReadDisk, WriteDisk, ConvertDisk };
    
    void setOperation(Operation op, const QString& path);
    void requestCancel() { m_cancelled = true; }
    
signals:
    void progressChanged(int percent, const QString& message);
    void trackCompleted(int cylinder, int head);
    void finished(bool success, const QString& error);
    void warningOccurred(const QString& message);
    
protected:
    void run() override {
        for (int cyl = 0; cyl < m_cylinders && !m_cancelled; cyl++) {
            emit progressChanged(cyl * 100 / m_cylinders, 
                                 tr("Reading track %1").arg(cyl));
            
            // Actual work
            readTrack(cyl);
            
            emit trackCompleted(cyl, 0);
        }
        
        emit finished(!m_cancelled, m_cancelled ? tr("Cancelled") : "");
    }
    
private:
    std::atomic<bool> m_cancelled{false};
    int m_cylinders{80};
};
```

### 1.4 Progress-Dialog Pattern

```cpp
// gui/dialogs/ProgressDialog.cpp

void MainWindow::onReadDisk() {
    ProgressDialog* dlg = new ProgressDialog(this);
    dlg->setWindowTitle(tr("Reading Disk"));
    dlg->setCancelButton(tr("Cancel"));
    
    DiskWorker* worker = new DiskWorker();
    worker->setOperation(DiskWorker::ReadDisk, m_currentPath);
    
    // Connections
    connect(worker, &DiskWorker::progressChanged,
            dlg, &ProgressDialog::setProgress);
    connect(worker, &DiskWorker::finished,
            dlg, &ProgressDialog::close);
    connect(dlg, &ProgressDialog::cancelled,
            worker, &DiskWorker::requestCancel);
    connect(worker, &DiskWorker::finished,
            worker, &QObject::deleteLater);
    
    worker->start();
    dlg->exec();  // Modal, aber UI bleibt responsiv
}
```

### 1.5 Cancel-Anforderungen

| Anforderung | Implementierung |
|-------------|-----------------|
| Jede lange Operation muss abbrechbar sein | `std::atomic<bool> m_cancelled` |
| Cancel reagiert innerhalb 100ms | Check in innerer Schleife |
| UI zeigt Cancel-Status | `progressChanged("Cancelling...")` |
| Cleanup bei Cancel | RAII oder finally-Block |

---

## 2. Fehlermeldungen (Frage 18)

### 2.1 Error-Message Struktur

```cpp
struct UserError {
    QString title;          // Kurz, beschreibend
    QString message;        // Was ist passiert (User-Sprache)
    QString suggestion;     // Was kann der User tun
    QString technical;      // Technische Details (versteckt)
    
    enum Severity { Info, Warning, Error, Critical };
    Severity severity;
};
```

### 2.2 Beispiele

**SCHLECHT:**
```
Error: -5
```

**BESSER:**
```
Error: UFT_ERROR_FILE_READ
```

**GUT:**
```
┌─────────────────────────────────────────────────────────────┐
│  ❌  Cannot Read Disk Image                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  The file "game.d64" could not be read completely.          │
│                                                             │
│  Track 18 appears to be damaged.                            │
│                                                             │
│  Suggestions:                                               │
│  • Try cleaning the original disk and re-imaging            │
│  • Enable "Skip bad sectors" in settings                    │
│  • Try a flux-level capture (SCP) for better recovery       │
│                                                             │
│  [Show Technical Details]              [OK]                 │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 Error-Mapping

```cpp
// gui/ErrorMessages.cpp

QString getUserMessage(uft_error_t err) {
    switch (err) {
        case UFT_ERROR_FILE_OPEN:
            return tr("Cannot open file. Check that it exists and "
                      "you have permission to access it.");
                      
        case UFT_ERROR_FORMAT_INVALID:
            return tr("The file format is not recognized. "
                      "It may be corrupted or not a disk image.");
                      
        case UFT_ERROR_CRC:
            return tr("Data verification failed. The disk or file "
                      "may be damaged.");
                      
        case UFT_ERROR_WRITE_PROTECTED:
            return tr("The disk is write-protected. "
                      "Remove write protection or use read-only mode.");
                      
        // ... weitere Mappings
        
        default:
            return tr("An unexpected error occurred.");
    }
}

QString getSuggestion(uft_error_t err, const QString& context) {
    switch (err) {
        case UFT_ERROR_FILE_OPEN:
            return tr("• Check the file path\n"
                      "• Ensure the file is not locked by another program\n"
                      "• Try running as administrator");
                      
        case UFT_ERROR_FORMAT_INVALID:
            return tr("• Try a different format\n"
                      "• Use 'Detect Format' to auto-detect\n"
                      "• Check if file was fully downloaded");
                      
        // ...
    }
}
```

### 2.4 Logging für Support

```cpp
// Bei jedem Fehler: Vollständiges Logging
void logError(uft_error_t err, const QString& context) {
    qCritical() << "UFT Error:" << uft_error_string(err)
                << "Context:" << context
                << "File:" << m_currentPath
                << "Version:" << UFT_VERSION
                << "OS:" << QSysInfo::prettyProductName();
}
```

---

## 3. Status-Anzeigen

### 3.1 Während Operationen

```
┌─────────────────────────────────────────────────────────────┐
│  Reading: game.scp                                          │
│  ████████████████████░░░░░░░░░░░░░░░░░░░░  45%             │
│  Track 36/80 (Head 0)                                       │
│  Elapsed: 0:12  Remaining: ~0:15                            │
│                                                [Cancel]     │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 Nach Abschluss

```
┌─────────────────────────────────────────────────────────────┐
│  ✓  Read Complete                                           │
│                                                             │
│  File: game.scp                                             │
│  Format: SuperCard Pro (SCP)                                │
│  Tracks: 80 (40 cylinders × 2 heads)                        │
│  Size: 12.4 MB                                              │
│  Duration: 27 seconds                                       │
│                                                             │
│  ⚠️  2 tracks had read errors (recovered)                    │
│                                                             │
│                          [Show Details]        [OK]         │
└─────────────────────────────────────────────────────────────┘
```

---

## 4. Accessibility

### 4.1 Keyboard Navigation

| Aktion | Shortcut |
|--------|----------|
| Open | Ctrl+O |
| Save | Ctrl+S |
| Convert | Ctrl+E |
| Cancel | Esc |
| Preferences | Ctrl+, |

### 4.2 Screen Reader Support

- Alle Widgets haben `accessibleName`
- Progress wird als Text angesagt
- Fehler werden als Alert angesagt

---

## 5. Dark Mode

```cpp
// gui/themes/DarkTheme.qss

QMainWindow {
    background-color: #1e1e1e;
    color: #d4d4d4;
}

QProgressBar {
    background-color: #3c3c3c;
    border-radius: 4px;
}

QProgressBar::chunk {
    background-color: #0078d4;
    border-radius: 4px;
}

QPushButton {
    background-color: #0078d4;
    color: white;
    border: none;
    padding: 8px 16px;
    border-radius: 4px;
}

QPushButton:hover {
    background-color: #1084d8;
}

QPushButton:pressed {
    background-color: #006cc1;
}
```

---

**DOKUMENT ENDE**

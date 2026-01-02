# UnifiedFloppyTool v3.0 - Designer Edition

## 🎨 **GUI bearbeiten? NUR .ui Dateien öffnen!**

```
Qt Designer → File → Open → forms/*.ui
```

**Das war's! Alles editierbar! 🚀**

---

## 📦 **Was ist drin:**

```
UFT_Designer/
├── forms/                          ⭐ EDITIERE DIESE IN QT DESIGNER!
│   ├── mainwindow.ui              (Hauptfenster + Tabs)
│   ├── tab_simple.ui              (Tab 1: Simple Read/Write)
│   ├── tab_format.ui              (Tab 2: Format Settings)
│   ├── tab_geometry.ui            (Tab 3: Geometry/a8rawconv)
│   ├── tab_protection.ui          (Tab 4: Copy Protection)
│   ├── tab_flux.ui                (Tab 5: Flux Policy)
│   ├── tab_advanced.ui            (Tab 6: Advanced Options)
│   ├── tab_hardware.ui            (Tab 7: Hardware/FloppyControl)
│   └── visualdisk.ui              (Visual Disk Fenster)
│
├── src/                            (Minimaler C++ Code - NICHT EDITIEREN NÖTIG!)
│   ├── main.cpp                   (Entry Point)
│   ├── mainwindow.h/cpp           (Verwendet .ui Dateien)
│   └── visualdisk.h/cpp           (Verwendet .ui Datei)
│
├── resources/
│   └── resources.qrc              (Für Icons später)
│
├── UnifiedFloppyTool.pro          (Qt Creator Projekt)
└── README.md                      (Diese Datei)
```

---

## 🚀 **Sofort starten:**

### **1. Qt Creator öffnen:**

```bash
qt-creator UnifiedFloppyTool.pro
```

### **2. Kompilieren & Starten:**

```
Build → Run (Ctrl+R)
```

**FERTIG! GUI läuft! ✅**

---

## 🎨 **GUI bearbeiten (Qt Designer):**

### **Option A: In Qt Creator:**

```
1. Linke Sidebar → "Forms"
2. Doppelklick auf eine .ui Datei
3. Qt Designer öffnet sich
4. Editieren → Speichern
5. Build → Run
```

### **Option B: Standalone Qt Designer:**

```bash
designer forms/mainwindow.ui
```

Oder:

```
File → Open → forms/tab_simple.ui
```

---

## 📋 **ALLE Parameter im GUI:**

### **Tab 1: Simple (Read/Write)**
```
✅ Hardware Selection (Greaseweazle/SCP/KryoFlux/etc.)
✅ Device Path + Auto-Detect
✅ Image File Selection
✅ Read/Write/Verify Buttons
✅ Progress Bar
```

### **Tab 2: Format Settings**
```
✅ Track Type (IBM FM/MFM, Amiga MFM, Apple GCR, C64 GCR)
✅ Number of Tracks (1-256)
✅ Sides (1-2)
✅ Bitrate (250000, 500000, 1000000)
✅ RPM (300, 360)
✅ Sectors per Track (1-64)
✅ Sector Size (128, 256, 512, 1024)
✅ Sector ID Start
✅ Interleave, Skew
✅ GAP3, PRE-GAP
✅ Reverse Side / Inter-Side Sector Numbering
```

### **Tab 3: Geometry (a8rawconv)**
```
✅ CHS Geometry (Cylinders, Heads, Sectors, Size)
✅ Common Presets (Atari/C64/Amiga/PC)
✅ Format Hint (ATR, ATX, D64, IMG, D88, RAW)
✅ Sector Numbering (0-based/1-based)
✅ Calculated Values (Total Size, Total Sectors)
```

### **Tab 4: Protection (XCopy + Flags)**
```
✅ Auto-Detect Copy Protection
✅ Preserve Protection Features
✅ X-Copy Error Detection (8 Error Types)
   - Error 1: Sector Count != 11 (PROTECTION!)
   - Error 2-8: Sync, CRC, GAP errors
✅ UFM Protection Flags
   - Weak Bits, Long/Short Track
   - Bad CRC, Duplicate IDs
   - Non-Standard GAP, Sync Anomaly
```

### **Tab 5: Flux Policy**
```
✅ Speed Mode (Minimum/Normal/Maximum)
✅ Error Policy (Strict/Tolerant/Ignore)
✅ Scan Mode (Standard/Advanced)
✅ DPM Precision (Off/Normal/High)
✅ Retry Policy
   - Max Revolutions (1-10)
   - Max Resyncs (0-20)
   - Max Retries (0-100)
   - Settle Time (ms)
✅ Read Options
   - Ignore Read Errors
   - Fast Error Skip
   - Read Sidechannel
   - Advanced Scan Factor
✅ Write Options
   - Verify After Write
   - Close Session
   - Underrun Protection
```

### **Tab 6: Advanced**
```
✅ Verification Options
   - Verify After Read/Write
   - CRC/Checksum Verification
✅ Logging
   - Level (Off/Errors/Normal/Verbose/Debug)
   - Show Progress/Statistics
   - Log to File
✅ Behavior
   - Confirm Overwrite
   - Auto-Eject
   - Sound Notification
   - Save Settings
✅ Expert Options
   - Raw Mode
   - Preserve Timestamps
   - Capture Raw Flux
```

### **Tab 7: Hardware (FloppyControl)**
```
✅ Device Selection
   - Hardware Type (GW/SCP/KryoFlux/Arduino Due/etc.)
   - Device Path + Auto-Detect
   - Baud Rate (9600-115200)
✅ FloppyControl Timing
   - Motor Spinup Time (500ms)
   - Track Settle Time (3ms)
   - Step Pulse Width (6µs)
   - Capture Window (10µs)
   - Buffer Size (4KB/8KB/16KB)
   - Index Timeout (2000ms)
✅ Hardware Info
   - Vendor ID, Product ID
   - HW Revision, Firmware
   - Sample Clock
```

### **Visual Disk Fenster (separates Popup)**
```
✅ Disk View (kreisförmig, wie Screenshot 7)
✅ Grid View (Matrix, wie Screenshot 4)
✅ Toggle zwischen Views
✅ Zoom In/Out
✅ Export (PNG)
✅ Track Details Panel
   - Track/Side/Sectors
   - Good/Bad/Retry Count
   - Format/RPM/Encoding
✅ Farbcodierung
   - Grün = Good
   - Gelb = Retry
   - Rot = Bad
   - Blau = Protection
   - Grau = Unread
```

---

## 💻 **Was du brauchst:**

```
Qt 6.x (oder Qt 5.15+)
Qt Creator
C++17 Compiler
```

### **Installation:**

**Linux:**
```bash
sudo apt install qtcreator qt6-base-dev build-essential
```

**macOS:**
```bash
brew install qt-creator qt@6
```

**Windows:**
Download von https://www.qt.io/download

---

## 🔧 **Widgets hinzufügen/ändern:**

### **In Qt Designer:**

```
1. Öffne .ui Datei (z.B. tab_simple.ui)
2. Linke Sidebar → Widget auswählen
3. Auf Form ziehen
4. Properties editieren (rechts)
5. Speichern
6. Kompilieren in Qt Creator
```

### **Beispiel: Neuer Button in Tab Simple:**

```
1. Öffne forms/tab_simple.ui in Designer
2. Widget Box → Buttons → Push Button
3. Ziehe auf groupOperations
4. Properties:
   - objectName: btnMyButton
   - text: "Meine Funktion"
   - minimumHeight: 50
5. Speichern
6. In Qt Creator: Build → Run
```

---

## 📝 **Code-Anpassungen (nur wenn nötig):**

### **Wenn du Button-Funktionalität brauchst:**

**In src/mainwindow.cpp:**

```cpp
// 1. In loadTabWidgets() das Widget finden:
QPushButton* myButton = tabSimple->findChild<QPushButton*>("btnMyButton");

// 2. Signal verbinden:
if (myButton) {
    connect(myButton, &QPushButton::clicked, this, [this]() {
        QMessageBox::information(this, "Info", "Button geklickt!");
    });
}
```

**ABER:** Für einfaches GUI-Design brauchst du das NICHT!

---

## 🎯 **Workflow:**

```
1. ✅ .ui Datei in Qt Designer öffnen
2. ✅ GUI bearbeiten (drag & drop)
3. ✅ Speichern
4. ✅ In Qt Creator: Build → Run
5. ✅ FERTIG!

Kein Code-Editing nötig für:
- Layout ändern
- Widgets hinzufügen/entfernen
- Text ändern
- Farben/Styles anpassen
- Größen anpassen
```

---

## 📊 **Statistik:**

```
UI Dateien:       9 Files (.ui)
Source Code:      6 Files (.h/.cpp)
Total Lines:      ~500 Zeilen C++
Forms:            ~2000 Zeilen XML

Tabs:             7 Haupttabs
Separate Windows: 1 (Visual Disk)
Widgets:          ~150+ Widgets
Parameters:       100+ Einstellungen!
```

---

## ✅ **Vorteile dieser Version:**

```
✅ ALLE Parameter aus unserem System
   - a8rawconv ✅
   - XCopy Error Detection ✅
   - Copy Protection Flags ✅
   - Flux Policy ✅
   - FloppyControl Timing ✅

✅ Komplett über GUI editierbar
   - Kein Code-Editing nötig
   - Qt Designer drag & drop
   - Sofort sichtbar

✅ Professional Layout
   - 7 übersichtliche Tabs
   - Separates Visual Disk Fenster
   - Alle Screenshots berücksichtigt

✅ Erweiterbar
   - Neue Tabs leicht hinzufügen
   - Widgets einfach ändern
   - Layout flexibel anpassbar

✅ Sofort kompilierbar
   - Keine fehlenden Dateien
   - Minimaler C++ Code
   - Qt 6 ready
```

---

## 🐛 **Troubleshooting:**

### **"Cannot find ui_*.h files"**

```bash
# In Qt Creator:
Build → Clean All
Build → Run qmake
Build → Rebuild All
```

### **"Qt Designer can't open .ui file"**

```bash
# Check file permissions:
chmod 644 forms/*.ui

# Try standalone Designer:
designer forms/mainwindow.ui
```

### **"Widgets not showing"**

```
# Check in mainwindow.cpp:
# Tabs need layouts:
if (!ui->tabWidget->widget(0)->layout()) {
    ui->tabWidget->widget(0)->setLayout(new QVBoxLayout());
}
```

---

## 🎉 **Zusammenfassung:**

**Du hast JETZT:**

```
✅ Komplettes GUI mit 7 Tabs
✅ ALLE Parameter aus unserem System
✅ Komplett über Qt Designer editierbar
✅ Separates Visual Disk Fenster
✅ Sofort kompilierbar
✅ Production-ready
✅ KEIN Code-Editing nötig!

→ Einfach .ui Dateien öffnen
→ In Qt Designer bearbeiten
→ Kompilieren
→ FERTIG! 🚀
```

**VIEL ERFOLG! 😊**

---

**© 2025 - UnifiedFloppyTool Project - GPL v3.0**

# ⚠️ Input Validation & Error Detection System

## ✅ **JA! Wir brauchen Error/Warning Dialogs!**

---

## 🎯 **Validation System - Übersicht:**

```
3 Error-Level:
├── 🔴 ERROR (Kritisch - Operation blockieren!)
├── 🟡 WARNING (Warnung - Operation erlauben)
└── 🔵 INFO (Information - nur anzeigen)
```

---

## 🔴 **CRITICAL ERRORS (Blockieren!):**

### **1. Hardware-Konflikte:**

```
ERROR: Keine Hardware erkannt
├── Condition: Hardware Type ausgewählt, aber kein Device
├── Message: "No hardware detected at /dev/ttyACM0"
├── Fix: "Check connection and try Auto-Detect"
└── Action: [Read]/[Write] Buttons DEAKTIVIERT

ERROR: USB Floppy + Flux Capture
├── Condition: USB Floppy + "Capture Raw Flux" aktiv
├── Message: "USB Floppy drives cannot capture raw flux data"
├── Fix: "Use Greaseweazle/SCP for flux capture OR disable flux mode"
└── Action: Operation blockiert

ERROR: XUM1541 + Non-C64 Format
├── Condition: XUM1541 aktiv + Format != C64
├── Message: "XUM1541 only supports C64 formats (1541/1571/1581)"
├── Fix: "Change Format to 'C64 (GCR)' or use different hardware"
└── Action: Operation blockiert
```

### **2. Format-Konflikte:**

```
ERROR: C64 GCR + MFM Encoding
├── Condition: Format = "C64 GCR" + Track Type = "IBM MFM"
├── Message: "C64 uses GCR encoding, not MFM"
├── Fix: "Tab 5 → C64 Nibbler is active, Tab 3 (Format) should be disabled"
└── Action: Operation blockiert

ERROR: G64 Output + Non-C64 Format
├── Condition: Output Format = G64 + Format != C64
├── Message: "G64 is only for C64 GCR format"
├── Fix: "Change output to D64 or change source format to C64"
└── Action: Operation blockiert

ERROR: ATX Output + Non-Atari Format
├── Condition: Output Format = ATX + Format != Atari
├── Message: "ATX is only for Atari 8-bit format"
├── Fix: "Change output to ATR or change source format to Atari"
└── Action: Operation blockiert
```

### **3. Geometry-Fehler:**

```
ERROR: Tracks > 83
├── Condition: Tracks > 83
├── Message: "Maximum track count is 83"
├── Fix: "Reduce tracks to 83 or less"
└── Action: Wert automatisch auf 83 begrenzen

ERROR: Tracks < 35
├── Condition: Tracks < 35
├── Message: "Minimum track count is 35"
├── Fix: "Increase tracks to 35 or more"
└── Action: Wert automatisch auf 35 setzen

ERROR: Sector Size nicht Power-of-2
├── Condition: Sector Size != 128/256/512/1024/2048/4096
├── Message: "Sector size must be power of 2 (128, 256, 512, 1024, 2048, 4096)"
├── Fix: "Select valid sector size"
└── Action: Dropdown-basiert, keine freie Eingabe

ERROR: Sectors per Track > 64
├── Condition: Sectors > 64
├── Message: "Maximum sectors per track is 64"
├── Fix: "Reduce sectors to 64 or less"
└── Action: Wert automatisch auf 64 begrenzen
```

### **4. Operation-Fehler:**

```
ERROR: Write ohne Source File
├── Condition: Operation = "Write to Disk" + No File selected
├── Message: "No source image file selected"
├── Fix: "Select an image file in Tab 2 (Operations)"
└── Action: [Write] Button DEAKTIVIERT

ERROR: Read ohne Destination
├── Condition: Operation = "Read" + No output path
├── Message: "No output file specified"
├── Fix: "Specify output filename"
└── Action: [Read] Button DEAKTIVIERT

ERROR: Convert ohne beide Files
├── Condition: Operation = "Convert" + (No source OR No dest)
├── Message: "Both source and destination files required"
├── Fix: "Select source AND destination files"
└── Action: [Convert] Button DEAKTIVIERT
```

---

## 🟡 **WARNINGS (Erlauben, aber warnen!):**

### **1. Capacity Mismatches:**

```
WARNING: 1.44M Disk + 720K Format
├── Condition: Detected Drive = "1.44M HD" + Format = "720K DD"
├── Message: "Disk capacity (1.44M) exceeds format capacity (720K)"
├── Fix: "This is OK if intentional (HD disk formatted as DD)"
└── Action: Warnung zeigen, dann fortfahren

WARNING: 40-Track Drive + 80-Track Format
├── Condition: Detected Tracks = 40 + Format Tracks = 80
├── Message: "Drive has only 40 tracks, but format requires 80"
├── Fix: "Drive cannot read outer 40 tracks - data loss possible"
└── Action: Warnung zeigen, dann fortfahren

WARNING: Single-Side Drive + Double-Side Format
├── Condition: Detected Heads = 1 + Format Heads = 2
├── Message: "Drive is single-sided, format is double-sided"
├── Fix: "Only Side 0 can be read/written"
└── Action: Warnung zeigen, dann fortfahren
```

### **2. Protection Mismatches:**

```
WARNING: Protection Profile != Detected Format
├── Condition: Profile = "Amiga Standard" + Format = "C64 GCR"
├── Message: "Protection profile doesn't match disk format"
├── Fix: "Change profile to 'C64 Standard' or 'Custom'"
└── Action: Warnung zeigen, dann fortfahren

WARNING: Quick Mode + Write
├── Condition: Profile = "Quick Mode" + Operation = "Write"
├── Message: "Quick Mode ignores protection - write may fail on protected disks"
├── Fix: "Use appropriate protection profile for better results"
└── Action: Warnung zeigen, dann fortfahren
```

### **3. Manual Override Warnings:**

```
WARNING: Manual Override aktiv
├── Condition: "Manual Override" checkbox = checked
├── Message: "Auto-detected drive information is being ignored"
├── Fix: "Double-check manual settings are correct"
└── Action: Einmaliges Popup beim Aktivieren

WARNING: Greaseweazle ohne Drive Type
├── Condition: Hardware = Greaseweazle + Drive Type = "Unknown"
├── Message: "Drive type not set - operation may fail"
├── Fix: "Enable Manual Override and select drive type"
└── Action: Warnung bei [Read]/[Write] Klick
```

### **4. Performance Warnings:**

```
WARNING: Max Revolutions > 10
├── Condition: Max Revolutions > 10
├── Message: "Very high retry count - operation may be very slow"
├── Fix: "Reduce to 5 or less for normal operation"
└── Action: Warnung zeigen, dann fortfahren

WARNING: DPM High + Quick Mode
├── Condition: DPM = High + Error Policy = "Ignore"
├── Message: "High DPM precision with Quick Mode may waste time"
├── Fix: "Either use Normal DPM or change to Tolerant/Strict mode"
└── Action: Warnung zeigen, dann fortfahren

WARNING: Archive Mode + Fast Skip
├── Condition: Profile = "Archive Mode" + Fast Skip = enabled
├── Message: "Archive Mode should preserve all data, but Fast Skip may skip bad sectors"
├── Fix: "Disable Fast Skip for complete archiving"
└── Action: Warnung zeigen, dann fortfahren
```

---

## 🔵 **INFO (Nur anzeigen):**

### **1. Helpful Information:**

```
INFO: C64 Nibbler Mode aktiv
├── Condition: C64 Nibbler enabled
├── Message: "C64 Nibbler mode is active - Tab 3/4 are disabled"
├── Action: Einmaliges Info-Popup

INFO: First-Time Greaseweazle User
├── Condition: Hardware = Greaseweazle + First Use
├── Message: "Greaseweazle requires manual drive type selection"
├── Fix: "See Tab 8 (Hardware) → Manual Override"
├── Action: Info-Dialog beim ersten Start

INFO: Batch Mode
├── Condition: Batch Queue > 0 items
├── Message: "Batch mode: X disks in queue"
├── Action: Status-Anzeige in Tab 7
```

---

## 🎨 **GUI-Integration:**

### **Validation Dialog (dialog_validation.ui):**

```
┌─ Validation Error/Warning ──────────────┐
│                                          │
│  ⚠️    Configuration Error               │
│                                          │
│ ┌─ Error Details ─────────────────────┐ │
│ │ The selected configuration is        │ │
│ │ invalid.                             │ │
│ │                                      │ │
│ │ Issues found:                        │ │
│ │ • Tracks set to 90 (max is 83)      │ │
│ │ • C64 GCR + MFM encoding active     │ │
│ └──────────────────────────────────────┘ │
│                                          │
│ ┌─ Suggested Fix ──────────────────────┐ │
│ │ • Set Tracks to 83 (max)            │ │
│ │ • Activate C64 Nibbler (Tab 5)      │ │
│ │ • Disable Tab 3 (Format)            │ │
│ └──────────────────────────────────────┘ │
│                                          │
│ ☐ Don't show this warning again         │
│                                          │
│                 [Cancel] [OK]            │
└──────────────────────────────────────────┘
```

### **Inline Validation (in Tabs):**

```
Tab 3 (Format):

Tracks: [90] ← Eingabe

⚠️ Warning: Maximum tracks is 83
[Auto-Fix to 83]

→ Sofortige visuelle Warnung!
```

### **Button States:**

```
[Read] Button:
├── Enabled: Wenn Validation OK ✓
├── Disabled: Wenn kritischer Fehler ❌
└── Tooltip: "Error: No hardware detected"

[Write] Button:
├── Enabled: Wenn Validation OK + File selected ✓
├── Disabled: Sonst ❌
└── Tooltip: "Error: No source file selected"
```

---

## 📋 **Validation Rules - Komplett:**

### **Hardware:**

```
✅ Hardware Type != "Mock" → Device path must exist
✅ USB Floppy → No Flux Capture
✅ XUM1541 → Only C64 formats
✅ Greaseweazle → Drive type must be set (manual override)
✅ Device path exists and is accessible
```

### **Format:**

```
✅ C64 GCR → Track Type must be GCR (Tab 5 active, Tab 3 disabled)
✅ Amiga MFM → Track Type must be MFM
✅ Apple GCR → Track Type must be GCR
✅ Output format matches source format type
✅ G64 → Only for C64
✅ ATX → Only for Atari
✅ ADF/DMS → Only for Amiga
```

### **Geometry:**

```
✅ Tracks: 35 ≤ x ≤ 83
✅ Heads: 1 ≤ x ≤ 2
✅ Sectors: 1 ≤ x ≤ 64
✅ Sector Size: {128, 256, 512, 1024, 2048, 4096}
✅ Bitrate matches format (250K/500K/1M)
✅ RPM matches format (300/360)
```

### **Protection:**

```
⚠️ Profile matches format (Amiga → Amiga, C64 → C64)
⚠️ Archive Mode + Fast Skip = conflict
⚠️ Quick Mode + Write = warning
✅ X-Copy only for Amiga
✅ C64 Nibbler only for C64
✅ DiskDupe only for Amiga
```

### **Operations:**

```
✅ Read → Output file must be specified
✅ Write → Source file must exist
✅ Convert → Both source and dest files
✅ Verify → Both disk and file
✅ Format → Disk must be writable
```

### **Flux:**

```
⚠️ Max Revolutions > 10 = slow warning
⚠️ DPM High + Ignore Errors = waste warning
✅ Flux Capture → Hardware must support it
```

---

## 💻 **Implementation (Pseudo-Code):**

### **Validation on Button Click:**

```cpp
void MainWindow::on_btnRead_clicked() {
    // Validate configuration
    QStringList errors;
    QStringList warnings;
    
    // Check hardware
    if (!isHardwareConnected()) {
        errors << "No hardware detected";
    }
    
    // Check output file
    if (outputFile.isEmpty()) {
        errors << "No output file specified";
    }
    
    // Check format conflicts
    if (format == C64_GCR && trackType == IBM_MFM) {
        errors << "C64 uses GCR encoding, not MFM";
    }
    
    // Check geometry
    if (tracks > 83) {
        errors << "Tracks must be ≤ 83";
    }
    
    // Check capacity
    if (detectedDriveCapacity != formatCapacity) {
        warnings << "Capacity mismatch detected";
    }
    
    // Show errors
    if (!errors.isEmpty()) {
        showValidationDialog(ERROR, errors);
        return; // BLOCK operation!
    }
    
    // Show warnings
    if (!warnings.isEmpty()) {
        if (showValidationDialog(WARNING, warnings) == CANCEL) {
            return; // User cancelled
        }
    }
    
    // Proceed with operation
    doRead();
}
```

### **Real-Time Validation:**

```cpp
void TabFormat::on_spinTracks_valueChanged(int value) {
    // Real-time check
    if (value > 83) {
        // Show inline warning
        labelTrackWarning->setText("⚠️ Maximum is 83");
        labelTrackWarning->setStyleSheet("color: red;");
        labelTrackWarning->show();
        
        // Auto-fix option
        btnAutoFixTracks->show();
        
        // Disable operation buttons
        emit validationError("Tracks > 83");
    } else {
        labelTrackWarning->hide();
        btnAutoFixTracks->hide();
        emit validationOK();
    }
}
```

---

## 📊 **Error Message Templates:**

### **Template 1: Hardware Error**

```
Title: "Hardware Error"
Icon: 🔴
Message: "No {hardware_type} detected at {device_path}"
Details: 
  • Check USB connection
  • Verify device path
  • Try Auto-Detect
Suggestion: "Click Auto-Detect in Tab 8 (Hardware)"
Buttons: [OK]
```

### **Template 2: Format Conflict**

```
Title: "Format Conflict"
Icon: 🔴
Message: "{format1} is incompatible with {format2}"
Details:
  • Current Format: {format1}
  • Current Encoding: {format2}
  • These are incompatible
Suggestion: "Activate C64 Nibbler mode in Tab 5 (Protection)"
Buttons: [Auto-Fix] [Cancel]
```

### **Template 3: Capacity Warning**

```
Title: "Capacity Mismatch"
Icon: 🟡
Message: "Drive capacity ({drive_cap}) != Format capacity ({format_cap})"
Details:
  • This may be intentional (HD disk as DD)
  • Or it could indicate wrong format selection
Suggestion: "Verify format settings match your disk"
Buttons: [Continue Anyway] [Cancel]
```

---

## ✅ **Summary - Validation System:**

```
✅ Critical Errors (🔴) → Block operation
✅ Warnings (🟡) → Show dialog, allow continue
✅ Info (🔵) → Just display

✅ Real-time validation (inline)
✅ Pre-operation validation (dialog)
✅ Auto-fix options where possible
✅ Clear error messages + suggestions
✅ Button enable/disable based on validation

Dialog: dialog_validation.ui
Integration: All tabs with validation logic
```

---

## 🚀 **Nächste Schritte:**

```
1. dialog_validation.ui erstellt ✓
2. Validation logic (C++) implementieren
3. Inline warnings zu allen Input-Feldern
4. Button states dynamisch anpassen
5. Error templates definieren
6. Testing mit allen Szenarien
```

---

**© 2025 - UnifiedFloppyTool v3.1 - Validation System Edition**

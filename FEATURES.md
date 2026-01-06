# UFT (UnifiedFloppyTool) v3.4.5 - Feature-Übersicht

## 🎯 Hauptfunktionen

### 📀 Disk-Imaging & Preservation
- **Flux-Level Imaging** - Magnetische Rohdaten erfassen
- **Multi-Revolution Capture** - Mehrere Umdrehungen für bessere Qualität
- **Bit-Level Preservation** - Jedes Bit wird exakt erhalten
- **Weak Bit Detection** - Schwache/instabile Bits erkennen
- **Timing Preservation** - Original-Timing beibehalten

### 🔄 Format-Konvertierung
- Zwischen 200+ Formaten konvertieren
- Automatische Format-Erkennung
- Batch-Konvertierung
- Verlustfreie Umwandlung wo möglich

---

## 💾 Unterstützte Plattformen & Formate

### Commodore
| Format | Beschreibung |
|--------|--------------|
| D64 | C64 1541 Disk Image |
| G64 | C64 GCR Raw Format |
| D71 | C128 1571 Disk Image |
| D81 | C64/128 1581 3.5" Disk |
| NIB | Nibble-Level Format |
| P64 | Preservation Format |
| T64 | Tape Image |

### Amiga
| Format | Beschreibung |
|--------|--------------|
| ADF | Amiga Disk File |
| ADZ | Compressed ADF |
| DMS | Disk Masher System |
| IPF | Interchangeable Preservation Format |
| FDI | Formatted Disk Image |
| HDF | Hard Disk File |

### Atari (8-bit & ST)
| Format | Beschreibung |
|--------|--------------|
| ATR | Atari 8-bit Disk |
| ATX | Atari Protected Format |
| XFD | Atari XFormer Disk |
| STX | Atari ST Protected |
| MSA | Atari ST Archive |
| ST | Atari ST Sector Dump |

### Apple
| Format | Beschreibung |
|--------|--------------|
| DSK | Apple II Sector |
| DO/PO | DOS/ProDOS Order |
| NIB | Apple Nibble Format |
| WOZ | Modern Preservation |
| 2IMG | Universal Disk Image |
| DC42 | DiskCopy 4.2 |

### IBM PC / DOS
| Format | Beschreibung |
|--------|--------------|
| IMG | Raw Sector Image |
| IMA | Floppy Image |
| IMD | ImageDisk |
| TD0 | Teledisk |
| DMK | TRS-80/CoCo Format |
| CQM | CopyQM Compressed |

### Flux-Level Formate
| Format | Beschreibung |
|--------|--------------|
| SCP | SuperCard Pro |
| KF | KryoFlux Stream |
| HFE | HxC Floppy Emulator |
| MFI | MAME Floppy Image |
| A2R | Applesauce Raw |
| FLUX | Generic Flux |

### Weitere Plattformen
- **Amstrad CPC** - DSK, EDSK
- **BBC Micro** - SSD, DSD, ADF
- **MSX** - DSK
- **TRS-80** - DMK, JV1, JV3
- **Acorn** - ADF, ADL, ADM
- **CP/M** - IMG, DSK
- **Thomson** - FD, SAP
- **Sharp** - D88, 2D
- **NEC PC-98** - D88, FDI
- **FM Towns** - BIN, CUE
- **Victor 9000** - DSK
- **Northstar** - NSI

---

## 🔧 Hardware-Unterstützung

| Gerät | Status | Funktionen |
|-------|--------|------------|
| **Greaseweazle** | ✅ Voll | Lesen, Schreiben, Flux-Capture |
| **KryoFlux** | ✅ Voll | Lesen, Streaming |
| **FluxEngine** | ✅ Voll | Lesen, Schreiben |
| **SuperCard Pro** | ✅ Voll | Lesen, Multi-Rev |
| **FC5025** | ✅ Voll | 5.25" Lesen |
| **Applesauce** | ✅ Voll | Apple II Imaging |
| **Catweasel** | ⚠️ Legacy | Lesen |
| **OpenCBM** | ✅ Voll | C64 1541/1571 |

---

## 🔍 Analyse-Funktionen

### Copy-Protection Erkennung
- **Commodore 64**: RapidLok, V-Max!, Vorpal, XEMAG
- **Amiga**: Rob Northen Copylock, Trace Vector
- **Atari ST**: Copylock, Macrodos, Rob Northen
- **Apple II**: Spiradisk, E7-Protection, Locksmith
- **PC**: Safedisc, SecuROM, LaserLock

### Forensik-Tools
- Datei-Signatur-Erkennung
- BAM/FAT-Rekonstruktion
- Gelöschte Dateien wiederherstellen
- Disk-Fehler-Analyse
- Audit-Trail erstellen

### OCR (Optical Character Recognition)
- Disk-Label scannen
- Text aus Bildern extrahieren
- Automatische Katalogisierung

---

## 📁 Dateisystem-Support

| Dateisystem | Lesen | Schreiben | Reparieren |
|-------------|-------|-----------|------------|
| Commodore DOS | ✅ | ✅ | ✅ |
| AmigaDOS (OFS/FFS) | ✅ | ✅ | ✅ |
| Apple DOS 3.3 | ✅ | ✅ | ⚠️ |
| ProDOS | ✅ | ✅ | ✅ |
| FAT12/16 | ✅ | ✅ | ✅ |
| CP/M | ✅ | ✅ | ⚠️ |
| Acorn DFS/ADFS | ✅ | ⚠️ | ⚠️ |
| BBC Micro DFS | ✅ | ✅ | ⚠️ |
| HFS (Classic Mac) | ✅ | ⚠️ | ❌ |

---

## 🖥️ Benutzeroberfläche

### Qt6 GUI
- **Catalog Tab** - Datei-Browser für Disk-Images
- **Format Tab** - Format-Erkennung und Info
- **Hardware Tab** - Geräte-Management
- **Nibble Tab** - Low-Level Disk-Analyse
- **Protection Tab** - Copy-Protection Analyse
- **Forensic Tab** - Forensik-Tools
- **Status Tab** - Fortschritt und Logs

### CLI (Command Line)
```bash
# Beispiele
uft detect image.d64           # Format erkennen
uft convert in.g64 out.d64     # Konvertieren
uft analyze disk.scp           # Analysieren
uft read --device gw0 out.scp  # Hardware lesen
uft catalog image.adf          # Dateien auflisten
```

---

## 🛡️ Sicherheits-Features (v3.4.5)

- **Stack Protection** - Gegen Buffer Overflows
- **FORTIFY_SOURCE** - Zusätzliche Bounds-Checks
- **Sichere String-Funktionen** - Kein strcpy/sprintf
- **Command Injection Schutz** - Shell-Escape
- **Input Validation** - Alle Eingaben geprüft

---

## 🏗️ Build & Deployment

### Unterstützte Betriebssysteme
| OS | Architektur | Paketformat |
|----|-------------|-------------|
| Linux | x86_64, ARM64 | .tar.gz, .deb |
| macOS | x86_64, ARM64 | .tar.gz |
| Windows | x64 | .zip |

### GitHub Actions CI/CD
- Automatische Builds auf Push/PR
- Automatische Releases bei Tags
- Cross-Platform Testing
- Security Hardening aktiviert

---

## 📊 Statistiken

| Kategorie | Anzahl |
|-----------|--------|
| Format-Parser | 667 |
| Unterstützte Formate | 200+ |
| Header-Dateien | 408 |
| C/C++ Source-Dateien | 1489 |
| Plattformen | 25+ |
| Copy-Protection Typen | 50+ |

---

## 🎯 Anwendungsfälle

1. **Archivierung** - Historische Disketten sichern
2. **Emulation** - Images für Emulatoren erstellen
3. **Forensik** - Gelöschte Daten wiederherstellen
4. **Konvertierung** - Zwischen Formaten wechseln
5. **Analyse** - Copy-Protection untersuchen
6. **Preservation** - Bit-perfekte Kopien erstellen

---

*UFT - "Bei uns geht kein Bit verloren"*

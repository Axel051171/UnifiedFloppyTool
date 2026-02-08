# MIG-Flash Architecture & Protocol v2.0

## ⚠️ WICHTIGE ERKENNTNIS

**MIG-Flash ist KEIN serielles Gerät!**

MIG-Flash ist ein **USB Mass Storage Device** das sich wie ein normaler USB-Stick/SD-Karte verhält. 
Die Kommunikation erfolgt über **Block I/O** (Sektor lesen/schreiben), nicht über ein serielles Protokoll.

---

## 1. Hardware Architektur

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           MIG-FLASH HARDWARE                                 │
│                                                                              │
│  ┌──────────────────┐    ┌──────────────────┐    ┌──────────────────────┐  │
│  │                  │    │                  │    │                      │  │
│  │  Nintendo Switch │───▶│    ESP32-S2      │───▶│  Genesys GL3227      │  │
│  │  Gamecard Slot   │    │    (MCU)         │    │  USB Storage IC      │  │
│  │                  │    │                  │    │                      │  │
│  │  - 60-pin        │    │  - Cartridge     │    │  - USB 2.0 HS        │  │
│  │  - Gamecard      │    │    Protocol      │    │  - Mass Storage      │  │
│  │    Interface     │    │  - XCI Handling  │    │  - Block Device      │  │
│  │                  │    │  - Auth          │    │                      │  │
│  └──────────────────┘    └──────────────────┘    └──────────┬───────────┘  │
│                                                              │              │
└──────────────────────────────────────────────────────────────┼──────────────┘
                                                               │
                                                      USB Mass Storage
                                                       (Bulk Transfer)
                                                               │
                                                               ▼
                                                    ┌─────────────────────┐
                                                    │      HOST PC        │
                                                    │                     │
                                                    │  - Sector Read      │
                                                    │  - Sector Write     │
                                                    │  - Block I/O        │
                                                    └─────────────────────┘
```

---

## 2. USB Device Identifikation

| Parameter | Wert | Beschreibung |
|-----------|------|--------------|
| **VID** | `0x05E3` | Genesys Logic, Inc. |
| **PID** | `0x0751` | microSD Card Reader |
| **Class** | Mass Storage | Bulk-Only Transport |
| **Interface** | 0 | Mass Storage Class |
| **Endpoints** | EP1 IN, EP2 OUT | Bulk Transfer |
| **Max Packet** | 512 bytes | USB 2.0 High Speed |

```
Bus 000 Device 009: ID 05e3:0751 Genesys Logic, Inc. microSD Card Reader
  bInterfaceClass        8 Mass Storage
  bInterfaceSubClass     6 SCSI
  bInterfaceProtocol    80 Bulk-Only
```

---

## 3. Speicher-Layout (Memory Map)

Das MIG-Flash Device präsentiert sich als Block-Device mit folgender Struktur:

```
Offset          │ Größe     │ Beschreibung
────────────────┼───────────┼─────────────────────────────────────────
0x00000000      │ 512 B     │ MBR (Master Boot Record)
0x00000200      │ 512 B     │ GPT Header
0x00000400      │ 512 B     │ GPT Partition Entry ◄── Validierung
                │           │ GUID: A2A0D0EB-E5B9-3344-87C0-68B6B72699C7
                │           │ (Microsoft Basic Data)
────────────────┼───────────┼─────────────────────────────────────────
0x00100000      │ 512 B     │ XCI Header (vermutlich)
(1 MB)          │           │ - Signatur, Magic "HEAD"
                │           │ - Cart Size, Flags
────────────────┼───────────┼─────────────────────────────────────────
0x00100200      │ 512 B     │ XCI Certificate (vermutlich)
────────────────┼───────────┼─────────────────────────────────────────
0x00200000      │ variabel  │ XCI Data Start (vermutlich)
(2 MB)          │           │ - HFS0 Partitions
                │           │ - Game Data
────────────────┼───────────┼─────────────────────────────────────────
0x20000000      │ variabel  │ Control Area (vermutlich)
(512 MB)        │           │ - Status Register
                │           │ - Command Register
────────────────┼───────────┼─────────────────────────────────────────
0x209A4000      │ 278,528 B │ FIRMWARE AREA ◄── Bestätigt!
(~549 MB)       │ (0x44000) │ - Version String (16 bytes)
                │           │ - Firmware Binary
                │           │ - 544 Sektoren × 512 bytes
────────────────┴───────────┴─────────────────────────────────────────
```

---

## 4. Zugriffsmethode

### 4.1 Windows

```python
import win32file
import win32con

# Physical Drive öffnen
handle = win32file.CreateFile(
    r"\\.\PhysicalDrive2",          # Device Path
    win32con.GENERIC_READ | win32con.GENERIC_WRITE,
    win32con.FILE_SHARE_READ | win32con.FILE_SHARE_WRITE,
    None,
    win32con.OPEN_EXISTING,
    win32con.FILE_ATTRIBUTE_NORMAL,
    None
)

# Seek zu Offset
win32file.SetFilePointer(handle, 0x209A4000, win32con.FILE_BEGIN)

# Sektor lesen
_, data = win32file.ReadFile(handle, 512, None)

# Sektor schreiben
win32file.WriteFile(handle, data, None)

win32file.CloseHandle(handle)
```

### 4.2 macOS

```python
import subprocess

# Disk unmounten
subprocess.run(["diskutil", "unmountDisk", "force", "/dev/disk4"])

# Raw Device öffnen
with open("/dev/rdisk4", "r+b") as f:
    f.seek(0x209A4000)
    data = f.read(512)
```

### 4.3 Linux

```python
# Device öffnen (benötigt root)
with open("/dev/sdb", "r+b") as f:
    f.seek(0x209A4000)
    data = f.read(512)
```

---

## 5. Firmware Update Prozess

Aus `migupdater.py` analysiert:

```python
OFFSET = 0x209A4000    # Firmware Start
SIZE   = 0x44000       # 278,528 bytes (544 Sektoren)

def check_firmware_version(drive):
    """Firmware Version auslesen"""
    drive.seek(0x400)
    gpt = drive.read(512)
    
    # GPT GUID validieren
    if gpt[0:16] != b'\xA2\xA0\xD0\xEB\xE5\xB9\x33\x44\x87\xC0\x68\xB6\xB7\x26\x99\xC7':
        raise Exception("Kein gültiges MIG Device!")
    
    # Firmware Version lesen
    drive.seek(OFFSET)
    blob = drive.read(SIZE)
    
    # Alle Sektoren müssen identisch sein!
    sector = blob[0:512]
    for i in range(0, len(blob), 512):
        if sector != blob[i:i+512]:
            raise Exception("Firmware Bereich korrupt!")
    
    # Version ist ASCII String am Anfang
    version = sector[0:16].decode("ascii").rstrip('\x00')
    return version

def update_firmware(drive, firmware_path):
    """Firmware Update"""
    with open(firmware_path, 'rb') as f:
        fw = f.read(SIZE)
    
    drive.seek(OFFSET)
    
    # In 512-byte Blöcken schreiben
    while len(fw) > 0:
        block = fw[0:512]
        fw = fw[512:]
        drive.write(block)
```

---

## 6. XCI Dump Prozess (Vermutete Implementierung)

```python
class MIGFlash:
    # Memory Map Konstanten
    XCI_HEADER_OFFSET = 0x00100000
    XCI_DATA_OFFSET   = 0x00200000
    
    def read_xci_header(self):
        """XCI Header lesen (512 bytes)"""
        self.device.seek(self.XCI_HEADER_OFFSET)
        data = self.device.read(512)
        
        # Validieren
        if data[0x100:0x104] != b'HEAD':
            raise Exception("Keine gültige XCI Cartridge!")
        
        return XCIHeader.from_bytes(data)
    
    def dump_xci(self, filename, total_size):
        """XCI komplett dumpen"""
        CHUNK_SIZE = 1024 * 1024  # 1 MB
        
        with open(filename, 'wb') as f:
            offset = 0
            
            while offset < total_size:
                self.device.seek(self.XCI_DATA_OFFSET + offset)
                
                remaining = total_size - offset
                read_size = min(CHUNK_SIZE, remaining)
                
                data = self.device.read(read_size)
                f.write(data)
                
                offset += len(data)
                print(f"Progress: {offset / total_size * 100:.1f}%")
```

---

## 7. XCI Header Struktur

```
Offset  │ Größe │ Beschreibung
────────┼───────┼────────────────────────────────────
0x000   │ 256   │ RSA-2048 Signatur
0x100   │ 4     │ Magic "HEAD"
0x104   │ 4     │ Secure Area Start (Sektoren)
0x108   │ 4     │ Backup Area Start (Sektoren)
0x10C   │ 1     │ Title Key Dec Index
0x10D   │ 1     │ Gamecard Size (siehe unten)
0x10E   │ 1     │ Gamecard Header Version
0x10F   │ 1     │ Gamecard Flags
0x110   │ 8     │ Package ID
0x118   │ 8     │ Valid Data End (Sektoren)
0x120   │ 16    │ IV für Verschlüsselung
0x130   │ 8     │ HFS0 Partition Offset
0x138   │ 8     │ HFS0 Header Size
0x140   │ 32    │ SHA-256 Hash
0x160   │ 32    │ Initial Data Hash
0x180   │ 1     │ Secure Mode Flag
0x181   │ 1     │ Title Key Flag
0x182   │ 1     │ Key Flag
0x183   │ 4     │ Normal Area End
```

**Gamecard Size Codes:**
| Code | Größe |
|------|-------|
| 0xFA | 1 GB |
| 0xF8 | 2 GB |
| 0xF0 | 4 GB |
| 0xE0 | 8 GB |
| 0xE1 | 16 GB |
| 0xE2 | 32 GB |

---

## 8. GUI Integration

### Event-basierte Architektur

```python
from mig_gui_interface import MIGController, MIGEvent

class MyGUI:
    def __init__(self):
        self.controller = MIGController()
        self.controller.set_event_callback(self.on_mig_event)
    
    def on_mig_event(self, event):
        # WICHTIG: Vom Worker-Thread aufgerufen!
        # GUI-Update auf Main-Thread schedulen
        self.root.after(0, lambda: self.handle_event(event))
    
    def handle_event(self, event):
        if event.event == MIGEvent.DEVICE_CONNECTED:
            self.status_label.text = f"Verbunden: {self.controller.firmware_version}"
        
        elif event.event == MIGEvent.CART_INSERTED:
            self.enable_dump_button()
        
        elif event.event == MIGEvent.DUMP_PROGRESS:
            self.progress_bar.value = event.progress
            self.speed_label.text = f"{event.speed_kbps} KB/s"
        
        elif event.event == MIGEvent.DUMP_COMPLETE:
            self.show_success("Dump fertig!")
```

---

## 9. Wichtige Hinweise

### 9.1 Administrator/Root Rechte

Block-Device Zugriff erfordert erhöhte Rechte:
- **Windows**: Als Administrator ausführen
- **macOS**: Mit `sudo` oder Disk Arbitration Framework
- **Linux**: Mit `sudo` oder udev Regeln

### 9.2 Volume Lock (Windows)

Vor dem Schreiben muss das Volume gelockt werden:
```python
win32file.DeviceIoControl(handle, FSCTL_LOCK_VOLUME, None, 0, None)
```

### 9.3 Unmount (macOS)

Vor Zugriff muss das Volume unmounted werden:
```bash
diskutil unmountDisk force /dev/diskN
```

### 9.4 Sektor-Alignment

Alle Reads/Writes müssen auf 512-byte Grenzen aligned sein!

---

## 10. Gelieferte Dateien

| Datei | Zeilen | Beschreibung |
|-------|--------|--------------|
| `mig_flash.py` | ~650 | Haupt-Library mit Block I/O |
| `mig_gui_interface.py` | ~400 | GUI Controller mit Events |
| `MIG_ARCHITECTURE.md` | ~350 | Diese Dokumentation |

---

## Changelog

### v2.0.0 (2026-01-20)
- **BREAKING**: Komplette Neuimplementierung
- Korrektes Block I/O statt seriellem Protokoll
- Basiert auf migupdater.py Analyse
- GUI-freundliche Event-Architektur
- Plattform-übergreifende Unterstützung

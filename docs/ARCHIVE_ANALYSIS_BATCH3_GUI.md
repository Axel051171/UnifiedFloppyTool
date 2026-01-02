# Archive Analysis Batch 3 - GUI Parameter Integration

## Übersicht

**Analysierte Archive:** 5
**Gesamtdateien:** 265
**Neue Header erstellt:** 4
**Fokus:** GUI Parameter Abgleich mit offiziellem Greaseweazle

| Archiv | Dateien | Sprache | Fokus |
|--------|---------|---------|-------|
| greaseweazle-master | 146 | Python | Offizielles Referenz-Tool |
| DataRecovery-1-master | 101 | C++ | FAT/NTFS Recovery |
| FAT12Extractor-master | 3 | C | FAT12 Filesystem |
| flopp-to-winch-master | 6 | C | Norsk Data Backup |
| sgrecover-master | 9 | D | SCSI/SG Recovery |

---

## 1. Greaseweazle (Offizielles Tool)

**Autor:** Keir Fraser
**Lizenz:** Public Domain

### Extrahierte Parameter

#### PLL Parameter (track.py)

| Parameter | Aggressive | Conservative | Beschreibung |
|-----------|------------|--------------|--------------|
| period_adj_pct | 5% | 1% | Frequenzkorrektur |
| phase_adj_pct | 60% | 10% | Phasenkorrektur |
| lowpass_thresh | - | - | Rauschfilter (optional) |

**UFT V1 → V2 Mapping:**
- `freq_adjust` → `period_adj` (Umbenennung für GW-Kompatibilität)
- `phase_adjust` → `phase_adj` (Wert angepasst: 65% → 60%)
- NEU: `lowpass_thresh` (µs)

#### Precompensation (track.py)

```python
# MFM Patterns
10100 -> shift bits 2,3
00101 -> shift bits 2,3 (inverse)

# FM/GCR Patterns (adjacent 1s)
110 -> shift bits 1,2
011 -> shift bits 1,2 (inverse)
```

**Default:** 140ns für MFM

#### Gap Parameter (ibm.py)

| Gap | FM | MFM | Beschreibung |
|-----|-----|-----|--------------|
| Gap1 | 26 | 50 | Post-IAM |
| Gap2 | 11 | 22 | Post-IDAM |
| Gap3[128] | 27 | 32 | Post-DAM (128 Bytes) |
| Gap3[256] | 42 | 54 | Post-DAM (256 Bytes) |
| Gap3[512] | 58 | 84 | Post-DAM (512 Bytes) |
| Gap3[1024] | 138 | 116 | Post-DAM (1024 Bytes) |
| Gap4a | 40 | 80 | Post-Index |

#### Drive Delays (usb.py)

| Parameter | Default | Beschreibung |
|-----------|---------|--------------|
| step_delay | 3000µs | Schrittmotor-Verzögerung |
| settle_delay | 15ms | Kopf-Einschwingzeit |
| motor_delay | 500ms | Motor-Anlaufzeit |
| auto_off | 10s | Auto-Abschaltung |

#### USB Protokoll (usb.py)

**Commands:**
```c
GetInfo=0, Update=1, Seek=2, Head=3, SetParams=4, GetParams=5,
Motor=6, ReadFlux=7, WriteFlux=8, GetFluxStatus=9, GetIndexTimes=10,
SwitchFwMode=11, Select=12, Deselect=13, SetBusType=14, SetPin=15,
Reset=16, EraseFlux=17, SourceBytes=18, SinkBytes=19, GetPin=20,
TestMode=21, NoClickStep=22
```

**Responses:**
```c
Okay=0, BadCommand=1, NoIndex=2, NoTrk0=3, FluxOverflow=4,
FluxUnderflow=5, Wrprot=6, NoUnit=7, NoBus=8, BadUnit=9,
BadPin=10, BadCylinder=11, OutOfSRAM=12, OutOfFlash=13
```

#### SCP Format (image/scp.py)

- **Sample Frequency:** 40 MHz
- **Max Tracks:** 168
- **Flux Encoding:** 16-bit Big-Endian, 0x0000 = +65536

**Disk Types:**
- C64: 0x00
- Amiga: 0x04, 0x08 (HD)
- Atari ST: 0x14, 0x15
- Apple: 0x20-0x26
- IBM PC: 0x30-0x33
- TRS-80: 0x40-0x43

#### Format-Presets (codec/)

| Format | Mode | Cyls | Heads | Secs | Size | RPM | Rate |
|--------|------|------|-------|------|------|-----|------|
| PC 720K DD | MFM | 80 | 2 | 9 | 512 | 300 | 250k |
| PC 1.44M HD | MFM | 80 | 2 | 18 | 512 | 300 | 500k |
| Amiga DD | MFM | 80 | 2 | 11 | 512 | 300 | 250k |
| C64 1541 | GCR | 35 | 1 | 21 | 256 | 300 | - |
| Apple II | GCR | 35 | 1 | 16 | 256 | 300 | - |
| Mac 800K | GCR | 80 | 2 | 12 | 512 | 394 | - |

---

## 2. FAT12Extractor

**Extrahierte Algorithmen:**

### FAT12 Entry Decoding

```c
// From FAT12Extractor/msdosextr.c
unsigned short get_next_block(unsigned short block, ...) {
    pos = bb.n_bytes_per_block + (long)(block)*1.5;
    fread(&next_block, sizeof(unsigned char), 2, *f);
    
    if (block % 2 == 0) {
        next_block &= 0x000FFF;  // Even: low 12 bits
    } else {
        next_block >>= 4;         // Odd: high 12 bits
        next_block &= 0x000FFF;
    }
    return next_block;
}
```

### Directory Entry Validation

```c
bool is_valid_entry(struct root_dir_entry r) {
    if (r.starting_cluster == 0) return false;
    if (sign == 0x00 || sign == 0x20) return false;  // Empty
    if (sign == 0xE5 || sign == 0x05) return false;  // Deleted
    if ((r.attr & 0x08) == 0x08) return false;       // Volume label
    return true;
}
```

---

## 3. DataRecovery

**Extrahierte Strukturen:**

### FAT32 BPB (fatmbr.h)

```c
typedef struct {
    UCHAR Jump[3];
    UCHAR Format[8];
    USHORT BytesPerSector;
    UCHAR SectorsPerCluster;
    USHORT ResverdSector;
    UCHAR NumberOfFat;
    USHORT RootEntries;
    // ... etc
    ULONG SectorPerFAT;
    ULONG RootDir1stCluster;
    USHORT FSInfoSector;
} fat32mbr;
```

### NTFS Structures (ntfs.h)

```c
typedef enum {
    AttributeStandardInformation = 0x10,
    AttributeAttributeList = 0x20,
    AttributeFileName = 0x30,
    AttributeData = 0x80,
    AttributeIndexRoot = 0x90,
    // ...
} ATTRIBUTE_TYPE;
```

---

## 4. flopp-to-winch

**Norsk Data SINTRAN-III Backup Recovery**

- Big-Endian Format (68k-basiert)
- 2048 Byte Pages
- Volume Header mit Page-Index

### Endianness Handling

```c
#if BYTE_ORDER==LITTLE_ENDIAN
# define be32_to_cpu(x) swap4((x))
# define be16_to_cpu(x) swap2((x))
#else
# define be32_to_cpu(x) (x)
# define be16_to_cpu(x) (x)
#endif
```

---

## 5. sgrecover

**SCSI Generic Device Recovery (D-Sprache)**

- Linux SCSI Generic Interface
- Kapazitätsabfrage
- Formatierbare Kapazitäten

---

## Erstellte Header

### 1. uft_gw_official_params.h (290 Zeilen)

Offizielle Greaseweazle Parameter:
- PLL Presets (Aggressive, Conservative)
- Precompensation Types & Patterns
- Gap Parameters (FM/MFM)
- Drive Delays
- Format Presets
- GUI Mapping Structure

### 2. uft_gui_params_v2.h (460 Zeilen)

Erweiterte GUI Parameter V2:
- Migration V1 → V2
- Greaseweazle-kompatible Namensgebung
- Vollständige Widget-Konfiguration
- Format-Preset-Anwendung

### 3. uft_fat_filesystem.h (500 Zeilen)

FAT12/16/32 Support:
- BPB Strukturen
- FAT Entry Decoding
- Directory Entry Handling
- Date/Time Decoding
- Layout Calculation
- Recovery Parameters

### 4. uft_scp_format.h (380 Zeilen)

SuperCard Pro Format:
- Header Structure
- Track Data Header
- Flux Encoding/Decoding
- Extension Blocks (WRSP)
- Timing Conversion
- TLUT Handling

---

## GUI Parameter Abgleich

### Änderungen UFT V1 → V2

| Kategorie | V1 Parameter | V2 Parameter | Änderung |
|-----------|--------------|--------------|----------|
| PLL | freq_adjust | period_adj | Umbenannt |
| PLL | phase_adjust 65% | phase_adj 60% | Wert angepasst |
| PLL | - | lowpass_thresh | Neu hinzugefügt |
| Precomp | - | type, ns, enabled | Neu hinzugefügt |
| Gaps | - | gap1, gap2, gap3, gap4a | Neu hinzugefügt |
| Interleave | - | interleave, cskew, hskew | Neu hinzugefügt |
| Timing | - | step, settle, motor, auto_off | Neu hinzugefügt |
| R/W | - | revs, verify_revs, retries | Neu hinzugefügt |

### Qt Widget Mapping

```
// PLL Tab
QSpinBox: period_adj (0-20%, step 1)
QSpinBox: phase_adj (0-100%, step 5)
QDoubleSpinBox: lowpass_thresh (0-10µs, step 0.5)
QComboBox: preset (Aggressive, Conservative, Custom, WD1772, MAME)

// Precomp Tab
QComboBox: type (MFM, FM, GCR)
QSpinBox: ns (0-500, step 10)
QCheckBox: enabled

// Gaps Tab
QSpinBox: gap1 (0-100)
QSpinBox: gap2 (0-50)
QSpinBox: gap3 (0-200)
QSpinBox: gap4a (0-150)

// Drive Tab
QSpinBox: step_delay (1000-20000µs)
QSpinBox: settle_delay (5-50ms)
QSpinBox: motor_delay (100-2000ms)
QSpinBox: auto_off (0-60s)

// Format Tab
QComboBox: preset (17 Format-Presets)
QSpinBox: cyls, heads, secs
QComboBox: encoding (FM, MFM, GCR, etc.)
```

---

## Integration Empfehlungen

### Priorität 1: PLL Parameter Migration

1. Bestehende `uft_gui_pll_params_t` auf `uft_pll_params_v2_t` migrieren
2. Qt Bindings für neue Parameter erstellen
3. Preset-System implementieren

### Priorität 2: Gap/Precomp Integration

1. Gap-Parameter in Format-Presets einbinden
2. Automatische Gap-Berechnung bei Format-Wechsel
3. Precompensation mit Encoding-Type verknüpfen

### Priorität 3: SCP Support

1. SCP Import/Export implementieren
2. Multi-Revolution Support
3. Write-Splice Extension (WRSP)

### Priorität 4: FAT Recovery

1. FAT12/16/32 Auto-Detection
2. Cluster-Chain-Following
3. Deleted File Recovery

---

## Statistik

**Batch 3 Gesamt:**
- 5 Archive analysiert
- 265 Dateien untersucht
- 4 neue Header erstellt (1630+ Zeilen)
- 17 Format-Presets dokumentiert
- 23 USB-Commands dokumentiert
- 32+ Disk-Types katalogisiert

**Projekt-Gesamt (Batch 1-3):**
- 14 Archive analysiert
- 1331+ Dateien untersucht
- 66+ Header erstellt
- 25,479+ Zeilen Code

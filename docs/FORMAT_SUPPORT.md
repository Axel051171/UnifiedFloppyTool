# UnifiedFloppyTool - Format Support Matrix

## Bereits Implementiert ✓

| Format | Ext | System | Varianten | R/W | Beschreibung |
|--------|-----|--------|-----------|-----|--------------|
| D64 | .d64 | C64 | 6 | R/W | 35/40/42 Tracks ± Error |
| G64 | .g64 | C64 | 1 | R/W | GCR Raw mit Copy Protection |
| ADF | .adf | Amiga | 3 | R/W | DD/HD/Extended |
| IMG | .img,.ima | PC | 10+ | R/W | 160KB-2.88MB |
| SCP | .scp | Flux | 1 | R/W | SuperCard Pro Flux |
| HFE | .hfe | Flux | 2 | R/W | HxC Floppy Emulator |

## Neu Implementiert (Sprint 4)

| Format | Ext | System | Varianten | R/W | Status |
|--------|-----|--------|-----------|-----|--------|
| D71 | .d71 | C128 | 2 | R/W | ✓ Plugin erstellt |
| D81 | .d81 | C128 | 2 | R/W | ✓ Plugin erstellt |
| MSA | .msa | Atari ST | 1 | R/W | ✓ RLE-Kompression |
| IMD | .imd | 8"/PC | 1 | R/W | ✓ ImageDisk |
| DSK (CPC) | .dsk | CPC/Spectrum | 2 | R/W | ✓ Standard/Extended |

## Geplant (Priorität 1)

| Format | Ext | System | Beschreibung |
|--------|-----|--------|--------------|
| IPF | .ipf | Multi | CAPS/SPS Preservation |
| TD0 | .td0 | PC | Teledisk Archiv |
| ST | .st | Atari ST | Raw Sektor-Dump |
| NIB | .nib | Apple II | Nibble Format |
| WOZ | .woz | Apple II | Modern Flux-ähnlich |

## Geplant (Priorität 2)

| Format | Ext | System | Beschreibung |
|--------|-----|--------|--------------|
| D80 | .d80 | C64/PET | Commodore 8050 |
| D82 | .d82 | C64/PET | Commodore 8250 |
| G71 | .g71 | C128 | GCR Double-Sided |
| A2R | .a2r | Apple II | Applesauce Flux |
| FDI | .fdi | Multi | UKV Soft Format |
| DMK | .dmk | TRS-80 | David Keil Format |

## Geplant (Priorität 3 - Nischen)

| Format | Ext | System | Beschreibung |
|--------|-----|--------|--------------|
| ATR | .atr | Atari 8-bit | 90KB-1MB |
| TRD | .trd | Spectrum | TR-DOS Beta |
| SSD/DSD | .ssd,.dsd | BBC Micro | Acorn DFS |
| D88 | .d88 | PC-88/98 | Japanese |
| SAD | .sad | Sam Coupé | 800KB |

## Format Detection Strategie

### 1. Magic Bytes (Höchste Priorität)
```
SCP: "SCP" (3 Bytes)
HFE: "HXCPICFE" (8 Bytes)
G64: "GCR-1541" (8 Bytes)
MSA: 0x0E 0x0F (2 Bytes)
IMD: "IMD " (4 Bytes)
DSK: "MV - CPC" oder "EXTENDED" 
IPF: "CAPS" (4 Bytes)
TD0: "TD"/"td" (2 Bytes)
```

### 2. Dateigröße (Mittlere Priorität)
```
D64: 174848, 175531, 196608, 197376, 205312, 206114
D71: 349696, 351062
D81: 819200, 822400
ADF: 901120, 1802240
IMG: 163840, 184320, 327680, 368640, 737280, 1228800, 1474560, 2949120
```

### 3. Struktur-Analyse (Niedrigere Priorität)
- BAM @ Track 18 → D64/D71
- FAT Bootsector → IMG/ST
- Amiga Boot Block → ADF

## Hinzufügen neuer Formate

```c
// 1. Plugin-Datei erstellen
// src/formats/xxx/uft_xxx.c

// 2. Probe-Funktion
static bool xxx_probe(const uint8_t* data, size_t size, 
                      size_t file_size, int* confidence);

// 3. Open/Close
static uft_error_t xxx_open(uft_disk_t* disk, const char* path, bool read_only);
static void xxx_close(uft_disk_t* disk);

// 4. Plugin registrieren
static const uft_format_plugin_t xxx_plugin = {
    .name = "XXX",
    .description = "...",
    .extensions = "xxx",
    .format = UFT_FORMAT_XXX,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE,
    .probe = xxx_probe,
    .open = xxx_open,
    .close = xxx_close,
    // ...
};

// 5. In uft_format_plugin.c registrieren
uft_register_xxx();
```

# UFT Tool Capability Matrix

## Übersicht: Welches Tool erwartet welche Daten?

### Daten-Schichten (Data Layers)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          DATA LAYER HIERARCHY                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   LAYER 0: FLUX (Raw Timing)                                                │
│   ─────────────────────────────────────────────────────────────             │
│   • Absolute höchste Treue                                                  │
│   • Timing jedes Flux-Übergangs in Nanosekunden                            │
│   • Erhält Kopierschutz, Weak Bits, Timing-Variationen                     │
│   • Formate: SCP, Kryoflux raw, A2R                                        │
│                           │                                                 │
│                           ▼ (Decode: PLL, Bit Detection)                    │
│                                                                             │
│   LAYER 1: BITSTREAM (Encoded)                                              │
│   ─────────────────────────────────────────────────────────────             │
│   • Bit-genaue Darstellung der Disk                                        │
│   • MFM/FM/GCR encoded                                                     │
│   • Erhält Sync-Marks, Inter-Sector Gaps                                   │
│   • Formate: HFE, G64, WOZ, NIB                                            │
│                           │                                                 │
│                           ▼ (Decode: Sync finden, Header/Data parsen)       │
│                                                                             │
│   LAYER 2: SECTOR (Raw Data)                                                │
│   ─────────────────────────────────────────────────────────────             │
│   • Nur die eigentlichen Nutzdaten                                         │
│   • Kein Encoding, keine Timing-Info                                       │
│   • Formate: D64, ADF, IMG, DSK, IMD                                       │
│                           │                                                 │
│                           ▼ (Parse: Filesystem-Strukturen)                  │
│                                                                             │
│   LAYER 3: FILESYSTEM (Files/Dirs)                                          │
│   ─────────────────────────────────────────────────────────────             │
│   • Datei- und Verzeichnisebene                                            │
│   • AmigaDOS, CBM DOS, FAT12/16                                            │
│   • Tools: unadf, mtools, c1541                                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Tool Capability Matrix

### Hardware-Tools (Disk → File)

| Tool | Input | Output Flux | Output Bit | Output Sector | Hardware |
|------|-------|-------------|------------|---------------|----------|
| **Greaseweazle** | Hardware | SCP, HFE, Kryoflux | HFE | IMG | Greaseweazle F7/F7+ |
| **FluxEngine** | Hardware | SCP, Kryoflux | HFE | IMG, ADF, D64 | FluxEngine, Greaseweazle |
| **Kryoflux DTC** | Hardware | Kryoflux, SCP | - | IMG, ADF, D64, G64 | Kryoflux |
| **nibtools** | Hardware (XUM1541) | - | G64, NIB | D64 | XUM1541, ZoomFloppy |

### Konverter-Tools (File → File)

| Tool | Input Flux | Input Bit | Input Sector | Output Flux | Output Bit | Output Sector |
|------|------------|-----------|--------------|-------------|------------|---------------|
| **HxCFE** | SCP, Kryoflux, A2R | HFE, G64, WOZ, NIB | D64, ADF, IMG, STX, IPF, IMD, TD0 | - | HFE, G64 | D64, ADF, IMG |
| **disk-analyse** | SCP, Kryoflux | HFE, G64 | ADF, IMG | SCP | - | ADF, IMG |
| **nibconv** | - | G64, NIB | D64 | - | G64, NIB | D64 |

### Filesystem-Tools (Sector-Level)

| Tool | Input | Operations | Output |
|------|-------|------------|--------|
| **unadf/ADFlib** | ADF | List, Extract, Create | ADF, Files |
| **mtools** | IMG (FAT) | List, Extract, Copy | IMG, Files |
| **c1541** | D64 | List, Extract, Create | D64, Files |

---

## Detaillierte Tool-Profile

### Greaseweazle (gw)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ GREASEWEAZLE                                                                │
├─────────────────────────────────────────────────────────────────────────────┤
│ Type: USB Floppy Adapter (Open Source)                                      │
│ Website: github.com/keirf/greaseweazle                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ INPUT:                                                                      │
│   • Hardware: Floppy Drive via USB                                          │
│   • Files: SCP, HFE, Kryoflux raw, IMG, ADF, D64                           │
│                                                                             │
│ OUTPUT:                                                                     │
│   • Hardware: Write to floppy                                               │
│   • Files: SCP, HFE, Kryoflux raw, IMG                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│ KEY FLAGS:                                                                  │
│   --revs N        Capture N revolutions (default: 3)                        │
│   --tracks A:B    Track range                                               │
│   --retries N     Read retry count                                          │
│   -d DEVICE       Serial device path                                        │
│   --densel        Force density select                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ LIMITATIONS:                                                                │
│   • Max 85 cylinders, 2 heads                                              │
│   • Needs USB connection                                                    │
│   • No direct IPF support                                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│ EXAMPLE:                                                                    │
│   gw read --revs=5 --tracks=0:79 -d /dev/ttyACM0 output.scp               │
│   gw write input.hfe -d /dev/ttyACM0                                       │
│   gw convert input.scp output.hfe                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### nibtools

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ NIBTOOLS                                                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│ Type: CBM-specific imaging tool                                             │
│ Website: c64preservation.com                                                │
├─────────────────────────────────────────────────────────────────────────────┤
│ INPUT:                                                                      │
│   • Hardware: 1541/1571 via XUM1541/ZoomFloppy                             │
│   • Files: G64, NIB, D64                                                   │
│   ⚠️ NICHT: SCP, HFE, Kryoflux, ADF                                        │
│                                                                             │
│ OUTPUT:                                                                     │
│   • Hardware: Write to 1541/1571                                           │
│   • Files: G64, NIB, D64                                                   │
├─────────────────────────────────────────────────────────────────────────────┤
│ KEY FLAGS:                                                                  │
│   -D N            Drive number (8-11)                                       │
│   -S N            Start track (1-42)                                        │
│   -E N            End track                                                 │
│   -h              Include half-tracks                                       │
│   -V              Verify after write                                        │
│   -P              Parallel transfer mode                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│ LIMITATIONS:                                                                │
│   • CBM-only (1541, 1571, 1581)                                            │
│   • Max 42 tracks                                                           │
│   • Single-sided (1541) or dual (1571)                                     │
│   • Needs XUM1541/ZoomFloppy hardware                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ ⚠️ PROBLEM: nibtools kann keine SCP-Flux-Daten direkt verarbeiten!         │
│                                                                             │
│ LÖSUNG: UFT I/O Abstraction Layer                                          │
│   SCP → [Decode] → G64 → nibtools                                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

### HxCFloppyEmulator (hxcfe)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ HxCFE                                                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│ Type: Universal Format Converter                                            │
│ Website: hxc2001.com                                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│ INPUT: (50+ Formate!)                                                       │
│   • Flux: SCP, Kryoflux, A2R                                               │
│   • Bitstream: HFE, G64, WOZ, NIB                                          │
│   • Sector: D64, ADF, IMG, DSK, STX, IPF, IMD, TD0                         │
│                                                                             │
│ OUTPUT:                                                                     │
│   • Bitstream: HFE (v1, v2, v3), G64                                       │
│   • Sector: D64, ADF, IMG                                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│ KEY FLAGS:                                                                  │
│   -finput:FILE    Input file                                               │
│   -foutput:FILE   Output file                                              │
│   -conv:FORMAT    Output format name                                       │
│   -infos          Show disk information                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│ BEST USE:                                                                   │
│   • Universal converter zwischen allen Formaten                            │
│   • IPF → ADF/HFE (wenn CAPS library verfügbar)                           │
│   • TD0 decompression                                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│ EXAMPLE:                                                                    │
│   hxcfe -finput:disk.scp -foutput:disk.hfe -conv:HXC_HFE                  │
│   hxcfe -finput:disk.ipf -foutput:disk.adf -conv:AMIGA_ADF                │
│   hxcfe -finput:disk.g64 -infos                                            │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## I/O Abstraction: Warum und Wo?

### Das Problem

```
nibtools erwartet G64/NIB ──────────┐
                                    │
Wir haben SCP (Flux) ───────────────┼───► INKOMPATIBEL!
                                    │
adftools erwartet ADF ──────────────┘
```

### Die Lösung: Unified I/O Layer

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         UFT I/O ABSTRACTION                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│   ANY SOURCE                    UNIFIED TRACK                    ANY SINK   │
│   ──────────                    ─────────────                    ────────   │
│                                                                             │
│   ┌─────────┐                   ┌─────────────┐                  ┌────────┐ │
│   │   SCP   │──┐                │ uft_track_t │                ┌─│nibtools│ │
│   └─────────┘  │                │             │                │ └────────┘ │
│   ┌─────────┐  │  ┌──────────┐  │ • flux      │  ┌──────────┐  │ ┌────────┐ │
│   │Kryoflux │──┼──│  SOURCE  │──│ • bitstream │──│  SINK    │──┼─│adftools│ │
│   └─────────┘  │  │  READER  │  │ • sectors   │  │  WRITER  │  │ └────────┘ │
│   ┌─────────┐  │  └──────────┘  │             │  └──────────┘  │ ┌────────┐ │
│   │   G64   │──┘                └──────┬──────┘                └─│ hxcfe  │ │
│   └─────────┘                          │                         └────────┘ │
│   ┌─────────┐                          │                                    │
│   │   D64   │──────────────────────────┘                                    │
│   └─────────┘                                                               │
│                                                                             │
│                        ┌─────────────────┐                                  │
│                        │ LAYER CONVERTER │                                  │
│                        ├─────────────────┤                                  │
│                        │ Flux→Bitstream  │ (PLL Decode)                     │
│                        │ Bitstream→Sector│ (Sync Detect)                    │
│                        │ Sector→Bitstream│ (GCR/MFM Encode) ⚠️ Synthetic    │
│                        │ Bitstream→Flux  │ (Timing Synth)   ⚠️ Synthetic    │
│                        └─────────────────┘                                  │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Beispiel: SCP → nibtools

```c
// Ohne I/O Abstraction:
// 1. SCP manuell lesen
// 2. Flux dekodieren
// 3. GCR-Bitstream extrahieren
// 4. G64 schreiben
// 5. nibtools aufrufen

// Mit I/O Abstraction:
uft_io_source_t* src;
uft_io_sink_t* sink;

uft_io_open_source("disk.scp", &src);           // Auto-detect format
uft_io_create_sink("temp.g64", UFT_FORMAT_G64,  // Create G64 output
                   cylinders, heads, &sink);
                   
uft_io_copy(src, sink, progress_callback, NULL); // Auto-convert!

// Jetzt kann nibtools das G64 verarbeiten
system("nibwrite -D8 temp.g64");
```

### Wo lohnt sich die Abstraktion am meisten?

| Use Case | Ohne Abstraktion | Mit Abstraktion |
|----------|------------------|-----------------|
| SCP → nibtools | SCP→(decode)→G64→nibtools | `uft_io_copy()` |
| Kryoflux → adftools | Kryoflux→(decode)→ADF→unadf | `uft_io_copy()` |
| D64 → Hardware Write | D64→(encode)→G64→gw write | `uft_io_copy()` |
| Format-Analyse | Je Format eigener Parser | `uft_io_source->read_track()` |

---

## Vollständige Tool-Vergleichstabelle

```
╔══════════════════════╦═══════╦═══════╦═══════╦═══════╦═══════╦═══════╦═══════╗
║                      ║ HARD- ║ FLUX  ║ BIT-  ║SECTOR ║ FILE- ║CONVERT║ANALYZE║
║ TOOL                 ║ WARE  ║       ║STREAM ║       ║SYSTEM ║       ║       ║
╠══════════════════════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╣
║ Greaseweazle (gw)    ║ R/W   ║ R/W   ║ R/W   ║ R/W   ║   -   ║  ✓    ║  ✓    ║
║ FluxEngine           ║ R/W   ║ R/W   ║ R/W   ║  W    ║   -   ║  ✓    ║  ✓    ║
║ Kryoflux DTC         ║  R    ║ R/W   ║   -   ║  W    ║   -   ║  ✓    ║  ✓    ║
║ nibtools             ║ R/W   ║   -   ║ R/W   ║ R/W   ║   -   ║  ✓    ║   -   ║
╠══════════════════════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╣
║ HxCFE                ║   -   ║  R    ║ R/W   ║ R/W   ║   -   ║  ✓    ║  ✓    ║
║ disk-analyse         ║   -   ║ R/W   ║  R    ║ R/W   ║   -   ║  ✓    ║  ✓    ║
╠══════════════════════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╣
║ unadf/ADFlib         ║   -   ║   -   ║   -   ║ R/W   ║ R/W   ║   -   ║  ✓    ║
║ mtools               ║   -   ║   -   ║   -   ║ R/W   ║ R/W   ║   -   ║  ✓    ║
║ c1541                ║   -   ║   -   ║   -   ║ R/W   ║ R/W   ║   -   ║  ✓    ║
╠══════════════════════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╬═══════╣
║ UFT (dieses Tool)    ║ via   ║ R/W   ║ R/W   ║ R/W   ║ (via) ║  ✓    ║  ✓    ║
║                      ║ tools ║       ║       ║       ║ tools ║       ║       ║
╚══════════════════════╩═══════╩═══════╩═══════╩═══════╩═══════╩═══════╩═══════╝

Legend: R=Read, W=Write, R/W=Both, -=Not supported
```

---

## Format-spezifische Einschränkungen

### Flux-Formate

| Format | Tool Support | Besonderheiten |
|--------|--------------|----------------|
| SCP | gw, fluxengine, hxcfe, disk-analyse | Standard, gut unterstützt |
| Kryoflux | gw, fluxengine, dtc, disk-analyse | Stream-Format, multi-file |
| A2R | hxcfe, fluxengine | Apple-spezifisch |

### Bitstream-Formate

| Format | Tool Support | Besonderheiten |
|--------|--------------|----------------|
| HFE | gw, fluxengine, hxcfe | Universell, HxC Emulator |
| G64 | nibtools, hxcfe, disk-analyse | CBM GCR, half-tracks |
| WOZ | hxcfe | Apple II, copy protection |
| NIB | nibtools, hxcfe | Apple II nibbles |

### Sector-Formate

| Format | Tool Support | Besonderheiten |
|--------|--------------|----------------|
| D64 | nibtools, hxcfe, c1541 | CBM, 35-42 tracks |
| ADF | unadf, disk-analyse, hxcfe | Amiga DD/HD |
| IMG | mtools, gw, hxcfe | PC FAT12/16 |
| IPF | hxcfe (+ CAPS) | Geschützte Amiga/Atari |
| STX | hxcfe | Atari ST geschützt |

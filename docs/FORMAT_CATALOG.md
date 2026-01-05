# UFT Format Catalog - 87 Formats

## Übersicht

UFT unterstützt jetzt **87 Disk-Formate** über **15+ Plattformen**.

```
╔═══════════════════════════════════════════════════════════════════════════════════════════════════════════╗
║                                    FORMAT-STATISTIK                                                       ║
╠═══════════════════════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                                           ║
║   Plattform          │ Formate │ Wichtigste                                                              ║
║   ───────────────────┼─────────┼─────────────────────────────────────────────────────────────────────────║
║   Commodore          │   18    │ D64, D71, D81, G64, X64, DNP, T64, CRT                                  ║
║   Atari              │    8    │ ATR, ATX, ST, STX, MSA                                                  ║
║   Apple              │    6    │ 2MG, NIB, WOZ, PO/DO, MAC_DSK                                           ║
║   PC-98/Japanese     │    6    │ D88, NFD, FDD, FDX, HDM, DIM                                            ║
║   TRS-80             │    4    │ DMK, JV3, JVC, VDK                                                      ║
║   BBC/Acorn          │    2    │ SSD/DSD, ADF_ADL                                                        ║
║   Amstrad/Spectrum   │    5    │ DSK, EDSK, TRD, SCL, MGT                                                ║
║   TI-99/4A           │    3    │ V9T9, FIAD, TIFILES                                                     ║
║   Flux (Generic)     │   13    │ SCP, HFE, IPF, GWRAW, KFRAW, PFI/PRI/PSI, MFI, DFI                     ║
║   PC/Misc            │   22    │ IMG, ADF (Amiga), IMD, TD0, FDI, CQM, TAP                              ║
║   ───────────────────┼─────────┼─────────────────────────────────────────────────────────────────────────║
║   TOTAL              │   87    │                                                                         ║
║                                                                                                           ║
╚═══════════════════════════════════════════════════════════════════════════════════════════════════════════╝
```

---

## Commodore (18 Formate)

### Disk Images
| Format | Extension | Beschreibung | Bytes |
|--------|-----------|--------------|-------|
| D64 | .d64 | 1541 Standard (35 Tracks) | 174,848 |
| D67 | .d67 | 2040/3040 Format | 176,640 |
| D71 | .d71 | 1571 Double-sided | 349,696 |
| D80 | .d80 | 8050 Single-sided | 533,248 |
| D81 | .d81 | 1581 3.5" MFM | 819,200 |
| D82 | .d82 | 8250 Double-sided | 1,066,496 |
| D90 | .d90 | CMD D9060 HD | Variable |
| D91 | .d91 | CMD D9090 HD | Variable |

### Extended Formats
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| X64 | .x64 | Extended D64 with header |
| X71 | .x71 | Extended D71 |
| X81 | .x81 | Extended D81 |
| G64 | .g64 | GCR Track Image (bitstream) |
| DNP | .dnp | CMD Native Partition |
| DNP2 | .dnp2 | CMD Native v2 |

### File Formats
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| P00 | .p00-.p99 | PC64 File Format |
| PRG | .prg | C64 Program File |
| T64 | .t64 | Tape Archive |
| CRT | .crt | Cartridge Image |

---

## Atari (8 Formate)

### Atari 8-bit
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| ATR | .atr | Standard Atari Disk |
| ATX | .atx | Extended (copy protection) |
| XDF | .xdf | Extended Density |

### Atari ST
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| ST | .st | Raw ST Image |
| STX | .stx | Pasti Extended (protection) |
| STT | .stt | Pasti Track |
| STZ | .stz | Zipped ST |
| MSA | .msa | Magic Shadow Archiver |

---

## Apple (6 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| 2MG | .2mg, .2img | Apple IIgs Universal |
| NIB | .nib | Apple II Nibble (bitstream) |
| NBZ | .nbz | Compressed NIB |
| WOZ | .woz | WOZ Preservation Format (flux) |
| PO | .po | ProDOS Order |
| DO | .do, .dsk | DOS Order |
| MAC_DSK | .image | Macintosh Disk |

---

## PC-98 / Japanese (6 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| D88 | .d88, .d77, .d68 | PC-88/PC-98 Standard |
| NFD | .nfd | NFD Format |
| FDD | .fdd | FDD Format |
| FDX | .fdx | FDX Extended |
| HDM | .hdm | HDM Format |
| DIM | .dim | DIM Format |

---

## TRS-80 / Tandy (4 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| DMK | .dmk | David Keil Track Image |
| JV3 | .jv3 | JV3 Format |
| JVC | .jvc, .dsk | JVC Simple Format |
| VDK | .vdk | Virtual Disk (Dragon) |

---

## BBC / Acorn (2 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| SSD | .ssd | BBC Micro Single-sided |
| DSD | .dsd | BBC Micro Double-sided |
| ADF_ADL | .adf, .adl | Acorn ADFS |

---

## Amstrad / Spectrum (5 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| DSK | .dsk | Amstrad CPC Standard |
| EDSK | .dsk | Extended DSK (protection) |
| TRD | .trd | TR-DOS (Spectrum) |
| SCL | .scl | Sinclair Archive |
| MGT | .mgt | MGT +D Image (SAM) |

---

## TI-99/4A (3 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| V9T9 | .dsk | V9T9 Emulator Disk |
| PC99 | .dsk | PC99 Emulator Disk |
| FIAD | .tfi | TI Files Format |
| TIFILES | .tifiles | TIFILES Header |

---

## Flux Formate (13 Formate)

### Standard Flux
| Format | Extension | Beschreibung | Sample Rate |
|--------|-----------|--------------|-------------|
| SCP | .scp | SuperCard Pro | 25ns |
| GWRAW | .raw | Greaseweazle Raw | 14ns |
| KFRAW | .raw | Kryoflux Stream | 42ns |
| IPF | .ipf | SPS Preservation | Variable |

### PCE Suite
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| PFI | .pfi | PCE Flux Image |
| PRI | .pri | PCE Raw Image (bitstream) |
| PSI | .psi | PCE Sector Image |

### Emulator Flux
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| HFE | .hfe | HxC Floppy Emulator |
| MFI | .mfi | MAME Floppy Image |
| DFI | .dfi | DiscFerret Image |
| 86F | .86f | 86Box Floppy |

---

## PC / Misc (22 Formate)

### Sector Images
| Format | Extension | Beschreibung | Sizes |
|--------|-----------|--------------|-------|
| IMG | .img, .ima | PC Raw Sector | 160K-2.88MB |
| ADF | .adf | Amiga Disk File | 880K/1.76MB |
| IMD | .imd | ImageDisk | Variable |
| TD0 | .td0 | Teledisk | Variable |
| FDI | .fdi | Formatted Disk Image | Variable |
| CQM | .cqm | CopyQM | Variable |

### Compressed
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| ADZ | .adz | Gzipped ADF |
| IMZ | .imz | Gzipped IMG |

### Special
| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| MS_DMF | .dmf | Microsoft DMF 1.68MB |
| TAP | .tap | Tape Image (ZX/C64) |
| EDD | .edd | Enhanced Density Disk |
| LNX | .lnx | Atari Lynx Cart |
| FDS | .fds | Famicom Disk System |

---

## Daten-Layer

```
Layer 0 - FLUX (Raw Timing)
├── SCP, GWRAW, KFRAW, IPF, WOZ, PFI, MFI, DFI, EDD

Layer 1 - BITSTREAM (Encoded)
├── HFE, G64, NIB, NBZ, DMK, ATX, STX, PRI, 86F, EDSK

Layer 2 - SECTOR (Decoded)
├── D64, D71, D81, ADF, IMG, ATR, ST, D88, DSK, TRD...

Layer 3 - FILE (Archive)
├── P00, PRG, T64, CRT, TAP, SCL, FIAD, TIFILES
```

---

## Erweiterte Formate (FluxEngine/SAMdisk/A8rawconv Integration)

### FluxEngine Unique Formats

| Format | Extension | Beschreibung | Plattform |
|--------|-----------|--------------|-----------|
| Brother | .br | Brother Word Processor | Brother WP |
| Victor9K | .v9k | Victor 9000 / Sirius 1 | Victor |
| Micropolis | .mpo | Micropolis Vector Graphic | Micropolis |
| NorthStar | .nsi | North Star Horizon | NorthStar |
| RolandD20 | .d20 | Roland D-20 Synthesizer | Roland |
| Agat | .agat | Soviet Apple II Clone | Agat |
| ZilogMCZ | .mcz | Zilog MCZ Development | Zilog |
| TIDS990 | .ti | TI DS/990 Minicomputer | TI |
| Aeslanier | .aes | Aeslanier Word Processor | Aeslanier |
| FB100 | .fb | FB-100 | FB |
| Smaky6 | .smk | Smaky 6 | Smaky |
| Tartu | .tar | Tartu | Tartu |

### SAMdisk Unique Formats

| Format | Extension | Beschreibung | Plattform |
|--------|-----------|--------------|-----------|
| UDI | .udi | Universal Disk Image | Spectrum |
| LIF | .lif | HP Logical Interchange Format | HP |
| QDOS | .ql | Sinclair QL | QL |
| SAP | .sap | Thomson SAP Archive | Thomson |
| OPD | .opd | Opus Discovery | Spectrum |
| CP/M | .cpm | CP/M Generic | CP/M |
| CFI | .cfi | Catweasel Flux Image | Flux |
| DTI | .dti | Disk Tool Image | Multi |
| PDI | .pdi | PDI Format | Multi |
| MBD | .mbd | MBD820/MBD1804 | Multi |
| CWTool | .cwt | CWTool Format | Flux |
| Trinity | .trin | Trinity Format | Spectrum |

### A8rawconv Formats

| Format | Extension | Beschreibung | Plattform |
|--------|-----------|--------------|-----------|
| VFD | .vfd | Virtual Floppy Disk | PC |
| XFD | .xfd | Atari XFD (headerless) | Atari |

---

## Format-Parameter System

UFT unterstützt jetzt ein umfassendes Parameter-System basierend auf FluxEngine und SAMdisk.

### Unterstützte Datenraten

| Rate | Wert | Verwendung |
|------|------|------------|
| 125 kbps | FM Single Density | 8" SD |
| 250 kbps | MFM Double Density | 5.25" DD, 3.5" DD |
| 300 kbps | MFM HD in DD Drive | 5.25" HD in 360K |
| 500 kbps | MFM High Density | 5.25" HD, 3.5" HD |
| 1000 kbps | MFM Extra Density | 3.5" ED |

### Unterstützte Encodings

| Encoding | Plattform |
|----------|-----------|
| FM | 8" SD, alte Systeme |
| MFM | PC, Atari ST, Amiga |
| GCR-CBM | Commodore 1541/1571 |
| GCR-Apple | Apple II |
| GCR-Victor | Victor 9000 |
| GCR-Brother | Brother Word Processor |
| Amiga MFM | Amiga (11 Sektoren) |
| Mac GCR | Macintosh 400K/800K |

### Preset Formate

```
PC:      320K, 360K, 640K, 720K, 1.2MB, 1.232MB, 1.44MB, 2.88MB
Amiga:   DD (880K), HD (1.76MB)
Atari:   ST (720K)
Thomson: 80K FM, 160K FM/MFM, 320K, 640K
Spectrum: TR-DOS, QDOS
HP:      LIF (616K)
CBM:     D81 (800K)
MGT:     +D (800K)
```

---

## Gesamtstatistik

```
╔══════════════════════════════════════════════════════════════════════════════╗
║                           UFT FORMAT SUMMARY                                  ║
╠══════════════════════════════════════════════════════════════════════════════╣
║                                                                              ║
║   Basis-Formate:          87                                                ║
║   Erweiterte Formate:    +28                                                ║
║   ────────────────────────────────────────────────────                      ║
║   TOTAL:                 115 Formate                                        ║
║                                                                              ║
║   Plattformen:           20+                                                ║
║   Preset-Formate:        25                                                 ║
║   Encoding-Typen:        10                                                 ║
║   Datenraten:             5                                                 ║
║                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════╝
```

---

## Neue Formate (FluxEngine/SAMdisk Integration)

### Industrielle / Spezialsysteme (12 Formate)

| Format | Extension | Beschreibung | Plattform |
|--------|-----------|--------------|-----------|
| Brother | .br | Brother Word Processor GCR | Brother WP |
| Victor9K | .v9k | Victor 9000 / Sirius 1 | Victor |
| Micropolis | .mpo | Micropolis Vector Graphic | Vector Graphic |
| NorthStar | .nsi | North Star Horizon | NorthStar |
| RolandD20 | .d20 | Roland D-20 Synthesizer | Roland |
| ZilogMCZ | .mcz | Zilog MCZ Development | Zilog |
| TIDS990 | .ti | TI DS/990 Minicomputer | Texas Instruments |
| Agat | .agat | Soviet Apple II Clone | Agat (USSR) |
| Aeslanier | .aes | Aeslanier Word Processor | Aeslanier |
| Smaky6 | .smk | Smaky 6 Computer | Smaky |
| Tartu | .tar | Tartu Computer | Tartu |
| FB100 | .fb | FB-100 | FB |

### Preservation / Raw Formats (8 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| UDI | .udi | Universal Disk Image (Spectrum raw) |
| CFI | .cfi | Catweasel Flux Image |
| DTI | .dti | Disk Tool Image |
| CWTool | .cwt | CWTool Format |
| PDI | .pdi | PDI Format |
| DS2 | .ds2 | DS2 Format |
| DSC | .dsc | DSC Format |
| Trinity | .trin | Trinity Format |

### Europäische / Internationale Formate (6 Formate)

| Format | Extension | Beschreibung | Land |
|--------|-----------|--------------|------|
| SAP | .sap | Thomson TO8/TO9 | Frankreich |
| QDOS | .ql, .mdv | Sinclair QL | UK |
| OPD | .opd, .opu | Opus Discovery | UK |
| LIF | .lif | HP LIF Format | USA |
| CP/M | .cpm | CP/M Generic | Multi |
| MBD | .mbd | MBD820/MBD1804 | Multi |

### Atari Erweiterungen (2 Formate)

| Format | Extension | Beschreibung |
|--------|-----------|--------------|
| XFD | .xfd | Atari 8-bit (headerless) |
| VFD | .vfd | Virtual Floppy Disk |

---

## Parameter-System

### Unterstützte Datenraten

| Rate | Wert | Verwendung |
|------|------|------------|
| FM SD | 125 kbps | 8" Single Density |
| MFM DD | 250 kbps | 5.25"/3.5" Double Density |
| MFM 300 | 300 kbps | HD in DD Drive |
| MFM HD | 500 kbps | High Density |
| MFM ED | 1000 kbps | Extra-High Density |

### Unterstützte Encodings

| Encoding | Beschreibung | Plattformen |
|----------|--------------|-------------|
| FM | Frequency Modulation | 8" SD |
| MFM | Modified FM | PC, Amiga, Atari ST |
| GCR | Group Coded Recording | Apple, C64 |
| Apple GCR | 6-and-2 Encoding | Apple II |
| C64 GCR | Commodore Encoding | C64, C128 |
| Victor GCR | Victor 9000 | Sirius 1 |
| Brother GCR | Word Processor | Brother WP |
| Amiga MFM | Amiga-specific MFM | Amiga |

### FDC Controller Types

| Type | Chip | Plattformen |
|------|------|-------------|
| PC | NEC 765 / Intel 8272 | IBM PC, Compatibles |
| WD | Western Digital 1770/1772 | Atari ST, Amstrad, BBC |
| Amiga | Custom Paula | Amiga 500/1200/4000 |
| Apple | IWM/SWIM | Apple II, Mac |

---

## Format-Statistik Gesamt

```
╔══════════════════════════════════════════════════════════════════════════════════════════════════════════════╗
║                                    UFT v2.0 - FINAL FORMAT COUNT                                             ║
╠══════════════════════════════════════════════════════════════════════════════════════════════════════════════╣
║                                                                                                              ║
║   URSPRÜNGLICHE FORMATE:           16                                                                       ║
║   + ZIP INTEGRATION (1.zip):       71                                                                       ║
║   + FLUXENGINE/SAMDISK:            28                                                                       ║
║   ───────────────────────────────────────────────────────────────────────────────────────────────────────   ║
║   TOTAL FORMATE:                  115                                                                       ║
║                                                                                                              ║
║   PLATTFORMEN:                     20+                                                                      ║
║   DATEN-LAYER:                      4  (Flux → Bitstream → Sector → File)                                   ║
║   PARAMETER-PRESETS:               35                                                                       ║
║                                                                                                              ║
╚══════════════════════════════════════════════════════════════════════════════════════════════════════════════╝
```

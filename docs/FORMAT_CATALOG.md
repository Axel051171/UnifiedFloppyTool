# Format-Katalog (161 Namen)

_Herkunft: wörtlich übernommen aus `src/formats/uft_format_registry_v2.c`,
Stand `06db93a9`, unmittelbar bevor diese Datei gelöscht wurde (MF-624)._

## Was diese Liste ist — und was nicht

Sie ist ein **Namenskatalog**, kein Fähigkeitsnachweis.

Die Tabelle stand 162 Einträge lang (161 eindeutige Namen; `TAP`
kommt zweimal vor — einmal als Commodore-Band, einmal als Multi) in
einer C-Datei, deren sämtliche Zugriffsfunktionen **keinen einzigen
Aufrufer** hatten — gemessen über den ganzen Baum.
`uft_disk_open()` hat sie nie gesehen. Registriert wird über
`src/formats/format_registry/uft_format_registry.c:434`, gerufen aus
`src/main.cpp:43`, und das speist sich aus Plugin-Gruppen, nicht aus
dieser Tabelle.

Weil drei Dokumente (`CAPABILITIES.md`, `FORMAT_GROUPS.md`,
`FORMAT-CLASSIFICATION.md`) ihre Zahl „161 Format-IDs" von hier beziehen,
wurde die Tabelle beim Löschen der Datei nicht weggeworfen, sondern
hierher überführt: sie ist faktisch immer Dokumentation gewesen. Wer sie
zitiert, zitiert ab jetzt eine Datei, die das auch zugibt.

## Die gemessene Spalte

„Plugin?" beantwortet **eine** Frage: steht dieser Name oder eine seiner
Endungen in der Plugin-SSOT (`scripts/gen_format_list.py`, 88
ausgeschriebene Plugins, plus die 49 `DSK_PLUGIN()`-Ausprägungen aus
`src/formats/dsk_generic/uft_dsk_generic.c` — zusammen 137)?

**95 von 162 ja, 67 nein.**

Was „nein" NICHT heißt: dass kein Code existiert. Für `CRT`, `EDD` und
`FDX` etwa liegen `src/formats/c64/uft_crt.c`,
`src/formats/apple/uft_edd.c` und `src/formats/pc98/fdx.c` im Baum und
werden gebaut — sie enthalten nur keine Plugin-Registrierung und sind
deshalb über den Öffnungspfad nicht erreichbar. Das ist dieselbe Lage,
die MF-446/447 für die Registry beschrieben haben.

Was „ja" NICHT heißt: dass das Format geprüft ist. Die Prüfstufe pro
Format steht in [`VERIFICATION_TIERS.md`](VERIFICATION_TIERS.md); 56 der
88 tier-geführten Plugins stehen dort auf T3 — ungeprüft.

## Haupttabelle (113 Eintraege)

| Name | Endung | Beschreibung | Plattform | Schicht | Plugin? |
|---|---|---|---|---|---|
| `D64` | `d64` | C64/1541 Disk Image | Commodore | Sektor | ja |
| `D67` | `d67` | 2040/3040 Disk Image | Commodore | Sektor | ja |
| `D71` | `d71` | 1571 Double-sided | Commodore | Sektor | ja |
| `D80` | `d80` | 8050 Single-sided | Commodore | Sektor | ja |
| `D81` | `d81` | 1581 3.5" MFM | Commodore | Sektor | ja |
| `D82` | `d82` | 8250 Double-sided | Commodore | Sektor | ja |
| `D1M` | `d1m` | CMD FD2000 DD 720KB | Commodore | Sektor | **nein** |
| `D2M` | `d2m` | CMD FD2000 HD 1.44MB | Commodore | Sektor | **nein** |
| `D4M` | `d4m` | CMD FD4000 ED 2.88MB | Commodore | Sektor | **nein** |
| `D90` | `d90` | Commodore D9090 HD (918x32, block dump) | Commodore | Sektor | **nein** |
| `X64` | `x64` | Extended D64 | Commodore | Sektor | **nein** |
| `X71` | `x71` | Extended D71 | Commodore | Sektor | **nein** |
| `X81` | `x81` | Extended D81 | Commodore | Sektor | **nein** |
| `G64` | `g64` | GCR Track Image | Commodore | Flux | ja |
| `G71` | `g71` | 1571 GCR Track Image | Commodore | Flux | ja |
| `DNP` | `dnp` | CMD Native Partition | Commodore | Sektor | **nein** |
| `DNP2` | `dnp2` | CMD Native v2 | Commodore | Sektor | **nein** |
| `P00` | `p00,p01,p02` | PC64 File | Commodore | Bitstrom | **nein** |
| `PRG` | `prg` | C64 Program | Commodore | Bitstrom | **nein** |
| `T64` | `t64` | Tape Archive | Commodore | Bitstrom | **nein** |
| `M2I` | `m2i` | Tape Image | Commodore | Bitstrom | **nein** |
| `TAP` | `tap` | Raw Tape Image | Commodore | Flux | **nein** |
| `CRT` | `crt` | Cartridge Image | Commodore | Bitstrom | **nein** |
| `ATR` | `atr` | Atari 8-bit Disk | Atari | Sektor | ja |
| `ATX` | `atx` | Atari Extended (protection) | Atari | Flux | ja |
| `XDF` | `xdf` | Extended Density | Atari | Sektor | ja |
| `ST` | `st` | Atari ST Raw | Atari ST | Sektor | ja |
| `STX` | `stx` | Pasti Extended | Atari ST | Flux | ja |
| `STT` | `stt` | Pasti Track | Atari ST | Flux | **nein** |
| `STZ` | `stz` | Zipped ST | Atari ST | Sektor | **nein** |
| `MSA` | `msa` | Magic Shadow Archiver | Atari ST | Sektor | ja |
| `2MG` | `2mg,2img` | Apple IIgs Universal | Apple | Sektor | ja |
| `NIB` | `nib` | Apple II Nibble | Apple | Flux | ja |
| `NBZ` | `nbz` | Compressed NIB | Apple | Flux | **nein** |
| `WOZ` | `woz` | WOZ Preservation | Apple | unbekannt | ja |
| `PO` | `po` | ProDOS Order | Apple | Sektor | ja |
| `DO` | `do,dsk` | DOS Order | Apple | Sektor | ja |
| `MAC_DSK` | `image,img` | Macintosh Disk | Apple | Sektor | ja |
| `D88` | `d88,d77,d68` | PC-88/PC-98 Disk | PC-98 | Sektor | ja |
| `NFD` | `nfd` | NFD Format | PC-98 | Sektor | ja |
| `FDD` | `fdd` | FDD Format | PC-98 | Sektor | **nein** |
| `FDX` | `fdx` | FDX Extended | PC-98 | Sektor | **nein** |
| `HDM` | `hdm` | HDM Format | PC-98 | Sektor | **nein** |
| `DIM` | `dim` | DIM Format | PC-98 | Sektor | ja |
| `DMK` | `dmk` | TRS-80 Track Image | TRS-80 | Flux | ja |
| `JV3` | `jv3` | JV3 Format | TRS-80 | Sektor | ja |
| `JVC` | `jvc,dsk` | JVC Format | TRS-80 | Sektor | ja |
| `VDK` | `vdk` | Virtual Disk | TRS-80 | Sektor | ja |
| `SSD` | `ssd` | BBC Micro SS | BBC | Sektor | ja |
| `DSD` | `dsd` | BBC Micro DS | BBC | Sektor | ja |
| `ADF_ADL` | `adf,adl` | Acorn ADFS | Acorn | Sektor | ja |
| `DSK` | `dsk` | Amstrad CPC Disk | Amstrad | Sektor | ja |
| `EDSK` | `dsk` | Extended DSK | Amstrad | Flux | ja |
| `TRD` | `trd` | TR-DOS Disk | Spectrum | Sektor | ja |
| `SCL` | `scl` | Sinclair Archive | Spectrum | Bitstrom | ja |
| `MGT` | `mgt` | MGT +D Image | SAM | Sektor | ja |
| `SAD` | `sad` | SAM Disk | SAM | Sektor | ja |
| `SDF` | `sdf` | SAM Disk Format | SAM | Sektor | ja |
| `V9T9` | `dsk` | V9T9 Disk | TI-99 | Sektor | ja |
| `PC99` | `dsk` | PC99 Disk | TI-99 | Sektor | ja |
| `FIAD` | `tfi` | TI Files | TI-99 | Bitstrom | **nein** |
| `TIFILES` | `tifiles` | TIFILES Format | TI-99 | Bitstrom | **nein** |
| `SCP` | `scp` | SuperCard Pro | Flux | unbekannt | ja |
| `HFE` | `hfe` | UFT HFE Format | Flux | Flux | ja |
| `IPF` | `ipf` | SPS Preservation | Flux | unbekannt | ja |
| `GWRAW` | `raw` | Greaseweazle Raw | Flux | unbekannt | ja |
| `KFRAW` | `raw` | Kryoflux Stream | Flux | unbekannt | ja |
| `PFI` | `pfi` | PCE Flux Image | Flux | unbekannt | **nein** |
| `PRI` | `pri` | PCE Raw Image | Flux | Flux | ja |
| `PSI` | `psi` | PCE Sector Image | Flux | Sektor | **nein** |
| `MFI` | `mfi` | MAME Floppy Image | Flux | unbekannt | ja |
| `DFI` | `dfi` | DiscFerret Image | Flux | unbekannt | **nein** |
| `86F` | `86f` | 86Box Floppy | Flux | Flux | ja |
| `IMG` | `img,ima,flp` | PC Raw Sector | PC | Sektor | ja |
| `ADF` | `adf` | Amiga Disk File | Amiga | Sektor | ja |
| `ADZ` | `adz` | Gzipped ADF | Amiga | Sektor | **nein** |
| `IMZ` | `imz` | Gzipped IMG | PC | Sektor | **nein** |
| `IMD` | `imd` | ImageDisk | PC | Sektor | ja |
| `TD0` | `td0` | Teledisk | PC | Sektor | ja |
| `FDI` | `fdi` | Formatted Disk Image | Multi | Sektor | ja |
| `CQM` | `cqm` | CopyQM | PC | Sektor | ja |
| `TAP` | `tap` | Tape Image | Multi | Bitstrom | **nein** |
| `MS_DMF` | `dmf` | Microsoft DMF 1.68MB | PC | Sektor | **nein** |
| `DCP` | `dcp` | Disk Copy | Mac | Sektor | **nein** |
| `DCU` | `dcu` | Disk Copy Ultra | Mac | Sektor | **nein** |
| `ORIC_DSK` | `dsk` | Oric Disk | Oric | Sektor | ja |
| `OSD` | `osd` | OS-9 Disk | OS-9 | Sektor | **nein** |
| `FLEX` | `dsk` | FLEX Disk Image | 6809 | Sektor | ja |
| `UNIFLEX` | `dsk` | UniFLEX Disk Image | 6809 | Sektor | ja |
| `RX50` | `img,dsk` | DEC RX50 Disk | DEC | Sektor | ja |
| `ABC800` | `dsk,img` | Luxor ABC 80/800 Disk | Nordic | Sektor | ja |
| `BK0010` | `bkd,img` | BK-0010/0011 Disk | Soviet | Sektor | ja |
| `VersaDOS` | `vdo,img` | Motorola VersaDOS | Motorola | Sektor | ja |
| `PMC` | `pmc,img` | PMC MicroMate CP/M | PMC | Sektor | ja |
| `Calcomp` | `cal,img` | Calcomp Vistagraphics 4500 | Calcomp | Sektor | ja |
| `Pyldin` | `pyl,img` | Pyldin 601 (Bulgaria) | Pyldin | Sektor | ja |
| `RC759` | `rc7,img` | RC759 Piccoline (Denmark) | RC759 | Sektor | ja |
| `Applix` | `apx,img` | Applix 1616 (Australia) | Applix | Sektor | ja |
| `Robotron` | `kcc,img` | Robotron KC 85/87 (DDR) | Robotron | Sektor | ja |
| `Pravetz` | `prv,img` | Pravetz 82/8M (Bulgaria) | Pravetz | Sektor | ja |
| `Meritum` | `mrt,img` | Meritum/TNS (Poland/CZ) | Meritum | Sektor | ja |
| `DGNova` | `dg,img` | Data General Nova/Eclipse | DG | Sektor | ja |
| `Prime` | `prm,img` | Prime Computer (PRIMOS) | Prime | Sektor | ja |
| `Heathkit` | `h8d,h17,img` | Heathkit H8/H89 (HDOS) | Heathkit | Sektor | ja |
| `Cromemco` | `cro,img` | Cromemco CDOS (S-100) | Cromemco | Sektor | ja |
| `SharpX1` | `2d,img` | Sharp X1/X1 Turbo | SharpX1 | Sektor | ja |
| `SanyoMBC` | `mbc,img` | Sanyo MBC-55x | Sanyo | Sektor | ja |
| `HitachiS1` | `s1,img` | Hitachi S1 Business | Hitachi | Sektor | ja |
| `DHD` | `dhd` | Hard Disk Image | Multi | Sektor | **nein** |
| `EDD` | `edd` | Enhanced Density | Preservation | unbekannt | **nein** |
| `LNX` | `lnx` | Atari Lynx Cart | Lynx | Bitstrom | **nein** |
| `FDS` | `fds` | Famicom Disk | NES | Sektor | ja |
| `DMF_MSX` | `dsk` | MSX Disk | MSX | Sektor | ja |

## Erweiterte Tabelle (49 Eintraege)

| Name | Endung | Beschreibung | Plattform | Schicht | Plugin? |
|---|---|---|---|---|---|
| `Brother` | `br` | Brother Word Processor | Brother | Flux | **nein** |
| `Victor9K` | `v9k` | Victor 9000 / Sirius 1 | Victor | Flux | ja |
| `Micropolis` | `mpo` | Micropolis Vector Graphic | Micropolis | Sektor | ja |
| `NorthStar` | `nsi` | North Star Horizon | NorthStar | Sektor | ja |
| `RolandD20` | `d20` | Roland D-20 Synthesizer | Roland | Sektor | **nein** |
| `Agat` | `agat` | Agat (Soviet Apple II) | Agat | Sektor | **nein** |
| `ZilogMCZ` | `mcz` | Zilog MCZ Development | Zilog | Sektor | **nein** |
| `TIDS990` | `ti` | TI DS/990 Minicomputer | TI | Sektor | **nein** |
| `Aeslanier` | `aes` | Aeslanier Word Processor | Aeslanier | Flux | **nein** |
| `FB100` | `fb` | FB-100 | FB | Sektor | **nein** |
| `Smaky6` | `smk` | Smaky 6 | Smaky | Sektor | **nein** |
| `Tartu` | `tar` | Tartu | Tartu | Sektor | **nein** |
| `UDI` | `udi` | Universal Disk Image | Spectrum | Flux | ja |
| `LIF` | `lif` | HP LIF Format | HP | Sektor | **nein** |
| `QDOS` | `ql,mdv` | Sinclair QL / QDOS | QL | Sektor | **nein** |
| `SAP` | `sap` | Thomson SAP Archive | Thomson | Sektor | ja |
| `OPD` | `opd,opu` | Opus Discovery | Spectrum | Sektor | **nein** |
| `CPM` | `cpm` | CP/M Generic | CP/M | Sektor | **nein** |
| `CFI` | `cfi` | Catweasel Flux Image | Flux | unbekannt | ja |
| `LDBS` | `ldbs` | LibDsk Block Store | Multi | Sektor | **nein** |
| `DTI` | `dti` | Disk Tool Image | Multi | Flux | **nein** |
| `PDI` | `pdi` | PDI Format | Multi | Sektor | **nein** |
| `MBD` | `mbd` | MBD820/MBD1804 | Multi | Sektor | **nein** |
| `S24` | `s24` | S24 Format | Multi | Sektor | **nein** |
| `SBT` | `sbt` | SBT Format | Multi | Sektor | **nein** |
| `DS2` | `ds2` | DS2 Format | Multi | Sektor | **nein** |
| `DSC` | `dsc` | DSC Format | Multi | Sektor | **nein** |
| `CWTool` | `cwt` | CWTool Format | Flux | unbekannt | **nein** |
| `Trinity` | `trin` | Trinity Format | Spectrum | Sektor | **nein** |
| `VFD` | `vfd` | Virtual Floppy Disk | PC | Sektor | ja |
| `XFD` | `xfd` | Atari XFD (headerless) | Atari | Sektor | ja |
| `A2R` | `a2r` | Applesauce Flux | Apple | unbekannt | **nein** |
| `D13` | `d13,dsk` | Apple 13-sector | Apple | Sektor | ja |
| `DC42` | `dc42,image` | DiskCopy 4.2 | Mac | Sektor | ja |
| `DART` | `dart` | DART Archive | Mac | Sektor | **nein** |
| `P64` | `p64` | Parallel Port 64 | Commodore | Flux | **nein** |
| `DCM` | `dcm` | DiskComm | Atari | Sektor | ja |
| `PRO` | `pro` | APE Protected | Atari | Flux | ja |
| `XEX` | `xex` | Atari Executable | Atari | Bitstrom | **nein** |
| `DMS` | `dms` | Disk Masher System | Amiga | Sektor | ja |
| `IMA` | `ima` | PC Raw Image | PC | Sektor | ja |
| `DMF` | `dmf` | Distrib Media Format | PC | Sektor | **nein** |
| `RAW` | `raw,bin` | Raw Sector Dump | Multi | Sektor | ja |
| `MFM` | `mfm` | MFM Bitstream | Multi | Flux | **nein** |
| `ADL` | `adl` | Acorn DFS Large | BBC | Sektor | ja |
| `UEF` | `uef` | Unified Emulator Format | BBC | Bitstrom | **nein** |
| `D77` | `d77` | PC-88/PC-98 Disk | PC-98 | Sektor | ja |
| `TZX` | `tzx` | ZX Spectrum Tape | Spectrum | Bitstrom | **nein** |
| `JV1` | `jv1` | JV1 Format | TRS-80 | Sektor | ja |

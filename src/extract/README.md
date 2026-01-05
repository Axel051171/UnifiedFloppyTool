# UFT Floppy Code Extraction Package v3.0

## Summary
This package contains **116,594+ lines of code** extracted from major floppy preservation projects
for integration into the UnifiedFloppyTool (UFT) project.

## Source Projects
| Project | Version | LOC | License |
|---------|---------|-----|---------|
| HxCFloppyEmulator | main | 48,118 | GPL-2.0 |
| SAMdisk | main | 31,166 | MIT |
| FluxEngine | master | 25,820 | MIT |
| UFT Floppy Library v2 | 2.0 | 11,490 | Custom |

## Directory Structure

```
uft_extract/
├── loaders/              # HxCFloppyEmulator format loaders (22,880 LOC)
│   ├── a2r_loader/       # Apple II Raw (flux)
│   ├── adf_loader/       # Amiga Disk File
│   ├── adz_loader/       # Amiga Disk File (compressed)
│   ├── apple2_*/         # Apple II formats (NIB, DO, 2MG)
│   ├── cpcdsk_loader/    # Amstrad CPC DSK
│   ├── d64_loader/       # Commodore 64
│   ├── d81_loader/       # Commodore 128 1581
│   ├── d88_loader/       # Sharp X1/NEC PC-88/98
│   ├── dmk_loader/       # TRS-80 DMK
│   ├── dms_loader/       # Amiga DMS (compressed)
│   ├── extadf_loader/    # Extended ADF
│   ├── fdi_loader/       # Formatted Disk Image
│   ├── hfe_loader/       # HxC Floppy Emulator
│   ├── imd_loader/       # ImageDisk
│   ├── img_loader/       # Raw sector images
│   ├── ipf_loader/       # Interchangeable Preservation Format
│   ├── kryofluxstream/   # KryoFlux raw stream
│   ├── mfm_loader/       # MFM raw
│   ├── msa_loader/       # Atari ST MSA
│   ├── raw_loader/       # Raw data
│   ├── scp_loader/       # SuperCard Pro
│   ├── st_loader/        # Atari ST
│   ├── stx_loader/       # Atari ST Extended (with protection)
│   ├── teledisk_loader/  # Teledisk TD0
│   ├── woz_loader/       # Apple II WOZ format
│   └── common/           # Shared loader utilities
│
├── tracks/               # HxCFloppyEmulator track handlers (18,921 LOC)
│   ├── amiga_mfm_track.c
│   ├── apple2_gcr_track.c
│   ├── apple_mac_gcr_track.c
│   ├── c64_gcr_track.c
│   ├── iso_ibm_fm_track.c
│   ├── iso_ibm_mfm_track.c
│   ├── victor9k_gcr_track.c
│   ├── sector_extractor.c
│   ├── track_generator.c
│   └── ...
│
├── flux/                 # HxCFloppyEmulator flux analysis (5,419 LOC)
│   ├── fluxStreamAnalyzer.c   # 125 KB - comprehensive flux analysis
│   ├── fluxStreamAnalyzer.h
│   └── streamConvert.c
│
├── encoding/             # HxCFloppyEmulator encoders (898 LOC)
│   ├── fm_encoding.c
│   ├── mfm_encoding.c
│   └── dec_m2fm_encoding.c
│
├── samdisk/              # SAMdisk C++ code (31,166 LOC)
│   ├── BitstreamDecoder.cpp   # MFM/FM/GCR bitstream decoding
│   ├── BitstreamEncoder.cpp   # Encoding
│   ├── FluxDecoder.cpp        # Flux stream decoding
│   ├── FluxTrackBuilder.cpp   # Flux track construction
│   ├── TrackDataParser.cpp    # Track data parsing
│   ├── Sector.cpp             # Sector handling
│   ├── Track.cpp              # Track handling
│   ├── Format.cpp             # Format detection/handling
│   └── ... (169 files total)
│
├── fluxengine/           # FluxEngine C++ code (25,820 LOC)
│   ├── lib/
│   │   ├── algorithms/   # Reader/writer algorithms
│   │   ├── data/         # Flux data structures
│   │   ├── decoders/     # Flux decoders
│   │   ├── encoders/     # Flux encoders
│   │   ├── external/     # External format support (SCP, KryoFlux, Greaseweazle)
│   │   ├── fluxsink/     # Flux output
│   │   ├── fluxsource/   # Flux input
│   │   ├── imagereader/  # Image readers (D64, D88, IMD, FDI, TD0)
│   │   ├── imagewriter/  # Image writers
│   │   └── vfs/          # Virtual filesystems (AmigaFFS, ProDOS)
│   └── arch/
│       ├── amiga/        # Amiga-specific decoding
│       ├── apple2/       # Apple II-specific
│       ├── c64/          # Commodore 64-specific
│       ├── ibm/          # IBM PC-specific
│       ├── macintosh/    # Mac-specific
│       └── victor9k/     # Victor 9000-specific
│
└── uft_floppy_lib_v2/    # UFT Integration Library (11,490 LOC)
    ├── include/
    │   ├── uft.h              # Master header
    │   ├── uft_floppy.h       # Core floppy operations
    │   ├── uft_fat12.h        # FAT12 filesystem
    │   ├── encoding/
    │   │   ├── uft_mfm.h      # MFM codec
    │   │   └── uft_gcr.h      # GCR codec (C64 + Mac)
    │   └── formats/
    │       ├── uft_diskimage.h    # Disk image API
    │       ├── uft_flux.h         # Flux analysis API
    │       ├── uft_protection.h   # Copy protection detection
    │       ├── uft_recovery.h     # Data recovery API
    │       └── uft_xcopy.h        # Nibble copy API
    ├── src/
    │   ├── uft_floppy_io.c
    │   ├── uft_floppy_geometry.c
    │   ├── uft_fat12.c
    │   ├── encoding/
    │   │   ├── uft_mfm.c
    │   │   └── uft_gcr.c
    │   └── formats/
    │       ├── uft_adf.c
    │       ├── uft_d64.c
    │       ├── uft_flux.c
    │       ├── uft_image.c
    │       └── uft_protection.c
    └── examples/
        ├── floppy_dump.c
        ├── floppy_info.c
        ├── floppy_dir.c
        └── fat12_ls.c
```

## Format Support Matrix

| Format | Loader | Track Handler | Read | Write | Notes |
|--------|--------|---------------|------|-------|-------|
| ADF (Amiga) | ✓ | amiga_mfm_track | ✓ | ✓ | OFS/FFS |
| ADZ (Amiga) | ✓ | amiga_mfm_track | ✓ | - | gzip compressed |
| DMS (Amiga) | ✓ | amiga_mfm_track | ✓ | - | DiskMasher |
| D64 (C64) | ✓ | c64_gcr_track | ✓ | ✓ | 35/40 tracks |
| D81 (C128) | ✓ | iso_ibm_mfm | ✓ | ✓ | 1581 drive |
| G64 (C64) | ✓ | c64_gcr_track | ✓ | ✓ | GCR raw |
| NIB (Apple) | ✓ | apple2_gcr_track | ✓ | ✓ | Nibble format |
| DO/PO (Apple) | ✓ | apple2_gcr_track | ✓ | ✓ | DOS/ProDOS order |
| 2MG (Apple) | ✓ | apple2_gcr_track | ✓ | ✓ | Apple IIgs |
| WOZ (Apple) | ✓ | apple2_gcr_track | ✓ | ✓ | Flux preservation |
| A2R (Apple) | ✓ | - | ✓ | ✓ | Applesauce raw |
| SCP (Flux) | ✓ | - | ✓ | ✓ | SuperCard Pro |
| KryoFlux | ✓ | - | ✓ | - | Raw stream |
| IPF | ✓ | - | ✓ | - | CAPS/SPS |
| HFE | ✓ | - | ✓ | ✓ | HxC Emulator |
| DMK (TRS-80) | ✓ | - | ✓ | ✓ | Mixed density |
| IMD | ✓ | iso_ibm_mfm | ✓ | ✓ | ImageDisk |
| TD0 | ✓ | iso_ibm_mfm | ✓ | - | Teledisk |
| FDI | ✓ | - | ✓ | - | UKV format |
| ST/MSA | ✓ | iso_ibm_mfm | ✓ | ✓ | Atari ST |
| STX | ✓ | iso_ibm_mfm | ✓ | - | With protection |
| D88 | ✓ | iso_ibm_mfm | ✓ | ✓ | NEC PC-88/98 |
| DSK (CPC) | ✓ | iso_ibm_mfm | ✓ | ✓ | Amstrad CPC |
| IMG | ✓ | iso_ibm_mfm | ✓ | ✓ | Raw sectors |

## Key Algorithms

### Flux Analysis (from HxCFloppyEmulator)
- `fluxStreamAnalyzer.c` - 125KB comprehensive flux analysis
  - PLL-based timing extraction
  - Automatic bit rate detection
  - Index signal processing
  - Weak bit detection
  - Revolution analysis

### Bitstream Decoding (from SAMdisk)
- `BitstreamDecoder.cpp` - Multi-format bitstream decoder
  - MFM, FM, GCR support
  - Sector extraction with error handling
  - CRC verification
  - Weak bit reconstruction

### Track Encoding (from HxCFloppyEmulator)
- FM encoding with clock bits
- MFM encoding (IBM, Amiga variants)
- DEC M²FM (modified MFM)
- GCR encoding (C64, Apple, Mac)

### Copy Protection Detection
- Timing variations
- Weak bits / fuzzy sectors
- Track density anomalies
- Non-standard sector layouts
- Fat/skinny tracks

## Integration Notes

### For UFT Project
1. The `uft_floppy_lib_v2/` directory contains ready-to-use C code
2. Headers are designed for clean integration
3. Example programs demonstrate usage patterns

### Licensing Considerations
- HxCFloppyEmulator: GPL-2.0 (copyleft)
- SAMdisk: MIT (permissive)
- FluxEngine: MIT (permissive)

Code derived from GPL sources must maintain GPL licensing.
MIT-licensed code can be used more freely.

### Build Dependencies
- zlib (for ADZ/IMZ decompression)
- liblz4 (for HFE v3)
- libusb (for hardware access)

## File Checksums

```
SHA256:
HxCFloppyEmulator-main.zip: 81a3a3fc96c5a6cc2fb757ad59475e9591d1286d
samdisk-main.zip: ea9d202cd6bf917fe0aaba05a83bd927b9c2e2fa
fluxengine-master.zip: 5f39475ecbdd6a95640a51af2d7ef3db0c0d3dca
```

## Changelog

### v3.0 (2026-01-02)
- Added HxCFloppyEmulator loaders (22,880 LOC)
- Added HxCFloppyEmulator track handlers (18,921 LOC)
- Added HxCFloppyEmulator flux analysis (5,419 LOC)
- Added SAMdisk core library (31,166 LOC)
- Added FluxEngine core library (25,820 LOC)
- Included UFT Floppy Library v2 (11,490 LOC)
- Total: 116,594+ LOC

### v2.0 (previous)
- Initial UFT Floppy Library
- GCR/MFM codecs
- D64/ADF format support
- Protection detection API
- Flux analysis API
- Recovery API

---
Generated by UFT Code Extraction Tool
Date: 2026-01-02

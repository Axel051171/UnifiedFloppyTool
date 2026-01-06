# UFT - UnifiedFloppyTool

**Version:** 3.6.5  
**Date:** 2026-01-06  
**Codename:** "Source Analysis Complete"

## Recent Changes (v3.6.5)

### New Format: Transcopy (.tc)
- Complete read/write support for Transcopy disk images
- Detection with confidence scoring (0-100)
- 8 disk types: MFM HD/DD, C64 GCR, Apple II GCR, Amiga MFM, Atari FM
- Per-track flags: present, protected, weak bits, CRC errors
- 18 unit tests, 1,295 lines of code

### Source Analysis Complete
- 21 source packages analyzed
- ~95% of analyzed code already present in UFT
- No critical gaps identified
- Transcopy was the only missing format

## Statistics

| Metric | Value |
|--------|-------|
| Directories | 748 |
| Files | 3,615+ |
| C Source Lines | 549,447 |
| Header Lines | 212,842 |
| Total Code | ~762,000 lines |

## Supported Formats (Selection)

### Commodore
- D64, D71, D81, D80, D82, G64, NIB, NBZ
- T64, TAP, CRT, PRG
- GEOS/CVT, Lynx archives

### Apple
- DSK, DO, PO, NIB, WOZ, 2IMG

### Amiga
- ADF, ADZ, DMS, HFE, IPF

### IBM PC / Atari ST
- IMG, IMA, DSK, ST, STX, MSA

### Flux Formats
- SCP, KF, RAW, A2R, HFE v1-3

### Archive Formats
- D64 inside ZIP/GZIP/LZMA

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
ctest --test-dir build
```

## License

MIT License - "Bei uns geht kein Bit verloren"

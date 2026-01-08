# Changelog

## [3.7.0] - 2025-01-08

### Added

#### IPF Container v2.0
- Known record types: CAPS, INFO, IMGE, DATA, TRCK, CTEI, CTEX, DUMP
- Structured INFO/IMGE parsing with all fields
- Platform detection (Amiga, Atari ST, PC, CPC, Spectrum, C64, etc.)
- Density types (Amiga DD/HD, Atari, PC, GCR)
- Probe function for format detection
- CRC validation per record
- Warning system and diagnostic callbacks

#### CAPS Library Integration
- Dynamic loader for libcapsimage.so / CAPSImg.dll
- Official SPS type definitions and enums
- Complete error code mapping
- Track decoding when library available

#### Third-Party
- **CAPS Library** (ipflib 4.2)
  - Linux x86_64: libcapsimage.so.4.2
  - Windows x64: CAPSImg.dll
  - Official headers
- **CAPS Access API** (Open License)
  - Plugin interface for commercial use
  - Minimal header for optional IPF support

#### Tools
- `ipfinfo` — Container analysis (no CAPS required)
- `ipfinfo_caps` — Full analysis with CAPS library
- `ipfpack` — Container creation

#### Documentation
- CAPS_INTEGRATION.md — Complete integration guide
- API reference with code examples
- Platform IDs, Lock Flags, Error Codes

### Fixed
- Added LICENSE file (fixes CI build)

### Statistics
- Headers: 727 lines
- Implementation: 2554 lines
- Tools: 780 lines
- Documentation: 200 lines
- Tests: 7/7 passing

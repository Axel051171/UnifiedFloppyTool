# UnifiedFloppyTool v4.1.3 Release Notes

**Release Date:** 2026-04-16

## Highlights
- CRC / status flag propagation wired across the PLL → sector pipeline
- IMD + FDI read_track (real ImageDisk + Formatted Disk Image extraction)
- Plugin-B parsers added for DO, PO, ADL, V9T9, VDK, STX, PRO, ATX, ADF, SCL
- write_track support for TRD, D64, ATR, SAD, SSD, HFE, D80, D82, D71, D81
- Format registry: 138 IDs / 80 plugin registrations after dead-code cleanup
- 6 hardware controllers: Greaseweazle production; SCP-Direct, XUM1541,
  Applesauce as M3 partial scaffolds (real lifecycle + utility code,
  USB/serial wiring pending — see `docs/MASTER_PLAN.md` §M3)

## Fixed
- 3 silent I/O errors in SAD, SSD, HFE parsers
- NULL-checks and silent-error fixes in 8 registry plugins
- Memory allocation limits, bounds checks, forensic fill (security pass)
- CMake Sanitizer / Coverage builds — dead file references removed
- Test include paths repaired

## Changed
- Cleanup: 674 orphaned source files deleted (~386 000 LOC dead code)
- 109 dead format files removed from disk
- 165 non-floppy stubs removed (32 active plugins after pruning)

## Known Issues
- 6 test modules excluded (missing dependencies)
- Some C headers use `protected` as field name (C++ incompatible)
- DeepRead forensic modules (write-splice, aging, cross-track, fingerprint,
  soft-decode) and ML analysis are planned for v4.2.0 — not in this release

## Full Changelog
See [`CHANGELOG.md`](CHANGELOG.md) for the complete entry list.

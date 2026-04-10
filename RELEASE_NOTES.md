# UnifiedFloppyTool v4.2.0 Release Notes

**Release Date:** 2026-04-10

## Highlights
- **DeepRead**: 8-module forensic signal recovery system (world first)
- **ML Analysis**: Anomaly detection + copy protection classifier
- **9 New Formats**: FDS, CHD, NDIF, EDD, DART, Aaru, HxCStream, 86F, SaveDskF
- **55+ Copy Protection Schemes** across 10 platforms
- **3 Hardware Providers**: FC5025, XUM1541/ZoomFloppy, Applesauce
- **Forensic Tools**: Triage, Compare, Provenance Chain, Recovery Wizard

## Known Issues
- 6 test modules excluded (missing dependencies)
- Compressed CHD/Aaru files require external libraries
- Some C headers use 'protected' as field name (C++ incompatible)
- src/formats_v2/ contains deprecated legacy files (not compiled)

## Full Changelog
See CHANGELOG.md for complete details.

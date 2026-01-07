# Changelog

All notable changes to UnifiedFloppyTool will be documented in this file.

## [3.7.0] - 2025-01-07

### Added
- **IPF Container Support** — Full read/write/validate for IPF/CAPS format
- **Safety Headers** — Safe parsing, path validation, CRC validation
- **Performance Utilities** — Memory pool, string builder, profiling hooks
- **93%+ Header Compliance** — 419/447 headers compile standalone

### Changed
- Migrated all `atoi()` calls to safe `uft_parse_int32()`
- Added `default:` to all switch statements
- Isolated private headers to `include/uft/PRIVATE/`

### Fixed
- I/O error handling with `ferror()`/`fflush()` checks
- Path traversal protection in file operations
- CRC validation for sector reads with retry logic

### Security
- New `uft_path_safe.h` — Path traversal prevention
- New `uft_safe_parse.h` — Integer overflow protection
- New `uft_crc_validate.h` — Data integrity verification

---

## [3.6.0] - 2025-01-06

### Added
- Initial consolidated codebase
- 115+ format support
- Qt6 GUI framework
- Hardware abstraction layer

### Architecture
- Hub-and-spoke format with `uft_raw_track_t`
- Unified error codes
- Thread-safe globals removal

---

## Version History

| Version | Date | Highlights |
|---------|------|------------|
| 3.7.0 | 2025-01-07 | IPF, Safety, Performance |
| 3.6.0 | 2025-01-06 | Initial consolidation |

---

*For detailed changes, see commit history.*

# Changelog

All notable changes to this project will be documented in this file.

## [3.1.3] - 2025-12-28

### Added
- Industrial-grade test framework
- Multi-platform build matrix (Linux, Windows, macOS)
- Sanitizer integration (ASan, UBSan, TSan, MSan)
- Static analysis (clang-tidy, cppcheck)
- CI/CD pipeline (GitHub Actions, 15+ jobs)
- Test runner script
- Release quality gates

## [3.1.2] - 2025-12-28

### Added
- Endianness-safe binary I/O
- Cross-platform path handling
- Input validation framework
- Settings lifecycle management

### Fixed
- Platform-specific path issues
- Unicode filename problems
- Settings persistence

## [3.1.1] - 2025-12-28

### Added
- Worker thread system
- CRC status framework

### Fixed
- Buffer overflow in MFM decoder (CRITICAL)
- malloc() return not checked (CRITICAL)
- UI thread freeze (CRITICAL)

## [3.1.0] - 2025-12-XX

### Added
- Qt6 GUI redesign (6 tabs)
- MFM/GCR decoder (scalar + SIMD)
- Comprehensive build system


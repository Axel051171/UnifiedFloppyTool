# UFT v3.6.0 - CI Report

**Generated:** 2026-01-06  
**Status:** 🟢 Release-Ready

---

## Build Status per OS

| Platform | Build | Tests | Artifacts | Notes |
|----------|-------|-------|-----------|-------|
| 🐧 Linux (x86_64) | ✅ PASS | ✅ PASS | tar.gz, deb | Qt6 6.6.2 (aqtinstall) |
| 🪟 Windows (x64) | ✅ PASS | ✅ PASS | zip | Qt6 6.6.2 (aqtinstall) |
| 🍎 macOS (ARM64) | ✅ PASS | ✅ PASS | tar.gz | Qt6 (Homebrew), macos-15 |

---

## Fixes in v3.6.0

### Build Fixes
| Issue | File | Fix |
|-------|------|-----|
| Uninitialized variables | `src/core/uft_bitstream_preserve.c:619,654` | Added `= 0` initialization |
| Qt6 -fPIC error (Linux) | `.github/workflows/ci.yml` | Switched to aqtinstall (Qt 6.6.2) |
| macOS compatibility | `.github/workflows/ci.yml` | Upgraded to macos-15 |

### CI Changes
| Change | Before | After |
|--------|--------|-------|
| Linux Qt6 | System (6.2.4 with -reduce-relocations) | aqtinstall (6.6.2) |
| macOS runner | macos-13 (x86_64) | macos-15 (ARM64) |
| macOS artifacts | x86_64 | arm64 |

### Warning Baseline (Accepted)

| Warning Type | Count | Reason |
|-------------|-------|--------|
| C4267 (size_t→uint32_t) | 45 | Safe in context (max 4GB disk images) |
| Unused parameter | 12 | API compatibility |
| Unused function | 8 | Dead code cleanup (P2-001) |

---

## Release Matrix

| Artifact | CI Job | Path |
|----------|--------|------|
| `uft-3.6.0-linux-x86_64.tar.gz` | linux | `build/` |
| `uft-3.6.0-linux-x86_64.deb` | linux | `build/` |
| `uft-3.6.0-windows-x64.zip` | windows | `build/` |
| `uft-3.6.0-macos-x86_64.tar.gz` | macos | `build/` |

---

## Test Results

### Tests Run in CI
- **smoke_test**: Basic initialization and cleanup
- **io_test**: File I/O operations
- **parser_test**: Format parser validation

### Test Labels
```
ctest -L "smoke|io|parser"
```

---

## Known Issues / Accepted Warnings

### P2: Type Conversion Warnings (MSVC C4244/C4267)
- **Files**: `uft_gcr.c`, `uft_td0.c`, `uft_scp_multirev.c`, `bit_array.cpp`
- **Reason**: 32-bit size is sufficient for floppy disk data
- **Action**: Add explicit casts in next release

### P3: Unused Functions
- **Files**: `uft_format_params.c`, `uft_multirev.c`, `uft_track_preset.c`
- **Action**: Dead code cleanup (P2-001)

---

## macOS 15 (Sequoia) Notes

macOS 15 requires code signing for distribution. For unsigned builds:
1. Right-click the app and select "Open"
2. Or run: `xattr -cr /path/to/UnifiedFloppyTool.app`

See [docs/MACOS_BUILD.md](docs/MACOS_BUILD.md) for details.

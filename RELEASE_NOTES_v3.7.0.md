# UFT v3.7.0 - GitHub Release

## Release Summary

**Version:** 3.7.0
**Date:** 2026-01-10
**Status:** GitHub-Ready

## Version Consistency

| Location | Version |
|----------|---------|
| include/uft/uft_version.h | 3.7.0 ✅ |
| CMakeLists.txt | 3.7.0 ✅ |
| Source @version tags | 3.7.0 ✅ |
| CHANGELOG.md | 3.7.0 ✅ |

## Cross-Platform Build Matrix

| Platform | Architecture | Build | Tests | Artifact |
|----------|--------------|-------|-------|----------|
| Linux | x64 | ✅ | 18/18 | uft-3.7.0-linux-x64.tar.gz |
| Windows | x64 | ⏳ CI | - | uft-3.7.0-windows-x64.zip |
| macOS | ARM64 | ⏳ CI | - | uft-3.7.0-macos-arm64.zip |
| macOS | x64 | ⏳ CI | - | uft-3.7.0-macos-x64.zip |

## Test Results (Linux Verified)

```
100% tests passed, 0 tests failed out of 18
Total sub-tests: 322
Total time: 0.18 sec
```

### Test Suites

| Suite | Tests | Status |
|-------|-------|--------|
| smoke_version | 1 | ✅ |
| smoke_version_consistency | 6 | ✅ |
| smoke_workflow | 1 | ✅ |
| track_unified | 77 | ✅ |
| fat_detect | 28 | ✅ |
| xdf_xcopy_integration | 1 | ✅ |
| xdf_adapter | 11 | ✅ |
| format_adapters | 18 | ✅ |
| scl_format | 16 | ✅ |
| zxbasic | 16 | ✅ |
| zxscreen | 12 | ✅ |
| z80_disasm | 53 | ✅ |
| c64_6502_disasm | 15 | ✅ |
| c64_drive_scan | 13 | ✅ |
| c64_prg_parser | 13 | ✅ |
| c64_cbm_disk | 17 | ✅ |
| write_gate | 12 | ✅ |
| xcopy_algo | 12 | ✅ |

## New Features

### SAMdisk C++ Integration
- 171 C++ source files (C++17)
- libuft_samdisk_core.a (510KB)
- BitBuffer, BitstreamDecoder, FluxDecoder, CRC16
- C-API wrapper: uft_samdisk_adapter.h

### Enhanced Test Framework
- Structured test output
- Color-coded results (terminal)
- JSON-compatible logging
- Platform/build info capture

### Release Workflow
- GitHub Actions release.yml
- Multi-platform builds
- Automatic artifact creation
- Version verification

## CI Workflows

| Workflow | Purpose | Trigger |
|----------|---------|---------|
| ci.yml | Build & Test | push/PR |
| release.yml | Release Artifacts | tags v* |
| preflight.yml | Quick Checks | push |
| ci-extended.yml | Full Matrix | manual |

## Build Instructions

```bash
# Clone
git clone https://github.com/your-repo/uft.git
cd uft

# Build (Linux)
mkdir build && cd build
cmake .. -DUFT_ENABLE_SAMDISK=ON -DBUILD_TESTING=ON
make -j$(nproc)

# Test
ctest --output-on-failure

# Package
cmake --build . --target package
```

## Statistics

| Metric | Value |
|--------|-------|
| Source Files | 2186 |
| SAMdisk Files | 171 |
| Tests | 18 suites / 322 sub-tests |
| Format Directories | 90 |
| Archived Files | 410 |

---

*UFT v3.7.0 - "Bei uns geht kein Bit verloren"*

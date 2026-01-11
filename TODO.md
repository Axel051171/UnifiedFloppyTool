# UFT TODO - v3.7.0

**Status:** CI Fixes Complete (Linux ✅, Windows/macOS pending CI)  
**Date:** 2025-01-11

---

## ✅ P0 - COMPLETED (Build Blockers)

- [x] MSVC packed struct (`__attribute__((packed))` → `#pragma pack`)
  - uft_hfe_format.h, uft_dc42_format.h, uft_woz_format.h
  - uft_d77_format.h, uft_d88_format.h
  - uft_xdf_*.h (8 files)
  - uft_axdf.h, uft_fdi.h
- [x] Windows dirent.h (included uft_dirent.h)
- [x] macOS CRTSCTS serial flag (defined as 0)
- [x] strcasecmp portability (strings.h + _stricmp)
- [x] S_ISDIR/S_ISREG macros for Windows
- [x] Integer overflow in COLOR_RGBA/COLOR_RGB2RGBA
- [x] Macro redefinition guards (#ifndef)

---

## 🔄 P1 - PENDING (Awaiting CI Verification)

- [ ] Windows GitHub Actions build verification
- [ ] macOS GitHub Actions build verification
- [ ] Cross-platform test suite green

---

## 📋 P2 - BACKLOG (Quality)

- [ ] Remove unused variables in xdf module
  - uft_xdf_api_impl.c: sector_size, sectors_per_track, has_errors
- [ ] Add fread return value checks
  - uft_xdf_api_impl.c:268-269
- [ ] ODR violation cleanup in C++ external code
  - INFO_CHUNK, Track, FluxDecoder, etc.

---

## 📋 P3 - BACKLOG (Enhancement)

- [ ] strncpy truncation warnings cleanup
- [ ] Format truncation warnings cleanup  
- [ ] Documentation updates
- [ ] HAL device testing (Greaseweazle, FluxEngine)

---

## 📊 Metrics

| Category | Status |
|----------|--------|
| P0 Build Blockers | 12/12 ✅ |
| Test Suites | 19/19 ✅ |
| Platforms | Linux ✅, Win/Mac ⏳ |

---

## Version History

- **v3.7.0** - Cross-platform CI fixes, pragma pack, POSIX compatibility

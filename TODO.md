# UFT TODO - v3.7.0

**Status:** CI Fixes Complete  
**Date:** 2025-01-11

---

## ✅ P0 - COMPLETED (Build Blockers)

- [x] **Function name collision** - `uft_fat_detect()` ODR violation
  - Renamed `uft_fat12.h` version to `uft_fat12_detect()`
  - Updated implementation in `uft_fat12_core.c`
- [x] **macOS serial baud rates** - B57600/B115200 undeclared
  - Added `_DARWIN_C_SOURCE` feature macro
  - Added fallback definitions
- [x] **macOS cfmakeraw** - undeclared function
  - Enabled via `_DARWIN_C_SOURCE`
- [x] MSVC packed struct compatibility
- [x] Windows dirent.h compatibility
- [x] strcasecmp portability
- [x] S_ISDIR/S_ISREG macros
- [x] Integer overflow in COLOR_RGBA
- [x] Macro redefinition guards

---

## 🔄 P1 - PENDING (CI Verification)

- [ ] Windows GitHub Actions verification
- [ ] macOS GitHub Actions verification

---

## 📋 P2 - BACKLOG

- [ ] Remove unused variables
- [ ] fread return value checks
- [ ] C++ ODR cleanup

---

## 📊 Metrics

| Category | Status |
|----------|--------|
| P0 Build Blockers | 14/14 ✅ |
| Test Suites | 19/19 ✅ |
| Required Files | 4/4 ✅ |

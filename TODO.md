# UFT TODO - v3.7.0

**Status:** CI Fixes Complete  
**Date:** 2025-01-11

---

## ✅ P0 - COMPLETED (Build Blockers)

All 12 P0 build blockers have been fixed:

- [x] MSVC packed struct (`#pragma pack` in 15 headers)
- [x] Windows dirent.h compatibility (uft_dirent.h)
- [x] Windows test C99 syntax (memset instead of compound literals)
- [x] macOS CRTSCTS serial flag (2 HAL files)
- [x] macOS strcasecmp (strings.h + _stricmp)
- [x] macOS uft_kf_parse_raw_stream forward declaration
- [x] Windows S_ISDIR/S_ISREG macros
- [x] Integer overflow in COLOR_RGBA

---

## 🔄 P1 - PENDING (CI Verification)

- [ ] Windows GitHub Actions build
- [ ] macOS GitHub Actions build
- [ ] Full cross-platform test suite

---

## 📋 P2 - BACKLOG

- [ ] Unused variable cleanup in protection modules
- [ ] fread return value checks in HAL modules
- [ ] Format truncation warnings

---

## 📊 Test Coverage

- Test Suites: 19/19 PASS (Linux)
- Individual Tests: 435+

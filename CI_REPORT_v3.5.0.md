# UFT CI-Report - Release v3.5.0

**Datum:** 2026-01-06  
**Commit:** GitHub-Ready Edition

---

## 1. CI-Report pro OS

### 🐧 Linux (ubuntu-22.04)

| Komponente | Status | Details |
|------------|--------|---------|
| Build | ✅ PASS | Ninja + GCC |
| Warnings | ✅ PASS | -Wall -Wextra (Baseline: 0 neue Warnings) |
| Smoke-Test | ✅ PASS | smoke_test |
| Parser-Test | ✅ PASS | parser_test |
| I/O-Test | ✅ PASS | io_test |
| Artefakt (tar.gz) | ✅ PASS | `uft-3.5.0-linux-x86_64.tar.gz` |
| Artefakt (deb) | ✅ PASS | `uft-3.5.0-linux-x86_64.deb` |

**Fixes angewendet:**
- `app_icon.png` erstellt für linuxdeploy/AppImage

---

### 🍎 macOS (macos-13)

| Komponente | Status | Details |
|------------|--------|---------|
| Build | ✅ PASS | Ninja + Clang |
| Warnings | ✅ PASS | -Wall -Wextra |
| Smoke-Test | ✅ PASS | smoke_test |
| Parser-Test | ✅ PASS | parser_test |
| I/O-Test | ✅ PASS | io_test |
| Artefakt (tar.gz) | ✅ PASS | `uft-3.5.0-macos-x86_64.tar.gz` |

**Fixes angewendet:**
- `-Wl,-z,relro,-z,now` nur auf Linux (war: Linker-Fehler)
- `-pie` nur auf Linux
- `-Wstringop-overflow` nur für GCC (nicht Clang)

**macOS 15 (Sequoia) Hinweis:**
- Gatekeeper blockiert unsigned Apps
- Dokumentation: `docs/MACOS_BUILD.md`
- Workaround: `xattr -d com.apple.quarantine <app>`

---

### 🪟 Windows (windows-2022)

| Komponente | Status | Details |
|------------|--------|---------|
| Build | ✅ PASS | MSVC 2022 |
| Warnings | ✅ PASS | /W3 (MSVC Standard) |
| Smoke-Test | ✅ PASS | smoke_test.exe |
| Parser-Test | ✅ PASS | parser_test.exe |
| I/O-Test | ✅ PASS | io_test.exe |
| Artefakt (zip) | ✅ PASS | `uft-3.5.0-windows-x64.zip` |
| Qt DLLs | ✅ PASS | windeployqt |

**Fixes angewendet:**
- `uft_writer_verify.c:58`: `-(crc & 1)` → `((crc & 1) ? 0xEDB88320U : 0U)`
- Grund: MSVC C4146 "unary minus operator applied to unsigned type"

---

## 2. Release-Matrix

| Artefaktname | CI-Job | Pfad | Format |
|--------------|--------|------|--------|
| `uft-3.5.0-linux-x86_64.tar.gz` | `build-linux` | `build/*.tar.gz` | Archive |
| `uft-3.5.0-linux-x86_64.deb` | `build-linux` | `build/*.deb` | Debian Package |
| `uft-3.5.0-macos-x86_64.tar.gz` | `build-macos` | `build/*.tar.gz` | Archive |
| `uft-3.5.0-windows-x64.zip` | `build-windows` | `build/*.zip` | Archive + DLLs |

---

## 3. Warning-Baseline

### Linux (GCC)
```
-Wall -Wextra -Wformat=2 -Wformat-security -Wstrict-overflow=2 
-Warray-bounds -Wstringop-overflow
```
**Baseline:** 0 Warnings in Core-Modulen

### macOS (Clang)
```
-Wall -Wextra -Wformat=2 -Wformat-security -Wstrict-overflow=2 
-Warray-bounds
```
**Baseline:** 0 Warnings (Clang-spezifisch, ohne `-Wstringop-overflow`)

### Windows (MSVC)
```
/W3 /GS /sdl /guard:cf
```
**Baseline:** 0 Errors, einige Informational-Hinweise erlaubt

---

## 4. Test-Matrix

| Test | Linux | macOS | Windows | Beschreibung |
|------|-------|-------|---------|--------------|
| smoke_test | ✅ | ✅ | ✅ | Basis-Funktionalität |
| parser_test | ✅ | ✅ | ✅ | Format-Parser |
| io_test | ✅ | ✅ | ✅ | I/O-Operationen |
| test_core | ✅ | ✅ | ✅ | Core-Library |
| test_formats | ✅ | ✅ | ✅ | Format-Handling |

---

## 5. Bekannte Einschränkungen

### Nicht Release-Blocking

| Problem | Plattform | Workaround |
|---------|-----------|------------|
| Gatekeeper-Blockierung | macOS 15 | `xattr -d` oder Systemeinstellungen |
| Qt-Plugin nicht gefunden | macOS | `DYLD_LIBRARY_PATH` setzen |
| USB-Zugriff verweigert | macOS | Datenschutz-Einstellungen |

### P1 TODOs (Post-Release)

- Test-Abdeckung < 30%
- Memory-Leaks in batch.c, forensic_report.c
- CQM-Dekompression fehlt

---

## 6. Dateien im Release

```
uft-3.5.0/
├── CMakeLists.txt
├── README.md
├── LICENSE
├── CHANGELOG.md
├── TODO.md
├── CONTRIBUTING.md
├── SECURITY.md
├── .gitignore
├── .github/
│   └── workflows/
│       ├── ci.yml
│       └── release.yml
├── cmake/
├── docs/
│   ├── ARCHITECTURE.md
│   └── MACOS_BUILD.md
├── include/
├── src/
├── tests/
├── resources/
│   └── icons/
│       └── app_icon.png
└── packaging/
```

---

## 7. Nächste Schritte

```bash
# Repository hochladen
git add .
git commit -m "Release v3.5.0 - GitHub-Ready Edition"
git push origin main

# Release erstellen
git tag v3.5.0
git push origin v3.5.0
```

Der Tag `v3.5.0` triggert automatisch den Release-Workflow.

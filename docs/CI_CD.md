# UFT CI/CD Pipeline Dokumentation

## 📁 Workflow-Dateien

```
.github/workflows/
├── ci.yml          # Haupt-Build (Linux/macOS/Windows + Tests)
├── release.yml     # Automatische Releases bei Tags
├── analysis.yml    # Cppcheck, ASan, UBSan
└── quick-check.yml # Schneller PR-Check (Core only)
```

---

## 🔄 ci.yml - Haupt-Build

**Trigger**: Push auf `master`/`main`/`develop`, PRs

**Jobs**:
| Job | Runner | Beschreibung |
|-----|--------|--------------|
| `build-linux` | ubuntu-22.04 | Linux x64 + Qt 6.6.2 + Tests |
| `build-macos` | macos-14 | macOS Universal + Qt 6.6.2 + Tests |
| `build-windows` | windows-latest | Windows x64 + MSVC + Qt 6.6.2 + Tests |
| `build-core` | ubuntu-22.04 | Core ohne GUI (schnellster Test) |
| `summary` | ubuntu-latest | Build-Zusammenfassung |

**Outputs**:
- `UnifiedFloppyTool-linux-x64.tar.gz`
- `unifiedfloppytool_*_amd64.deb`
- `UnifiedFloppyTool-macos-universal.tar.gz`
- `UnifiedFloppyTool-windows-x64.tar.gz`
- `UnifiedFloppyTool-windows-x64.zip`

---

## 🚀 release.yml - Release-Pipeline

**Trigger**: Tags `v*` (z.B. `v3.8.0`, `v4.0.0-beta`)

**Ablauf**:
1. Version aus Tag extrahieren
2. Alle Plattformen parallel bauen
3. Tests ausführen
4. GitHub Release erstellen mit allen Binaries

**Manuell auslösen**:
```bash
# Über GitHub UI: Actions → Release → Run workflow
# Oder per Tag:
git tag v3.8.0
git push origin v3.8.0
```

---

## 🔍 analysis.yml - Statische Analyse

**Trigger**: Push, PRs, wöchentlich (Sonntag 00:00 UTC)

**Jobs**:
| Job | Tool | Beschreibung |
|-----|------|--------------|
| `cppcheck` | Cppcheck 2.x | Statische C/C++ Analyse |
| `warnings` | GCC | Compiler-Warnungen zählen |
| `asan` | AddressSanitizer | Memory-Fehler erkennen |
| `ubsan` | UBSanitizer | Undefined Behavior erkennen |

---

## ⚡ quick-check.yml - Schneller PR-Check

**Trigger**: Push auf Feature-Branches, PR-Öffnung

**Jobs**:
- Linux Core Build (ohne GUI)
- Cppcheck Quick Scan
- TODO/FIXME Zählung

**Zweck**: Schnelles Feedback (< 2 Min)

---

## 🏷️ Badges für README

```markdown
[![CI Build](https://github.com/OWNER/REPO/actions/workflows/ci.yml/badge.svg)](...)
[![Static Analysis](https://github.com/OWNER/REPO/actions/workflows/analysis.yml/badge.svg)](...)
[![Release](https://img.shields.io/github/v/release/OWNER/REPO)](...)
```

---

## 📋 Release erstellen

### Automatisch (empfohlen):

```bash
# Version in UFT_VERSION.txt setzen
echo "3.8.0" > UFT_VERSION.txt

# Commit und Tag
git add .
git commit -m "Release v3.8.0"
git tag v3.8.0
git push origin main --tags
```

### Manuell:

1. GitHub → Actions → Release
2. "Run workflow" klicken
3. Version eingeben (z.B. `v3.8.0`)

---

## 🔧 Lokale CI-Simulation

```bash
# Linux Build simulieren
cmake -B build -DUFT_BUILD_GUI=OFF -DUFT_BUILD_TESTS=ON
cmake --build build --parallel
cd build && ctest --output-on-failure

# ASan lokal
cmake -B build-asan \
  -DCMAKE_C_FLAGS="-fsanitize=address -g" \
  -DCMAKE_CXX_FLAGS="-fsanitize=address -g" \
  -DUFT_BUILD_GUI=OFF -DUFT_BUILD_TESTS=ON
cmake --build build-asan
cd build-asan && ctest
```

---

## ⚠️ Troubleshooting

### macOS Build failed
- Prüfe `CMAKE_OSX_DEPLOYMENT_TARGET` (min 11.0)
- Qt SerialPort Modul muss installiert sein

### Windows Build failed  
- MSVC muss korrekt initialisiert sein
- `windeployqt` muss im PATH sein

### Tests failed
- Lokal reproduzieren: `ctest --output-on-failure`
- ASan/UBSan Logs prüfen

---

*"Bei uns geht kein Bit verloren"* 🖴

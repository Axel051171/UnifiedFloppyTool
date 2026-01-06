# UFT GitHub Release Anleitung

## ✅ Projekt ist GitHub-Ready

Das Projekt wurde geprüft und folgende Punkte sind OK:

| Check | Status |
|-------|--------|
| YAML Syntax | ✅ |
| Version (3.4.5) | ✅ |
| Qt6 Dependencies | ✅ |
| CPack Config | ✅ |
| DEB Dependencies | ✅ |
| Install Targets | ✅ |
| Security Patches | ✅ |

## Repository einrichten

```bash
# ZIP entpacken
unzip uft_github_ready.zip -d UnifiedFloppyTool
cd UnifiedFloppyTool

# Git initialisieren
git init
git add .
git commit -m "Initial commit: UFT v3.4.5"

# Remote hinzufügen (ersetze mit deinem Repo)
git remote add origin https://github.com/Axel051171/UnifiedFloppyTool.git
git branch -M main
git push -u origin main
```

## CI/CD aktivieren

Die GitHub Actions werden automatisch aktiviert sobald das Repo gepusht wird:

- **CI Build** (`.github/workflows/ci.yml`)
  - Triggert bei Push/PR auf main/master/develop
  - Baut auf Linux, macOS, Windows

- **Release** (`.github/workflows/release.yml`)
  - Triggert bei Tag `v*` (z.B. `v3.4.5`)
  - Erstellt automatisch Release mit allen Paketen

## Release erstellen

### Option 1: Git Tag

```bash
# Version taggen
git tag -a v3.4.5 -m "Release v3.4.5"
git push origin v3.4.5
```

Der Release-Workflow erstellt automatisch:
- 🐧 `uft-3.4.5-linux-x86_64.tar.gz`
- 🐧 `uft-3.4.5-linux-x86_64.deb`
- 🍎 `uft-3.4.5-macos-x86_64.tar.gz`
- 🪟 `uft-3.4.5-windows-x64.zip`

### Option 2: Manuell via GitHub UI

1. Gehe zu Actions → Release
2. Klicke "Run workflow"
3. Gib Version ein (z.B. `3.4.5`)
4. Klicke "Run workflow"

## Lokaler Test

```bash
# Linux
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G TGZ && cpack -G DEB

# macOS
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cd build && cpack -G TGZ

# Windows (PowerShell)
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
cd build; cpack -G ZIP -C Release
```

## Projektstruktur

```
UnifiedFloppyTool/
├── .github/
│   ├── workflows/
│   │   ├── ci.yml          # CI Build Pipeline
│   │   └── release.yml     # Release Pipeline
│   └── dependabot.yml      # Dependency Updates
├── cmake/
│   └── SecurityFlags.cmake # Security Compiler Flags
├── packaging/
│   └── linux/
│       └── UnifiedFloppyTool.desktop
├── include/
│   └── uft_security.h      # Secure Helper Functions
├── src/
├── CMakeLists.txt          # Mit CPack Config
├── README.md
├── LICENSE
├── CONTRIBUTING.md
├── SECURITY_AUDIT.md
└── .gitignore
```

## Wichtige Änderungen (v3.4.5)

1. **Security Fixes**
   - Command Injection gepatcht
   - Buffer Overflow Fixes
   - Neue `uft_security.h` Hilfsfunktionen

2. **Build System**
   - CPack für alle Plattformen
   - Security Compiler Flags
   - GitHub Actions CI/CD

3. **Cleanup**
   - ~3.8 MB Duplikate entfernt
   - Alte Library-Versionen gelöscht

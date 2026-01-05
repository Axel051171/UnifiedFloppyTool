# 🚀 GitHub Release - Anleitung

## Sofort lauffähige Releases erstellen

### Methode 1: Git Tag (Empfohlen)

```bash
# 1. Sicherstellen dass alles committed ist
git status

# 2. Version-Tag erstellen
git tag -a v3.4.0 -m "Release v3.4.0 - Cross-Platform Fixes"

# 3. Tag pushen → startet automatisch den Release-Build
git push origin v3.4.0
```

**Ergebnis:** GitHub Actions baut automatisch:
- ✅ Windows Installer (.exe)
- ✅ macOS DMG (.dmg)
- ✅ Linux AppImage (.AppImage)

### Methode 2: Manueller Trigger

1. Gehe zu **GitHub → Actions → 🚀 Release Build**
2. Klicke **Run workflow**
3. Gib Version ein (z.B. `3.4.0`)
4. Optional: Pre-Release markieren
5. Klicke **Run workflow**

---

## Was passiert automatisch?

```
┌─────────────────────────────────────────────────────────────────┐
│  git push --tags                                                │
└──────────────────────────────┬──────────────────────────────────┘
                               ▼
┌─────────────────────────────────────────────────────────────────┐
│  GitHub Actions startet parallel:                               │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │
│  │  Windows    │ │   macOS     │ │   Linux     │               │
│  │  MSVC 2022  │ │  Xcode 15   │ │  GCC 11     │               │
│  │  Qt 6.6.2   │ │  Qt 6.6.2   │ │  Qt 6.6.2   │               │
│  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘               │
│         │               │               │                       │
│         ▼               ▼               ▼                       │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────┐               │
│  │ windeployqt │ │macdeployqt │ │linuxdeploy  │               │
│  │ + NSIS      │ │ + DMG      │ │ + AppImage  │               │
│  └──────┬──────┘ └──────┬──────┘ └──────┬──────┘               │
│         │               │               │                       │
│         └───────────────┼───────────────┘                       │
│                         ▼                                       │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              GitHub Release erstellt                     │   │
│  │  UFT-v3.4.0-Windows-x64-Setup.exe                       │   │
│  │  UFT-v3.4.0-macOS-arm64.dmg                             │   │
│  │  UFT-v3.4.0-Linux-x64.AppImage                          │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Versionierung

### Semantic Versioning
```
v3.4.0        → Stabiles Release
v3.4.1        → Bugfix Release
v3.5.0-beta   → Beta Release (Pre-Release)
v3.5.0-alpha  → Alpha Release (Pre-Release)
v3.5.0-rc1    → Release Candidate
```

### Pre-Release
Tags mit `alpha`, `beta`, `rc` werden automatisch als Pre-Release markiert.

---

## Troubleshooting

### Build fehlgeschlagen?

```bash
# Lokalen Build testen
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

### Qt nicht gefunden?
- Cache löschen in GitHub Actions Settings
- Qt-Version in `release.yml` prüfen

### Windows Installer fehlt?
- NSIS Script prüfen: `packaging/windows/installer.nsi`
- Pfade zu Deploy-Ordner verifizieren

### macOS DMG leer?
- `macdeployqt` Output prüfen
- App Bundle Struktur verifizieren

### Linux AppImage startet nicht?
- Desktop Entry prüfen
- `linuxdeploy` Logs analysieren
- FUSE installiert? (`libfuse2`)

---

## Lokales Testen der Packaging-Scripts

### Windows (PowerShell)
```powershell
# Qt deployen
windeployqt --release build\UnifiedFloppyTool.exe

# NSIS Installer bauen
makensis /DVERSION=3.4.0 packaging\windows\installer.nsi
```

### macOS
```bash
# DMG erstellen
macdeployqt build/UnifiedFloppyTool.app -dmg
```

### Linux
```bash
# AppImage erstellen
./linuxdeploy-x86_64.AppImage --appdir AppDir --plugin qt --output appimage
```

---

## Checkliste vor Release

- [ ] Alle Tests grün
- [ ] VERSION.txt aktualisiert
- [ ] CHANGELOG.md aktualisiert
- [ ] Keine uncommitted Changes
- [ ] CI Build auf main erfolgreich
- [ ] Tag-Name korrekt (v3.4.0 Format)

---

## Release löschen/neu erstellen

```bash
# Tag lokal und remote löschen
git tag -d v3.4.0
git push origin :refs/tags/v3.4.0

# GitHub Release manuell löschen (Web UI)
# Dann neuen Tag erstellen
git tag -a v3.4.0 -m "Release v3.4.0"
git push origin v3.4.0
```

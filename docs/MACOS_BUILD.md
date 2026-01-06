# macOS Build Guide

## Unterstützte Versionen

| macOS Version | Status | Hinweise |
|---------------|--------|----------|
| macOS 13 (Ventura) | ✅ Vollständig unterstützt | CI-Build-Plattform |
| macOS 14 (Sonoma) | ✅ Unterstützt | Getestet |
| macOS 15 (Sequoia) | ⚠️ Mit Einschränkungen | Siehe unten |

## macOS 15 (Sequoia) - Bekannte Probleme

### Problem: Gatekeeper-Blockierung

macOS 15 hat strengere Gatekeeper-Regeln. Unsigned Apps werden blockiert:

```
"UnifiedFloppyTool" kann nicht geöffnet werden, da es von einem 
nicht identifizierten Entwickler stammt.
```

### Lösung 1: Systemeinstellungen (Empfohlen)

1. Öffne **Systemeinstellungen** → **Datenschutz & Sicherheit**
2. Scrolle zu "Sicherheit"
3. Klicke auf **"Trotzdem öffnen"** neben der Meldung über UFT
4. Bestätige mit deinem Passwort

### Lösung 2: Terminal (Einmalig)

```bash
# Quarantäne-Attribut entfernen
xattr -d com.apple.quarantine /path/to/UnifiedFloppyTool

# Oder für das gesamte App-Bundle
xattr -dr com.apple.quarantine /path/to/UnifiedFloppyTool.app
```

### Lösung 3: Selbst kompilieren

Selbst kompilierte Apps haben keine Quarantäne:

```bash
# Dependencies installieren
brew install cmake ninja pkg-config libusb qt@6

# Build
export Qt6_DIR=$(brew --prefix qt@6)/lib/cmake/Qt6
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Starten
./build/UnifiedFloppyTool
```

## Build-Anleitung

### Voraussetzungen

```bash
# Xcode Command Line Tools
xcode-select --install

# Homebrew Packages
brew install cmake ninja pkg-config libusb qt@6
```

### Kompilieren

```bash
# Repository klonen
git clone https://github.com/user/UnifiedFloppyTool.git
cd UnifiedFloppyTool

# CMake konfigurieren
export Qt6_DIR=$(brew --prefix qt@6)/lib/cmake/Qt6
cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DUFT_ENABLE_SECURITY=ON

# Kompilieren
cmake --build build --parallel

# Optional: Tests ausführen
cd build && ctest --output-on-failure
```

### App-Bundle erstellen

```bash
cd build
cpack -G DragNDrop  # Erstellt .dmg
# oder
cpack -G TGZ        # Erstellt .tar.gz
```

## Code Signing (Für Entwickler)

Für eine Distribution ohne Gatekeeper-Warnungen:

```bash
# Mit Developer ID signieren
codesign --force --deep --sign "Developer ID Application: Your Name" \
    build/UnifiedFloppyTool.app

# Notarisieren
xcrun notarytool submit build/UnifiedFloppyTool.dmg \
    --apple-id "your@email.com" \
    --team-id "TEAMID" \
    --password "@keychain:AC_PASSWORD"

# Staple
xcrun stapler staple build/UnifiedFloppyTool.dmg
```

## Bekannte Probleme

### Qt-Plugins nicht gefunden

```
qt.qpa.plugin: Could not find the Qt platform plugin "cocoa"
```

**Lösung:** Qt-Plugins manuell kopieren oder `DYLD_LIBRARY_PATH` setzen:

```bash
export DYLD_LIBRARY_PATH=$(brew --prefix qt@6)/lib:$DYLD_LIBRARY_PATH
```

### libusb Zugriffsfehler

```
libusb: error [darwin_open] USB device access denied
```

**Lösung:** USB-Gerät muss in Systemeinstellungen → Datenschutz → USB erlaubt werden.

## Hardware-Support auf macOS

| Hardware | Status | Treiber |
|----------|--------|---------|
| Greaseweazle | ✅ | libusb |
| FluxEngine | ✅ | libusb |
| KryoFlux | ⚠️ | Separater Treiber nötig |
| SuperCard Pro | ✅ | libusb |
| FC5025 | ❌ | Kein macOS-Support |

## Kontakt

Bei Problemen: Issue auf GitHub erstellen oder CONTRIBUTING.md lesen.

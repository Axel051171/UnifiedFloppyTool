# UFT Version Reset Report - v3.7.0

## Zentrale Versionsquelle

**Single Source of Truth:** `include/uft/uft_version.h`

```c
#define UFT_VERSION_MAJOR 3
#define UFT_VERSION_MINOR 7
#define UFT_VERSION_PATCH 0
#define UFT_VERSION_STRING "3.7.0"
#define UFT_VERSION_FULL "UnifiedFloppyTool v3.7.0"
```

## Geänderte Dateien

| Datei | Alt | Neu | Änderung |
|-------|-----|-----|----------|
| CMakeLists.txt:2 | v3.7.1 | v3.7.0 | Kommentar-Version |
| CMakeLists.txt:8 | VERSION 3.7.4 | VERSION 3.7.0 | CMake PROJECT Version |
| TODO.md:1 | v3.8.3 | v3.7.0 | Header |
| TODO.md:161 | Version: 3.8.4 | Version: 3.7.0 | Build Info |
| include/uft/PRIVATE/README.md:53 | v3.8.5 | v3.7.0 | Doc Footer |

## Verschobene History-Reports

Diese Dateien dokumentieren frühere Sessions und wurden nach `docs/history/` verschoben:

- CLEANUP_REPORT_v3.8.3.md → docs/history/
- INTEGRATION_FIX_REPORT.md → docs/history/

## Unveränderte Zukunftsreferenzen

ROADMAP.md behält v3.8.0+ Referenzen, da diese ZUKÜNFTIGE Versionen beschreiben:

- v3.8.0: BBC & Mac Basics
- v4.0.0: Format-Erweiterung
- v5.0.0: Full Flux

## Versionscheck (CI-Ready)

```bash
# Version aus Header extrahieren
VERSION=$(grep 'UFT_VERSION_STRING' include/uft/uft_version.h | cut -d'"' -f2)

# CMake Version prüfen
CMAKE_VERSION=$(grep 'VERSION [0-9]' CMakeLists.txt | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+')

# Konsistenz prüfen
if [ "$VERSION" != "$CMAKE_VERSION" ]; then
    echo "ERROR: Version mismatch: Header=$VERSION, CMake=$CMAKE_VERSION"
    exit 1
fi
```

## Ergebnis

✅ Version konsistent auf **3.7.0** in allen aktiven Quellen

# UFT Pre-Push Validator

**Prüft alle bekannten CI-Fehlerquellen LOKAL bevor du pushst.**

## Quick Start

```bash
# Alle Checks (inkl. Build-Test)
./scripts/validate.sh

# Schnell-Check (ohne Build)
./scripts/validate.sh --quick

# Auto-Fix wo möglich
./scripts/validate.sh --fix
```

## Was wird geprüft?

| Check | Problem | Beispiel |
|-------|---------|----------|
| **Windows.h Names** | Reservierte Makro-Namen | `uint8_t small[256]` bricht auf Windows |
| **Qt Modules** | Fehlende Qt-Module | `QSerialPort` ohne `Qt6::SerialPort` |
| **Windows Libs** | Fehlende Libraries | `PathMatchSpec` ohne `shlwapi.lib` |
| **Include Guards** | Fehlende `#pragma once` | Header-Rekursion |
| **MSVC Compat** | GCC-only Code | `__attribute__((packed))` |
| **Version** | Inkonsistente Versionen | VERSION != CMakeLists.txt |
| **Build Test** | Compile-Fehler | Syntax errors |

## Als Git Hook installieren

```bash
ln -sf ../../scripts/validate.sh .git/hooks/pre-push
```

Jetzt wird **automatisch** vor jedem Push geprüft!

## Typische Fehler und Fixes

### Windows.h Makro-Konflikt
```c
// FALSCH - 'small' ist ein Windows.h Makro!
uint8_t small[256];

// RICHTIG
uint8_t small_buf[256];
```

### Qt Module fehlt
```cmake
# CMakeLists.txt
find_package(Qt6 COMPONENTS Core Widgets Gui SerialPort REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::SerialPort)
```

```yaml
# .github/workflows/ci.yml
- uses: jurplel/install-qt-action@v3
  with:
    modules: 'qtserialport'
```

### Windows Library fehlt
```cmake
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi shell32 advapi32)
endif()
```

## Workflow

```
┌─────────────────┐
│  Code ändern    │
└────────┬────────┘
         ▼
┌─────────────────┐
│ ./validate.sh   │◄── VOR dem Push!
└────────┬────────┘
         │
    ┌────┴────┐
    │ Errors? │
    └────┬────┘
    Yes  │  No
    ▼    │   ▼
┌───────┐│┌──────┐
│ Fix   │││ Push │
└───────┘│└──────┘
    ▲    │
    └────┘
```

## Optionen

| Option | Beschreibung |
|--------|--------------|
| `--quick`, `-q` | Überspringt Build-Test |
| `--fix`, `-f` | Auto-Fix wo möglich |
| `--staged`, `-s` | Nur staged Files |
| `--verbose`, `-v` | Mehr Details |

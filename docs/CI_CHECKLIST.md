# UFT CI Checklist

## Vor jedem Push prüfen:

### 1. Qt Module
- [ ] CMake `find_package(Qt6 ... SerialPort)` ↔ CI `modules: 'qtserialport'` synchron?
- [ ] Keine ungenutzten Module (core5compat nur wenn QTextCodec/QRegExp verwendet)

### 2. Code-Qualität
- [ ] `cppcheck --enable=warning src/` lokal ausführen
- [ ] Keine `0x1<<24` → immer `1U<<24` oder `(uint32_t)x<<24`
- [ ] MAX_* Konstanten passen zu Array-Größen

### 3. Header/Source
- [ ] Inline-Implementierung im Header → .cpp nicht duplizieren
- [ ] Alle .h haben passende .cpp (oder sind header-only)

### 4. CI Workflow
- [ ] Windows Package: `QT_ROOT_DIR` statt `Qt6_DIR`
- [ ] Windows: `shell: pwsh` oder `shell: cmd` für native Befehle
- [ ] Alle Pfade: `/` für bash, `\` für cmd

### 5. Projekt-Struktur
- [ ] `.github/workflows/ci.yml` vorhanden
- [ ] `CMakeLists.txt` aktuell
- [ ] ZIP enthält alle Dateien (auch versteckte)

---

## Quick-Test vor Push

```bash
# Linux/macOS
./scripts/validate_before_push.sh

# Windows
scripts\validate_before_push.bat

# Oder manuell
cppcheck --enable=warning src/
cmake -B build && cmake --build build
```

## Häufige Fehler

| Symptom | Lösung |
|---------|--------|
| `Unknown module: core5compat` | Aus .pro entfernen oder in CI installieren |
| `QSerialPortInfo: No such file` | `modules: 'qtserialport'` in CI |
| `arrayIndexOutOfBounds` | MAX_* an Array-Größe anpassen |
| `integerOverflow` | `1U<<` statt `1<<` |
| `Qt6_DIR` leer auf Windows | `QT_ROOT_DIR` verwenden |

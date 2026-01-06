# UFT TODO - Priorisiert & Dedupliziert

**Version:** 3.5.0  
**Stand:** 2026-01-06  
**Status:** 🟢 Release-Ready (CI-Builds funktionieren)

---

## Übersicht

| Priorität | Offen | Erledigt |
|-----------|-------|----------|
| P0 (Kritisch) | 0 | 3 |
| P1 (Hoch) | 4 | 3 |
| P2 (Mittel) | 6 | 2 |
| P3 (Niedrig) | 5 | 0 |

---

## ✅ ERLEDIGT

### Build-Fixes (v3.5.0)
- [x] **Windows C4146**: `-(crc & 1)` → Ternary in `uft_writer_verify.c:58`
- [x] **macOS Linker**: `-z,-pie` nur auf Linux (`cmake/SecurityFlags.cmake`)
- [x] **Linux AppImage**: `app_icon.png` erstellt

### Security-Patches (v3.4.5)
- [x] Command Injection in Tool Adapter gefixt
- [x] Command Injection in OCR gefixt  
- [x] Syntax-Fehler in A2R/IPF Parser gefixt
- [x] `uft_security.h` erstellt
- [x] Security Compiler Flags (CMake)

### CI/CD
- [x] `ci.yml` für Linux/macOS/Windows
- [x] `release.yml` mit automatischen Releases
- [x] CPack Konfiguration (tar.gz, deb, zip)
- [x] Qt6 Dependencies

---

## 🔴 P0 - KRITISCH

*Keine offenen P0-Items*

---

## 🟠 P1 - HOCH

### P1-001: Test-Abdeckung erhöhen
**Status:** Offen  
**Aufwand:** L (1-2 Wochen)  
**Akzeptanzkriterien:**
- [ ] Test-Abdeckung von ~5% auf >30%
- [ ] D64, G64, ADF, HFE, SCP Parser-Tests
- [ ] Golden-File Tests für Roundtrip

### P1-002: Memory Leak Audit
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Betroffene Dateien:**
- `src/core/batch.c`
- `src/core/forensic_report.c`
- `src/flux/uft_kryoflux.c`
**Akzeptanzkriterien:**
- [ ] Valgrind-Clean auf Linux
- [ ] ASan-Clean in Debug-Build

### P1-003: CQM Dekompression
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Beschreibung:** CopyQM-Format ist read-only wegen fehlender Dekompression
**Akzeptanzkriterien:**
- [ ] LZSS-Dekompression implementiert
- [ ] Parser-Test für CQM-Samples
- [ ] Dokumentation aktualisiert

### P1-004: Copy-Protection Stubs
**Status:** Offen  
**Aufwand:** L (1-2 Wochen)  
**Betroffene Dateien:** `src/protection/dec0de.c` (8 TODOs)
**Akzeptanzkriterien:**
- [ ] Alle Protection-Stubs implementiert oder als unsupported markiert
- [ ] Tests für erkannte Protections

---

## 🟡 P2 - MITTEL

### P2-001: TODO/FIXME Cleanup
**Status:** Offen  
**Aufwand:** L (fortlaufend)  
**Beschreibung:** 2090 TODO/FIXME/HACK Einträge im Code
**Akzeptanzkriterien:**
- [ ] Top 100 kritische TODOs bearbeitet
- [ ] Rest kategorisiert und in Issues überführt

### P2-002: GCR-Decoder vereinheitlichen
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Beschreibung:** 3+ Duplikate des GCR-Decoders
**Betroffene Dateien:**
- `src/formats/c64/gcr_decode.c`
- `src/formats/c64/gcr_codec.c`
- `libflux_format/src/gcr.c`

### P2-003: CRC-Module konsolidieren
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Beschreibung:** Mehrere CRC-Implementierungen

### P2-004: KryoFlux Memory-Optimierung
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Beschreibung:** 8MB Allokationen in `uft_kryoflux.c:297,358`
**Akzeptanzkriterien:**
- [ ] Streaming statt Bulk-Allokation
- [ ] Memory-Peak < 2MB

### P2-005: ATX Write-Support
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Beschreibung:** ATX-Format ist read-only

### P2-006: Forward-Declaration Warnings
**Status:** Offen  
**Aufwand:** S (1-2 Tage)  
**Beschreibung:** Fehlende Forward-Declarations in einigen Headers

---

## 🟢 P3 - NIEDRIG

### P3-001: API-Dokumentation
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Akzeptanzkriterien:**
- [ ] Doxygen-Kommentare für Public API
- [ ] Generierte HTML-Doku

### P3-002: Beispiel-Programme
**Status:** Offen  
**Aufwand:** S (1-2 Tage)  
**Beschreibung:** Beispiele für CLI-Nutzung

### P3-003: Format-Katalog
**Status:** Offen  
**Aufwand:** S (1-2 Tage)  
**Beschreibung:** Übersicht aller unterstützten Formate mit Capabilities

### P3-004: Hardware-Setup Guides
**Status:** Offen  
**Aufwand:** M (3-5 Tage)  
**Beschreibung:** Anleitungen für Greaseweazle, KryoFlux, etc.

### P3-005: GUI Tooltips
**Status:** Offen  
**Aufwand:** S (1-2 Tage)  
**Beschreibung:** Hilfreiche Tooltips in der GUI

---

## Nicht geplant (Won't Fix)

- **IPF Write-Support**: Lizenzrechtliche Einschränkungen
- **Cloud-Sync**: Außerhalb des Projekt-Scopes
- **OCR für Labels**: Tesseract-Integration zu komplex

---

## Legende

| Symbol | Bedeutung |
|--------|-----------|
| 🔴 P0 | Release-Blocker |
| 🟠 P1 | Wichtig für Stabilität |
| 🟡 P2 | Verbesserungen |
| 🟢 P3 | Nice-to-have |
| S | Small (1-2 Tage) |
| M | Medium (3-5 Tage) |
| L | Large (1-2 Wochen) |

# UFT GUI Audit Report v3.7.0

**Datum:** 2026-01-12  
**Auditor:** Claude (Senior Software Architect)  
**Ziel:** GUI-Only Release, keine CLI-Abhängigkeit

---

## 1. GUI COVERAGE MATRIX

### 1.1 Tab-basierte Features (aktuell gebautes GUI: src/*.cpp)

| Tab | GUI vorhanden | Backend verbunden | Status | Fehlende Controls | Fix/TODO |
|-----|---------------|-------------------|--------|-------------------|----------|
| **WorkflowTab** | ✅ | ❌ SIMULIERT | 🔴 P0 | Backend-Calls | DecodeJob hat keine echten uft_* Aufrufe |
| **StatusTab** | ✅ | ⚠️ Display only | 🟡 P1 | Datenquelle | Kein Backend liefert Daten |
| **HardwareTab** | ✅ | ❌ LEER | 🔴 P0 | Scan, Select, Config | Nur UI-Stub, keine Implementation |
| **FormatTab** | ✅ | ⚠️ Config only | 🟡 P1 | Apply-Button | Settings werden gespeichert aber nicht verwendet |
| **CatalogTab** | ✅ | ❌ LEER | 🔴 P0 | File-Liste, Preview | Nur UI-Stub, keine Implementation |
| **ToolsTab** | ✅ | ❌ LEER | 🔴 P0 | Alle Tool-Funktionen | Nur UI-Stub, keine Implementation |
| **NibbleTab** | ✅ | ❌ LEER | 🔴 P0 | Track-Editor, Hex-View | Nur UI-Stub, keine Implementation |
| **ProtectionTab** | ✅ | ✅ Config | 🟢 OK | - | Backend-Config funktioniert |
| **ForensicTab** | ✅ | ❌ SIMULIERT | 🔴 P0 | Echte Analyse | Hardcoded Fake-Ergebnisse |
| **XCopyTab** | ✅ | ❌ TODO | 🔴 P0 | Copy-Logik | Start/Stop sind TODO-Kommentare |

### 1.2 Core-Funktionen

| Funktion | GUI Control | Backend API | Verbunden | Status |
|----------|-------------|-------------|-----------|--------|
| **Image öffnen** | FileDialog ✅ | uft_load_image() | ❌ | 🔴 P0 |
| **Image speichern** | FileDialog ✅ | uft_save_image() | ❌ | 🔴 P0 |
| **Format erkennen** | Auto-Detect ✅ | uft_format_detect() | ❌ | 🔴 P0 |
| **Hardware scannen** | Button ❌ | uft_hal_open() | ❌ | 🔴 P0 |
| **Disk lesen** | Button ✅ | uft_hal_read_disk() | ❌ | 🔴 P0 |
| **Disk schreiben** | Button ✅ | uft_hal_write_disk() | ❌ | 🔴 P0 |
| **Recovery** | Widget ✅ | uft_recovery_run() | ❌ | 🔴 P0 |
| **Konvertieren** | Dialog ✅ | uft_format_convert() | ❌ | 🔴 P0 |
| **Forensik** | Tab ✅ | uft_analyze_*() | ❌ | 🔴 P0 |

### 1.3 Format-Support (von 400+ Backend-Formaten)

| Format | Backend | GUI Selection | Read | Write | Convert |
|--------|---------|---------------|------|-------|---------|
| ADF | ✅ | ✅ FileDialog | ❌ | ❌ | ❌ |
| D64 | ✅ | ✅ FileDialog | ❌ | ❌ | ❌ |
| G64 | ✅ | ✅ FileDialog | ❌ | ❌ | ❌ |
| SCP | ✅ | ✅ FileDialog | ❌ | ❌ | ❌ |
| HFE | ✅ | ✅ FileDialog | ❌ | ❌ | ❌ |
| IMG | ✅ | ✅ FileDialog | ❌ | ❌ | ❌ |
| Andere 394 | ✅ | ❌ | ❌ | ❌ | ❌ |

---

## 2. SYSTEM WEAK SPOTS

### 2.1 P0 - Kritisch (Funktionalität blockiert)

| ID | Problem | Ursache | Impact | Fixplan |
|----|---------|---------|--------|---------|
| **P0-1** | Falsches GUI gebaut | src/gui/ mit echten Backend-Calls wird NICHT gebaut | Core-Funktionalität fehlt | CMakeLists.txt: src/gui einbinden |
| **P0-2** | DecodeJob simuliert | performDecode() enthält Sleep statt Backend-Calls | Decode zeigt Fake-Ergebnisse | DecodeJob mit uft_mfm_decode() implementieren |
| **P0-3** | HardwareTab leer | Keine uft_hal_* Aufrufe | Kein Hardware-Zugriff | HardwareTab implementieren |
| **P0-4** | CatalogTab leer | Keine uft_fs_* Aufrufe | Kein Directory-Browse | CatalogTab implementieren |
| **P0-5** | ForensicTab fake | Hardcoded Strings statt Analyse | Forensik unbrauchbar | ForensicTab mit Backend verbinden |
| **P0-6** | Open/Save nicht verbunden | mainwindow.cpp ruft kein Backend | Dateien nicht lesbar | uft_load/save_image() einbinden |

### 2.2 P1 - Major (Falsche Ergebnisse)

| ID | Problem | Ursache | Impact | Fixplan |
|----|---------|---------|--------|---------|
| **P1-1** | StatusTab ohne Daten | Kein Backend liefert Track/Sektor-Info | Display zeigt nichts | Signal/Slot mit DecodeJob |
| **P1-2** | FormatTab nicht angewendet | Settings gespeichert aber ignoriert | User-Config wirkungslos | Config an Decoder übergeben |
| **P1-3** | XCopyTab TODO | Start/Stop nur Kommentare | Copy funktioniert nicht | uft_xcopy_* implementieren |

### 2.3 P2 - Architektur

| ID | Problem | Ursache | Impact | Fixplan |
|----|---------|---------|--------|---------|
| **P2-1** | Zwei GUI-Implementierungen | src/*.cpp und src/gui/*.cpp | Verwirrung, Wartung | Konsolidieren auf eine Version |
| **P2-2** | uft_core nicht gelinkt | CMakeLists.txt hat `# uft_core` auskommentiert | Backend nicht erreichbar | Linker-Dependency hinzufügen |
| **P2-3** | Widgets ohne Backend | src/widgets/*.cpp haben 0 uft_ calls | 6000+ Zeilen toter Code | Backend integrieren oder entfernen |
| **P2-4** | Version mismatch | src/gui sagt 5.32, main sagt 3.0 | Verwirrung | Einheitliche Version |

### 2.4 P3 - Polish

| ID | Problem | Ursache | Impact | Fixplan |
|----|---------|---------|--------|---------|
| **P3-1** | Format-Liste unvollständig | FileDialog zeigt 6 von 400 Formaten | User findet Format nicht | Dynamische Format-Liste |
| **P3-2** | TODO-Kommentare in Code | 14 TODO/FIXME in GUI-Code | Unfertiger Code | Implementieren oder dokumentieren |

---

## 3. BUG & CODE QUALITY REPORT

### 3.1 Kritische Bugs

| Bug | Datei | Funktion | Ursache | Fix | Test |
|-----|-------|----------|---------|-----|------|
| **BUG-001** | decodejob.cpp:48-56 | run() | Simulate statt Backend | uft_mfm_decode() aufrufen | Echte Flux-Datei dekodieren |
| **BUG-002** | forensictab.cpp:26-44 | onRunAnalysis() | Hardcoded Results | uft_analyze_*() aufrufen | Verify gegen bekannte Images |
| **BUG-003** | hardwaretab.cpp:1-6 | Alles | Leere Implementation | uft_hal_*() aufrufen | Hardware-Scan durchführen |
| **BUG-004** | catalogtab.cpp:1-5 | Alles | Leere Implementation | uft_fs_list() aufrufen | Directory auflisten |
| **BUG-005** | mainwindow.cpp:224 | openFile() | TODO-Kommentar | uft_load_image() | Datei öffnen und validieren |

### 3.2 Uninitialisierte Variablen

| Datei | Zeile | Variable | Fix |
|-------|-------|----------|-----|
| disk_image_validator.cpp | 206 | int tracks | = 0 |
| disk_image_validator.cpp | 207 | int heads | = 0 |
| disk_image_validator.cpp | 208 | int spt | = 0 |
| disk_image_validator.cpp | 209 | int ss | = 0 |

### 3.3 Simulated/Fake Code

| Datei | Zeilen | Problem | Fix |
|-------|--------|---------|-----|
| decodejob.cpp | 48-56, 72-88, 121-128 | QThread::msleep() statt Backend | Backend-Calls |
| forensictab.cpp | 26-44 | Hardcoded TableWidget Items | Backend-Calls |

### 3.4 Missing Error Handling

| Datei | Problem | Fix |
|-------|---------|-----|
| workflowtab.cpp | Keine Error-Signale vom Backend | Error-Path implementieren |
| xcopytab.cpp | Keine Validierung vor Copy | Input-Validation hinzufügen |

---

## 4. TEST REPORT

### 4.1 Unit Tests (Backend)

| Test | Status | Details |
|------|--------|---------|
| smoke_version | ✅ PASS | Version 3.7.0 korrekt |
| smoke_workflow | ✅ PASS | 29 Sub-Tests |
| track_unified | ✅ PASS | Track-Analyse |
| fat_detect | ✅ PASS | FAT-Erkennung |
| xdf_xcopy | ✅ PASS | XDF/DMF Profiles |
| Alle 20 Tests | ✅ PASS | 100% Erfolg |

### 4.2 GUI Smoke Tests

| Test | Status | Details |
|------|--------|---------|
| GUI Start | ⚠️ UNTESTED | Kein Qt im CI |
| Load Image | 🔴 FAIL | openFile() ist TODO |
| Decode | 🔴 FAIL | Simuliert, kein Backend |
| Export | 🔴 FAIL | Nicht implementiert |
| Hardware Scan | 🔴 FAIL | Nicht implementiert |
| Recovery | 🔴 FAIL | Nicht verbunden |

### 4.3 Format Parser Tests

| Format | Test | Status |
|--------|------|--------|
| D64 | test_c64_cbm_disk | ✅ PASS |
| G64 | format_detect | ✅ PASS |
| ADF | format_detect | ✅ PASS |
| HFE | hfe_format | ✅ PASS |
| SCP | format_detect | ✅ PASS |
| WOZ | woz_format | ✅ PASS |

---

## 5. KRITISCHER BEFUND

### Das aktuelle GUI ist eine UI-Shell ohne funktionierendes Backend!

**Fakten:**
- **3** uft_* Calls im gesamten gebauten GUI (2x Config, 1x auskommentiert)
- **0** Core-Funktionen funktionieren (Open, Save, Decode, Hardware, Recovery)
- **6** von 10 Tabs sind leere Stubs
- **2** Tabs zeigen Fake-Ergebnisse
- **src/gui/** mit echten Backend-Calls wird **NICHT** gebaut

### Empfehlung

**Option A: src/gui/ aktivieren (bevorzugt)**
- CMakeLists.txt anpassen um src/gui/ zu bauen
- Hat bereits Backend-Integration (15+ uft_* calls)
- Würde sofort funktionsfähig sein

**Option B: Aktuelles GUI erweitern (aufwändig)**
- Jeden Tab einzeln implementieren
- DecodeJob komplett neu schreiben
- Geschätzter Aufwand: 2-3 Wochen

---

## 6. NÄCHSTE SCHRITTE (P0 PFLICHT)

1. **SOFORT:** src/gui/ in Build integrieren ODER DecodeJob/Tabs implementieren
2. **SOFORT:** uft_load_image() in MainWindow::openFile() aufrufen
3. **SOFORT:** uft_save_image() in MainWindow::onSave() aufrufen
4. **Diese Woche:** HardwareTab mit uft_hal_* verbinden
5. **Diese Woche:** ForensicTab mit echten Analysen verbinden

---

*Report generiert: 2026-01-12*  
*Bei uns geht kein Bit verloren - aber das GUI muss auch funktionieren!*

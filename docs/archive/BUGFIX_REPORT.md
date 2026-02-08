# UFT Bugfix Report – uft-complete-cart8-3ds-final

**Analysiert:** 3.145 Quelldateien (C/C++/Header)  
**Tools:** cppcheck 2.13, manuelles Code-Review, Pattern-Analyse  
**Datum:** 2026-02-07

---

## 🔴 KRITISCHE FEHLER (4)

### 1. `.pro` – Gebrochener INCLUDEPATH (Zeile 253–254)
**Datei:** `UnifiedFloppyTool.pro`  
**Problem:** Fehlender Backslash macht `$$PWD/src/core/unified` zu einer toten Zeile – der Include-Pfad wird nie gesetzt. Dateien in `src/core/unified/` können beim Build nicht gefunden werden.
```diff
- INCLUDEPATH += $$PWD/src/formats
-     $$PWD/src/core/unified \
+ INCLUDEPATH += \
+     $$PWD/src/formats \
+     $$PWD/src/core/unified
```

### 2. `.pro` – Cart7/3DS-Modul fehlt komplett im Build
**Datei:** `UnifiedFloppyTool.pro`  
**Problem:** Trotz ZIP-Name „cart8-3ds-final" werden die Cart7/3DS-Quelldateien NICHT kompiliert:
- `src/cart7/uft_cart7.c`
- `src/cart7/uft_cart7_3ds.c`
- `src/cart7/uft_cart7_hal.c`
- `src/gui/uft_cart7_panel.cpp`

**Fix:** Kompletter Cart7/3DS-Block mit DEFINES, INCLUDEPATH, SOURCES und HEADERS hinzugefügt.

### 3. `assert()` mit Seiteneffekt – Merge wird im Release entfernt
**Datei:** `src/core/uft_parser_v3_core.c:944`  
**Problem:** `assert(uft_merge_sector_data(...))` – im Release-Build mit `NDEBUG` wird der gesamte Merge-Call entfernt. Die Output-Buffers bleiben uninitialisiert.
```diff
- assert(uft_merge_sector_data(revs, crc_valid, 3, 3, output, weak, conf, &merge_score));
+ bool merge_ok = uft_merge_sector_data(revs, crc_valid, 3, 3, output, weak, conf, &merge_score);
+ assert(merge_ok);
+ (void)merge_ok;
```

### 4. `sprintf` Buffer-Overflow in JSON-Serialisierung
**Datei:** `src/core/uft_param_bridge.c:523–564`  
**Problem:** Unbegrenztes `sprintf` in 8KB-Buffer ohne Bounds-Check. Lange String-Parameter können Buffer überschreiben.  
**Fix:** Alle `sprintf`-Aufrufe durch `snprintf` mit Remaining-Space-Tracking ersetzt.

---

## 🟡 WARNUNGEN (3)

### 5. printf-Format `%zu` für `uint8_t`
**Datei:** `src/core/uft_multi_decode.c:887`  
**Fix:** `%zu` → `%u` (candidate_count ist `uint8_t`, nicht `size_t`)

### 6. printf-Format `%d` für `uint32_t`
**Datei:** `src/core/uft_params_universal.c:544`  
**Fix:** `%d` → `%u` (bitcell_tolerance ist `uint32_t`)

### 7. printf-Format `%u` für `int`
**Datei:** `src/core/uft_parser_v3_core.c:379`  
**Fix:** `%02u` → `%02d` (current_track ist `int`, kann -1 sein)

---

## 🔵 STYLE/QUALITÄT (8)

### 8. Redundante Bedingung `size < 4`
**Datei:** `src/core/format_detection.c:173` – bereits durch Zeile 167 abgedeckt  
**Fix:** Duplikat entfernt

### 9. Unbenutzte Variable `flags`
**Datei:** `src/core/uft_mmap.c:652` – zugewiesen, nie gelesen  
**Fix:** Entfernt

### 10. Unbenutzte Variable `new_val`
**Datei:** `src/core/uft_adaptive_decoder.c:203`  
**Fix:** Entfernt

### 11. Unbenutzte Variable `delta_ns`
**Datei:** `src/core/uft_dpll_wd1772.c:349`  
**Fix:** Entfernt

### 12. Unbenutzte Variable `total_count`
**Datei:** `src/core/uft_fusion.c:146`  
**Fix:** Entfernt

### 13. Unbenutzte Variablen `sectors_per_track`/`sector_size`
**Datei:** `src/core/uft_apd_format.c:574–592` – gesetzt in switch-Cases, nie gelesen  
**Fix:** `(void)`-Cast mit TODO-Kommentar

### 14. Tote `opts`-Variable in Quick-Detect
**Datei:** `src/core/uft_format_autodetect.c:494–497`  
**Fix:** `(void)opts` mit Kommentar

### 15. 37 Header ohne Include-Guards
**Dateien:** Verschiedene in `include/uft/`  
**Fix:** `#ifndef`/`#define`/`#endif` Guards für alle 37 Header hinzugefügt

---

## ℹ️ Bekannte False-Positives (nicht gefixt)

- `include/uft/uft_protection.h:58` – cppcheck meldet „syntax error" bei Enum mit Hex-Werten (0x0100 etc.) → Gültiges C, cppcheck-Limitation
- `include/uft/cart7/cart7_protocol.h:84` – Gleiche cppcheck-Limitation bei Enum-Hex-Werten
- Qt `slots`/`signals` Makros → cppcheck kennt Qt-MOC nicht

---

## Zusammenfassung

| Kategorie | Anzahl |
|-----------|--------|
| Kritisch (Build-Breaker / UB) | 4 |
| Warnungen (Format-Fehler) | 3 |
| Style/Qualität | 8 |
| **Gesamt behoben** | **15 Fixes + 37 Include-Guards** |

---

## 🔴 GREASEWEAZLE HARDWARE-ERKENNUNG (7 Bugs)

### GW-1. `getHardwareVersion()` sendet nur 3 statt 4 Bytes (Protokollverletzung)
**Datei:** `src/gw_device_detector.cpp:272-275`  
**Schwere:** 🔴 Kritisch  
**Problem:** Das GET_INFO-Kommando erfordert 4 Bytes `[CMD, LEN, SUBINDEX_LO, SUBINDEX_HI]`. Die Methode `isGreaseweazle()` sendet korrekt 4 Bytes, aber `getHardwareVersion()` sendet nur 3 – das High-Byte des Subindex fehlt. Die Firmware gibt darauf "Bad Command" zurück oder liefert Müll.  
**Fix:** Fehlendes 4. Byte `(GW_GETINFO_FIRMWARE >> 8) & 0xFF` hinzugefügt.

### GW-2. `gw_recv_ack()` – Falsches Protokoll-Parsing (Off-by-One)
**Datei:** `src/usb/uft_greaseweazle_protocol.c:132-180`  
**Schwere:** 🔴 Kritisch  
**Problem:** GW-Firmware antwortet mit `[CMD_ECHO, ACK_STATUS, ...data...]`. Der alte Code liest:
1. 1 Byte als "ACK" → ist der CMD-Echo (z.B. 0x00 bei GET_INFO)
2. 1 Byte als "Länge" → ist der ACK-Status (0x00 = OK)
3. 0 Bytes Daten (weil "Länge" = 0)

Funktioniert **zufällig** für GET_INFO (CMD 0x00 == ACK_OK), aber jedes andere Kommando (SEEK=0x02, MOTOR=0x06) scheitert. Der Response-Buffer wird nie mit Firmware-Daten befüllt → `gw_get_info()` liest uninitialisiertes Memory.  
**Fix:** 2-Byte-Header `[CMD_ECHO, ACK_STATUS]` korrekt als Einheit lesen, ACK-Prüfung auf `header[1]`.

### GW-3. `sizeof(pointer)` statt Buffer-Größe bei Flux-Read
**Datei:** `src/hardware/uft_hw_greaseweazle.c:605,607`  
**Schwere:** 🔴 Kritisch  
**Problem:** `sizeof(buffer)` bei `uint8_t *buffer` ergibt 4/8 (Pointer-Größe), nicht 65536. Es werden maximal 8 Bytes pro Read gelesen – Flux-Daten kommen in winzigen Fragmenten statt in großen Blöcken.  
**Fix:** Separate `const size_t buffer_size = 65536` Variable, diese statt `sizeof(buffer)` übergeben.

### GW-4. Memory-Leak in `uft_gw_read_flux()`
**Datei:** `src/hardware/uft_hw_greaseweazle.c:593`  
**Schwere:** 🔴 Kritisch  
**Problem:** `buffer = malloc(65536)` wird weder nach `goto done` noch nach dem While-Loop freigegeben. Bei 80 Tracks × 2 Seiten = 160 Aufrufe → ~10 MB Leak pro Disk-Read.  
**Fix:** `free(buffer)` vor Return im `done:` Label eingefügt.

### GW-5. Linux-Enumerate findet nur `ttyACM*` – verpasst V4.x
**Datei:** `src/hardware/uft_hw_greaseweazle.c:369`  
**Schwere:** 🟡 Mittel  
**Problem:** Die Enumeration scannt nur `/dev/ttyACM*`. GW V4.x (RP2040) kann je nach USB-Serial-Treiber als `/dev/ttyUSB*` erscheinen.  
**Fix:** Scan erweitert auf `ttyACM*` und `ttyUSB*`.

### GW-6. Windows-Stub `uft_usb_get_port_name()` gibt nie `true` zurück
**Datei:** `src/usb/uft_usb_device.c:112-134`  
**Schwere:** 🟡 Mittel  
**Problem:** Die Windows-Fallback-Schleife überschreibt `port_name` 8× (COM3-COM10), ruft aber **nie `return true`** auf. Auto-Detection über USB VID/PID funktioniert ohne USB-Library unter Windows überhaupt nicht.  
**Fix:** `CreateFileA()` testet ob der COM-Port existiert, bei Erfolg `return true`.

### GW-7. Zu breiter `"GW"` Beschreibungs-Match erzeugt False Positives
**Datei:** `src/gw_device_detector.cpp:84` + `greaseweazlehardwareprovider.cpp:131`  
**Schwere:** 🟡 Niedrig  
**Problem:** `desc.contains("GW", CaseInsensitive)` matcht jedes Gerät mit "GW" im Namen (z.B. "Gateway", "Network GW Interface"), was zu unnötigen Probe-Versuchen und Timeouts führt.  
**Fix:** `"GW"` Match entfernt, nur noch `"Greaseweazle"` und `"Keir Fraser"` als Beschreibungsfilter.

---

## Aktualisierte Zusammenfassung

| Kategorie | Anzahl |
|-----------|--------|
| Kritisch (Build-Breaker / UB) | 4 |
| GW Hardware-Erkennung Kritisch | 4 |
| GW Hardware-Erkennung Mittel/Niedrig | 3 |
| Warnungen (Format-Fehler) | 3 |
| Style/Qualität | 8 |
| **Gesamt behoben** | **22 Fixes + 37 Include-Guards** |

---

## 🔴 ETAPPE 1: CORE ENGINE (14 Bugs in src/core/ + src/core_recovery/)

### CORE-1. Undefined Behavior: Evaluation Order in Flux-Decoder
**Datei:** `src/core/unified/uft_flux_decoder.c:374`  
**Schwere:** 🔴 Kritisch  
**Problem:** `decoded[out_pos++] = (decoded[out_pos] << 4) | out` — ob `out_pos` vor oder nach dem Lesen inkrementiert wird, ist laut C-Standard undefiniert. Compiler können das unterschiedlich optimieren → korrupte Decoder-Ausgabe.  
**Fix:** In zwei Statements aufgeteilt: erst `decoded[out_pos] = ... | out;` dann `out_pos++;`

### CORE-2. Array Out-of-Bounds in Protection-Suite
**Datei:** `src/core/uft_protection_suite.c:1674`  
**Schwere:** 🔴 Kritisch  
**Problem:** Loop-Grenze `UFT_PROT_MAX_INDICATORS` = 64, aber `uft_prot_scheme_t.indicators[]` hat nur 16 Elemente. Jeder `indicator_count > 16` liest über das Array hinaus → Crash oder Datenkorruption.  
**Fix:** Neue Konstante `UFT_PROT_SCHEME_MAX_INDICATORS = 16` in Header definiert und im Loop verwendet.

### CORE-3. Buffer Overflow in CLI-Konvertierung (sprintf)
**Datei:** `src/core/uft_param_bridge.c:692-711`  
**Schwere:** 🔴 Kritisch  
**Problem:** Unbegrenztes `sprintf()` in 4096-Byte-Buffer. Bei vielen Parametern mit langen String-Werten (z.B. Dateipfade) Overflow möglich → Stack/Heap-Corruption.  
**Fix:** Komplett auf `snprintf()` mit Remaining-Tracking umgestellt, bricht bei vollem Buffer ab.

### CORE-4. Buffer Overflow in JSON-Export (sprintf)
**Datei:** `src/core/uft_settings.c:345-362`  
**Schwere:** 🔴 Kritisch  
**Problem:** `uft_settings_to_json()` nutzt unbegrenztes `sprintf()` in 4096-Byte-Buffer. Bei vielen Settings oder langen Werten Overflow.  
**Fix:** Alle `sprintf()` durch `snprintf()` mit Bounds-Checking ersetzt.

### CORE-5. Unsigned Underflow in Multi-Rev-Summary
**Datei:** `src/core/uft_multirev.c:1091+`  
**Schwere:** 🔴 Kritisch  
**Problem:** `2048 - (size_t)pos` — wenn `pos` (int) durch viele snprintf-Aufrufe 2048 überschreitet, wird `(size_t)pos` abgezogen und das Ergebnis wraps zu einem riesigen size_t-Wert → massiver Buffer Overflow.  
**Fix:** Safe-Macro `MULTIREV_SNPRINTF` mit Bounds-Check: prüft `pos >= 0 && pos < buf_size` vor jedem Schreibzugriff.

### CORE-6–9. Unchecked fread() in 4 Dateien
**Dateien:**  
- `src/core/uft_apd_format.c:143` — APD-Datei wird geladen
- `src/core/uft_fat_editor.c:232` — FAT-Image wird geladen
- `src/core/uft_writer_backend.c:212` — Bestehendes Image wird eingelesen
- `src/core/uft_sos_protection.c:427` — KryoFlux-Stream wird geladen

**Schwere:** 🟡 Hoch  
**Problem:** `fread()` Rückgabewert wird nicht geprüft. Bei I/O-Fehlern oder kurzen Reads werden uninitialisierte/teilweise Daten weiterverarbeitet → stille Datenkorruption.  
**Fix:** Überall Rückgabewert geprüft, bei Short-Read Fehlermeldung und sauberes Cleanup.

### CORE-10. Dead Code + Redundante Bedingung im Flux-Decoder PLL
**Datei:** `src/core/unified/uft_flux_decoder.c:185-222`  
**Schwere:** 🟡 Mittel  
**Problem:** Die `while`-Bedingung ist identisch mit der inneren `if`-Bedingung → der `else`-Zweig ("No pulse yet - output 0") war unerreichbarer Dead Code. Die Zero-Bit-Ausgabe und Sync-Loss-Erkennung wurden nie ausgeführt.  
**Fix:** Redundantes `if`/`else` entfernt, Dead Code bereinigt, korrekte Einrückung.

### CORE-11. Format-Spezifizierer %u für signed int
**Datei:** `src/core/unified/uft_tool_adapter.c:282,293`  
**Schwere:** 🟡 Mittel  
**Problem:** `%u` für `start_track` (int) und `revolutions` (int) — undefiniertes Verhalten bei negativen Werten.  
**Fix:** `%u` → `%d` für signed Variablen.

### CORE-12. Signed Integer Overflow bei Byte-Shifts (uint8_t << 24)
**Dateien:** `src/core/uft_sos_protection.c` (10 Stellen), `src/core/uft_apd_format.c:60`  
**Schwere:** 🟡 Mittel  
**Problem:** `uint8_t` wird zu `int` promoted, dann `<< 24`. Bei Byte-Wert ≥ 0x80 setzt das Bit 31 (Vorzeichenbit) → Signed Integer Overflow = UB.  
**Fix:** Expliziter Cast auf `(uint32_t)` vor dem Shift.

### CORE-13. Format-Spezifizierer %u für uint8_t (Promotion zu int)
**Dateien:** `src/core/uft_protection_suite.c:1539,1570,1670`, `src/core/uft_writer_verify.c:949`  
**Schwere:** 🟢 Niedrig  
**Problem:** `uint8_t` Argumente an `%u` (erwartet `unsigned int`) — funktioniert praktisch, aber strikt UB.  
**Fix:** Expliziter Cast `(unsigned)` an allen betroffenen Stellen.

### CORE-14. Funktionssignatur-Mismatch (Header vs. Definition)
**Datei:** `include/uft/uft_protection.h:703` vs `src/core/uft_protection_suite.c:584`  
**Schwere:** 🔴 Kritisch  
**Problem:** Header deklariert `uft_prot_apple_detect_nibble_count(data, size, scheme)` (3 Params), Definition hat `(data, size, track, scheme)` (4 Params). Jeder Aufruf über den Header-Prototyp übergibt 3 Args an eine 4-Param-Funktion → Stack-Corruption.  
**Fix:** Header-Deklaration um fehlenden `uint8_t track` Parameter ergänzt.

---

## Etappe 2: HAL & Hardware (src/hal/, src/hardware/, src/hardware_providers/, src/usb/)

**Scope:** 46 Dateien, ~22.700 Zeilen  
**Methode:** cppcheck + manuelles Review + Pattern-Analyse

### HAL-1. Array Out-of-Bounds in DAM-Namenstabelle
**Datei:** `src/hardware/uft_gw2dmk.c:139`  
**Schwere:** 🔴 Kritisch  
**Problem:** `dam_names[]` hat durch Designated-Initializer nur 252 Einträge (höchster Index 0xFB), aber der Bounds-Check prüft `dam < 256`. Index 252–255 → OOB-Lesen.  
**Fix:** `dam < 256` durch `dam < sizeof(dam_names) / sizeof(dam_names[0])` ersetzt.

### HAL-2. Unsigned Underflow in End-of-Stream-Erkennung
**Datei:** `src/hal/uft_greaseweazle_full.c:1242`  
**Schwere:** 🔴 Kritisch  
**Problem:** `chunk_read - 2` ist `size_t`-Subtraktion. Bei `chunk_read < 3` wraps zu ~4 Milliarden → die Schleife liest weit über den Buffer hinaus.  
**Fix:** Guard `if (chunk_read >= 3)` hinzugefügt, Schleifengrenze korrigiert zu `<= total_read + chunk_read - 3`.

### HAL-3. Signed Integer Overflow bei Byte-Shifts (uint8_t << 24/16) — 27 Stellen in 7 Dateien
**Dateien:**  
- `src/hal/uft_pauline.c` (12 Stellen)
- `src/hal/uft_zoomtape.c` (3 Stellen)
- `src/hal/uft_greaseweazle_full.c` (4 Stellen)
- `src/hal/uft_hal_unified.c` (2 Stellen)
- `src/hardware/uft_gw2dmk.c` (2 Stellen)
- `src/hardware/uft_hw_greaseweazle.c` (2 Stellen)
- `src/hardware/uft_hw_supercard.c` (1 Stelle)
- `src/usb/uft_greaseweazle_protocol.c` (2 Stellen)

**Schwere:** 🟡 Mittel (UB bei Byte-Werten ≥ 0x80)  
**Problem:** `uint8_t` wird zu `int` promoted, dann `<< 24`. Setzt Vorzeichenbit → Signed Overflow = Undefined Behavior. Besonders gefährlich bei Firmware-Versionsfeldern und Flux-Daten.  
**Fix:** Expliziter `(uint32_t)`-Cast vor allen `<< 24` und `<< 16` Shifts.

### HAL-4. Unchecked fread() in Datei-Parsern (NIB + G64)
**Datei:** `src/hal/uft_nibtools.c` (9 Stellen)  
**Schwere:** 🟡 Hoch  
**Problem:** NIB- und G64-Dateiparser nutzen `fread()` ohne Rückgabewert-Prüfung für Magic-Bytes, Track-Count, Offset-Tabellen, Speed-Tabellen und Track-Daten. Bei korrupten/abgeschnittenen Dateien → uninitialisierte Daten werden verarbeitet.  
**Fix:** Alle `fread()`-Aufrufe mit Rückgabewert-Prüfung versehen. Bei Short-Read: Fehlermeldung, Cleanup, frühzeitiger Abbruch.

### HAL-5. Unchecked fread() in KryoFlux Raw-Stream Import
**Datei:** `src/hal/uft_kryoflux_dtc.c:1308`  
**Schwere:** 🟡 Hoch  
**Problem:** KryoFlux-Raw-Datei wird geladen ohne Prüfung ob alle Bytes gelesen wurden.  
**Fix:** Rückgabewert geprüft, bei `bytes_read != size` wird Buffer freigegeben und Track übersprungen.

### HAL-6. Buffer Overflow in Track-Writer (memcpy ohne Bounds-Check)
**Datei:** `src/hardware/uft_track_writer.c:386,457`  
**Schwere:** 🔴 Kritisch  
**Problem:** `memcpy(raw + leader, track_data, length)` in zwei Funktionen ohne Prüfung ob `leader + length` den Buffer (`WRITER_TRACK_SIZE * 2 = 16KB`) überschreitet. Zusätzlich: Pre-Sync-Check greift auf `track_data[0]` und `[1]` zu ohne `length >= 2` zu prüfen.  
**Fix:** Bounds-Check `if ((size_t)leader + length > buf_size)` vor memcpy. Pre-Sync um `length >= 2`-Guard ergänzt.

### HAL-7. Format-Spezifizierer %d für uint32_t
**Datei:** `src/usb/uft_greaseweazle_protocol.c:231`  
**Schwere:** 🟢 Niedrig  
**Problem:** `hw_model` (uint32_t) wird mit `%d` formatiert. Bei Werten > INT_MAX → UB.  
**Fix:** `%d` → `%u`, zusätzlich `(unsigned)` Cast für `major`/`minor` (uint8_t).

---

## Aktualisierte Gesamtzusammenfassung

| Kategorie | Anzahl |
|-----------|--------|
| Runde 1: Build/Style/Format-Fixes | 15 + 37 Include-Guards |
| Runde 2: GW Hardware-Erkennung | 7 |
| **Etappe 1: Core Engine** | **14** |
| **Etappe 2: HAL & Hardware** | **7 (27+ Einzelstellen)** |
| **Gesamt behoben** | **43 Fixes + 37 Include-Guards** |

### Betroffene Dateien (Etappe 2)
| Datei | Bugs |
|-------|------|
| `src/hardware/uft_gw2dmk.c` | HAL-1, HAL-3 |
| `src/hal/uft_greaseweazle_full.c` | HAL-2, HAL-3 |
| `src/hal/uft_pauline.c` | HAL-3 |
| `src/hal/uft_zoomtape.c` | HAL-3 |
| `src/hal/uft_hal_unified.c` | HAL-3 |
| `src/hardware/uft_hw_greaseweazle.c` | HAL-3 |
| `src/hardware/uft_hw_supercard.c` | HAL-3 |
| `src/hal/uft_nibtools.c` | HAL-4 |
| `src/hal/uft_kryoflux_dtc.c` | HAL-5 |
| `src/hardware/uft_track_writer.c` | HAL-6 |
| `src/usb/uft_greaseweazle_protocol.c` | HAL-3, HAL-7 |

### Betroffene Dateien (Etappe 1)
| Datei | Bugs |
|-------|------|
| `src/core/unified/uft_flux_decoder.c` | CORE-1, CORE-10 |
| `src/core/uft_protection_suite.c` | CORE-2, CORE-13 |
| `include/uft/uft_protection.h` | CORE-2, CORE-14 |
| `src/core/uft_param_bridge.c` | CORE-3 |
| `src/core/uft_settings.c` | CORE-4 |
| `src/core/uft_multirev.c` | CORE-5 |
| `src/core/uft_apd_format.c` | CORE-6, CORE-12 |
| `src/core/uft_fat_editor.c` | CORE-7 |
| `src/core/uft_writer_backend.c` | CORE-8 |
| `src/core/uft_sos_protection.c` | CORE-9, CORE-12 |
| `src/core/unified/uft_tool_adapter.c` | CORE-11 |
| `src/core/uft_writer_verify.c` | CORE-13 |

---

## Etappe 3: Format-Parser (741 Dateien, ~172K Zeilen)

### FMT-1: Apple GCR Buffer-Overflow (uft_apple_gcr.c)
- **Schwere:** Hoch – Schreiben an Index 342 bei buffer[342]
- **Fix:** `buffer[342]` → `buffer[343]` für Checksumme

### FMT-2: G64 Halftrack OOB (uft_d64_g64.c)
- **Schwere:** Hoch – `halftrack = i + 2` kann `tracks[84]` überschreiten
- **Fix:** `if (halftrack >= G64_MAX_TRACKS) break;`

### FMT-3: Struct-Filename OOB (uft_format_extensions.h)
- **Schwere:** Hoch – Null-Terminator überschreibt nächstes Struct-Member
- **Fix:** BBC DFS `filename[7]` → `[8]`, TR-DOS `filename[8]` → `[9]`

### FMT-4: MFMTOBIN Macro UB (amiga_hw.c:679)
- **Schwere:** Hoch – `i++` in Makro-Expansion wird zweimal evaluiert
- **Fix:** `MFMTOBIN(track_buffer[i]); i++;` statt `MFMTOBIN(track_buffer[i++])`

### FMT-5: Memory-Leak pc_img.c
- **Schwere:** Mittel – `ctx` nicht freigegeben bei vorzeitigem Return
- **Fix:** `free(ctx)` vor `return UFT_ERR_FILE_OPEN`

### FMT-6: Memory-Leak + sizeof(pointer) in Test-Code
- **Schwere:** Mittel – sizeof(pointer) statt sizeof(buffer), kein free()
- **Fix:** 3 Dateien: Korrekte Größe + `free()` hinzugefügt (sav/sram/srm)

### FMT-7: Uninitialisierte Variable h17_writer.c
- **Schwere:** Mittel – `str_tmp[512]` uninitialisiert, mit strlen() verwendet
- **Fix:** `char str_tmp[512] = "";`

### FMT-8: Array-OOB format_detect_complete.c
- **Schwere:** Hoch – `names[]` kleiner als `UFT_FMT_COUNT`
- **Fix:** `sizeof(names)/sizeof(names[0])` statt `UFT_FMT_COUNT`

### FMT-9: Byte-Shift UB (<< 24/16 ohne Cast) — 412 Instanzen
- **Schwere:** Hoch – uint8_t << 24 ist undefiniertes Verhalten
- **Fix:** `(uint32_t)` Cast in 167 Dateien (408 Batch + 4 manuell)

### FMT-10: Unchecked fread() (g71/ldbs/imz) — 8 Instanzen
- **Schwere:** Mittel – Datenkorruption bei Short-Read
- **Fix:** Return-Wert-Prüfung mit Cleanup bei Fehler

### FMT-11: Buffer-Overflow make_bootblock.c (dos_sign)
- **Schwere:** Hoch – `memcpy("DOS", 4)` in `dos_sign[3]`
- **Fix:** `memcpy("DOS", 3)`

### FMT-12: Format-Specifier-Mismatches — 8 Fixes
- **Schwere:** Mittel – %d für uint32_t, %u für int (UB auf manchen Plattformen)
- **Fix:** Korrekte Specifier + (unsigned) Casts

### FMT-13: Uninitialisiertes exec_offset (make_bootblock.c)
- **Schwere:** Hoch – Return von uninitialisiertem Wert
- **Fix:** `int exec_offset = -1`

### FMT-14: NULL-Deref nach malloc (d88/dmk/nib)
- **Schwere:** Hoch – malloc-Return nicht geprüft → Crash
- **Fix:** NULL-Check + Cleanup in 3 Dateien

### FMT-15: Unchecked fread in imz.c
- **Schwere:** Mittel – Kompletterfolg-Annahme bei I/O
- **Fix:** Rückgabewert-Prüfung mit Cleanup

### FMT-16: blocktable[4] OOB (params.h)
- **Schwere:** Hoch – Guard prüft `ldtindex < 8` aber Array hat nur 4 Einträge
- **Fix:** `blocktable[4]` → `blocktable[8]` in params.h

### Betroffene Dateien (Etappe 3) — 179 Dateien modifiziert
| Kategorie | Dateien |
|-----------|---------|
| Byte-Shift UB (FMT-9) | 167 Dateien (automatisiert) |
| Manuell gefixt | 12 Dateien (siehe oben) |

---

## Gesamtstatistik

| Etappe | Bugs | Dateien | Zeilen |
|--------|------|---------|--------|
| Runde 1 (Initial) | 15 + 37 Guards | ~20 | ~50K |
| Runde 2 (Greaseweazle) | 7 | 7 | ~5K |
| Etappe 1 (Core) | 14 | 12 | 18K |
| Etappe 2 (HAL/Hardware) | 7 (28+ Instanzen) | 11 | 23K |
| Etappe 3 (Formate) | 16 (445+ Instanzen) | 179 | 172K |
| **Gesamt** | **59 Bugs + 37 Guards** | **~229** | **~268K** |

---

## TODO: goto-Cleanup Refactoring

**Priorität:** Mittel — Kein Bug, aber reduziert Fehlerrate bei zukünftigen Änderungen

**Kandidaten für goto-Cleanup-Pattern (20-30 Funktionen):**
- `src/formats/g71/uft_g71.c` — 4× duplizierte Cleanup-Ketten
- `src/formats/scp/uft_scp.c` — Komplexe Multi-Ressourcen-Verwaltung
- `src/formats/ldbs/uft_ldbs.c` — Verschachtelte Block-Parsing-Loops
- `src/formats/pce/uft_pce.c` — Chunk-basiertes Parsing mit malloc
- `src/formats/td0/uft_td0_parser_v2.c` — LZSS + Track-Parsing
- `src/formats/c64/uft_d64_g64.c` — G64-Import mit Track-Allokation
- `src/formats/nib/uft_nib.c` — Bereits 3-stufige Cleanup-Kette
- `src/formats/dmk/uft_dmk.c` — Track-Buffer + File-Handle
- `src/formats/d88/uft_d88.c` — Sektor-Allokation in Loop

**Pattern:**
```c
int func(void) {
    int ret = UFT_ERR_MEMORY;
    FILE *f = NULL;
    void *buf = NULL;
    // ...
    ret = UFT_OK;
    // fall through to cleanup
fail:
    free(buf);
    if (f) fclose(f);
    return ret;
}
```

---

## Etappe 4: Decoder/Encoder/Analyse (26 Dateien, ~12K Zeilen)

**Bewertung:** Deutlich saubererer Code als Format-Parser. Konsequente malloc-NULL-Checks, saubere Cleanup-Pfade.

### DEC-1: Format-Specifier bitrate_analysis.c:515
- **Schwere:** Mittel – uint32_t track/halftrack mit %d statt %u
- **Fix:** `%d` → `%u`

### DEC-2: Format-Specifier metrics.c:437
- **Schwere:** Mittel – int scores mit %u statt %d
- **Fix:** `%u` → `%d` (3 Felder)

### DEC-3: Division-by-Zero cell_analyzer.c:85
- **Schwere:** Hoch – `1e9 / cell_time_ns` ohne Guard (cell_time_ns kann 0 sein nach failed auto-detect)
- **Fix:** Ternärer Guard `(cell_time_ns > 0) ? 1e9 / cell_time_ns : 0`

### Betroffene Dateien (Etappe 4)
| Datei | Bugs |
|-------|------|
| `src/analysis/uft_bitrate_analysis.c` | DEC-1 |
| `src/analysis/uft_metrics.c` | DEC-2 |
| `src/decoder/uft_cell_analyzer.c` | DEC-3 |

---

## TODO: goto-Cleanup Refactoring

**Priorität:** Mittel — Kein Bug, aber verbessert Wartbarkeit und reduziert zukünftige Fehlerrate.

**Betroffene Funktionen (komplexe Parser mit 3+ Ressourcen):**
- `src/formats/g71/uft_g71.c` — uft_g71_load()
- `src/formats/scp/uft_scp.c` — scp_load()
- `src/formats/ldbs/uft_ldbs.c` — ldbs_open()
- `src/formats/pce/uft_pce.c` — pce_load_pri(), pce_load_psi()
- `src/formats/td0/uft_td0.c` — td0_load()
- `src/formats/c64/uft_d64_g64.c` — g64_parse()
- `src/formats/nib/uft_nib.c` — nib_open()
- `src/formats/dmk/uft_dmk.c` — dmk_read_track()
- `src/formats/d88/uft_d88.c` — d88_read_track()
- `src/hal/uft_nibtools.c` — nib_import(), g64_import()

**Muster:** Linux-Kernel-Stil goto-Cleanup statt verschachtelter if-Ketten.

---

## Etappe 5: GUI (42 Dateien, ~24K Zeilen, C++/Qt)

### GUI-1: Byte-Shift UB in filesystem_browser.cpp:865
- **Schwere:** Hoch – `(uint8_t) << 24` ohne Cast
- **Fix:** `(uint32_t)` Casts auf alle 4 Shift-Operanden

### GUI-2: Division-by-Zero (3 Stellen)
- **Schwere:** Hoch – Crash bei leeren Daten / Resize vor Datenlade
- **a)** `UftTrackVisualization.cpp:312` — `availH / m_tracks` (m_tracks=0 bei Resize)
- **b)** `uft_format_converter_wizard.cpp:856` — `(track*100) / total` ohne Guard
- **c)** `uft_gw2dmk_panel.cpp:442` — `((track*2+head)*100) / total` ohne Guard
- **Fix:** Ternäre Guards `(x > 0) ? ... / x : 0`

### GUI-3: Format-Specifier zxtap.c:582
- **Schwere:** Mittel – `uint16_t - 2` promoted zu int, mit %u gedruckt
- **Fix:** `(unsigned)` Cast

### Betroffene Dateien (Etappe 5)
| Datei | Bugs |
|-------|------|
| `src/gui/uft_filesystem_browser.cpp` | GUI-1 |
| `src/gui/UftTrackVisualization.cpp` | GUI-2a |
| `src/gui/uft_format_converter_wizard.cpp` | GUI-2b |
| `src/gui/uft_gw2dmk_panel.cpp` | GUI-2c |
| `src/formats/tzx/uft_zxtap.c` | GUI-3 |

---

## Etappe 6: Tools/CLI (11 Dateien, ~3.3K Zeilen)

### TOOL-1: sizeof(pointer) in uft_tool_greaseweazle.c:167/169
- **Schwere:** Hoch – `sizeof(info)` gibt 8 (Pointer-Größe) statt Buffer-Größe
- **Fix:** `sizeof(info)` → `size` (Funktionsparameter)

### Hinweis: Command-Injection-Risiko
- 5 Tools nutzen `popen()` mit `snprintf`-generierten Kommandozeilen
- Dateinamen mit Shell-Metazeichen (`$`, `` ` ``, `"`) können ausbrechen
- **Empfehlung:** Shell-Escaping oder `execvp()`-basierte Alternative (TODO)

### Betroffene Dateien (Etappe 6)
| Datei | Bugs |
|-------|------|
| `src/tools/uft_tool_greaseweazle.c` | TOOL-1 |

---

## Etappe 7: Tests (77 Dateien, ~29K Zeilen)

**Bewertung:** Keine kritischen Bugs gefunden. Test-Code ist angemessen geschrieben.

### Ergebnis
- cppcheck arrayIndexOutOfBounds in test_p00.c:64 — **False Positive** (Loop mit Null-Terminator-Guard)
- 376 assertWithSideEffect — Nur Test-Code, harmlos in Debug-Builds
- Memory-Leaks in 4 Test-Dateien — Normal für Test-Prozesse (exit() räumt auf)
- **Keine Fixes nötig**

---

## Etappe 8-10: Plugins/Build (101 Dateien)

Nicht-C/C++ Dateien (CMakeLists, Makefiles, etc.) — kein Code-Audit nötig.
1 Plugin-Datei — bei cppcheck clean.

---

## Finale Gesamtstatistik

| Etappe | Scope | Bugs | Instanzen | Dateien |
|--------|-------|------|-----------|---------|
| Runde 1 (Initial) | Build + Headers | 15 + 37 Guards | ~52 | ~20 |
| Runde 2 (Greaseweazle) | USB/HAL/GUI | 7 | 7 | 7 |
| Etappe 1 (Core Engine) | 59 Dateien, 18K LOC | 14 | 14 | 12 |
| Etappe 2 (HAL/Hardware) | 46 Dateien, 23K LOC | 7 Kategorien | 28+ | 11 |
| Etappe 3 (Format-Parser) | 741 Dateien, 172K LOC | 16 Kategorien | 445+ | 179 |
| Etappe 4 (Decoder/Analyse) | 26 Dateien, 12K LOC | 3 | 3 | 3 |
| Etappe 5 (GUI) | 42 Dateien, 24K LOC | 5 | 5 | 5 |
| Etappe 6 (Tools/CLI) | 11 Dateien, 3.3K LOC | 1 | 2 | 1 |
| Etappe 7 (Tests) | 77 Dateien, 29K LOC | 0 | 0 | 0 |
| **Gesamt** | **~1000 Dateien, ~281K LOC** | **68 Bug-Kategorien** | **~556 Instanzen** | **~238 Dateien** |

### Bug-Verteilung nach Schwere
| Schwere | Anzahl Kategorien | Beschreibung |
|---------|-------------------|-------------|
| 🔴 Hoch | 38 | Buffer-Overflow, OOB, UB, NULL-Deref, Div-by-Zero |
| 🟡 Mittel | 27 | Memory-Leaks, Format-Mismatches, Unchecked I/O |
| ⚪ Niedrig | 3 | Style-Issues, Dead-Code |

### Bug-Verteilung nach Typ
| Typ | Instanzen |
|-----|-----------|
| Byte-Shift UB (<< 24/16) | ~415 |
| Format-Specifier Mismatch | ~18 |
| Array OOB / Buffer-Overflow | 12 |
| Unchecked malloc/fread | 15 |
| Memory-Leaks | 8 |
| Division-by-Zero | 5 |
| Uninitialisierte Variablen | 3 |
| sizeof(pointer) | 5 |
| Evaluation-Order UB | 1 |
| Include-Guard-Fehler | 37 |
| Sonstige | ~37 |

### TODOs (nicht im Audit gefixt)
1. **goto-Cleanup Refactoring** — 10+ Parser-Funktionen (Wartbarkeit)
2. **Command-Injection-Härtung** — 5 Tool-Wrapper mit popen() (Sicherheit)
3. **assertWithSideEffect** — 376 Stellen in Test-Code (Code-Qualität)

---

## Nachaudit: Tiefenanalyse (Runde 2)

### DEEP-1 bis DEEP-5: Unsigned-Underflow in Loop-Bedingungen (106 Fixes)

**Problem:** `for (size_t i = 0; i < size - N; i++)` — wenn `size < N`, wrapped `size - N` zu `SIZE_MAX` (~18 Exabytes), Loop läuft über gesamten Speicher → **Crash/Buffer-Overread**

**Fix-Strategie:** Loop-Bedingung umschreiben: `i < size - N` → `i + N < size`
- Mathematisch identisch
- Kein Underflow möglich
- Kein Risiko für Verhaltensänderung

**Statistik:**
- 160 Stellen gefunden
- 54 bereits durch Guards gesichert
- 101 per Batch-Script gefixt
- 5 manuell gefixt (komplexe Ausdrücke)

**Betroffene Module (Auswahl):**
- `src/protection/` — 25+ Stellen (Signatur-Suche in Tracks)
- `src/formats/` — 20+ Stellen (Parser-Loops)
- `src/algorithms/` — 15+ Stellen (Pattern-Matching)
- `src/recovery/` — 10+ Stellen (Bitstream-Recovery)
- `src/flux/` — 8+ Stellen (Flux-Analyse)
- `src/core/` — 10+ Stellen (Core-Funktionen)
- `src/integration/` — 6+ Stellen (Track-Decoder)

### STR-1/STR-2: strncpy ohne Null-Terminator (2 Fixes)
- `src/cloud/uft_cloud.c:419` — `strncpy(temp, dest_path, sizeof(temp))` ohne `-1`
- `src/fatfs/uft_fatfs.c:282` — `strncpy(entries[].name, name, sizeof(...))` ohne `-1`
- **Fix:** `sizeof(x) - 1` + explizites `'\0'`

### STR-3: strncpy ohne Null-Term in Protection-Code (4 Fixes)
- `src/protection/c64/uft_c64_protection_ext.c:290,304,415,439`
- Beschreibungs-Strings ohne Null-Terminator bei Puffer-Überlauf
- **Fix:** `-1` + explizites `[sizeof-1] = '\0'`

### Betroffene Dateien (Nachaudit)
| Kategorie | Dateien | Instanzen |
|-----------|---------|-----------|
| Unsigned Underflow | ~95 | 106 |
| strncpy Null-Term | 3 | 6 |
| **Gesamt** | ~98 | 112 |

---

## Aktualisierte Gesamtstatistik (inkl. Nachaudit)

| Runde | Scope | Kategorien | Instanzen |
|-------|-------|------------|-----------|
| Runde 1 (Initial) | Build + Headers | 15 + 37 Guards | ~52 |
| Runde 2 (Greaseweazle) | USB/HAL/GUI | 7 | 7 |
| Etappe 1 (Core Engine) | 59 Dateien | 14 | 14 |
| Etappe 2 (HAL/Hardware) | 46 Dateien | 7 | 28+ |
| Etappe 3 (Format-Parser) | 741 Dateien | 16 | 445+ |
| Etappe 4 (Decoder/Analyse) | 26 Dateien | 3 | 3 |
| Etappe 5 (GUI) | 42 Dateien | 5 | 5 |
| Etappe 6 (Tools/CLI) | 11 Dateien | 1 | 2 |
| **Nachaudit (Tiefenanalyse)** | **~95 Dateien** | **3** | **112** |
| **GESAMTTOTAL** | **~1000 Dateien** | **71 Bug-Kategorien** | **~668 Instanzen** |

---

## Robustness-Runde: I/O-Fehlerbehandlung & Typsicherheit

### ROB-1: ferror()-Checks für Schreibsequenzen (103 Fixes)

**Problem:** `fwrite()` ohne Prüfung des Rückgabewerts. Bei Disk-Full, I/O-Fehlern oder unterbrochenen Writes werden korrupte Disk-Images geschrieben — ohne Fehlermeldung.

**Fix-Strategie:** `ferror(fp)` Check vor jedem `fclose(fp) + return OK` in Writer-Funktionen. Fängt alle akkumulierten Schreibfehler einer Sequenz auf einmal ab.

**Betroffene Dateien:** 60 Dateien, darunter:
- `src/formats/` — 30+ Disk-Image-Writer (G64, SCP, D64, ADF, NIB, UDI, ...)
- `src/loaders/` — 15 Loader/Writer-Paare
- `src/core/` — Writer-Backend, Image-DB
- `src/hal/` — Nibtools, Zoomtape

### ROB-2: fseek()-Fehlerbehandlung (80 Fixes)

**Problem:** `fseek()` ohne Prüfung. Wenn seek fehlschlägt (Datei zu kurz, I/O-Fehler), werden nachfolgende `fread()`/`fwrite()` an falscher Position ausgeführt — korrupte Daten werden gelesen/geschrieben.

**Fix-Strategie:**
- In Loops: `if (fseek(...) != 0) continue;`
- In Funktionen: `if (fseek(...) != 0) return ERR;`
- Kritische Writer (SCP, UDI): Explizite Fehlerbehandlung mit Datei-Cleanup

**Betroffene Dateien:** 25+ Dateien
- Writer-Backend (5 fseek-Fixes)
- SCP Writer mit Checksum-Rewrite
- UDI Writer mit Header-Update
- Format-Parser (G71, 86F, MFM, HDF, ADF Recovery, ...)

### ROB-3: Signed/Unsigned Loop-Vergleiche (74 Fixes)

**Problem:** `for (int i = 0; i < size_t_var; i++)` — wenn `size_t_var > INT_MAX` (>2 GB), overflow des `int` Zählers → Endlosloop oder falsches Verhalten. Compiler-Warning `-Wsign-compare`.

**Fix:** `int` → `size_t` für Loop-Variable bei size_t/unsigned Bounds.

**Spezialfall:** `for (size_t b = 0; b < bit_count - 1; ...)` in Kalman-PLL hatte doppeltes Risiko (unsigned underflow + signed comparison) → Guard hinzugefügt.

**Betroffene Dateien:** 48 Dateien quer durch:
- Decoder (GCR, MFM, PLL)
- Format-Parser
- Protection-Scanner
- Flux-Analyse
- Core-Algorithmen

### Verbleibende I/O ohne Check (nicht gefixt)

| Typ | Verbleibend | Risiko | Grund |
|-----|-------------|--------|-------|
| `fseek()` | 246 | Niedrig | Positions-Operationen (SEEK_END, ftell-Äquivalente), nicht direkt vor fread/fwrite |
| `fwrite()` | 164 | Mittel | In Sequenzen mit ferror()-Check am Ende, oder in unkritischen Pfaden |

Diese wurden bewusst nicht gefixt:
- Sequentielle fwrite-Calls werden durch ferror() am Funktionsende abgefangen
- fseek() ohne direkt folgendes I/O ist primär ein ftell/Positioning-Call
- Restrisiko: fseek-Fehler in der Mitte einer Funktion → nächstes fread/fwrite liest/schreibt an alter Position

---

## Finale Gesamtstatistik (Alle Runden)

| Runde | Kategorien | Instanzen | Dateien |
|-------|------------|-----------|---------|
| Initial + Guards | 15 + 37 | ~52 | ~20 |
| Greaseweazle | 7 | 7 | 7 |
| Etappe 1 (Core Engine) | 14 | 14 | 12 |
| Etappe 2 (HAL/Hardware) | 7 | 28+ | 11 |
| Etappe 3 (Format-Parser) | 16 | 445+ | 179 |
| Etappe 4 (Decoder/Analyse) | 3 | 3 | 3 |
| Etappe 5 (GUI) | 5 | 5 | 5 |
| Etappe 6 (Tools/CLI) | 1 | 2 | 1 |
| Nachaudit (Underflow+strncpy) | 3 | 112 | ~98 |
| **Robustness (I/O + Types)** | **3** | **257** | **~120** |
| **GESAMTTOTAL** | **74 Kategorien** | **~925 Instanzen** | **~456 Dateien** |

### Bug-Verteilung nach Schwere:
- 🔴 **Hoch** (Crash/Sicherheit): ~555 — UB, Buffer-Overflow, OOB, NULL-Deref, Underflow
- 🟡 **Mittel** (Datenkorruption): ~333 — Unchecked I/O, Format-Mismatch, Leaks
- ⚪ **Niedrig** (Compiler-Warnings): ~37 — Include-Guards, signed/unsigned

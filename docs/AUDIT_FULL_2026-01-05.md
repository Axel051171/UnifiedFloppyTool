# UFT Vollständiges Audit
## Datum: 2026-01-05
## Version: 3.7.0
## Prüfumfang: 2983 Quelldateien, 458.417 LOC

---

# 1. ZUSAMMENFASSUNG (Kurzstatus)

| Kategorie | Status | Kritische Issues | Handlungsbedarf |
|-----------|--------|------------------|-----------------|
| **Speichersicherheit** | 🔴 KRITISCH | 30+ malloc ohne NULL-Check | SOFORT |
| **Buffer Overflows** | 🔴 KRITISCH | 256× sprintf, 285× strcpy/strcat | SOFORT |
| **I/O-Sicherheit** | 🟠 HOCH | 25+ ungeprüfte fread/fwrite | Sprint 1 |
| **Architektur** | 🟡 MITTEL | 15+ Track-Strukturen, CRC-Redundanz | Sprint 2 |
| **Endian-Handling** | 🟡 MITTEL | Direkte Pointer-Casts ohne Konvertierung | Sprint 2 |
| **Vollständigkeit** | 🟢 GUT | 154 TODOs, 11 kritische | Backlog |
| **Thread-Safety** | 🟡 MITTEL | 5 Mutex-Mismatches | Sprint 1 |

**Gesamtbewertung: 🟠 BEDINGT PRODUKTIONSBEREIT - Kritische Fixes erforderlich**

---

# 2. GEFUNDENE BUGS (priorisiert)

## 2.1 🔴 KRITISCH - Sofortige Aktion erforderlich

### Memory Bugs (malloc ohne NULL-Check)

| ID | Datei | Zeile | Beschreibung |
|----|-------|-------|--------------|
| MEM-001 | `fs/uft_cbm_fs.c` | 414 | `fs->error_table = malloc(error_size)` ohne Check |
| MEM-002 | `fs/uft_cpmfs_impl.c` | 670 | `strcpy(malloc(...))` ohne Check |
| MEM-003 | `fs/uft_cpmfs_impl.c` | 997 | `sb->ds=malloc(dsblks*sb->blksiz)` - Integer Overflow + kein Check |
| MEM-004 | `forensic/uft_recovery_core.c` | 179 | `uint8_t* temp = malloc(sector_size)` ohne Check |
| MEM-005 | `integration/uft_track_drivers.c` | 122, 209, 301, 402 | Mehrere `malloc()` ohne Check |
| MEM-006 | `ir/uft_ir_serialize.c` | 819 | `char *b64 = malloc(b64_size)` ohne Check |
| MEM-007 | `ir/uft_ir_timing.c` | 669 | `uint8_t *buffer = malloc(ser_size)` ohne Check |
| MEM-008 | `gcr/uft_apple_gcr_fusion.c` | 573 | `ctx->fused_nibbles = malloc()` ohne Check |
| MEM-009 | `decoder/uft_fusion.c` | 327 | `fusion->fused = malloc(byte_count)` ohne Check |
| MEM-010 | `decoder/uft_forensic_flux_decoder.c` | 811, 881 | 2× `malloc()` ohne Check |
| MEM-011 | `io/uft_io_abstraction.c` | 98, 104 | 2× `malloc()` ohne Check |
| MEM-012 | `core/layers/uft_job_manager.c` | 288, 300, 340 | 3× `malloc()` ohne Check |
| MEM-013 | `core/uft_audit_trail.c` | 458 | `entry->ext_data = malloc()` ohne Check |
| MEM-014 | `core/unified/uft_unified_image.c` | 591 | Tief verschachtelt, kein Check |
| MEM-015 | `core/uft_multirev.c` | 947 | `ir_track->decoded_data = malloc()` ohne Check |
| MEM-016 | `core/uft_cache.c` | 236, 246 | 2× `malloc()` ohne Check |
| MEM-017 | `core/uft_core.c` | 427, 485 | 2× `malloc()` ohne Check |
| MEM-018 | `core/uft_bitstream_preserve.c` | 645 | `uint8_t* bitstream = malloc()` ohne Check |

**Schweregrad:** KRITISCH - Kann zu Segfaults und undefiniertem Verhalten führen.

### Buffer Overflow Risiken

| ID | Kategorie | Anzahl | Betroffene Module |
|----|-----------|--------|-------------------|
| BOF-001 | sprintf() statt snprintf() | 256 | loaders, fs, protection |
| BOF-002 | strcpy() statt strncpy() | 189 | fs, protection, core |
| BOF-003 | strcat() statt strncat() | 96 | fs, capability, trsdos |

**Kritischste Beispiele:**
```c
// src/loaders/kryofluxstream_loader.c:95
sprintf(filepath,"%s"DIR_SEPARATOR"track%.2d.%d.raw",imgfile->path,track,side);
// Problem: imgfile->path kann beliebig lang sein

// src/fs/uft_cpmfs_impl.c:635-636
sprintf(pat,"??%s",pattern);
// Problem: pattern ist nicht längengeprüft
```

### Integer Overflow Risiken

| ID | Datei | Zeile | Ausdruck | Risiko |
|----|-------|-------|----------|--------|
| INT-001 | `fs/uft_cpmfs_impl.c` | 997 | `dsblks*sb->blksiz` | Multiplikation ohne Überlaufprüfung |
| INT-002 | `fs/uft_cpmfs_impl.c` | 1077 | `d->maxdir*32` | Kann überlaufen |
| INT-003 | `fs/uft_fat12_dir.c` | 340 | `ctx->vol.sectors_per_cluster * UFT_FAT_SECTOR_SIZE` | Multiplikation |
| INT-004 | `floppy_lib/src/uft_floppy_io.c` | 723, 752 | `sector_count * disk->sector_size` | Multiplikation |

## 2.2 🟠 HOCH - Nächster Sprint

### Use-After-Free Risiken

| ID | Datei | Zeile | Variable | Problem |
|----|-------|-------|----------|---------|
| UAF-001 | `protection/uft_atari8_protection.c` | 162 | result | Verwendung nach potentiellem free |
| UAF-002 | `protection/uft_apple2_protection.c` | 72-74 | result | Mehrfache Verwendung nach free |
| UAF-003 | `protection/uft_protection_detect.c` | 606 | ctx | ctx nach free |
| UAF-004 | `protection/uft_pc_protection.c` | 383, 985 | result | result nach free |
| UAF-005 | `fs/uft_amigados_file.c` | 178 | chain | chain nach free |
| UAF-006 | `fs/uft_cbm_fs.c` | 234-240, 443-444 | f, fs | Mehrere Variablen |

### I/O ohne Fehlerbehandlung

| ID | Datei | Zeilen | Operationen |
|----|-------|--------|-------------|
| IO-001 | `ir/uft_ir_serialize.c` | 201, 202, 215, 231, 270 | 5× fwrite ohne Check |
| IO-002 | `floppy_lib/src/formats/uft_d64.c` | 424, 432, 436, 490, 491, 498, 499, 557 | 8× fwrite ohne Check |
| IO-003 | `floppy_lib/src/formats/uft_adf.c` | 192 | fwrite ohne Check |
| IO-004 | `fs/uft_cbm_fs.c` | 489 | fwrite ohne Check |
| IO-005 | `params/uft_params_json.c` | 645 | fwrite ohne Check |
| IO-006 | `ocr/uft_ocr.c` | 199, 305 | fread/fwrite ohne Check |

### fseek/ftell ohne Fehlerprüfung

| ID | Datei | Zeilen |
|----|-------|--------|
| SEEK-001 | `protection/uft_atari8_protection.c` | 391-393 |
| SEEK-002 | `protection/uft_pc_protection.c` | 757-759 |
| SEEK-003 | `fs/uft_amigados_file.c` | 517-519 |
| SEEK-004 | `fs/uft_cbm_fs.c` | 349-351 |
| SEEK-005 | `fs/uft_atari_dos_file.c` | 470-472 |
| SEEK-006 | `fs/uft_ti99_fs_file.c` | 464-466 |
| SEEK-007 | `fs/uft_apple_core.c` | 366-367 |

### fopen ohne NULL-Prüfung

| ID | Datei | Zeile |
|----|-------|-------|
| FOPEN-001 | `fs/uft_scandisk.c` | 112 |
| FOPEN-002 | `test/uft_test_suite.c` | 108 |
| FOPEN-003 | `floppy_lib/src/formats/uft_image.c` | 167 |
| FOPEN-004 | `core/uft_audit_trail.c` | 168 |
| FOPEN-005 | `uft_write_transaction.c` | 179 |

## 2.3 🟡 MITTEL - Backlog

### Endian-Probleme (Big-Endian Inkompatibilität)

| ID | Datei | Zeilen | Problem |
|----|-------|--------|---------|
| END-001 | `ir/uft_ir_timing.c` | 443-497 | Direkte uint16_t/uint32_t Casts ohne Konvertierung |
| END-002 | `core/uft_bit_confidence.c` | 719-758 | Direkte Pointer-Casts |
| END-003 | `params/uft_canonical_params.c` | 554-615 | Direkte int32_t/uint32_t Casts |

### Array Out-of-Bounds Risiken

| ID | Datei | Zeile | Zugriff |
|----|-------|-------|---------|
| ARR-001 | `fs/uft_cbm_fs.c` | 97, 105 | `d64_track_offset[track]` ohne Bounds-Check |
| ARR-002 | `fs/uft_cbm_fs_bam.c` | 286, 305, 306, 329, 330 | `fs->bam->tracks[track]` ohne Check |
| ARR-003 | `loaders/atx_loader.c` | 206, 244 | `img->tracks[track]` ohne Check |
| ARR-004 | `loaders/woz_writer.c` | 214-231 | `img->tracks[track]` ohne Check |

### Mutex Lock/Unlock Mismatch

| ID | Datei | Locks | Unlocks | Differenz |
|----|-------|-------|---------|-----------|
| MTX-001 | `core/layers/uft_job_manager.c` | 11 | 18 | +7 |
| MTX-002 | `core/layers/uft_device_manager.c` | 5 | 9 | +4 |
| MTX-003 | `core/uft_memory.c` | 2 | 3 | +1 |
| MTX-004 | `core/uft_gui_bridge_v2.c` | 15 | 19 | +4 |
| MTX-005 | `core_recovery/uft_memory.c` | 5 | 6 | +1 |

**Hinweis:** Mehr Unlocks als Locks deutet auf frühe Returns mit Unlock hin. Muss manuell geprüft werden.

## 2.4 🟢 NIEDRIG

- Keine kritischen Issues in dieser Kategorie

---

# 3. FEHLENDE / PROBLEMATISCHE MODULE

## 3.1 Architektur-Redundanz

### Mehrfache Track-Strukturen (15+ Definitionen)

```
flux_track_t           (flux_core.h)
uft_track_t            (uft_format_plugin.h, uft_integration.h)
uft_raw_track          (uft_decoder_adapter.h)
uft_bitstream_track    (separat)
uft_ir_track           (IR-Modul)
uft_unified_track      (separat)
uft_td0_track          (TD0-Format)
ipf_track              (IPF-Format)
uft_flux_track_data    (flux)
uft_scp_track_*        (SCP-Format)
uft_kf_track_*         (KryoFlux)
uft_track_v3           (Version 3)
uft_track_forensic_meta
uft_track_format
uft_track_latency
```

**Empfehlung:** Einheitliche `uft_track_t` mit Konvertierungsfunktionen.

### Redundante CRC-Implementierungen

| Funktion | Vorkommen | Betroffene Module |
|----------|-----------|-------------------|
| crc16 | 304 | protection, simd, forensic, integration |
| crc32 | 285 | protection, ir, archive, core |

**Empfehlung:** Zentrale `src/crc/uft_crc_unified.c` verwenden.

### Redundante Decoder

| Funktion | Vorkommen |
|----------|-----------|
| mfm_decode | 171 |
| gcr_decode | 235 |
| pll | 1514 |

## 3.2 Unvollständige Module

| Modul | Status | Fehlend |
|-------|--------|---------|
| IPF MFM Decode | 🔴 STUB | `uft_amiga_caps.c:479` |
| IPF MFM Encode | 🔴 STUB | `uft_amiga_caps.c:493` |
| V-MAX! GCR | 🔴 STUB | `uft_c64_protection_enhanced.c:184` |
| Amiga Extension Blocks | 🟡 TODO | `uft_amigados_file.c:487` |
| SpartaDOS Full Extract | 🟡 TODO | `uft_spartados.c:425` |
| APFS Checksum | 🟡 BROKEN | `uft_apfs_cksum.c:92` |
| Unified API Mount | 🟡 TODO | `uft_unified_api.c:485` |
| OCR Pipeline | 🟡 STUB | `uft_ocr.c:237-257` |
| Binary Audit Log Load | 🟡 TODO | `uft_audit_trail.c:665` |
| IR Compression | 🟡 TODO | `uft_ir_format.c:837` |

---

# 4. I/O-PROBLEME

## 4.1 Datei-I/O

| Kategorie | Anzahl | Schweregrad |
|-----------|--------|-------------|
| fwrite ohne Check | 25+ | HOCH |
| fread ohne Check | 10+ | HOCH |
| fseek/ftell ohne Check | 15+ | MITTEL |
| fopen ohne NULL-Check | 15+ | MITTEL |

## 4.2 Hardware-I/O

| Datei | Zeile | Problem |
|-------|-------|---------|
| `hal/uft_greaseweazle_full.c` | 214 | `ioctl()` ohne Rückgabewert-Prüfung |

## 4.3 Potentielle TOCTOU-Risiken

Keine expliziten `access()` vor `fopen()` Muster gefunden, aber:
- Kein Locking bei parallelen Dateizugriffen
- Keine atomaren Operationen bei Schreibvorgängen

---

# 5. INTEGRATIONSPROBLEME

## 5.1 Datenformat-Inkonsistenzen

| Von | Nach | Problem |
|-----|------|---------|
| Format-Parser | Decoder | 15+ verschiedene Track-Strukturen |
| HAL | Core | `uft_raw_track_t` vs `flux_track_t` |
| IR-Writer | IR-Reader | Endian-abhängige Serialisierung |

## 5.2 Globale Abhängigkeiten

| Variable | Datei | Risiko |
|----------|-------|--------|
| g_initialized | uft_core.c | Thread-Safety |
| g_log_handler | uft_core.c | Thread-Safety |
| g_global_session | uft_audit_trail.c | Thread-Safety |
| g_format_plugins | uft_format_plugin.c | Nicht thread-safe initialisiert |

## 5.3 Validierung

- 872 NULL/Validierungs-Checks in core (GUT)
- Mehrere öffentliche Funktionen ohne Eingabevalidierung (PRÜFEN)

---

# 6. VERGESSENE PUNKTE

## 6.1 Dokumentierte Annahmen

| Annahme | Betrifft | Status |
|---------|----------|--------|
| Little-Endian Host | ir/uft_ir_timing.c, core/uft_bit_confidence.c | 🔴 NICHT ABGESICHERT |
| 8-bit char | Mehrere | ✅ Standard |
| POSIX-konformes Filesystem | loaders | 🟡 Windows-Pfade problematisch |

## 6.2 Fehlende Funktionalität

| Feature | Status | Priorität |
|---------|--------|-----------|
| Big-Endian Support | FEHLT | MITTEL |
| Cross-Platform Pfade | TEILWEISE | NIEDRIG |
| Atomic File Operations | FEHLT | NIEDRIG |

## 6.3 Edge Cases

| Edge Case | Behandelt | Risiko |
|-----------|-----------|--------|
| 0-Byte Dateien | Teilweise | NIEDRIG |
| Maximale Trackgröße | Teilweise | MITTEL |
| Beschädigte Header | 717 Referenzen | GUT |
| Timeout bei HW | 106 Referenzen | GUT |

---

# 7. TODO-LISTE (vollständig & priorisiert)

## P0 - KRITISCH (Sofort, Blocker)

| ID | Beschreibung | Module | Komplexität | Abhängigkeiten |
|----|--------------|--------|-------------|----------------|
| P0-001 | NULL-Checks nach allen malloc() | src/*, 30+ Stellen | M | - |
| P0-002 | sprintf→snprintf Migration | loaders, fs | L | - |
| P0-003 | strcpy→strncpy Migration | fs, protection | M | - |
| P0-004 | strcat→strncat Migration | fs, core | S | - |
| P0-005 | Integer Overflow Schutz bei malloc | fs/uft_cpmfs_impl.c | S | - |

## P1 - HOCH (Sprint 1)

| ID | Beschreibung | Module | Komplexität | Abhängigkeiten |
|----|--------------|--------|-------------|----------------|
| P1-001 | fwrite Rückgabewert prüfen | ir, floppy_lib, fs | M | - |
| P1-002 | fread Rückgabewert prüfen | params, ocr, fs | M | - |
| P1-003 | fseek/ftell Fehlerbehandlung | protection, fs | S | - |
| P1-004 | fopen NULL-Checks | fs, test, core | S | - |
| P1-005 | Mutex Lock/Unlock Audit | core/layers | M | - |
| P1-006 | Use-After-Free Fixes | protection, fs | M | - |
| P1-007 | ioctl Rückgabewert prüfen | hal | S | - |
| P1-008 | Array Bounds Checks | fs/uft_cbm_fs.c, loaders | M | - |

## P2 - MITTEL (Sprint 2)

| ID | Beschreibung | Module | Komplexität | Abhängigkeiten |
|----|--------------|--------|-------------|----------------|
| P2-001 | Endian-sichere Serialisierung | ir, core | L | - |
| P2-002 | Track-Struktur Vereinheitlichung | core, alle | XL | - |
| P2-003 | CRC Zentralisierung | alle | L | P2-002 |
| P2-004 | Decoder Registry Refactoring | decoder, alle | L | P2-002 |
| P2-005 | IPF MFM Decode implementieren | protection | L | - |
| P2-006 | V-MAX! GCR implementieren | protection | M | - |
| P2-007 | Amiga Extension Blocks | fs | M | - |
| P2-008 | SpartaDOS Full Extract | fs | M | - |

## P3 - NIEDRIG (Backlog)

| ID | Beschreibung | Module | Komplexität | Abhängigkeiten |
|----|--------------|--------|-------------|----------------|
| P3-001 | APFS Checksum Fix | fs/modern | M | - |
| P3-002 | OCR Pipeline | ocr | L | - |
| P3-003 | IR Compression | ir | M | - |
| P3-004 | Binary Audit Log | core | M | - |
| P3-005 | Big-Endian Tests | test | M | P2-001 |
| P3-006 | Cross-Platform Pfade | loaders | M | - |

---

# 8. METRIKEN

```
Geprüfte Dateien:           2,983
Geprüfte LOC:               458,417
Gefundene Bugs:             85+
  - Kritisch (P0):          30+ (malloc, sprintf, strcpy)
  - Hoch (P1):              40+ (I/O, UAF, Array)
  - Mittel (P2):            10+ (Endian, Mutex)
  - Niedrig (P3):           5
TODOs im Code:              154
Architektur-Issues:         3 (Track-Strukturen, CRC, Decoder)
I/O-Issues:                 50+
```

---

# 9. EMPFOHLENE SOFORTMASSNAHMEN

## 9.1 Quick Wins (< 1 Stunde)

```bash
# 1. Automatische sprintf→snprintf Konvertierung (Testdurchlauf)
find src -name "*.c" -exec grep -l "sprintf(" {} \; | head -5

# 2. NULL-Check Template erstellen
cat > templates/null_check.c << 'EOF'
void* ptr = malloc(size);
if (!ptr) {
    return UFT_ERR_NOMEM;
}
EOF
```

## 9.2 Erste Fixes (1-4 Stunden)

1. **P0-001**: NULL-Checks in kritischen Pfaden (fs/uft_cpmfs_impl.c, core/uft_core.c)
2. **P1-005**: Mutex-Audit in uft_job_manager.c

## 9.3 Sprint 1 Ziele

- Alle P0 abgeschlossen
- 80% der P1 abgeschlossen
- Audit-Report aktualisiert

---

*Audit durchgeführt: 2026-01-05 01:32 UTC*
*Nächstes Audit empfohlen: Nach Abschluss der P0/P1 Fixes*
*Reviewer: UFT Code Analysis System*

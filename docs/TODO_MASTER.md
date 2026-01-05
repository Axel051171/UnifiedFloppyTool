# UFT - Offene TODOs und Implementierungsstand

## Übersicht

| Kategorie | Header | Source | Status |
|-----------|--------|--------|--------|
| **Total** | 85 | 17 | ~20% implementiert |

---

## 1. KRITISCH - Core APIs (MÜSSEN implementiert werden)

### 1.1 Basis-Infrastruktur

| Datei | Header | Source | Status | Priorität |
|-------|--------|--------|--------|-----------|
| `uft_types.c` | ✅ 366 Zeilen | ❌ FEHLT | TODO | **P0** |
| `uft_error.c` | ✅ 235 Zeilen | ❌ FEHLT | TODO | **P0** |
| `uft_core.c` | ✅ 394 Zeilen | ❌ FEHLT | TODO | **P0** |
| `uft_format_plugin.c` | ✅ 338 Zeilen | ❌ FEHLT | TODO | **P0** |
| `uft_decoder_plugin.c` | ✅ 397 Zeilen | ❌ FEHLT | TODO | **P0** |

**Fehlende Funktionen in uft_types.c:**
```c
uft_geometry_t uft_geometry_for_preset(uft_geometry_preset_t preset);
const uft_format_info_t* uft_format_get_info(uft_format_t format);
uft_format_t uft_format_from_extension(const char* extension);
```

**Fehlende Funktionen in uft_error.c:**
```c
const char* uft_error_string(uft_error_t err);
const char* uft_error_name(uft_error_t err);
const uft_error_info_t* uft_error_get_info(uft_error_t err);
void uft_error_set_context(...);
```

**Fehlende Funktionen in uft_core.c:**
```c
uft_disk_t* uft_disk_open(const char* path, bool read_only);
uft_disk_t* uft_disk_create(const char* path, uft_format_t format, ...);
void uft_disk_close(uft_disk_t* disk);
uft_track_t* uft_track_read(uft_disk_t* disk, int cyl, int head, ...);
uft_error_t uft_track_write(uft_disk_t* disk, const uft_track_t* track, ...);
uft_error_t uft_convert(uft_disk_t* src, const char* dst_path, ...);
uft_error_t uft_analyze(uft_disk_t* disk, uft_analysis_t* analysis, ...);
uft_error_t uft_init(void);
void uft_shutdown(void);
```

---

## 2. HOCH - Format Plugins

### 2.1 Sektor-basierte Formate

| Format | Header | Plugin | Read | Write | Create | Priorität |
|--------|--------|--------|------|-------|--------|-----------|
| **ADF** (Amiga) | ❌ | ❌ | ❌ | ❌ | ❌ | **P1** |
| **IMG/IMA** (PC) | ❌ | ❌ | ❌ | ❌ | ❌ | **P1** |
| **D64** (C64) | ❌ | ❌ | ❌ | ❌ | ❌ | **P2** |
| **ST** (Atari) | ❌ | ❌ | ❌ | ❌ | ❌ | **P2** |
| **DSK** (Generic) | ❌ | ❌ | ❌ | ❌ | ❌ | **P3** |

### 2.2 Flux-basierte Formate

| Format | Header | Plugin | Read | Write | Priorität |
|--------|--------|--------|------|-------|-----------|
| **SCP** | ✅ | ⚠️ 60% | ✅ | ❌ | **P1** |
| **HFE** | ✅ 383 Z. | ❌ | ❌ | ❌ | **P1** |
| **KryoFlux Stream** | ✅ 649 Z. | ❌ | ❌ | N/A | **P2** |
| **A2R** (Apple) | ✅ 406 Z. | ❌ | ❌ | ❌ | **P3** |
| **WOZ** (Apple) | ✅ 763 Z. | ❌ | ❌ | ❌ | **P3** |
| **IPF** | ❌ | ❌ | ❌ | ❌ | **P4** |

---

## 3. HOCH - Decoder Plugins

| Encoding | Header | Plugin | Decode | Encode | Auto-Detect | Priorität |
|----------|--------|--------|--------|--------|-------------|-----------|
| **IBM MFM** | ✅ | ⚠️ 70% | ✅ | ❌ | ✅ | **P1** |
| **IBM FM** | ✅ | ⚠️ 50% | ✅ | ❌ | ❌ | **P2** |
| **Amiga MFM** | ✅ 313 Z. | ❌ | ❌ | ❌ | ❌ | **P1** |
| **GCR C64** | ✅ 374 Z. | ❌ | ❌ | ❌ | ❌ | **P2** |
| **GCR Apple** | ✅ 349 Z. | ❌ | ❌ | ❌ | ❌ | **P3** |

---

## 4. MITTEL - Hardware Drivers

| Hardware | Header | Driver | Read | Write | Priorität |
|----------|--------|--------|------|-------|-----------|
| **Greaseweazle** | ✅ 343 Z. | ❌ | ❌ | ❌ | **P2** |
| **KryoFlux** | ✅ (Teil) | ❌ | ❌ | N/A | **P3** |
| **SuperCard Pro** | ❌ | ❌ | ❌ | ❌ | **P3** |
| **FluxEngine** | ✅ 599 Z. | ❌ | ❌ | ❌ | **P4** |

---

## 5. MITTEL - Qt GUI Integration

| Komponente | Header | Impl. | Status | Priorität |
|------------|--------|-------|--------|-----------|
| **TrackGridWidget** | ✅ 738 Z. | ❌ | Header fertig | **P2** |
| **AmigaPanel** | ✅ 492 Z. | ❌ | Header fertig | **P2** |
| **MainWindow** | ⚠️ | ❌ | Konzept | **P2** |
| **FormatConverter Dialog** | ❌ | ❌ | TODO | **P3** |
| **Settings Dialog** | ❌ | ❌ | TODO | **P3** |
| **Hardware Selection** | ❌ | ❌ | TODO | **P3** |

---

## 6. NIEDRIG - Spezial-Features

| Feature | Status | Priorität |
|---------|--------|-----------|
| Copy Protection Detection | Header ✅ | P4 |
| Virus Scanner (XVS-style) | Header ✅ | P4 |
| DiskSalv Recovery | Header ✅ | P4 |
| Forensic Imaging | Header ✅ | P4 |
| SIMD Optimization | ⚠️ Teilweise | P4 |

---

## 7. Build-System

| Task | Status |
|------|--------|
| CMakeLists.txt (Root) | ⚠️ Basis vorhanden |
| CMake für Core | ❌ TODO |
| CMake für Plugins | ❌ TODO |
| CMake für Qt GUI | ❌ TODO |
| CI/CD Pipeline | ❌ TODO |
| Unit Tests | ❌ TODO |
| Doxygen Config | ❌ TODO |

---

## Zusammenfassung: Nächste Schritte

### Sprint 1 (1-2 Wochen): Foundation
1. [ ] `uft_types.c` implementieren
2. [ ] `uft_error.c` implementieren  
3. [ ] `uft_core.c` Grundgerüst
4. [ ] Plugin Registry implementieren
5. [ ] ADF Format Plugin (einfachstes Format)

### Sprint 2 (1-2 Wochen): Verbinden
1. [ ] SCP Plugin fertigstellen
2. [ ] IBM MFM Decoder als Plugin registrieren
3. [ ] Format-Konvertierung: SCP → ADF
4. [ ] Basis-CLI Tool

### Sprint 3 (2 Wochen): GUI
1. [ ] Qt MainWindow Grundgerüst
2. [ ] TrackGrid Widget implementieren
3. [ ] AmigaPanel implementieren
4. [ ] File Open/Save Dialoge

### Sprint 4 (2 Wochen): Erweiterung
1. [ ] HFE Format
2. [ ] Amiga MFM Decoder
3. [ ] Hardware-Anbindung (Greaseweazle)
4. [ ] Weitere Formate nach Bedarf


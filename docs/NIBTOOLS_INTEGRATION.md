# UFT R46: NIBTOOLS Integration

## Übersicht

Integration von C64/1541 Track-Alignment und Format-Tabellen aus dem
nibtools-Projekt (C64 Preservation Project) in UnifiedFloppyTool.

**Quelle:** https://github.com/OpenCBM/nibtools
**Lizenz:** GPL
**Original-Autoren:** Pete Rittwage, Markus Brenner

## Neue Module

### 1. uft_c64_track_align.h/.c (1031 Zeilen)

Track-Alignment-Funktionen für C64/1541 Disk-Preservation.

**Unterstützte Protection Schemes:**

| Scheme | Funktion | Beschreibung |
|--------|----------|--------------|
| V-MAX! | `uft_c64_align_vmax()` | Marker: $4B, $49, $69, $5A, $A5 |
| V-MAX Cinemaware | `uft_c64_align_vmax_cw()` | Pattern: $64 $A5 $A5 $A5 |
| PirateSlayer/EA | `uft_c64_align_pirateslayer()` | Signature: $D7 $D7 $EB $CC $AD |
| Rapidlok | `uft_c64_align_rapidlok()` | Track Header mit RL Version Detection |
| Long Sync | `uft_c64_align_longsync()` | Längste Sync-Sequenz |
| Auto Gap | `uft_c64_align_autogap()` | Längste Gap-Region |
| Bad GCR | `uft_c64_align_badgcr()` | Ungültige GCR-Bereiche |

**Zusätzliche Features:**

- Fat Track Detection (`uft_c64_search_fat_tracks()`)
- Sync Alignment für Kryoflux/Raw Streams (`uft_c64_sync_align()`)
- Buffer-Manipulation (shift left/right, rotate)
- GCR-Validierung (`uft_c64_is_bad_gcr()`, `uft_c64_count_bad_gcr()`)
- Auto-Alignment mit Priorität (`uft_c64_auto_align()`)

**Rapidlok Spezial:**
- Erkennung von Rapidlok Versionen 1-7
- TV-Standard Detection (NTSC/PAL)
- Track Header Length Analyse

### 2. uft_c64_track_format.h/.c (587 Zeilen)

1541 Track-Format-Tabellen und Konstanten.

**Tabellen:**

| Tabelle | Funktion | Beschreibung |
|---------|----------|--------------|
| sector_map | `uft_c64_sectors_per_track()` | Sektoren pro Track (17-21) |
| speed_map | `uft_c64_speed_zone()` | Speed-Zonen (0-3) |
| gap_map | `uft_c64_sector_gap_length()` | Inter-Sector Gap Längen |
| capacity | `uft_c64_track_capacity()` | Track-Kapazität bei RPM |

**Speed-Zonen:**

| Zone | Tracks | Sektoren | Datenrate | Kapazität @300RPM |
|------|--------|----------|-----------|-------------------|
| 3 | 1-17 | 21 | 307692 bps | ~7692 Bytes |
| 2 | 18-24 | 19 | 285714 bps | ~7143 Bytes |
| 1 | 25-30 | 18 | 266667 bps | ~6667 Bytes |
| 0 | 31-42 | 17 | 250000 bps | ~6250 Bytes |

**D64 Funktionen:**
- `uft_c64_ts_to_block()` - Track/Sector → Block
- `uft_c64_block_to_ts()` - Block → Track/Sector
- `uft_c64_d64_offset()` - Byte-Offset in D64

**DOS Error Codes:**
- Alle 1541 DOS Error Codes als Enum
- Bitflags für Track-Analyse

## Integration

Die Module sind in `libflux_core` integriert:

```
libflux_core/
├── include/c64/
│   ├── uft_c64_track_align.h    (360 Zeilen)
│   └── uft_c64_track_format.h   (309 Zeilen)
└── src/c64/
    ├── uft_c64_track_align.c    (671 Zeilen)
    └── uft_c64_track_format.c   (278 Zeilen)
```

## Verwendungsbeispiel

```c
#include "c64/uft_c64_track_align.h"
#include "c64/uft_c64_track_format.h"

// Track-Daten analysieren
uint8_t track_buffer[UFT_NIB_TRACK_LENGTH];
size_t track_len = /* read from G64/NIB */;

// Auto-Alignment
uft_c64_align_result_t result;
uft_c64_align_method_t method = uft_c64_auto_align(track_buffer, track_len, &result);

printf("Alignment: %s at offset %zu\n", 
       uft_c64_align_method_name(method), 
       result.offset);

if (result.method == UFT_C64_ALIGN_RAPIDLOK && result.rl_version > 0) {
    printf("Rapidlok v%d detected (%s)\n",
           result.rl_version,
           result.rl_tv == UFT_RL_TV_NTSC ? "NTSC" : "PAL");
}

// Track-Format abfragen
int track = 18;
printf("Track %d: %d sectors, zone %d, gap %d bytes\n",
       track,
       uft_c64_sectors_per_track(track),
       uft_c64_speed_zone(track),
       uft_c64_sector_gap_length(track));
```

## Ursprung aus nibtools

### Übernommene Funktionen:

| nibtools | UFT | Status |
|----------|-----|--------|
| `align_vmax()` | `uft_c64_align_vmax()` | ✅ Portiert |
| `align_vmax_cw()` | `uft_c64_align_vmax_cw()` | ✅ Portiert |
| `align_pirateslayer()` | `uft_c64_align_pirateslayer()` | ✅ Portiert |
| `align_rl_special()` | `uft_c64_align_rapidlok()` | ✅ Vereinfacht |
| `find_long_sync()` | `uft_c64_align_longsync()` | ✅ Portiert |
| `auto_gap()` | `uft_c64_align_autogap()` | ✅ Portiert |
| `find_bad_gap()` | `uft_c64_align_badgcr()` | ✅ Portiert |
| `sync_align()` | `uft_c64_sync_align()` | ✅ Portiert |
| `search_fat_tracks()` | `uft_c64_search_fat_tracks()` | ✅ Portiert |
| `shift_buffer_left/right()` | `uft_c64_shift_buffer_left/right()` | ✅ Portiert |
| `sector_map[]` | `uft_c64_get_sector_map()` | ✅ Portiert |
| `speed_map[]` | `uft_c64_get_speed_map()` | ✅ Portiert |
| `sector_gap_length[]` | `uft_c64_get_gap_map()` | ✅ Portiert |

### Nicht übernommen (außerhalb Scope):

- Hardware-Kommunikation (OpenCBM, XUM1541)
- Disk-Schreibfunktionen
- NIB/NBZ Dateiformat-IO

## Audit

### Sicherheit:
- ✅ Bounds-Checking in allen Funktionen
- ✅ NULL-Pointer Checks
- ✅ Keine dynamische Allokation außer temporären Buffern
- ✅ `malloc()`-Fehlerbehandlung

### Portabilität:
- ✅ C99/C11 kompatibel
- ✅ Keine plattformspezifischen Aufrufe
- ✅ Keine Endianness-Probleme (nur Byte-Operationen)

### Wartbarkeit:
- ✅ Vollständige Doxygen-Dokumentation
- ✅ Konsistentes `uft_c64_` Präfix
- ✅ Enum statt Magic Numbers
- ✅ Statische Tabellen (kein Init nötig)

## TODO

- [ ] Unit Tests für alle Alignment-Funktionen
- [ ] Golden Tests mit echten G64-Images
- [ ] Integration in Protection-Analyzer-Pipeline
- [ ] GUI-Widget für Track-Alignment-Visualisierung

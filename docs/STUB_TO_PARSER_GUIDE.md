# Einweisung: Stub-Parser → echte Implementierung
**Für:** Claude Code Agent  
**Projekt:** UnifiedFloppyTool 4.1.0  
**Aufgabe:** 287 Format-Stubs in vollständige Parser umwandeln  
**Oberste Regel:** Kein neuer Compile-Error. Kein neues Sicherheitsrisiko. Kein verändertes Verhalten bestehender Funktionen.

---

## 0. Lies das zuerst — bevor du eine einzige Zeile schreibst

Dieses Projekt hat eine Eigenheit: Es gibt **zwei verschiedene Parser-Patterns** die beide im Build existieren:

**Pattern A — Standalone** (`_parse()` + `#ifdef FORMAT_V3_TEST`)  
→ Datei-Namensschema: `uft_FORMAT_parser_v3.c`  
→ Kein Plugin-Struct. Wird intern genutzt aber nicht über die Plugin-API registriert.  
→ Stubs dieser Art haben struct-Definitionen und parse-Skelett, aber extrahieren keine echten Daten.

**Pattern B — Plugin** (`probe()` + `open()` + `read_track()` + `UFT_REGISTER_FORMAT_PLUGIN`)  
→ Datei-Namensschema: `uft_FORMAT.c`  
→ Registriert sich über `uft_format_plugin_t`. Wird von der GUI über die Plugin-API aufgerufen.  
→ Echte Parser sind **immer Pattern B**. IMD, ADF, DSK sind Beispiele.

**Deine Aufgabe:** Einen Stub (Pattern A, ~60-353 LOC) in einen echten Parser (Pattern B, ~200-1400 LOC) verwandeln.  
Das bedeutet: neue Datei schreiben, nicht die Stub-Datei editieren.

---

## 1. Checkliste vor dem ersten Tastendruck

Führe diese Checks für jedes Format durch, bevor du implementierst:

```bash
# 1. Existiert bereits eine Pattern-B Implementierung?
find src/formats/FORMAT/ -name "uft_FORMAT.c" -o -name "uft_FORMAT_plugin.c"
# → Wenn ja: nur ergänzen, nicht neu schreiben

# 2. Ist das Format im Build?
grep "FORMAT" UnifiedFloppyTool.pro | grep "SOURCES"
# → Zeile merken - du musst die neue Datei dort eintragen

# 3. Gibt es Basename-Kollisionen?
find src/ -name "$(basename NEUE_DATEI)" | wc -l
# → Muss 0 sein! Bei Kollision: anderer Dateiname

# 4. Gibt es eine Spec-Quelle?
# Prüfe: src/formats/FORMAT/*.h auf Kommentare mit Spec-Referenzen
# Prüfe: CLAUDE.md auf Format-spezifische Hinweise
# Nutze: info-finder Agent für öffentliche Specs

# 5. Gibt es Tests?
find tests/ -name "test_FORMAT*"
# → Wenn ja: Tests erst lesen, dann implementieren
```

---

## 2. Das unveränderliche UFT-Regelwerk

Diese Regeln sind nicht verhandelbar. Kein Ausnahme.

### 2.1 Build-System

```
PFLICHT: Jede neue .c-Datei MUSS in UnifiedFloppyTool.pro unter SOURCES eingetragen werden.
         Fehlender Eintrag = Datei wird silently ignoriert. Kein Fehler, keine Warnung.

PFLICHT: CONFIG += object_parallel_to_source ist gesetzt und muss es bleiben.
         Grund: 35+ Dateinamen-Kollisionen im Projekt (z.B. crc.c existiert 12x).
         Neue Dateien dürfen KEINEN basename haben der schon existiert.
```

Prüfe vor dem Hinzufügen:
```bash
NEW_FILE="src/formats/MYFORMAT/uft_myformat.c"
BASENAME=$(basename "$NEW_FILE")
EXISTING=$(find src/ -name "$BASENAME" | wc -l)
echo "Basename '$BASENAME' bereits $EXISTING mal vorhanden"
# Muss 0 sein
```

Eintrag in `.pro`:
```qmake
# Alphabetisch sortiert im entsprechenden SOURCES-Block eintragen:
    src/formats/myformat/uft_myformat.c \
```

### 2.2 I/O — Eiserne Regel: Jeder fread/fseek wird geprüft

Das Projekt hat 997 geprüfte I/O-Calls und 0 ungeprüfte. Diese Quote hält.

```c
/* VERBOTEN */
fread(buf, 1, size, f);
fseek(f, offset, SEEK_SET);

/* PFLICHT */
if (fread(buf, 1, size, f) != size) {
    fclose(f);
    free(pdata);
    return UFT_ERROR_IO;
}
if (fseek(f, offset, SEEK_SET) != 0) {
    fclose(f);
    free(pdata);
    return UFT_ERROR_IO;
}
```

Einzige Ausnahme: wenn der Fehlerfall harmlos ist (z.B. optionaler Chunk), dann:
```c
(void)fseek(f, optional_skip, SEEK_CUR);  /* (void) cast zeigt: bewusst ignoriert */
```

### 2.3 Speicher-Verwaltung

```c
/* Jedes malloc wird auf NULL geprüft */
uint8_t* buf = malloc(size);
if (!buf) return UFT_ERROR_NO_MEMORY;

/* Jedes malloc hat einen Free-Pfad für ALLE Fehlerpfade */
pdata = calloc(1, sizeof(my_data_t));
if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

/* calloc statt malloc+memset für Structs */
my_struct_t* s = calloc(1, sizeof(my_struct_t));  /* Initialisiert auf 0 */
```

### 2.4 String-Handling

```c
/* VERBOTEN: truncation-Warnung und Sicherheitslücke */
strncpy(dst, src, sizeof(dst));

/* RICHTIG: explizit null-terminieren */
strncpy(dst, src, sizeof(dst) - 1);
dst[sizeof(dst) - 1] = '\0';

/* ODER: snprintf für formatierte Strings */
snprintf(dst, sizeof(dst), "%s", src);
```

### 2.5 Integer-Arithmetik

```c
/* VERBOTEN: integer overflow möglich */
uint32_t offset = track * sectors_per_track * sector_size;

/* RICHTIG: Overflow-Check bei Berechnungen mit user-controlled Werten */
if (track >= MAX_TRACKS || sectors_per_track > MAX_SECTORS) return UFT_ERROR_FORMAT_INVALID;
uint32_t offset = (uint32_t)track * sectors_per_track * sector_size;
```

### 2.6 Bounds-Check vor jedem Datenzugriff

```c
/* VERBOTEN */
uint32_t val = read_le32(data + offset);

/* PFLICHT */
if (offset + 4 > size) return UFT_ERROR_FORMAT_TRUNCATED;
uint32_t val = read_le32(data + offset);
```

### 2.7 Keine neuen Makro-Konflikte

Die Header `uft_platform.h` und `uft_error.h` definieren bereits:
`UFT_PACKED`, `UFT_PACK_BEGIN`, `UFT_PACK_END`, `UFT_API`, `UFT_COMPILER_VERSION`,
`UFT_ERROR_NO_MEMORY`, `UFT_ERROR_NOT_SUPPORTED`, `UFT_ERROR_IO`, `UFT_ERROR_CRC`, `UFT_ERROR_INTERNAL`

```c
/* VERBOTEN: Diese Makros NICHT redefinieren */
#define UFT_PACKED __attribute__((packed))  /* Schon da! */
#define UFT_ERROR_CRC 0x01                  /* Schon da! */

/* Neue Makros: Format-spezifisches Prefix */
#define FDS_HEADER_SIZE   16    /* OK: Format-Prefix */
#define IMD_MAGIC         0x1A  /* OK */
```

---

## 3. Die Plugin-Schnittstelle — das Ziel jedes echten Parsers

Jeder fertige Parser implementiert diese Funktionen und registriert sich:

```c
#include "uft/uft_format_common.h"

/* ── PROBE ─────────────────────────────────────────────────────────────── */
/* Gibt confidence 0-100 zurück. Schnell, kein malloc, kein fopen.          */
bool FORMAT_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (size < MINIMUM_HEADER_SIZE) return false;
    if (memcmp(data, FORMAT_MAGIC, 4) == 0) {
        *confidence = 95;
        return true;
    }
    return false;
}

/* ── OPEN ──────────────────────────────────────────────────────────────── */
/* Öffnet Datei, liest Geometrie, speichert State in disk->plugin_data.     */
static uft_error_t FORMAT_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    format_data_t* pdata = calloc(1, sizeof(format_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    /* Header lesen + validieren */
    uint8_t hdr[HEADER_SIZE];
    if (fread(hdr, 1, HEADER_SIZE, f) != HEADER_SIZE) {
        free(pdata); fclose(f); return UFT_ERROR_IO;
    }
    if (memcmp(hdr, FORMAT_MAGIC, 4) != 0) {
        free(pdata); fclose(f); return UFT_ERROR_FORMAT_INVALID;
    }

    pdata->file = f;
    pdata->tracks = hdr[TRACKS_OFFSET];
    pdata->sides  = hdr[SIDES_OFFSET];
    /* ... weitere Felder ... */

    disk->plugin_data = pdata;
    disk->geometry.cylinders   = pdata->tracks;
    disk->geometry.heads       = pdata->sides;
    disk->geometry.sectors     = SECTORS_PER_TRACK;
    disk->geometry.sector_size = SECTOR_SIZE;

    return UFT_OK;
}

/* ── CLOSE ─────────────────────────────────────────────────────────────── */
static void FORMAT_close(uft_disk_t* disk) {
    format_data_t* pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ── READ_TRACK ────────────────────────────────────────────────────────── */
/* Liest einen Track. Nutzt uft_format_add_sector() für jeden Sektor.       */
static uft_error_t FORMAT_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    format_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Seek zu Spur-Position */
    long offset = FORMAT_track_offset(pdata, cyl, head);
    if (fseek(pdata->file, offset, SEEK_SET) != 0) return UFT_ERROR_IO;

    /* Sektoren lesen */
    uint8_t buf[MAX_SECTOR_SIZE];
    for (int s = 0; s < pdata->sectors_per_track; s++) {
        if (fread(buf, 1, pdata->sector_size, pdata->file) != pdata->sector_size) {
            return UFT_ERROR_IO;
        }
        uft_format_add_sector(track, s, buf, pdata->sector_size, cyl, head);
    }

    return UFT_OK;
}

/* ── PLUGIN-STRUCT ─────────────────────────────────────────────────────── */
const uft_format_plugin_t uft_format_plugin_FORMAT = {
    .name         = "FORMAT",
    .description  = "Beschreibung des Formats (Plattform, Autor)",
    .extensions   = "ext1,ext2",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,    /* oder passender Wert */
    .capabilities = UFT_FORMAT_CAP_READ,
    .probe        = FORMAT_probe,
    .open         = FORMAT_open,
    .close        = FORMAT_close,
    .read_track   = FORMAT_read_track,
};

UFT_REGISTER_FORMAT_PLUGIN(FORMAT)
```

---

## 4. Schritt-für-Schritt Ablauf pro Format

### Schritt 1: Spec finden (NICHT überspringen)

Ohne Spec keine korrekte Implementierung. Quellen in dieser Reihenfolge:

1. **Im Stub selbst**: Kommentare oben oft mit Referenzen
2. **Im Repo**: `src/formats/FORMAT/*.h` — oft komplette Struct-Definitionen der Spec
3. **Andere echte Parser im Projekt** die ähnliche Formate haben (z.B. D64→D71, ADF→ADL)
4. **Öffentliche Quellen**: The Undocumented PC, Zimmers.net, format.sh, VICE-Quellcode

Erst wenn die Spec klar ist: anfangen.

### Schritt 2: Stub analysieren — was ist schon da?

```bash
# Stub lesen
cat src/formats/FORMAT/uft_FORMAT_parser_v3.c

# Was hat der Stub?
# ✓ Struct-Definitionen (oft gut)
# ✓ Magic-Konstanten (meistens korrekt)
# ✓ Header-Parsing (oft korrekt aber unvollständig)
# ✗ Echte Daten-Extraktion (fehlt immer)
# ✗ Plugin-Struct (fehlt immer)
# ✗ Vollständige Geometrie-Erkennung (meist stub)
```

### Schritt 3: Neue Datei anlegen (nicht Stub editieren)

```
Dateiname: src/formats/FORMAT/uft_FORMAT.c
           NICHT: uft_FORMAT_parser_v3.c (der bleibt als Referenz)
           NICHT: uft_FORMAT_v2.c (Versionsnummern sind Chaos)
```

### Schritt 4: Implementierung in dieser Reihenfolge

```
1. probe()         — 5-10 Zeilen, Magic-Check
2. struct           — plugin_data Struct definieren
3. open()          — Header lesen, Geometrie ermitteln
4. close()         — Resources freigeben
5. read_track()    — Sektoren extrahieren
6. Plugin-Struct   — uft_format_plugin_FORMAT
7. REGISTER-Makro  — UFT_REGISTER_FORMAT_PLUGIN(FORMAT)
```

### Schritt 5: In .pro eintragen

```bash
# Neue Zeile in UnifiedFloppyTool.pro einfügen
# Alphabetisch sortiert im FORMAT-Block:
    src/formats/FORMAT/uft_FORMAT.c \

# Prüfen ob Basename-Kollision:
find src/ -name "uft_FORMAT.c" | wc -l
# Muss 1 ergeben (nur die neue Datei)
```

### Schritt 6: Build-Test

```bash
export PATH="/path/to/Qt/6.7.3/gcc_64/bin:$PATH"
mkdir -p build_test && cd build_test
qmake ../UnifiedFloppyTool.pro CONFIG+=release 2>&1 | grep -v "locale"
make -j$(nproc) 2>&1 | grep "error:\|warning:" | grep -v "^#\|//"
# Ziel: 0 neue Errors, 0 neue Warnings
```

### Schritt 7: Test schreiben (wenn Test-Datei existiert)

```bash
# Prüfe ob Test existiert
ls tests/test_FORMAT*

# Wenn ja: Test erweitern
# Wenn nein: Minimalen Test anlegen
# Mindestanforderung: probe() + open() + ein read_track() Test
```

---

## 5. Prioritätsliste der 287 Stubs

### Tier 1 — Sofort machbar (simple Formate, spec öffentlich, ~200-400 LOC)

Diese zuerst angehen. Hoher Nutzen, geringes Risiko:

| Format | Aktuell | Erwartet | Schwierigkeit | Warum wertvoll |
|---|---|---|---|---|
| `fds` | 76 LOC | ~400 LOC | Einfach | Famicom Disk System, häufig in Preservation |
| `d77` | 73 LOC | ~300 LOC | Einfach | PC-88/FM-7 Standard, klare Spec |
| `st` | 72 LOC | ~200 LOC | Trivial | Atari ST raw dump, nur Geometrie |
| `edsk` | 69 LOC | ~400 LOC | Mittel | Extended DSK, fast wie DSK aber mehr |
| `cas` | 77 LOC | ~250 LOC | Einfach | Tape image, Byte-für-Byte |
| `dc42` | 77 LOC | ~350 LOC | Mittel | Apple DiskCopy 4.2, Mac-Preservation |
| `mfi` | 61 LOC | ~300 LOC | Mittel | MAME Floppy Image |
| `dim` | 69 LOC | ~350 LOC | Mittel | DIM (Japanese PC format) |

### Tier 2 — Mittlere Komplexität (~400-800 LOC)

| Format | Aktuell | Schwierigkeit | Hinweis |
|---|---|---|---|
| `a2r` | 108 LOC | Mittel | Flux-Timing-Daten. Applesauce Spec öffentlich |
| `woz` | 353 LOC | Mittel | Bitstream-Parsing. Spec sehr gut dokumentiert |
| `chd` | 60 LOC | Schwer | Hunk-Dekompression. libchd als Referenz |
| `kfx` | 1508 LOC | Mittel | Gut angefangen, Fertigstellung |
| `pri` | 80 LOC | Mittel | PCE Format, niche aber vollständig spezifiziert |

### Tier 3 — Nicht anfassen (Kompression oder proprietär)

Diese gehören nicht in Tier 1 oder 2. Falsche Implementierung ist schlimmer als Stub:

`dms` (Amiga Diskmasher — proprietäre Kompression)  
`lzh` (LHA-Kompression — Lizenzfragen)  
`gz` / `bz2` / `xz` / `lz4` / `zstd` (System-Libraries nutzen, nicht neu implementieren)  
`chd` (MAME — nur wenn libchd als Systemabhängigkeit akzeptiert wird)

---

## 6. Format-spezifische Hinweise

### FDS (Famicom Disk System)

```
Spec: https://wiki.nesdev.org/w/index.php/FDS_file_format
Stub hat: fwNES-Header-Check, Nintendo-Signatur-Check (korrekt)
Fehlt:    Seiten-Iteration, Datei-Header-Parsing, BAM-Rekonstruktion
Besonderheit: Zwei Varianten — mit fwNES 16-Byte-Header und raw
Sektorgröße: 512 Byte oder variabel je nach Datei
Referenz im Projekt: src/formats/misc/fds.c (lesen!)
```

### D77 (PC-88, FM-7, Sharp X1)

```
Spec: recherchierbar über "D77 format specification"
Stub hat: Nichts außer Struct
Aufbau: 2608-Byte-Header + Track-Offset-Tabelle + Track-Daten
Sektorgröße: 128/256/512/1024 (size_code in jedem Sektor-Header)
Besonderheit: Big-endian-Headerfelder
```

### ST (Atari ST Raw Dump)

```
Spec: Trivial — kein Header, nur rohe Sektordaten
Geometrie: aus Dateigröße ableiten
DD: 80×2×9×512 = 737280 Byte
HD: 80×2×18×512 = 1474560 Byte
ED: 80×2×36×512 = 2949120 Byte
Implementierung: probe() nach Dateigröße, read_track() = direkter Offset
```

### WOZ (Apple II Flux)

```
Spec: https://applesaucefdc.com/woz/reference2/
Stub hat: Chunk-Parser (korrekt!), Struct-Definitionen (gut)
Fehlt:    tracks[t].bits wird nie befüllt — das ist der Kern-Bug
Fix:      Bei TRKS-Chunk: data + start_block*512 als bits-pointer setzen
          bit_count aus TRKS-Entry lesen (schon da im Stub)
Achtung:  WOZ 1.0 hat anderes TRKS-Format als WOZ 2.0
```

### A2R (Applesauce Flux)

```
Spec: https://applesaucefdc.com/a2r/
Stub hat: Chunk-Iterator (korrekt), stream_count (aber ignoriert)
Fehlt:    STRM-Chunk auslesen, flux_transitions Array befüllen
          SLVD-Chunk (solved data) als Fallback
Hinweis:  A2R ist hauptsächlich für Flux-Analyse, kein Sektor-Format
          read_track() gibt Raw-Flux, nicht Sektor-Daten
```

### EDSK (Extended CPC DSK)

```
Spec: https://www.cpcwiki.eu/index.php/Format:EDSK_disk_image_file_format
Referenz im Projekt: src/formats/dsk_cpc/uft_dsk_cpc.c (sehr ähnlich!)
Unterschied zu DSK: Track-Größen variabel (256-Byte-Einträge im Header)
Stub hat: EXTENDED-Magic-Check (korrekt)
Basis: Von dsk_cpc.c ausgehend implementieren
```

---

## 7. Häufige Fehler — und wie man sie vermeidet

### Fehler 1: Alte Stub-Funktionen nicht aus .pro entfernen

```
Problem:  uft_FORMAT_parser_v3.c und uft_FORMAT.c beide in SOURCES
          → Linker-Fehler durch doppelte Definitionen
Fix:      Wenn neue Plugin-Datei fertig: Stub aus SOURCES entfernen
          ODER: Stub beibehalten als Referenz, aber aus SOURCES löschen
```

### Fehler 2: fseek-Rückgabe ignorieren in read_track()

```c
/* TYPISCHER STUB-FEHLER: */
fseek(f, track_offset, SEEK_SET);  /* Kein Check! */

/* Kann crash verursachen wenn Datei kürzer als erwartet */
/* FIX: */
if (fseek(f, track_offset, SEEK_SET) != 0) {
    return UFT_ERROR_IO;
}
```

### Fehler 3: pdata->file nach close() noch benutzen

```c
/* BUG: Dangling pointer */
static void FORMAT_close(uft_disk_t* disk) {
    format_data_t* pdata = disk->plugin_data;
    fclose(pdata->file);
    free(pdata);
    /* disk->plugin_data = NULL fehlt! */
}

/* FIX: */
disk->plugin_data = NULL;  /* Immer am Ende von close() */
```

### Fehler 4: Geometrie aus kaputten Daten ableiten

```c
/* GEFÄHRLICH: unvalidierter Wert aus Header direkt verwendet */
disk->geometry.cylinders = hdr[TRACKS_OFFSET];  /* Was wenn 0 oder 255? */

/* SICHER: */
uint8_t tracks = hdr[TRACKS_OFFSET];
if (tracks == 0 || tracks > 84) {  /* Physikalisches Maximum */
    free(pdata); fclose(f);
    return UFT_ERROR_FORMAT_INVALID;
}
disk->geometry.cylinders = tracks;
```

### Fehler 5: C++ inkompatibler Header

```c
/* PROBLEM: In C ist 'protected' kein Keyword, in C++ schon */
typedef struct {
    int protected;  /* OK in C, FEHLER in C++ */
} my_t;

/* FIX: */
typedef struct {
    int protected_flag;  /* Umbenennen */
} my_t;
```

### Fehler 6: Vergessen dass object_parallel_to_source aktiv ist

```
Symptom:  make läuft durch, aber Änderungen haben keine Wirkung
Ursache:  Datei hat gleichen basename wie existierende Datei,
          .o-Datei wird aus falschem Verzeichnis genommen
Diagnose: find src/ -name "$(basename MEINE_DATEI)"
Fix:      Eindeutigen Namen wählen
```

---

## 8. Qualitäts-Gate

Ein Stub gilt als **fertig konvertiert** wenn:

- [ ] Compile: `make` produziert **0 neue Errors, 0 neue Warnings** (relativ zum Stand vor dem Format)
- [ ] probe(): Erkennt valide Dateien mit confidence ≥ 90, lehnt invalide mit `false` ab
- [ ] open(): Korrekte Geometrie für alle bekannten Varianten (DD/HD/etc.)
- [ ] read_track(): Alle Sektoren korrekt extrahiert (nachweisbar mit Test-Disk-Image)
- [ ] Fehlerbehandlung: Abgeschnittene/korrupte Dateien crashen nicht, geben UFT_ERROR_* zurück
- [ ] Memory: kein `valgrind`-Fund bei normalem Durchlauf (oder gleichwertige manuelle Prüfung)
- [ ] `.pro`-Datei: Neue Datei eingetragen, Stub ausgetragen
- [ ] Test: Mindestens `probe()` und `open()` mit synthetischer Minimal-Disk getestet

---

## 9. Was du NICHT tun sollst

```
✗ Stub-Dateien inhaltlich ändern (sie sind Referenz, nicht Basis)
✗ Neue Formate erfinden die nicht in der Stub-Liste stehen
✗ Bestehende echte Parser (>500 LOC) "verbessern" — zu hohes Regressionsrisiko
✗ Drittbibliotheken einbinden ohne Rücksprache (zlib, lzma, etc.)
✗ Non-Floppy-Formate implementieren (mp3, pdf, etc.) — das ist Scope-Drift
✗ Mehr als einen Stub gleichzeitig bearbeiten — ein Format komplett, dann nächstes
✗ Probe-Funktion mit > 20ms Laufzeit — sie wird für jede Datei aufgerufen
✗ Globale Variablen in Plugin-Dateien — nicht thread-safe
```

---

## 10. Referenz-Implementierungen zum Kopieren

Für jeden neuen Parser: zuerst einen echten Parser mit ähnlicher Komplexität lesen.

| Dein Format | Lerne von | Warum |
|---|---|---|
| Simples Sektor-Image | `src/formats/imd/` | Klar, gut kommentiert, komplett |
| Header + Tracks | `src/formats/d88/` | Variante Sektorgröße, gut strukturiert |
| Amiga-ähnlich | `src/formats/adf/` | Big-Endian, Dateisystem-Parsing |
| Flux-Format | `src/formats/kryoflux/` | Stream-Parsing, OOB-Handling |
| Komprimiert | `src/formats/imz/` | Zeigt wie Dekompression eingehängt wird |
| Mit Varianten (v1/v2) | `src/formats/hfe/` | Versionierte Formate |

---

*Dieses Dokument basiert auf direkter Analyse von 533 kompilierten Objekten, allen 287 Stub-Dateien und 12 vollständigen echten Parsern im Projekt.*

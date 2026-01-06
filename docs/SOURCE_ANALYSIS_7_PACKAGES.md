# Sourcecode-Analyse: 7 Pakete (Batch 2)

**Datum:** 2026-01-06  
**Analysiert:** UFT CODE-ANALYSE Rolle

---

## Übersicht

| Paket | LOC | Sprache | Relevanz | Highlight |
|-------|-----|---------|----------|-----------|
| **wav-prg-4.2.1** | 14.981 | C | ⭐⭐⭐ HOCH | **49 Tape-Loader-Decoder!** |
| **c64tapedecode** | 2.742 | C | ⭐⭐⭐ HOCH | TAP/WAV Decoding |
| **crt_format** | 241 | C | ⭐⭐⭐ HOCH | CRT (Cartridge) Format |
| **loader306/229** | ~55K | ASM | ⭐⭐ Referenz | Covert Bitops Fastloader |
| **DiskImageTool** | ~50K | VB.NET | ⭐ Niedrig | Windows GUI Tool |
| **tsdump** | 86 | C | ⭐ Niedrig | D64 Track/Sector Dump |

---

## 1. wav-prg-4.2.1 (14.981 LOC) ⭐⭐⭐

### 🔴 MASSIVES TAPE-DECODER-PROJEKT!

**49 Loader-Decoder inkludiert:**

| Kategorie | Loader |
|-----------|--------|
| **Turbo-Loader** | turbotape, turbo220, flashload, microload, novaload |
| **Publisher-Loader** | ocean, empire, audiogenic, atlantis, chr |
| **Spezial-Loader** | jetload, dragonload, thunderload, visiload |
| **Custom** | alien, anirog, burner, cult, jedi, snake, etc. |

### Architektur

```
wav-prg-4.2.1/
├── wav2prg_core/          # Core-Bibliothek (8.522 LOC)
│   ├── wav2prg_core.c     # 36 KB - Haupt-Decoder
│   ├── get_pulse.c        # Puls-Extraktion
│   ├── loaders.c          # Loader-Management
│   └── create_t64.c       # T64 Export
├── prg2wav_core/          # PRG→WAV Encoder
├── loaders/               # 49 Loader-Plugins
└── common_core/           # Shared Code
```

### API (wav2prg_api.h)

```c
// Plugin-Architektur für Tape-Loader
struct wav2prg_plugin_functions {
    wav2prg_get_bit_func get_bit_func;
    wav2prg_get_byte_func get_byte_func;
    wav2prg_get_sync_check get_sync;
    wav2prg_get_block_info get_block_info;
    wav2prg_get_block_func get_block_func;
    wav2prg_compute_checksum_step compute_checksum_step;
};

// Konfiguration pro Loader
struct wav2prg_plugin_conf {
    enum wav2prg_plugin_endianness endianness;  // LSB/MSB first
    enum wav2prg_checksum checksum_type;        // XOR/ADD
    uint8_t num_pulse_lengths;
    uint16_t *thresholds;
    uint8_t pilot_byte;
    uint8_t *sync_sequence;
    // ...
};
```

### Lizenz: GPL ⚠️

Nicht direkt übernehmbar, aber exzellente Referenz für:
- Pulse-Threshold-Algorithmen
- Loader-Erkennung
- Sync-Pattern-Matching

---

## 2. c64tapedecode (2.742 LOC) ⭐⭐⭐

### TAP Format Support

```c
// tap.c - Core TAP Handling
static const char tapmagic[] = "C64-TAPE-RAW";

int tap_read_header(struct tap *t, FILE *f);
int tap_write_header(struct tap *t, FILE *f);
long tap_get_pulse(struct tap *t);
int tap_put_pulse(long len, struct tap *t);
```

### Module

| Datei | LOC | Funktion |
|-------|-----|----------|
| `tap.c` | 103 | TAP Read/Write |
| `wav.c` | ~200 | WAV Audio I/O |
| `filter.c` | ~150 | Audio-Filter |
| `packet.c` | ~300 | Daten-Pakete |
| `mktap.c` | ~200 | TAP-Erstellung |

### Lizenz: GPL-2.0 ⚠️

---

## 3. crt_format (241 LOC) ⭐⭐⭐

### 🟢 DIREKT INTEGRIERBAR!

**Bereits UFT-konform vorbereitet:**

```c
// uft_crt.h - Cartridge Container Format
#define UFT_CRT_MAGIC "C64 CARTRIDGE   "

typedef struct uft_crt_header {
    uint8_t  magic[16];
    uint32_t header_len;
    uint16_t version;
    uint16_t hw_type;       // Hardware-Typ ID
    uint8_t  exrom, game;   // Control Lines
    char     name[32];
} uft_crt_header_t;

typedef struct uft_crt_chip_header {
    uint8_t  magic[4];      // "CHIP"
    uint32_t packet_len;
    uint16_t chip_type;     // 0=ROM, 1=RAM
    uint16_t bank;
    uint16_t load_addr;
    uint16_t rom_len;
} uft_crt_chip_header_t;

// API
uft_crt_status_t uft_crt_parse(blob, len, &view);
uft_crt_status_t uft_crt_next_chip(&view, &cursor, &chip);
```

---

## 4. Covert Bitops Loader (loader306/229)

### Turbo-Loader für 1541/1571/1581

- **Format:** 6502 Assembly (DASM)
- **Features:**
  - 2-bit Fastloading Protocol
  - SD2IEC ELoad Support
  - PAL/NTSC/Drean Kompatibilität
  - Exomizer3 Decompression

### Nicht direkt nutzbar, aber Referenz für:
- Drive-Detection-Algorithmen
- Turbo-Protocol-Timing
- Kopierschutz-Erkennung

---

## 5. DiskImageTool (VB.NET)

Windows GUI für FAT12/16 Disk-Images. Nicht für UFT relevant.

---

## 6. tsdump (86 LOC)

Einfaches D64 Track/Sector Dump. Bereits in UFT vorhanden.

---

## Empfehlungen

### P1-NEW-008: CRT Cartridge Support
```c
// Direkte Integration möglich!
// Dateien: crt_format/include/uft_crt.h
//          crt_format/src/uft_crt.c
```
**Priorität:** P1 (Hoch)  
**Aufwand:** XS (< 1 Tag)  
**Status:** Bereits UFT-konform!

### P2-NEW-009: TAP Format Support
```c
// Basierend auf c64tapedecode
int uft_tap_detect(const uint8_t* data, size_t size);
int uft_tap_open(const uint8_t* data, size_t size, uft_tap_t* tap);
int uft_tap_get_pulse(uft_tap_t* tap, uint32_t* pulse_len);
int uft_tap_write_pulse(uft_tap_t* tap, uint32_t pulse_len);
```
**Priorität:** P2 (Mittel)  
**Aufwand:** S (1-2 Tage)

### P2-NEW-010: Tape Loader Detection (Referenz)
Dokumentation der 49 bekannten Loader-Formate aus wav-prg:
- Sync-Patterns
- Pulse-Thresholds
- Checksum-Algorithmen

**Priorität:** P2 (Mittel)  
**Aufwand:** M (Nur Dokumentation)

---

## Integration in UFT

| Modul | Quelle | Integration | Status |
|-------|--------|-------------|--------|
| CRT Format | crt_format | ✅ Direkt | Ready |
| TAP Format | c64tapedecode | ⚠️ GPL | Neu schreiben |
| Loader-DB | wav-prg | ⚠️ GPL | Nur Referenz |

---

## Zusammenfassung

| Frage | Antwort |
|-------|---------|
| **Direkt integrierbar?** | ✅ CRT Format (241 LOC) |
| **Neue Formate?** | TAP (Tape), CRT (Cartridge) |
| **Referenz-Material?** | 49 Tape-Loader-Specs |
| **Lizenzen?** | GPL-2.0 (wav-prg, c64tapedecode) |

**Nächste Schritte:**
1. CRT Format integrieren (sofort möglich)
2. TAP Format neu implementieren (GPL vermeiden)
3. Loader-Datenbank dokumentieren

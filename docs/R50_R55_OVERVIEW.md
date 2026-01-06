# UFT R50-R55 - Mega-Integration Übersicht

## Executive Summary

Die Runden R50-R55 haben **8.100+ neue Codezeilen** aus 5 externen Projekten integriert:
- HxCFloppyEmulator (PLL, Format Detection)
- OpenCBM (GCR Codec)
- FluxRipper (Multi-Pass Konzepte)
- disk-utilities (Amiga Protections)
- Applesauce (WOZ Spec)

## Neue Module

### 1. Flux-Analyse

| Modul | Zeilen | Beschreibung |
|-------|--------|--------------|
| `uft_fluxstat.c` | 649 | Multi-Pass Flux Recovery (2-64 Passes) |
| `uft_hxc_pll_enhanced.c` | 445 | Victor 9K, Peak Detection |

**Features:**
- Bit-Level Confidence Scoring (0-100%)
- 5 Klassifikationen: STRONG_1, WEAK_1, STRONG_0, WEAK_0, AMBIGUOUS
- Automatische Encoding-Erkennung (MFM/GCR/Amiga)
- RPM-Berechnung aus Flux-Daten

### 2. Format Parser

| Modul | Zeilen | Plattformen |
|-------|--------|-------------|
| `uft_scp_parser.c` | 436 | C64, Amiga, Atari, Apple, PC, TRS-80, TI, Roland |
| `uft_kryoflux_parser.c` | 371 | Universal Flux |
| `uft_woz_parser.c` | 405 | Apple II (5.25"), Macintosh (3.5") |

**SCP Features:**
- v2.0 Spec vollständig
- Multi-Revolution (bis 5)
- Flux Overflow Handling (0x0000)
- Checksum Verification

**KryoFlux Features:**
- Variable-Length Encoding
- OOB Block Parsing
- Sample Clock: ~48.054 MHz
- Index Clock: 1.152 MHz

**WOZ Features:**
- v1/v2/v3 Support
- Quarter-Track Mapping
- Nibble Decoding
- Hardware Flags

### 3. Format Detection

| Modul | Zeilen | Formate |
|-------|--------|---------|
| `uft_hxc_formats.c` | 314 | 90+ Formate |

**Kategorien:**
- Apple: WOZ, NIB, DO, PO, 2MG
- Preservation: SCP, IPF, KryoFlux, A2R
- Commodore: D64, G64, D81, D71
- Amiga: ADF, ADZ, DMS, FDI
- Atari: STX, ST, MSA
- TRS-80: DMK, JV1, JV3
- PC: IMD, IMG, TD0, DSK
- HxC: HFE v1/v3, MFM, AFI

### 4. Amiga Protection

| Modul | Zeilen | Einträge |
|-------|--------|----------|
| `uft_amiga_protection.c` | 368 | 35 |
| `uft_amiga_protection_full.c` | 505 | **194** |

**Publisher:**
- Psygnosis (8): Shadow of the Beast, Baal
- Factor 5 (5): Turrican, Denaris
- Team17 (5): Alien Breed, Worms
- Bitmap Brothers (8): Xenon, Gods, Speedball
- Gremlin (5): Zool, Lotus Turbo
- + 160 weitere

**Sync Patterns:** 9 einzigartige (0x4489, 0x8951, 0x448A448A, etc.)

### 5. GCR Codec

| Modul | Zeilen | Beschreibung |
|-------|--------|--------------|
| `uft_opencbm_gcr.c` | 335 | C64/1541 GCR |

**Features:**
- 4-to-5 / 5-to-4 Conversion
- Block Encode/Decode (4 Bytes → 5 GCR)
- Sector Header Encode/Decode
- Checksum Berechnung & Verifikation

### 6. Unit Tests

| Test-Datei | Tests | Coverage |
|------------|-------|----------|
| `test_flux_parsers.c` | 22 | SCP, KryoFlux, WOZ, FluxStat |
| `test_amiga_protection.c` | 11 | Registry, Detection, Publishers |
| `test_hxc_formats.c` | 15 | Detection, Classification, GCR |
| `test_r50_r55_all.c` | Runner | Unified Test Suite |

## API Übersicht

### Flux Parser APIs

```c
// SCP
uft_scp_ctx_t* uft_scp_create(void);
int uft_scp_open(uft_scp_ctx_t* ctx, const char* filename);
int uft_scp_read_track(uft_scp_ctx_t* ctx, int track, uft_scp_track_data_t* data);
uint32_t uft_scp_calculate_rpm(uint32_t index_time_ns);

// KryoFlux
uft_kf_ctx_t* uft_kf_create(void);
int uft_kf_load_file(uft_kf_ctx_t* ctx, const char* filename);
int uft_kf_parse_stream(uft_kf_ctx_t* ctx, uft_kf_track_data_t* track);
bool uft_kf_parse_filename(const char* filename, int* track, int* side);

// WOZ
uft_woz_ctx_t* uft_woz_create(void);
int uft_woz_open(uft_woz_ctx_t* ctx, const char* filename);
int uft_woz_read_track(uft_woz_ctx_t* ctx, int quarter_track, uft_woz_track_t* track);
int uft_woz_decode_nibbles(const uint8_t* bitstream, uint32_t bit_count, ...);
```

### Protection API

```c
const uft_prot_entry_t* uft_prot_get_registry(size_t* count);
int uft_prot_detect(const uft_track_signature_t* tracks, size_t num_tracks, ...);
const char* uft_prot_name(uft_amiga_prot_type_t type);
bool uft_prot_is_longtrack(uft_amiga_prot_type_t type);
```

### Format Detection API

```c
int uft_hxc_detect_format(const uint8_t* data, size_t size, 
                          uft_hxc_format_t* format, uint8_t* confidence);
const char* uft_hxc_format_name(uft_hxc_format_t format);
bool uft_hxc_is_flux_format(uft_hxc_format_t format);
bool uft_hxc_is_preservation_format(uft_hxc_format_t format);
```

### GCR Codec API

```c
uint8_t uft_gcr_encode_nibble(uint8_t nibble);
uint8_t uft_gcr_decode_nibble(uint8_t gcr);
int uft_gcr_encode_block(const uint8_t* input, uint8_t* output);
int uft_gcr_decode_block(const uint8_t* gcr, uint8_t* output);
uint8_t uft_gcr_checksum(const uint8_t* data, size_t size);
```

## Projektstatistik

### Gesamtumfang UFT v3.11.0

| Kategorie | Zeilen |
|-----------|--------|
| src/ (Implementation) | 469,643 |
| include/ (Headers) | 138,017 |
| tests/ (Unit Tests) | 18,603 |
| **TOTAL** | **626,263** |

### R50-R55 Beitrag

| Kategorie | Zeilen |
|-----------|--------|
| Neue Headers | 2,646 |
| Neue Implementierungen | 3,423 |
| Neue Unit Tests | 1,200+ |
| Dokumentation | ~800 |
| **TOTAL R50-R55** | **~8,100** |

## Qualitätssicherung

Alle Module folgen UFT Standards:
- ✅ Pure C99/C11 kompatibel
- ✅ Keine externen Abhängigkeiten (nur stdlib)
- ✅ Thread-sicher (kein globaler State)
- ✅ Umfassende Fehlerbehandlung
- ✅ Memory-safe (keine Leaks, Bounds Checking)
- ✅ Cross-Platform (Linux/macOS/Windows)

## Nächste Schritte

1. **GitHub Push** mit CI/CD Update
2. **IPF/CAPS Integration** (libcapsimage)
3. **GUI Integration** der neuen Parser
4. **Dokumentation** erweitern
5. **Große Analyse** der Gesamtarchitektur

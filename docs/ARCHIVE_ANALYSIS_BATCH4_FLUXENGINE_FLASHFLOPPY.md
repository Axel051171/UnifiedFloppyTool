# Archive Analysis Batch 4 - FluxEngine + FlashFloppy

## Übersicht

**Analysierte Projekte:** 2
**Gesamtdateien:** 1317
**Neue Header erstellt:** 2
**Fokus:** PLL-Algorithmen, Format-Decoder, MFM/GCR-Codierung

| Projekt | Dateien | Sprache | Autor | Lizenz |
|---------|---------|---------|-------|--------|
| FluxEngine | 1158 | C++ | David Given | Public Domain |
| FlashFloppy | 159 | C | Keir Fraser | Public Domain |

---

## 1. FluxEngine

### Projektstruktur

```
fluxengine-master/
├── arch/              # Format-Decoder pro System
│   ├── amiga/         # Amiga MFM
│   ├── apple2/        # Apple II GCR
│   ├── brother/       # Brother Textverarbeitung
│   ├── c64/           # Commodore 64 GCR
│   ├── ibm/           # IBM PC MFM/FM
│   ├── macintosh/     # Mac GCR
│   ├── micropolis/    # Micropolis
│   ├── northstar/     # North Star
│   ├── victor9k/      # Victor 9000 (variable speed)
│   └── ...
├── lib/
│   ├── algorithms/    # Reader/Writer Logik
│   ├── core/          # CRC, Bytes, Utils
│   ├── data/          # Fluxmap, Sector, Track
│   ├── decoders/      # PLL, FluxDecoder
│   ├── encoders/      # Track Encoder
│   └── ...
└── protocol.h         # Hardware-Protokoll
```

### PLL-Algorithmus (Kernkomponente)

**Quelle:** `lib/decoders/fluxdecoder.cc`
**Basis:** SamDisk von Simon Owen

```cpp
// FluxEngine PLL - SamDisk derived
// https://github.com/simonowen/samdisk/

FluxDecoder::FluxDecoder(...):
    _pll_phase(config.pll_phase()),       // 0.75 default
    _pll_adjust(config.pll_adjust()),     // 0.05 default
    _flux_scale(config.flux_scale()),     // 1.0 default
    _clock(bitcell),
    _clock_centre(bitcell),
    _clock_min(bitcell * (1.0 - _pll_adjust)),
    _clock_max(bitcell * (1.0 + _pll_adjust))
{}

bool FluxDecoder::readBit() {
    // Wait for half clock
    while (_flux < (_clock / 2))
        _flux += nextFlux() * _flux_scale;
    
    _flux -= _clock;
    
    if (_flux >= (_clock / 2)) {
        // Zero bit
        _clocked_zeroes++;
        return false;
    }
    
    // One bit - PLL adjustment
    if (_clocked_zeroes <= 3) {
        // In sync: adjust clock
        _clock += _flux * _pll_adjust;
    } else {
        // Out of sync: pull to centre
        _clock += (_clock_centre - _clock) * _pll_adjust;
    }
    
    // Clamp clock range
    _clock = clamp(_clock, _clock_min, _clock_max);
    
    // Phase damping
    _flux = _flux * (1.0 - _pll_phase);
    
    return true;
}
```

**Wichtige Parameter:**

| Parameter | Default | Beschreibung |
|-----------|---------|--------------|
| pll_phase | 0.75 | Phase-Dämpfung (höher = stabiler) |
| pll_adjust | 0.05 | Frequenz-Anpassung (5%) |
| flux_scale | 1.0 | Flux-Timing-Skalierung |
| clock_min | bitcell * 0.95 | Minimale Clock |
| clock_max | bitcell * 1.05 | Maximale Clock |

### IBM MFM Sync Patterns

```cpp
// MFM record separator: 0xA1 with missing clock
// Normal:  01 00 01 00 10 10 10 01 = 0x44A9
// Special: 01 00 01 00 10 00 10 01 = 0x4489
//                       ^^^^^
// Missing clock makes it unambiguous sync marker

#define MFM_PATTERN 0x448944894489LL  // 3x A1

// FM patterns (with clock byte)
FM_IDAM_PATTERN = 0xF57E  // 0xFE data, 0xC7 clock
FM_DAM1_PATTERN = 0xF56A  // 0xF8 data, 0xC7 clock
FM_DAM2_PATTERN = 0xF56F  // 0xFB data, 0xC7 clock
FM_IAM_PATTERN  = 0xF77A  // 0xFC data, 0xD7 clock
```

### C64 GCR Decoder

```cpp
// C64 uses 5-bit GCR for 4-bit data
#define C64_SECTOR_RECORD 0xFFD49  // Sync + 0x08
#define C64_DATA_RECORD   0xFFD57  // Sync + 0x07

// GCR decode table
GCR_ENTRY(0x0A, 0x0);
GCR_ENTRY(0x0B, 0x1);
GCR_ENTRY(0x12, 0x2);
// ... 16 entries total
```

### Apple II "Crazy Data" Decoder

```cpp
// Apple II 6&2 encoding
// 342 encoded bytes -> 256 data bytes
// Complex checksum with XOR chain

static Bytes decode_crazy_data(const uint8_t* inp, Status& status) {
    uint8_t checksum = 0;
    for (unsigned i = 0; i < 342; i++) {
        checksum ^= decode_data_gcr(*inp++);
        
        if (i >= 86) {
            // 6-bit data
            output[i - 86] |= (checksum << 2);
        } else {
            // 3 * 2-bit auxiliary data
            output[i + 0] = ((checksum >> 1) & 0x01) | ((checksum << 1) & 0x02);
            output[i + 86] = ((checksum >> 3) & 0x01) | ((checksum >> 1) & 0x02);
            if ((i + 172) < 256)
                output[i + 172] = ((checksum >> 5) & 0x01) | ((checksum >> 3) & 0x02);
        }
    }
    // Verify final checksum
}
```

### Amiga MFM Interleaving

```cpp
// Amiga stores odd bits, then even bits
// Uses 64-bit multiply trick for fast interleaving

uint32_t amigaChecksum(const Bytes& bytes) {
    uint32_t checksum = 0;
    for each 32-bit word:
        checksum ^= word;
    return checksum & 0x55555555;  // Mask out clock bits
}

// Bit interleave using 64-bit multiply
uint16_t result =
    (((e * 0x0101010101010101ULL & 0x8040201008040201ULL) *
            0x0102040810204081ULL >> 49) & 0x5555) |
    (((o * 0x0101010101010101ULL & 0x8040201008040201ULL) *
            0x0102040810204081ULL >> 48) & 0xAAAA);
```

### Hardware Protocol

```c
// FluxEngine Hardware (12 MHz)
TICK_FREQUENCY = 12000000
TICKS_PER_US = 12
NS_PER_TICK = 83.33...

// Precompensation threshold
PRECOMPENSATION_THRESHOLD_TICKS = 2.25 * TICKS_PER_US = 27 ticks

// Flux encoding
F_BIT_PULSE = 0x80  // Flux pulse
F_BIT_INDEX = 0x40  // Index mark
```

---

## 2. FlashFloppy

### Projektstruktur

```
flashfloppy-master/
├── src/
│   ├── image/          # Image-Format-Handler
│   │   ├── adf.c       # Amiga ADF
│   │   ├── hfe.c       # HxC HFE
│   │   ├── img.c       # IBM Raw Images
│   │   └── mfm.c       # MFM-Codierung
│   ├── floppy.c        # Floppy-Emulation
│   ├── floppy_generic.c # Low-Level I/O
│   └── crc.c           # CRC16-CCITT
└── inc/
    └── types.h         # Basisdefinitionen
```

### MFM-Tabelle (256 Einträge)

```c
// Complete MFM encoding table
const uint16_t mfmtab[256] = {
    0xAAAA, 0xAAA9, 0xAAA4, 0xAAA5, 0xAA92, 0xAA91, 0xAA94, 0xAA95,
    // ... 256 entries
    0x5552, 0x5551, 0x5554, 0x5555
};

// FM sync with custom clock
uint16_t fm_sync(uint8_t dat, uint8_t clk) {
    uint16_t _dat = mfmtab[dat] & 0x5555;
    uint16_t _clk = (mfmtab[clk] & 0x5555) << 1;
    return _clk | _dat;
}
```

### CRC16-CCITT (Tabellenbasiert)

```c
static const uint16_t crc16tab[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    // ... 256 entries
};

uint16_t crc16_ccitt(const void *buf, size_t len, uint16_t crc) {
    const uint8_t *b = buf;
    for (size_t i = 0; i < len; i++)
        crc = crc16tab[(crc >> 8) ^ *b++] ^ (crc << 8);
    return crc;
}
```

### HFE Format Support

```c
// HFE Header Structure
struct disk_header {
    char sig[8];           // "HXCPICFE" or "HXCHFEV3"
    uint8_t formatrevision;
    uint8_t nr_tracks, nr_sides;
    uint8_t track_encoding;
    uint16_t bitrate;      // kB/s
    uint16_t rpm;
    uint8_t interface_mode;
    // ...
};

// Track encoding types
enum {
    ENC_ISOIBM_MFM = 0,
    ENC_Amiga_MFM = 1,
    ENC_ISOIBM_FM = 2,
    ENC_Emu_FM = 3
};

// Interface modes
enum {
    IFM_IBMPC_DD = 0,
    IFM_IBMPC_HD = 1,
    IFM_AtariST_DD = 2,
    IFM_Amiga_DD = 4,
    // ... 14 modes total
};

// HFEv3 opcodes (bit-reversed)
OP_Nop      = 0x0F
OP_Index    = 0x8F
OP_Bitrate  = 0x4F
OP_SkipBits = 0xCF
OP_Rand     = 0x2F
```

### Format-Definitionen (img.c)

```c
// Standard IBM PC formats
const struct raw_type img_type[] = {
    {  8, 1, IAM, 84, 1, 2, 1, 0, 0, 0, C40, R300 }, // 160K
    {  9, 1, IAM, 84, 1, 2, 1, 0, 0, 0, C40, R300 }, // 180K
    {  9, 2, IAM, 84, 1, 2, 1, 0, 0, 0, C40, R300 }, // 360K
    { 15, 2, IAM, 84, 1, 2, 1, 0, 0, 0, C80, R360 }, // 1.2M
    {  9, 2, IAM, 84, 1, 2, 1, 0, 0, 0, C80, R300 }, // 720K
    { 18, 2, IAM,108, 1, 2, 1, 0, 0, 0, C80, R300 }, // 1.44M
    { 36, 2, IAM, 84, 1, 2, 1, 0, 0, 0, C80, R300 }, // 2.88M
    // ...
};

// ADFS (Acorn) formats
const struct raw_type adfs_type[] = {
    {  5, 2, IAM, 116, 1, 3, 0, 0, 1, 0, C80, R300 }, // ADFS D/E 800K
    { 10, 2, IAM, 116, 1, 3, 0, 0, 2, 0, C80, R300 }, // ADFS F 1.6M
    // ...
};

// Akai sampler formats
const struct raw_type akai_type[] = {
    {  5, 2, IAM, 116, 1, 3, 1, 0, 2, 0, C80, R300 }, // Akai DD
    { 10, 2, IAM, 116, 1, 3, 1, 0, 5, 0, C80, R300 }, // Akai HD
};
```

### Flux zu Bitcell Konvertierung

```c
uint16_t bc_rdata_flux(struct image *im, uint16_t *tbuf, uint16_t nr) {
    uint32_t ticks_per_cell = im->ticks_per_cell;
    uint32_t ticks = im->ticks_since_flux;
    
    // Convert bitcells to flux timings
    while (bc_c != bc_p) {
        x = be32toh(bc_b[(bc_c / 32) & bc_mask]) << y;
        bc_c += 32 - y;
        
        while (y < 32) {
            y++;
            ticks += ticks_per_cell;
            if ((int32_t)x < 0) {  // 1 bit
                *tbuf++ = (ticks >> 4) - 1;
                ticks &= 15;
                if (!--todo) goto out;
            }
            x <<= 1;
        }
    }
}
```

---

## Erstellte Header

### 1. uft_fluxengine_algorithms.h (650+ Zeilen)

**Inhalt:**
- FluxEngine Hardware-Konstanten (12 MHz Timing)
- PLL-Algorithmus (SamDisk-Port)
- MFM-Encoding-Tabelle (256 Einträge)
- FM Sync-Erzeugung
- IBM MFM Sync-Patterns
- C64 GCR Encoding/Decoding
- Apple II 6&2 GCR Tables
- Amiga MFM Interleaving
- CRC16-CCITT (Tabellenbasiert)
- HFE Format-Konstanten

### 2. uft_flashfloppy_formats.h (450+ Zeilen)

**Inhalt:**
- Format-Definition-Struktur
- IBM PC Formate (18 Presets)
- ADFS Formate (5 Presets)
- Akai Sampler Formate
- Commodore D81 Formate
- DEC RX Formate
- Ensoniq Formate
- MSX Formate
- PC-98 Formate
- Casio Keyboard Formate
- Gap-Defaults (FM/MFM)
- Data Rate Konstanten
- Track-Längen-Berechnung

---

## Wichtige Erkenntnisse

### PLL-Vergleich: FluxEngine vs. Greaseweazle

| Aspekt | FluxEngine | Greaseweazle |
|--------|------------|--------------|
| Phase Default | 75% | 60% |
| Period Default | 5% | 5% |
| Sync Detection | 3 Nullen | 3 Nullen |
| Re-sync Methode | Pull to centre | Pull to centre |
| Clock Clamping | ±5% | ±adjust |

### Format-Coverage

| System | FluxEngine | FlashFloppy | UFT |
|--------|------------|-------------|-----|
| IBM PC | ✓ | ✓ | ✓ |
| Amiga | ✓ | ✓ | ✓ |
| Apple II | ✓ | - | ✓ |
| Macintosh | ✓ | - | ✓ |
| C64 | ✓ | - | ✓ |
| Atari ST | ✓ | ✓ | ✓ |
| ADFS | - | ✓ | ✓ |
| Akai | - | ✓ | ✓ |
| PC-98 | - | ✓ | ✓ |
| Victor 9K | ✓ | - | ✓ |
| Brother | ✓ | - | ✓ |
| Micropolis | ✓ | - | ✓ |

### Algorithmische Highlights

1. **64-bit Multiply Bit Interleave** (Amiga)
   - Extrem schnelle Bit-Verschränkung
   - Verwendet Magic Numbers für parallele Extraktion

2. **Apple II "Crazy Data"**
   - Komplexes 6&2 Encoding
   - 342 Bytes → 256 Bytes
   - Verschachtelte Checksumme

3. **FM Sync mit separatem Clock-Pattern**
   - Erlaubt illegale Clock-Kombinationen
   - Eindeutige Sync-Marker

4. **HFEv3 Opcodes**
   - In-Stream Bitrate-Änderung
   - Variable-Speed-Unterstützung
   - Random-Data für Kopierschutz

---

## Integration Empfehlungen

### Priorität 1: PLL Konsolidierung

Zusammenführen der PLL-Implementierungen:
- UFT V1 (eigene Implementation)
- Greaseweazle PLL
- FluxEngine/SamDisk PLL

**Empfohlene Parameter:**
```c
// "Universal" PLL defaults
pll_phase = 0.70        // Kompromiss zwischen 60% (GW) und 75% (FE)
pll_adjust = 0.05       // Übereinstimmend
sync_threshold = 3      // Übereinstimmend
clock_tolerance = 0.05  // ±5%
```

### Priorität 2: Format-Registry

Einheitliche Format-Datenbank:
```c
struct uft_format_entry {
    uint32_t id;
    const char *name;
    uft_ff_format_t params;
    uft_pll_preset_t pll;
    uft_gap_params_t gaps;
};

// Auto-detection by size + magic bytes
const uft_format_entry* uft_detect_format(
    const uint8_t *data, size_t size);
```

### Priorität 3: GCR-Codecs vereinheitlichen

Gemeinsame GCR-Infrastruktur:
```c
typedef struct {
    uint8_t bits_in;    // 4, 5, or 6
    uint8_t bits_out;   // 5, 6, or 8
    const uint8_t *encode_table;
    const int8_t *decode_table;
} uft_gcr_codec_t;

extern const uft_gcr_codec_t UFT_GCR_C64;      // 4→5 bits
extern const uft_gcr_codec_t UFT_GCR_APPLE2;   // 6→8 bits
extern const uft_gcr_codec_t UFT_GCR_MAC;      // 6→8 bits
```

---

## Statistik

**Batch 4 Analyse:**
- 2 Projekte analysiert
- 1317 Dateien durchsucht
- 2 neue Header erstellt (1100+ Zeilen)
- 40+ Format-Presets dokumentiert
- 3 PLL-Varianten verglichen

**Projekt-Gesamt (Batch 1-4):**
- 16 Archive analysiert
- 2648+ Dateien untersucht
- 68+ Header erstellt
- 27,000+ Zeilen Code

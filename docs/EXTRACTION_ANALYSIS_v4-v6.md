# UFT Extraction Analysis v4-v6

> **Datum:** 2026-01-04  
> **Pakete:** uft_cbm_code_extraction_v4.zip, uft_floppy_extraction_v5.zip, uft_floppy_extraction_v6.zip  
> **Gesamt analysiert:** ~78,000 LOC  
> **Integriert:** 4 Header + 17 Referenz-Dateien

---

## 📊 Capability Matrix

| Version | Fokus | LOC | Status |
|---------|-------|-----|--------|
| **v4** | CBM/HxC Code | ~35,000 | ✅ 4 Header, 6 Referenzen |
| **v5** | CP/M, Amstrad | ~28,000 | ✅ 1 Header, 4 Referenzen |
| **v6** | Disk-Utilities | ~15,000 | ✅ 7 Referenzen |

---

## 🆕 Neue Header-Module

### uft_victor9k_gcr.h (246 LOC)
**Victor 9000/Sirius 1 Variable-Density GCR**

Der Victor 9000 verwendet ein einzigartiges GCR-Schema mit variabler Dichte:
- **8 Zonen** mit unterschiedlicher Sektoranzahl (12-19 Sektoren/Track)
- **Äußere Tracks schneller** (Zone 0: 394 kbps, 19 Sektoren)
- **Innere Tracks langsamer** (Zone 7: 247 kbps, 12 Sektoren)
- **Kapazität:** 606KB (single-sided), 1.2MB (double-sided)

```c
static const uft_v9k_zone_t uft_v9k_zones[8] = {
    {  0,  4, 19, 394000, 2.538 },  // Zone 0: 19 sectors
    { 72, 80, 12, 247000, 4.049 }   // Zone 7: 12 sectors
};
```

### uft_amstrad_cpc.h (272 LOC)
**Amstrad CPC DSK/EDSK Format**

- DSK/EDSK Format-Erkennung und -Parsing
- AMSDOS Header mit 16-Bit Checksum
- CP/M Directory Entry Struktur
- Data/System/Vendor Format-Varianten

```c
typedef struct {
    uint8_t  user;
    char     filename[8];
    char     extension[3];
    uint16_t load_address;
    uint16_t exec_address;
    uint16_t checksum;
} uft_amsdos_header_t;
```

### uft_track_generator.h (298 LOC)
**Track-Generierung für FM/MFM/GCR**

- Low-Level Bit-Schreiben
- FM/MFM/GCR Encoding
- Gap, Sync, Address Mark Generation
- CRC Berechnung und Schreiben
- Format-Presets für IBM DD/HD und Amiga

### uft_sector_extractor.h (275 LOC)
**Sektor-Extraktion aus Raw-Tracks**

- Auto-Detect für FM/MFM/GCR/Amiga
- A1 Sync Pattern Search
- IDAM/DAM Parsing mit CRC-Validierung
- Weak Bit Detection
- Status-Flags für jeden Sektor

---

## 📁 Referenz-Quellen (für spätere Implementation)

### v4_hxc/ - HxCFloppyEmulator Sources

| Datei | LOC | Beschreibung |
|-------|-----|--------------|
| c64_gcr_track.c | 639 | C64 GCR Encode/Decode |
| victor9k_gcr_track.c | 645 | Victor 9000 Variable-GCR |
| apple_mac_gcr_track.c | 616 | Apple Mac GCR |
| amiga.c | 1776 | Amiga Format Handler |
| fs_amigados.c | 846 | AmigaDOS Filesystem |
| atari_st.c | 1142 | Atari ST Format |

**Schlüssel-Algorithmen:**
- GCR 4-to-5 Encode/Decode Tables
- Zone-basierte Timing-Berechnung
- Sector Interleave Handling

### v5_cpm/ - CP/M Tools

| Datei | LOC | Beschreibung |
|-------|-----|--------------|
| cpmfs.c | 1973 | Komplettes CP/M Filesystem |
| cpmfs.h | 127 | CP/M Header |
| cpcemu.c | 91 | CPC Emulator Helpers |
| amsdos.c | 74 | AMSDOS Routinen |

### v6_diskutil/ - Keir Fraser's disk-utilities

| Datei | LOC | Beschreibung |
|-------|-----|--------------|
| copylock.c | 353 | Rob Northen CopyLock LFSR |
| speedlock.c | 91 | Speedlock Protection |
| longtrack.c | 535 | Longtrack Protections |
| ibm.c | 1207 | IBM PC Format Handler |
| mfm_codec.cpp | 525 | MFM Codec Implementation |
| kryoflux_stream.c | 259 | KryoFlux Stream Reader |

**CopyLock LFSR:**
```c
// 23-bit LFSR with taps at positions 1 and 23
static uint32_t lfsr_next_state(uint32_t x) {
    return ((x << 1) & ((1u << 23) - 1)) | (((x >> 22) ^ x) & 1);
}
```

---

## 🔑 Erkenntnisse

### Victor 9000 - Einzigartiges Format
- **Variable Dichte** ist komplex zu implementieren
- Erfordert **zonenbewusstes PLL-Tracking**
- Referenz-Code in HxC ist gut strukturiert

### Amstrad CPC - CP/M Basis
- DSK/EDSK sind gut dokumentierte Formate
- AMSDOS Header wichtig für BASIC-Programme
- Sektor-IDs starten bei 0xC1 (nicht 0x01!)

### Track Generator/Extractor
- **Bidirektional:** Generator und Extractor als Paar
- Ermöglicht Roundtrip-Tests
- Basis für Format-Konvertierung

### Referenz-Code Qualität
- **HxC Code:** Gut strukturiert, GPL-lizenziert
- **disk-utilities:** Kompakt, Public Domain
- **cpmtools:** Produktionsreif, lange Historie

---

## 📋 Offene Tasks (EXT3)

### Sofort (MUST) ✅
- [x] Victor 9k GCR Header
- [x] Amstrad CPC Header
- [x] Track Generator Header
- [x] Sector Extractor Header

### Diese Woche (SHOULD)
- [ ] EXT3-005: C64 GCR Implementation
- [ ] EXT3-006: Apple Mac GCR Implementation
- [ ] EXT3-010: CP/M Filesystem Integration
- [ ] EXT3-014: IBM Format Handler

### Later (COULD)
- [ ] EXT3-016: CRC32 Lookup Tables (9446 LOC!)
- [ ] EXT3-020: FDC Bitstream Analyzer

---

## ✅ Integration Summary

- **4 neue Header-Module** (1,091 LOC)
- **17 Referenz-Dateien** (~10,000 LOC)
- **3 Pakete** analysiert
- **~78,000 LOC** reviewed
- Projekt gewachsen auf **196,100 LOC**

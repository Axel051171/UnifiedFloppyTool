# FC5025 Driver Analysis für UnifiedFloppyTool

**Analysiert:** 2024-12-30
**Quelle:** FC5025_Driver_Source_Code_v1309_tar.gz
**Hardware:** Device Inside Box (DIB) FC5025 USB Floppy Controller

---

## Executive Summary

Der FC5025 ist ein Hardware-USB-Floppy-Controller mit professionellem Treiber-Code.
Die wichtigsten Erkenntnisse:

1. **Vollständige Encoder/Decoder** für MFM, FM, GCR (Apple, CBM)
2. **Hardware-Abstraktion** über `struct phys` - ideal für UFT
3. **CRC-16 CCITT** mit vorberechneter Tabelle
4. **Interleave-Tabellen** für DOS 3.3, ProDOS, etc.
5. **Bitcell-Timing** pro Format/Zone

---

## 1. Encoding/Decoding Algorithmen

### 1.1 MFM Encoder/Decoder (endec_mfm.c)

**FC5025 Kompressionsschema:**
- Run-Length Encoding für Nullen
- Bit-weise Verarbeitung

```c
/* MFM Decoder - aus FC5025 */
static unsigned char get_decoded_byte(void) {
    int i;
    unsigned char byte = 0;
    
    for (i = 0; i < 8; i++) {
        get_disk_bit();  /* Clock-Bit ignorieren */
        byte <<= 1;
        byte |= get_disk_bit();  /* Daten-Bit */
    }
    return byte;
}

/* MFM Encoder */
static void encode_bit(char bit) {
    char clock, data;
    
    data = bit;
    if (bit == 1) {
        clock = 0;
    } else {
        clock = lastbit ^ 1;  /* Clock nur wenn beide Bits 0 */
    }
    lastbit = bit;
    emit_disk_bit(clock);
    emit_disk_bit(data);
}
```

### 1.2 FM Encoder/Decoder (endec_fm.c)

**Einfachste Kodierung - jedes Bit hat einen Clock:**

```c
/* FM Encoder - aus FC5025 */
static unsigned char enc_fm_nibble(unsigned char in) {
    int i;
    unsigned char out = 0;
    
    for (i = 0; i < 4; i++) {
        out <<= 2;
        out |= 2;           /* Clock-Bit immer gesetzt */
        if (in & 0x80)
            out |= 1;       /* Daten-Bit */
        in <<= 1;
    }
    return out;
}

/* FM Decoder */
int dec_fm(unsigned char *raw, unsigned char *fm, int count) {
    int status = 0;
    
    while (count--) {
        /* Prüfe Clock-Bits (müssen 0xAA sein) */
        if (((*fm) & 0xAA) != 0xAA)
            status = 1;
        
        /* Extrahiere Daten-Bits */
        *raw = 0;
        if ((*fm) & 0x40) *raw |= 0x80;
        if ((*fm) & 0x10) *raw |= 0x40;
        if ((*fm) & 0x04) *raw |= 0x20;
        if ((*fm) & 0x01) *raw |= 0x10;
        fm++;
        if ((*fm) & 0x40) *raw |= 0x08;
        if ((*fm) & 0x10) *raw |= 0x04;
        if ((*fm) & 0x04) *raw |= 0x02;
        if ((*fm) & 0x01) *raw |= 0x01;
        fm++;
        raw++;
    }
    return status;
}
```

### 1.3 Apple 6-and-2 GCR (endec_6and2.c)

**Apple DOS 3.3/ProDOS 16-Sektor Format:**

```c
/* 6-and-2 Decode Table (FC5025) */
static unsigned char dec_6and2_tbl[] = {
    /* 0x90-0x9F */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x04,
    0xFF,0xFF,0x08,0x0C,0xFF,0x10,0x14,0x18,
    /* 0xA0-0xAF */
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x1C,0x20,
    0xFF,0xFF,0xFF,0x24,0x28,0x2C,0x30,0x34,
    /* ... weitere Einträge ... */
};

/* 6-and-2 Sektor-Dekodierung (256 Bytes) */
int dec_6and2(unsigned char *out, unsigned char *in, int count) {
    int status = 0;
    unsigned char acc = 0;
    unsigned char aux[86];
    
    /* Erst 86 Auxiliary-Bytes lesen */
    for (int i = 0; i < 86; i++) {
        aux[i] = dec_6and2_tbl[*in];
        if (aux[i] == 0xFF) status = 1;
        in++;
    }
    
    /* Dann 256 Daten-Bytes */
    for (int i = 0; i < 256; i++) {
        out[i] = dec_6and2_tbl[*in];
        if (out[i] == 0xFF) status = 1;
        in++;
    }
    
    /* Checksum prüfen */
    unsigned char csum = dec_6and2_tbl[*in];
    if (in[1] != 0xDE || in[2] != 0xAA)  /* Epilog */
        status = 1;
    
    /* XOR-Chain dekodieren */
    for (int i = 0; i < 86; i++) {
        aux[i] ^= acc;
        acc = aux[i];
    }
    for (int i = 0; i < 256; i++) {
        out[i] ^= acc;
        acc = out[i];
    }
    
    if (out[255] != csum)
        status = 1;
    
    /* 2-bit Teile aus aux[] in out[] einfügen */
    for (int i = 0; i < 86; i++) {
        unsigned char c = aux[i] >> 2;
        unsigned char d;
        d = 0; if (c & 1) d |= 2; if (c & 2) d |= 1;
        out[i] |= d;
        /* ... weitere Bit-Manipulation ... */
    }
    
    return status;
}
```

### 1.4 Commodore 5/4 GCR (endec_54gcr.c)

**C64/1541 Format (4 Bytes → 5 Bytes):**

```c
/* CBM GCR Encode Table */
static unsigned char enc_tbl[] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* CBM GCR Decode Table */
static unsigned char dec_tbl[] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 0-7 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 8-F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

/* 4 Bytes → 5 GCR Bytes */
static void enc_54gcr_chunk(unsigned char out[5], unsigned char in[4]) {
    out[0] = enc_tbl[in[0] >> 4] << 3;
    out[0] |= enc_tbl[in[0] & 0xF] >> 2;
    out[1] = enc_tbl[in[0] & 0xF] << 6;
    out[1] |= enc_tbl[in[1] >> 4] << 1;
    out[1] |= enc_tbl[in[1] & 0xF] >> 4;
    out[2] = enc_tbl[in[1] & 0xF] << 4;
    out[2] |= enc_tbl[in[2] >> 4] >> 1;
    out[3] = enc_tbl[in[2] >> 4] << 7;
    out[3] |= enc_tbl[in[2] & 0xF] << 2;
    out[3] |= enc_tbl[in[3] >> 4] >> 3;
    out[4] = enc_tbl[in[3] >> 4] << 5;
    out[4] |= enc_tbl[in[3] & 0xF];
}

/* 5 GCR Bytes → 4 Bytes */
static int dec_54gcr_chunk(unsigned char out[4], unsigned char in[5]) {
    int baddog = 0;
    out[0] = decode_nib(in[0] >> 3) << 4;
    out[0] |= decode_nib((in[0] << 2) | (in[1] >> 6));
    out[1] = decode_nib(in[1] >> 1) << 4;
    out[1] |= decode_nib((in[1] << 4) | (in[2] >> 4));
    out[2] = decode_nib((in[2] << 1) | (in[3] >> 7)) << 4;
    out[2] |= decode_nib(in[3] >> 2);
    out[3] = decode_nib((in[3] << 3) | (in[4] >> 5)) << 4;
    out[3] |= decode_nib(in[4]);
    return baddog;
}
```

### 1.5 Apple 4-and-4 (Address Field Encoding)

```c
/* 4-and-4 für Apple Address Fields */
static void enc_4and4_byte(unsigned char *out, unsigned char *in_ptr) {
    unsigned char in = *in_ptr;
    out[0] = 0; out[1] = 0;
    
    for (int i = 0; i < 4; i++) {
        out[0] <<= 2; out[1] <<= 2;
        out[0] |= 2; out[1] |= 2;      /* Clock-Bits */
        if (in & 0x80) out[0] |= 1;    /* High-Bits */
        if (in & 0x40) out[1] |= 1;    /* Low-Bits */
        in <<= 2;
    }
}

static void dec_4and4_byte(unsigned char *out, unsigned char *in) {
    *out = ((in[0] << 1) | 1) & in[1];
}
```

---

## 2. CRC-16 CCITT Implementation

```c
/* CRC-16 CCITT (Poly=0x1021, IV=0xFFFF) */
/* Test: 0xFE 0x00 0x00 0x07 0x01 → 0x6844 */

static unsigned short crctable[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    /* ... vollständige Tabelle ... */
};

void crc_init(void) {
    crc = 0xFFFF;
}

void crc_add(unsigned char c) {
    crc = (crc << 8) ^ crctable[((crc >> 8) ^ c) & 0xFF];
}

unsigned short crc_block(unsigned char *buf, int count) {
    crc_init();
    while (count--)
        crc_add(*buf++);
    return crc;
}

void crc_append(unsigned char *buf, int count) {
    crc_init();
    while (count--)
        crc_add(*buf++);
    *buf++ = crc >> 8;
    *buf = crc & 0xFF;
}
```

---

## 3. Physikalisches Format-Interface

```c
/* Generisches Format-Interface */
struct phys {
    int (*min_track)(struct phys *);
    int (*max_track)(struct phys *);
    int (*num_tracks)(struct phys *);
    int (*min_side)(struct phys *);
    int (*max_side)(struct phys *);
    int (*num_sides)(struct phys *);
    int (*min_sector)(struct phys *, int track, int side);
    int (*max_sector)(struct phys *, int track, int side);
    int (*num_sectors)(struct phys *, int track, int side);
    int (*tpi)(struct phys *);          /* 48 oder 96 TPI */
    int (*density)(struct phys *);      /* Low/High Density */
    int (*sector_bytes)(struct phys *, int track, int side, int sector);
    int (*track_bytes)(struct phys *, int track, int side);
    int (*physical_track)(struct phys *, int track);
    void (*best_read_order)(struct phys *, struct sector_list *, int, int);
    int (*read_sector)(struct phys *, unsigned char *, int, int, int);
    int (*prepare)(struct phys *);
};
```

---

## 4. Format-Konstanten

### 4.1 C64/1541 Bitcell-Timing

```c
/* Bitcell-Zeit variiert nach Zone (Track) */
static int cbm_bitcell(int track) {
    if (track < 18)  return 2708;  /* Zone 0: Tracks 1-17 */
    if (track < 25)  return 2917;  /* Zone 1: Tracks 18-24 */
    if (track < 31)  return 3125;  /* Zone 2: Tracks 25-30 */
    return 3333;                    /* Zone 3: Tracks 31-35 */
}

/* Sektoren pro Track (C64) */
static int c64_sectors[] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24 */
    18, 18, 18, 18, 18, 18,                                              /* 25-30 */
    17, 17, 17, 17, 17                                                   /* 31-35 */
};
```

### 4.2 DOS 3.3 Interleave

```c
/* DOS 3.3 Sektor-Interleave */
static int dos33_to_phys[] = {
    0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
};
```

### 4.3 ProDOS Interleave

```c
/* ProDOS Sektor-Mapping */
static int prodos_to_phys[] = {
    0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
};
```

### 4.4 MFM Bitcell-Zeiten

```c
/* MS-DOS Formate */
#define BITCELL_1200K  2000  /* 1.2MB HD */
#define BITCELL_360K   3333  /* 360K DD */
```

---

## 5. Unterstützte Formate

| Format | Extension | Encoder | Sektoren | Bytes/Sektor |
|--------|-----------|---------|----------|--------------|
| Apple DOS 3.3 | .dsk | 6-and-2 | 16/Track | 256 |
| Apple DOS 3.2 | .d13 | 5-and-3 | 13/Track | 256 |
| Apple ProDOS | .po | 6-and-2 | 16/Track | 256 |
| Commodore 1541 | .d64 | 5/4 GCR | 17-21/Track | 256 |
| TI-99/4A | .v9t9 | FM | 9/Track | 256 |
| Atari 810 | .xfd | FM | 18/Track | 128 |
| MS-DOS 1200k | .img | MFM | 15/Track | 512 |
| MS-DOS 360k | .img | MFM | 9/Track | 512 |
| Kaypro 2 | .dsk | MFM | 10/Track | 512 |
| Kaypro 4 | .dsk | MFM | 10/Track | 512 |
| TRS-80 CoCo | .dsk | MFM | 18/Track | 256 |

---

## 6. Integration in UFT

### 6.1 Empfohlene Header-Struktur

```c
/* uft_fc5025_compat.h */

typedef struct {
    int min_track;
    int max_track;
    int (*sectors_per_track)(int track);
    int sector_size;
    int (*bitcell_time)(int track);
    const char *name;
    const char *extension;
} disk_format_t;

/* Format-Registry */
extern const disk_format_t format_apple33;
extern const disk_format_t format_apple32;
extern const disk_format_t format_c1541;
extern const disk_format_t format_msdos360;
extern const disk_format_t format_msdos1200;
```

### 6.2 CRC-Modul

Die FC5025 CRC-Implementierung ist direkt verwendbar und getestet:
- Poly: 0x1021 (CRC-16-CCITT)
- IV: 0xFFFF
- Testvektor verifiziert

### 6.3 Encoder/Decoder

Alle Encoder/Decoder sind rein in C, keine Hardware-Abhängigkeiten
außer dem USB-Interface. Perfekt für UFT-Integration.

---

## 7. UFT CI/CD Integration

Zusätzlich wurde das CI/CD-Scaffold analysiert:

### GitHub Actions Workflows

1. **ci.yml** - Continuous Integration
   - Linux (Ubuntu 24.04) + Windows (2022)
   - Qt 6.8.3 mit Caching
   - CMake + Ninja Build
   - CTest für Unit-Tests
   - Artifact-Upload

2. **release.yml** - Release-Builds
   - Trigger auf Tags (v*)
   - Automatische GitHub Releases
   - Portable Bundle-Erstellung

### Projekt-Dateien

- CONTRIBUTING.md - Coding-Guidelines
- SECURITY.md - Vulnerability-Reporting
- .gitignore - Standard-Patterns
- CODEOWNERS - Code-Review-Zuweisungen
- Issue/PR Templates

---

## Zusammenfassung

Der FC5025-Code bietet:

1. ✅ **Vollständige Encoder/Decoder** für alle wichtigen Formate
2. ✅ **CRC-16 CCITT** mit Lookup-Table
3. ✅ **Interleave-Tabellen** für Apple/DOS
4. ✅ **Bitcell-Timing** für C64 Zonen
5. ✅ **Sauberes Interface** über struct phys
6. ✅ **Portable C-Implementation**

Besonders wertvoll für UFT:
- Apple 5-and-3 / 6-and-2 Algorithmen
- CBM 5/4 GCR Chunk-Encoding
- Sektor-Interleave-Tabellen
- CRC-Tabelle

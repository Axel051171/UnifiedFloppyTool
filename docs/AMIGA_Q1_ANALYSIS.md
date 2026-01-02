# Amiga-Stuff & Q1Decode Analysis für UFT

**Analysiert:** 2024-12-30
**Quellen:** amiga-stuff-master.zip, q1decode-main.zip
**Autor:** Keir Fraser (Amiga), unknown (Q1)

---

## Executive Summary

### Amiga-Stuff (Keir Fraser)
Professionelles Amiga-Testkit mit exzellenten MFM-Routinen:
- **68k Assembly MFM Encoder/Decoder** - Hochoptimiert
- **Hardware-Register-Definitionen** - Vollständig
- **CRC16-CCITT in Assembly** - Table-driven, schnell
- **Floppy-Controller-Code** - Direkte Hardware-Ansteuerung

### Q1Decode
Decoder für exotisches **Minicomputer-Diskformat (Q1)** mit EBCDIC:
- **Unbekanntes 4-Level Encoding** - Nicht MFM/FM!
- **EBCDIC ↔ ASCII Konversion** - Vollständige Tabellen
- **Catweasel-Sample-Verarbeitung** - Flux-basiert
- **Historische Relevanz** für Mainframe-Preservation

---

## 1. Amiga MFM Implementation (mfm.S)

### 1.1 Hochoptimierter MFM Decoder (68k Assembly)

**Kerntechnik: Odd/Even Bit-Interleaving**

```asm
/* Amiga MFM speichert Bits in zwei getrennten Longwords:
   - Odd bits  in erstem Longword
   - Even bits in zweitem Longword
   Dekodierung: d0 = (odd << 1) | even */

decode_mfm_long:
    and.l   d5,d0           /* d5 = 0x55555555 (Odd-Bit-Maske) */
    and.l   d5,d1           /* Maskiere beide */
    add.l   d0,d0           /* Odd bits << 1 */
    or.l    d1,d0           /* Kombiniere mit Even */
    rts
```

**C-Äquivalent:**
```c
uint32_t amiga_mfm_decode_long(uint32_t odd, uint32_t even) {
    const uint32_t mask = 0x55555555;
    return ((odd & mask) << 1) | (even & mask);
}
```

### 1.2 MFM Encoder mit Clock-Bit-Insertion

```asm
encode_mfm_long:
    move.l  d0,d1
    lsr.l   #1,d0           /* Odd bits = data >> 1 */
    and.l   d5,d0           /* Maskiere */
    and.l   d5,d1           /* Even bits = data & mask */
    rts

/* Clock-Bit-Insertion: */
    move.l  (a1),d0         /* Lade Daten */
    move.l  d0,d1
    roxr.l  #1,d0           /* d0 = (X . data) >> 1 */
    rol.l   #1,d1           /* d1 = data << 1 */
    or.l    d0,d1
    not.l   d1              /* clock[n] = data[n-1] NOR data[n] */
    and.l   d6,d1           /* d6 = 0xAAAAAAAA (Clock-Maske) */
    or.l    d1,(a1)+        /* Füge Clock-Bits ein */
```

**C-Äquivalent:**
```c
void amiga_mfm_insert_clocks(uint32_t *data, size_t count) {
    const uint32_t clock_mask = 0xAAAAAAAA;
    uint32_t prev_bit = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t d = data[i];
        uint32_t d_shifted = d >> 1;
        uint32_t d_left = (d << 1) | prev_bit;
        uint32_t clocks = ~(d_shifted | d_left) & clock_mask;
        data[i] = d | clocks;
        prev_bit = (d >> 31) & 1;
    }
}
```

### 1.3 Track-Struktur (Amiga OFS/FFS)

```c
/* Amiga Sektor-Header (aus mfm.S) */
struct amiga_sector_info {
    uint8_t format;         /* 0xFF = Standard */
    uint8_t track;          /* Track-Nummer (0-159) */
    uint8_t sector;         /* Sektor (0-10 für DD) */
    uint8_t sectors_to_gap; /* 11 - sector */
};

/* Sync-Word */
#define AMIGA_SYNC 0x4489

/* Track-Dekodierung:
   1. Suche 0x4489 Sync
   2. Lese Header (2 Longwords Odd+Even)
   3. Lese 16 Bytes Label (Odd+Even)
   4. Lese Header-Checksum (2 Longwords)
   5. Lese Data-Checksum (2 Longwords)
   6. Lese 512 Bytes Data (Odd+Even = 256 Longwords)
*/
```

### 1.4 Precompensation

```c
/* Amiga Write Precompensation (aus floppy.c):
   140ns für Zylinder 40-79 */
if (cur_cyl >= 40)
    cust->adkcon = 0xa000;  /* Enable precomp */
```

---

## 2. Amiga Hardware-Konstanten

### 2.1 CIA-B Floppy-Pins

```c
/* CIAB PRB (Port B) - Floppy Control */
#define CIABPRB_STEP 0x01   /* Step Pulse (active low) */
#define CIABPRB_DIR  0x02   /* Direction: 0=inward, 1=outward */
#define CIABPRB_SIDE 0x04   /* Side: 0=lower, 1=upper */
#define CIABPRB_SEL0 0x08   /* Select DF0: */
#define CIABPRB_SEL1 0x10   /* Select DF1: */
#define CIABPRB_SEL2 0x20   /* Select DF2: */
#define CIABPRB_SEL3 0x40   /* Select DF3: */
#define CIABPRB_MTR  0x80   /* Motor: 0=on, 1=off */

/* CIAA PRA (Port A) - Floppy Status */
#define CIAAPRA_CHNG 0x04   /* Disk changed */
#define CIAAPRA_WPRO 0x08   /* Write protected */
#define CIAAPRA_TK0  0x10   /* Track 0 sensor */
#define CIAAPRA_RDY  0x20   /* Drive ready */
```

### 2.2 Custom Chip Disk Register

```c
/* ADKCON - Disk Control */
#define ADK_MFM     0x0100  /* MFM mode (vs GCR) */
#define ADK_WORDSYNC 0x0400 /* Enable word sync */
#define ADK_MSBSYNC 0x0200  /* MSB sync */
#define ADK_PRECOMP 0x2000  /* Precompensation enable */

/* Standard Einstellung für MFM Read: */
cust->adkcon = 0x7f00;      /* Clear all */
cust->adkcon = 0x9500;      /* MFM, wordsync, fast */
cust->dsksync = 0x4489;     /* Amiga sync word */
```

### 2.3 Drive ID Protocol

```c
/* Amiga Drive IDs */
#define DRT_AMIGA      0x00000000  /* Standard DD drive */
#define DRT_37422D2S   0x55555555  /* 40-track drive */
#define DRT_150RPM     0xAAAAAAAA  /* HD drive (Gotek) */
#define DRT_EMPTY      0xFFFFFFFF  /* No drive */

/* ID-Leseprotokoll (32 Bit seriell):
   1. MTRXD low
   2. SELxB low
   3. SELxB high
   4. MTRXD high
   5. SELxB low
   6. SELxB high
   7-38: Toggle SEL, read RDY bit each time
*/
```

---

## 3. CRC16-CCITT in 68k Assembly

```asm
/* Table-driven CRC16-CCITT
   Poly: 0x1021
   Init: 0xFFFF
   
   Table-Layout: 256 Bytes High + 256 Bytes Low */

crc16_ccitt:
    /* a0 = data, a1 = table, d0 = crc_in, d1 = # bytes */
    move.b  d0,d2           /* d2 = crc.lo */
    lsr.w   #8,d0           /* d0 = crc.hi */
.loop:
    move.b  (a0)+,d3
    eor.b   d3,d0           /* crc.hi ^= *p++ */
    lea     (a1,d0.w),a2
    move.b  (a2),d0         /* crc.hi = tab[idx].hi */
    eor.b   d2,d0           /* crc.hi ^= crc.lo */
    move.b  256(a2),d2      /* crc.lo = tab[idx].lo */
    dbf     d1,.loop
    /* Result in d0.w */
```

**UFT C-Implementation (Split-Table):**
```c
/* Split-Table für bessere Cache-Nutzung */
static uint8_t crc16_hi[256], crc16_lo[256];

void crc16_gentable(void) {
    for (int i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x8000) ? 0x1021 : 0);
        }
        crc16_hi[i] = crc >> 8;
        crc16_lo[i] = crc & 0xFF;
    }
}

uint16_t crc16_ccitt_split(const uint8_t *data, size_t len) {
    uint8_t hi = 0xFF, lo = 0xFF;
    while (len--) {
        uint8_t idx = hi ^ *data++;
        hi = crc16_hi[idx] ^ lo;
        lo = crc16_lo[idx];
    }
    return (hi << 8) | lo;
}
```

---

## 4. Q1 Minicomputer Format

### 4.1 Unbekanntes 4-Level Encoding

Q1 verwendet ein **proprietäres 4-Level Encoding** (nicht MFM/FM!):

```c
/* Q1 Thresholds (Catweasel Samples) */
#define Q1_THRESH_SHORT  39   /* Short pulse */
#define Q1_THRESH_MED    57   /* Medium pulse */
#define Q1_THRESH_LONG   75   /* Long pulse */
#define Q1_GARBAGE       98   /* Garbage/Index */

/* Sample → Bit-Sequenz:
   Short  (<=39): 1 + 1 Zero  = "10"
   Medium (<=57): 1 + 2 Zeros = "100"
   Long   (<=75): 1 + 3 Zeros = "1000"
   XLong  (<=98): 1 + 4 Zeros = "10000"
   Garbage (>98): Skip
*/
```

### 4.2 EBCDIC Konversion

Q1 verwendet **EBCDIC** statt ASCII (IBM Mainframe-Erbe):

```c
/* Vollständige EBCDIC → ASCII Tabelle */
static const uint8_t e2a[256] = {
    0,  1,  2,  3,156,  9,134,127,151,141,142, 11, 12, 13, 14, 15,
    16, 17, 18, 19,157,133,  8,135, 24, 25,146,143, 28, 29, 30, 31,
    /* ... */
    48, 49, 50, 51, 52, 53, 54, 55, 56, 57,250,251,252,253,254,255
};

/* EBCDIC Besonderheiten:
   - Buchstaben nicht kontinuierlich!
   - A-I = 0xC1-0xC9
   - J-R = 0xD1-0xD9
   - S-Z = 0xE2-0xE9
   - 0-9 = 0xF0-0xF9
*/
```

### 4.3 Q1 Address/Data Marks

```c
/* Q1 Sync Patterns (32-bit) */
#define Q1_ADDRESS_MARK 0x55424954  /* "UBIT" */
#define Q1_DATA_MARK    0x55424945  /* "UBIE" */

/* Unterschied im letzten Nibble:
   Address: ...0100 (T)
   Data:    ...0101 (E)
*/
```

### 4.4 Postcompensation

```c
/* Q1 verwendet Software-Postcompensation */
float postcomp = 0.5;
float adj = 0.0;

void process_sample(int sample) {
    int len = classify_sample(sample + adj);
    
    /* Postcompensation: Korrigiere nächsten Sample
       basierend auf Abweichung vom idealen Wert */
    adj = (sample - ideal_value(len)) * postcomp;
}
```

---

## 5. UFT Integration

### 5.1 Amiga MFM Decoder (C-Port)

```c
/* uft_amiga_mfm.h */

#include <stdint.h>

#define AMIGA_SYNC       0x4489
#define AMIGA_SECTORS_DD 11
#define AMIGA_SECTORS_HD 22
#define AMIGA_SECTOR_SIZE 512
#define AMIGA_TRACK_SIZE_DD (AMIGA_SECTORS_DD * AMIGA_SECTOR_SIZE)

/* Odd/Even Bit-Mask */
#define ODD_EVEN_MASK 0x55555555

/**
 * @brief Dekodiere Amiga MFM Longword
 */
static inline uint32_t amiga_decode_long(uint32_t odd, uint32_t even) {
    return ((odd & ODD_EVEN_MASK) << 1) | (even & ODD_EVEN_MASK);
}

/**
 * @brief Enkodiere zu Amiga MFM (Odd/Even Split)
 */
static inline void amiga_encode_long(uint32_t data, 
                                      uint32_t *odd, uint32_t *even) {
    *even = data & ODD_EVEN_MASK;
    *odd = (data >> 1) & ODD_EVEN_MASK;
}

/**
 * @brief Berechne Amiga Sektor-Checksum
 */
static inline uint32_t amiga_sector_checksum(const uint32_t *data,
                                              size_t longs) {
    uint32_t csum = 0;
    for (size_t i = 0; i < longs; i++) {
        csum ^= data[i];
    }
    return csum & ODD_EVEN_MASK;
}
```

### 5.2 EBCDIC Support

```c
/* uft_ebcdic.h - Für Mainframe-Disk-Formate */

/**
 * @brief EBCDIC → ASCII Konversion
 */
static inline char ebcdic_to_ascii(uint8_t e) {
    static const uint8_t e2a[256] = { /* ... Tabelle ... */ };
    return e2a[e];
}

/**
 * @brief ASCII → EBCDIC Konversion
 */
static inline uint8_t ascii_to_ebcdic(char a) {
    static const uint8_t a2e[256] = { /* ... Tabelle ... */ };
    return a2e[(uint8_t)a];
}

/**
 * @brief Konvertiere EBCDIC-Block zu ASCII
 */
void ebcdic_block_to_ascii(char *out, const uint8_t *in, size_t len);
```

### 5.3 Flux Histogram

```c
/* uft_flux_histogram.h - Aus q1decode/hist.c */

typedef struct {
    uint32_t bins[256];
    uint32_t total;
    uint32_t min_sample;
    uint32_t max_sample;
} flux_histogram_t;

void flux_histogram_init(flux_histogram_t *h);
void flux_histogram_add(flux_histogram_t *h, uint8_t sample);
void flux_histogram_print(const flux_histogram_t *h);
int flux_histogram_find_peaks(const flux_histogram_t *h, 
                               uint8_t *peaks, int max_peaks);
```

---

## 6. Schlüssel-Erkenntnisse

### Amiga-Stuff

| Feature | Beschreibung | UFT-Wert |
|---------|--------------|----------|
| Odd/Even Split | Schnelle MFM-Dekodierung | 🔥 HOCH |
| Clock Insertion | NOR-basiert | MITTEL |
| Hardware-Defs | Vollständig | HOCH |
| CRC16 Split-Table | Cache-optimiert | HOCH |
| Precompensation | 140ns ab Cyl 40 | MITTEL |
| Drive ID Protocol | 32-bit seriell | NISCHE |

### Q1Decode

| Feature | Beschreibung | UFT-Wert |
|---------|--------------|----------|
| EBCDIC Tabellen | Vollständig | MITTEL |
| 4-Level Encoding | Proprietär | NISCHE |
| Postcompensation | Software-basiert | MITTEL |
| Flux Histogram | Sample-Analyse | HOCH |

---

## 7. Empfohlene UFT-Erweiterungen

1. **Amiga OFS/FFS Support**
   - Odd/Even MFM-Decoder
   - Sektor-Checksum-Verifikation
   - ADF-Image-Export

2. **EBCDIC-Modul**
   - Für IBM 3740, Q1, und andere Mainframe-Formate
   - Bidirektionale Konversion

3. **Flux-Histogram-Widget**
   - Visualisierung in GUI
   - Automatische Threshold-Erkennung
   - Peak-Detection für Auto-Format-Erkennung

4. **Amiga Precompensation**
   - Konfigurierbar pro Zylinder-Zone
   - Wichtig für Write-Support

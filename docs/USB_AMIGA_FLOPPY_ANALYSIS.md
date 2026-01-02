# USB-Amiga-Floppy & Amiga-GCC Analysis für UFT

**Analysiert:** 2024-12-30
**Quellen:** usbamigafloppy-master.zip, amiga-gcc-master.zip
**Autoren:** John Tsiombikas, Robert Smith (@RobSmithDev)

---

## Executive Summary

### USB-Amiga-Floppy
Komplettes USB-Floppy-Controller-Projekt mit:
- **AVR Firmware** - Echtzeit-MFM-Lesen/Schreiben auf Mikrocontroller
- **Host-Software** - MFM-Dekodierung, ADF-Export
- **Hardware-Design** - KiCad Schaltpläne

### Amiga-GCC
GCC-Toolchain-Patches mit:
- **volatile Qualifiers** - Kritisch für Hardware-Register!
- **Vollständige Register-Maps** - CIA, Custom Chips

---

## 1. MFM Flux-Kompression (AVR → Host)

### 1.1 Run-Length-Encoding für Flux-Daten

Die AVR-Firmware komprimiert Flux-Timing-Daten in 2-Bit-Codes:

```c
/* AVR Firmware: Timing-Klassifizierung */
/* Timer @ 16MHz, Prescale=1 → 62.5ns pro Tick */

if (counter < 80) {           /* < 5µs: Short (2T) */
    data_output_byte |= 1;    /* Code: 01 */
    total_bits += 2;
} else if (counter > 111) {   /* > 6.9µs: Long (4T) */
    data_output_byte |= 3;    /* Code: 11 */
    total_bits += 4;
} else {                      /* 5-6.9µs: Medium (3T) */
    data_output_byte |= 2;    /* Code: 10 */
    total_bits += 3;
}
/* Code 00 = End of Data */
```

### 1.2 Host-Dekompression

```c
/* Host Software: Dekompression */
static int uncompress(unsigned char *dest, unsigned char *src, int size)
{
    int outbits = 0;
    unsigned int val = 0;
    
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < 4; j++) {
            int shift = (~j & 3) * 2;
            switch ((*src >> shift) & 3) {
            case 1:  /* Short: 10 */
                val = (val << 2) | 1;
                outbits += 2;
                break;
            case 2:  /* Medium: 100 */
                val = (val << 3) | 1;
                outbits += 3;
                break;
            case 3:  /* Long: 1000 */
                val = (val << 4) | 1;
                outbits += 4;
                break;
            case 0:  /* End of Data */
                goto done;
            }
            
            if (outbits >= 8) {
                *dest++ = (val >> (outbits - 8)) & 0xFF;
                outbits -= 8;
            }
        }
        ++src;
    }
done:
    return dest - start;
}
```

---

## 2. Amiga Sektor-Struktur (Detailliert)

### 2.1 Sektor-Header

```c
struct sector_header {
    unsigned char magic[4];     /* AA AA 44 89 44 89 (nach Alignment) */
    unsigned char fmt;          /* Format: 0xFF */
    unsigned char track;        /* Track: 0-159 */
    unsigned char sector;       /* Sektor: 0-10 */
    unsigned char sec_to_gap;   /* 11 - sector */
    unsigned char osinfo[16];   /* OS Label (meist 0) */
    uint32_t hdr_sum;           /* Header Checksum */
    uint32_t data_sum;          /* Daten Checksum */
} __attribute__((packed));
```

### 2.2 MFM Layout im Speicher

```
Offset  Inhalt                  Größe (Bytes MFM)
------  ----------------------  -----------------
0x00    Magic (AA AA 44 89...)  8
0x08    Sector Info (fmt etc)   8 (4 bytes × 2)
0x10    OS Info                 32 (16 bytes × 2)
0x30    Header Checksum         8 (4 bytes × 2)
0x38    Data Checksum           8 (4 bytes × 2)
0x40    Sector Data             1024 (512 bytes × 2)
```

### 2.3 Sync-Pattern-Suche (Bit-Alignment)

```c
/* Magic Pattern: Nach AA AA suchen, dann 44 89 44 89 */
static const unsigned char magic[] = { 
    0xaa, 0xaa, 0xaa, 0xaa, 0x44, 0x89, 0x44, 0x89 
};

/* Bit-genaues Alignment nötig! */
static int align_track(unsigned char *buf, int size)
{
    for (int i = 0; i < size - sizeof(magic) - 1; i++) {
        for (int shift = 0; shift < 8; shift++) {
            copy_bits(tmp, ptr, sizeof(magic), shift);
            if (check_magic(tmp)) {
                /* Gefunden bei Offset i, Bit-Shift shift */
                copy_bits(buf, ptr, size - i, shift);
                return 0;
            }
        }
        ++ptr;
    }
    return -1;  /* Nicht gefunden */
}
```

### 2.4 Bit-Kopie mit Shift

```c
/* Kopiere Bytes mit Bit-Verschiebung */
static void copy_bits(unsigned char *dest, unsigned char *src, 
                      int size, int shift)
{
    if (!shift) {
        memcpy(dest, src, size);
    } else {
        for (int i = 0; i < size; i++) {
            *dest++ = (src[0] << shift) | (src[1] >> (8 - shift));
            ++src;
        }
    }
}
```

---

## 3. Verbesserte MFM-Dekodierung

### 3.1 Byte-weise Odd/Even Dekodierung

```c
/* Alternative Implementierung - Byte-weise statt Longword-weise */
static void decode_mfm(unsigned char *dest, unsigned char *src, int blksz)
{
    for (int i = 0; i < blksz; i++) {
        unsigned char even = src[blksz];  /* Even bytes nach Odd */
        unsigned char odd = *src++;
        
        /* 4 Bit-Paare pro Byte */
        *dest = 0;
        for (int j = 0; j < 4; j++) {
            *dest <<= 2;
            if (even & 0x40) *dest |= 1;  /* Even bit */
            if (odd & 0x40) *dest |= 2;   /* Odd bit */
            even <<= 2;
            odd <<= 2;
        }
        ++dest;
    }
}
```

### 3.2 Amiga Checksum

```c
/* Amiga Checksum: XOR + Shift + Mask */
static uint32_t checksum(void *buf, int size)
{
    uint32_t *p = buf;
    uint32_t sum = 0;
    
    size /= 4;
    for (int i = 0; i < size; i++) {
        sum ^= ntohl(*p++);  /* Big-Endian! */
    }
    
    /* Amiga-spezifische Transformation */
    return (sum ^ (sum >> 1)) & 0x55555555;
}
```

---

## 4. AVR Timing-Konstanten

### 4.1 MFM-Timing (500 kbit/s)

```c
/* 16 MHz AVR, Prescale=1 */
#define TIMER_TICK_NS     62.5    /* Nanosekunden pro Tick */

/* MFM Bitcell = 2µs bei 500 kbit/s */
#define BITCELL_TICKS     32      /* 2µs / 62.5ns */

/* Pulse-Zeiten in Timer-Ticks */
#define SHORT_MIN         0       /* 2T: 4µs */
#define SHORT_MAX         80      /* ~5µs */
#define MEDIUM_MIN        80      
#define MEDIUM_MAX        111     /* ~7µs */
#define LONG_MIN          111     /* 4T: 8µs */

/* Write Timing (Timer-Werte für Bit-Position) */
#define WRITE_BIT_0       0x10    /* Bit 7 */
#define WRITE_BIT_1       0x30    /* Bit 6 */
#define WRITE_BIT_2       0x50    /* Bit 5 */
#define WRITE_BIT_3       0x70    /* Bit 4 */
#define WRITE_BIT_4       0x90    /* Bit 3 */
#define WRITE_BIT_5       0xB0    /* Bit 2 */
#define WRITE_BIT_6       0xD0    /* Bit 1 */
#define WRITE_BIT_7       0xF0    /* Bit 0 */
```

### 4.2 Track-Größe

```c
/* Paula-kompatible Track-Größe */
#define RAW_TRACKDATA_LENGTH  (0x1900 * 2 + 0x440)  /* 13888 Bytes */

/* Berechnung:
   - Paula liest 0x1900 Words = 6400 Words = 12800 Bytes
   - Plus 0x440 = 1088 Bytes extra
   - Total: 13888 Bytes
   
   Bei 300 RPM, 500 kbit/s:
   - 1 Umdrehung = 200ms
   - 500000 bits/s × 0.2s = 100000 bits = 12500 Bytes
   - 13888 > 12500 → Kompletter Track + Überlappung
*/
```

---

## 5. USB-Protokoll (Commands)

### 5.1 Command Reference

| Cmd | Funktion | Response |
|-----|----------|----------|
| `?` | Firmware Version | `1V1.3` |
| `+` | Motor ON (Read Mode) | `1`/`0` |
| `~` | Motor ON (Write Mode) | `1`/`0`/`N` |
| `-` | Motor OFF | `1` |
| `.` | Goto Track 0 | `1`/`0`/`#` |
| `#xx` | Goto Track xx | `1`/`0` |
| `[` | Select Lower Head | `1` |
| `]` | Select Upper Head | `1` |
| `<` | Read Track | `1` + data |
| `>` | Write Track | `1`/`0`/`N`/`Y` |
| `X` | Erase Track (0xAA) | `1`/`0`/`N`/`Y` |
| `&x` | Diagnostic | varies |

### 5.2 Serial Settings

```c
#define BAUDRATE          2000000  /* 2 Mbit/s! */
#define SERIAL_BUFFER_SIZE 128
#define TIMEOUT_MSEC      2000

/* Hardware Flow Control (CTS) */
CTS_PORT &= ~CTS_BIT;  /* Ready to receive */
CTS_PORT |= CTS_BIT;   /* Buffer full */
```

---

## 6. volatile Qualifier (Amiga-GCC)

### 6.1 Warum volatile kritisch ist

```c
/* OHNE volatile - Compiler optimiert weg! */
struct Custom *cust = (struct Custom *)0xDFF000;
cust->dmacon = 0x8010;  /* DMA an */
while (cust->dmaconr & 0x0010);  /* Loop wird wegoptimiert! */

/* MIT volatile - Korrekt */
volatile struct Custom *cust = (volatile struct Custom *)0xDFF000;
cust->dmacon = 0x8010;
while (cust->dmaconr & 0x0010);  /* Jedes Mal gelesen */
```

### 6.2 Register-Zugriff Pattern

```c
/* Korrektes Pattern für Hardware-Zugriff */
#define CUSTOM_BASE  ((volatile struct Custom *)0xDFF000)
#define CIAA_BASE    ((volatile struct CIA *)0xBFE001)
#define CIAB_BASE    ((volatile struct CIA *)0xBFD000)

/* Makros für sicheren Zugriff */
#define CUSTOM_WRITE(reg, val)  (CUSTOM_BASE->reg = (val))
#define CUSTOM_READ(reg)        (CUSTOM_BASE->reg)
```

---

## 7. UFT Integration

### 7.1 Flux-Kompression für UFT

```c
/* uft_flux_compress.h */

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} flux_compressed_t;

/* Komprimiere Flux-Timings zu 2-Bit-Codes */
int flux_compress(flux_compressed_t *out, 
                  const uint32_t *timings, 
                  size_t count,
                  uint32_t short_thresh,
                  uint32_t long_thresh);

/* Dekomprimiere zu Bitstream */
int flux_decompress(uint8_t *bitstream,
                    size_t *bit_count,
                    const flux_compressed_t *in);
```

### 7.2 Verbessertes Bit-Alignment

```c
/* uft_bit_align.h */

typedef struct {
    int byte_offset;
    int bit_shift;
    int confidence;
} alignment_result_t;

/* Finde Sync-Pattern mit Bit-Genauigkeit */
int find_sync_aligned(alignment_result_t *result,
                      const uint8_t *data, size_t size,
                      const uint8_t *pattern, size_t plen);

/* Extrahiere Bits mit Alignment */
void extract_bits_aligned(uint8_t *dest, 
                          const uint8_t *src, 
                          size_t bytes,
                          int bit_shift);
```

---

## 8. Schlüssel-Erkenntnisse

### USB-Amiga-Floppy

| Feature | Beschreibung | UFT-Wert |
|---------|--------------|----------|
| Flux-Kompression | 2-Bit RLE | 🔥 HOCH |
| Bit-Alignment | 8-fach Suche | HOCH |
| Checksum | XOR + Shift | HOCH |
| USB-Protokoll | 2Mbit/s + CTS | MITTEL |
| Write-Timing | Timer2-basiert | REFERENZ |

### Amiga-GCC

| Feature | Beschreibung | UFT-Wert |
|---------|--------------|----------|
| volatile Patches | Hardware-Register | KRITISCH |
| Register-Maps | CIA + Custom | HOCH |
| Struct Layout | Vollständig | HOCH |

---

## 9. ADF-Workshop

Das ADF-Workshop Archiv enthält **keine Floppy-relevanten Inhalte** - es ist ein Oracle ADF (Application Development Framework) Java-Projekt für Web-Anwendungen. Nicht relevant für UFT.

# Archive Analysis - Batch 2 (RIDE, fileextractor, floppy8, c128_bdos)

## Executive Summary

| Archive | Files | Key Value |
|---------|-------|-----------|
| **RIDE** | 173 | Complete disk editor with GW/KF/SCP/HFE support, DOS recognition, precompensation |
| **fileextractor** | 33 | Magic byte signatures for 20+ file types |
| **floppy8** | 4 | Clean FM/MFM decoder with CRC-16, format auto-detection |
| **c128_bdos** | 2+ | FAT/BPB parsing for 6502 (reference) |

---

## 1. RIDE - Reversible Image Disk Editor

**Location:** `/home/claude/ride/RIDE-master/`

RIDE is a comprehensive Windows disk image editor supporting:
- Real hardware: Greaseweazle V4, KryoFlux, fdrawcmd.sys
- Image formats: SCP, HFE, IPF, KryoFlux streams, DSK
- DOS systems: MS-DOS, TR-DOS, GDOS, BSDOS, SpectrumDos

### 1.1 Greaseweazle Protocol (Greaseweazle.h/cpp)

```cpp
// Command codes
enum TRequest : BYTE {
    GET_INFO        = 0,
    UPDATE          = 1,
    SEEK_ABS        = 2,
    HEAD            = 3,
    SET_PARAMS      = 4,
    GET_PARAMS      = 5,
    MOTOR           = 6,
    READ_FLUX       = 7,
    WRITE_FLUX      = 8,
    GET_FLUX_STATUS = 9,
    GET_INDEX_TIMES = 10,
    SWITCH_FW_MODE  = 11,
    SELECT_DRIVE    = 12,
    DESELECT_DRIVE  = 13,
    SET_BUS_TYPE    = 14,
    SET_PIN         = 15,
    SOFT_RESET      = 16,
    ERASE_FLUX      = 17,
    SOURCE_BYTES    = 18,
    SINK_BYTES      = 19,
    GET_PIN         = 20,
    TEST_MODE       = 21,
    NO_CLICK_STEP   = 22
};

// Response codes
enum TResponse : BYTE {
    OKAY            = 0,
    BAD_COMMAND     = 1,
    NO_INDEX        = 2,
    NO_TRK0         = 3,
    FLUX_OVERFLOW   = 4,
    FLUX_UNDERFLOW  = 5,
    WRPROT          = 6,
    NO_UNIT         = 7,
    NO_BUS          = 8,
    BAD_UNIT        = 9,
    BAD_PIN         = 10,
    BAD_CYLINDER    = 11,
    OUT_OF_SRAM     = 12,
    OUT_OF_FLASH    = 13
};

// Bus types
enum TBusType : BYTE {
    BUS_UNKNOWN,
    BUS_IBM,
    BUS_SHUGART,
    BUS_LAST
};
```

### 1.2 Greaseweazle V4 Stream Parsing

```cpp
// Flux stream opcodes (after 0xFF prefix)
enum TFluxOp : BYTE {
    Index   = 1,    // Index pulse information
    Space   = 2,    // Long flux (unformatted area)
    Astable = 3,    // Astable region
    Special = 255
};

// Read 28-bit value from stream
static int ReadBits28(PCBYTE p) {
    return p[0]>>1 | (p[1]&0xfe)<<6 | (p[2]&0xfe)<<13 | (p[3]&0xfe)<<20;
}

// Write 28-bit value to stream
static PBYTE WriteBits28(int i, PBYTE p) {
    *p++ = 1 | i<<1;
    *p++ = 1 | i>>6;
    *p++ = 1 | i>>13;
    *p++ = 1 | i>>20;
    return p;
}

// Stream to Track conversion
CTrackReaderWriter GwV4StreamToTrack(PCBYTE p, DWORD length) {
    for (int sampleCounter=0, sampleCounterSinceIndex=0; p < pEnd; p++) {
        const BYTE i = *p;
        if (i < TFluxOp::Special) {
            // Flux information
            if (i < 250) {
                // Short flux (1-249 samples)
                sampleCounter += i;
            } else {
                // Long flux (250-1524 samples, two bytes)
                sampleCounter += 250 + (i-250)*255 + *++p - 1;
            }
            result.AddTime(result.GetLastIndexTime() + sampleClock * (sampleCounterSinceIndex += sampleCounter));
            sampleCounter = 0;
        } else {
            // Special: 0xFF prefix
            switch ((TFluxOp)*++p) {
                case TFluxOp::Index:
                    // Index pulse
                    int value = ReadBits28(p);
                    result.AddIndexTime(result.GetLastIndexTime() + sampleClock * (sampleCounterSinceIndex + sampleCounter + value));
                    sampleCounterSinceIndex = -(sampleCounter + value);
                    break;
                case TFluxOp::Space:
                    // Extra long flux (1525 to 2^28-1)
                    sampleCounter += ReadBits28(p);
                    break;
            }
        }
    }
}
```

### 1.3 fdrawcmd.sys IOCTL Commands

```c
// FDC raw commands via Windows driver
#define IOCTL_FDCMD_READ_TRACK          0x0022e00a
#define IOCTL_FDCMD_SPECIFY             0x0022e00c
#define IOCTL_FDCMD_SENSE_DRIVE_STATUS  0x0022e010
#define IOCTL_FDCMD_WRITE_DATA          0x0022e015
#define IOCTL_FDCMD_READ_DATA           0x0022e01a
#define IOCTL_FDCMD_RECALIBRATE         0x0022e01c
#define IOCTL_FDCMD_SENSE_INT_STATUS    0x0022e020
#define IOCTL_FDCMD_WRITE_DELETED_DATA  0x0022e025
#define IOCTL_FDCMD_READ_ID             0x0022e028
#define IOCTL_FDCMD_READ_DELETED_DATA   0x0022e032
#define IOCTL_FDCMD_FORMAT_TRACK        0x0022e034
#define IOCTL_FDCMD_SEEK                0x0022e03c
#define IOCTL_FDCMD_RELATIVE_SEEK       0x0022e23c
#define IOCTL_FDCMD_FORMAT_AND_WRITE    0x0022e3bc

// Extended operations
#define IOCTL_FD_SCAN_TRACK             0x0022e400
#define IOCTL_FD_SET_DATA_RATE          0x0022e410
#define IOCTL_FD_RAW_READ_TRACK         0x0022e45a
#define IOCTL_FD_GET_TRACK_TIME         0x0022e460

// Data rates
#define FD_RATE_500K    0   // HD
#define FD_RATE_300K    1   // DD on HD drive
#define FD_RATE_250K    2   // DD
#define FD_RATE_1M      3   // ED

// FDC types
#define FDC_TYPE_82077      4
#define FDC_TYPE_82077AA    5
#define FDC_TYPE_82078_44   6
#define FDC_TYPE_82078_64   7
#define FDC_TYPE_NATIONAL   8

// Command flags
#define FD_OPTION_MT    0x80  // Multi-track
#define FD_OPTION_MFM   0x40  // MFM encoding
#define FD_OPTION_SK    0x20  // Skip deleted
#define FD_OPTION_FM    0x00  // FM encoding

// Structures
typedef struct {
    BYTE cyl, head, sector, size;
} FD_ID_HEADER;

typedef struct {
    BYTE flags;     // MT MFM SK
    BYTE phead;
    BYTE cyl, head, sector, size;
    BYTE eot, gap, datalen;
} FD_READ_WRITE_PARAMS;

typedef struct {
    BYTE st0, st1, st2;
    BYTE cyl, head, sector, size;
} FD_CMD_RESULT;

typedef struct {
    DWORD reltime;  // Time relative to index (µs)
    BYTE cyl, head, sector, size;
} FD_TIMED_ID_HEADER;
```

### 1.4 HFE Format Support

```cpp
#define HEADER_SIGNATURE_V1 "HXCPICFE"
#define HEADER_SIGNATURE_V3 "HXCHFEV3"
#define BASE_FREQUENCY      36000000  // 36 MHz

enum TTrackEncoding : BYTE {
    ISOIBM_MFM,
    AMIGA_MFM,
    ISOIBM_FM,
    EMU_FM,
    UNKNOWN = 0xFF
};

enum TFloppyInterface : BYTE {
    IBMPC_DD,
    IBMPC_HD,
    ATARIST_DD,
    ATARIST_HD,
    AMIGA_DD,
    AMIGA_HD,
    CPC_DD,
    GENERIC_SHUGART,
    IBMPC_ED,
    MSX2_DD,
    C64_DD,
    EMU_SHUGART,
    S950_DD,
    S950_HD,
    LAST_KNOWN
};
```

### 1.5 DOS Recognition System

```cpp
// Priority-based DOS recognition
struct TRecognition {
    PCProperties order[256];
    BYTE nDoses;
    
    // Add DOS by priority (higher = earlier check)
    BYTE AddDosByPriorityDescending(PCProperties props);
    
    // Recognition thread
    static UINT Thread(PVOID pCancelableAction);
    
    // Perform recognition
    PCProperties Perform(PImage image, PFormat pOutFormatBoot) const;
};

// Recognition callback signature
typedef TStdWinError (*fnRecognize)(PImage image, PFormat pOutFormatBoot);
```

### 1.6 Precompensation Calculation

```cpp
// Uses Ordinary Least Squares for precomp determination
struct CPrecompensation {
    enum TMethodVersion {
        None,
        Identity,
        MethodVersion1,
        MethodVersion2
    };
    
    // V1: 2D coefficient matrix
    struct {
        double coeffs[2][5];  // [even/odd flux][neighbor distance]
    } v1;
    
    // V2: 3D coefficient matrix
    struct {
        double coeffs[2][5][5];
    } v2;
    
    // Gaussian elimination for solving normal equations
    static UINT PrecompensationDetermination_thread(PVOID pCancelableAction);
};
```

---

## 2. floppy8 - 8-bit Floppy Preservation

**Location:** `/home/claude/floppy8/floppy8-main/`

Clean, minimal FM/MFM decoder for Teensy 4.1 captured data.

### 2.1 Timing Constants (600MHz Teensy)

```c
#define TWO_US      75      // 2µs = 75 counts (600MHz/16 = 37.5 counts/µs)
#define ONE_US      (TWO_US/2)
#define THREE_US    (TWO_US + ONE_US)
#define FOUR_US     (TWO_US * 2)
#define FIVE_US     ((TWO_US * 2) + ONE_US)

#define HALF_US     (ONE_US / 2)
#define ONEP5_US    (ONE_US + HALF_US)
#define TWOP5_US    (TWO_US + HALF_US)
#define THREEP5_US  (THREE_US + HALF_US)
#define FOURP5_US   (FOUR_US + HALF_US)

// Thresholds
#define FM_SPLIT    THREE_US        // FM: 2µs vs 4µs
#define MFM_SPLIT_LO TWOP5_US       // MFM: 2µs vs 3µs
#define MFM_SPLIT_HI THREEP5_US     // MFM: 3µs vs 4µs
```

### 2.2 Format Auto-Detection via Histogram

```c
// Determine FM vs MFM from sample histogram
static int determine_format(const sample_t *samples, unsigned int n) {
    unsigned int histogram[MAX_US] = {0};
    
    for (unsigned int i = 0; i < n; i++) {
        unsigned int us = (samples[i] + ONE_US/2) / ONE_US;
        histogram[us < MAX_US ? us : MAX_US-1]++;
    }
    
    // >5% samples at 3µs = MFM
    return (histogram[3] * 100 / n > 5) ? TT_MFM : TT_FM;
}
```

### 2.3 FM Special Marks (Clock Violations)

```c
// FM marks with clock patterns (bit-level)
uint8_t FM_indx_mark[] = { 1,1,1,0,1,1,0,1,1,1,0,0 };  // Data 0xFC, Clock 0xD7
uint8_t FM_addr_mark[] = { 1,1,1,0,0,0,1,1,1,1,1,0 };  // Data 0xFE, Clock 0xC7
uint8_t FM_data_mark[] = { 1,1,1,0,0,0,1,0,1,1,1,1 };  // Data 0xFB, Clock 0xC7
uint8_t FM_deld_mark[] = { 1,1,1,0,0,0,1,0,0,0,1 };    // Data 0xF8, Clock 0xC7

// MFM marks (sync + ID byte)
uint8_t MFM_indx_mark[] = { 0xC2, 0xC2, 0xC2, 0xFC };
uint8_t MFM_addr_mark[] = { 0xA1, 0xA1, 0xA1, 0xFE };
uint8_t MFM_data_mark[] = { 0xA1, 0xA1, 0xA1, 0xFB };
uint8_t MFM_deld_mark[] = { 0xA1, 0xA1, 0xA1, 0xF8 };
```

### 2.4 CRC-16 CCITT Implementation

```c
// Polynomial: X^16 + X^12 + X^5 + 1
// Initial value: 0xFFFF
// Valid CRC result: 0x0000
static unsigned short crc16(const uint8_t *buf, unsigned int count) {
    unsigned short crc = 0xFFFF;
    
    for (unsigned int i = 0; i < count; i++) {
        uint8_t x = crc >> 8 ^ buf[i];
        x ^= x >> 4;
        crc = (crc << 8) ^ ((unsigned short)(x << 12)) 
                        ^ ((unsigned short)(x << 5)) 
                        ^ ((unsigned short)x);
    }
    return crc;
}
```

### 2.5 FM Byte Extraction

```c
// FM: clock bits interleaved with data bits
// 11 = clock+1, 10 = clock+0, 01 = 1, 00 = error
static uint8_t fm_fetch_byte(uint8_t **buf) {
    unsigned int byte = 0;
    uint8_t *p = *buf;
    
    for (int i = 0; i < 8; i++) {
        byte <<= 1;
        byte |= *p;
        // Skip clock bit if present
        if (p[0] == 1 && p[1] == 1)
            p += 2;  // clock+data
        else
            p += 1;  // data only
    }
    *buf = p;
    return byte;
}
```

### 2.6 MFM Bit Extraction

```c
// MFM: pairs of bits encode single data bit
// 00 -> 0, 01 -> 1, 10 -> 0, 11 -> invalid
static uint8_t mfm_fetch_bit(uint8_t *buf) {
    unsigned int pair = (buf[0] << 1) + buf[1];
    
    switch (pair) {
        case 0: case 2: return 0;
        case 1: return 1;
        default: error("Invalid MFM bit"); return 0;
    }
}

static uint8_t mfm_fetch_byte(uint8_t **buf) {
    unsigned int byte = 0;
    uint8_t *p = *buf;
    
    for (int i = 0; i < 8; i++) {
        byte <<= 1;
        byte |= mfm_fetch_bit(p);
        p += 2;
    }
    *buf = p;
    return byte;
}
```

---

## 3. fileextractor - Magic Byte Signatures

**Location:** `/home/claude/fileextractor/fileextractor-master/`

### 3.1 Signature Detection Types

```python
TYPE_END_SEQUENCE = 0   # File ends with specific bytes
TYPE_FILE_SIZE = 1      # Size encoded in header
TYPE_MANUAL = 2         # Custom parsing needed
```

### 3.2 File Signatures

```python
# JPEG
_jpeg = {
    'start_sequence': [0xFF, 0xD8, 0xFF, 0xE1],
    'end_sequence': [0xFF, 0xD9],
    'skip_end_seqs': 1,  # Skip first end marker
    'filesize_type': TYPE_END_SEQUENCE
}

# BMP (size in header)
_bmp = {
    'start_sequence': [0x42, 0x4D, None, None, None, None, 0x00, 0x00, 0x00, 0x00, 0x36, None, 0x00, 0x00],
    'filesize_address_offsets': [0x05, 0x04, 0x03, 0x02],  # Little-endian at offset 2
    'filesize_type': TYPE_FILE_SIZE
}

# GIF
_gif = {
    'start_sequence': [0x47, 0x49, 0x46, 0x38],  # "GIF8"
    'end_sequence': [0x00, 0x3B],
    'filesize_type': TYPE_END_SEQUENCE
}

# WAV (size in header)
_wav = {
    'start_sequence': [0x52, 0x49, 0x46, 0x46],  # "RIFF"
    'filesize_address_offsets': [0x07, 0x06, 0x05, 0x04],
    'filesize_info_correction': 8,  # Add 8 to size
    'filesize_type': TYPE_FILE_SIZE
}

# ZIP
_zip = {
    'start_sequence': [0x50, 0x4B, 0x03, 0x04],  # "PK\x03\x04"
    'end_sequence': [0x50, 0x4B, 0x05, 0x06],    # End of central directory
    'filesize_type': TYPE_END_SEQUENCE
}

# PDF
_pdf = {
    'start_sequence': [0x25, 0x50, 0x44, 0x46],  # "%PDF"
    'end_sequence': [0x25, 0x25, 0x45, 0x4F, 0x46],  # "%%EOF"
    'filesize_type': TYPE_END_SEQUENCE
}
```

---

## 4. Integration Recommendations

### Priority 1: From RIDE
- **Greaseweazle V4 protocol** - Complete command set for hardware control
- **Stream parsing** - 28-bit encoding for flux values
- **fdrawcmd.sys IOCTLs** - Windows FDC access
- **Precompensation calculation** - OLS-based algorithm
- **DOS recognition system** - Priority-based format detection

### Priority 2: From floppy8
- **FM/MFM auto-detection** - Histogram analysis (>5% at 3µs = MFM)
- **FM clock marks** - Bit-level patterns for address/data marks
- **CRC-16 CCITT** - Clean implementation
- **Timing constants** - Well-documented µs thresholds

### Priority 3: From fileextractor
- **Magic byte signatures** - Extensible signature system
- **Size detection types** - End marker vs header size vs custom

---

## Key Algorithms to Extract

### 1. Greaseweazle Stream Codec
```c
// Compact flux encoding
if (samples < 250) {
    output 1 byte: samples
} else if (samples < 1525) {
    output 2 bytes: 250 + (samples-250)/255, (samples-250)%255 + 1
} else {
    output 5 bytes: 0xFF, 0x02, 28-bit value
}

// Index encoding
output 5 bytes: 0xFF, 0x01, 28-bit sample offset
```

### 2. Format Detection Histogram
```c
// Build timing histogram
for (each sample) histogram[sample_us]++;

// Classify
if (histogram[3µs] / total > 5%) return MFM;
else return FM;
```

### 3. Write Precompensation
```c
// Determine coefficients using OLS:
// A = matrix of neighbor distances
// dt = measured timing errors
// Solve (A^T * A) * x = A^T * dt
```

# raszpl/sigrok-disk - Sigrok FM/MFM/RLL Decoder

## Source
- **Repository**: https://github.com/raszpl/sigrok-disk
- **License**: GPL-3.0
- **Language**: Python (sigrok protocol decoder)
- **Stars**: 7

## Overview

Comprehensive protocol decoder for Sigrok/PulseView/DSView that analyzes
floppy disk and MFM/RLL hard drive data at the low level.

## Key Features

### Encoding Support
- **FM** (Frequency Modulation) - Single density floppies
- **MFM** (Modified FM) - Double density, hard drives
- **RLL** (Run Length Limited) - Multiple variants:
  - RLL_Seagate
  - RLL_Adaptec
  - RLL_Adaptec4070
  - RLL_WD
  - RLL_OMTI
  - Custom mode

### PLL Implementation

#### PI Loop Filter PLL
```python
# Modern decoder with Proportional-Integral control
class PLL:
    def __init__(self):
        self.kp = 0.5        # Proportional constant
        self.ki = 0.0005     # Integral constant
        self.sync_tol = 0.25 # Initial sync tolerance (25%)
    
    def process_transition(self, delta):
        # Error calculation
        error = delta - expected_period
        
        # PI filter
        self.integral += error
        adjustment = (self.kp * error) + (self.ki * self.integral)
        
        # Update expected period
        expected_period += adjustment
```

#### Legacy Decoder
```python
# Hardcoded immediate adjustments
# Simpler but less robust
```

## CRC/ECC Polynomials

### Standard Polynomials
| Polynomial | Format | Description |
|------------|--------|-------------|
| 0x1021 | CRC-16 | CCITT standard |
| 0xA00805 | CRC-32 | CCSDS (DEC VAX) |
| 0x140a0445 | CRC-32 | WD1003/1006 |
| 0x0104c981 | CRC-32 | OMTI 8240/5510 |
| 0x41044185 | CRC-32 | Seagate ST11/21 |

### ECC Polynomials
| Polynomial | Bits | Controller |
|------------|------|------------|
| 0x140a0445000101 | 56 | WD40C22 |
| 0x181814503011 | 48 | OMTI/Adaptec |

### Polynomial Conversion
```
x32 + x23 + x21 + x11 + x2 + 1
= 0b10000000101000000000100000000101 (0x80A00805)
Drop MSB = 0b101000000000100000000101 (0xA00805)
```

## Data Rates

| Rate (bps) | Application |
|------------|-------------|
| 125,000 | FM floppy |
| 250,000 | MFM floppy (DD) |
| 500,000 | MFM floppy (HD) |
| 5,000,000 | MFM hard drive |
| 7,500,000 | RLL hard drive |
| 10,000,000 | Fast RLL |

## Sector Detection

### Standard MFM Marks
```
Sync:    A1 A1 A1 (with clock violations)
ID Mark: FE
Data:    FB (normal) / F8 (deleted)
```

### Sector Layout
```
┌─────────┬──────────┬────────────┬─────────┬──────────┐
│  GAP3   │ SYNC(A1) │ ID(FE+HDR) │  CRC16  │  GAP2    │
├─────────┼──────────┼────────────┼─────────┼──────────┤
│ ~4E...  │ A1A1A1   │ CHHSLL     │ 2 bytes │ ~4E...   │
└─────────┴──────────┴────────────┴─────────┴──────────┘
                        │
                        ▼
┌─────────┬──────────┬────────────┬─────────┬──────────┐
│  GAP2   │ SYNC(A1) │ DATA(FB)   │  CRC32  │  GAP4    │
├─────────┼──────────┼────────────┼─────────┼──────────┤
│ ~4E...  │ A1A1A1   │ 512 bytes  │ 4 bytes │ ~4E...   │
└─────────┴──────────┴────────────┴─────────┴──────────┘
```

## Header Formats

### Format 3 (Standard)
```
Byte 0: Cylinder High
Byte 1: Cylinder Low
Byte 2: Head
Byte 3: Sector
```

### Format 4 (Extended)
```
Byte 0: Cylinder High
Byte 1: Cylinder Low
Byte 2: Head
Byte 3: Sector
Byte 4: Sector Size (log2)
```

### Seagate Format
```
Custom header structure with
32-bit CRC using poly 0x41044185
```

### OMTI Format
```
Custom header with
32-bit CRC init 0x2605fb9c
48-bit Data ECC init 0x6062ebbf22b4
```

## RLL Encoding Details

### RLL 2,7 (Most Common)
```
Constraints:
- Minimum 2 zeros between ones
- Maximum 7 zeros between ones

Data      | Encoded
----------|----------
11        | 1000
10        | 0100
000       | 100100
010       | 000100
011       | 001000
0010      | 00100100
0011      | 00001000
```

### RLL 1,7 (Adaptec)
```
Higher density variant
Used by AIC-6225 (33 Mbit/s)
```

## Custom Mode

### Building Custom Decoders
```python
# Define sync patterns
sync_marks = [[8, 3, 5], [5, 8, 3, 5], [7, 8, 3, 5]]

# Define marks
IDData_mark = 0xA1
ID_mark = 0xFE
Data_mark = 0xFB

# Shift alignment
shift_index = 11
```

### Command Line Escaping
```bash
# Commas become dashes, underscores separate lists
# For [[8, 3, 5], [5, 8, 3, 5]]
--sync_marks=8-3-5_5-8-3-5
```

## Sample Files

### Floppy Samples
- `fdd_fm.sr` - FM 125kbps, 256-byte sectors
- `fdd_mfm.sr` - MFM 250kbps, 256-byte sectors

### MFM Hard Drive Samples
- `hdd_mfm_RQDX3.sr` - DEC RD54 (VAX VMS)
- `hdd_mfm_WD1003V-MM2.sr` - WD controller
- `hdd_mfm_ST21M.sr` - Seagate controller
- `hdd_mfm_OMTI8240.sr` - OMTI controller

### RLL Hard Drive Samples
- `hdd_rll_ACB2370A.sr` - Adaptec RLL
- `hdd_rll_WD1006V-SR2.sr` - WD RLL
- `hdd_rll_OMTI8247.sr` - OMTI RLL
- `hdd_rll_ST21R.sr` - Seagate RLL

## Binary Output

### Available Outputs
| Name | Content |
|------|---------|
| id | Raw ID records (headers) |
| data | Raw data records |
| iddata | Combined ID + data |
| idcrc | ID + CRC (for reverse engineering) |
| datacrc | Data + CRC (for reverse engineering) |

### CRC Reverse Engineering
```bash
# Extract header CRCs for analysis
sigrok-cli -i disk.sr -P mfm -B mfm=idcrc > headers.bin

# Use RevEng to find polynomial
xxd -p -c 8 headers.bin | paste -sd' ' - | \
    xargs reveng -w 16 -F -s
```

## Annotations

### Pulse Level
- `pul` - Normal pulse
- `erp` - Bad pulse (out of tolerance)

### Window Level
- `clk` - Clock window
- `dat` - Data window
- `erw` - Extra pulse in window

### Bit Level
- `bit` - Normal bit
- `erb` - Bad bit (glitched clock, e.g. A1 sync marks)

### Field Level
- `syn` - Sync pattern
- `mrk` - Mark (FE, FB, etc.)
- `rec` - Record
- `crc` - CRC OK
- `cre` - CRC error

## Report Statistics

```
Summary: IAM=0, IDAM=17, DAM=17, DDAM=0, 
         CRC_OK=34, CRC_err=0, EiPW=0, CkEr=0, OoTI=13/74987

IAM   - Index Address Marks
IDAM  - ID Address Marks (headers)
DAM   - Data Address Marks
DDAM  - Deleted Data Address Marks
EiPW  - Extra pulse in window
CkEr  - Clock errors
OoTI  - Out of tolerance intervals
```

## Integration for UFT

### Key Components
1. **PLL with PI filter** - Better than fixed adjustments
2. **RLL decoding** - Critical for many hard drives
3. **Custom mode** - Framework for new formats
4. **Polynomial database** - Known CRC/ECC configs

### PLL Parameters
```python
# Recommended settings
pll_kp = 0.5       # Proportional
pll_ki = 0.0005    # Integral
sync_tol = 0.25    # 25% initial tolerance

# For wobbly drives (e.g., ACB4070)
pll_kp = 1.0       # More aggressive
```

### C Conversion Notes
```c
// Python PI filter -> C implementation
typedef struct {
    double kp;           // Proportional constant
    double ki;           // Integral constant
    double integral;     // Accumulated error
    double period;       // Current expected period
    double tolerance;    // Sync tolerance
} uft_pll_t;

void uft_pll_init(uft_pll_t *pll, double data_rate);
int uft_pll_process(uft_pll_t *pll, double delta, int *bits);
void uft_pll_sync(uft_pll_t *pll, const double *transitions, int count);
```

## References
- David C. Wiens - Original author
- Matt Jenkins (Majenko) - Hard drive support
- Rasz_pl - Modern maintenance
- Thierry Nouspikel - TI floppy controller docs (excellent resource)

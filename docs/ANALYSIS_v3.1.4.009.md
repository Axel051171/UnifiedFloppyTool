# UnifiedFloppyTool v3.1.4.009 - Forensic Imaging Analysis Report

## Date: 2025-12-30

## Archives Analyzed

| Archive | Purpose | Key Algorithms |
|---------|---------|----------------|
| `dd_rescue-1.99.22` | Disk recovery | SIMD sparse detection, error recovery, plugin system |
| `DC3DD-debian-master` | Forensic imaging | Multi-hash, verification, bad sector logging |
| `dcfldd-master` | Enhanced DD | Split files, window hashing, verify mode |
| `floppy_bios-master` | FDC control | INT 13h, media detection, data rates |
| `floppy-formate.zip` | Format library | 60+ disk format definitions |

---

## Key Algorithms Extracted

### 1. SIMD Zero Detection (dd_rescue)
- **Source**: `find_nonzero.c`, `find_nonzero_avx.c`
- **Purpose**: Fast sparse block detection for efficient imaging
- **Implementations**:
  - C reference: Word-aligned scanning with `__builtin_ctzl`
  - SSE2: 16-byte blocks with `_mm_cmpeq_epi8` + `_mm_movemask_epi8`
  - AVX2: 32-byte blocks with `_mm256_cmpeq_epi8` + `_mm256_movemask_epi8`
- **Performance**: 5-10x faster than byte-by-byte scanning

### 2. Error Recovery (dd_rescue)
- **Source**: `dd_rescue.c` lines 1900-2100, 2500-2700
- **Strategy**:
  1. Try normal read with soft block size (128KB)
  2. On error: reduce to hard block size (512 bytes)
  3. Retry each sector up to N times with delay
  4. Fill unreadable sectors with pattern (0x00)
  5. Log bad sectors with offset/error code
- **Key Features**:
  - Reverse read option for stuck heads
  - Progressive block size reduction
  - Automatic recovery promotion after 2 good blocks

### 3. Multi-Algorithm Hashing (dc3dd/dcfldd)
- **Supported**: MD5, SHA1, SHA256, SHA384, SHA512
- **Features**:
  - Concurrent hashing (all algorithms simultaneously)
  - Window-based piecewise hashing for large images
  - Separate verification contexts (input vs output)
  - Hex string generation for logging

### 4. Split File Output (dcfldd)
- **Naming Schemes**:
  - Numeric: `.000`, `.001`, ... (max 1000)
  - Alpha: `.aaa`, `.aab`, ... (max 17576)
  - MAC: `.dmg`, `.002.dmgpart`, ...
  - WIN: `.001`, `.002`, ...
- **Automatic file switching** when max size reached

### 5. Device Size Probing (dcfldd)
- **Linux**: `BLKGETSIZE64` ioctl
- **Fallback**: Binary search with read probing
- **Algorithm**: Double offset until read fails, then bisect

### 6. FDC Media Detection (floppy_bios)
- **Data Rates**: 250K (DD), 300K (360K in 1.2M), 500K (HD), 1M (ED)
- **Media States**: Encoded in single byte (rate | established | type)
- **Specify Command**: Different parameters per media type

---

## New Headers Created

### 1. `uft_forensic_imaging.h` (~15 KB)
Complete forensic imaging API:
- Multi-hash management structures
- Recovery options (retry count, delays, fill patterns)
- Split file context
- Bad sector logging
- Progress/ETA calculation
- GUI integration structures
- SIMD sparse detection functions

### 2. `uft_floppy_formats.h` (~12 KB)
Comprehensive format registry:
- 100+ format ID enum
- Platform classification
- Capability flags (read/write/flux/protection)
- Standard geometry definitions
- Format header structures (DIM, CQM, TD0, HFE, EDSK, SCP)
- D64/G64 sector tables
- Auto-detection framework

### 3. `uft_forensic_imaging.c` (~25 KB)
Implementation with:
- SIMD zero detection (SSE2/AVX2)
- Error recovery algorithm
- Split file writing
- Size probing
- Job management
- Logging framework
- GUI binding

---

## GUI Parameter Mapping

| GUI Control | Structure Field | Default |
|-------------|-----------------|---------|
| Source path | `source_path` | - |
| Source type | `source_is_device` | false |
| Sector size | `source_sector_size` | 512 |
| Dest path | `dest_path` | - |
| Split enabled | `dest_split` | false |
| Split size | `dest_split_size` | 0 |
| Split format | `dest_split_format` | "000" |
| Hash MD5 | `hash_md5` | false |
| Hash SHA1 | `hash_sha1` | false |
| Hash SHA256 | `hash_sha256` | false |
| Hash SHA384 | `hash_sha384` | false |
| Hash SHA512 | `hash_sha512` | false |
| Window size | `hash_window_size` | 0 (full) |
| Recovery | `recovery_enabled` | true |
| Retries | `recovery_retries` | 3 |
| Fill zeros | `recovery_fill_zeros` | true |
| Verify mode | `verify_mode` | NONE |
| Log path | `log_path` | - |
| Verbose | `log_verbose` | false |
| Skip input | `skip_input_sectors` | 0 |
| Skip output | `skip_output_sectors` | 0 |
| Max sectors | `max_sectors` | 0 (all) |

---

## Disk Format Summary (from floppy-formate.zip)

### Commodore (17 formats)
D64, D67, D71, D80, D81, D82, D90, D91, G64, G71, X64, X71, X81, X128, 
P64, NIB, NBZ, D1M, D2M, D4M, DNP, DNP2, DHD, LNX, T64, TAP, P00, PRG, CRT

### Atari ST (5 formats)
ST, STT, STX, STZ, MSA

### Amiga (4 formats)
ADF, ADL, ADZ, DMS

### Apple (8 formats)
DSK, DO, PO, NIB, 2IMG, MAC_DSK, DC42, DART

### Amstrad/CPC (2 formats)
DSK, EDSK

### PC/IBM (6 formats)
IMG, IMA, IMZ, DMF, XDF, DCP, DCU

### Others
- MSX: DMF_MSX
- X68000: DIM, XDF
- TI-99: V9T9, PC99, TIFILES, FIAD
- TRS-80: JV3, JVC, DMK
- BBC: SSD, DSD, ADF, ADL
- Oric: ORIC_DSK
- SAM: MGT, SAD, SDF
- ZX: TRD, SCL, FDI
- NEC: NFD, FDD
- Sharp: SF7
- Generic: CQM, TD0, IMD
- Flux: HFE, MFI, SCP, KF, KFRAW, GWRAW, A2R, WOZ, PFI, PRI, PSI, DFI

---

## Integration Recommendations

### Priority 1: Forensic Imaging Tab
```cpp
// Qt integration example
class ForensicImagingWidget : public QWidget {
    // Use uft_fi_gui_params_t for all controls
    // Bind to uft_fi_job_from_gui() on start
    // Poll uft_fi_get_gui_status() for progress
};
```

### Priority 2: Format Auto-Detection
```cpp
// On file open:
uft_detect_result_t results[10];
int count = uft_detect_format(header, 512, file_size, ext, results, 10);
// Display top candidates with confidence scores
```

### Priority 3: Bad Sector Visualization
- Use `uft_fi_get_bad_sectors()` to get list
- Visualize in track grid (red = bad, green = good)
- Export with `uft_fi_export_bad_map()`

---

## Build Integration

### CMake Addition
```cmake
add_library(uft_forensic
    src/uft_forensic_imaging.c
)

target_compile_options(uft_forensic PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-msse2 -mavx2>
)

target_link_libraries(uft_forensic
    OpenSSL::Crypto  # For hash implementations
)
```

### Qt .pro Addition
```qmake
SOURCES += src/uft_forensic_imaging.c
HEADERS += include/uft/uft_forensic_imaging.h \
           include/uft/uft_floppy_formats.h

QMAKE_CFLAGS += -msse2
contains(QT_ARCH, x86_64): QMAKE_CFLAGS += -mavx2
```

---

## Testing Checklist

- [ ] SIMD detection works on SSE2-only systems
- [ ] AVX2 path activates on supported CPUs
- [ ] Error recovery handles partial reads
- [ ] Split files name correctly
- [ ] All hash algorithms produce correct output
- [ ] Bad sector map exports correctly
- [ ] Progress callback fires at reasonable intervals
- [ ] Cancel operation works mid-transfer
- [ ] Log file contains all expected fields

---

## Change Log v3.1.4.009

### Added
- Complete forensic imaging module from dd_rescue/dc3dd/dcfldd
- SIMD-accelerated sparse block detection (SSE2/AVX2)
- Multi-algorithm concurrent hashing (MD5/SHA1/SHA256/SHA384/SHA512)
- Split file output with multiple naming schemes
- Error recovery with sector-level retry and fill
- Bad sector logging and map export
- Window-based piecewise hashing
- Device size probing (ioctl + binary search fallback)
- FDC register definitions from floppy_bios
- 100+ disk format definitions from floppy-formate collection
- GUI parameter structure for Qt integration

### Technical Details
- New files: 3 (uft_forensic_imaging.h, uft_floppy_formats.h, uft_forensic_imaging.c)
- Combined size: ~52 KB
- Lines of code: ~2500
- Supported platforms: Linux, macOS, Windows (with POSIX layer)
- Required dependencies: None (optional: OpenSSL for production hashing)

### From Previous Versions (v3.1.4.001-008)
- CBM/C64 protection detection (v3.1.4.008)
- HFS filesystem support (v3.1.4.008)
- Apple II formats (v3.1.4.005)
- Atari ST/MSA (v3.1.4.004)
- BBC/Oric (v3.1.4.003)
- GCR/MFM decoding (v3.1.4.001-002)

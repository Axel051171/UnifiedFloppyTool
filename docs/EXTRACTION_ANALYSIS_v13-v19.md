# UFT Extraction Analysis v13-v19
## Comprehensive Source Code Review

> **Datum:** 2026-01-04  
> **Quell-Pakete:** uft_extract_v13.zip - v19.zip  
> **Gesamt-Codezeilen:** 27,617  
> **Analyse-Modus:** Deep Audit + Integration Assessment

---

## EXECUTIVE SUMMARY

| Version | Fokus | Zeilen | Integrierbar | Status |
|---------|-------|--------|--------------|--------|
| v13 | Disk Recovery, MFM PLL, BBC DFS | ~5,200 | ✅ 90% | NEU |
| v14 | Filesystem Detection, MBR/Partition | ~2,800 | ✅ 85% | NEU |
| v15 | Atari 8-bit Disk Access | ~1,200 | 🟡 60% | TEILWEISE VORHANDEN |
| v16 | Atari DOS/ATR/ATX, HFS, exFAT | ~4,500 | ✅ 80% | NEU |
| v17 | BBC Tape/Disc Formats | ~2,600 | 🟡 50% | TEILWEISE VORHANDEN |
| v18 | Floppy Geometry, Encoding | ~1,800 | 🔴 20% | BEREITS IMPLEMENTIERT |
| v19 | IMD/TD0/DMK/FDC Formats | ~4,200 | ✅ 75% | TEILWEISE NEU |

**Gesamt-Nutzen:** ~15,000 LOC direkt integrierbar, ~8h Entwicklungszeit gespart

---

## CAPABILITY MATRIX

### v13: Disk Recovery & MFM PLL

| Feature | Datei | LOC | Integration | Priorität |
|---------|-------|-----|-------------|-----------|
| Multi-Stage Recovery | `uft_disk_recovery.h` | 396 | ✅ DIREKT | 🔴 HOCH |
| MFM Software PLL | `uft_mfm_pll.h` | 277 | ✅ DIREKT | 🔴 HOCH |
| Forensic Hashing (SHA256) | `uft_forensic_hash.h` | 248 | ✅ DIREKT | 🟡 MITTEL |
| BBC DFS Catalog | `uft_bbc_dfs.h` | 249 | 🟡 MERGE | 🟢 NIEDRIG |
| Endian Utilities | `uft_endian.h` | 252 | ✅ DIREKT | 🟡 MITTEL |
| safecopy Algorithm | `safecopy.c` | 1777 | 📋 REFERENZ | 🟡 MITTEL |
| recoverdm Algorithm | `recoverdm.c` | 496 | 📋 REFERENZ | 🟡 MITTEL |

**Schlüssel-Algorithmen v13:**
```c
// 3-Stage Recovery (safecopy)
uft_recovery_preset_stage1(&cfg);  // Fast scan, skip bad
uft_recovery_preset_stage2(&cfg);  // Find exact boundaries
uft_recovery_preset_stage3(&cfg);  // Maximum retry effort

// MFM PLL Parameters (Glasgow)
#define UFT_PLL_DEFAULT_FRAC_BITS    8
#define UFT_PLL_DEFAULT_KP_EXP       2   // Proportional gain
#define UFT_PLL_DEFAULT_GPH_EXP      1   // Phase gain
```

---

### v14: Filesystem Detection

| Feature | Datei | LOC | Integration | Priorität |
|---------|-------|-----|-------------|-----------|
| FS Magic Detection | `uft_fs_detect.h` | 371 | ✅ DIREKT | 🔴 HOCH |
| MBR/Partition Table | `uft_mbr_partition.h` | 396 | ✅ DIREKT | 🟡 MITTEL |
| FAT12/16/32 Probing | `libblkid_vfat.c` | 431 | 📋 REFERENZ | 🟡 MITTEL |
| Minix Detection | `libblkid_minix.c` | 157 | ✅ DIREKT | 🟢 NIEDRIG |
| HFS Detection | `libblkid_hfs.c` | 216 | ✅ DIREKT | 🟢 NIEDRIG |
| ISO9660 Detection | `libblkid_iso9660.c` | 142 | ✅ DIREKT | 🟢 NIEDRIG |
| exFAT Detection | `libblkid_exfat.c` | 98 | ✅ DIREKT | 🟢 NIEDRIG |

**Schlüssel-Strukturen v14:**
```c
typedef enum {
    UFT_FS_FAT12, UFT_FS_FAT16, UFT_FS_FAT32, UFT_FS_EXFAT,
    UFT_FS_MINIX1, UFT_FS_MINIX2, UFT_FS_MINIX3,
    UFT_FS_AMIGA_OFS, UFT_FS_AMIGA_FFS,
    UFT_FS_HFS, UFT_FS_ADFS,
    // ... 30+ Typen
} uft_fs_type_t;

// FAT Media Descriptors (Floppy-spezifisch)
#define UFT_FAT_MEDIA_FLOPPY    0xF0  // 3.5" floppy
#define UFT_FAT_MEDIA_F9        0xF9  // 720KB or 1.2MB
#define UFT_FAT_MEDIA_FD        0xFD  // 360KB
```

---

### v15: Atari 8-bit Disk Access

| Feature | Datei | LOC | Integration | Priorität |
|---------|-------|-----|-------------|-----------|
| Atari Disk Geometry | `uft_atari8_disk.h` | 431 | 🟡 MERGE | 🟡 MITTEL |
| Low-Level Sector Access | `uft_atari8_disk.c` | 380 | 🟡 MERGE | 🟡 MITTEL |

**Status:** Teilweise in `uft_atari_image.h` bereits vorhanden.

---

### v16: Atari DOS & HFS/exFAT

| Feature | Datei | LOC | Integration | Priorität |
|---------|-------|-----|-------------|-----------|
| ATR/ATX/DCM Format | `uft_atari_image.h` | 387 | ✅ DIREKT | 🔴 HOCH |
| Atari DOS 2.x/MyDOS | `uft_atari_dos.h` | 374 | ✅ DIREKT | 🔴 HOCH |
| SpartaDOS Filesystem | `mkatr_spartafs.c/h` | 580 | ✅ DIREKT | 🟡 MITTEL |
| LiteDOS Filesystem | `mkatr_lsdos.c/h` | 520 | ✅ DIREKT | 🟡 MITTEL |
| SFS Filesystem | `mkatr_lssfs.c/h` | 410 | ✅ DIREKT | 🟢 NIEDRIG |
| ATX Copy Protection | `vapi_at8prot.h` | 156 | ✅ DIREKT | 🔴 HOCH |
| Apple HFS Format | `apple_hfs_format.h` | 805 | 🟡 MERGE | 🟡 MITTEL |
| exFAT On-Disk | `exfat_ondisk.h` | 278 | ✅ DIREKT | 🟡 MITTEL |
| ATR↔IMD Converter | `ataritools_*.c` | 2700 | 📋 REFERENZ | 🟢 NIEDRIG |

**ATX Copy Protection Features:**
```c
typedef enum {
    UFT_ATX_CU_DATA     = 0x00,  // Sector data
    UFT_ATX_CU_HDRLST   = 0x01,  // Header list
    UFT_ATX_CU_WK7      = 0x10,  // FX7-style weak bits
    UFT_ATX_CU_EXTHDR   = 0x11   // Extended sector header
} uft_atx_chunk_t;
```

---

### v17: BBC Tape/Disc

| Feature | Datei | LOC | Integration | Priorität |
|---------|-------|-----|-------------|-----------|
| BBC DFS Extended | `uft_bbc_dfs.h` | 405 | 🟡 MERGE | 🟢 NIEDRIG |
| BBC Tape Format | `uft_bbc_tape.h` | 180 | ✅ DIREKT | 🟢 NIEDRIG |
| bbctapedisc Main | `bbctapedisc_*.cpp` | 1800 | 📋 REFERENZ | 🟢 NIEDRIG |

**Status:** BBC DFS bereits vollständig in UFT implementiert.

---

### v18: Floppy Geometry & Encoding

| Feature | Datei | LOC | Integration | Priorität |
|---------|-------|-----|-------------|-----------|
| Floppy Geometry DB | `uft_floppy_geometry.h` | 536 | ⚠️ DUPLIKAT | - |
| Encoding Types | `uft_floppy_encoding.h` | 440 | ⚠️ DUPLIKAT | - |
| Format Registry | `uft_floppy_formats.c` | 414 | ⚠️ DUPLIKAT | - |

**Status:** Bereits vollständig in UFT implementiert. Keine Integration nötig.

---

### v19: IMD/TD0/DMK/FDC Formats

| Feature | Datei | LOC | Integration | Priorität |
|---------|-------|-----|-------------|-----------|
| **TD0 LZSS Decoder** | `uft_td0.c` | 631 | ✅ DIREKT | 🔴 HOCH |
| DMK Raw Track | `uft_dmk.c` | 515 | 🟡 MERGE | 🟡 MITTEL |
| IMD ImageDisk | `uft_imd.c` | 495 | 🟡 MERGE | 🟡 MITTEL |
| FDI Format | `uft_fdi.c` | 500 | ✅ DIREKT | 🟡 MITTEL |
| FDC Gap Tables | `uft_fdc.h` | 365 | ✅ DIREKT | 🟡 MITTEL |
| FAT12 Helpers | `uft_fat12.h` | 494 | 🟡 MERGE | 🟢 NIEDRIG |
| Altair HD | `uft_altair_hd.c` | 280 | ✅ DIREKT | 🟢 NIEDRIG |
| Test Suites | `test_*.c` | 550 | ✅ TESTS | 🟡 MITTEL |

**TD0 LZSS-Huffman Implementation (Kritisch!):**
```c
// Vollständige LZSS-Huffman Decompression für Teledisk
void uft_td0_lzss_init(uft_td0_lzss_state_t* state,
                       const uint8_t* data, size_t size);
int uft_td0_lzss_getbyte(uft_td0_lzss_state_t* state);

// Sector Data Decoding (RLE + Raw)
int uft_td0_decode_sector(const uint8_t* src, size_t src_size,
                          uint8_t* dst, size_t dst_size,
                          uint8_t method);
```

---

## PRIORISIERTE TODO-LISTE

### 🔴 MUST (Sofort integrieren)

| # | Task | Quelle | Aufwand | Dateien |
|---|------|--------|---------|---------|
| EXT-001 | Multi-Stage Recovery API | v13 | 4h | `uft_disk_recovery.h` |
| EXT-002 | TD0 LZSS-Huffman Decoder | v19 | 3h | `uft_td0.c` (Merge) |
| EXT-003 | Filesystem Auto-Detection | v14 | 3h | `uft_fs_detect.h` |
| EXT-004 | ATX Copy Protection | v16 | 2h | `uft_atari_image.h` |
| EXT-005 | MFM Software PLL | v13 | 3h | `uft_mfm_pll.h` |

**Gesamt MUST:** ~15h

### 🟡 SHOULD (Diese Woche)

| # | Task | Quelle | Aufwand | Dateien |
|---|------|--------|---------|---------|
| EXT-006 | MBR/Partition Parsing | v14 | 2h | `uft_mbr_partition.h` |
| EXT-007 | Forensic Hashing | v13 | 2h | `uft_forensic_hash.h` |
| EXT-008 | SpartaDOS Filesystem | v16 | 3h | `mkatr_spartafs.c/h` |
| EXT-009 | FDC Gap Tables | v19 | 1h | `uft_fdc.h` |
| EXT-010 | Sector Voting (Recovery) | v13 | 2h | In `uft_disk_recovery.h` |
| EXT-011 | Bad Block List Management | v13 | 1h | In `uft_disk_recovery.h` |

**Gesamt SHOULD:** ~11h

### 🟢 COULD (Nice-to-have)

| # | Task | Quelle | Aufwand |
|---|------|--------|---------|
| EXT-012 | LiteDOS/SFS Filesystem | v16 | 4h |
| EXT-013 | BBC Tape Support | v17 | 3h |
| EXT-014 | Altair HD Floppy | v19 | 2h |
| EXT-015 | ATR↔IMD Converter | v16 | 4h |
| EXT-016 | HFS Extended | v16 | 3h |

---

## KONKRETE INTEGRATIONSPUNKTE

### EXT-001: Multi-Stage Recovery API

**Ziel-Dateien:**
- `include/uft/recovery/uft_disk_recovery.h` (NEU)
- `src/recovery/uft_disk_recovery.c` (NEU)

**Integration:**
```c
// Bereits in UFT: uft_recovery.h (Basis-API)
// NEU hinzufügen: 3-Stage Presets + Bad Block Management

// Copy from v13:
typedef struct {
    uft_device_type_t device_type;
    size_t block_size;
    uint8_t max_retries;
    uint8_t head_moves;
    uint32_t skip_blocks;
    // ...
} uft_recovery_config_t;

static inline void uft_recovery_preset_stage1(uft_recovery_config_t *cfg);
static inline void uft_recovery_preset_stage2(uft_recovery_config_t *cfg);
static inline void uft_recovery_preset_stage3(uft_recovery_config_t *cfg);
```

---

### EXT-002: TD0 LZSS-Huffman Decoder

**Ziel-Dateien:**
- `src/formats/td0/uft_td0_lzss.c` (NEU)
- Update: `include/uft/formats/uft_td0.h`

**Integration:**
```c
// Existiert: uft_td0.h Header
// NEU: Vollständige LZSS-Huffman State Machine

typedef struct {
    const uint8_t *input;
    size_t input_size, input_pos;
    bool eof;
    
    uint16_t freq[UFT_TD0_LZSS_TSIZE + 1];
    uint16_t parent[UFT_TD0_LZSS_TSIZE * 2];
    uint16_t son[UFT_TD0_LZSS_TSIZE];
    
    uint8_t ring_buff[UFT_TD0_LZSS_SBSIZE];
    uint16_t r;
    // ...
} uft_td0_lzss_state_t;
```

---

### EXT-003: Filesystem Auto-Detection

**Ziel-Dateien:**
- `include/uft/uft_fs_detect.h` (NEU)
- `src/detect/uft_fs_detect.c` (NEU)

**Integration:**
```c
// Merge mit existierendem uft_format_detect.h

uft_fs_type_t uft_fs_detect(const uint8_t *data, size_t len);
uft_fs_type_t uft_fat_detect(const uint8_t *data);
bool uft_fat_validate(const uint8_t *data);
const char *uft_fs_type_name(uft_fs_type_t type);
```

---

## ALGORITHMEN & HEURISTIKEN

### Recovery-Algorithmus (v13 safecopy)

```
Stage 1 (Fast Rescue):
├── Block-Size: 10% of device
├── Retries: 1
├── Skip: 64 blocks on error
└── Mark bad areas with "BaDbLoCk"

Stage 2 (Boundary Detection):
├── Block-Size: 1 sector
├── Retries: 1
├── Skip: 1 block on error
└── Use Stage 1 bad block list

Stage 3 (Maximum Effort):
├── Block-Size: 1 sector
├── Retries: 4-8
├── Head realignment: 1-2 moves
└── Retry all remaining bad blocks
```

### TD0 Compression (v19 Teledisk)

```
TD0 Format Detection:
├── "TD" = Normal (uncompressed)
└── "td" = Advanced (LZSS-Huffman)

LZSS-Huffman Pipeline:
├── Input: Compressed bytestream
├── Huffman tree: Adaptive, 314 symbols
├── Ring buffer: 4KB sliding window
├── Output: Decompressed data
└── Sector RLE: Additional per-sector compression
```

### Filesystem Detection Priority (v14)

```
Detection Order:
1. FAT signature (55 AA at 510) → FAT12/16/32
2. Minix magic at 1024+16 → Minix1/2/3
3. Ext magic at 1024+56 → ext2/3/4
4. NTFS at offset 3 → NTFS
5. ISO9660 at 0x8001 → CD-ROM
6. Amiga "DOS" at 0 → OFS/FFS
7. HFS/HFS+ at 1024 → Apple
8. ADFS at specific offsets → Acorn
```

---

## GUI-RELEVANTE PARAMETER

### Recovery Workflow UI

```
┌─────────────────────────────────────┐
│ Recovery Mode                       │
├─────────────────────────────────────┤
│ ○ Stage 1: Fast Scan                │
│   └── Skip bad areas quickly        │
│ ○ Stage 2: Find Boundaries          │
│   └── Locate exact bad sectors      │
│ ● Stage 3: Maximum Effort           │
│   └── Retry with head moves         │
├─────────────────────────────────────┤
│ Max Retries: [4  ] ▼                │
│ Head Moves:  [1  ] ▼                │
│ Block Size:  [512] ▼                │
└─────────────────────────────────────┘
```

### Filesystem Detection UI

```
┌─────────────────────────────────────┐
│ Detected Filesystem                 │
├─────────────────────────────────────┤
│ Type:    FAT12 (MS-DOS)             │
│ Media:   0xF9 (720KB Floppy)        │
│ Cluster: 2 sectors                  │
│ FATs:    2                          │
│ Root:    224 entries                │
└─────────────────────────────────────┘
```

---

## NICHT-INTEGRIERBARER CODE

| Datei | Grund | Alternative |
|-------|-------|-------------|
| `sha256.cpp` | C++, externe Lib verfügbar | OpenSSL / libtomcrypt |
| `bbctapedisc_*.cpp` | C++, komplexe Deps | Referenz für uft_bbc_tape.h |
| `safecopy.c` | GPL, monolithisch | Algorithmus reimplementieren |
| `recoverdm.c` | GPL, Linux-spezifisch | Algorithmus reimplementieren |

---

## ROADMAP NACH INTEGRATION

### Woche 1
- [ ] EXT-001: Multi-Stage Recovery
- [ ] EXT-002: TD0 LZSS Decoder
- [ ] EXT-003: FS Auto-Detection

### Woche 2
- [ ] EXT-004: ATX Copy Protection
- [ ] EXT-005: MFM Software PLL
- [ ] EXT-006: MBR Parsing

### Woche 3
- [ ] EXT-007: Forensic Hashing
- [ ] EXT-008: SpartaDOS
- [ ] EXT-009: FDC Gap Tables

---

## CHANGELOG

| Datum | Änderung |
|-------|----------|
| 2026-01-04 | Initial Extraction Analysis v13-v19 |
| 2026-01-04 | 27,617 LOC analysiert, 16 Tasks identifiziert |
| 2026-01-04 | Prioritäten: 5 MUST, 6 SHOULD, 5 COULD |

---

*„Bei uns geht kein Bit verloren" - Auch nicht in externem Code*

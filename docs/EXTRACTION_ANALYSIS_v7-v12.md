# UFT Extraction Analysis v7-v12

> **Datum:** 2026-01-04  
> **Pakete:** uft_copy_protection_v7.zip, uft_advanced_flux_v8.zip, uft_extract_v9-v12.zip  
> **Gesamt analysiert:** ~25,000 LOC  
> **Integriert:** 12 neue Module (~5,500 LOC)

---

## 📊 Capability Matrix

| Version | Fokus | LOC | Integriert | Status |
|---------|-------|-----|------------|--------|
| **v7** | Copy Protection | ~650 | 0 | ✅ Bereits vorhanden |
| **v8** | PI-PLL, CRC | ~1,100 | 838 | ✅ PI-PLL integriert |
| **v9** | KryoFlux, CRC, Altair | ~3,500 | 732 | ✅ Altair CPM, Floppy Formats |
| **v10** | CPM, GCR, HFE | ~3,200 | 0 | ⚠️ Duplikate - bereits vorhanden |
| **v11** | Modern FS, Carving | ~19,000 | 1,519 | ✅ APFS, BTRFS, FATX, SBX, Signatures |
| **v12** | MFM Flux, FAT, ADFS | ~3,500 | 1,374 | ✅ MFM Flux, Clustering, ADFS, FAT |

---

## 🆕 Neue Module (integriert)

### Flux Processing

| Modul | Datei | LOC | Quelle | Feature |
|-------|-------|-----|--------|---------|
| **PI-PLL** | `flux/pll/uft_pll_pi.h` | 338 | v8 | Proportional-Integral Clock Recovery |
| **PI-PLL Impl** | `flux/pll/uft_pll_pi.c` | 500 | v8 | FM/MFM/RLL-2,7/RLL-1,7 Support |
| **MFM Flux** | `flux/uft_mfm_flux.h` | 352 | v12 | IBM Address Marks, CRC-16 |
| **Flux Cluster** | `flux/uft_flux_cluster.h` | 320 | v12 | 1T/2T/3T Band Detection |

### Modern Filesystems

| Modul | Datei | LOC | Quelle | Feature |
|-------|-------|-----|--------|---------|
| **APFS** | `fs/modern/uft_apfs.h` | 375 | v11 | Apple FS Container/Volume |
| **BTRFS** | `fs/modern/uft_btrfs.h` | 324 | v11 | Linux B-Tree FS |
| **FATX** | `fs/modern/uft_fatx.h` | 285 | v11 | Xbox Original/360 |
| **SBX** | `fs/modern/uft_sbx.h` | 313 | v11 | SeqBox Recovery Containers |

### Retro Filesystems

| Modul | Datei | LOC | Quelle | Feature |
|-------|-------|-----|--------|---------|
| **Acorn ADFS** | `formats/acorn/uft_acorn_adfs.h` | 288 | v12 | Archimedes D/E/F/L |
| **Altair CPM** | `formats/uft_altair_cpm.h` | 383 | v9 | Altair 8800 CP/M |
| **FAT Floppy** | `fs/uft_fat_floppy.h` | 414 | v12 | FAT12/16 Floppy-spezifisch |

### Data Recovery

| Modul | Datei | LOC | Quelle | Feature |
|-------|-------|-----|--------|---------|
| **File Signatures** | `carving/uft_file_signatures.h` | 222 | v11 | 23+ Dateityp-Signaturen |
| **Floppy Formats** | `uft_floppy_formats.h` | 349 | v9 | Linux Floppy Geometrie |

---

## 🔑 Schlüssel-Algorithmen

### PI Loop Filter PLL (v8)
```c
typedef struct {
    double kp;              // Proportional constant (0.5)
    double ki;              // Integral constant (0.0005)
    double sync_tolerance;  // 0.0-1.0 (0.25 default)
    uft_encoding_t encoding; // FM/MFM/RLL
} uft_pll_config_t;

int uft_pll_process_transition(uft_pll_t *pll, double delta_ns,
                               uft_pll_bit_t *bits, int *bit_count);
```

### MFM Address Mark Detection (v12)
```c
typedef enum {
    UFT_AM_IAM  = 1,   // C2C2C2 FC - Index
    UFT_AM_IDAM = 2,   // A1A1A1 FE - Sector Header
    UFT_AM_DAM  = 3,   // A1A1A1 FB - Sector Data
    UFT_AM_DDAM = 4    // A1A1A1 F8 - Deleted Data
} uft_mfm_am_type_t;

size_t uft_mfm_find_preambles(const uft_mfm_train_t *train,
                              size_t *positions, size_t max);
```

### APFS Fletcher-64 Checksum (v11)
```c
static inline uint64_t uft_apfs_fletcher64(const uint32_t *block, 
                                            size_t block_size)
{
    // APFS variant: Skip first 2 words (checksum location)
    // c1 = -(simple_sum + second_sum) mod modulus
}
```

### File Carving (v11)
```c
static const uft_file_sig_t uft_file_signatures[] = {
    { "jpg",  "JPEG Image", {0xFF,0xD8,0xFF}, 3, {0xFF,0xD9}, 2, 20MB },
    { "png",  "PNG Image",  {0x89,0x50,0x4E,0x47...}, 8, ... },
    { "pdf",  "PDF Document", {0x25,0x50,0x44,0x46...}, 5, ... },
    // 20+ weitere...
};
```

---

## ⚠️ Duplikate (nicht integriert)

| Modul | Grund | Aktion |
|-------|-------|--------|
| v7 `uft_fuzzy_bits.h` | Identisch mit bestehendem | Übersprungen |
| v10 `uft_cpmfs.h` | Bereits vorhanden (380 LOC) | Vergleichen |
| v10 `uft_gcr.h` | Bereits vorhanden (313 LOC) | Vergleichen |
| v10 `uft_hfe.h` | Bereits vorhanden (281+383 LOC) | Vergleichen |
| v9 `uft_kryoflux.h` | Bereits vorhanden (649 LOC) | Merge-Kandidat |
| v8 `uft_crc_polys.h` | Bereits vorhanden | Vergleichen |

---

## 📋 Offene Tasks (TODO)

### EXT2-COULD (Nice-to-have)

| Task | Beschreibung | Priorität |
|------|--------------|-----------|
| EXT2-013 | CRC Polynomials Extended (v8) - Vergleich | Low |
| EXT2-014 | KryoFlux Enhanced Parser (v9) - Merge | Medium |
| EXT2-015 | CRC RevEng Integration (v9) | Low |
| EXT2-016 | AES-XTS Source Validation | Medium |
| EXT2-017 | CPM Filesystem Enhanced (v10) | Low |

---

## 📁 Verzeichnisstruktur (neu)

```
include/uft/
├── flux/
│   ├── pll/
│   │   └── uft_pll_pi.h          # NEW: PI Loop Filter
│   ├── uft_mfm_flux.h            # NEW: MFM Analysis
│   └── uft_flux_cluster.h        # NEW: Band Clustering
├── fs/
│   ├── modern/
│   │   ├── uft_apfs.h            # NEW: Apple APFS
│   │   ├── uft_btrfs.h           # NEW: Linux BTRFS
│   │   ├── uft_fatx.h            # NEW: Xbox FATX
│   │   └── uft_sbx.h             # NEW: SeqBox
│   └── uft_fat_floppy.h          # NEW: FAT Floppy
├── formats/
│   ├── acorn/
│   │   └── uft_acorn_adfs.h      # NEW: Archimedes
│   └── uft_altair_cpm.h          # NEW: Altair CPM
├── carving/
│   └── uft_file_signatures.h     # NEW: File Carving
└── uft_floppy_formats.h          # NEW: Linux Floppy

src/
├── flux/pll/
│   └── uft_pll_pi.c              # NEW: PI-PLL Implementation
├── fs/modern/
│   └── uft_apfs_cksum.c          # NEW: APFS Checksum
├── crypto/
│   └── uft_aes_xts.c             # NEW: AES-XTS
└── core/
    └── uft_crc16_ccitt.c         # NEW: IBM Floppy CRC
```

---

## ✅ Integration Summary

- **12 neue Header-Module** integriert
- **4 neue Source-Implementierungen**
- **~5,500 LOC** neuer Code
- **6 Pakete** analysiert (v7-v12)
- **~25,000 LOC** reviewed
- **5 Duplikate** identifiziert (später vergleichen)

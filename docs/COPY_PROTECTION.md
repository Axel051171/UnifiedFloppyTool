# Copy Protection Handling in UFT

**Version:** 1.0  
**Status:** INFORMATIV

---

## 1. Übersicht

Copy Protection auf Floppy-Disketten nutzt Eigenschaften des physischen Mediums,
die von normalen Lese-Routinen nicht erfasst werden:

- **Weak Bits:** Unzuverlässig lesbare Bits (lesen bei jedem Versuch anders)
- **Timing-Variationen:** Präzise Abstände zwischen Bits
- **Dichte-Variationen:** Variable Bitrate auf einem Track
- **Nicht-Standard-Sektoren:** Fehlende Sync-Marks, ungültige CRCs
- **Überlange Tracks:** Mehr Daten als normal möglich

---

## 2. Auswirkungen auf Formate

### 2.1 Welche Formate können Kopierschutz speichern?

| Format | Weak Bits | Timing | Dichte | Non-Standard | Status |
|--------|-----------|--------|--------|--------------|--------|
| SCP | ✅ | ✅ | ✅ | ✅ | Vollständig |
| KryoFlux | ✅ | ✅ | ✅ | ✅ | Vollständig |
| IPF | ✅ | ✅ | ✅ | ✅ | Vollständig |
| G64 | ❌ | ❌ | ⚠️ | ✅ | Teilweise |
| D64 | ❌ | ❌ | ❌ | ❌ | Keine |
| ADF | ❌ | ❌ | ❌ | ❌ | Keine |
| IMG | ❌ | ❌ | ❌ | ❌ | Keine |

### 2.2 Konvertierungs-Matrix

| Von → Nach | Verlust | Warnung erforderlich |
|------------|---------|---------------------|
| SCP → D64 | Ja | ⚠️ "Copy protection will be lost" |
| SCP → G64 | Teilweise | ⚠️ "Weak bits lost, timing lost" |
| G64 → D64 | Ja | ⚠️ "Non-standard sectors ignored" |
| D64 → SCP | N/A | ❌ Nicht möglich (Info fehlt) |

---

## 3. Detection API

```c
/**
 * @brief Detected copy protection types
 */
typedef enum {
    UFT_PROT_NONE = 0,          /**< No protection detected */
    UFT_PROT_UNKNOWN = 1,       /**< Something unusual, type unknown */
    
    // Specific types
    UFT_PROT_WEAK_BITS = 0x10,  /**< Weak/fuzzy bits */
    UFT_PROT_TIMING = 0x20,     /**< Timing-based protection */
    UFT_PROT_DENSITY = 0x40,    /**< Variable density */
    UFT_PROT_SIGNATURE = 0x80,  /**< Sector signatures */
    UFT_PROT_LONG_TRACK = 0x100,/**< Overlength track */
    
    // Known schemes
    UFT_PROT_VORPAL = 0x1000,   /**< Vorpal (C64) */
    UFT_PROT_V_MAX = 0x2000,    /**< V-Max (C64) */
    UFT_PROT_FAT_TRACK = 0x4000,/**< Fat Track */
    UFT_PROT_COPYLOCK = 0x8000, /**< Copylock (Amiga) */
} uft_protection_type_t;

/**
 * @brief Protection detection result
 */
typedef struct {
    uint32_t types;             /**< Bitmask of UFT_PROT_* */
    int confidence;             /**< 0-100 */
    int track_hints[168];       /**< Per-track protection hints */
    char description[256];      /**< Human-readable description */
} uft_protection_info_t;

/**
 * @brief Detect copy protection in disk
 * @param disk Open disk handle
 * @param info [OUT] Protection information
 * @return UFT_OK on success
 */
uft_error_t uft_detect_protection(const uft_disk_t* disk,
                                   uft_protection_info_t* info);

/**
 * @brief Check if conversion will lose protection
 * @param src_format Source format
 * @param dst_format Destination format
 * @param info Protection info from source
 * @return true if protection will be lost
 */
bool uft_conversion_loses_protection(int src_format, int dst_format,
                                      const uft_protection_info_t* info);
```

---

## 4. GUI-Hinweise

### 4.1 Beim Öffnen einer geschützten Disk

```
┌─────────────────────────────────────────────────────────────┐
│  ⚠️  Copy Protection Detected                               │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  This disk appears to use copy protection:                  │
│  • Weak bits on tracks 18-20                                │
│  • Timing variations detected                               │
│                                                             │
│  Protection type: V-Max (90% confidence)                    │
│                                                             │
│  Note: Converting to D64 will lose protection.              │
│  Use SCP or G64 format to preserve protection features.     │
│                                                             │
│  [View Details]           [Continue]           [Cancel]     │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Bei Konvertierung mit Datenverlust

```
┌─────────────────────────────────────────────────────────────┐
│  ⚠️  Data Loss Warning                                       │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Converting SCP → D64 will lose:                            │
│                                                             │
│  ❌ Weak bits (tracks 18-20)                                │
│  ❌ Timing information                                       │
│  ❌ Non-standard sectors (track 35)                         │
│                                                             │
│  The resulting disk image may not work correctly            │
│  with copy-protected software.                              │
│                                                             │
│  Recommendation: Keep the original SCP file.                │
│                                                             │
│  [ ] Don't show this warning again                          │
│                                                             │
│  [Convert Anyway]                          [Cancel]         │
└─────────────────────────────────────────────────────────────┘
```

---

## 5. Bekannte Kopierschutz-Systeme

### 5.1 Commodore 64

| System | Erkennung | Preservation |
|--------|-----------|--------------|
| Vorpal | Sync-Marker-Analyse | SCP/G64 |
| V-Max | Timing-Analyse | SCP only |
| RapidLok | Sector-Header | SCP/G64 |
| Fat Track | Track-Länge | SCP only |

### 5.2 Amiga

| System | Erkennung | Preservation |
|--------|-----------|--------------|
| Copylock | Weak Bits | SCP/IPF |
| Rob Northen | Timing | SCP/IPF |
| FBI | Sector CRC | SCP/IPF |

### 5.3 Atari ST

| System | Erkennung | Preservation |
|--------|-----------|--------------|
| Fuzzy Bits | Weak Sectors | STX/SCP |
| Long Tracks | Track Size | STX/SCP |

---

## 6. Best Practices

### 6.1 Für Preservation

1. **Immer Flux-Level speichern** (SCP, KryoFlux, IPF)
2. **Mehrere Reads machen** (für Weak-Bit-Erkennung)
3. **Original aufbewahren** (nie nur konvertierte Version)
4. **Dokumentieren** (welcher Schutz, welche Methode)

### 6.2 Für Entwickler

1. **Warnung bei Datenverlust** (nie silent)
2. **Original nicht überschreiben** (immer neue Datei)
3. **Audit-Trail** (was wurde geändert/verloren)

---

**DOKUMENT ENDE**

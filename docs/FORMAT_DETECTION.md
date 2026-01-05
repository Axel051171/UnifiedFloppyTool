# UFT Format Detection & Validation System

## Probe Pipeline mit Confidence-Scoring

### Erkennungsstufen

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        PROBE PIPELINE                                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐  │
│  │   MAGIC     │    │    SIZE     │    │  STRUCTURE  │    │  HEURISTIC  │  │
│  │  +40 max    │ →  │  +20 max    │ →  │  +30 max    │ →  │  +10 max    │  │
│  └─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘  │
│                                                                             │
│  Confidence = Σ(stage scores), max 100                                      │
│                                                                             │
│  95-100: DEFINITE  (Magic + Structure valid)                               │
│  80-94:  HIGH      (Magic OR Size+Structure)                               │
│  60-79:  MEDIUM    (Heuristic match)                                       │
│  40-59:  LOW       (Plausible)                                             │
│  0-39:   UNLIKELY  (No match)                                              │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Erkennungsstrategie pro Format

| Format   | Magic | Size | Structure | Heuristic | Confidence |
|----------|-------|------|-----------|-----------|------------|
| SCP      | ✓ "SCP" | - | Header validation | - | 95+ |
| HFE      | ✓ "HXCPICFE" | - | Version, tracks | - | 95+ |
| IPF      | ✓ "CAPS" | - | Encoder type | - | 95+ |
| STX      | ✓ "RSY\0" | - | Track headers | - | 95+ |
| G64      | ✓ "GCR-1541" | - | Track table | - | 95+ |
| WOZ      | ✓ "WOZ1/2" | - | Chunk parsing | - | 95+ |
| D64      | - | ✓ 174848+ | BAM, Directory | PETSCII | 70-85 |
| ADF      | ~ "DOS" | ✓ 901120 | Bootblock checksum | Root block | 75-90 |
| IMG      | ~ 0x55AA | ✓ std sizes | BPB validation | FAT media | 65-85 |
| NIB      | - | ✓ 232960 | - | GCR patterns | 60-70 |
| TD0      | ✓ "TD"/"td" | - | Header CRC | - | 90+ |
| IMD      | ✓ "IMD " | - | Track headers | - | 95+ |

### Unknown/Ambiguous Handling

```c
typedef enum uft_unknown_action {
    UFT_UNKNOWN_REJECT,     // Return error
    UFT_UNKNOWN_BEST_GUESS, // Use highest confidence
    UFT_UNKNOWN_ASK_USER,   // Return special code for UI
    UFT_UNKNOWN_RAW,        // Treat as raw sector data
} uft_unknown_action_t;
```

---

## Format Varianten

### D64 Varianten

| Variante | Größe | Tracks | Mit Error-Map |
|----------|-------|--------|---------------|
| D64-35 | 174848 | 35 | 175531 |
| D64-40 | 196608 | 40 | 197376 |
| D64-42 | 205312 | 42 | 206114 |

**Validierung:**
- BAM (Block Availability Map) at Track 18, Sector 0
- Directory chain starting at T18 S1
- DOS version byte ('A' or 'B')
- Disk name in PETSCII

### ADF Varianten

| Variante | Größe | Sektoren | Tracks×Heads |
|----------|-------|----------|--------------|
| ADF-DD | 901120 | 1760 | 80×2 |
| ADF-HD | 1802240 | 3520 | 80×2 |

**Validierung:**
- Bootblock checksum (Amiga-style carry-add)
- DOS type byte (0-7)
- Root block at center (880 or 1760)
- Root block type = 2 (T_HEADER)

### IMG/PC Varianten

| Variante | Größe | Cylinders×Heads×Sectors |
|----------|-------|-------------------------|
| 160KB SS/DD | 163840 | 40×1×8 |
| 180KB SS/DD | 184320 | 40×1×9 |
| 320KB DS/DD | 327680 | 40×2×8 |
| 360KB DS/DD | 368640 | 40×2×9 |
| 720KB DS/DD | 737280 | 80×2×9 |
| 1.2MB DS/HD | 1228800 | 80×2×15 |
| 1.44MB DS/HD | 1474560 | 80×2×18 |
| 2.88MB DS/ED | 2949120 | 80×2×36 |

**Validierung:**
- Boot signature 0x55 0xAA
- BPB (BIOS Parameter Block)
- FAT media descriptor (0xF0-0xFF)
- Consistent sector counts

---

## Format Klassifikation

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                     FORMAT CLASSIFICATION                                    │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  FLUX (Raw Timing)           BITSTREAM (Encoded)       SECTOR (Data)       │
│  ─────────────────           ──────────────────        ─────────────       │
│  • SCP                       • HFE                     • D64               │
│  • Kryoflux                  • G64                     • ADF               │
│  • A2R                       • WOZ                     • IMG               │
│                              • NIB                     • DSK               │
│                                                        • IMD               │
│  ↓ Highest fidelity          ↓ Bit-accurate            ↓ Data only        │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  CONTAINER (Metadata)        ARCHIVE (Compressed)                          │
│  ────────────────────        ────────────────────                          │
│  • IPF                       • TD0                                         │
│  • STX                       • NBZ                                         │
│                                                                             │
│  ↓ Protection info           ↓ Decompress first                            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Konverter-Pfade

### Lossless (Verlustfrei)

```
SCP ──────────► HFE         (Flux → Bitstream)
Kryoflux ─────► SCP         (Stream → Standard flux)
G64 ──────────► HFE         (Bitstream → Bitstream)
ADF ──────────► IMG         (Layout conversion)
NBZ ──────────► D64/G64     (Decompress)
```

### Lossy (Datenverlust)

```
SCP ──────────► D64         ⚠️ Timing lost
G64 ──────────► D64         ⚠️ GCR encoding lost
IPF ──────────► ADF         ⚠️ Protection info lost
WOZ ──────────► DSK         ⚠️ Copy protection lost
```

### Synthetic (Daten synthetisiert)

```
D64 ──────────► G64         ⚠️ GCR timing estimated
D64 ──────────► SCP         ⚠️ Flux fully synthesized
ADF ──────────► HFE         ⚠️ MFM encoding synthesized
DSK ──────────► WOZ         ⚠️ GCR timing estimated
```

---

## Validierungschecks

### Strukturelle Integrität

| Format | Check | Confidence Boost |
|--------|-------|------------------|
| D64 | BAM directory link (T18 S1) | +15 |
| D64 | DOS version ('A') | +10 |
| ADF | Bootblock checksum = 0 | +20 |
| ADF | Root block type = 2 | +10 |
| SCP | Track count valid | +10 |
| SCP | Revolution count 1-20 | +5 |
| G64 | Track offset table | +10 |

### Checksummen

| Format | Checksum Type | Location |
|--------|---------------|----------|
| ADF | Amiga carry-add | Bootblock, Root block |
| D64 | - | (keine in D64) |
| SCP | XOR | Header |
| TD0 | CRC-16 | Header, Tracks |
| IMD | - | Per-sector mode |
| G64 | - | (keine in G64) |

### Logische Konsistenz

| Format | Validation |
|--------|------------|
| D64 | BAM entry count matches bitmap |
| D64 | Directory chain follows valid sectors |
| ADF | Bitmap block count matches used blocks |
| IMG | FAT entries within valid range |
| SCP | Track offsets within file size |

### Plausibilität

| Format | Heuristic |
|--------|-----------|
| D64 | PETSCII characters in disk name |
| D64 | Speed zone consistency |
| ADF | Valid AmigaDOS type (0-7) |
| IMG | FAT media descriptor (0xF0-0xFF) |
| SCP | Reasonable track lengths |

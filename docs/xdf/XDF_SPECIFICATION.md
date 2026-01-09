# XDF Container Family Specification

**Version:** 1.0.0  
**Date:** 2025-01-08  
**Author:** UFT Team

## Overview

XDF (eXtended Disk Format) is a forensic container family for preserving floppy disk data with full metadata, confidence scores, and repair audit trails.

## Container Family

| Format | Platform | Extension | Description |
|--------|----------|-----------|-------------|
| AXDF | Amiga | .axdf | ADF/ADZ Extended |
| DXDF | C64 | .dxdf | D64/G64 Extended |
| PXDF | PC | .pxdf | IMG/IMA Extended |
| TXDF | Atari ST | .txdf | ST/MSA Extended |
| ZXDF | ZX Spectrum | .zxdf | TRD/DSK Extended |
| MXDF | Multi | .mxdf | Multi-Format Bundle |

## Design Principles

1. **No assumptions without measurement** - Every value is measured, not guessed
2. **No repair without justification** - Every change is documented
3. **No "OK/Error"** - Only confidence scores (0-100%)
4. **Copy protection ≠ defect** - Protection is preserved, not "fixed"
5. **Everything explicit** - No hidden state, no implicit behavior

## 7-Phase Forensic Pipeline

### Phase 1: Read (Erfassung)
- Multiple independent reads per track/sector
- Capture raw data, timing, jitter
- Store ALL reads, not just "best"

### Phase 2: Compare (Vergleich)
- Bit-by-bit comparison across reads
- Generate stability maps
- Calculate reproducibility metrics

### Phase 3: Analyze (Struktur)
- Treat track as SIGNAL, not bytes
- Identify zones: Sync, Data, Gap, Weak, Protection
- Detect timing anomalies

### Phase 4: Knowledge Match (Erwartungsmodell)
- Compare against known patterns
- Sources: WHDLoad, CAPS, TOSEC, Scene docs
- Identify intentional protection vs. defects

### Phase 5: Validate (Bewertung)
- Score each element (0-100%)
- Classify: OK, Weak, Protected, Defect, Unreadable
- Generate decision matrix ("Why is it classified this way?")

### Phase 6: Repair (kontrolliert)
- Only repair if:
  - Defect is unambiguous
  - Not copy protection
  - Decision is documented
- All repairs are reversible

### Phase 7: Rebuild (Export)
- Generate classic format (ADF/D64/IMG/etc.)
- Generate XDF with:
  - Full track descriptors
  - Protection flags
  - Confidence maps
  - Recovery log

## File Structure

```
┌──────────────────────────────────────────────┐
│ XDF Header (512 bytes)                       │
├──────────────────────────────────────────────┤
│ Track Table                                  │
├──────────────────────────────────────────────┤
│ Sector Table                                 │
├──────────────────────────────────────────────┤
│ Zone Table (optional)                        │
├──────────────────────────────────────────────┤
│ Repair Log                                   │
├──────────────────────────────────────────────┤
│ Decision Matrix                              │
├──────────────────────────────────────────────┤
│ Knowledge Matches                            │
├──────────────────────────────────────────────┤
│ Stability Maps                               │
├──────────────────────────────────────────────┤
│ Data Block (4KB aligned)                     │
│   - Sector data                              │
│   - Flux data (optional)                     │
│   - Timing data (optional)                   │
└──────────────────────────────────────────────┘
```

## Confidence Scores

| Range | Meaning |
|-------|---------|
| 0-25% | Very low confidence |
| 25-50% | Low confidence |
| 50-80% | Medium confidence |
| 80-95% | High confidence |
| 95-100% | Very high confidence |

## Zone Types

| Type | Description |
|------|-------------|
| SYNC | Synchronization pattern |
| HEADER | Sector header |
| DATA | Sector data |
| GAP | Inter-sector gap |
| WEAK | Weak/unstable bits |
| NOISE | Undefined/noise |
| PROTECTION | Protection area |
| TIMING_ANOMALY | Timing-based protection |

## Protection Flags

| Flag | Description |
|------|-------------|
| WEAK_BITS | Weak/unstable bits |
| FUZZY_BITS | Fuzzy bits (intentional) |
| LONG_TRACK | Extended track length |
| SHORT_TRACK | Shortened track |
| DENSITY_CHANGE | Density manipulation |
| TIMING | Timing-based protection |
| EXTRA_SECTORS | More sectors than expected |
| MISSING_SECTOR | Missing sector ID |
| DUPLICATE_ID | Duplicate sector ID |
| BAD_CRC | Intentional CRC error |
| SYNC_PATTERN | Non-standard sync |
| GAP_ENCODING | Modified gap |
| HALF_TRACKS | Uses half-tracks |
| NO_FLUX | No-flux area |

## Repair Actions

| Action | Description |
|--------|-------------|
| CRC_1BIT | Single-bit CRC correction |
| CRC_2BIT | Two-bit CRC correction |
| MULTI_REV | Multi-revolution fusion |
| INTERPOLATE | Weak bit interpolation |
| PATTERN | Pattern-based reconstruction |
| REFERENCE | Reference image comparison |
| MANUAL | Manual correction |
| UNDO | Repair was undone |

## API Usage

```c
// Create context for platform
xdf_context_t *ctx = xdf_create(XDF_PLATFORM_AMIGA);

// Configure options
xdf_options_t opts = xdf_options_default();
opts.enable_repair = true;
opts.max_repair_bits = 2;
xdf_set_options(ctx, &opts);

// Import classic format
xdf_import(ctx, "game.adf");

// Run full pipeline
xdf_pipeline_result_t result;
xdf_run_pipeline(ctx, &result);

// Check results
printf("Confidence: %d.%02d%%\n", 
       result.overall_confidence / 100,
       result.overall_confidence % 100);
printf("Repairs: %d/%d\n", 
       result.repairs_successful, 
       result.repairs_attempted);

// Export
xdf_export(ctx, "game.axdf");
xdf_export_classic(ctx, "game_fixed.adf");

// Cleanup
xdf_destroy(ctx);
```

## Platform-Specific Notes

### AXDF (Amiga)
- MFM encoding
- 11 sectors/track (DD), 22 sectors/track (HD)
- OFS/FFS filesystem support
- Protection: RNC CopyLock, Rob Northen, Trace Vector

### DXDF (C64)
- GCR encoding (5-to-4)
- 4 density zones (21/19/18/17 sectors)
- Half-track support
- Protection: V-MAX!, RapidLok, Vorpal, GMA

### PXDF (PC)
- MFM encoding
- Multiple densities (DD/HD/ED)
- FAT12/16/32 filesystem
- Protection: SafeDisc, SecuROM (floppy components)

### TXDF (Atari ST)
- MFM encoding (PC-compatible)
- WD1772 FDC quirks
- Protection: CopyLock, Macrodos, Fuzzy, Flaschel

### ZXDF (ZX Spectrum)
- TR-DOS or +3 DOS
- 16 sectors/track (TR-DOS) or 9 sectors/track (+3)
- Beta Disk, +3, DISCiPLE support

### MXDF (Multi-Format)
- Contains any combination of above
- Relationships between disks
- Bundle-level metadata
- Perfect for multi-disk games

## Mandatory Output Artifacts

For every disk:
1. **Disk Summary** - Overall status
2. **Track Grid** - Status, confidence, protection per track
3. **Recovery Log** - All repair actions
4. **Decision Matrix** - "Why is something classified this way?"

## Architecture Rules

Strict layer separation:
1. **Physics** - Flux/Timing (raw signal)
2. **Encoding** - MFM/GCR (bit patterns)
3. **Layout** - Track/Sector (structure)
4. **Logic** - DOS/Loader (filesystem)
5. **Meaning** - Game/Protection (interpretation)

No mixing of layers!

# UFT Protection Module Changelog

## v20.1 - Protection Extensions (2026-01-04)

### Neue Dateien
- `integration/uft_protection_ext.h` - Extended Protection Header
- `sources/uft_protection_ext.c` - Extended Protection Implementation
- `tests/test_protection.c` - Unit Tests (18 Tests, alle PASS)

### Neue Features

#### Longtrack-Varianten (12 Typen)
| Typ | Sync | Min Bits | Publisher/Spiele |
|-----|------|----------|------------------|
| PROTEC | 0x4454 | 107,200 | Viele Amiga-Titel |
| Protoscan | 0x41244124 | 102,400 | Lotus I/II |
| Tiertex | 0x41244124 | 99,328-103,680 | Strider II |
| Silmarils | 0xa144 + "ROD0" | 104,128 | Französische Publisher |
| Infogrames | 0xa144 | 104,160 | Hostages, etc. |
| Prolance | 0x8945 | 109,152 | B.A.T. (Ubisoft) |
| APP | 0x924a | 110,000 | Amiga Power Pack |
| Seven Cities | 0x9251/0x924a | 101,500 | Seven Cities Of Gold |
| Super Methane Bros | GCR 0x99999999 | 52,500 | GCR-Track |
| Empty | - | 105,000 | Leerer Long-Track |
| Zeroes | - | 99,000 | Nur Nullen |
| RNC Empty | - | 99,000 | RNC Dual-Format |

#### Detection-Funktionen
- `uft_detect_longtrack_ext()` - Auto-Detection aller Typen
- `uft_detect_longtrack_protec()` - PROTEC-spezifisch
- `uft_detect_longtrack_protoscan()` - Protoscan-spezifisch
- `uft_detect_longtrack_tiertex()` - Tiertex-spezifisch
- `uft_detect_longtrack_silmarils()` - Silmarils-spezifisch
- `uft_detect_longtrack_infogrames()` - Infogrames-spezifisch
- `uft_detect_longtrack_prolance()` - Prolance-spezifisch
- `uft_detect_longtrack_app()` - APP-spezifisch
- `uft_detect_longtrack_sevencities()` - Seven Cities mit CRC-Prüfung
- `uft_detect_longtrack_supermethanebros()` - GCR-Track
- `uft_detect_longtrack_empty()` - Empty/Zeroes

#### Generation-Funktionen
- `uft_generate_longtrack()` - Universelle Generation
- `uft_generate_longtrack_protec()` - PROTEC generieren
- `uft_generate_longtrack_protoscan()` - Protoscan generieren
- `uft_generate_longtrack_silmarils()` - Silmarils generieren

#### Utility-Funktionen
- `uft_longtrack_type_name()` - Typ zu Name
- `uft_longtrack_get_def()` - Definition abrufen
- `uft_check_pattern_sequence()` - Pattern-Prüfung
- `uft_crc16_ccitt()` - CRC16-CCITT Berechnung
- `uft_longtrack_ext_print()` - Ergebnisse ausgeben

### Bestehende Module (unverändert)
- `uft_protection.c/h` - CopyLock, Speedlock, Weak Bits, Custom Sync

### Test-Ergebnisse
```
=== UFT Protection Detection Tests ===

LFSR Tests:                    3/3 PASS
CopyLock Tests:                3/3 PASS
Longtrack Definition Tests:    3/3 PASS
Longtrack Generation Tests:    2/2 PASS
Longtrack Detection Tests:     3/3 PASS
CRC Tests:                     1/1 PASS
Utility Tests:                 3/3 PASS

=== Results: 18/18 tests passed ===
```

### Algorithmen-Quelle
Basierend auf Analyse von Disk-Utilities (Keir Fraser, GPL).
Clean-Room Reimplementierung der dokumentierten Algorithmen.

### Lizenz-Kompatibilität
- Header: Eigenentwicklung (beliebige Lizenz)
- Implementation: Clean-Room (keine GPL-Übernahme)
- Tests: Eigenentwicklung

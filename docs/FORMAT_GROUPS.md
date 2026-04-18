# UnifiedFloppyTool — Format-Gruppen

Alle 130 registrierten Plugins kategorisiert nach Plattform/Typ.

## Gruppen-Übersicht

| Gruppe | Anzahl | Beschreibung |
|--------|--------|--------------|
| `CBM` | 8 | Commodore 8-bit (C64, C128, VIC-20, PET, CBM 8000) |
| `ATARI` | 9 | Atari 8-bit + Atari ST |
| `APPLE` | 7 | Apple II + Macintosh |
| `AMIGA` | 3 | Commodore Amiga |
| `IBM_PC` | 10 | IBM PC kompatibel + Generic |
| `AMSTRAD` | 3 | Amstrad CPC + PCW |
| `SPECTRUM` | 8 | ZX Spectrum + SAM Coupe + Jupiter Ace |
| `ACORN` | 3 | BBC Micro + Acorn Archimedes |
| `TRS80` | 6 | Tandy TRS-80 + CoCo |
| `MSX` | 2 | MSX Computer |
| `CPM` | 5 | CP/M Generic + CP/M-spezifisch |
| `JAPAN` | 12 | NEC PC-88/98, Sharp X1/X68000, Fujitsu FM-7, Toshiba |
| `FLUX` | 4 | Universelle Flux/Bitstream-Container |
| `TAPE` | 1 | Tape-Formate |
| `OTHER` | 49 | Nischen + nicht-klassifizierte DSK-Varianten |

**Gesamt:** 130 Plugins

---

## CBM — Commodore 8-bit

| Plugin | Format | Gerät/System |
|--------|--------|--------------|
| `d64` | Commodore 1541 | C64/VIC-20 Floppy |
| `d67` | Commodore 2040/4040 | CBM 8032 |
| `d71` | Commodore 1571 | C128 |
| `d80` | Commodore 8050 | CBM 8000-Serie |
| `d81` | Commodore 1581 | C128 3.5" |
| `d82` | Commodore 8250 | CBM 8000-Serie |
| `g71` | GCR 1571 Bitstream | C128 |
| `dsk_vic` | VIC-1540 | VIC-20 |

## ATARI — Atari 8-bit + ST

| Plugin | Format | System |
|--------|--------|--------|
| `atr` | ATR | Atari 8-bit |
| `atx` | ATX (VAPI) | Atari 8-bit protected |
| `xfd` | XFD Raw | Atari 8-bit |
| `dcm` | DCM Compressed | Atari 8-bit |
| `pro` | PRO (APROI) | Atari 8-bit protected |
| `st` | ST Raw | Atari ST |
| `msa` | MSA Compressed | Atari ST |
| `stx` | STX (Pasti) | Atari ST protected |
| `dim_atari` | Atari DIM (FastCopy) | Atari ST |

## APPLE — Apple II + Macintosh

| Plugin | Format | System |
|--------|--------|--------|
| `do` | DOS 3.3 Order | Apple II |
| `po` | ProDOS Order | Apple II |
| `woz` | WOZ v1/v2/v2.1 | Apple II (flux) |
| `nib` | Nibble | Apple II |
| `2img` | Universal 2IMG | Apple II |
| `d13` | DOS 3.2 (13-sector) | Apple II |
| `dc42` | DiskCopy 4.2 | Macintosh |

## AMIGA — Commodore Amiga

| Plugin | Format | Info |
|--------|--------|------|
| `adf` | ADF | Amiga Disk File |
| `dms` | DMS | Disk Masher System (komprimiert) |
| `ipf` | IPF (CAPS/SPS) | Preservation-Format |

## IBM_PC — IBM PC kompatibel

| Plugin | Format | Info |
|--------|--------|------|
| `imd` | ImageDisk | Dave Dunfield |
| `td0` | Teledisk | Sydex |
| `cqm` | CopyQM | Sydex |
| `fdi` | Formatted Disk Image | Generic |
| `86f` | 86Box Floppy | 86Box Emulator |
| `dsk_emu` | Emulator Generic | Raw IBM |
| `dsk_uni` | Universal DSK | Raw IBM |
| `dsk_krg` | Kriegsmarine | Raw IBM (DD) |
| `t1k` | Tandy 1000 | IBM-kompatibel |
| `dsk_dc42v` | DiskCopy variant | Raw |

## AMSTRAD — Amstrad CPC + PCW

| Plugin | Format | System |
|--------|--------|--------|
| `dsk_cpc` | CPC DSK | Amstrad CPC |
| `edsk` | Extended DSK | Amstrad CPC |
| `dsk_pcw` | PCW | Amstrad PCW |

## SPECTRUM — ZX Spectrum + SAM Coupe

| Plugin | Format | System |
|--------|--------|--------|
| `trd` | TR-DOS | ZX Spectrum |
| `scl` | SCL | ZX Spectrum |
| `udi` | UDI | ZX Spectrum flux |
| `mgt` | +D/DISCiPLE | ZX Spectrum |
| `sam` | MGT SAM Coupe | SAM Coupe |
| `sad` | SAM Coupe SAD | SAM Coupe |
| `dsk_p3` | Spectrum +3 | ZX Spectrum +3 |
| `dsk_ace` | Jupiter Ace | Jupiter Ace |

## ACORN — BBC Micro + Acorn

| Plugin | Format | System |
|--------|--------|--------|
| `ssd` | SSD/DSD | BBC Micro |
| `adl` | ADFS Large | Acorn 80-track |
| `adf_arc` | Archimedes ADFS | Acorn Archimedes |

## TRS80 — Tandy TRS-80

| Plugin | Format | System |
|--------|--------|--------|
| `jv1` | JV1 | TRS-80 simple |
| `jv3` | JV3 | TRS-80 w/ directory |
| `jvc` | JVC | TRS-80 w/ header |
| `dmk` | DMK (David Keil) | TRS-80 MFM |
| `vdk` | VDK | Tandy CoCo |
| `tan` | TAN | TRS-80 Raw |

## MSX — MSX Computer

| Plugin | Format | Info |
|--------|--------|------|
| `dsk_msx` | MSX Generic | Raw FAT12 |
| `msx_disk` | MSX Enhanced | Boot-Sector-Check |

## CPM — CP/M Generic

| Plugin | Format | Info |
|--------|--------|------|
| `cpm` | CP/M DiskDef | libdsk CP/M |
| `rcpmfs` | Remote CP/M FS | Network CP/M |
| `myz80` | MYZ80 | Z80-Emulator |
| `dsk_cro` | Cromemco | S-100 Cromemco |
| `dsk_flex` | Flex OS | FLEX |

## JAPAN — NEC/Sharp/Fujitsu/Toshiba

| Plugin | Format | System |
|--------|--------|--------|
| `d77` | D77 | Sharp X1, FM-7 |
| `d88` | D88 | PC-88/PC-98 |
| `dim` | X68000 DIM | Sharp X68000 |
| `nfd` | T98-Next NFD | PC-98 |
| `fdi_pc98` | PC-98 FDI (Anex86) | PC-98 |
| `dsk_fm7` | FM-7 DSK | Fujitsu FM-7 |
| `dsk_nec` | PC-6001 | NEC PC-6001 |
| `dsk_cg` | SHARP MZ (CG) | Sharp MZ |
| `dsk_xm` | Sharp X1 (XM) | Sharp X1 |
| `dsk_mz` | SHARP MZ-800 | Sharp MZ-800 |
| `dsk_sc3` | SHARP SC-3000 | Sharp SC-3000 |
| `dsk_smc` | SMC-777 | Sony SMC-777 |
| `dsk_tok` | Toshiba T100 | Toshiba T100 |

## FLUX — Universelle Flux/Bitstream

| Plugin | Format | Info |
|--------|--------|------|
| `hfe` | HFE v1/v2/v3 | HxC Universal |
| `kfx` | KryoFlux Stream | KryoFlux |
| `mfi` | MAME Floppy | MAME |
| `pri` | PCE Raw Image | PCE Emulator |

## TAPE — Tape-Formate

| Plugin | Format | System |
|--------|--------|--------|
| `cas` | MSX Cassette | MSX |

## OTHER — Nischen

49 Plugins, meist platform-spezifische DSK-Varianten oder Nischen-Hardware:

`apridisk` (ACT Apricot), `cfi` (Catweasel), `dsk_agt`, `dsk_ak`, `dsk_aln` (Alphatronic),
`dsk_aq` (Mattel Aquarius), `dsk_bk` (Elektronika BK), `dsk_bw` (Bondwell), `dsk_ein` (Einstein),
`dsk_eqx` (Equinox), `dsk_fp` (Epson FP), `dsk_hk`, `dsk_hp` (HP LIF), `dsk_kc` (Robotron KC85),
`dsk_lyn` (Camputers Lynx), `dsk_m5` (Sord M5), `dsk_mtx` (Memotech), `dsk_nas` (Nascom),
`dsk_nb` (Bondwell NB), `dsk_ns` (North Star DSK), `dsk_oli` (Olivetti M20), `dsk_orc` (Oric),
`dsk_os9` (OS-9), `dsk_px` (Epson PX-8), `dsk_rc` (RC702), `dsk_rld` (Roland),
`dsk_san` (Sanyo MBC), `dsk_sv` (Spectravideo), `dsk_vec` (Vectrex), `dsk_vt` (DEC VT180),
`dsk_wng` (Wang), `dsk_x820` (Xerox 820), `edk` (Ensoniq Sampler), `fds` (Famicom Disk System),
`fdi_pc98` (bereits in JAPAN), `hardsector`, `logical`, `micropolis` (Micropolis MetaFloppy),
`nanowasp` (Microbee), `northstar` (North Star DOS), `opus` (Opus Discovery),
`pdp` (DEC PDP-11 RX01/02), `posix`, `qrst`, `sap_thomson` (Thomson MO/TO),
`syn` (Synclavier), `v9t9` (TI-99/4A), `victor9k` (Victor 9000/Sirius 1),
`xdm86` (TI-99 Disk Manager)

---

## Verwendung

```c
#include "uft/uft_format_plugin.h"

/* Plugin-Lookup nach Gruppe */
size_t count = 0;
const uft_format_group_t *groups = uft_format_get_groups(&count);

for (size_t i = 0; i < count; i++) {
    printf("Group %s: %zu plugins\n",
           groups[i].name, groups[i].plugin_count);
}

/* Gruppe nach Namen finden */
const uft_format_group_t *cbm = uft_format_find_group("CBM");
if (cbm) {
    for (size_t i = 0; i < cbm->plugin_count; i++)
        printf("  %s\n", cbm->plugins[i]->name);
}
```

---

*Letzte Aktualisierung: 2026-04-17 (Phase 2 der UFT Master-Anweisung)*

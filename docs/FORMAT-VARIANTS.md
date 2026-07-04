# FORMAT-VARIANTS — dokumentierte Sub-Versionen pro Formatfamilie

_Web-recherchiert 2026-07-05, quer-referenziert mit dem tatsächlichen Reader/
Writer-Code (Phase-4 Arbeitspaket „Format-Varianten-Recherche"). Support-Status
stammt aus dem Code, nicht aus Annahmen; Spec-Links zur Verifikation._

Spalten: **Variante** · **Spezifikation** · **UFT-Status** (aus Code) ·
**Aufwand** falls Lücke. „Status" ist R=Read, W=Write.

Legende Aufwand: S = klein (<50 LOC), M = mittel (50–150 LOC), L = groß
(>150 LOC, mehrere Sessions / neues Subsystem).

---

## Apple — WOZ (Klasse 1, Flux/Bitcell)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| WOZ 1.0 | [applesaucefdc.com/woz/reference1](https://applesaucefdc.com/woz/reference1/) | R+W (`WOZ_SIGNATURE_V1`, `uft_woz.c`) | — |
| WOZ 2.0 (+WRIT-Chunk) | [applesaucefdc.com/woz/reference2](https://applesaucefdc.com/woz/reference2/) | R+W (`WOZ_SIGNATURE_V2`, byte-identity Round-Trip MF-317) | — |
| WOZ 2.1 (INFO-Erweiterung) | [reference2](https://applesaucefdc.com/woz/reference2/) | R (INFO version-gated), W schreibt v2-Layout | INFO-2.1-Felder beim Write: S |

WRIT-Chunk (Rückschreib-Anweisungen) wird beim Read ignoriert, beim Write
nicht erzeugt — für den forensischen Read-Pfad unkritisch (Bitstream ist in
TRKS). Effort für WRIT-Emit: M.

## Apple — A2R (Klasse 1, Flux)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| A2R 2.x (STRM-Chunk) | [applesaucefdc.com/a2r2-reference](https://applesaucefdc.com/a2r2-reference/) | R (`version==2`, STRM, `uft_a2r_parser.c`) | — |
| A2R 3.x (RWCP-Chunk, 2021, nicht rückwärtskompatibel, hard-sectored) | [applesaucefdc.com/a2r](https://applesaucefdc.com/a2r/) | R (`version==3`, RWCP) | — |

Beide Versionen gelesen. Location-Formel 3.5" v3 = `((track<<1)+side)` —
verifizieren dass der Reader das nutzt (sonst Seiten-Vertauschung): Prüf-S.

## Apple — MOOF (Klasse 1/2, Bitstream + Apple 3.5")

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| MOOF 1.0 | [applesaucefdc.com](https://applesaucefdc.com/moof-reference/) | R + W (semantic Round-Trip MF-319) | — |

## HxC — HFE (Klasse 2, Bitstream)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| HFE v1 (`HXCPICFE`, reiner Bitstream) | [hxc2001 HFE-file-format](https://hxc2001.com/floppy_drive_emulator/HFE-file-format.html) | R+W (`uft_hfe.c`) | — |
| HFE v2 (4-Byte-Opcodes, variable Bitrate, 2012) | [Rev.3.1 PDF](https://hxc2001.com/download/floppy_drive_emulator/HxC_Floppy_Emulator_HFE_file_format.pdf) | R als v1 (Opcodes nicht interpretiert) | Opcode-Decode: M |
| HFE v3.0 (`HXCHFEV3`, Opcode-Redesign, 2017) | [Rev.3.1 PDF](https://hxc2001.com/download/floppy_drive_emulator/HxC_Floppy_Emulator_HFE_file_format.pdf) | R (`HFE_SIGNATURE_V3`, variable Bitrate) | — |
| HFE v3.1 (**Weakbits-Opcodes**, 2019) | [Rev.3.1 PDF](https://hxc2001.com/download/floppy_drive_emulator/HxC_Floppy_Emulator_HFE_file_format.pdf) | **R: Weak-Opcodes NICHT dekodiert** (`uft_hfe.c:789` „HFE does not carry weak-bit flags") | Weak-Opcode-Decode: **M** |

> **Klasse-2-Lücke (direkt relevant fürs Snapshot-Arbeitspaket):** HFE v3.1
> kann Weak/Fuzzy-Bits als Opcodes speichern — genau die „Snapshot wo die Spec
> es vorsieht"-Fähigkeit. UFT liest den v3-Bitstream, ignoriert aber die
> Weak-Opcodes. Das ist die konkrete nächste Klasse-2-Implementierung.
> Gleiches gilt für die v2-Opcodes (variable Bitrate).

## SuperCard Pro — SCP (Klasse 1, Flux)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| SCP Basis (Version-Byte @0x03, bis 5 Revolutionen) | [scp_image_specs v2.5](https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt) | R + W (Multi-Rev-Weak-Bit Round-Trip MF-327) | — |
| Footer-Extension (FLAG bit5, Laufwerks-/Creator-Metadaten) | [v2.5 spec](https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt) | R (`scp_extension_footer_t`), W emittiert keinen Footer | Footer-Emit: S |
| v2.3 Flag bit7 (3rd-party-Flux-Creator-Kennung) | [v2.5 spec](https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt) | Flags gelesen, bit7 nicht ausgewertet | S (nur Metadaten) |

## SPS — IPF (Klasse 1, Flux/CTR)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| IPF (CAPS/SPS, Density + Protection) | [IPF v1.6 PDF](https://www.kryoflux.com/download/ipf_documentation_v1.6.pdf) | R (CTRaw, `uft_ipf_ctraw_v2.c`) | — |

IPF ist proprietär (CAPS-Lib-Referenz). Read über eigene CTRaw-Impl.
Density-/Protection-Marker-Vollständigkeit ist ohne Ground-Truth-Korpus nicht
byte-verifizierbar → offener Punkt (KNOWN_ISSUES FMT-9).

## Commodore — D64 / G64 (D64=Klasse 3 error-aware, G64=Klasse 2)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| D64 35-Track (174.848 B) | [Schepers D64.TXT](https://ist.uwaterloo.ca/~schepers/formats/D64.TXT) | R+W (`uft_d64_parser_v2.c`) | — |
| D64 35-Track +Errormap (175.531 B, +683) | [Schepers](https://ist.uwaterloo.ca/~schepers/formats/D64.TXT) | R+W (`D64_SIZE_35_ERRORS`, `has_error_bytes`) | — |
| D64 40-Track (196.608 B) / +Errormap (197.376 B, +768) | [Schepers](https://ist.uwaterloo.ca/~schepers/formats/D64.TXT) | R+W (`D64_SIZE_40*`) | — |
| G64 (84 Halbtrack-Einträge + Speed-Zones) | [Schepers G64.TXT](https://ist.uwaterloo.ca/~schepers/formats/G64.TXT) | R (`uft_g64_parser_v3.c`) | — |

D64-Errormap (1541-Controller-Fehlercodes pro Sektor) = die orthogonale
Disk-Fehler-Achse; wird gelesen und beim Write erhalten. ✓

## Amiga — ADF (Klasse 3 Sektor; Extended-ADF grenzt an Klasse 2)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| ADF Standard DD (901.120 B) / HD (1.802.240 B) | [archiveteam ADF](http://fileformats.archiveteam.org/wiki/ADF_(Amiga)) | R+W (`uft_adf*`) | — |
| ADZ (gzip-komprimiertes ADF) | [Wikipedia ADF](https://en.wikipedia.org/wiki/Amiga_Disk_File) | Nicht direkt (externe Dekompression nötig) | gzip-Wrapper: S |
| **Extended ADF** (MFM-Info für Kopierschutz) | [archiveteam ADF](http://fileformats.archiveteam.org/wiki/ADF_(Amiga)) | **Nicht als eigene Variante** | R+W Extended: M |

> **Scope-Hinweis:** Extended-ADF speichert rohe MFM für kopiergeschützte
> Disks (mehr als Standard-ADF). Fehlt aktuell — sinnvoller Kandidat, weil es
> Amiga-Protection abbildet, die das Standard-ADF strukturell verliert.
> Aufwand M; Priorität nach HFE-v3-Weakbits.

## IBM PC — IMD / TD0 (Klasse 3 error-aware)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| IMD (offen dokumentiert) | [ImageDisk imd.pdf](https://oldcomputers-ddns.org/public/pub/manuals/imd.pdf) | R+W (CRC/Deleted-Marks, `uft_imd_plugin.c`) | — |
| TD0 Normal (`TD` = 0x5444) | [archiveteam TD0](http://fileformats.archiveteam.org/wiki/TD0) | R (`TD0_MAGIC_NORMAL`) | — |
| TD0 Advanced (`td` LZH/LZSS-Huffman) | [archiveteam TD0](http://fileformats.archiveteam.org/wiki/TD0) | R (`TD0_MAGIC_ADVANCED`, `uft_td0_lzss.c`) | — |

TD0 ist offiziell undokumentiert (reverse-engineered via Dunfield). Read-only
ist angemessen; Write würde Re-Kompression erfordern (`uft_td0.c:247`).

## Amstrad/Spectrum — DSK / EDSK (Klasse 2/3)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| DSK Standard (CPCEMU) | [cpcwiki DSK](https://www.cpcwiki.eu/index.php/Format:DSK_disk_image_file_format) | R+W (`uft_dsk_cpc.c`) | — |
| EDSK Extended (`EXTENDED`, variable Sektorgrößen, uPD765-Status, Weak-Sektoren) | [cpctech extdsk](https://cpctech.cpcwiki.de/docs/extdsk.html) | R+W (`memcmp(...,"EXTENDED")`) | — |

EDSK Weak-Sektor-Mechanik (3×-Sampling desselben Sektors mit Unterschieden):
Read der mehrfachen Sektor-Kopien verifizieren — Prüf-S.

## Atari ST — STX (Pasti) / ATX (Klasse 2 Bitstream)

| Variante | Spezifikation | UFT-Status | Aufwand |
|---|---|---|---|
| STX/Pasti v3 (`RSY\0`, Fuzzy-Mask-Records, readTime-Timing) | [Pasti-documentation PDF](http://info-coach.fr/atari/documents/_mydoc/Pasti-documentation.pdf) | R (`uft_stx_parser*`, Fuzzy-Masken) | — |
| ATX (Atari 8-bit, Weak/Timing) | [a2600/atx docs] | R (`uft_atx_parser_v2.c`, 54 Fehler-Marker) | — |

STX Fuzzy-Bits + readTime (Sektor-Timing) sind Kopierschutz-relevant und
werden gelesen. Byte-genaue Fuzzy-Reproduktion vs. reale Disk: nur mit
Ground-Truth-Korpus verifizierbar → offener Punkt.

---

## Zusammenfassung der gefundenen Lücken (priorisiert)

| # | Lücke | Klasse-Bezug | Aufwand |
|---|---|---|---|
| 1 | **HFE v3.1 Weak-Bit-Opcodes nicht dekodiert** | Klasse-2 Snapshot | M |
| 2 | HFE v2 Opcodes (variable Bitrate) nicht interpretiert | Klasse-2 | M |
| 3 | Extended-ADF (MFM-Protection) fehlt | Klasse-2/3-Grenze | M |
| 4 | WOZ WRIT-Chunk / INFO-2.1-Emit beim Write | Klasse-1 W | S–M |
| 5 | SCP-Footer-Emit beim Write (Metadaten) | Klasse-1 W | S |
| 6 | ADZ (gzip-ADF) kein direkter Reader | Klasse-3 Komfort | S |

Lücke #1 ist der logische nächste Klasse-2-Implementierungsschritt: HFE v3
kann Weak-Bits laut Spec speichern, UFT liest den Bitstream aber ohne die
Weak-Opcodes. Alle Lücken sind additiv (kein Format wird falsch gelesen —
sie erweitern die Abdeckung).

## Nicht byte-verifizierbar ohne Korpus (explizit offen)

IPF-Density/Protection-Marker, STX-Fuzzy-Reproduktion, EDSK-Weak-Sampling und
HFE-v3-Weakbits lassen sich gegen **reale kopiergeschützte Disks** nur mit
einem Ground-Truth-Testimage-Korpus byte-exakt verifizieren. Ohne diesen
Korpus bleibt die Read-Korrektheit dieser Protection-Pfade ein offener Punkt
(siehe `docs/KNOWN_ISSUES.md` FMT-9) — nicht stillschweigend als erledigt
markiert.

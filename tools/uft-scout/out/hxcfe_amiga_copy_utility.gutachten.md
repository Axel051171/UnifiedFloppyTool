<!-- uebernommen: MF-646 -->
# Gutachten: jfdelnero/HXCFE_Amiga_copy_utility

Stand: 2026-08-27 · dritter Scout-Zyklus
Messung: `hxcfe_amiga_copy_utility.messung.json`
(HEAD `8bb80d68d0`, letzter Commit 2016-03-01, Autor gonk23)
· Inventar: UFT `bb74f540` (SSOT ok, 88 Plugins, 22 Korpus-Abbilder)

## TL;DR

Der **Code** ist Zielmaschinen-Software (m68k-AmigaOS-Programm, das über
das HxC-Direct-Access-Protokoll FAT32 von der Emulator-SD-Karte liest) —
für UFT nicht übertragbar und nicht nötig. Der **Fund** ist eine Datei:
`hxcfe.hfe.zip` enthält ein **HxC-natives HFE-v1-Abbild**, das genau die
Header-Felder belegt (track_encoding=AMIGA_MFM, interface_mode=0x04,
84 Spuren), die das einzige liegende Korpus-HFE (greaseweazle) auf
0xFF/„unknown" lässt. **Kategorie: Daten** (Fixture), sonst irrelevant.

## Was das Repo ist (gemessen)

- 41 Dateien; 13 `.c`, 15 `.h` (Messung, `sprachen`)
- Autor gonk23, 8 Commits, alle Feb/März 2016 (`git log`, e10ed9e…8bb80d6)
- Ableger von jfdelnero/HXCFE_Amiga_file_selector — `COPYING_FULL` Z. 1–5
  und README §Development nennen die Ableitung ausdrücklich
- Kern `amiga_hw.c` (851 Z.): Paula-Register-I/O (`amiga_regs.h`,
  DSKSYNC 0x4489 in Z. 333/378), LUT-basierter MFM→Bin-Decoder
  (`MFMTOBIN`, Z. 97–101) und Bin→MFM-Encoder (`BuildCylinder`,
  Z. 407–430) für **IBM/ISO-MFM** (0xFE-IDAM, CRC16 — Z. 513, 619–664).
  Es ist KEIN AmigaDOS-Encoder (kein odd/even-Split, kein
  Longword-Checksum-Verfahren) — die MF-539-Lücke (fehlender
  AmigaDOS-Encoder für ADF→HFE) wird hiervon **nicht** geschlossen.
- `hxcfeda.h` (63 Z.): Direct-Access-Sektorstrukturen
  (`direct_access_status_sector`, `direct_access_cmd_sector`) — das
  Protokoll, mit dem eine Zielmaschine den HxC-Emulator über den
  Floppy-Bus steuert
- `fat32/`: vendorte „FAT File IO Library" (Rob Riglar)
- `crc.c` (76 Z.): CRC16 — UFT hat eine CRC-Engine (Inventar: `uft_crc`)
- `hxcfe.hfe.zip`: das Distributions-Abbild des Utilities, siehe Fund

## Lizenz (aus den Dateien, je Ebene — Zone: PRÜFEN)

| Quelle | Befund | Zone |
|---|---|---|
| `COPYING` (Wurzel) | GPL-3.0-Volltext („Version 3, 29 June 2007", Z. 2) | GELB |
| Datei-Header `amiga_hw.c`/`hxcfeda.h`/`hxcfe.c` | „either version 2 of the License, or (at your option) any later version" (GPLv2+, DEL-NERO-Notice) | GRÜN |
| `fat32/COPYRIGHT.txt` | GPL-2.0-Volltext („Version 2, June 1991", Z. 1–2) | GRÜN |
| `fat32/License.txt` | „This versions license: GPL" ohne Version, Dual-Angebot kommerziell | PRÜFEN |

**Datei-Header ≠ Repo-Lizenz** (GPLv2+ vs. GPL-3.0) → Matrix-Fall
PRÜFEN: **falls je Code portiert werden sollte, Eigentümer-Vorlage,
keine eigene Auslegung.** Für den hier vorgeschlagenen Fund ist das
folgenlos: es wird kein Code portiert, und die Fixture-Datei wird nicht
redistribuiert (Korpus ist gitignored, im Repo liegen nur
SHA-256-Manifeste).

## Was das Inventar sagt (Abfragen zitiert)

- `hfe` → `{"vorhanden": true, "tier": "T1b", "treffer": ["hfe"],
  "plugin_liste_vollstaendig": true}`
- `hxc` → `{"vorhanden": false, "abgedeckt": false, …}` und
  `direct access` → `{"vorhanden": false, "abgedeckt": false, …}` —
  `abgedeckt: false`, also **von Hand nachgesehen**: 16 Dateien im Baum
  nennen HxC (u. a. `src/formats/hfe/uft_hfe.c`,
  `src/formats/flux/uft_hxcstream.c`); einen Direct-Access-Client oder
  HxC-Provider gibt es nicht (ProviderV2-Liste `src/hardwaretab.h:45-62`,
  9 Provider, kein HxC) — und er wäre auch sinnlos: das Protokoll läuft
  auf der **Zielmaschine** über deren Floppy-Bus, nicht auf einem PC-Host.
- `fat32` → `{"vorhanden": true, "treffer": ["fat32", "uft_fat32_mbr"]}`
- `amiga` → `{"vorhanden": true, "treffer": ["amiga", "amiga_ext",
  "uft_amiga_syncs"]}`
- Korpus (`inv["korpus"]`, 22 Einträge): genau **ein** HFE-Abbild liegt —
  `tests/corpus_free/gw_amigados.hfe`, Herkunft „greaseweazle 1.23".
  Kein HxC-natives HFE vorhanden.

## Der Fund: HxC-natives HFE-v1-Fixture (Kategorie: Daten)

Gemessene Header (jeweils Bytes 0–17 der Datei):

| Feld | `gw_amigados.hfe` (liegt, Korpus) | `hxcfe.hfe` (Repo B) |
|---|---|---|
| Signatur | `HXCPICFE` (v1) | `HXCPICFE` (v1) |
| number_of_tracks | 0x50 = 80 | **0x54 = 84** |
| number_of_sides | 2 | 2 |
| track_encoding | **0xFF (unknown)** | **0x01 (AMIGA_MFM)** |
| bitrate | 0x00FD = 253 kbit/s | 0x00FD = 253 kbit/s |
| interface_mode | **0xFF (unknown)** | **0x04** |
| Größe / SHA-256 | 2 049 024 B | 2 151 424 B / `71bcfca460dab57b9681bae0d57a527a4a54789086bc59287574bea6d9153aa4` |

Bedeutung für UFT:

1. `src/formats/hfe/uft_hfe.c` mappt `HFE_ENC_AMIGA_MFM` (Z. 47, 137)
   und reicht es an `track->encoding` durch (Z. 693) — aber **kein
   liegendes Korpus-Abbild übt diesen Zweig**: das einzige HFE im Korpus
   trägt 0xFF in beiden semantischen Feldern.
2. `docs/VERIFICATION_TIERS.md` Z. 32 führt `hfe` auf T1b mit
   Korpus-Zähler **1** und nennt als Referenz HxC-Doku/-Loader — der
   Korpus-Erzeuger ist aber greaseweazle. `docs/PLAN_v4.1.7.md` Z. 122
   warnt genau vor dieser Konstellation (Oracle und Korpus-Erzeuger aus
   derselben Hand): ein vom **Format-Autor-Ökosystem selbst** erzeugtes
   Abbild ist die unabhängige Herstellerquelle, die dem HFE-Korpus fehlt.
3. 84 Spuren (statt 80) ist zusätzlich ein realer Randfall oberhalb der
   üblichen Zylinderzahl, aus einer Herstellerdatei statt synthetisch.

Beschaffung: bereits gemessen und trivial — `hxcfe.hfe.zip` liegt im
Repo (`https://github.com/jfdelnero/HXCFE_Amiga_copy_utility`, HEAD
`8bb80d68d0`), SHA-256 der entpackten Datei oben. Gegen `inv["korpus"]`
geprüft: nichts Gleichwertiges vorhanden.

## Einhängepunkt

`docs/PLAN_v4.1.7.md` §Korpus/Tier-Hebung (Z. 19 ff.,
`tests/corpus_manifest/manifest.json`) + `docs/VERIFICATION_TIERS.md`
Z. 32 (`hfe`, T1b, Korpus-Zähler 1 → 2). Bestehender Test als Träger:
`test_corpus_hfe` (dort bereits als hfe-Beleg geführt).

## Einfrier-konformer Weg für Stufe 4

Korpus-/Verifikationsarbeit ist unter der Einfrier-Regel ausdrücklich
erlaubt (kein neuer Format-Code nötig — der AMIGA_MFM-Zweig existiert).
Weg: (a) Datei nach `tests/corpus/` (gitignored) + SHA-256 ins Manifest
mit Provenienz (Repo-URL, HEAD `8bb80d68d0`, Zip-Name); (b) **Rotbeweis
zuerst**: Korpus-Test schreiben, der aus dem Fixture
`track_encoding==AMIGA_MFM`, `interface_mode==0x04`, 84 Zylinder und
`track->encoding==UFT_ENC_AMIGA_MFM` erwartet — läuft er sofort grün,
ist er als Absicherungs-Test zu kennzeichnen, feuert er, ist der Befund
dokumentationspflichtig; (c) Referenz im Test-Header: Repo-URL + SHA-256
+ „HxC HFE v1 docs" (dieselbe Referenzfamilie, die Z. 32 der Tier-Tabelle
schon nennt).

## Oracle-Kandidat

Kein neuer. Das passende Oracle (hxcfe-CLI aus dem
HxCFloppyImageConverter) ist bereits bewertet
(`data/known_negatives.json`: latchdevel/HxCFloppyImageConverter,
„hxcfe-CLI als Oracle") — dieses Fixture wäre dessen natürlicher
Prüfgegenstand.

## Aufwandsklasse

**S** (eine Datei, ein Manifest-Eintrag, ein Korpus-Test; kein neuer
Format-Code).

## Kein Differenzlauf-Plan

Es wird keine Überlegenheit behauptet; der Code-Anteil des Repos wird
nicht vorgeschlagen (IBM-MFM-LUT-Codec hat UFT: Inventar `mfm` →
`vorhanden: dsk_mfm, mfm, mfm_native, uft_mfm`).

## UNGEKLÄRT

- Ob `hxcfe.hfe` neben dem GPL-Utility AmigaOS-/Kickstart-Material
  enthält (Bootblock-Inhalt nicht analysiert). Für den gitignorierten
  Korpus mit Manifest folgenlos; VOR jeder etwaigen Redistribution zu
  klären.
- Der Lizenz-Widerspruch Datei-Header (GPLv2+) vs. `COPYING` (GPL-3.0)
  ist nur relevant, falls je Code portiert werden soll — dann
  Eigentümer-Vorlage (Matrix: PRÜFEN).
- Ob `test_corpus_hfe` heute überhaupt Headerfelder prüft (Stufe-4-Frage;
  der Rotbeweis beantwortet sie nebenbei).

## OPEN_ITEMS-Vorschlag (siehe Sammelblock des Zyklus)

Ein Vorschlag, Priorität P2 — Wortlaut im Abschlussbericht des Zyklus.

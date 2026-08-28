# Gutachten: mamedev/mame (Zuschnitt: Floppy-Formatbibliothek + floptool)
<!-- uebernommen: MF-623 -->
Stand: 2026-08-27 · Messung: `work/mame.messung.json`
(HEAD `c0d3677674`, letzter Commit 2026-08-27, „commodore/c64: GEOS boots now")
· Inventar: UFT HEAD `bb74f540` (`scout_inv4.json`, 88 Plugins aus SSOT,
`plugin_liste_vollstaendig: true`, 22 Korpus-Abbilder)
· Vierter Zyklus, Repo vom Eigentümer benannt. Nicht auf der Negativliste.

> **Entwurf von Hand statt aus `gutachten.py`** — das Skript verweigerte
> mit „RATENBREMSE: bereits 5 offene Gutachten in diesem Zyklus", weil
> `zaehle_offene()` alle `.gutachten.md` in `out/` zählt, auch die sechs
> abgeschlossenen aus Zyklus 1–3. Siehe Werkzeugkasten-Befund W3.

---

## 0. Zuschnitt — was vermessen wurde und was NICHT

MAME hat Millionen Zeilen. Vermessen wurde ein **Sparse-Checkout** mit
genau zwei Teilbäumen plus Wurzeldateien (529 Dateien):

| Teilbaum | Inhalt | Warum |
|---|---|---|
| `src/lib/formats/` | 452 Dateien, die Floppy-/Cassette-/FS-Formatbibliothek | der einzige Teil mit Format-Wissen |
| `src/tools/` | floptool, imgtool, chdman-Quelle u. a. | das Oracle-Kandidaten-Werkzeug |

**Ausdrücklich NICHT angesehen** (und daher hier ohne jede Aussage):
`src/devices/` (FDC-/Drive-Emulation, u. a. WD177x/µPD765-Modelle),
`src/lib/util/` (u. a. `chd.cpp` — deshalb ist die CHD-Feldprüfung unten
UNGEKLÄRT), `src/emu/`, `src/mame/` (Treiber), `3rdparty/`, `hash/`,
`docs/` außer Web-Doku-Links. Das Feld `dateien: 529` in der Messung ist
die **Zuschnitt-Zahl, keine Repo-Zahl** (W2).

## 1. Messwerte (Zuschnitt)

- Dateien: 529 · Sprachen: .cpp 275, .h 241 · Domänen-Score: 23 von 24
  Begriffen (alle außer `atx`-Spezifika; Quelle `work/mame.messung.json`)
- floptool: 1495 Zeilen (`src/tools/floptool.cpp`), Kommandos gemessen
  aus `s_command_usage[]` (floptool.cpp:38ff): `identify`, `flopconvert`,
  `flopcreate`, `flopdir`, `flophashes`, `flopblocks`, `flopread`,
  `flopwrite`, `flopchmeta`, `floprename`
- Formatregistry: **154 Floppy-Formate** (`grep -o "FLOPPY_[A-Z0-9_]*_FORMAT"
  src/lib/formats/all.cpp | sort -u | wc -l` = 154; floptool lädt sie über
  `mame_formats_full_list`, image_handler.cpp:68)
- **12 Dateisysteme** im floptool-Registry (all.cpp, `HAS_FORMATS_FS_*`):
  cbmdos, fat, prodos, oric_jasmin, vtech, coco_rsdos, coco_os9, hplif,
  isis, hp98x5, adam_eos, unformatted

## 2. Lizenz — je Verzeichnis, aus den Dateien

MAMEs `COPYING` (Wurzel) erklärt: Projekt als Ganzes GPL-2.0, „Individual
source files may be made available under less restrictive licenses, as
noted in their respective header comments" (COPYING Zeile 4–7). Die
Datei-Header (`// license:`-Konvention, kein SPDX) sind daher die
maßgebliche Ebene. **Zensus gemessen** (grep über alle .cpp/.h):

| Verzeichnis | BSD-3-Clause | GPL-2.0+ | LGPL-2.1+ | ohne Header |
|---|---|---|---|---|
| `src/lib/formats/` (493 .cpp/.h) | 441 (inkl. 2 Einzeiler-Varianten) | 6 (`rx50_dsk.*`, `vt_cas.*`, `zx81_p.*`) | 6 (`mfm_hd.*`, `rpk.*`, `ti99_dsk.*`) | 0 |
| `src/tools/*.cpp/.h` (15) | 15 | 0 | 0 | 0 |
| `src/tools/imgtool/` (42) | 41 | 1 | 0 | 0 |

**Zonen-Urteil (nach `playbook/lizenzmatrix.md`):**

- `src/lib/formats/` und floptool: **GRÜN** — BSD-3 portierbar mit
  Attribution (samdisk-Muster); GPL-2.0+ und LGPL-2.1+ ebenfalls GRÜN
  für ein GPL-2.0-Projekt (SPDX je portierter Datei korrekt führen,
  Lehre P0-5). Für UFTs T3-Formate `v9t9` (→ `ti99_dsk.cpp`, LGPL-2.1+)
  gilt: portierbar, aber SPDX-Kennung der Datei wäre LGPL-2.1+.
- **floptool als Oracle-BINARY: Zone irrelevant** — Prozess-Aufruf, kein
  Linken. Das Binary ist GPL-2.0 (Gesamtwerk), Nutzung als externes
  Orakel ist lizenzrechtlich unkritisch, gleiche Lage wie `c1541`.
- Achtung Werkzeug-Fehlurteil: `vermessen.py` hat `COPYING` als
  „MIT (GRUEN)" eingestuft — falsch begründet, zufällig richtige Zone.
  Siehe Werkzeugkasten-Befund W1. Dieses Gutachten stützt sich auf den
  Datei-Header-Zensus oben, nicht auf das Skript-Urteil.

## 3. Inventar-Abgleich (Abfragen zitiert)

- `floptool` → `vorhanden: false, abgedeckt: false` („INVENTAR DECKT DAS
  NICHT AB … von Hand prüfen"). **Von Hand geprüft:** kein Treffer für
  `floptool` in `scripts/` oder `tests/` (grep leer); `docs/
  FORMAT_SOURCES.md:79-84` nennt floptool nur als **Doku-Quelle**;
  `docs/PLAN_v4.1.7.md:135` führt als Oracles `c1541`, `unadf`,
  `xdftool`, `atrcopy` — floptool fehlt. → kein Bestand, gültiger Fund.
- `mfi` → `vorhanden: true, tier: T3, plugin_liste_vollstaendig: true`.
- `ipf` → `vorhanden: true, tier: T3`.
- `stx` → `vorhanden: true, tier: T2`.
- `chd` → `vorhanden: false, abgedeckt: false`. **Von Hand geprüft:**
  `src/formats/mame/uft_chd.c` existiert (459 Zeilen, kompiliert via
  `.pro:2555`), ist aber **kein Registry-Plugin** und hat **keinen
  Aufrufer** im Baum (grep über src/include/tests: nur die Datei selbst).
- `oracle differential test` → `abgedeckt: false`. **Von Hand geprüft:**
  Oracle-Differenztests sind der Kern von `PLAN_v4.1.7.md` Phase 1
  (Zeile 104: „Das ist ein Oracle-Differenztest") — Konzept vorhanden,
  floptool als Oracle nicht.
- Bekannte Überschneidung: `docs/KNOWN_ISSUES.md:9247-9251` weist bereits
  SAMdisk als Cross-Check für 17 T3-Formate aus (`adf_arc`, `cfi`, `cpm`,
  `do`, `fdi_pc98`, `ipf`, `mfi`, `mgt`, `sad`, `sap_thomson`, `scl`,
  `trd`, `udi` + 4 heute-T2). floptool ist dazu **komplementär**, siehe §5.

## 4. Divergenzprüfung — der MAME-Code, den UFT schon hat

UFT führt **drei** MFI-Implementierungen. Gegen die maßgebliche Quelle
(`src/lib/formats/mfi_dsk.cpp` @c0d36776) gemessen:

**Reales MFI-Format** (mfi_dsk.cpp:81-82, mfi_dsk.h:51-59, load():124ff):
- Signatur **16 Bytes inkl. `\0`**: `"MAMEFLOPPYIMAGE"` (neu) oder
  `"MESSFLOPPYIMAGE"` (alt)
- Header 32 Bytes: sign[16] + `cyl_count` u32 (obere 2 Bits =
  resolution) + `head_count` u32 + `form_factor` u32 + `variant` u32
- ab Offset **0x20**: `(cyl_count<<resolution)*head_count` Einträge à
  16 Bytes {offset, compressed_size, uncompressed_size, write_splice}
- Spurdaten zlib-komprimiert; u32-Wörter, obere 4 Bits MG-Code
  (MG_F/N/D/E), untere 28 Bits Zeit

| UFT-Datei | Signatur-Prüfung | Urteil (gemessen) |
|---|---|---|
| `src/formats/mfi/uft_mfi.c` (**registriertes Plugin**, T3) | 8 Bytes `"MAMEFLOP"` (Zeile 37) | akzeptiert echte neue MFI-Dateien (Präfix passt), **misparst dann jede davon**: liest form_factor bei 0x08 (dort steht `"PYIMAGE\0"`) und Track-Einträge ab **0x10** (uft_mfi.c:127-155) — dort liegen im echten Format cyl/head/ff/variant. Erster „Track-Eintrag" = Headerfelder, alle weiteren um 16 Bytes verschoben. Der Doc-Kommentar nennt mfi_dsk.cpp als Referenz (Zeile 29), **das beschriebene Layout steht dort nicht**. Alte `MESSFLOPPYIMAGE`-Dateien werden ganz abgelehnt. |
| `src/formats/mame/uft_mame_mfi.c` (kompiliert `.pro:2554`, kein Plugin, keine Aufrufer) | 17 Bytes `"MAME FLOPPY IMAGE"` **mit Leerzeichen** (Zeile 17) | matcht **keine reale MFI-Datei, je**. Headerstruktur (u8 cylinders/heads, Zeile 32-40) frei erfunden. Fabrikat nach FMT-Muster; `KNOWN_ISSUES.md:4683-4687` kennt die Datei nur als falschen SIGS-Verweis (MF-553), nicht als Layout-Fabrikat. |
| `src/samdisk/mfi.cpp` (vendored) | 16 Bytes, nur `"MESSFLOPPYIMAGE"` (Zeile 114) | Layout korrekt, aber lehnt moderne `"MAMEFLOPPYIMAGE"`-Dateien ab — Divergenz durch MAME-Umbenennung, upstream-samdisk-Stand. |

**Antwort auf die Divergenzfrage:** Unsere „Ableitung" ist keine.
Keine der drei Implementierungen liest eine von aktuellem MAME
geschriebene MFI-Datei korrekt; keine trägt Commit-Provenienz.
`uft_chd.c`: Magic `"MComprHD"` und v3/v4/v5-Headergrößen sind konsistent
mit dem bekannten CHD-Format (Feld-für-Feld-Abgleich UNGEKLÄRT — MAMEs
`chd.cpp` liegt in `src/lib/util/`, außerhalb des Zuschnitts), aber die
Datei ist toter Code ohne Aufrufer.

## 5. Der Hauptfund: floptool als zweites Oracle für die T3-Hebung

Schnittmenge **UFT-T3 (57 Formate) ∩ floptool (154 Formate)**, gemessen
über die `::extensions()`-Strings der MAME-Formatklassen:

**28 von 57 T3-Formaten haben ein floptool-Gegenstück:**

| UFT-T3 | MAME-Format (Quelle) | | UFT-T3 | MAME-Format (Quelle) |
|---|---|---|---|---|
| 2img | APPLE_2MG (ap_dsk35.cpp) | | jvc | JVC (jvc_dsk.cpp) |
| 86f | 86F (86f_dsk.cpp) | | mfi | MFI (mfi_dsk.cpp) |
| adf_arc | ACORN_ADFS_OLD/NEW (acorn_dsk.cpp, ext „adf,ads,adm,adl") | | mgt | MGT (coupedsk.cpp) |
| adl | ACORN_ADFS_OLD (dito) | | msx_disk | MSX (msx_dsk.cpp) |
| apridisk | APRIDISK (apridisk.cpp) | | nib | NIB (ap2_dsk.cpp) |
| d13 | A213S (ap2_dsk.cpp) | | opus | OPUS_DDOS/DDCPM (acorn_dsk.cpp) |
| d77 | D88 (d88_dsk.cpp, ext „d77,d88,1dd") | | po | A216S_PRODOS + APPLE_GCR (ap2/ap_dsk35) |
| dim | DIM (dim_dsk.cpp, X68000) | | sap_thomson | SAP (sap_dsk.cpp) |
| do | A216S_DOS (ap2_dsk.cpp, ext „dsk,do") | | ssd | ACORN_SSD (acorn_dsk.cpp) |
| edsk | DSK (dsk_dsk.cpp:285 „EXTENDED CPC DSK") | | trd | TRD (trd_dsk.cpp) |
| fdi_pc98 | PC98FDI (pc98fdi_dsk.cpp) | | v9t9 | TI99_SDF (ti99_dsk.cpp:883 „aka v9t9") |
| img | PC (pc_dsk.cpp, ext „dsk,ima,img,ufi,360") | | vdk | VDK (vdk_dsk.cpp) |
| ipf | IPF (ipf_dsk.cpp, read-only) | | victor9k | VICTOR_9000 (victor9k_dsk.cpp) |
| jv1 / jv3 | JV1/JV3 (trs80_dsk.cpp) | | | |

Davon sind **~20 NICHT durch den bereits notierten SAMdisk-Cross-Check
abgedeckt** (KNOWN_ISSUES:9249 nennt 17 Formate; neu hinzu kommen u. a.
2img, 86f, adl, apridisk, d13, d77, dim, edsk, img, jv1, jv3, jvc,
msx_disk, nib, opus, po, ssd, v9t9, vdk, victor9k). Für die
Überlappenden ist floptool das **zweite unabhängige** Orakel — genau
die Konstellation, die die Warnung in PLAN_v4.1.7 („wenn Oracle und
Korpus-Erzeuger dasselbe Werkzeug sind") entschärft.

**Ohne floptool-Gegenstück bleiben** (kein Fund, ehrlich benannt):
cas, cfi, cpm, dcm, dim_atari, dms, edk, fds, kfx, logical, micropolis,
myz80, nanowasp, northstar, pdp, posix, pri, pro, qrst, rcpmfs, sad,
sam, scl, syn, t1k, tan, udi, xdm86 — `hardsector` nur teilweise (H17D,
h17disk.cpp, nur Heathkit).

**Zusatznutzen T2:** d88, imd, td0, cqm, dmk, msa, st, stx (PASTI),
woz, dc42, nfd, hfe haben ebenfalls floptool-Gegenstücke — zweites
Orakel für T2→T1b.

**Beschaffung:** floptool wird mit den offiziellen MAME-Binärpaketen
ausgeliefert (mamedev.org-Release; Linux: Paket `mame-tools`). **Kein
Build aus dem Millionen-Zeilen-Baum nötig.** Quellen:
[mamedev.org Release-Seite](https://www.mamedev.org/release.html),
[Ubuntu mame-tools](https://launchpad.net/ubuntu/focal/+package/mame-tools),
[MAME-Doku Tools](https://docs.mamedev.org/tools/index.html).

## 6. Vorschläge für OPEN_ITEMS (max. 5, nach Priorität)

> Vorschlagsblock zur Übernahme durch einen Menschen. Kein Eintrag durch
> diesen Agenten.

**V1 · Oracle · floptool in den Phase-1-Prüfstand aufnehmen (Aufwand S)**
floptool (MAME-Binärdistribution, GPL-2.0-Prozess-Orakel wie c1541)
deckt 28 von 57 T3-Formaten ab, ~20 davon ohne bisheriges Orakel
(Messung: Gutachten §5, `::extensions()`-Abgleich @c0d36776).
Einhängepunkt: `docs/PLAN_v4.1.7.md` Phase 1, Oracle-Liste Zeile 135
(Skip-77-Konvention gilt unverändert). Einfrier-konform: floptool
`identify`/`flopconvert` ist die benannte Referenz; je Format zuerst ein
Rotbeweis (UFT-Lesung ≠ floptool-Lesung am selben Abbild muss rot
feuern können); Referenzvermerk im Testheader: „Oracle: MAME floptool
<Version>, mamedev/mame@c0d36776 src/lib/formats/<datei>".

**V2 · Correctness/Divergenz · MFI: drei Implementierungen, keine liest
aktuelles MFI (Aufwand M)**
Registriertes Plugin `uft_mfi.c` misparst jede reale MFI-Datei
(Track-Einträge ab 0x10 statt 0x20, Beleg §4); `uft_mame_mfi.c` hat eine
erfundene 17-Byte-Signatur und ist toter Code; `samdisk/mfi.cpp` lehnt
die moderne Signatur ab. Weg: Rotbeweis zuerst — Fixture per
`floptool flopcreate mfi <fs> ref.mfi` erzeugen (MFI `supports_save() =
true`, mfi_dsk.cpp:103), UFT-Öffnung dagegen: muss rot sein. Verhaltens-
Spec aus `mfi_dsk.cpp/h` (BSD-3 → sogar Port mit Attribution zulässig);
Referenz im Header: mamedev/mame@c0d36776 `src/lib/formats/mfi_dsk.cpp:81-82`
+ Struktur mfi_dsk.h:51-59. `uft_mame_mfi.c` an den
stub-eliminator-Prozess: DELETE-Kandidat (kein Plugin, kein Aufrufer).

**V3 · Referenz · MAMEs IPF-Parser als BSD-3-Referenz für UFTs IPF-T3
(Aufwand M)**
`src/lib/formats/ipf_dsk.cpp` (807 Zeilen, BSD-3, self-contained,
read-only) ist eine lizenzsaubere benannte Referenz für UFTs fünf
IPF-Dateien (`src/formats/ipf/`, T3) — ohne die CAPS-Bibliothek, auf
die sich `uft_caps_ipf.c:5` beruft. Einfrier-konform: Verhaltens-Spec
aus ipf_dsk.cpp, Rotbeweis gegen eine per floptool geprüfte IPF-Datei,
Referenz im Header wie V1. Fixture-Beschaffung siehe UNGEKLÄRT U3.

**V4 · Daten/Beschaffung · flopcreate+flophashes als Fixture-Fabrik
(Aufwand S)**
`flopcreate` erzeugt vorformatierte Abbilder in jedem der 154 Formate ×
12 Dateisysteme; `flophashes`/`flopdir` liefern Datei-Hashes je Abbild —
fertige Metrik für die VFS-P1-Differenztests (CBM DOS ist Nr. 1 im
Plan, fs_cbmdos liegt in floptool). Gegen `inv["korpus"]` geprüft:
keines der 22 liegenden Abbilder stammt aus floptool — keine Dublette,
und für ~20 T3-Formate existiert bisher gar kein Korpus-Eintrag.
Kreuz-Erzeugung beachten: floptool erzeugt, c1541/xdftool liest (oder
umgekehrt), nie dasselbe Werkzeug auf beiden Seiten (PLAN_v4.1.7:122).

**V5 · Hygiene · Herkunftspflicht für die MAME-Ableitungen (Aufwand S)**
`uft_chd.c` (kompiliert, keine Aufrufer, keine Commit-Provenienz) und
die MFI-Dateien aus V2 verletzen die Referenz-im-Header-Pflicht;
`docs/FORMAT_SOURCES.md:79-84` nennt floptool als Quelle, ohne dass der
Code der Quelle entspricht (§4). Weg: je Datei Herkunfts-Header
nachtragen (mamedev/mame@commit + Datei) ODER DELETE über den
stub-eliminator; Rotbeweis für CHD wäre eine chdman-erzeugte Datei
(chdman liegt derselben Binärdistribution bei).

## 7. Differenzlauf-Plan (für die Oracle-Behauptung in V1)

- **Binaries:** UFT-Testtreiber (Core-Lib, kein CLI — GUI-only-Regel)
  vs. `floptool` aus offizieller MAME-Binärdistribution (Version im
  Testlog festhalten).
- **Korpus:** je Format 1 floptool-`flopcreate`-Abbild + vorhandene
  `tests/corpus_free/`-Abbilder; Schadensfälle ausdrücklich außen vor
  (Orakel lesen gesunde Abbilder, PLAN_v4.1.7:150).
- **Metrik:** (a) `identify`-Urteil vs. UFT-Probe-Urteil; (b) Sektor-
  inhalt: `flopconvert <fmt>→PC-Raw` vs. UFT-Sektorlesung, bytweise;
  (c) für FS-Formate `flophashes` vs. UFT-VFS-Lesung.
- **Toleranzliste:** Formatierungs-Füllbytes (Gap/Filler) dürfen
  abweichen, wenn beide Werkzeuge sie unterschiedlich setzen —
  je Format einmalig dokumentieren; Flux-Formate (mfi) nur auf
  Container-Ebene vergleichen, nicht PLL-Bit-für-Bit.

## 8. UNGEKLÄRT

- **U1:** Feld-für-Feld-Abgleich `uft_chd.c` ↔ MAME `chd.cpp` —
  `src/lib/util/` lag außerhalb des Zuschnitts. Nicht geprüft, nicht
  behauptet.
- **U2:** Ob UFTs `dim` dasselbe X68000-DIM meint wie MAMEs
  `dim_dsk.cpp` (UFT führt auch `dim_atari` getrennt) — vor dem ersten
  Differenztest je Format die Geometrie-Erwartung abgleichen.
- **U3:** Frei verteilbare IPF-Fixtures: SPS-Images sind i. d. R. nicht
  redistributierbar. Kandidat: selbst erzeugte IPF? MAMEs IPF ist
  read-only (kein `flopcreate ipf`) — Beschaffungsweg offen,
  Eigentümer-Entscheidung nötig.
- **U4:** floptool-`flopconvert`-Schreibpfade sind selbst ungeprüfte
  Software; für T1b-Hebungen zählt Übereinstimmung zweier unabhängiger
  Werkzeuge, nicht floptool allein.
- **U5:** `src/tools/imgtool/` (41× BSD-3, 1× GPL-2.0+) wurde nur
  lizenz-gezählt, nicht funktional bewertet — imgtool überlappt mit
  floptool-FS-Funktionen, Bewertung ggf. eigener Zyklus.
- **U6:** Ob MAMEs `mdos_dsk.cpp` (Motorola MDOS) etwas für UFTs
  `micropolis`/`northstar`-Hard-Sector-Lücke hergibt: nein nach
  Extension-Messung, aber `h17disk.cpp` (H17D) wurde nicht tiefer
  geprüft.

## 9. Werkzeugkasten-Befunde (gemessen, an den Eigentümer)

- **W1 · `vermessen.py` Lizenz-Fehlurteil durch Mustereihenfolge:**
  MAMEs `COPYING` erklärt GPL-2.0 (Zeile 15–17), enthält aber ab Zeile
  64 und 210 eingebettete MIT-Texte („Permission is hereby granted…").
  `LIZENZ_MUSTER` prüft MIT **vor** GPL-2 und bricht beim ersten
  Treffer ab → `mame.messung.json` führt `kennung: MIT`. Hier zufällig
  zonengleich (beide GRÜN); ein GPL-3.0-Repo mit zitiertem MIT-Vermerk
  würde **GRÜN statt GELB** — lizenzkritisch. Fix-Richtung: alle
  Treffer sammeln, konservativste Zone melden, Mehrfachtreffer
  ausweisen.
- **W2 · `vermessen.py` sieht Datei-Header-Lizenzen nicht:** Es prüft
  nur LICENSE/COPYING auf Wurzel + 1 Ebene. Bei per-File-Lizenzierung
  (MAME `// license:`, anderswo SPDX) ist das Wurzelurteil
  strukturell unvollständig — der Zensus in §2 war Handarbeit. Zudem
  meldet `dateien` bei Sparse-Checkouts die Zuschnitt-Zahl ohne
  Kennzeichnung.
- **W3 · `gutachten.py` Ratenbremse zählt Lebenszeit statt Zyklus:**
  `zaehle_offene()` (gutachten.py:77-81) zählt alle `.gutachten.md` in
  `out/` — auch abgeschlossene aus früheren Zyklen. Mit 6 liegenden
  Gutachten aus Zyklus 1–3 ist das Skript **dauerhaft blockiert**; die
  Meldung nennt zudem den Konfigurationswert („bereits 5"), nicht den
  Ist-Stand (6). Die AGENT.md-Regel 5 begrenzt OPEN_ITEMS-Vorschläge
  **je Zyklus**, nicht Gutachten-Dateien insgesamt. Dieses Gutachten
  wurde deshalb von Hand nach der Vorlage erstellt.

## 10. Regeln, die für diesen Fund gelten

- Zone je Verzeichnis, nicht pauschal (§2); LGPL-2.1+-Dateien mit
  eigener SPDX-Kennung führen.
- Kein Code aus diesem Agenten (AGENT.md Regel 1); Stufe 4 geht über
  Rotbeweis + benannte Referenz + Referenz im Header (je Vorschlag
  benannt in §6).
- floptool-Nutzung als Prozess-Orakel berührt kein Lizenz-Linken.

<!-- uebernommen: MF-623 — floptool als Oracle registriert (Commit 06db93a9); MFI-Befunde in MF-614/616 abgearbeitet -->

<!-- uebernommen: MF-650 -->
# Gutachten: OpenCBM/OpenCBM + OpenCBM/libcbmimage

**Zyklus:** 2026-08-28 (Eigentümer-Auftrag: M.4-Blocker nachmessen; neue
Fragestellung seit MF-648: zweite unabhängige CBM-Hand für den
D64-Pfad — Anlass benannt gemäß AGENT.md Regel 6).
**Klone:** `tools/uft-scout/work/OpenCBM/` (HEAD `894195ca`, letzter
Commit 2026-04-10, shallow depth 50) · `tools/uft-scout/work/libcbmimage/`
(HEAD `1e2673ff`, letzter Commit 2025-03-28, shallow depth 30).
**Messungen:** `work/OpenCBM.messung.json` (1367 Dateien),
`work/libcbmimage.messung.json` (458 Dateien).

**Kategorie:** OpenCBM = Daten/Spec (Protokoll-Referenz, bereits per
MF-301 als Audit-Quelle genutzt) · libcbmimage = **Oracle + Daten**
(Konsolen-Referenz + Fixtures).

---

## 1. Was die zwei M.4-Befunde behaupten (wörtlich)

Auftragsprämisse war „zwei offene HIGH-Blocker". **Das ist der Stand vor
MF-301.** Die Original-Fassung (aus `git show a10f80cf~1:docs/KNOWN_ISSUES.md`,
Z. 420–428):

> **Befund 1 (HIGH):** `src/hal/uft_xum1541.c:239` (`xum_iec_command`) und
> `:580` (`uft_xum_iec_write`) lesen 1-Byte Bulk-IN-Status; die
> OpenCBM-Referenz liest 3 Bytes (`XUM_STATUSBUF_SIZE = 3`:
> `[status, val_lo, val_hi]`). Mit realer Firmware → vermutlich
> `LIBUSB_ERROR_OVERFLOW`, IEC-Pfad bricht beim ersten Silizium-Kontakt.

> **Befund 2 (HIGH):** WRITE/READ-Bulk-Header: HAL sendet
> `[opcode, len_lo, len_hi, 0]`, OpenCBM `[opcode, mode, len_lo, len_hi]`
> (Mode-Byte wählt serial/parallel-nibbler). Eine der beiden Seiten ist
> falsch — OpenCBM-Re-Audit nötig BEVOR M3.2-Wiring weitergeht.

Heutiger Stand von M.4 (`docs/KNOWN_ISSUES.md:425-459`): **„✓ RESOLVED
IN CODE (MF-301, OpenCBM-Quell-Audit)"** mit fünf Verdikten;
Tier-3-HW-Bench bleibt ausstehend (wired-but-unbenched). Die Befunde
blockieren M3.2 also **nicht mehr** — was noch blockiert, steht in §3.

## 2. Prüfung am Original — beide Befunde stimmen, MF-301 auch

Jede Aussage gegen den Klon `work/OpenCBM/` gemessen:

| Behauptung (M.4 / MF-301) | OpenCBM-Fundstelle | Urteil |
|---|---|---|
| Status ist 3 Bytes `[status, val_lo, val_hi]` | `xum1541/xum1541_types.h:64` (`XUM_STATUSBUF_SIZE 3`); Makros `:132-133` (`XUM_GET_STATUS_VAL = (buf[2]<<8)\|buf[1]`, also LE) | **stimmt** |
| Host liest Status im BUSY-Loop bis READY/ERROR | `opencbm/lib/plugin/xum1541/xum1541.c:797-846` (`xum1541_wait_status`: `while (deviceBusy)`, 3-Byte-Bulk-IN, `IO_BUSY=1/READY=2/ERROR=3` per `xum1541_types.h:121-123`) | **stimmt** |
| WRITE-Header `[opcode, proto\|flags, size_lo, size_hi]` | `xum1541.c:998-1001` (`cmdBuf[0]=XUM1541_WRITE; cmdBuf[1]=modeFlags; cmdBuf[2]=size&0xff; cmdBuf[3]=(size>>8)`) | **stimmt** |
| Protokoll-Nibble oben, Flags unten | `xum1541_types.h:140-152` (`XUM_RW_PROTO(x)=(x)&0xf0`, `XUM1541_CBM=(1<<4)` … `TAP_CONFIG=(11<<4)`); Flags `:155-156` (`WRITE_TALK=1<<0`, `WRITE_ATN=1<<1`) | **stimmt** |
| Nur READ=8/WRITE=9 als Bulk-Datenopcodes | `xum1541_types.h:77-78` | **stimmt** (MF-301 Verdikt 3) |
| IEC-Adressierung = CBM-WRITE mit ATN-Flag, Payload rohe ATN-Bytes | `archlib.c:336-339` (LISTEN: `0x20\|dev`, `0x60\|sec`), `:370-373` (TALK: `0x40\|dev`, zusätzlich `WRITE_TALK`), `:402-405` (OPEN: `0xf0\|sec`), `:434-437` (CLOSE: `0xe0\|sec`) | **stimmt** |
| IOCTL als Bulk-Command `[cmd, addr, secaddr, 0]` + 3-Byte-Status, nicht Control-Transfer | `xum1541.c:913-916` (in `xum1541_ioctl`) + `:940` (`wait_status`); Control-Transfer nur für RESET/INIT-Klasse (`xum1541_control_msg`, `:760-793`) | **stimmt** (Verdikt 4) |
| IOCTL-Konstanten 23–31 | `xum1541_types.h:92-104` (`IOCTL=16`, `GET_EOI=+7=23` … `IEC_POLL=+11=27` … `PARBURST_WRITE=+15=31`) | **stimmt** |

Zählmethode der Tabelle: jede Zeile ist EIN Vergleich Behauptung↔Datei;
Quelle der Behauptungen sind M.4 (beide Fassungen) und die fünf
MF-301-Verdikte; Treffer = wortlautgleiche Konstante/Struktur im Klon.

**Fazit §2:** Der MF-301-Umbau unserer HAL entspricht dem
OpenCBM-Original in allen acht geprüften Punkten. Kein neuer Delta-Fund
auf der Draht-Ebene. Firmware-Versions-Gate (`XUM1541_VERSION 8`,
`MINIMUM_COMPATIBLE_VERSION 7`, `xum1541_types.h:18-19`) wurde gegen
unsere INIT-Auswertung **nicht** verglichen → UNGEKLÄRT (e).

## 3. Unsere Seite: was an M3.2 wirklich noch offen ist

`src/hal/uft_xum1541.c` (748 Zeilen, 23 Funktionsdefinitionen, Stand
nach MF-301 + MF-470):

- **Real unter `UFT_HAS_LIBUSB`** (gesetzt von `UnifiedFloppyTool.pro:106`,
  `CMakeLists.txt:158`, Tests via Mock-libusb `tests/CMakeLists.txt:1898/1913`):
  Lifecycle `uft_xum_open/close` (libusb_init, claim_interface,
  INIT-Control-Transfer, Z. 331–420), `xum_wait_status` **liest 3 Bytes
  im BUSY-Loop** (Z. 250–272, Kommentar Z. 249 nennt die Referenz),
  `xum_cbm_write`, IEC listen/talk/unlisten/untalk/read/write mit
  ATN-Payload-Semantik (z. B. Z. 563–581), `iec_poll` (IOCTL 27).
- **Sechs unbedingte Stubs, alle auf der CBM-DOS-Kommandoebene** (nicht
  USB): `identify_drive`:453 (M-R-ROM-Probe), `get_status`:462
  (Fehlerkanal 15), `read_track_gcr`:515, `read_track`:526 (U1-Blockread),
  `read_disk`:537, `write_track`:546 (U2). Zählmethode:
  `grep -n "not implemented"` auf Funktionskörper ohne
  `#ifdef`-Alternative; 16 weitere NOT_IMPLEMENTED-Stellen sind die
  ehrlichen `#else`-Zweige für Builds ohne libusb.

**Drei Dokument-Driften, gemessen:**

1. `docs/MASTER_PLAN.md:284-291` (§M3.2) sagt „13/26 echten Funktionen …
   Multi-Session-libusb-Wiring offen" — das libusb-Wiring ist seit
   MF-301 gelandet; offen ist die DOS-Kommandoschicht (6 Stubs oben).
2. `src/hal/uft_xum1541.c:3` nennt sich „M3.2 partial scaffold" und
   Z. 22 behauptet „Honest stubs for the 18 USB/IEC-touching functions" —
   es sind noch 6.
3. `src/hal/uft_xum1541.c:5-6` attributiert „based on opencbm xum1541.c
   (https://github.com/OpenCBM/OpenCBM, **BSD-2**)" — **falsch**, siehe §4.
   `scripts/audit_spdx_policy.py` führt die Datei bereits auf der
   Attributionsliste (Ausgabe: `src/hal/uft_xum1541.c:5 based on opencbm
   xum1541.c`), mit falscher Lizenzangabe statt ohne.

**Hardware-Vorbehalt (MF-310):** Die Vervollständigung der sechs Stubs
ist hardwarefrei spezifizier- und testbar: Referenzverhalten liegt in
`opencbm/libd64copy/std.c:24` (`"U1:2 0 %d %d"` — exakt das Kommando,
das unser Stub-TODO Z. 522 nennt), `d64copy.c` (Fehler-/Retry-Semantik),
`cbmctrl.c` (Kanal-15-Status); Prüfstand ist der vorhandene Emulator
`tests/emulators/xum1541/` (firmware_state_machine.c + DIVERGENCES.md).
Tier-3-Bench bleibt community-delegiert.

## 4. Lizenz (aus den Dateien, je Ebene)

**OpenCBM:** Kein Wurzel-LICENSE; `opencbm/COPYING` = GPLv2-Volltext.
Dateiköpfe durchgängig „version 2 … **or (at your option) any later
version**" → **GPL-2.0-or-later**, Stichproben: Host-Plugin
`opencbm/lib/plugin/xum1541/xum1541.c:10-13`, `xum1541/xum1541_types.h:5-8`
(Firmware-Header), Firmware `xum1541/main.c`, `xum1541/commands.c`,
`archlib.c`, `opencbm/lib/cbm.c`, `opencbm/libd64copy/std.c`,
`opencbm/cbmctrl/cbmctrl.c`, `opencbm/d64copy/main.c`. **Zone GRÜN** —
Code portierbar mit Attribution-Header (samdisk-Muster), Konzept und
Oracle frei. `vermessen.py` meldete PRUEFEN („GPL-2.0 + LGPL mehrdeutig")
— von Hand entschieden: die LGPL-Erwähnung ist die GPLv2-Präambel
selbst, kein zweiter Lizenztext. **Vendoring-Achtung:** `xum1541/LUFA/`
(LUFA-USB-Stack), `xu1541/bootloader/usbdrv/` (V-USB),
`xum1541cfg/dfu-programmer-0.5.4/`, `cbm4wingui/` tragen eigene
Lizenzdateien — bei jedem Port meiden; für Protokoll-Spec irrelevant.

**libcbmimage:** Wurzel-`LICENSE` = GPLv2-Volltext; die geprüften
Quelldateien (`lib/alloc.c`, `lib/bam.c`, `lib/d40_d64_d71.c`,
`app/cbmimage.c`) tragen **keine** eigene Lizenzangabe → konservativ
**GPL-2.0-only**; beide Lesarten **Zone GRÜN** für ein GPL-2.0-Projekt.

**Attributions-Konsequenz (MF-636):** Unsere Zeile
`src/hal/uft_xum1541.c:6` behauptet BSD-2 für eine
GPL-2.0-or-later-Quelle. Da GPL-2.0-or-later GRÜN ist, ist nichts
zurückzubauen — aber die rechtliche Aussage muss stimmen (SPDX fehlt in
Datei und Header ebenfalls; Lehre P0-5).

## 5. libcbmimage als zweite CBM-Hand — gemessen, nicht gelesen

**Bau belegt (hardwarefrei, Windows/MinGW):**
`mingw32-make CC=gcc lib app` mit gcc 13.1.0 → `output/cbmimage/cbmimage.exe`.
Ohne `CC=gcc` scheitert der Bau (Makefile-Default `cc` existiert unter
MinGW nicht — `make/common.mk:66`).

**Läufe belegt (rc=0):**

| Lauf | Ergebnis |
|---|---|
| `cbmimage open tests/images/simpletest.d64 dir` | Verzeichnis inkl. Header `"SIMPLETEST" ST 2A` |
| `… simpletest.d64 bam` | Spurweise Belegungskarte (`1: (21) …`) |
| `… open <UFT>/tests/corpus_free/vice_c1541_35trk.d64 dir` | `0 "UFTCORPUS" 42 2A · 1 "UFT MARKER" PRG · 663 BLOCKS FREE` — **liest unser eigenes Korpus-Abbild** |
| `… open tests/images/empty-dd8.d1m dir` | CMD-Partitionstabelle (`"SYSTEM" SYS`, `3200 "PARTITION 1" D81`) |

**Konsolen-Befehle** (`app/cbmimage.c:749-792`): `open`, `dir`, `bam`,
`checkbam`, `fat`, `read` (Block), `showfile` (Extraktion), `validate`,
`chdir` (Partitionen). Dazu `tests/expected-results/` mit
Soll-Ausgaben je Format (bam/checkbam/dir/fat/read/validate für
d40/d64/d71/d80/d81/d82 …).

**Für MF-648/A5 (40-Spur-BAM-Bug) einschlägig:** libcbmimage führt die
40-Spur-Varianten **explizit getrennt** — `TYPE_D64_40TRACK`,
`…_SPEEDDOS`, `…_DOLPHIN`, `…_PROLOGIC` (`lib/fileimage.c:34-37` Größen,
`:162-175` Dispatch; `lib/d40_d64_d71.c:444-446` und `:611-661`
Konstruktoren) und setzt `info_offset_diskname = 0x90`
(`d40_d64_d71.c:425`) — exakt das Byte, an dem unser Parser laut MF-648
die Belegungskarte mit dem Disknamen kollidieren lässt
(`uft_d64_parser_v3.c:1029ff`, `4+(track-1)*4 = 0x90` für Spur 36).
`bam`/`checkbam` liefern genau die Struktur, die der A2-Differenzlauf
laut MF-648 mit `flophashes` **nicht** vergleichen konnte („hätte eine
falsche Belegung verglichen, grün wie rot gleich wertlos").

**Für das offene KNOWN_ISSUES-Item FMT-3 (CMD FD) einschlägig:**
`docs/KNOWN_ISSUES.md:578-606` fordert die verifizierte
Neu-Konsolidierung der zwei falschen CMD-FD-Leser
(`src/formats/c64/uft_cmd.c`, `src/formats/cmd_fd/uft_cmd_fd.c`; beide
ohne `uft_format_plugin_`-Struct, nicht tier-geführt — gemessen: kein
Treffer in `docs/VERIFICATION_TIERS.md`). libcbmimage liefert alle drei
Bausteine: Referenz-Implementierung (`lib/d1m_d2m_d4m.c` 342 Z.,
`lib/dnp.c` 279 Z., GPL GRÜN = portierbar), **Fixtures mit gemessen
exakt den in KNOWN_ISSUES:590-593 verifizierten Nativgrößen**
(`tests/images/empty-*.d1m` = 829440 B, `.d2m` = 1658880 B,
`.d4m` = 3317760 B; je zwei Varianten dd8/ddn usw.), und das Oracle
(`dir`-Lauf oben). `inv["korpus"]` hat **null** d1m/d2m/d4m-Einträge.

**Einschränkung:** README (`README.md:30`) nennt die API „preliminary …
subject to change" — für das Oracle-Binary egal, für einen Port die
Commit-ID pinnen. Kein Image-**Erzeuger** (keine format/create-Kommandos
gefunden) — hebt daher selbst nichts auf T1b, ist die Leser-Zweithand.

## 6. Inventar-Abfragen (zitiert)

`inventar.py query` gegen `work/inv.json` (Build rc=0, „88 Plugins
(SSOT ok), 232 Format-Dirs, 88 Tier-Zeilen, 22 Korpus-Abbilder,
HEAD 548f204b"):

- `xum1541` → `vorhanden: true` (Controller-Treffer; korrekt — HAL + Provider + Emulator existieren).
- `d1m`/`d2m`/`d4m` → `vorhanden: true, tier: null` — **Index-Falle**:
  der Treffer stammt aus `format_dirs: ['cmd_fd', 'd1m']`, und
  `src/formats/d1m/` ist ein **leeres, ungetracktes Verzeichnis**
  (gemessen: `ls -A` leer, `git ls-files` kennt nur `c64/uft_cmd.c` und
  `cmd_fd/uft_cmd_fd.c`). Dieselbe Klasse wie `flux visualization`
  (AGENT.md Regel 4) — deshalb hier trotz starkem Treffer von Hand
  geprüft: die Fähigkeit fehlt (beide Leser falsch, FMT-3 offen).
- `opencbm`, `libcbmimage`, `d64 oracle` → `abgedeckt: false` („INVENTAR
  DECKT DAS NICHT AB … `false` heisst hier NICHT `fehlt`") — von Hand
  geprüft: kein OpenCBM-Code vendored; CBM-Oracles laut
  `docs/ORACLES.md:74-75` (floptool `flophashes` d64) und `:124`
  (nibtools, GPL-3.0 GELB); **kein** BAM-Dump-Oracle für flache
  D64-Familie mit GRÜN-Lizenz im Register.
- Korpus: CBM-Abbilder d64/d71/d81/d67/d80/d82/g64/g71 — **acht
  cross-tool-Zeilen, alle VICE c1541** → die im Auftrag genannte
  Zirkularitätsfrage betrifft die gesamte CBM-Reihe; kein 40-Spur-D64,
  keine CMD-FD-Abbilder vorhanden.

## 7. Vorschläge (max. 5; Nummern **vorläufig**, ab SCOUT-54 wegen drei paralleler Zyklen)

### SCOUT-54 — Falsche Lizenz-Attribution der XUM1541-HAL berichtigen (+ zwei Status-Driften)

`src/hal/uft_xum1541.c:5-6` erklärt „based on opencbm xum1541.c … BSD-2".
Gemessen ist OpenCBM **GPL-2.0-or-later** (Dateikopf
`opencbm/lib/plugin/xum1541/xum1541.c:10-13` u. a., §4). Eine
Attribution ist eine rechtliche Aussage (MF-636); hier ist sie falsch —
folgenlos nur zufällig, weil GPL-2.0-or-later GRÜN ist. Fix: Angabe +
SPDX in .c und .h korrigieren; im selben S-Commit die zwei gemessenen
Status-Driften: Header Z. 3/22 („partial scaffold", „18 stubs" — real 6)
und `docs/MASTER_PLAN.md:284-291` („libusb-Wiring offen" — seit MF-301
gelandet; offen ist die DOS-Kommandoschicht, §3).
**Kennzahl:** die begründete fünfte — Dateien mit ungeklärter Herkunft
(LIZ-1-Attributionsliste: von „43 mit genannter Codebasis, nur 2 mit
genannter Lizenz" auf 3 mit **richtiger** Lizenz). **Aufwand S.**
**Einhängepunkt:** `docs/OPEN_ITEMS.md` LIZ-1 (`git grep LIZ-1`).

### SCOUT-55 — libcbmimage als CBM-BAM-Oracle registrieren (zweite Hand für A5/MF-648)

`docs/ORACLES.md`-Eintrag für `cbmimage.exe` (Bau- und Laufbeleg §5,
Commit `1e2673ff` pinnen): `bam`/`checkbam`/`dir`/`showfile`/`validate`
über D40/D64(35/40/42, inkl. SpeedDOS/DolphinDOS/ProLogic)/D71/D80/D81/
D82/D1M/D2M/D4M/DNP. Löst zwei benannte Lücken: (a) der A5-Fix
(40-Spur-BAM, MF-648) bekommt die BAM-genaue Vergleichsgröße, die
`flophashes` nicht liefert; (b) die VICE-Zirkularität der acht
CBM-Korpuszeilen bekommt eine unabhängige Gegenhand (lsatr/atrcopy-
Muster, `docs/ORACLES.md:126-128`). Lizenz GPL-2.0(-only) GRÜN — als
Oracle ohnehin frei, als Port-Referenz zulässig.
**Kennzahl:** ungeprüfte Formate runter — mittelbar über A5/SCOUT-42,
dessen Zuordnung bereits steht (`docs/OPEN_ITEMS.md:2472-2473`:
„d64-Hebung ist Moratoriums-Bedingung"); ehrlich gesagt bewegt dieser
Eintrag die Zahl nur zusammen mit dem A5-Fix. **Aufwand B** (Bau
dokumentieren, Registry-Zeile, Differenzlauf-Plan §8).
**Einhängepunkt:** `docs/ORACLES.md` Register + OPEN_ITEMS **A5**.

### SCOUT-56 — FMT-3 (CMD FD) bekommt Referenz, Korpus und Oracle in einem Zug

Das offene KNOWN_ISSUES-Item FMT-3 (`docs/KNOWN_ISSUES.md:578-606`,
seit MF-316) verlangt „careful, verified reimplementation — not a hasty
constant swap" und nennt die Nativgrößen 829440/1658880/3317760.
libcbmimage liefert alle drei fehlenden Zutaten (§5): GRÜN-portierbare
Referenz (`lib/d1m_d2m_d4m.c` + `lib/dnp.c`), Fixtures **in gemessen
exakt diesen Größen**, Oracle-Lauf belegt. Damit kann Stufe 4
regelkonform arbeiten: Rotbeweis zuerst (beide bestehenden Leser lehnen
ein echtes 829440-B-`.d1m` ab — KNOWN_ISSUES:595-596), Referenz im
Header (libcbmimage `1e2673ff` + d2m-dnp-Spec), jede Größe gemessen.
Einfrier-Regel: Konsolidierungs-Bugfix an Bestehendem, kein neues
Plugin-Moratoriumsthema — die Plugin-Frage (Registrierung) entscheidet
Stufe 4.
**Kennzahl:** ungeprüfte Formate runter — mit Ansage: d1m/d2m/d4m sind
heute **keine** Tier-Zeilen (gemessen §6); der Fix führt sie erstmals
mit Referenz in die Leiter ein, statt drei neue T3 zu erzeugen.
**Aufwand M** (Stufe 4; die Scout-Lieferung selbst ist B).
**Einhängepunkt:** KNOWN_ISSUES **FMT-3** (`git grep "FMT-3"`).

*(Nur 3 Vorschläge — die übrigen Funde bewegen keine der geführten
Zahlen und bleiben Fundus, §9.)*

## 8. Differenzlauf-Plan (für SCOUT-55, Pflicht bei „zweite Hand")

- **Binaries:** UFT-Testtreiber über `uft_d64_parser_v3`-Pfad (nach
  A5-Fix) vs. `cbmimage.exe bam` + `checkbam` (Commit `1e2673ff`,
  `CC=gcc`-Bau).
- **Korpus:** `tests/corpus_free/vice_c1541_35trk.d64` (liegt);
  zu beschaffen: 40-Spur-D64 (§10). Zusätzlich libcbmimage-eigene
  `tests/images/*.d64` mit `tests/expected-results/*` als drittem
  Fixpunkt.
- **Metrik:** Belegungsbits je Spur (frei/belegt je Sektor) +
  „BLOCKS FREE"-Summe + Diskname/ID; Treffer = bitgleiche Karte.
- **Toleranzliste:** PETSCII↔ASCII-Darstellung des Disknamens;
  Formatierung der Ausgabe (geparst, nicht textdiffed); Spuren > 35 bei
  Plain-40 nur vergleichen, wenn beide Seiten eine Karte behaupten —
  behauptet nur eine, ist genau DAS der Befund.

## 9. Fundus (bewegt keine Kennzahl — nicht in OPEN_ITEMS)

- **M3.2-DOS-Kommandoschicht-Spec:** Die sechs Stubs (§3) haben ihre
  vollständige GRÜN-Referenz in `opencbm/libd64copy/std.c` (U1/U2,
  Z. 24), `libd64copy/d64copy.c` (Retry/Fehlerkarte),
  `cbmctrl/cbmctrl.c` (Kanal-15-Status), plus Turbo-/Warp-6502-Routinen
  (`*.a65`) für später. Hardwarefrei verifizierbar gegen
  `tests/emulators/xum1541/`. Wird Auftrag, sobald M3.2 wieder
  Milestone-Arbeit ist — dann über MASTER_PLAN, nicht über diese Liste.
- **d82copy/imgcopy** (`opencbm/libimgcopy/`) als Referenz für
  IEEE-488-Laufwerke (8050/8250) — gleiche Lage.
- **Index-Falle `format_dirs`:** leeres Verzeichnis `src/formats/d1m/`
  erzeugt `vorhanden: true` (§6) — Kandidat für eine `inventar.py`-Härtung
  (leere/ungetrackte Verzeichnisse nicht als Treffer werten).

## 10. Beschaffungsliste (gegen `inv["korpus"]` geprüft, §6)

| Was | Warum | Weg (hardwarefrei) |
|---|---|---|
| `empty-dd8.d1m`, `empty-hd8.d2m`, `empty-ed8.d4m` (+ `*n`-Varianten) | FMT-3-Fixtures in verifizierten Nativgrößen; Korpus hat null CMD-FD | liegen im Klon `work/libcbmimage/tests/images/`; Übernahme mit Provenienz-Manifest. Lizenz-Notiz: GPL-Repo, aber leere formatierte Abbilder sind Daten mit fraglicher Schöpfungshöhe → kurze Eigentümer-Notiz im Manifest genügt m. E.; im Zweifel mit libcbmimage selbst reproduzieren (UNGEKLÄRT c2) |
| 40-Spur-D64 (Plain; ideal zusätzlich SpeedDOS-formatiert) | A5-Differenzlauf §8; Korpus hat nur 35 Spuren | VICE c1541 (Werkzeug + Version bereits als Korpus-Provenienz etabliert); SpeedDOS-Variante nur aus historischem Material — niedrige Priorität |

Kein Gerät erforderlich; nichts angefordert, was schon liegt.

## 11. UNGEKLÄRT

a) Ob libcbmimage bei **Plain**-40-Spur-D64 die Spuren 36–40 wirklich
   kartiert oder nur die Typen unterscheidet — entscheidet der
   `bam`-Lauf auf der 40-Spur-Beschaffung (§10); für den Differenzlauf
   ist beides verwertbar (Toleranzliste §8).
b) GPL-2.0-only vs. -or-later bei libcbmimage (Dateiköpfe schweigen;
   konservativ -only angenommen; beide GRÜN).
c) (1) Läuft `cbmimage.exe` außerhalb der MSYS/Git-Bash-Umgebung?
   (2) Reproduziert `cbmimage` die leeren Fixtures selbst (kein
   create-Kommando gefunden)?
d) `xum1541cfg`-Firmware-Update-Pfad — nicht untersucht (kein UFT-Bezug
   erkennbar).
e) Firmware-Versions-Gate (`XUM1541_VERSION 8` / `MIN_COMPAT 7`,
   `xum1541_types.h:18-19`) vs. unsere INIT-Auswertung in
   `uft_xum_open` — nicht verglichen; gehört in die
   M3.2-Fortsetzung (Fundus §9).
f) OpenCBM-Windows-Treiberarchitektur (opencbm.dll-Plugins, `windrv/`)
   — für unsere direkte libusb-HAL wahrscheinlich irrelevant, nicht
   vertieft.

## 12. Negativlisten-Status

`OpenCBM/OpenCBM` und `OpenCBM/libcbmimage` → **bewertet** (nicht
verworfen): OpenCBM bleibt die Protokoll-Referenz der
M3.2-Fortsetzung, libcbmimage der Oracle-Kandidat SCOUT-55/56.

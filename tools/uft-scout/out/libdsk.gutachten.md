<!-- uebernommen: MF-650 -->
# Gutachten: lipro-cpm4l/libdsk

- **Repo:** https://github.com/lipro-cpm4l/libdsk.git
- **Stand:** HEAD `1a6803566a0e4a766f7f51acb9baf9e11551ba77`, Committer-
  Datum **2015-11-15** („Add Robotron SCP format definitions");
  Versionsangabe im Baum: `configure.ac:` `AC_INIT(LibDsk, 1.5.12)`.
  **Der Fork ist gegenüber Upstream veraltet:** John Elliotts
  Original-Distribution (seasip.info/Unix/LibDsk/) führt aktuell
  **1.5.22** mit später hinzugekommenen Backends (CP2, D64/D64CPM,
  LDBST, D88 — WebFetch 2026-08-28). Belegbar im Baum selbst: UFT
  referenziert `drvmgt.c` und `drvopus.c` (s. Abschnitt 5), beide
  existieren in diesem Fork **nicht** (`ls lib | grep -i "mgt\|opus"`
  → 0 Treffer). Upstream ist Tarball-Distribution ohne öffentliches
  Git.
- **Messung:** `work/libdsk.messung.json` (2026-08-28, 323 Dateien,
  106 `.c` + 80 `.h`, Domänen-Score 17, Zone laut Skript PRUEFEN —
  von Hand aufgelöst, s. Abschnitt 1). Inventar `work/inv.json`
  (UFT HEAD `548f204b`, heute erzeugt, `rc=0`, SSOT ok).
- **Anlass (Regel 6):** Eigentümer-Auftrag dieses Zyklus. Das
  Schwester-Repo `lipro-cpm4l/cpmtools` steht als `bewertet` in der
  Negativliste (Ertrag: 131 diskdefs via Laufzeit-Parser); libdsk
  selbst ist **nicht** in `data/known_negatives.json` (grep `libdsk`
  → 0 Treffer). Neue Fragestellungen: Oracle-Tauglichkeit von
  `dsktrans`/`dskid`/`dskform` (docs/ORACLES.md-Fragen mit Beleg),
  die Geometrie-Frage aus MF-648, Lizenzwortlaut only vs. or-later.
- **Werkzeug:** C-Bibliothek + Konsolenwerkzeuge für Sektor-Disketten-
  abbilder und Floppy-Zugriff (John Elliott, Basis von cpmtools/JOYCE).
  25 Treiber (`dsktrans -types`, selbst ausgeführt): dsk, edsk,
  apridisk, copyqm, tele(disk), ldbs, sap, qrst, imd, ydsk, raw/rawoo/
  rawob, myz80, simh, nanowasp, logical, jv3, dc42, cfi, gotek,
  remote, rcpmfs.

**Kategorie:** Oracle + Daten (Korpus-Erzeugung) + Entlastungsbefund.
Kein Portier-Kandidat in diesem Zyklus (nichts vorgeschlagen, was Code
übernimmt).

---

## 1. Lizenzurteil (Zone, mit Wortlaut)

**Lizenzdatei:** einzige im Baum ist `doc/COPYING`
(`find . -iname "COPYING*" -o -iname "LICEN*"` → 1 Treffer):

> „GNU LIBRARY GENERAL PUBLIC LICENSE — Version 2, June 1991"
> (`doc/COPYING:1-2`)

**Dateiköpfe — die entscheidende only/or-later-Frage:** Standard-
Kopf aller Kernquellen (z. B. `lib/dskgeom.c:1-21`):

> „modify it under the terms of the GNU Library General Public
> License as published by the Free Software Foundation; **either
> version 2 of the License, or (at your option) any later
> version.**"

Also **LGPL-2.0-or-later** — nicht GPL, nicht only. Zählung
(Methode: `grep -l "version 2 of the License, or (at your option)
any later version"` über `lib/*.c`, `lib/*.h`, `include/*.h`,
`tools/*.c`, `tools/*.h`; Treffer zählt, wenn die Phrase wörtlich im
Kopf steht): **134 von 150 Dateien**. Die 16 ohne diese Phrase, je
von Hand gelesen:

| Datei(en) | Befund |
|---|---|
| `lib/blast.c`/`.h` | Mark Adler, zlib-artige Erlaubnisklausel (`blast.h:5-11`) — GRÜN |
| `lib/ldbs.c`/`.h` | MIT-Wortlaut („Permission is hereby granted, free of charge…", `ldbs.c:2-11`) — GRÜN |
| `lib/crc16.c`/`.h`, `tools/crc16.c`/`.h` | Übersetzung aus `CRCSUBS.Z80`, **kein Lizenz-Kopf** (`lib/crc16.c:6-13`) |
| `lib/crctable.c`/`.h` | „Automatically generated CRC table", kein Kopf |
| `lib/dskjni.c`, `lib/rpcfuncs.h`, `lib/w16defs.h`, `lib/w95defs.h`, `tools/labelopt.h`, `tools/utilopts.h` | kein Lizenz-Kopf (Projektdateien derselben Hand; Repo-Lizenz greift, aber ohne eigenen Kopf) |

**Zonen-Urteil: GRÜN** für den Werkzeug- und Konzept-Gebrauch.
Begründung entlang `playbook/lizenzmatrix.md`: LGPL-2.1 steht
ausdrücklich in GRÜN; LGPL-2.0-**or-later** erfüllt „2.1" per
or-later-Klausel, und die LGPL erlaubt zusätzlich die Konvertierung
zu GPL (LGPL-2.0 §3) — für ein GPL-2.0-Projekt kompatibel.
**Einschränkung, kein Freibrief:** würde je Code portiert, sind
`crc16.c`/`crctable.c` (kein Kopf → formal „alle Rechte
vorbehalten") ausgenommen → Eigentümer-Vorlage je Datei. Für dieses
Gutachten folgenlos: **kein Vorschlag unten portiert Code.** Oracle-
Nutzung vergleicht ausschließlich Programm-**Ausgaben** (ORACLES.md,
Frage 4).

## 2. Attribution der Quelle (Pflichtfeld, MF-636)

libdsk selbst vendort: `lib/blast.c` (Mark Adler, zlib-artig, für
PKWare-„explode"-Dekompression) und `lib/crc16.c` (Übersetzung von
`CRCSUBS.Z80`, Original-Kopf zitiert, keine Lizenz genannt). Beides
für Oracle-Gebrauch unschädlich; bei einem späteren Port
entscheidungsbedürftig.

**Umgekehrte Richtung — UFT trägt bereits libdsk-Attributionen, und
das ist ein eigener Befund** (s. Abschnitt 5, SCOUT-62).

## 3. Oracle-Tauglichkeit — die drei Fragen aus `docs/ORACLES.md`, mit Beleg

**Alles unten wurde gebaut und ausgeführt, nicht geschätzt.**
Autotools/`make` fehlen auf dieser Maschine (`which make` → leer;
nur `mingw32-make`); `./configure` wurde deshalb **nicht** versucht.
Stattdessen Hand-Bau mit MinGW gcc 13.1.0, vollständig
reproduzierbar:

```
# minimales config.h von Hand (22 Zeilen, HAVE_WINDOWS_H ja,
#   HAVE_WINIOCTL_H NEIN -> keine Win32-Floppy-Gerätetreiber nötig)
gcc -O1 -DNOTWINDLL -I. -I include -I lib -c <alle lib/*.c außer
  drvdos16 drvdos32 drvint25 drvlinux drvntwdm drvwin16 drvwin32
  dskjni compgz compbz2 rpcfork rpctios rpcfossl rpcserv>
# -> 0 Fehler. ar rcs libdsk.a *.o
gcc -O1 -DNOTWINDLL ... tools/{dskid,dsktrans,dskform}.c \
    tools/{utilopts,formname,bootsec}.c libdsk.a -> alle drei OK
```

Stolpersteine, dokumentiert für die Registrierung: ohne `-DNOTWINDLL`
erwartet `include/libdsk.h:40-48` DLL-Import (`__imp_`-Linkfehler);
mit `HAVE_WINIOCTL_H` schaltet `lib/drvi.h:64-66` die Win32-
Gerätetreiber zwangsweise ein.

**(a) Konsole ohne GUI?** **Ja, gemessen.** Alle drei Binaries laufen
headless; `dskid --version` und `dsktrans --version` antworten
`libdsk version 1.5.12` (Version abfragbar). Binary-Pins des
Messlaufs (sha256, gekürzt): `dskid` `6a2595886e3c1e65ef223…`,
`dsktrans` `dab4f5c3d7a7d4fc66075…`, `dskform` `caf9dc416d1b6abe…`.
Hinweis: Fortschrittsanzeige geht nach stderr und ist gesprächig —
im Differenzskript stderr verwerfen.

**(b) Hashes/Inhalte je Datei/Sektor?** **Je Sektor ja, je Datei
nein — und das ehrlich benannt:** libdsk hat **keinen**
Dateisystem-Layer (der liegt bei cpmtools). Es ist ein
**Sektor-Oracle**: `dsktrans <fmt>→raw` normalisiert jeden Container
auf rohe Sektorbytes, die dann selbst gehasht werden — das ist die
Stufe „stärker" des Differenzlauf-Standards (Bytes herausholen und
selbst hashen), nur eben je Abbild/Spur/Sektor statt je FS-Datei.
Gemessener Rundlauf: 184 320-Byte-Zufallsdatei (Python
`random.seed(42)`, sha256
`f6212db79c64394105cafd414b0a9ca5e5ac0fcc99bb0b35a37e6d7039866e77`)
→ `dsktrans -itype raw -format pcw180 -otype edsk` → zurück nach raw
→ **byteidentisch** (`cmp` grün). `dskid` erkennt das Erzeugnis
selbstständig als „Extended .DSK driver", Geometrie vollständig
ausgewiesen.

Schreib+Rundlauf-Matrix (Methode: je `-otype` einmal seed-raw
hinschreiben, zurücklesen, `cmp` gegen Seed; alles mit
pcw180-Geometrie):

| WRITE+ROUNDTRIP **byteidentisch** (13) | Rücklauf NICHT identisch (3) |
|---|---|
| dsk, edsk, imd, tele (TD0), qm (CopyQM), qrst, jv3, ldbs, apridisk, ydsk, myz80, nanowasp, logical | dc42, sap, cfi — Ursache **nicht untersucht** (vermutlich Geometrie-Zwang des Formats, DC42 ist Mac-GCR, SAP Thomson; als UNGEKLÄRT geführt, nicht als Defekt behauptet) |

**(c) Unabhängig von der Hand, die unseren Korpus erzeugt hat?**
**Ja.** `inv["korpus"]` (22 Einträge) führt als Erzeuger VICE c1541,
amitools/xdftool, atrcopy, greaseweazle und historische Aufnahmen —
kein Eintrag stammt aus libdsk, und **kein einziges** der von libdsk
bedienten Formate (dsk/edsk/imd/td0/cqm/…) liegt überhaupt im Korpus
(Methode: Format-Feld aller 22 Einträge aufgezählt: d64, adf, atr,
xfd, fdi, d71, d81, hfe, scp, g64, d67, d80, d82, g71 +
protection:copylock_st). Eigene C-Codebasis (John Elliott, seit
2001), keine Verwandtschaft zu ADFlib/atrcopy/VICE. Die fünfte Frage
(„dieselbe Hand?", MF-644) ist bei seed-erzeugtem Korpus zusätzlich
entschärft: Ground Truth ist der **Seed-Bytestrom selbst**, nicht
libdsks Ausgabe — liest UFT den Container, muss der Seed
herauskommen, egal wer den Container schrieb. Für die Fälle, wo
libdsks Container-**Schreiber** selbst falsch sein könnte, steht mit
`floptool` (MAME, bereits begutachtet 2026-08-27) eine zweite Hand
für dsk/imd/td0 bereit.

## 4. Die Geometrie-Frage aus MF-648 — **Entlastungsbefund**

Frage des Eigentümers: unterscheidet libdsk Xerox 820 / Cromemco /
Hitachi S1 / DG Nova / VersaDOS (alle 77×26×1×128 = 256 256 B), und
woran?

**Antwort: libdsk unterscheidet sie nicht — es kennt sie nicht
einmal, und es rät grundsätzlich nie aus der Dateigröße.** Drei
Messungen:

1. **Namenssuche über den ganzen Baum:**
   `grep -rin "xerox|cromemco|hitachi|nova|versados|3740"` über alle
   `.c/.h/.txt/.html/.sample` → **0 Treffer**.
2. **Die eingebaute Geometrie-Tabelle** (`lib/dsksgeom.c:39-84`,
   36 benannte Einträge, plus `doc/libdskrc.sample`) enthält **keine
   einzige 8"-Geometrie mit 26×128** — kein IBM-3740-Eintrag, unter
   keinem Namen. (Einzige 128-Byte-Zeile ist `myz80`, ein
   8-MB-Festplattencontainer, `dsksgeom.c:78`.)
3. **Verhaltensprobe:** 256 256-Byte-Zufallsdatei (seed 7, sha256
   `a7d48c0240cf8ad1…`) → `dskid ibm8.raw` → **„Bad format."**
   libdsk lehnt ab, statt einen Systemnamen zu erfinden.

Wie libdsk stattdessen identifiziert (`lib/dskgeom.c`,
`dsk_defgetgeom` :236ff): erst Sektor-IDs lesen (Datenraten-Probe),
dann **Boot-Sektor-INHALT** — DOS-BPB (`dg_dosgeom` :449), PCW/CPC-
Spec-Byte (`dg_pcwgeom` :457), Apricot (`dg_aprigeom` :463), Opus
(`dg_opusgeom` :472), BBC-DFS-Katalog (`dg_dfsgeom` :479), CP/M-86
(`dg_cpm86geom` :485). Was der Inhalt nicht hergibt, muss der
Benutzer mit `-format <name>` **selbst benennen**. Größe allein ist
nie ein Urteil.

**Konsequenz für SCOUT-47:** Die Referenz-Abstraktionsschicht des
CP/M-Ökosystems — genau die Nische, in der Xerox 820 und Cromemco
leben — sieht keinen Weg, diese Systeme aus einem Sektor-Abbild zu
unterscheiden, und versucht es nicht. UFTs ehrliche Sammelzeile
(„IBM-3740-kompatibel, System aus dem Abbild nicht unterscheidbar")
ist damit nicht nur zulässig, sondern Stand der Referenzpraxis.
Wer es doch je unterscheiden will, muss es libdsk-artig über
**Inhalt** tun (Boot-/Katalog-Sektor), nie über die Größe.

## 5. Was das Inventar sagt — und der Befund im eigenen Baum

Inventar-Abfrage (zitiert, `inventar.py query work/inv.json …`):

```
edsk:     vorhanden=true  tier=T3
jv3:      vorhanden=true  tier=T3
apridisk: vorhanden=true  tier=T3
myz80:    vorhanden=true  tier=T3
nanowasp: vorhanden=true  tier=T3
logical:  vorhanden=true  tier=T3
qrst:     vorhanden=true  tier=T3
cfi:      vorhanden=true  tier=T3
mgt:      vorhanden=true  tier=T3
opus:     vorhanden=true  tier=T3
rcpmfs:   vorhanden=true  tier=T3
posix:    vorhanden=true  tier=T3
imd:      vorhanden=true  tier=T2
td0:      vorhanden=true  tier=T2
cqm:      vorhanden=true  tier=T2
dc42:     vorhanden=true  tier=T2
ldbs:     vorhanden=true  tier=null (uft_ldbs, nicht in Tier-Liste)
```

**Zwölf T3-Formate in UFT sind libdsk-Abkömmlinge.** Messung im
eigenen Baum: `grep -rin "libdsk" src include` → **31 Dateien**
(15 `src/formats/*/…c` + 16 Header). Die Köpfe sagen wörtlich
„Reference: libdsk drvapdsk.c by John Elliott",
„Reference: libdsk drvposix.c" usw. — **keiner nennt die Lizenz der
Quelle** (Stichprobe 8 Köpfe gelesen; einzig `uft_cqm.c:348` trägt
einen RE-Status-Vermerk). Zwei referenzieren Treiber, die es in
diesem Fork gar nicht gibt (`drvmgt.c`, `drvopus.c` — erst in
neuerem Upstream). Das ist exakt die MF-636-Klasse „genannte fremde
Codebasis ohne genannte Lizenz" — und dieser Zyklus hat die fehlende
Lizenz jetzt **bestimmt**: LGPL-2.0-or-later (Abschnitt 1), Zone
GRÜN, also billig heilbar.

Pikant und nützlich zugleich: dieselbe Fremdbibliothek, deren
Attributionsschuld im Baum liegt, ist zugleich das **einzige
bekannte Werkzeug, das für diese 12-Format-Gruppe hardwarefrei
Referenz-Abbilder schreiben kann.**

---

## Vorschläge (Ratenbremse: 5; Nummern **vorläufig** ab SCOUT-59, da drei Zyklen parallel laufen)

### SCOUT-59 (vorläufig) — libdsk-Trio als Oracle registrieren, T3-Gruppe heben
**Kennzahl: ungeprüfte Formate (T3) runter — bis zu 7 auf einmal.**
`dsktrans`/`dskid`/`dskform` (gebaut, gepinnt: Version abfragbar
`libdsk version 1.5.12` + sha256 der Binaries oben) in
`tests/differential/oracles.py` + `docs/ORACLES.md` eintragen.
Deckt von den 12 libdsk-stämmigen T3-Formaten **7 mit gemessenem
Schreib+Rundlauf**: `edsk`, `jv3`, `apridisk`, `myz80`, `nanowasp`,
`logical`, `qrst` (Methode: Schnittmenge der T3-Zeilen aus
`docs/VERIFICATION_TIERS.md:58-94` mit der Rundlauf-Matrix aus
Abschnitt 3b). Rezept je Format: seed-raw (Ground Truth = Seed) →
`dsktrans raw→<fmt>` → UFT liest → Sektorbytes müssen dem Seed
gleichen; plus Gegenrichtung UFT-schreibt → `dsktrans <fmt>→raw` →
`cmp`. Korpus entsteht dabei kostenlos und ohne Hardware (MF-310
gewahrt). Einhängepunkt: `docs/plans/UMSETZUNGSLISTE.md` Stufe B
(ein Bau nötig) + `docs/ORACLES.md` Registrierungsregel.
Aufwand: **S-M** (Bau reproduzierbar dokumentiert; je Format ein
Fixture + ein Differenztest).

### SCOUT-60 (vorläufig) — TD0/CQM/IMD-Zweithand-Korpus (die seltenen Schreiber)
**Kennzahl: Tier-Leiter (T2→T1b für `td0`, `cqm`, `imd`).**
Schreibfähige TeleDisk- und CopyQM-Implementierungen sind rar —
libdsk hat beide, und beide liefen hier **byteidentisch im Kreis**
(`o.tele` 187 736 B, `o.qm` 184 533 B, Hashes im Messlauf). Damit
bekommen die drei T2-Formate erstmals Korpus-Abbilder aus
unabhängiger Hand (heute: null Einträge im Korpus für alle drei) und
einen Differenzpfad gegen die bestehenden UFT-Leser. Für TD0
zusätzlich `floptool` (MAME-Gutachten) als dritte Hand verfügbar —
zwei unabhängige Fremde, stärker als der D64-Standard. Aufwand: **S**.

### SCOUT-61 (vorläufig) — Entlastungsbefund als Nachtrag zu SCOUT-47 übernehmen
**Kennzahl: ungeprüfte Formate (T3) runter (die fünf 256 256er-Zeilen).**
Der Befund aus Abschnitt 4 gehört wörtlich in die SCOUT-47-Akte
(`docs/OPEN_ITEMS.md:2477-2506`): die Referenz-Bibliothek des
CP/M-Ökosystems führt keine der sechs Systemzuschreibungen, führt
überhaupt keine 26×128-Geometrie und urteilt nie nach Dateigröße
(drei Messungen, oben). Das entscheidet die SCOUT-47-Abwägung
zugunsten der **Sammelzeile** — sofern nicht je System eine
Primärquelle beigebracht wird. Aufwand: **S** (Doku-Entscheid).

### SCOUT-62 (vorläufig) — 31 libdsk-Attributionen heilen (Lizenz jetzt bestimmt)
**Kennzahl: fünfte Zahl — Dateien mit ungeklärter Herkunft, runter.**
15 Quell- + 16 Header-Dateien nennen libdsk als Referenz ohne dessen
Lizenz (Messung Abschnitt 5). Dieser Zyklus liefert das fehlende
Stück: **LGPL-2.0-or-later, 134/150 Dateien, Wortlaut zitiert** —
Zone GRÜN, GPL-2.0-kompatibel. Je Datei ist nur noch zu entscheiden:
Port (→ SPDX `LGPL-2.0-or-later` + Quelle+Version in den Kopf, nach
dem samdisk-Muster) oder eigenständige Implementierung nach Doku
(→ Formulierung nach MF-636 korrigieren). Achtung Detailfalle:
`uft_ldbs.c` referenziert `drvldbs.c` (LGPL), der LDBS-**Kern**
`ldbs.c` ist dagegen MIT — je nachdem, woraus wirklich abgeleitet
wurde, unterscheidet sich der korrekte SPDX. Speist `LIZ-1`
(48 Verdachtsfälle) direkt. Aufwand: **M** (31 Dateien, aber
mechanisch; Eigentümer entscheidet nur die Port-vs-Spec-Frage).

### SCOUT-63 (vorläufig) — Verhaltens-Spec „Geometrie aus Inhalt, nie aus Größe"
**Kennzahl: ungeprüfte Formate (T3) runter (Zuarbeit `dsk_generic` + `mfm_detect`-Mehrdeutigkeit, gleiche Klasse wie SCOUT-47).**
`lib/dskgeom.c` (924 Zeilen, LGPL-2.0-or-later) ist eine seit 2001
gereifte Probe-Kette: Sektor-ID → Datenrate → Boot-Inhalt (DOS-BPB,
PCW-Spec-Byte, Apricot, Opus, BBC-DFS-Katalog, CP/M-86), Größe
niemals als Urteil. Als Verhaltens-Spec (Template `templates/spec.md`)
für die Sanierung der größenbasierten Mehrfach-Zuschreibungen in
`src/formats/dsk_generic/` — die Spec beschreibt die
Entscheidungsreihenfolge, Stufe 4 belegt jede Regel gegen
Primärquellen (Rotbeweis zuerst). Kein Code-Port nötig (Zone wäre
sogar GRÜN, aber die UFT-Probe-Architektur ist eine andere).
Aufwand: **M**.

**Fundus (bewegt heute keine Zahl, nicht eingeplant):** LDBS als
Differenz-Drehscheibe (jedes libdsk-Format ↔ UFTs `uft_ldbs`-Plugin;
erst sinnvoll, wenn `ldbs` eine Tier-Zeile hat) · `ydsk`/`simh`-
Header ohne Plugin (`include/uft/formats/uft_ydsk.h`, `uft_simh.h`
existieren, `src/formats/ydsk|simh` nicht — möglicher „türlos"-Fall,
gehört zur SCOUT-39-Klasse, hier nur notiert) · Upstream-1.5.22-
Backends (CP2, D64CPM, D88, LDBST) — neue Formate = Moratorium ·
`rcpmfs`-Konzept (Verzeichnis als virtuelle CP/M-Diskette) als
Test-Idee.

## Beschaffungsliste

**Leer.** Alles Benötigte entsteht seed-basiert auf dieser Maschine;
`inv["korpus"]` wird um nichts gebeten, Hardware ist nicht im Spiel
(MF-310). Einzige optionale Beschaffung, Eigentümer-Entscheid: der
Upstream-Tarball `libdsk-1.5.22` von seasip.info, falls die
Oracle-Registrierung lieber die gepflegte Fassung pinnt als diesen
2015er-Fork (dann Bau-Rezept erneut messen).

## Differenzlauf-Plan (Pflicht, da „Referenz"-Anspruch)

- **Binaries:** `dsktrans`/`dskid` 1.5.12 (Pins oben) vs. UFT-Kern
  (Testtreiber über `uft_disk_open()`, kein CLI — feedback_no_cli).
- **Korpus:** je Format 2 Abbilder: seed-raw 184 320 B (pcw180) und
  ein zweites mit formatgerechter Geometrie (z. B. 40×1×18×256 für
  Opus, falls Upstream-Fassung); Seeds und Hashes im Manifest.
- **Metrik:** sha256 der konkatenieren Sektorbytes in
  CHS-Reihenfolge; Zusatzfeld Geometrie (Zyl/Köpfe/Sekt/Größe) aus
  `dskid` vs. UFT-Probe.
- **Toleranzliste:** Füllbytes hinter der Nutzgeometrie (myz80-
  Container ist 5 243 136 B fix); Kompressionscontainer (tele/qm)
  vergleichen dekomprimiert, nie Containerbytes; `dc42`/`sap`/`cfi`
  ausgenommen, bis der Rundlauf-Rotpunkt geklärt ist.

## UNGEKLÄRT

1. **dc42/sap/cfi-Rundlauf rot** mit pcw180-Geometrie — Geometrie-
   Zwang des Formats oder Treiberfehler? Nicht untersucht; vor einer
   Oracle-Nutzung dieser drei Formate klären.
2. **Fork vs. Upstream:** 1.5.12 (dieser Fork, eingefroren
   2015/2017) vs. 1.5.22 (seasip.info). `drvmgt.c`/`drvopus.c` (für
   UFTs `mgt`/`opus`-T3-Hebung nötig) existieren nur upstream.
   Eigentümer-Entscheid, welche Fassung gepinnt wird.
3. **`lib/ldbs.c:184,237`** castet Pointer↔`long` (Win64: 4-Byte-
   long!) — Compiler-Warnung im Messlauf; unser LDBS-Rundlauf lief
   trotzdem grün; ob der Pfad nur zufällig heil ist, nicht
   untersucht. Bei LDBS-Oracle-Nutzung auf Win64 vorher prüfen.
4. **`edsk`-Plugin (amstrad/) vs. `dsk_cpc` (T2)** — zwei
   UFT-Fassungen desselben Formats (`VERIFICATION_TIERS.md:43,69`);
   welche die Tür bekommt, ist eine SCOUT-43-artige Frage und hier
   nicht entschieden.
5. **`crc16.c`/`crctable.c` ohne Lizenz-Kopf** — nur relevant, falls
   je ein Port erwogen wird; dann Eigentümer-Vorlage.
6. **`dskform`-Systemspuren:** `dskform -format cpcdata` erzeugt
   194 816-B-DSK mit CP/M-Formatierung (gemessen, `dskid` erkennt
   sie); ob die PCW-Boot-Spec-Bytes für UFT-Filesystem-Tests taugen,
   nicht geprüft.

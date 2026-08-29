<!-- uebernommen: MF-678 -->
<!-- Befunde nach docs/OPEN_ITEMS.md, SCOUT-26 uebertragen: getrennt
     nach Fehlern gegen benannte Referenzen (kein Vorlagenbedarf) und
     echten Eigentuemer-Vorlagen (Lizenz/Verteilung betroffen). -->
<!-- stufe: 3 — Tiefenprüfung abgeschlossen (Zyklus Atari-ST-Werkzeuge, 2026-08-29) -->
# Gutachten: Atari-ST-Werkzeuge — Jacknife, atari-st-tools, st2disk

Stand: 2026-08-29 · Inventar: `work/inv.json` (UFT HEAD `18fe2bcc`,
88 Plugins aus SSOT, 22 Korpus-Abbilder) · Messungen:
`work/Jacknife.messung.json`, `work/atari-st-tools.messung.json`,
`work/st2disk.messung.json` · Ersetzt die drei mechanischen Entwürfe
gleichen Datums (gelöscht, Inhalt hier aufgegangen).

**Anlass des Zyklus:** UFT führt `st`, `stx`, `msa` (alle T2) und
`dim_atari` (T3); `tests/corpus/` enthält echte ST-Kopierschutz-Samples
(dec0de, MF-377). Kein Repo dieses Zyklus steht in
`data/known_negatives.json` (vor dem Lauf geprüft: 51 Einträge, keiner
der drei Namen darunter).

---

## Repo 1: ggnkua/Jacknife (HEAD `97ce2a49`, 2026-08-25)

- **Kategorie:** Oracle + Verbesserung (Referenzbeschreibung) + Fundus
- **Was es ist:** Total-Commander-WCX-Plugin (C, 2914 Zeilen eigener
  Code: `dllmain.c` 2346, `SamariTan.c` 448, `jacknife.h` 120) plus
  eigenständiges CLI **SamariTan**. Liest/schreibt .ST/.MSA, liest
  FastCopy-/E-Copy-.DIM und AHDI-Hard-Disk-Images (.AHD). FAT12 über
  vendortes dosfs-1.03.

### Lizenz (aus Dateien, nie aus dem README)

| Teil | Beleg | Zone |
|---|---|---|
| Eigencode (dllmain.c, SamariTan.c, jacknife.h) | **keine LICENSE-Datei im Repo**, keine Lizenz in den Dateiköpfen (geprüft: `head -40 dllmain.c`, `head -60 SamariTan.c` — nur TODO-Kommentare) | **ROT** — alle Rechte vorbehalten |
| `dosfs-1.03/` (vendort) | eigene Lizenz in `readme.txt:43-82`, custom-permissiv („you are hereby licensed to use the DOSFS code in any application … not required to disclose sourcecode") | permissiv, aber Nicht-Standard-Text → bei Portierabsicht PRÜFEN |
| PCG32-Schnipsel | `readme.md:122`: „Licensed under Apache License 2.0" | GELB |
| SdFat-Schnipsel | `readme.md:123-132`: MIT-Text wörtlich | GRÜN |

**Konsequenz Zone ROT:** kein Code-Port, kein Vendoring. Verhaltens-Spec
aus Lektüre und **Vergleich der Ausgabe** sind zulässig. Oracle-Binary:
die offiziellen Releases (geprüft `gh api …/releases`, v0.10-v0.12)
enthalten **nur** die WCX-Plugin-Binaries, **kein** SamariTan-CLI —
der CLI-Weg erfordert Selbstbau aus Quelle ohne Lizenz-Grant →
**Eigentümer-Vorlage** (unten, U-1).

### Oracle-Tauglichkeit SamariTan (die drei Fragen aus docs/ORACLES.md, gemessen)

Gebaut und gelaufen — kein Urteil auf Zusicherung:

1. **Konsole ohne GUI, Rückgabewert?** **Ja.** `SamariTan.c:60-70`
   (Modi CREATE/ADD/DELETE/EXTRACT/TEST/UNDELETE). Gemessen:
   `samaritan -v test.msa` → rc=0; korrupte Datei mit gültigem Magic →
   „Can't open the image file" + rc=127; `-c`/`-x` → rc=0.
2. **Inhalte je Datei?** **Ja — stark.** `-x` extrahiert Dateien;
   `cmp` gegen die Originale: HELLO.TXT, RAND.BIN (3000 B Zufall),
   NESTED.TXT **byte-identisch** aus .ST, .MSA **und** synthetischem
   .DIM. Kein bloßes Listing (MF-629-Schwelle übersprungen).
3. **Unabhängige Hand?** **Ja.** FAT-Kette dosfs (Lewin Edwards) +
   Eigencode ggnkua/tin-nl; keine Verwandtschaft zu SAMdisk (unsere
   T2-Referenz für st/msa), Hatari, msa-to-zip oder irgendeinem
   Werkzeug, das UFT-Korpusmaterial erzeugt hat.

**Bau-Befund (ehrlich):** Upstream baut auf MinGW 13.1.0 **nicht**
unverändert: `dllmain.c:2321` ruft POSIX-`mkdir(path, 0765)` (2 Argumente),
MinGW-`io.h:282` deklariert `mkdir(const char*)` (1 Argument) → Compile-
Fehler. Lokaler Messbau mit dokumentiertem 1-Zeilen-Patch (nur im Klon,
`work/Jacknife/dllmain.c`, als `LOCAL SCOUT PATCH` markiert); die
`build_linux_mac.sh`/MSVC-Wege sind davon nicht betroffen. Für eine
Registrierung wäre ein Upstream-Issue der saubere Weg.

**Fünfte Frage (MF-644, dieselbe Hand?):** Wer SamariTan-erzeugte
Abbilder als Korpus nimmt UND SamariTan als Oracle, prüft Zirkel.
Auflösung im Rezept unten: das DIM-Fixture wird von **uns** nach der
wörtlich zitierten Hatari-Spec synthetisiert (Hand 1), SamariTan urteilt
(Hand 2), UFT ist Prüfling — drei Hände, kein Zirkel.

### Befund J-1 — `dim_atari` (T3): unser Probe prüft das Magic nicht, das beide unabhängigen Referenzen prüfen

Drei Beschreibungen desselben Formats (FastCopy-Pro-DIM):

| Frage | UFT `src/formats/dim_atari/uft_dim_atari.c` | Jacknife `dllmain.c` | Hatari `src/floppies/dim.c` (wörtlich, GPL-2.0 „version 2 or at your option any later version", Kopf Z.4-5) |
|---|---|---|---|
| Magic `0x4242` „BB" @0x00 | **fehlt** — `grep -c "0x4242\|'B'" uft_dim_atari.c` = **0** | geprüft, `dllmain.c:591` `== 0x4242` | geprüft, `pDimFile[0x00] != 0x42 \|\| pDimFile[0x01] != 0x42` |
| used-sectors-Flag @0x03 | im Kopfkommentar (`:12`) genannt, **im Code nirgends gelesen** (kein `[3]`/`[0x03]`-Zugriff) | gelesen (`get_sectors`), Image wird per FAT12-Lauf expandiert (`expand_dim`, `dllmain.c:439-527`) | gelesen, used-sectors-Images werden **abgewiesen** |
| Geometrie-Felder | Bytes @0x06/0x08/0x0A/0x0C/0x0D (`:60-64`) | Struct-Worte @6/8/0xA/0xC, **ohne** Byteswap gelesen (`dllmain.c:600-602`) — auf Little-Endian numerisch äquivalent zu Hatari-Bytes, solange das Folgebyte 0 ist | Bytes @0x06/0x08/0x0A/0x0C/0x0D (Kopfkommentar Z.33-46, wörtlich) |

**Gute Nachricht zuerst:** unsere Offset-Belegung stimmt mit Hatari
überein — `dim_atari` ist **nicht** fabriziert. Aber: (a) ohne
Magic-Prüfung akzeptiert der Probe jede Datei mit passender Größe
(`:134-138`: Größen-Match ⇒ confidence 80-90), und die ganze
X68000-Abgrenzungslogik (`:103-130`) leistet mühsam, was zwei Bytes
trivial leisten; (b) ein used-sectors-DIM wird nur **zufällig** durch
den strikten Größenvergleich abgewiesen, nicht erkannt; (c) der Header
nennt als Referenz nur „FastCopy Pro documentation, Atari ST
preservation community" (`:33`) — unbenannt im Sinne von MF-498.

**Fixture-Rezept (im Zyklus vollzogen, reproduzierbar):** 32-Byte-Kopf
nach Hatari-Spec (`'BB'`, auto=1, used=0, sides@0x06=1, spt@0x08=9,
start@0x0A=0, end@0x0C=79, dens@0x0D=0, BPB-Kopie ab 0x0E aus
Bootsektor-Offset 0x0B) + ST-Inhalt. Ergebnis (737 312 B) von SamariTan
verifiziert (rc=0) und je Datei byte-identisch extrahiert.

### Befund J-2 — MSA: dritte unabhängige Beschreibung, deckungsgleich

Siehe Abschnitt „MSA-Konvergenz" unten (repoübergreifend).

### Fundus (bewegt keine Kennzahl, nicht vorgeschlagen)

- **AHD/AHDI-Partitionsschemata** (AHDI 3.00 + ICD, `readme.md:23`):
  Inventar-Abfrage `ahd` → `vorhanden: false, abgedeckt: false` mit
  Pflicht-Handprüfung; Handprüfung: `grep -ril "ahd" src/formats/
  include/uft/formats/` = 0 Treffer. Neues Format ⇒ Einfrier-Regel ⇒
  Fundus.
- **used-sectors-DIM-Expansion** über FAT12-Kettenlauf
  (`dllmain.c:439-527`, inkl. Toleranz für am Ende abgeschnittene
  Dateien, `:433`) — Fähigkeit, die UFT fehlt; gehört in die
  dim_atari-Spec als dokumentierte Variante, Implementierung erst nach
  Moratoriumsregeln.

---

## Repo 2: suiryc/atari-st-tools (HEAD `3cc0aaf2`, 2016-12-14)

- **Kategorie:** Verbesserung (Referenzbeschreibung) / Oracle-Kandidat **gescheitert am Bau**
- **Was es ist:** Scala-CLI (28 Dateien) — Konvertierung ST↔MSA (auch in
  ZIP), Deduplizierung über Checksummen (`DiskInfo.scala:29-32`:
  MessageDigest-Checksumme des Abbilds, zweite ohne Bootsektor),
  Normalisierung, Test-Modus.

### Lizenz

`LICENSE` = GPL-3.0-Volltext („Version 3, 29 June 2007"). **Kein**
Dateikopf im Quelltext trägt einen eigenen Grant (geprüft:
`grep -rn "Copyright\|License\|GPL" src/` → 0 Treffer); der einzige
„any later"-Treffer im LICENSE (Z.640) ist der Boilerplate-Anhang „How
to Apply", nicht die Erklärung des Autors. Damit gilt die Repo-Lizenz
ohne only/or-later-Präzisierung des Autors: **GPL-3.0 → Zone GELB**.
Konsequenz: kein Code-Port in ein GPL-2.0-Projekt; Verhaltens-Spec und
Oracle-Binary zulässig.

### Oracle-Tauglichkeit: **nicht erfüllt — Bau gescheitert, Fehler benannt**

`sbt` ist auf dem Messrechner nicht vorhanden (`which sbt` leer).
Schwerer wiegt: `build.sbt` verlangt
`"suiryc" %% "suiryc-scala-core" % "0.0.2-SNAPSHOT"` (Versionstabelle
Z.12, Dependency Z.50) und bietet als Auflösung `Resolver.mavenLocal`
(Z.41) — ein **SNAPSHOT der Privatbibliothek des Autors**, der nur
durch historisches Auschecken und lokales Publishen eines vierten Repos
beschaffbar wäre, unpinnbar im Sinne von ORACLES.md Frage 3.
Scala 2.12.1 / sbt-assembly 0.14.1 (2016) auf JVM 8/21 ist zusätzliches
Risiko. **Kein Oracle-Eintrag.** Der Wert des Repos ist die vierte
unabhängige MSA-Beschreibung (unten) — die ist durch Lektüre gesichert
und braucht keinen Bau.

---

## Repo 3: vinz6751/st2disk (HEAD `b99a2135`, 2020-01-20)

- **Kategorie:** **irrelevant** für UFT
- **Was es ist:** 360-Zeilen-Pure-C-1.1-Programm **für den Atari ST
  selbst** (GEM/TOS): schreibt .ST-Abbilder per XBIOS `Flopwr`
  (`src/st2disk.c:239`) auf eine physische Diskette im ST-Laufwerk.
- **Lizenz:** keine Lizenzdatei; `readme.txt` bittet nur um Historie im
  Readme → **Zone ROT**.
- **Urteil:** Läuft ausschließlich auf Atari-Hardware (MF-310: keine
  Hardware-Vorschläge), auf PC weder baubar (Pure C) noch nützlich,
  liest kein Format, das UFT fehlt, enthält keine Formatbeschreibung
  über das hinaus, was `st`/T2 schon belegt. Kein Fund. Ein Zyklus-Teil
  ohne Fund ist ein gültiges Ergebnis.

---

## MSA-Konvergenz: vier unabhängige Beschreibungen, ein Verhalten

**Methode (MF-615):** verglichen wurden die Menge der
Header-/RLE-Semantikpunkte aus UFT (`src/formats/msa/uft_msa.c:66-70,
127-158, 301-345` + Plugin `src/formats/msa/uft_msa_plugin.c:12-26`)
mit denselben Punkten in Jacknife (`dllmain.c:302-380`) und
atari-st-tools (`MSADisk.scala:16-108`); SAMdisk ist als bestehende
T2-Referenz (MF-460) gesetzt. Ein Punkt zählt als Übereinstimmung, wenn
alle Implementierungen bei gleicher Eingabe dieselbe Ausgabe bzw.
Ablehnung erzeugen.

**7 Übereinstimmungen:** Magic 0x0E0F (BE) · alle 5 Kopffelder BE16 ·
Seiten = Feld+1 · Endspur inklusiv · Spursatz: BE16-Länge, Länge ==
Spurgröße ⇒ unkomprimiert · RLE `$E5 <data> <BE16-count>` · Dekodier-
schleife läuft eingabelängen-getrieben über exakt die komprimierte
Spurlänge.

**3 Divergenzen (keine davon ein Fehler bei uns, aber spec-würdig):**

1. **Grenzen:** UFT verwirft spt<9 oder >11 und end_track≥86
   (`uft_msa.c:85,94`); Jacknife und atari-st-tools prüfen keine
   Obergrenzen. Zusätzlich intern: das Plugin akzeptiert spt bis 18
   (`uft_msa_plugin.c:103`), der Probe aus `uft_msa.c` nur 9-11 — zwei
   Grenzwerte im selben Baum.
2. **Partielle Abbilder** (start_track>0): UFT und atari-st-tools
   akzeptieren, Jacknife verwirft (`dllmain.c:311-314`, bewusst: FAT
   läge außerhalb).
3. **Abgeschnittene RLE-Sequenz am Spurende:** `uft_msa.c:143` →
   Fehler TRUNCATED; `uft_msa_plugin.c:42` behandelt ein `$E5` mit <3
   Restbytes als **Literal** — zwei RLE-Dekoder mit verschiedener
   Randsemantik im selben Baum.
4. *(Schreibseite, nur Konsistenznotiz)*: UFT fällt korrekt auf
   unkomprimiert zurück, wenn RLE nicht **kleiner** wird
   (`uft_msa.c:345` `comp_len < track_size`) — damit ist die
   Mehrdeutigkeit „komprimierte Länge == Spurgröße" konstruktiv
   ausgeschlossen. Beide Fremdleser lesen genau diese Konvention.

**Bedeutung für `LIZ-1`:** `uft_msa.c:10` erklärt „Based on msa-to-zip
(Olivier Bruchez) - Algorithm extraction" **ohne Lizenz** — einer der 14
offenen LIZ-1-Fälle (`docs/OPEN_ITEMS.md:2014`). Die Konvergenz von
vier unabhängigen Codebasen auf identische Semantik ist ein starkes
Indiz, dass der Inhalt **Formatwissen** ist, nicht Ausdrucksübernahme —
dasselbe Muster, mit dem HxC freigesprochen wurde (MF-650,
Idiom-Audit in beide Richtungen). Zusätzlich gemessen: die attribuierte
Codec-API in `uft_msa.c` hat außerhalb von `tests/test_msa.c` **keinen
Produktionsaufrufer** (`uft_msa_probe` ist der einzige Export mit
Plugin-Aufrufer; `uft_msa_to_st`/`uft_st_to_msa`: **0 Aufrufer**,
grep über src/tests) — der Produktionspfad läuft über den
SAMdisk-verifizierten Eigenparser in `uft_msa_plugin.c`.

## STX/Pasti: ausdrücklich geprüft, negativ

`grep -rniE "stx|pasti"` über die Quellen aller drei Repos: **0
Treffer.** Keines liest STX. Der Quarantäne-Neubaupfad für
`src/formats/stx/uft_stx_air.c` (`docs/QUARANTINE.md:41`, Weg 2)
bekommt aus diesem Zyklus **keinen** Baustein. Die T2-Quelle
(Pasti-Spec, `docs/VERIFICATION_TIERS.md:53`) bleibt der Stand.

---

## Inventar-Abfragen (zitiert)

```
msa  → vorhanden: true  (msa, st_msa, uft_msa)         Tier T2
st   → vorhanden: true  (st, st_msa, …)                Tier T2
stx  → vorhanden: true  (stx)                          Tier T2
dim  → vorhanden: true  (dim, dim_atari)               Tier T3
ahd  → vorhanden: false, abgedeckt: false → Handprüfung: 0 Treffer im Baum
fat12→ vorhanden: true  (uft_fat12)
```
`plugin_liste_vollstaendig: true` bei allen Formatnamen-Abfragen.
Korpus-Abgleich: unter den 22 Abbildern in `inv["korpus"]` ist **kein**
.st, .msa oder .dim — die Beschaffungslisten unten fordern nichts an,
was schon liegt.

---

## OPEN_ITEMS-Vorschläge (3 von max. 5 — Nummern vorläufig ab SCOUT-76)

### SCOUT-76 — `dim_atari` von T3 heben: Spec aus Hatari (wörtlich) + Magic-Rotbeweis + synthetisches Fixture

**Kennzahl: ungeprüfte Formate (T3) runter** (−1).
`dim_atari` steht auf T3 ohne Beleg (`docs/VERIFICATION_TIERS.md:65`).
Dieser Zyklus liefert: (a) autoritative Referenz — Hatari
`src/floppies/dim.c`, GPL-2.0-or-later, Header-Layout wörtlich im
Gutachten; (b) Rotbeweis-Kandidat — ein Nicht-DIM mit passender Größe
und Byte 0 ≠ 'B' wird vom heutigen Probe akzeptiert
(`uft_dim_atari.c`: 0 Vorkommen von 0x4242), beide unabhängigen
Referenzen prüfen das Magic; dazu das ungelesene used-sectors-Flag
@0x03; (c) Fixture-Rezept, im Zyklus vollzogen und von SamariTan
gegengeprüft. Stufe-4-Weg: Rotbeweis zuerst (Probe akzeptiert
Magic-loses File), dann Magic- und Flag-Prüfung mit Hatari-Referenz im
Header, Fixture + Test nach Rezept. Aufwand **S**.
Einhängepunkt: `docs/VERIFICATION_TIERS.md` Zeile `dim_atari`;
Verfahren wie `mfi` T3→T2 (`docs/PLAN_v4.1.7.md:78-84`).

### SCOUT-77 — `LIZ-1`-Fall `uft_msa.c` („Based on msa-to-zip", ohne Lizenz) zur Entscheidung führen

**Kennzahl: fünfte — Dateien mit ungeklärter Herkunft runter**
(LIZ-1-Rückstand 14 → 13; die fünfte Zahl ist in CLAUDE.md/AGENT.md
Regel 9 als Kandidatin benannt). Material aus diesem Zyklus: (a)
Vier-Wege-Verhaltenskonvergenz (Methode + Fundstellen oben) — die
Semantik ist Formatwissen; (b) gemessen: die attribuierte API hat
keinen Produktionsaufrufer, der Produktionspfad ist der
SAMdisk-verifizierte Eigenparser; `uft_msa_to_st`/`uft_st_to_msa` sind
verwaist (0 Aufrufer). Vorlage an den Eigentümer: Idiom-Audit nach
HxC-Muster (MF-650) gegen msa-to-zip, dann Kopf nach MF-636 berichtigen
(entweder Lizenz der Quelle nennen oder auf „Verhalten nach
Formatdokumentation, eigenständig" umschreiben) — plus Verwaisten-
Entscheid für die zwei aufruferlosen Konverter (Anker oder Löschung;
SCOUT-78 wäre der Anker). Aufwand **S**.
Einhängepunkt: `docs/OPEN_ITEMS.md` §LIZ-1 (Zeile 1966, Nennung 2014).

### SCOUT-78 — ST↔MSA als angebotenen Wandlungspfad prüfen und eintragen

**Kennzahl: angebotene Wandlungspfade rauf** (+1 bis +2).
Gemessen: die Wandlungstabelle (`src/formats/uft_format_convert_tables.c`)
enthält **kein** MSA-Paar (grep `MSA` = 0 Einträge), die Rundlauf-Matrix
(`src/core/uft_roundtrip.c`) auch nicht — obwohl beide Richtungen als
Funktionen im Baum liegen (`uft_msa_to_st` `uft_msa.c:222`,
`uft_st_to_msa` `:301`, beide T2-nah, beide **ohne Aufrufer**) und MSA
ein reines Sektorformat ist (verlustfrei wandelbar, Kandidat für die
4er-Liste der Verlustfreien). Stufe-4-Weg: Bit-Identitäts-Messung
ST→MSA→ST und MSA→ST→MSA auf einem fremderzeugten Paar, Matrix-Eintrag
nach dem Muster MF-532/533/539, erst dann Tabellen-Eintrag.
Differenzlauf (Pflicht bei Verlustfrei-Behauptung): UFT wandelt
`test.msa`→ST; SamariTan extrahiert aus Original-MSA und aus
UFT-ST-Ausgabe; Metrik: `cmp` je Datei, Toleranz: keine. Zweitweg ohne
SamariTan: RLE-Rundlauf rein rechnerisch + Vergleich gegen ein von
fremder Hand erzeugtes MSA/ST-Paar gleichen Inhalts. Aufwand **S-M**.
Einhängepunkt: `src/core/uft_roundtrip.c` (SSOT der Matrix),
Kennzahlentabelle CLAUDE.md §MF-640.

**Nicht vorgeschlagen (Ratenbremse/Kennzahl):** SamariTan-Registrierung
als eigener Punkt (steckt als Oracle-Kandidat in 76/78 und hängt an
U-1); AHD; used-sectors-Expansion; ST-Realabbild-Beschaffung (hebt T2
nur auf T1b, bewegt die T3-Zahl nicht) — alles Fundus.

---

## Beschaffungsliste

| Was | Wofür | Status |
|---|---|---|
| Hatari `src/floppies/dim.c` | Spec-Referenz SCOUT-76 | **liegt** (wörtlich zitiert; Quelle raw.githubusercontent.com/hatari/hatari, main) |
| DIM-Fixture | SCOUT-76 | **Rezept liegt**, lokal erzeugbar, kein Download |
| ST/MSA-Paar fremder Hand | SCOUT-78 Differenzlauf | erzeugbar via SamariTan (nach U-1) oder alternativ via Hatari/Emulator-Export — nichts davon liegt im Korpus (geprüft) |
| msa-to-zip-Quelle | SCOUT-77 Idiom-Audit | Beschaffung nötig (nicht Teil dieses Zyklus) |

## UNGEKLÄRT

- **U-1 (Eigentümer-Vorlage, Lizenz-Grenzfall):** Jacknife hat keine
  Lizenzdatei; Releases enthalten kein CLI-Binary. Selbstbau und
  interne Nutzung von SamariTan als Oracle ist formal nicht gedeckt
  (Zone ROT deckt „Oracle ✅" nur bei legal beziehbarem Binary).
  Optionen: (a) Upstream-Issue mit Bitte um LICENSE-Datei — das Repo
  kreditiert seine eigenen Quellen sorgfältig, Aussicht gut; (b) auf
  SamariTan verzichten, SCOUT-76 trägt sich mit der Hatari-Spec allein,
  SCOUT-78 mit dem Zweitweg. Entscheidung liegt beim Eigentümer.
  (Werkzeug-**Ausgaben** — erzeugte Abbilder — sind davon unabhängig
  nutzbar, wie bei greaseweazle-Fixtures im Korpus.)
- **U-2:** Ob FastCopy selbst bei HD-DIMs (density=1) das Folgebyte von
  end_track ungleich 0 schreibt — dort läge der einzige Fall, in dem
  Jacknifes Word-Lesart und Hataris Byte-Lesart real divergieren.
  Keine der Quellen belegt ein echtes HD-DIM; für die Spec als offen
  markieren.
- **U-3:** atari-st-tools nennt keinen only/or-later-Grant zum
  GPL-3.0-Volltext. Für dieses Gutachten irrelevant (GELB in beiden
  Lesarten), bei etwaiger späterer Nutzung PRÜFEN.
- **U-4:** Ob die Grenzwert-Doppelung im MSA-Pfad (spt≤11 in
  `uft_msa.c:85` vs. spt≤18 in `uft_msa_plugin.c:103`) real erreichbar
  divergiert, hängt an der Probe-Reihenfolge der Registry — nicht
  vollständig verfolgt; gehört in die Stufe-4-Arbeit von SCOUT-77/78.

## Lokale Artefakte dieses Zyklus (Transparenz)

- `work/Jacknife/dllmain.c`: 1-Zeilen-Messpatch (mkdir), markiert;
  `work/Jacknife/samaritan.exe`: lokaler Messbau, nicht Teil des Repos.
- Wegwerf-Fixtures (test.st/test.msa/test.dim) im Scratchpad, bewusst
  nicht eingecheckt — Rezepte stehen oben.

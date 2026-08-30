# Gutachten: cmosher01/Apple-II-Disk-Tools

> Gemessen 2026-08-30 gegen HEAD `639dc1c` (2019-01-11, Repo laut README
> DEPRECATED). Messdatei: `tools/uft-scout/work/apple2-disk-tools.messung.json`
> (Erstsichtung 2026-08-28, diese Runde ergänzt).
> Inventar: `tools/uft-scout/work/inv.json` (SSOT ok, 88 Plugins,
> UFT-HEAD `921671bf`, 24 Korpus-Abbilder).
>
> **Neubesuchs-Anlass (AGENT.md Regel 6):** Erstsichtung 2026-08-28 brach
> vor dem Gutachten ab (Ratenbremse, im Messkopf vermerkt); `docs/STAND.md`
> führte die Zone seither als `?`. Neue Fragen dieses Zyklus: (1) Zone
> auflösen, (2) Oracle-Baubarkeit auf MinGW — die Frage stellte sich erst,
> nachdem `fftool` (kein cargo) und DiskImageTool (WinForms, MF-708) als
> Oracles ausfielen, (3) Wandlungspfad-Frage DSK→WOZ gegen die
> Roundtrip-SSOT. Upstream unverändert: `git fetch` 2026-08-30, origin-HEAD
> = lokaler HEAD `639dc1c`.

## Kategorie

**Oracle** — der Hauptfund des Zyklus. Ein bau- und laufbewiesenes
C-CLI, das DOS-geordnete Apple-II-Sektorabbilder (`.dsk`/`.do`/`.d13`)
deterministisch nach WOZ 2.0 wandelt. Dazu Daten-Nebenkanal (Werkzeug
erzeugt Referenz-WOZ; das Repo selbst bringt **keine** Abbilder mit:
`find -iname '*.dsk' -o -iname '*.woz' …` = 0 Treffer).

## 1. Was drinsteht (Frage a)

37 Dateien, 2 633 LOC C (Zählmethode in der Messdatei). Vier Programme
(`src/Makefile.am`: `bin_PROGRAMS=a2catalog a2nibblize to_woz2 empty_woz2`):

| Programm | was es tut | Beleg |
|---|---|---|
| `to_woz2` | `.dsk`/`.do` (143 360 B, 16-Sektor 6-and-2) und `.d13` (116 480 B, 13-Sektor 5-and-3) → WOZ 2.0; ohne Eingabe ein Blank-WOZ | `src/to_woz2.c:398-425` (usage), `:380-396` (Typ aus Endung) |
| `a2catalog` | erzeugt eine **leere DOS-3.3-Katalogspur** (VTOC + Katalogsektoren) nach stdout, raw oder hex — ein Generator, **kein FS-Leser** | `src/a2catalog.c:347-433` (`catalog_VTOC_out`), `:454-478` (`run_program`) |
| `a2nibblize` | Sektor↔Nibble-Kodierung 4-and-4/5-and-3/6-and-2 als Filter | `src/a2nibblize.c` |
| `empty_woz2` | leeres WOZ2-Gerüst | `src/empty_woz2.c` |

Was `to_woz2` in den WOZ-2.0-Kopf schreibt (alle Zeilen `src/to_woz2.c`):

* Header `WOZ2 FF 0A 0D 0A`, **CRC32 = 0** (`:465 header(0); // TODO
  calculate CRC`) — laut Spec zulässig („no CRC … should be ignored",
  WebFetch applesaucefdc.com/woz/reference2 2026-08-30), aber schwach.
* INFO 60 B: version=2, disk_type=1 (5,25″), write_protect=0, sync=0,
  **cleaned=1** (`:68` — keine erfundenen Zufallsbits), creator
  `to_woz2`, sides=1, boot_sector_format=1+dos33 (2=16-Sektor,
  1=13-Sektor), timing=32 (4 µs, `:79`), **hardware=0x01FF
  („alle", `:82` — Erfindung**, das Werkzeug weiß es nicht), ram=0,
  largest_track=14 Blöcke.
* TMAP: Spur t auf Vierteltracks t−0,25/t/t+0,25, 0xFF dazwischen
  (`:97-114`); gemessen am Output: `[0,0,255,1,1,1,255,…]`, 103 belegte
  Einträge.
* TRKS: 35 Spuren × 14 Blöcke, feste Bitzahl **0xC5C0** (50 624) je
  16-Sektor-Spur, **0xBB30** bei 13-Sektor (`:454-456`) — synthetische
  Einheitslänge, kein echtes Timing.
* Kein META-Chunk (der kam erst im Nachfolger DskToWoz2).
* Synthese je Sektor: Sync-Gaps mit 9/10-Bit-FF, Adressfeld
  `D5 AA 96` (16-S.) bzw. `D5 AA B5` (13-S.), Volume fix 0xFE,
  4-and-4-Adresse, Datenfeld `D5 AA AD` + Nibbles, Epilog `DE AA EB`
  (`:240-315`); Interleave DO-Tabelle `:170`, 13-Sektor-Skew ×10 mod 13
  `:160-168`. **ProDOS-Order (.po) fehlt** — nur TODO (`:412`).

Qualitätsbefund am Rand: `parse_filename` (`:367-369`) macht
`malloc(strlen(arg))` + `strcpy` → **1-Byte-Heap-Überlauf bei jedem
Aufruf mit Eingabedatei**. Für den Oracle-Einsatz mit kontrollierten
Namen praktisch folgenlos, aber bekannt zu machen; unter ASan würde
jeder Lauf rot.

**Haben wir das auch?** Inventar-Abfrage (zitiert, `inventar.py query`):
`do`→`vorhanden:true, tier:T3`; `po`→`vorhanden:true, tier:T3`;
`d13`→`vorhanden:true, tier:T3` (VERIFICATION_TIERS.md:63: **keine
Tests**); `woz`→`vorhanden:true, tier:T2`; `nib`→`vorhanden:true`;
`a2r`/`2mg`→`vorhanden:true, tier:null`. Lesen können wir das alles dem
Namen nach. Was fehlt, ist dreierlei, je gemessen:

1. **Kein Apple-Wandlungspfad.** `grep -n "WOZ\|DSK"
   src/core/uft_roundtrip.c` = 0 Paar-Treffer; DSK→WOZ ist nur in
   `src/formats/uft_format_convert_tables.c:272-276` als SYNTHETIC
   registriert, `UFT_FORMAT_WOZ` hat außerhalb der Tabelle **keinen**
   Wandler-Treffer. Das Preflight-Tor weist das Paar korrekt als
   UNGEPRÜFT ab.
2. **Kein gebauter GCR-Encoder.** `uft_nib_parser_v2.c:13` sagt selbst
   „6-and-2 and 5-and-3 **decoding**"; ein Encoder liegt nur im nicht
   gebauten Vendor-Baum (`grep a8rawconv UnifiedFloppyTool.pro` = 0).
3. **Null Apple-Abbilder im Korpus.** `inv["korpus"]`: 24 Einträge,
   Filter auf do/po/dsk/woz/nib/2mg/a2r/moof/d13 = leere Liste.

## 2. Können wir das entwickeln? (Frage b)

Ja — auf dem einzigen Weg, den GPL-3.0 lässt: **Oracle + Verhaltens-Spec
+ Nachbau**. Der Oracle-Beweis ist diese Runde erbracht, nicht vermutet:

* **Bau auf dieser Maschine:** MinGW gcc 13.1.0
  (`/c/Qt/Tools/mingw1310_64/bin`), eine Zeile, exakt die
  `to_woz2_SOURCES` aus `Makefile.am` (to_woz2.c + 5 nibblize + ctest.c),
  **ohne autotools/gnulib** — rc=0, 0 Warnungen, 94 505 B Binary.
  Einzige Vorarbeit: ctest-Submodul holen (`.gitmodules` nennt
  `git://…/ctest.git`, das Protokoll ist tot → URL auf https setzen,
  Commit `f665d228`). `a2catalog` bräuchte dagegen gnulib
  (`minmax.h`, `binary-io.h`) + bootstrap — nicht ohne Weiteres.
* **Lauf:** 16-Sektor rc=0, 13-Sektor rc=0, Blank rc=0; Ausgabe je
  252 416 B.
* **Determinismus:** zwei Läufe auf derselben Eingabe →
  SHA-256-identisch (`0015aa1e…` beide Male). Keine Zeitstempel, kein
  META.
* **Strukturprüfung (Zwei-Quellen):** Quelle 1 = fremder Quelltext +
  eigener Chunk-Walk (INFO@12, TMAP@80, TRKS@248, Datei restlos
  konsumiert); Quelle 2 = WOZ-2.0-Referenz applesaucefdc.com/woz/
  reference2 (WebFetch 2026-08-30: INFO-Daten @20, TMAP @88, TRKS @256,
  CRC-0-Semantik) — deckungsgleich.

Grenzen des Oracles, aus der Quelle gemessen: kein `.po`, kein
`--version` (Pinnen über Quell-Commit `639dc1c` + Bau-Rezept + Binary-
SHA), CRC bleibt 0, Volume fix 0xFE, Einheitsbitlänge — es beantwortet
die Frage „welcher GCR-Strom entspricht diesen Sektoren", nicht
„welches Timing hatte eine echte Diskette".

## 3. Sind wir besser? (Frage c)

**Beim Lesen ja, beim Erzeugen nein.** UFT dekodiert WOZ (T2,
Applesauce-Referenz, `test_woz_roundtrip`/`test_woz_writer`), NIB, A2R
— dieses Repo dekodiert gar nichts. Umgekehrt kann das Repo etwas, das
UFT gemessen nicht kann: aus Sektoren einen GCR-Bitstrom **erzeugen**
(Punkt 1-3 oben). „Besser" im Sinn von Regel 7 ist erst nach dem
Differenzlauf behauptbar, und der ist unten spezifiziert.

## 4. Lizenz

`COPYING` = wörtlicher **GPL-3.0**-Text (aus der Datei; Kennung der
Messung, Zone **GELB**). Konsequenz nach `playbook/lizenzmatrix.md` Z.9:
**kein Port** in ein GPL-2.0-Projekt; Verhaltens-Spec ✓, Oracle ✓.
**Attribution, die jede Übernahme tragen müsste:** Apple-II-Disk-Tools,
© Christopher Alan Mosher, GPL-3.0 — und zweistufig: `nibblize_5_3.c:18`
und `nibblize_6_2.c:66,94-95` erklären „Based on code by Andy McFadden,
from CiderPress" (CiderPress: BSD-3-Clause, per WebFetch 2026-08-28
verifiziert, siehe Messdatei `zusatzmessungen.attribution`). Ein
Nachbau stützt sich deshalb NICHT auf diesen Code, sondern auf die
freien Fakten: WOZ-2.0-Spec (applesaucefdc), Beneath Apple DOS
(Sektor-Interleave, Prologe, 4-and-4/5-and-3/6-and-2-Tabellen) — beides
Spec-Kanal.

## 5. Bewegte Kennzahlen

* **ungeprüfte Formate (T3) runter** — `do` und `d13` (beide T3, `d13`
  ohne einen einzigen Test) bekommen erstmals ein unabhängiges
  Gegenüber; `woz` (T2) bekommt sein erstes fremd erzeugtes
  Korpus-Abbild (dieselbe Bewegung wie `dim_atari` T3→T1b, MF-690).
* **angebotene Wandlungspfade rauf** — DO→WOZ wäre der erste
  Apple-Eintrag der Roundtrip-Matrix (Typ Sektor→Bitstream, wie das
  gemessene D64→G64).

## 6. Einhängepunkte (git-grep-fähig)

* `docs/ORACLES.md` §Register/§Prüfstand — Eintragsmuster wie `dtc`
  (nur Ausführung) bzw. `xdftool` (Erzeuger-Rolle ausgewiesen).
* `docs/PLAN_v4.1.7.md` §Oracle-Differenzlaufsatz (Z.119 ff.) und die
  Skip-77-Regel (Z.244-246) für nicht baubares Oracle.
* `src/core/uft_roundtrip.c` (SSOT) für den neuen Pfad;
  `docs/VERIFICATION_PLAN.md` §Einfrier-Regel für die Encoder-Route.
* Warnung aus `docs/plans/UMSETZUNGSLISTE.md:247`: „5-and-3-Apple-GCR"
  steht dort unter der Einfrier-Regel — ein neuer 5-and-3-**Decoder**
  wäre gesperrt; hier geht es um den **Encoder** für einen
  Wandlungspfad plus Verifikation Bestehendes, beides mit benannter
  Referenz + Rotbeweis zuerst + Referenz im Header (der von der
  EINFRIER-REGEL verlangte Weg).

## 7. Differenzlauf-Plan (für jede „besser/richtig"-Aussage)

Binaries: `to_woz2.exe` (Quell-Commit 639dc1c, MinGW-Rezept oben,
Binary-SHA im Manifest) gegen UFT-Testtreiber (woz-Plugin, T2).
Korpus: (1) synthetisches 143 360-B-DSK mit adressierbarem Muster
(Byte = f(Track,Sektor,Offset)), (2) dito 116 480-B-D13, (3) Blank.
Lauf: DSK →(to_woz2)→ WOZ →(UFT woz-Leser + 6-and-2-Decode)→ Sektoren.
Metrik: byteweise Identität aller 35×16×256 (bzw. 35×13×256) Bytes,
plus Adressfelder (Volume 0xFE, t, s, Checksumme). Toleranzliste:
Kopf-CRC 0 (Spec-legal), TMAP-Nachbarspuren, Einheitsbitlänge 0xC5C0,
kein META — alles dokumentierte Oracle-Eigenschaften, keine Fehler.
Rot-Kriterium: ein Sektor, den der UFT-Weg anders liest als er
hineingeschrieben wurde.

## 8. Beschaffungsliste

Nichts anzufordern: beide Klone liegen (`work/apple2-cmosher/`), das
ctest-Submodul ist geholt, das Bau-Rezept ist eine gcc-Zeile, die
Eingaben sind synthetisch erzeugbar. Gegen `inv["korpus"]` geprüft:
kein vorhandenes Abbild wird erneut angefordert (es gibt schlicht kein
Apple-Abbild). Ein **echtes** historisches DOS-3.3-Abbild bleibt
wünschenswert (Herkunft „real" statt „cross-tool"), ist aber nicht
Voraussetzung.

## 9. Aufwandsklasse

* Oracle-Registrierung + Eichung (Fünf-Fragen-Prüfstand): **S**
* Korpus: erstes Apple-WOZ (blank + synthetisch, Manifest): **S**
* Hebung `do`/`d13` per Differenzlauf: **M**
* Wandlungspfad DO→WOZ (Encoder + Matrix + Messung): **L**

## UNGEKLÄRT

* Ob UFTs woz-Leser die to_woz2-Ausgabe tatsächlich öffnet, ist
  **nicht gemessen** — dafür braucht es einen Testtreiber im Baum
  (Stufe 4, erster Rotbeweis des Differenzlaufs).
* `GPL-3.0` vs. `GPL-3.0-or-later`: COPYING ist der nackte Lizenztext,
  die Quellköpfe von Apple-II-Disk-Tools tragen keine
  „or-later"-Klausel. Für die Zone egal (beide GELB), für eine
  spätere Doppel-Nutzung dem Eigentümer vorzulegen.
* Der `hardware=0x01FF`-Eintrag: ob Applesauce-Ökosystem-Werkzeuge das
  als „kompatibel mit allem" oder als Fehlangabe behandeln, ist nicht
  geprüft — für den Toleranzlisten-Eintrag des Differenzlaufs ohne
  Belang, für Korpus-Metadaten zu vermerken.

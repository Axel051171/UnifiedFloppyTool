# Gutachten: cmosher01/DskToWoz2

> Gemessen 2026-08-30 gegen HEAD `8aeed94` (2020-02-08). Messdatei:
> `tools/uft-scout/work/apple2-disk-tools.messung.json` (gemeinsamer
> Zyklus mit Apple-II-Disk-Tools desselben Autors; Erstsichtung
> 2026-08-28, diese Runde ergänzt). Inventar: `tools/uft-scout/work/
> inv.json` (SSOT ok, 88 Plugins, UFT-HEAD `921671bf`).
>
> **Neubesuchs-Anlass:** wie beim Schwester-Gutachten — Erstsichtung
> ohne Gutachten (Ratenbremse), Zone `?` in STAND.md aufzulösen,
> Oracle-Frage neu seit dem fftool-/DiskImageTool-Ausfall. Upstream
> unverändert (`git fetch` 2026-08-30, origin = lokal `8aeed94`).

## Kategorie

**Verbesserung/Spec-Quelle** — der GUI-Nachfolger des CLI-Werkzeugs
`to_woz2`. Als Oracle **unbrauchbar** (keine Kommandozeile), als
Verhaltens-Spec in zwei Punkten dem Vorgänger voraus. Keine Abbilder
im Repo (`find` über beide Klone = 0 Treffer).

## 1. Was drinsteht (Frage a)

29 Dateien, 1 973 LOC C/C++ (Messdatei). Qt-Widgets-Programm
(`dsktowoz2.pro`; `main.cpp:22-25` = `QApplication` + `Window`, keine
CLI-Argumente); Batch-Konvertierung ganzer Verzeichnisbäume über
`QDirIterator` (`window.cpp:215`). Der Konvertierungskern
`dsktowoz2.c` ist eine Weiterentwicklung von `to_woz2.c` (diff:
verschieden; die nibblize-Dateien sind hier ctest-frei):

* **META-Chunk** mit Dateiname und **CRC32 der DSK-Quelldatei**
  (`dsktowoz2.c:365-372`, Aufruf `:416`, eigene `crc32.c`) — Provenienz
  im Ausgabeartefakt, die to_woz2 nicht hat.
* **DOS-Versions-Heuristik statt Dateiendung** (`conversion.cpp:31-77`):
  16-Sektor an Größe 0x23000, 13-Sektor an 0x1C700; dann VTOC-Probe —
  Katalogspur-Byte @0x11001 (bzw. 0x0DD01) == 0x11, Max-T/S-Pairs
  @0x11027 == 0x7A, DOS-Versionsbyte @0x11003 → „DOS 3.x". Nur bei
  bestandener Probe wird konvertiert (`Conversion::ok()`).
* Unverändert gegenüber to_woz2: Kopf-CRC bleibt 0 (`:411 // TODO
  calculate CRC`), **ProDOS-Order nicht unterstützt** (README:
  „ProDOS (PO) order is not supported"; `:176` PO-Interleave nur als
  auskommentiertes TODO), creator jetzt `dsktowoz2` (`:88`).

**Haben wir das auch?** Inventar wie im Schwester-Gutachten (do/po/d13
T3, woz T2). Der spezifische Abgleich hier: UFTs DO/PO-Unterscheidung
ist **reine Endungs-Raterei** — `src/formats/apple/prodos_po_do.c:71`:
`ctx->dos_order = (ext && (ext[1]=='d' || ext[1]=='D'))`. Ein
PO-geordnetes Abbild mit Endung `.dsk` wird still als DOS-Order
gelesen: jede Datei-Extraktion darauf wäre eine **stille Falschaussage**
(die FMT-2/3/10/11/12-Fehlerklasse). Eine Inhalts-Probe wie
`readDos()` existiert im Baum nicht (grep VTOC/0x11001 über
`src/formats/apple/` = 0 Treffer in der Probe-Logik).

## 2. Können wir das entwickeln? (Frage b)

Ja, als **Nachbau nach Verhaltens-Spec**: die VTOC-Probe besteht aus
drei Offsets und drei Erwartungswerten — das sind Fakten des
DOS-3.3-Dateisystems (Beneath Apple DOS, Kap. 4: VTOC in Spur 0x11
Sektor 0, Byte 1 = Katalogspur, Byte 0x27 = Max-T/S-Pairs 122, Byte 3
= DOS-Version), keine Idiome dieses Programms. Zweitquelle für jede
Zahl: Beneath Apple DOS; Erstquelle: `conversion.cpp:47-77`. Ein
GPL-3-Problem entsteht dabei nicht, weil nur die (freien) Fakten
übernommen werden — die Attribution lautet dann „Verhalten nach
DOS-3.3-VTOC-Layout (Beneath Apple DOS), Probe-Idee wie
cmosher01/DskToWoz2 (GPL-3.0), eigenständige Implementierung".

Als Oracle dagegen: **nein, gemessen** — kein `main()` außerhalb von
Qt, kein CLI-Pfad (`main.cpp:22-27`), Windows-Build wäre Qt-Deployment
für ein Klick-Werkzeug. Der Oracle-Kanal dieses Zyklus läuft über den
Vorgänger (siehe Schwester-Gutachten).

## 3. Sind wir besser? (Frage c)

Bei der DO/PO-Erkennung ist **dieses Repo besser, belegt ohne
Differenzlauf-Bedarf im strengen Sinn**: UFT prüft einen Buchstaben der
Endung (`prodos_po_do.c:71`), DskToWoz2 prüft drei Inhalts-Bytes und
lehnt Nicht-DOS-Abbilder ab. Wer die Aussage schärfer will: Messplan =
ein PO-geordnetes 143 360-B-Abbild als `.dsk` benennen, durch beide
Erkennungen schicken; UFT meldet „DOS order", `readDos()` meldet leer.
Toleranz: keine. Beim WOZ-**Lesen** hat DskToWoz2 nichts (es liest kein
WOZ), da ist UFT konkurrenzlos in diesem Vergleich.

## 4. Lizenz

`LICENSE` = wörtlicher **GPL-3.0**-Text (aus der Datei, Zone **GELB**;
Quellköpfe tragen © 2019 Christopher Alan Mosher). Konsequenz
(`playbook/lizenzmatrix.md` Z.9): kein Port; Verhaltens-Spec ✓.
**Attribution:** DskToWoz2, © Christopher Alan Mosher, GPL-3.0;
darunter dieselbe CiderPress-Kette wie beim Schwesterrepo
(`nibblize_5_3.c:29`, `nibblize_6_2.c:77,105-106`: „Based on code by
Andy McFadden, from CiderPress", BSD-3-Clause).

## 5. Bewegte Kennzahl

**ungeprüfte Formate (T3) runter** — die VTOC-Probe ist der fehlende
Baustein, damit eine `do`/`po`-Hebung nicht an einem falsch gelabelten
Korpus-Abbild scheitert; sie gehört in den Prüfauftrag der
`do`-Hebung (Vorschlag 2 der Zyklus-Liste), nicht als eigenes Plugin.

## 6. Einhängepunkt

`docs/VERIFICATION_PLAN.md` §Einfrier-Regel (Bugfix/Verifikation an
Bestehendem ist ausdrücklich erlaubt — `prodos_po_do.c` existiert, die
Probe ist eine Korrektur seines Erkennungspfads mit benannter Referenz
und Rotbeweis: erst der Test mit dem falsch benannten PO-Abbild, der
heute grün-falsch liefe). Für das WOZ-META-Muster: Fundus-Notiz, kein
Anker — UFTs woz-Writer-Metadaten sind eigene Arbeit unter
`docs/ORACLES.md` §Provenienz-Gedanke.

## 7. Beschaffungsliste

Nichts; Klon liegt, die Spec-Fakten stehen in Beneath Apple DOS
(frei zugänglich). Prüfabbilder synthetisch erzeugbar.

## 8. Aufwandsklasse

VTOC-Inhalts-Probe als Teil der do/po-Hebung: **S**.

## UNGEKLÄRT

* Ob `readDos()` bei DOS-3.2-Abbildern (13-Sektor) zuverlässig
  anschlägt, hängt daran, dass 13-Sektor-Disketten dieselben
  VTOC-Bytes tragen — aus Beneath Apple DOS plausibel, an einem
  echten DOS-3.2-Abbild **nicht gemessen** (keins im Korpus, keins in
  den Repos).
* Das Verhältnis „GUI-Nachfolger deprecates CLI-Vorgänger": ob der
  Autor die CLI je zurückportiert hat, wurde nur an diesen zwei Repos
  geprüft, nicht am gesamten Konto.

# Plan zu UFT-16 (FatFs) · UFT-17 (HxC) · UFT-18 (SAMdisk)

**Stand:** 2026-09-05, nach MF-895
**Filter:** „wir pushen nur was der Enduser braucht" — jede Phase nennt, was
ein Benutzer davon hat. Was keinem Benutzer nützt, steht unter *Fundus*
und wird **nicht** eingeplant (Regel 9, MF-640).

---

## Phase 0 — Was ich am Baum nachgemessen habe, bevor ich geplant habe

Die drei Berichte sind Fremdaussagen. Gemessen wurde jede tragende
Behauptung im eigenen Baum; drei Ergebnisse weichen ab.

| Behauptung | gemessen | Ergebnis |
|---|---|---|
| UFT-16: `src/fs/uft_fat12.c` hat 0 echte Aufrufer | `grep` über alle Quellen aus `git ls-files` | **stimmt** |
| UFT-16: „`explorertab.cpp` bindet nur den Header ein, ruft nichts auf" | dieselbe Messung | **trifft nicht zu — und der wahre Befund ist schärfer** (s. u.) |
| UFT-17 F1: `track0s0_altencoding` wird nirgends ausgewertet | `grep -rn "altencoding"` über `src/`, `include/`, `tests/` ohne `src/samdisk/` | **stimmt, und es ist schlimmer**: 3 Strukturen lesen das Feld ein, **null** Verzweigungen, und der Baum trägt **zwei einander widersprechende Kommentare** dazu |
| UFT-17 F3: libhxcfe kennt 0x0E/0x0F/0x10 | zwei unabhängige Tabellen im eigenen Baum | **nicht belegt** — beide hören bei `0x0D` auf, eine markiert `HFE_IF_LAST_KNOWN = 0x0E` ausdrücklich |
| UFT-18: `src/samdisk/` ist praktisch aktuell (92/100 byteidentisch) | nicht nachprüfbar ohne Klon | **offen — Fremdaussage** |

### Der schärfere FAT12-Befund

`src/explorertab.cpp` ruft `uft_fat12_init/free/delete/create_entry` —
aber die landen in `src/formats/uft_fat12_legacy.c` (98 Zeilen), **nicht**
in `src/fs/uft_fat12.c` (884 Zeilen). Der Baum hat **zwei** FAT12-Module
gleichen Namens; die GUI benutzt das kleine.

Gemessen, was das für einen Benutzer heißt:

| | |
|---|---|
| `uft_fat12_delete()` (legacy) | `return -1;` — **immer**, ohne Bedingung |
| `uft_fat12_create_entry()` (legacy) | `return -1;` — **immer** |
| die GUI dazu | meldet den Fehlschlag **ehrlich** — kein Klasse-A-Fall |
| Verzeichnisliste für `.img`/`.st` in `ExplorerTab::readDirectory()` | **gibt es nicht** — es steht „no directory listing – filesystem reading is not wired" |

Und die Schreibpfade des **kanonischen** Moduls sind ebenfalls ehrlich
read-only (`return UFT_FAT_ERR_READONLY;`, neun Funktionen). Eine
Umverdrahtung bringt also **kein** Löschen und **kein** Anlegen — sie
bringt **Lesen**: Verzeichnis, Extraktion, Bezeichner, freier Platz.

Das ist der Nutzen, um den es hier geht.

---

## Phase 1 — FAT12-Verzeichnis im Explorer sichtbar machen

**Was der Benutzer davon hat:** er öffnet ein PC- oder Atari-ST-Abbild
(`.img`, `.ima`, `.st`) und **sieht die Dateien darin**. Heute steht dort
„filesystem reading is not wired". Das ist der größte Einzelgewinn aus
allen drei Berichten.

**Warum jetzt und nicht früher:** MF-889 hat die Bedingung selbst
formuliert — *„ADF und FAT12 stehen ebenfalls auf FS-T2, sind hier aber
bewusst NICHT mitverdrahtet: jedes Format bringt seinen eigenen Beleg
mit."* UFT-16 liefert diesen Beleg: `uft_fat12.c` ist an **jedem**
geprüften Punkt deckungsgleich mit der kanonischen FAT-Referenz von ChaN
(`elm-chan.org/docs/fat_e.html`) — Bad-Cluster-Werte, EOC-Untergrenze,
letzte gültige Clusternummer, `0x00`/`0xE5`/`0x05`-Semantik der
Verzeichniseinträge, und die Typbestimmung **allein** über die
Clusterzahl statt über `BS_FilSysType`.

### Vorbedingung — und sie ist heute NICHT erfüllt

`tests/corpus_free/mtools_fat12_720k.img` ist **frisch formatiert und
leer** (Manifest-Eintrag 36: `mformat … -v UFTFAT12`). Ein Rotbeweis
gegen eine leere Wurzel prüft nicht, ob der Leser Dateien auflisten kann.

**Erster Schritt ist deshalb ein Korpus-Abbild MIT Dateien**, erzeugt von
derselben fremden Hand:

```
mcopy -i fat12_files.img <bekannte Dateien> ::
```

mtools 4.0.49 wurde für MF-789 bereits unter WSL gebaut. Ohne dieses
Abbild wird Phase 1 **nicht** begonnen — eine erfundene Liste durch eine
nie gegen fremde Aussage geprüfte zu ersetzen wäre derselbe Fehler in neu
(FMT-2/3/10/11/12).

### Schritte

1. Fixture `mtools_fat12_files.img` erzeugen, Befehlszeile und Inhalt ins
   `tests/corpus_manifest/manifest.json` (Muster: Eintrag 36).
2. **Rotbeweis zuerst:** `tests/test_explorer_tab_no_fiction.cpp` um eine
   Prüfung erweitern, die nach `loadImage()` einer `.img` die
   Dateinamen aus der Tabelle gegen die **im Manifest benannten**
   hält. Muss rot sein.
3. `readDirectory()` um den FAT12-Zweig ergänzen — über
   `uft/fs/uft_fat12.h` (`uft_fat_open`, `uft_fat_read_dir_path`),
   **nicht** über `uft_fat12_legacy.c`.
4. Grenzprüfung aus UFT-16 §6.2 in `uft_fat_get_entry()`: `off + 1` gegen
   die tatsächliche Größe von `fat_cache`. **Sobald das Modul
   Benutzerdateien anfasst, ist das keine Kür mehr** — eine zu klein
   deklarierte `fat_size` bei großer `data_clusters` treibt den Zugriff
   sonst aus dem Puffer. Eigener Rotbeweis mit einem gezielt
   widersprüchlichen BPB.
5. Fehlschlag → **keine halbe Liste**, sondern die bestehende ehrliche
   Meldung (Muster aus MF-889, D64-Zweig).

### Abnahme

- [ ] neuer Test rot vor, grün nach dem Eingriff
- [ ] Gegenprobe: FAT12-Zweig entfernt → genau dieser Test fällt
- [ ] Grenzprüfung: eigener Rotbeweis, eigene Gegenprobe
- [ ] `docs/VERIFICATION_TIERS_FS.md` nachgezogen
- [ ] `ctest` grün, alle Tore 0, Bau ohne Warnung

### Kennzahl

Keine der vier direkt. **Das wird hier bewusst überstimmt**: die
Anweisung „nur was der Enduser braucht" schlägt Regel 9, wenn ein
Benutzer eine Fähigkeit vermisst, die im Baum fertig und geprüft liegt.
Der Bezug wird als solcher im Commit benannt, nicht kaschiert.

---

## Phase 2 — HFE: die abweichende Kodierung der Spur 0 anwenden

**Was der Benutzer davon hat:** eine HFE-Aufnahme, deren Spur 0 in FM und
deren Rest in MFM geschrieben ist — der IBM-3740-Fall, bei 8-Zoll- und
manchen 5,25-Zoll-Medien der Normalfall — wird auf Spur 0 heute mit der
**falschen** Kodierung dekodiert. Ergebnis: die Sektoren der Spur 0
fehlen, still.

### Gemessen

```
include/uft/flux/uft_hfe.h:110    "0xFF = use default encoding"
include/uft/uft_hfe_format.h:196  hdr->track0s0_altencoding = 0xFF;  /* Disabled */
src/formats/hfe/uft_hfe.c:115     "0xFF = alternate encoding Track 0"   <-- widerspricht
```

Drei Strukturen lesen die vier Felder ein. **Keine einzige Verzweigung**
in `uft_hfe.c` oder `uft_hfe_parser_v2.c` reagiert darauf — gemessen über
alle Quellen aus `git ls-files`, ohne `src/samdisk/`.

Zwei der drei Kommentare sagen dasselbe wie der Referenzschreiber
(`altencoding = 0x00` ⇒ Override aktiv); der dritte sagt das Gegenteil.
Dieselbe Klasse wie MF-659: **der Baum führt die Tabelle mehrfach, und
eine Fassung ist falsch.**

### Lizenz — geklärt, nicht angenommen

`libhxcfe` ist GPL-2-or-later, UFT steht unter GPL-2 (`LICENSE`).
Vereinbar. Der Kopfkommentar nennt Quelle **und Lizenz** (MF-636), und
zwar als das, was es ist: Verhalten nachgerechnet gegen Leser
(`hfe_loader.c`) **und** Schreiber (`hfe_writer.c`), nicht übernommener
Code.

### Schritte

1. **Rotbeweis zuerst:** HFE-Fixture mit `track0s0_altencoding = 0x00`
   und einem `track0s0_encoding`, das von `track_encoding` abweicht —
   `.claude/skills/uft-flux-fixtures/scripts/generators/gen_hfe_fixture.py`
   existiert und erzeugt HFEs. Die Prüfung: Spur 0 wird mit der
   Override-Kodierung gelesen, Spur 1 mit der disk-weiten.
2. `hfe_track_encoding(hdr, track, side)` als **eine** Stelle einführen
   und an beiden Parsern benutzen.
3. Den falschen Kommentar in `uft_hfe.c:115` berichtigen und die
   Polarität an allen drei Fundstellen gleich schreiben.

### Abnahme

- [ ] Fixture-Erzeugung im Testkopf dokumentiert, nicht nur das Ergebnis
- [ ] Rotbeweis rot vor, grün nach
- [ ] Gegenprobe: Polarität umgedreht → genau dieser Test fällt
- [ ] Gegenprobe: Verzweigung entfernt → derselbe Test fällt
- [ ] `tests/test_corpus_hfe.c` bleibt grün (`gw_amigados.hfe` ist
      durchgehend MFM, darf sich nicht ändern)

### Kennzahl

Bugfix an Bestehendem — von der EINFRIER-REGEL ausdrücklich erlaubt.
Bewegt keine der vier Zahlen unmittelbar; ist aber Voraussetzung dafür,
`hfe` überhaupt über T2 zu heben, weil ein Leser mit bekanntem Lesefehler
nicht gehoben werden kann.

---

## Phase 3 — HFE `write_allowed`: Vermerk statt Verhalten

`src/formats/hfe/uft_hfe.c:513` setzt `read_only`, wenn
`write_allowed == 0xFF`. UFT-17 hat Leser **und** Schreiber der
Referenzimplementierung durchsucht: **keines** der beiden Felder
(`write_allowed`, `write_protected`) trägt dort operative Bedeutung; der
Schreiber setzt `write_protected = 1` unbedingt, der Leser wertet es nie
aus.

**Nicht löschen** — die Prüfung schadet nicht, und sie stammt womöglich
aus einer anderen, verbreiteten HFE-Beschreibung. Aber sie bekommt einen
Vermerk, dass sie **unbestätigt** ist, mit Nennung dessen, was sie
bestätigen würde. Zwei Zeilen Kommentar, kein Verhaltenswechsel, keine
Teständerung.

---

## Phase 4 — Die drei Interface-Modi: NICHT jetzt

UFT-17 nennt `0x0E S950_DD_HD`, `0x0F IBMPC_DD_HD`, `0x10 QUICKDISK` als
fehlend. Gemessen im eigenen Baum sagen **zwei unabhängige Tabellen** das
Gegenteil:

| Quelle | letzter bekannter Wert |
|---|---|
| `src/formats/hfe/uft_hfe.c` | `0x0D` (+ `0xFE` DISABLE) |
| `include/uft/uft_hfe_format.h` | `0x0D`, danach `HFE_IF_LAST_KNOWN = 0x0E` |
| `src/samdisk/hfe.cpp` (fremde Umsetzung, vendored) | `0x0D` (+ `0xFE`) |

Der Bericht steht damit **1 zu 2**, und die Projektregel verlangt zwei
unabhängige Quellen. MF-659 hat denselben Fehler an derselben Enum
korrigiert — mit **drei** Belegen, jeder selbst nachgelesen. Weniger geht
hier nicht.

**Was es öffnen würde:** ein Blick in `libhxcfe.h` bei einer benannten
Fassung (Datei, Zeile, Commit), plus eine zweite Quelle. Bis dahin bleibt
es Kandidat, nicht Auftrag. `HFE_IF_LAST_KNOWN = 0x0E` ist die ehrliche
Fassung des heutigen Wissensstandes und bleibt stehen.

---

## Phase 5 — `src/samdisk/VENDORED.md`: den Stand eintragen

`VENDORED.md` führt seit MF-369/AUD-7 „Version/Commit: nicht dokumentiert
… exakter Stand unbekannt". UFT-18 hat alle 100 vendorten `.cpp` gegen
den aktuellen HEAD verglichen: **92 byteidentisch**, 5 reine
Kommentar-Tippfehler, 3 mit Funktionsänderung — alle drei auf der
CLI-/Anzeigeebene, **keine** in den Kern-Formatparsern.

Das ist wichtig, weil `fdi.cpp` aus genau diesem Verzeichnis als
Ground-Truth für die FDI-Neuimplementierung diente (MF-359, FMT-12) —
und `fdi.cpp` gehört zu den 92 identischen.

**Aber:** die Zahl ist eine Fremdaussage, die ich ohne Klon nicht
nachrechnen kann. Sie kommt deshalb **als Fremdaussage** in `VENDORED.md`
— mit Datum, Quelle und dem ausdrücklichen Vermerk, dass sie hier nicht
nachgemessen wurde. Was sie zur eigenen Messung machen würde, steht
daneben.

Ehrlichkeit vor Vollständigkeit: „92/100, laut UFT-18, nicht
nachgerechnet" ist mehr wert als „unbekannt" **und** mehr wert als eine
übernommene Zahl ohne Kennzeichnung.

---

## Fundus — notiert, nicht eingeplant

Kein Kanal heute, oder kein Nutzen für einen Benutzer. Benannt wartend,
nicht verfallen (MF-695).

| Fund | warum nicht jetzt |
|---|---|
| **2657 von 4078 Exporten ohne Aufrufer** (`tuersucher.py`, eigener Lauf) | ein Zensus, kein Auftrag. Er wird erst zum Auftrag, wenn ein einzelnes Modul benannt ist — Phase 1 ist genau so ein Fall |
| **FatFs als neunter Oracle** für `kalibrierer.py` | Werkzeugkette, kein Benutzernutzen. Wäre nach Phase 1 sinnvoll: dann gäbe es einen zweiten Leser für dieselben Abbilder |
| **`RetryPolicy` als zentrale Bestätigungs-Richtlinie** (samdisk_plus) | wäre **neuer, ungeprüfter Code** und ersetzt Zähler, die es im Baum so noch gar nicht gibt. Fällt unter die EINFRIER-REGEL. Zudem eigene Lizenz des Forks, ungeprüft |
| **16-Cluster-Sicherheitsmarge** an der FAT12/16-Grenze | eine Diagnose, die heute niemand sieht, weil das Modul keinen Aufrufer hat. Wird nach Phase 1 sinnvoll — dann als eigener Punkt mit eigenem Beleg |
| **~35 HxC-Ladernamen ohne Entsprechung** | der Bericht nennt sie selbst ausdrücklich **keine** bestätigte Lückenliste. Neue Format-Plugins fallen ohnehin unter das Moratorium |
| **`OrphanDataCapableTrack` / `MultiScanResult`** aus samdisk_plus | ansehen, bevor Eigenes gebaut wird — aber es steht heute nichts an, das sie bräuchte |

---

## Reihenfolge und Aufwand

| # | Phase | Nutzen für den Benutzer | Aufwand | Vorbedingung |
|---|---|---|---|---|
| 1 | Phase 2 — HFE Spur-0-Kodierung | behebt einen **stillen Lesefehler** | klein–mittel | Fixture aus dem vorhandenen Generator |
| 2 | Phase 3 — `write_allowed`-Vermerk | Ehrlichkeit | sehr klein | keine |
| 3 | Phase 5 — `VENDORED.md` | Ehrlichkeit | sehr klein | keine |
| 4 | Phase 1 — FAT12-Verzeichnis | **neue sichtbare Fähigkeit** | mittel–groß | **Korpus-Abbild mit Dateien fehlt** |

Phase 2 zuerst, weil sie einen Fehler behebt statt eine Fähigkeit
hinzuzufügen, und weil sie keine Vorbedingung hat. Phase 1 ist der
größere Gewinn, hängt aber an einem Fixture, das es noch nicht gibt.

Phase 4 wird **nicht** eingeplant.

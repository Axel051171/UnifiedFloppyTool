# X-Copy unter Emulation — Sitzungsprotokoll

**Werkstatt-Dokument. Stand 2026-09-01 (MF-773).**

Dieses Blatt macht aus der Emulationssitzung **eine** Sitzung statt drei.
Jede Frage steht mit ihrer Fixture, ihrem erwarteten Bild und der Stelle
im Baum, die von der Antwort abhängt.

Das Binary ist hier **Oracle**, nicht Vorlage: seine Ausführung ist frei,
seine Weitergabe nicht (`MF-695`, Kanal „Oracle"). Beobachtet wird, was
auf dem Schirm steht — nicht, wie es zustande kommt.

---

## Was fehlt, bevor die Sitzung laufen kann

Gemessen am 2026-09-01:

| | Stand |
|---|---|
| **Oracle-Material** | **vorhanden** — drei Fassungen als entpackte AmigaDOS-Bäume: `xcopy_v3.4_authorsmaster_en_1991-02-17`, `xcopy_v5.21_en_c9.69`, `xcopy_01_95_master` |
| **Emulator** | **fehlt** — weder WinUAE noch FS-UAE installiert |
| **Kickstart-ROM** | **fehlt** — lizenziertes Eigentum (Cloanto), **Eigentümer-Beschaffung** |

Beides kann nur der Eigentümer bereitstellen. Ohne ROM startet kein
Amiga, und ein ROM aus unklarer Quelle wäre genau der Fehler, den dieses
Projekt bei fremdem Code nicht macht.

**Fixtures: gebaut und abgenommen (MF-771).**
`tests/flux_gen/xcopy/gen_xcopy_fixtures.c` schreibt fünf SCP-Disketten,
die WinUAE und FS-UAE einlegen können. Bauen und laufen lassen:

```
SRC="src/formats/scp/uft_scp_writer.c src/flux/uft_flux_decoder.c
     src/flux/uft_track_verdikt.c src/flux/uft_flux_histogram.c
     src/flux/uft_flux_sync_search.c src/flux/uft_dewarp.c
     src/flux/uft_decode_timeline.c src/flux/uft_media_profile.c
     src/flux/uft_mfm_sector_parser.c src/formats/amiga/uft_amiga_syncs.c
     src/core/uft_log.c src/formats/scp/uft_scp_parser*.c"

gcc -std=c11 -Iinclude -Iinclude/uft -Isrc -I. -Itests/flux_gen/amigados \
    tests/flux_gen/xcopy/gen_xcopy_fixtures.c \
    tests/flux_gen/amigados/flux_gen.c $SRC -o gen_fixtures

./gen_fixtures <ausgabeordner>
```

Die Dateien sind zusammen ~86 MB und gehören **nicht** ins Repository —
sie werden bei Bedarf erzeugt. Gemessen am 2026-09-01:

| Datei | Frage | Nachbarspur | Spur 40 Umdr. 0 | Umdr. 1 |
|---|---|---|---|---|
| `E1_leere_spur.scp` | E1 | 11/11 | **0/11** | — |
| `E3a_kein_sync_u2.scp` | E3 | 11/11 | 11/11 | **0/11** |
| `E3b_eine_umdrehung.scp` | E3 | 11/11 | 11/11 | — |
| `E4_marke_448A.scp` | E4 | 11/11 | **0/11** | — |
| `E5_gerettet.scp` | E5 | 11/11 | **10/11** | **11/11** |
| `E2_zwoelf_sektoren.scp` | E2 | 11/11 | **12/11** | — |
| `E6_kopf_und_daten.scp` | E6 | 11/11 | **10/11**, davon **1 Kopffehler** | — |

**Warum gemischte Disketten:** 159 gute Spuren und **eine** Fixture-Spur
(immer Spur 40). Eine Diskette aus lauter Fixture-Spuren zeigt 160 rote
Ziffern und sagt weniger — bei einer gemischten ist die Ziffer gegen ihre
grünen Nachbarn lesbar, und es steht fest, dass X-Copy die Diskette
überhaupt annimmt. Eine durchgehend unformatierte könnte es schon vor der
Spuranzeige abweisen, und die Sitzung wäre ergebnislos, ohne dass man
wüsste warum.

**Die Abnahme prüft beide Seiten.** Nachbarspur muss 11/11 liefern (die
Diskette ist gültig), und die Fixture-Spur muss **abweichen** (die Frage
steht wirklich darauf). Die erste Fassung prüfte nur die erste Hälfte —
eine still normal gebaute Fixture wäre durchgegangen.

**E2 hat eine physische Besonderheit, die keine Künstlichkeit ist.**
Gemessen: ein Sektor belegt 8704 Zellen, elf sind 95 744, und eine
Umdrehung bei 2000 ns fasst 100 000. **Zwölf wären 104 448 — sie passen
nicht.** Genau deshalb sind es elf. Die Fixture schreibt die
12-Sektor-Spur deshalb mit **1900 ns je Zelle** (105 263 Zellen je
Umdrehung); die anderen 159 Spuren bleiben beim Nennwert.

Das ist kein Kunstgriff, sondern das, was „lange Spur" als Kopierschutz
seit jeher tut: schneller schreiben, um mehr unterzubringen. **Beim
Deuten des Ergebnisses mitdenken:** wenn X-Copy hier eine Ziffer zeigt,
ist die Ursache nicht zwingend die Sektorzahl — sie könnte auch die
abweichende Datenrate sein. Wer das trennen will, braucht eine zweite
Fixture mit 11 Sektoren bei 1900 ns als Gegenprobe.

**E6 zählt Kopffehler getrennt.** Ohne das fällt E6 (Kopf **und** Daten
falsch) mit E5 (nur Daten falsch) auf dieselbe Zahl guter Sektoren, und
die Abnahme könnte die beiden Fixtures nicht auseinanderhalten.

---

## Das Sitzungsdesign: zwei Achsen, nicht sieben Läufe

Die Sichtung der Vorlage hat die meisten Fragen schon beantwortet — aber
sie hat **eine Fassung** gesehen (5.3). Die Binaries sind **3.4**, **5.21**
und der unbestimmte **01/95**-Stand. Die Sitzung liefert damit genau das,
was die Quelle nicht kann.

### Achse 1 — die Versionsachse

Jede Fixture wird auf **3.4** und **5.21** gefahren. Weichen sie ab, ist
das ein **Varianten-Fund** und keine Ungenauigkeit: zeigt 3.4 für die
leere Spur `1` und 5.21 eine `2`, dann hat sich das Verhalten zwischen
den Fassungen geändert, und jede Aussage „X-Copy tut Y" braucht künftig
die Fassung dazu.

`01/95` läuft als dritter Lauf nur dort, wo 3.4 und 5.21 sich
unterscheiden — sonst kostet er Zeit ohne Aussage.

### Achse 2 — die Betriebsartenachse

Die Vorlage hat **zwei** Vorrangketten: Direktkopie „erster Fehler
gewinnt", RAM-Kopie „letzter gewinnt". Beide Betriebsarten zu fahren
verdoppelt die Läufe — deshalb **nur dort, wo es zählt**:

> **Beide Betriebsarten nur für Fixtures mit mehr als einem Fehler.**
> „Erster gewinnt" und „letzter gewinnt" können sich gar nicht
> unterscheiden, wenn es nur einen Fehler gibt.

| Fixture | Fehler | Läufe |
|---|---|---|
| E1 leer | einer | einmal je Fassung |
| E2 / E2b | einer | einmal je Fassung |
| E3a / E3b | einer | einmal je Fassung |
| E4 `$448A` | einer | einmal je Fassung |
| E5 gerettet | einer, aber über zwei Umdrehungen | **beide Betriebsarten** |
| **E6** Kopf + Daten | **zwei** | **beide Betriebsarten** |

Damit: 8 Fixtures × 2 Fassungen = 16 Läufe, plus 2 × 2 für die
Betriebsarten bei E5 und E6 = **20 Läufe**. Nicht 8 × 2 × 2 = 32.

### Was zuerst

**E1 auf 3.4.** Nicht weil die Antwort unbekannt wäre — sie steht in der
Spec —, sondern weil sie dort für **5.3** steht und 3.4 die älteste
verfügbare Fassung ist. Stimmt sie überein, ist die Versionsachse für
diese Frage geschlossen und man kann die übrigen zügig durchfahren.
Weicht sie ab, ändert das die Reihenfolge aller weiteren Fragen.

---

## Die Fragen, nach Gewicht

### E1 — die Vorrangfrage (**zuerst**)

> **Zeigt X-Copy für eine leere Spur `1` oder `2`?**

Beide Codes treffen zu: die Spur hat *weniger als 11 Sektoren* (Code 1)
**und** *keinen Sync* (Code 2). Kein Handbuch sagt, welche Prüfung zuerst
greift.

* **Fixture:** eine gelöschte Spur — praktisch keine Wechsel.
  Entspricht Fall 1 in `tests/test_flux_nosync_split.c`.
* **Hängt davon ab:** `src/flux/uft_track_verdikt.c`, die
  Vorrangreihenfolge. Dort steht heute *„das Speziellere zuerst"* —
  ausdrücklich als **Setzung, nicht Messung** gekennzeichnet.
* **Warum zuerst:** jede weitere Antwort steht auf dieser. Wer die
  Reihenfolge nicht kennt, kann kein Verhalten bei Mehrfachfehlern
  deuten.

### E2 — greift Code 1 in **beide** Richtungen?

> Eine Spur mit **12** Sektoren — zeigt X-Copy `1`?

Das Handbuch sagt „less or **more** than 11 sectors". Eine einseitige
Schranke sähe die zu vielen Sektoren nicht, und das ist ein klassisches
Schutzmerkmal.

* **Fixture:** Spur mit 12 statt 11 Sektoren.
* **Hängt davon ab:** `uft_track_verdikt_bilden()` prüft heute
  `sector_count != expected_sectors`, also beidseitig — das ist die
  Lesart, die hier zu bestätigen ist.

### E3 — Code 3 gegen eine zu kurze Aufnahme

> Zeigt X-Copy `3`, wenn die **zweite** Umdrehung keinen Sync trägt? Und
> was zeigt es, wenn nur **eine** Umdrehung gelesen wurde?

* **Fixtures:** Fälle (b) und (c) aus `tests/test_flux_splice_pos.c`.
* **Hängt davon ab:** `uft_splice_lage_t` trennt seit MF-769 `FEHLT` von
  `UNBEKANNT`. Ob X-Copy das auch trennt, ist offen — es könnte beides
  als `3` zeigen. **Dann bliebe UFTs Trennung trotzdem richtig**, wäre
  aber nachweislich *feiner* als die Vorlage, und das gehört so
  aufgeschrieben.

### E4 — ist `$448A` eine eigene Marke?

> Erkennt X-Copy `$448A` als eigenes Muster oder behandelt es die Spur
> wie `$4489`?

`UFT_AMIGA_SYNCS` führt beide, weil die Vorlage `$448A` in ihrer
Suchschleife nennt. Ob das im Verhalten sichtbar wird, ist ungeprüft.

* **Fixture:** Spur mit `$448A` statt `$4489`, sonst gleich.
* **Hängt davon ab:** die Mindestzahl 2 und die dünne Spanne 4:3 bei
  `$A245` in `marke_suchen()` — beides Setzungen.

### E5 — wird „gerettet" wirklich anders angezeigt?

> Zeigt **5.21** eine nach Lesefehler gerettete Spur anders als **3.4**?

Das Änderungsprotokoll zu 5.21 behauptet genau diese Korrektur: vorher
erschien eine gerettete Spur als fehlerfrei, und die Autoren stufen das
als falsch ein. **Zwei Fassungen nebeneinander laufen lassen** — das ist
der einzige Punkt, an dem die Sitzung zwei Binaries braucht.

* **Hängt davon ab:** `UFT_REP_GERETTET` schlägt in
  `uft_track_verdikt.c` alles andere. Die Regel ist aus dem Protokoll
  gelesen, nicht am Verhalten gemessen.

### E6 — welche Ziffer gewinnt bei mehreren Fehlern?

> Eine Spur mit Kopf-**und** Datenprüfsummenfehler — zeigt X-Copy `4`
> oder `6`?

Ergänzt E1 um den allgemeinen Fall.

* **Fixture:** eine Spur, beide Zähler > 0.
* **Hängt davon ab:** dieselbe Vorrangkette.

### E7 — der Schreibstartpunkt (**nur mittelbar beobachtbar**)

> Woran erkennt man von außen, wo X-Copy zu schreiben beginnt?

Nicht am Schirm. Messbar nur über eine **geschriebene** Diskette, die
danach zurückgelesen wird — und dafür braucht es Hardware, die dieses
Projekt nicht hat (MF-310).

**Ehrlich: E7 ist in dieser Sitzung nicht beantwortbar.** Es steht hier,
damit niemand es für vergessen hält. Der zugehörige offene Punkt ist
**P3-7** in `OPEN_ITEMS.md`.

---

## Was die Sitzung **nicht** tut

* **Sie öffnet keinen Quelltext.** Das Binary wird ausgeführt und
  beobachtet; `xcop.s`, `xio.s` und die `.lha`-Archive bleiben in dieser
  Sitzung zu. Der Grund ist nicht Vorsicht, sondern die Beweisführung:
  der Nachbau behauptet, *nicht* aus der Vorlage abgeleitet zu sein, und
  diese Behauptung ist nur so viel wert wie die Grundlinie, die sie
  stützt (`tools/uft-nachbau/scripts/sichtprotokoll.py`).

  **Der Eigentümer hat am 2026-09-01 entschieden, die Quellen von einer
  ZWEITEN HAND öffnen zu lassen** — dem `uft-nachbau`-Agenten, der nur
  Dokumente liefert und nie nach `src/`, `include/` oder `tests/`
  schreibt. Das ist kein Widerspruch zur Zeile darüber, sondern genau
  die Bauform der Brandmauer: Hand A liest und beschreibt **Verhalten**,
  Hand B implementiert, ohne die Vorlage gesehen zu haben. Was Hand A
  liefert, sind Verhaltenstatsachen — keine Codezeilen, keine
  Bezeichner, keine Strukturen. Ihr Bericht steht in
  `XCOPY_VERHALTEN_HAND-A2.md`.
* **Sie ändert nichts an `src/`.** Ergebnisse landen hier und in
  `XCOPY_TRIPEL_TABELLE.md`; die Code-Änderung ist ein eigener Schritt
  mit eigenem Rotbeweis.

## Was danach zu tun ist

Je Frage genau eins:

1. Antwort in dieses Blatt, mit Fassung und Datum.
2. Betroffene Setzung in `uft_track_verdikt.c` von **„Setzung"** auf
   **„gemessen an X-Copy \<Fassung\>"** heben — oder ändern, wenn die
   Messung ihr widerspricht.
3. Der zugehörige Test bekommt die Fassung in den Kopfkommentar.

Eine Antwort, die keine dieser drei Zeilen auslöst, war keine Frage
wert — dann gehört sie gestrichen, nicht beantwortet.

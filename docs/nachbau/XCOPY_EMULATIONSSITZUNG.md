# X-Copy unter Emulation — Sitzungsprotokoll

**Werkstatt-Dokument. Stand 2026-09-01 (MF-771).**

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

**E2 und E6 fehlen noch.** Sie brauchen eine additive Erweiterung von
`tests/flux_gen/amigados/`: eine wählbare Sektorzahl (12 statt 11) und
einen Defekt am **Kopf** statt an den Daten. Klein, aber ein eigener
Schritt mit eigener Abnahme.

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

# Herkunft, Lizenz und Kanal — `tools/uft-retrace`

Eingebaut MF-1332, 2026-09-21, auf ausdrueckliche Anweisung
(„pruefe es und wenn es was bringt bau es ein, so sind die regeln").

## Woher

    neue-ideen/UFT_ReTrace_Entpack_Tracing_Workflow.zip
    SHA-256  a8bc0692c5d453882ad972f330320e6667cb4cb6421aa4db4408a415ba4f93cb
    32 Dateien, 50 743 Byte entpackt, davon 14 Python-Module

Eigentuemer-Zulieferung. Ausgepackt mit EIGENER Pfadpruefung, statt der
Zusage des Pakets („sichere ZIP-Extraktion ohne Pfadtraversal") zu
glauben — kein Eintrag verliess das Zielverzeichnis.

## Lizenz — an der DATEI gemessen

**GPL-2.0-or-later.** Beleg: `LICENSE` IM PAKET, 330 Byte, SPDX-Zeile
`GPL-2.0-or-later`, vollstaendig gelesen. Vereinbar mit dem GPL-2-Baum.

Die Lizenzdatei trennt ausdruecklich, was sie NICHT abdeckt:

> This license covers only the files in this package. It does not grant
> rights to any third-party binary, disk image, disassembly, or other
> input processed with the workflow.

`docs/LEGAL_CLEANROOM.md` im Paket fuehrt das aus — historische
Kopierprogramme, Kickstart-ROMs, Diskettenimages, erzeugte
Disassemblies, nahezu woertliche Rekonstruktionen — und benennt genau
die Falle, in die dieser Baum bei LisaEm und `fdtc` selbst getappt ist:

> Eine MIT-Datei neben einer Sammlung beweist nicht automatisch, dass
> der Sammler die enthaltenen historischen Programme unter MIT stellen
> durfte.

**Historische Programme sind NICHT mitgeliefert** und gehoeren auch
nicht hierher.

## Kanal nach MF-695: **Helfer-Prozess**

Ausfuehrbar, nicht einlinkbar. Es ist ein Python-Werkzeug NEBEN dem
Produkt — aus ihm kommt kein Code in `src/`. Was es liefert, ist eine
Clean-Room-Uebergabe mit Provenienz: die messende Seite dokumentiert
Verhalten, die implementierende Seite bekommt Beschreibung und
synthetische Fixtures, aber keine historischen Maschinenbytes.

Damit bedient es **Weg 2** aus `docs/QUARANTINE_PROCESS.md` §5 und
steht neben `tools/uft-nachbau`, das dieselbe Zwei-Haende-Brandmauer
fuer eine einzelne Vorlage aufbaut.

## Kennzahl nach MF-640: **Fundus**

Keine der vier Release-Zahlen bewegt sich. Es ist eine METHODE, kein
Formatbeleg und kein Wandlungspfad. Bewegen KANN sein Ertrag spaeter
etwas — wenn ein Trace eine Regel belegt, die in `src/recovery/`
einzieht —, die Sichtung selbst tut es nicht.

## Gemessen, nicht zugesichert (MF-1178)

* eigene Tests: **10 von 10**, am neuen Ort erneut gelaufen;
* die Kette `inventory -> analyze -> report` laeuft an einer echten
  Eingabe durch und erzeugt Quell-SHA-256, je Datei einen SHA-256,
  Hunk-Kennzeichen und einen schema-getaggten Bericht
  (`uft-retrace-inventory-1`);
* keine externen Python-Pakete noetig (Python >= 3.10).

## Vier Behauptungen ueber UFT — drei halten, eine nicht

`docs/UFT_INTEGRATION.md` im Paket nennt Anknuepfungspunkte im Baum.
Nachgemessen am 2026-09-21:

| Behauptung | gemessen |
|---|---|
| `src/recovery/uft_multiread_pipeline.c` | **existiert** |
| zentrale Amiga-Synctabelle | **existiert** — `include/uft/formats/uft_amiga_syncs.h` (+ `.c`) |
| `uft_copy_finding_t` | **existiert** — `include/uft/core/uft_copy_plan.h:278` |
| Empfehlung traegt „benoetigte Faehigkeiten, Verlustmeldung, Messquelle, Konfidenz" | **HAELT NICHT** |

Die vierte ist der eigentliche Befund: `uft_copy_finding_t` hat
gemessen **genau drei** Felder — `hard`, `id`, `text` — und traegt
keines der fuenf. Der Anknuepfungspunkt ist echt, seine Gestalt nicht.

Das Paket setzt ausserdem einen Satz voraus, den der Baum teilt:
*„Eine Empfehlung darf eine Benutzerauswahl nicht heimlich
ueberschreiben."* `uft_copy_plan_t` traegt seit MF-1311 `caps` und
`caps_bekannt` — die Dreiteilung, die genau das leisten soll. Die
Befundseite hat sie noch nicht.

**Erweitern ist NICHT gebaut**, sondern vorgelegt: fuenf Felder an
einer Kernstruktur sind neuer Code auf Grund eines Vorschlags, nicht
auf Grund eines gemessenen Defekts. Das gehoert entschieden, nicht
nebenbei erledigt.

## Was hier NICHT hineingehoert

* historische Kopierprogramme, ROMs, Diskettenimages;
* erzeugte Disassemblies;
* Traces, die aus einem Lauf mit solchen Eingaben stammen — sie
  gehoeren in den gitignorierten Arbeitsbereich, nicht in den Baum.

Das Werkzeug legt seine Ergebnisse standardmaessig unter `work/` bzw.
`output/` ab; beides bleibt aus dem Baum heraus.

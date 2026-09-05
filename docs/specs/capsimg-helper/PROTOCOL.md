# IPF-Helfer — Protokoll v1

> **Was dieses Dokument ist.** Der Vertrag zwischen UFT und einem
> externen Programm, das IPF-Dateien deutet. UFT implementiert die
> **linke** Seite (`src/formats/ipf/uft_ipf_helper.c`, gemessen in
> `tests/test_ipf_helper.c`). Die **rechte** Seite — das Programm — ist
> **nicht Teil dieses Baums** und wird es nie sein.
>
> **Warum getrennt.** `docs/QUARANTINE_PROCESS.md` §5, Weg 3. Das
> IPF-Format ist bewusst undokumentiert, ein Clean-Room-Neubau aus einer
> Spezifikation also versperrt. Die offizielle Bibliothek trägt eine
> Lizenz, die mit dem Baum unvereinbar ist (siehe §1). Also:
> Prozessgrenze.

---

## 1. Die Lizenzlage, gemessen

Quelle: `LICENCE.txt` im Paket **SPS DECODER LIBRARY v5.1**
(github.com/simonowen/capsimage, Lizenzfassung v1.02 vom 2014-05-20).
Wörtlich:

> „Redistributions may not be sold, nor may they be used in a commercial
> product or activity."

und in den *Common Questions*:

> „Q. Can I include the SPS DECODER LIBRARY with my commercial product?
>  A. No. … Using SPS DECODER LIBRARY as a ‚freebie' or including it at
>  ‚no cost' with your product still constitutes commercial usage and is
>  forbidden by the licence."

Ein Verkaufs- und Kommerzvorbehalt ist eine **zusätzliche Beschränkung**
im Sinne von GPL §6. Damit ist die Bibliothek **GPL-inkompatibel** —
dieselbe Rechtslage wie bei XCopy Pro (MF-746). Das ist keine
Rechtsverletzung, sondern eine Unvereinbarkeit, und sie hat genau drei
Folgen:

| | |
|---|---|
| **kein Einlinken** | die Bibliothek wird nie Teil eines UFT-Binärs |
| **keine Quelle im Baum** | kein `capsimg`-Code, in keiner Form, auch nicht abgeleitet |
| **keine Auslieferung** | UFT-Pakete enthalten den Helfer nicht; der Benutzer installiert ihn selbst und stimmt dabei der Lizenz der SPS zu |

Der Helfer trägt seine Lizenz **selbst**. Dieser Baum bleibt unberührt.

---

## 2. Der Datenfluss-Schnitt

Die Regel aus §5 lautet: *die Quelle bleibt bei UFT, der Helfer
interpretiert nur*. Konkret:

```
        ┌──────────────────────── UFT (GPL-2.0-or-later) ───────────────┐
        │  öffnet die .ipf-Datei, prüft sie, besitzt sie                │
        │  legt das Verzeichnis für die Beilage an                      │
        └───────────────────────────┬───────────────────────────────────┘
                                    │  nur PFADE, keine Daten
                                    ▼
        ┌──────────────── Helfer (eigene Lizenz, außerhalb) ────────────┐
        │  liest die .ipf, ruft capsimg, schreibt                       │
        │    · nach stdout: den TEXT-Index (dieses Protokoll)           │
        │    · in die Beilage: die Nutzdaten, roh, ohne Kopf            │
        └───────────────────────────┬───────────────────────────────────┘
                                    │  Text + Beilage
                                    ▼
        ┌──────────────────────── UFT ──────────────────────────────────┐
        │  zerlegt, prüft, füllt uft_track_t                            │
        └───────────────────────────────────────────────────────────────┘
```

**Warum eine Beilage statt aller Daten über stdout.** Nutzdaten sind
binär; über einen Textkanal müssten sie kodiert werden (Hex verdoppelt,
Base64 wächst um ein Drittel), und ein Kodierfehler wäre eine stille
Datenveränderung — genau das, was dieses Projekt ausschließt. Der Baum
hat für die Trennung bereits ein Muster: der FluxEngine-Läufer liest
seine SCP-Ausgabe ebenfalls aus einer Datei, nicht aus der Pipe
(`fluxengine_provider_v2.cpp`).

---

## 3. Der Text-Index

Zeilenweise, ASCII, `\n` als Trenner (`\r\n` wird geduldet). Erste Zeile
ist die Kennung, letzte Zeile ist `END`. Felder sind durch **ein oder
mehrere** Leerzeichen getrennt. Zeilen, die mit `#` beginnen, sind
Kommentar.

```
UFT-IPF-HELPER 1
BLOB /pfad/zur/beilage.bin
CYLS 84
HEADS 2
PLATFORM 1
TRACK 0 0 101376 2 0 0 12672
TRACK 0 1 101376 2 0 12672 12672
TRACK 1 0 101376 3 1 25344 12672
END
```

### Zeilen

| Zeile | Pflicht | Bedeutung |
|---|---|---|
| `UFT-IPF-HELPER <v>` | **ja**, erste Zeile | Protokollfassung. UFT lehnt jede ab, die es nicht kennt — **abwärts wie aufwärts**. Ein Helfer, der v2 spricht, wird nicht halb verstanden. |
| `BLOB <pfad>` | nein | Beilage mit den Nutzdaten. Fehlt sie, darf keine `TRACK`-Zeile eine Länge > 0 nennen. |
| `CYLS <n>` | **ja** | Zylinderzahl, 1…255 |
| `HEADS <n>` | **ja** | Kopfzahl, 1…2 |
| `PLATFORM <n>` | nein | Plattformkennung der IPF-Datei (0 = unbekannt) |
| `TRACK …` | nein | eine Spur, Felder siehe unten. Keine `TRACK`-Zeile heißt: Datei erkannt, keine Spur lesbar — ein **gültiges** Ergebnis. |
| `ERROR <text>` | nein | der Helfer konnte nicht deuten; UFT gibt @p text unverändert weiter |
| `END` | **ja**, letzte Zeile | ohne sie gilt die Antwort als abgeschnitten |

### `TRACK`-Felder

```
TRACK <cyl> <head> <bits> <density> <flags> <blob_off> <blob_len>
```

| Feld | Typ | Bedeutung |
|---|---|---|
| `cyl` | `int ≥ 0` | Zylinder |
| `head` | `int ≥ 0` | Kopf |
| `bits` | `uint32` | Spurlänge in Zellen; `0` = nicht ermittelt |
| `density` | `uint32` | Dichtekennung, wie in der Datei. `≥ 3` benennt eine Schutzart (siehe `ipf_air_density_name`) |
| `flags` | `uint32` | `track_flags` der Datei; Bit 0 = unscharfe Bits |
| `blob_off` | `uint64` | Versatz in der Beilage |
| `blob_len` | `uint32` | Länge in Byte; `0` = keine Nutzdaten für diese Spur |

**Der Helfer erfindet nichts.** Kann er ein Feld nicht ermitteln, meldet
er `0` — er schätzt nicht, und er lässt die Zeile nicht weg, solange die
Spur existiert. Die Unterscheidung „Spur fehlt" ↔ „Spur da, Inhalt
unbekannt" ist forensisch, nicht kosmetisch.

### Rückgabewert

| Wert | heißt |
|---|---|
| `0` | Antwort gültig, `END` geschrieben |
| `≠ 0` | Abbruch; UFT verwirft die Antwort und meldet den Rückgabewert **und** die Ausgabe |

---

## 4. Wie UFT sich verhält — die vier Fälle

Gemessen in `tests/test_ipf_helper.c`, jeder Fall mit eigener Prüfung:

| Fall | UFT-Antwort | Was der Benutzer sieht |
|---|---|---|
| kein Helfer eingerichtet | `UFT_ERR_NOT_SUPPORTED` | „IPF erkannt — Inhalt nicht lesbar. Helfer einrichten: `UFT_IPF_HELPER=<pfad>`" |
| Helfer startet nicht | `UFT_ERROR_TOOL_FAILED` | der Pfad, der nicht startete |
| Helfer bricht ab (`≠ 0`) | `UFT_ERROR_TOOL_FAILED` | Rückgabewert **und** seine Ausgabe |
| Antwort unverständlich | `UFT_ERR_FORMAT_INVALID` | welche Zeile und warum |

**Niemals** eine leere Spur, eine erfundene Geometrie oder ein stilles
`UFT_OK`. Das ist der Punkt, an dem Weg 3 steht oder fällt: eine
Prozessgrenze, deren Ausfall nach Erfolg aussieht, ist schlimmer als gar
keine.

Den Helfer findet UFT **ausschließlich** über `UFT_IPF_HELPER`. Der
`PATH` wird bewusst nicht durchsucht — ein Programm, das zufällig so
heißt, wäre sonst eine stille Entscheidung darüber, wer unsere Abbilder
deutet.

---

## 5. Prüfvektoren für den Helfer

Wer den Helfer baut, misst ihn hiergegen. UFTs Seite ist damit bereits
gemessen; diese Liste ist die **rechte** Seite.

| # | Eingabe | Erwartung |
|---|---|---|
| H1 | eine IPF-Datei, die `capsimg` öffnet | `0`, `END`, `CYLS`/`HEADS` gleich den Werten, die `CAPSGetImageInfo` liefert |
| H2 | dieselbe Datei zweimal | byteidentischer Index **und** byteidentische Beilage |
| H3 | eine Datei, die `capsimg` ablehnt | `≠ 0`, `ERROR`-Zeile mit dem Grund, **keine** `TRACK`-Zeile |
| H4 | eine abgeschnittene IPF | wie H3 — nie eine Teilantwort ohne `END` |
| H5 | Beilagenpfad nicht schreibbar | `≠ 0`, `ERROR`, kein Index |
| H6 | Spur mit unscharfen Bits | `flags` Bit 0 gesetzt |

**Zirkularität ausschließen** (`ORACLES.md`, fünfte Frage): der Helfer
und die IPF-Dateien im Prüfkorpus dürfen nicht dieselbe Hand sein. IPF
erzeugt ohnehin nur die SPS — die Dateien kommen also von dort, der
Helfer von uns, und `capsimg` deutet. Drei Hände.

---

## 6. Was hier **nicht** steht, und warum

Dieses Dokument beschreibt **UFTs Bedarf**, nicht `capsimg`s API. Die
Feldnamen oben stammen aus den vorhandenen Zugriffsfunktionen des Baums
(`ipf_air_get_track_meta`, `ipf_air_get_track_raw`), nicht aus fremden
Datenstrukturen. Das ist Absicht: ein Protokoll, das die fremde API
nachzeichnet, wäre eine Ableitung mit Zwischenschritt — genau das, was
§5 Weg 2 verbietet („nicht aus dem fremden Quelltext abschreiben, sonst
ist der Neubau ein Port mit Zwischenschritt").

Ebenso fehlt hier **jede Aussage darüber, was `capsimg` tatsächlich
liefert.** Auf der Maschine, auf der dieses Dokument entstand, liegt
kein `capsimg` (gemessen: `which capsimg` leer, keine `CAPSImg.dll` im
System). Wer den Helfer baut, misst H1–H6 und trägt das Ergebnis hier
nach.

---

## 7. Stand

| | |
|---|---|
| **UFT-Seite** | gebaut und gemessen — MF-917 |
| **Helfer** | offen; siehe `docs/OPEN_ITEMS.md` P3-190 |
| **Kennzahl** | fünfte (Dateien mit ungeklärter Herkunft) — der fertige Helfer macht `src/formats/ipf/uft_ipf_air.c` entbehrlich und schließt damit die teuerste Zeile in `QUARANTINE.md` |

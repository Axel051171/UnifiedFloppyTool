# Schreib-Stufen W0–W4

> **Warum es diese Datei gibt.** `docs/VERIFICATION_TIERS.md` führt T1/T1b/T2/T3
> je Format — und diese Stufen messen **ausschließlich das Lesen**. Ein Format
> auf T1b sagt: eine fremde Hand hat ein Abbild erzeugt, und UFT liest es
> richtig. Über die **Schreibseite** sagt es nichts, und der Baum hat
> dreimal gemessen, wie weit die beiden auseinanderliegen:
>
> * **MF-883** — neun Formate meldeten `UFT_OK`, änderten ihre
>   Speicherkopie und gaben sie beim Schließen frei. `plugin->flush` hat im
>   ganzen Baum **keinen Aufrufer**.
> * **MF-930** — es waren nicht neun, sondern **zwanzig**, und der Torkopf
>   selbst hat den Rest verdeckt, weil er acht Verdächtige *aufzählte*
>   statt zu messen.
> * **MF-931/1095/1112/1117/1119** — fünf einzelne Verdrahtungen, jede mit
>   der Lehre „die Schreibseite ist in diesem Baum systematisch die
>   ältere".
>
> Dazu die strukturelle Lücke **P3-154**: `test_capability_manifest.c`
> prüft `plugin->write_track != NULL`. Das belegt einen **Funktionszeiger**,
> nicht dass die Änderung die **Datei** erreicht.

## Herkunft dieser Datei — eine Einschränkung, die dazugehört

Die Stufen**namen** W0–W4 stammen aus der Audit-Vorgabe des Eigentümers.
Die Stufen**definitionen** unten sind **hier abgeleitet**, nicht von dort
übernommen: die ursprüngliche Formulierung ist in keinem der
Sitzungsprotokolle auf dieser Maschine mehr auffindbar. Abgeleitet sind sie
aus dem, was dieser Baum an Belegen wirklich unterscheiden kann — derselben
Logik der „fremden Hand", die die T-Stufen tragen. **Sie warten auf
Eigentümer-Bestätigung**; weicht die Vorgabe ab, gewinnt die Vorgabe, und
die Zuordnungen unten sind dann umzuschreiben, nicht zu löschen (MF-1077).

## Die Leiter

| Stufe | Bedingung | Was sie ausschließt |
|---|---|---|
| **W0** | `UFT_FORMAT_CAP_WRITE` ist zugesagt, es gibt **keinen** Durchschreibfall. | nichts — das ist die offene Schuld |
| **W1** | Die Änderung erreicht die **Datei**: geschrieben, `close()`, **neu geöffnet**, und jeder Sektor kommt geändert zurück. Der eigene Leser genügt. | MF-883/MF-930 — die Zusage ohne Tat |
| **W2** | Ein **zweiter Leser im Baum** liest die von UFT geschriebene Datei zurück. | eine Versatzformel, die Schreiber und Leser gemeinsam falsch haben (Klasse MF-1009/1028: Packer und Entpacker als Spiegelbilder derselben Erfindung) |
| **W3** | Eine **fremde Hand** liest die von UFT geschriebene Datei zurück und bestätigt Geometrie, Sektornummern und Inhalt. | ein Hausformat, das nur UFT versteht — Klasse MF-1032 (`logical` schrieb einen 32-Byte-Kopf, den es nicht gibt) und MF-1035 (`rcpmfs`) |
| **W4** | Die von UFT geschriebene Datei ist **byteidentisch** mit der, die eine fremde Hand aus derselben Eingabe schreibt. | jede stille Abweichung in Polsterung, Füllbyte, Reihenfolge oder Erzeugerfeld |

**W1 ist keine Formalie, und das ist gemessen.** Beim Einbau der elf neuen
Fälle (MF-1138) wurde für `trd` der `fwrite`-Pfad vorsätzlich unerreichbar
gemacht: von 2560 Sektoren stimmten danach **10**, 2550 wichen ab. Die
zehn treffen zufällig — genau deshalb vergleicht die Probe **je Sektor**
und nicht die Dateigröße oder eine Summe (Lehre aus MF-1026: eine Summe,
die aufgeht, sagt nichts über die Verteilung darin).

**W4 ist nicht immer stärker als W3.** MF-1084 hat an `opd` gemessen, dass
ein Lauf durch dasselbe Format die Bytes durchreichen kann — dann ist
Gleichheit aussagelos. W4 zählt nur, wenn zwischen Ein- und Ausgang ein
echtes Modell liegt (bei `nanowasp`, MF-1033, waren es **zwei unabhängige
Schreiber auf dieselben 409 600 Byte**, und dort ist die Gleichheit der
Beleg).

## Stand, gemessen

Quelle ist `scripts/audit_schreibfaelle.py` (81. Kategorie in
`check_consistency.py`); die Dateimenge kommt aus `git ls-files` (MF-636),
die Zusagen aus dem Feld `.capabilities` jedes Plugins.

| | Zahl |
|---|---|
| Plugins mit `UFT_FORMAT_CAP_WRITE` | **59** |
| davon **W1** (Durchschreibfall vorhanden) | **32** |
| davon **W0** (offene Schuld) | **27** |
| Fall ohne Zusage | **2** (`atx`, `imd`) |

**W1 (32):** adf, apridisk, atr, cfi, d64, d71, d80, d81, d82, dc42, do,
g64, hardsector, img, jv1, mgt, micropolis, msx_disk, myz80, nanowasp,
northstar, pdp, po, posix, qrst, sam, ssd, st, t1k, tan, trd, xfd

**W0 (27), die Arbeitsliste:** 2img, adf_arc, adl, akai_s900, d13, d67,
d77, dim, dim_atari, dsk_cpc, edk, fdi, fdi_pc98, fds, hfe, jv3, jvc,
korg_dss1, lisa_twiggy, nfd, opus, sad, syn, v9t9, vdk, victor9k, xdm86 —
die laufende Fassung steht in `docs/schreibfaelle_baseline.txt`, eine Zeile
je offene Schuld.

**W2/W3/W4 sind noch nicht je Format gemessen.** Es wäre falsch, hier
Zahlen hinzuschreiben: die Belege existieren teilweise, aber für die
**Lese**richtung. `logical` (MF-1032), `posix` (MF-1034) und `nanowasp`
(MF-1033) sind die drei aussichtsreichsten W3/W4-Kandidaten, weil dort
libdsk die Prüfdateien **geschrieben** hat und der Rundlauf byteidentisch
ist; das als W4 zu führen verlangt aber, die Richtung ausdrücklich zu
messen — UFT schreibt, libdsk liest — und genau das ist noch nicht getan.

## Warum 27 nicht einfach abzuarbeiten sind

Die elf aus MF-1138 gingen, weil sie **kopflos** sind und ihre Geometrie
aus der Dateigröße gewinnen: `tests/test_durchschreibprobe.c` baut sein
Prüfabbild selbst. Für ein Format mit Behälterkopf gibt es diesen Weg
nicht — es braucht entweder

* eine **Korpusdatei** als Ausgangspunkt (so lösen es `myz80` und `qrst`,
  deren Eingabe libdsk geschrieben hat, MF-1033), oder
* einen **`create`-Weg**, der die Datei von Null aufbaut.

Das zweite ist die **C-Leiter** (C0–C4) und eine eigene Achse: ein Format
kann schreiben können, ohne anlegen zu können. Sie ist hier ausdrücklich
**nicht** definiert, weil dieselbe Einschränkung wie oben gilt und eine
zweite erfundene Leiter die Lage nicht verbessert.

**Eine Messung ist dabei berichtigt worden und steht hier, damit sie nicht
wiederholt wird.** Die Kandidaten der zweiten Charge wurden zuerst danach
ausgewählt, ob in der Plugin-Datei ein `fwrite` und ein schreibbares
`fopen`-Handle stehen. Gemessen trifft das auf **34 von 34** zu — die
Hausform `fopen(path, ro ? "rb" : "r+b")` steht in fast jedem Plugin, und
die **Erreichbarkeit** des Schreibers misst ohnehin Tor 57 (Grundlinie 0).
Ein Merkmal, das bei allen zutrifft, unterscheidet nichts; es war ein
Erkenner, der nicht „nein" sagen kann.

## Verwandte Dokumente

* `docs/VERIFICATION_TIERS.md` — die Lese-Stufen T1/T1b/T2/T3
* `docs/schreibfaelle_baseline.txt` — die W0-Liste, mechanisch gehalten
* `docs/OPEN_ITEMS.md` P3-154 / P3-157 — was Tor 57 bewusst nicht sieht
* `docs/OPEN_ITEMS.md` P3-204 — die fertigen, unverdrahteten Schreiber

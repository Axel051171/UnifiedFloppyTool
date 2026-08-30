# uft-nachbau — die Werkstatt

Vierte Säule neben `uft-scout` (findet), `uft-variants` (versioniert),
`uft-innendienst` (misst den eigenen Baum): bereitet den
Clean-Room-Nachbau fremder Vorlagen vor. Kern ist die
Zwei-Hände-Brandmauer — Hand A (Werkstatt) vermisst und schreibt die
Spec, Hand B (MF-Workflow) implementiert und sieht **nur** die Spec.
Regeln: [`AGENT.md`](AGENT.md).

Sie erfindet keinen Prozess: sie bedient **Weg 2** aus
[`docs/QUARANTINE_PROCESS.md`](../../docs/QUARANTINE_PROCESS.md) §5, und
ihr Beweismaß ist §4 *Das Herkunftsaudit*.

## Stand — gemessen am 2026-08-30

| Stufe | Werkzeug | Lauf |
|---|---|---|
| 1 Triage | AGENT.md-Routen | GRÜN→Port · GELB→Nachbau · ORANGE→Helper · ROT→Blackbox · lizenzloser Zeitraum ⇒ wie ROT |
| 2 Messlauf | `scripts/messlauf.py` | Evidenz mit `M-NNNN`-IDs + SHA-256 je Ein-/Ausgabe |
| 3 Spec | `scripts/spec_gen.py` | Entwurf aus Evidenz; die urteilenden Zeilen bleiben `UNGEKLÄRT` |
| 4 Brandmauer | `scripts/kontamination.py` | **Selbsttest 3/3**, darunter der schwere Fall · Realprobe `uft_hfe` gegen `samdisk/hfe.cpp`: **1** Befund (9 Hausvokabular, 17 Weißlisten-Fakten) |
| 5 Grundlinie | `kontamination.py --grundlinie` | `samdisk/hfe.cpp`: 100 Bezeichner, 35 Strings, 43 Kommentar-Wortfolgen |

```bash
python tools/uft-nachbau/scripts/kontamination.py --selbsttest
python tools/uft-nachbau/scripts/kontamination.py --grundlinie <vorlage>
python tools/uft-nachbau/scripts/kontamination.py <neubau> <vorlage> \
       --weissliste tools/uft-nachbau/data/<fmt>_weissliste.txt
```

## Was bei der Übernahme geändert wurde (MF-696)

Der gelieferte Werkzeugkasten hatte die richtige Doktrin — die
Zwei-Hände-Trennung, die Lizenz-Routen, „Fakten ja, Ausdruck nie". Fünf
Messungen zeigten, wo die **Umsetzung** dahinter zurückblieb, und vier
davon wirkten in die gefährliche Richtung (falsche Negative):

| | Befund | gemessen | Folge |
|---|---|---|---|
| **A** | Der Selbsttest-Eigenbau war auf **Deutsch** geschrieben und teilte darum trivial keinen Bezeichner. Er belegt, dass das Werkzeug identische Namen sieht — **nicht**, dass es Nachbau von Port unterscheidet. Genau das ist aber seine einzige Aufgabe. | 2 Fälle, beide leicht | dritter Fall `unabhaengig_en.c`: unabhängige **englische** Reimplementierung, teilt Fachvokabular (`read_track`, `track_count`), muss freigesprochen werden. Selbsttest **3/3** |
| **B** | `ALLGEMEIN` — eine **gepflegte Liste** von Bibliotheksvokabular (`fopen`, `header`, `offset`, …). Die Aufzählung bekannter Fälle, die dieser Baum neunmal veralten sah, und sie unterdrückt Befunde: ein Eintrag zuviel kostet einen echten Fund. | 44 Einträge | ersetzt durch ein **abgeleitetes** Maß nach §4: wie viele **andere** Dateien des eigenen Baums kennen den Namen? `0` = die `carryshift`-Klasse (erfunden, beweiskräftig) · `≥5` = Hausvokabular. Aktualisiert sich selbst |
| **C** | Die HFE-Weißliste traf ihre eigenen Felder nicht: `number_of_side`/`number_of_track` (Singular) und `formatrevision` (ohne Unterstrich) gegen `number_of_sides`, `number_of_tracks`, `format_revision` im Code. Drei Spec-Fakten standen als Befund da — und das README erklärte sie zu „Spec-Fakten in Zweitschreibung", also zu etwas, das man übergeht. | 3 von 5 Resten | **normalisierter** Vergleich (klein, ohne Unterstriche, ohne Plural-s) + Meldung der Einträge, die **nirgends** treffen |
| **D** | README: „13→5 nach Bibliotheks-Filter". Gemessen waren es **15→5**. | — | alle Zahlen oben aus einem Lauf |
| **E** | `AGENT.md` Regel 6 verlangt eine **Kontaminations-Grundlinie vor der Übergabe** — einen Modus dafür gab es nicht. Eine Zusage ohne Werkzeug, dieselbe Klasse wie das nie existierende `zensus_vorlage.py` in MF-693. | — | `--grundlinie` gebaut |

Dazu drei kleinere: `QUARANTAENE_VERFAHREN §4` existiert nicht — die
Datei heißt `docs/QUARANTINE_PROCESS.md` (und hat den §4, den der Agent
meint); `messlauf.py` benutzte `tempfile.mktemp()` mit seinem Wettlauf
zwischen Name und Benutzung; und der Agent selbst fehlte —
`.claude/agents/uft-nachbau.md` ist neu.

**Eine eigene Korrektur mitten in der Anpassung.** Der erste Ersatz für
die Weißlisten-Tippfehler war eine „Beinah-Treffer"-Heuristik über
Teilzeichenketten. Gemessen war sie selbst eine Lücke: `encoding` und
`offset` wurden von den längeren Einträgen `track0s0_altencoding` und
`track_list_offset` verschluckt und verschwanden als Befund. Ein
Mechanismus, der nur falsche Negative erzeugt, ist weg — die
Normalisierung fängt alle drei realen Fälle ohnehin, und die
Gegenrichtung („welcher Eintrag trifft nirgends?") kann nichts
unterdrücken.

## Was das Werkzeug nicht sieht

Drei der sieben beweiskräftigen Klassen aus §4: Funktionszerlegung und
Aufrufreihenfolge, Fehlerbehandlungs-Idiome, charakteristische
Schwellwerte ohne Spec-Grundlage. **Null Befunde sind die notwendige,
nie die hinreichende Bedingung** — das Herkunftsaudit bleibt Handarbeit.

## Der eine offene Befund der Realprobe

`HFE_SIGNATURE` teilen `src/formats/hfe/uft_hfe.c` und
`src/samdisk/hfe.cpp`; der Name steht in nur **2** weiteren Dateien
unseres Baums, fällt also unter `PRÜFEN` und nicht unter Hausvokabular.
Er ist **nicht** weißgelistet worden: ihn wegzuerklären wäre genau der
Fehler, den Befund C beschreibt. Der Fall gehört ins Herkunftsaudit —
oder auf die Weißliste, wenn jemand belegt, dass HxC den Bezeichner
selbst so nennt.

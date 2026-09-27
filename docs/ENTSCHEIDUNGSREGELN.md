# Entscheidungsregeln

Eigentümerentscheidung vom 2026-09-20.

Die Sitzung entscheidet selbst, was hier abgedeckt ist, und meldet die
Nummer. Sie fragt nur, wenn keine Regel greift oder zwei sich widersprechen.
Jede beantwortete Frage wird zur Regel — im selben Commit.

## Warum es diese Datei gibt

Eine Sitzung fragt nicht aus Unsicherheit, sondern weil ihre Anweisung
sagt: entscheide nichts, was du nicht entscheiden kannst. Und was sie
entscheiden kann, ist nur das, wofür eine Regel geschrieben steht.

Die Regeln unten sind in Gesprächen gefallen und standen nirgends im Baum.
Das war der Fehler: **eine Regel, die nur in einem Gespräch steht, ist für
die nächste Sitzung keine.** Jede der ersten zwölf hat mindestens eine
Nachfrage erzeugt, die sie jetzt beantwortet, bevor sie gestellt wird.
E-13 bis E-15 kamen am 2026-09-26 aus drei Eigentümerentscheidungen dazu
(dort als „E-16 bis E-18" vorgeschlagen; vergeben sind die nächsten
freien Nummern, damit keine Lücke entsteht).

Der Mechanismus hat drei Teile:

1. **Diese Datei.** Was hier steht, entscheidet die Sitzung selbst und
   meldet „entschieden nach E-7" statt zu fragen.
2. **Die Ratsche.** Fragt die Sitzung doch, war keine Regel da. Die
   Antwort wird zur Regel, mit Nummer, **im selben Commit**. Dieselbe
   Frage kann nie zweimal kommen — genau wie ein behobener Fund die
   Grundlinie verlässt.
3. **Die eine erlaubte Frage.** Wenn zwei Regeln sich widersprechen, oder
   wenn eine Regel eine Tatsache braucht, die nur der Eigentümer hat
   („tut dieser Dialog etwas, das kein Parameter ist?"). Alles andere
   entscheidet die Sitzung.

## Die Regeln

**E-1  Eine Umsetzung je Begriff.** Gibt es zwei Leser, zwei Kettenläufer,
zwei Prüfungen für dasselbe, bleibt die mit Test und Aufrufern; die andere
fällt. Nicht reparieren, was gelöscht wird.
*(`uft_td0_read_mem` gegen den Plugin-Leser; `uft_msx.c` gegen `uft_fat12.c`)*

**E-2  Wer den Aufrufer entfernt, entfernt den Aufgerufenen im selben
Commit.** Nie eine übersetzte Klasse ohne Konstruktion zwischen zwei
Commits.
*(die drei Advanced-Dialoge, Tor 68)*

**E-3  Eine Zusage ohne Beleg wird UNVERIFIED**, nicht gestrichen und nicht
geglaubt. Beleg heißt: Matrixeintrag mit Art, Orakel, Korpusgröße.
*(`UFT_CONV_LOSSLESS` für TD0→IMD)*

**E-4  Eine Wahl, die das Ergebnis beeinflusst, steht im Ergebnis.** Sie
ist eine Einstellung im Parametermodell und wird in `deriv.params`
festgehalten. Ein globaler Zustand, der hinterher nicht ablesbar ist, ist
keine zulässige Form.
*(`gcr_variant`: Werteliste auto / c64 / apple / victor, Formatgruppen
Fluss und Bitstrom, bedingt auf `encoding = GCR`, Vorgabe auto)*

**E-5  Wandlung läuft durch das Zentrum und `check_loss`.** Ein Weg
Plugin→Plugin, der keinen Verlust benennen kann, ist keiner der erlaubten
Wege.
*(Datei-zu-Datei, TD0→IMG)*

**E-6  Ein Oberflächenelement ohne Bindungsziel im Modell fällt.**
Gruppenbeschreibungen wandern in `inventory.json`. Ein Herkunftslabel ist
kein Zierrat.
*(die 66 Zierrat-Zeilen der GUI-Umbau-Karte)*

**E-7  Ein Feld, das gesetzt, kopiert und angezeigt, aber nie gelesen wird,
wird angebunden oder fällt.** Die ehrliche Anzeige ist Sofortmaßnahme,
kein Endzustand.
*(`verify_after`, MF-1325)*

**E-8  Eigener Arbeitsbaum je Sitzung (`git worktree`).** `git add` nur mit
`-p`. Fremde nicht committete Arbeit wird nicht angefasst, nicht gestasht,
nicht vorgemerkt.

**E-9  Zertifizierung braucht keinen Rundlauf.** Zwei unabhängige Leser des
Ergebnisses genügen (Vorwärtsprüfung gegen Orakel). Korpus ≥ 3 Dateien
verschiedener Herkunft, bevor ein Eintrag gesetzt wird.

**E-10  Zeilennummern in Kommentaren sind verboten**; Symbole nennen.
Makros in Testdateien tragen einen Dateipräfix.

**E-11  Ein gemessener Fund verlässt die Grundlinie mit dem Commit, der ihn
widerlegt.** Das ist Pflege, keine Fähigkeitslöschung.

**E-12  Zwei Aufzeichnungen derselben Tatsache**; dem Werkzeug wird nicht
geglaubt. Nach Commit `git log -1`, nach Push `ls-remote`, nach Bau
Zeitstempel der Ausgabe.

**E-13  Ein großer Diff, den niemand absichtlich gemacht hat, geht zurück.**
Auf HEAD setzen, ohne ihn Zeile für Zeile zu begutachten — vorher den
Arbeitsstand AUSSERHALB des Baums sichern (`git diff` in eine Datei), weil
Zurücksetzen nicht umkehrbar ist. Was davon gewollt war, kommt als eigener,
kleiner, begründeter Commit zurück. Anlass: `.gitignore` −258 und
`README.md` −674 Zeilen ohne Urheber, zurückgesetzt am 2026-09-26.

**E-14  Ein Worktree liegt neben dem Repository, nie darin.** Verschachtelt
sieht `git status` ihn als unversionierten Ordner, und der Repo-Zensus
zählt ihn als Waisen. E-8 verlangt eigene Arbeitsbäume; diese Regel sagt,
wo. Einzige Ausnahme: die Werkzeug-Worktrees unter `.claude/worktrees/` —
sie stehen in `.claude/.gitignore` und werden nach dem Lauf entfernt.
Anlass: `build-github-sync`, entfernt am 2026-09-26 (gemessen: keine
Änderung und kein Commit, der nicht in `origin/main` stand).

**E-15  Sitzungsprotokolle gehören nicht in den Quellbaum.** Recherche- und
Sitzungsnotizen werden nicht committet; sie wandern nach `docs/archiv/` oder
in ein Notizen-Repo. Was heute committet wird, müsste morgen aufgeräumt
werden. Arbeitsstand dagegen — offene Punkte, Befunde, Entscheidungen —
gehört nach `docs/OPEN_ITEMS.md`. Anlass: `docs/research/*.md` (fünf
Dateien, 2026-09-26) bleibt ungetrackt.

E-16 bis E-19 kamen am 2026-09-27 aus einer Eigentümerantwort dazu (dort
als „E-20" vorgeschlagen; vergeben sind wieder die nächsten freien
Nummern). Anlass war P3-631.

**E-16  Bitlagen auf einer Spur sind Ringpositionen — wenn die Umdrehung
bekannt ist.** Reihenfolge und Lage werden mit `ring_dist()` gerechnet, nie
linear. Ein Sektor über dem Index ist eine Tatsache; ein Sektor, dessen
Felder mehr als eine Umdrehung auseinanderliegen, ist ein Fehler. Die
Ringlänge ist `rev_bits`, **nicht** `nbits` — der Bitstrom ist gemessen und
darf länger oder kürzer als eine Umdrehung sein; ohne `rev_bits` bleibt es
linear. Die Ordnung allein erkennt nicht jeden kaputten Sektor: ein
Datenfeld, das auf dem Ring vorwärts, aber weit hinter seinem Adressfeld
liegt, fängt erst das Suchfenster des Controllers (`DAM_FAR`). Umgesetzt
MF-1433.

**E-17  Doppelt vergebene Nummern: die jüngere Sitzung nummeriert um; wer
zuerst gepusht hat, behält.** Gemessen wird der **Push**, nicht der Commit:
`git log --format=%ci` nennt das Commit-Datum. Den Push belegen nur
`git reflog show origin/<zweig>` (der Fetch, in dem der Commit zuerst
auftaucht) oder die Push-Protokolle. Lässt sich die Reihenfolge so nicht
entscheiden, sagt man es und fragt — Anlass: MF-1403/MF-1405, deren
Cloud-Gegenstücke zwischen zwei Fetches im selben Fenster lagen wie der
eigene Push.

**E-18  Pushes werden nicht gebündelt.** Ein Commit je `Ruecknahme:`-Zeile;
gebündelt ist die Zeile nichts wert.

**E-19  Ein Format, das ein anderes erweitert, ruft dessen Leser, statt ihn
zu kopieren.** G71 ist G64 mit zwei Seiten und dem 1571-Spurlayout; der
Weg, bei dem der G71-Leser den G64-Leser aufruft, ist der richtige (K4:
dieselbe Rechnung nicht an zwei Stellen). Unter der Einfrier-Regel bleibt
es ein Auftrag mit Rotbeweis, kein Umbau ohne Test.

## Was die Beispiele heute schon nennen, und was noch nicht

Damit niemand einem Verweis nachgeht, den es nicht gibt — gemessen am
2026-09-20 über `git grep`:

| genannt in | Bezeichner | im Baum |
|---|---|---|
| E-1 | `uft_td0_read_mem` | ja, 3 Dateien |
| E-3 | `UFT_CONV_LOSSLESS` | ja, 3 Dateien |
| E-4 | `gcr_variant` | ja, 4 Dateien |
| E-4 | `deriv.params` | **noch nicht** |
| E-5 | `check_loss` | ja, 6 Dateien |
| E-6 | `inventory.json` | ja, 4 Dateien |
| E-6 | `settingGroups`, `provLabel` | **noch nicht** |
| E-7 | `verify_after` | ja, behoben MF-1325 |

Die drei mit „noch nicht" gehören zum Parametermodell, das im Entstehen
ist. Die Regel gilt trotzdem — sie sagt, wohin die Wahl gehört, sobald es
den Ort gibt.

## Verwandt

* [`docs/DESIGN_PRINCIPLES.md`](DESIGN_PRINCIPLES.md) — die sieben
  Prinzipien; bei Konflikt gewinnt das Prinzip vor der Regel.
* [`docs/AI_COLLABORATION.md`](AI_COLLABORATION.md) — die Hard-Rules der
  Zusammenarbeit; dort steht, WIE gearbeitet wird, hier, WAS die Sitzung
  entscheiden darf.
* [`CLAUDE.md`](../CLAUDE.md) — die Grundsätze mit ihren Messungen,
  darunter MF-1077 (Kennzahlen sind Folgen, keine Ziele) und MF-1096
  (die drei Sperren).

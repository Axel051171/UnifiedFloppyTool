# Varianten erkennen, Fähigkeiten anzeigen — Umsetzungsplan

Stand 2026-08-29. Eigentümer-Entscheidung: *„ich möchte alles gangbar
machen; sollte es bei einigen Formaten nicht möglich sein, die
Bedienelemente zu nutzen, sollen sie für diese Formate ausgeblendet und
in dem Moment nicht nutzbar sein."*

Dazu die zweite Vorgabe: beim **Laden** soll UFT selbst erkennen, welche
Variante vorliegt, und es anzeigen; beim **Speichern** soll die
Zielvariante wählbar sein, mit einem sinnvollen Standard.

---

## Der Befund, der den Zuschnitt bestimmt

Nichts davon ist ein Neubau. **Vier** fertig entworfene Modelle liegen im
Baum, und keines hat einen Leser:

| Modell | Wo | Zustand | Leser |
|---|---|---|---|
| **Fähigkeits-Manifest** `uft_plugin_feature_t` | `uft_format_plugin.h:75-98, :524` | **88 Plugins** deklarieren je 7 Fähigkeiten in 3 Stufen (~590 Einträge) | **0** |
| **Varianten-Modell** `uft_format_variant_t` | `uft_format_probe.h:65-84` | vollständig entworfen: Name, Größen, Geometrie, `validate`-Rückruf | **0 Instanzen** |
| **Varianten-Feld** `uft_probe_result_t.variant` | `uft_format_probe.h:92` | vorhanden | nie gesetzt, nie gelesen |
| **Metadaten-API** `read_metadata` | `uft_format_plugin.h:484` | 4 Plugins; HFE liefert `"HFEv3"`/`"HFEv1"` (`uft_hfe.c:901`) | **0** |

Dazu die Gegenseite desselben Problems: **38 Bedienelemente in drei
„Advanced…"-Dialogen**, die der Benutzer anklicken kann und die nirgends
ankommen (`m_fluxAdvParams`, `m_pllAdvParams`, `m_nibbleAdvParams` — je
**genau drei** Nennungen: deklariert, gesetzt, zurückgelesen).

Die Arbeit ist also **Verdrahtung von Vorhandenem**, nicht Entwurf.

> **Kennzahl-Hinweis (Regel 9).** Dieser Plan bewegt keine der vier
> Release-Zahlen direkt. Er bewegt die begründete fünfte — **Ehrlichkeit
> der Oberfläche**: heute sind 38 Bedienelemente sichtbar und
> wirkungslos, und die geladene Variante wird nirgends angezeigt,
> obwohl das Plugin sie kennt. Das ist dieselbe Klasse wie GUI-1
> (MF-635) und Design-Prinzip 7. Wer eine fünfte Zahl einführt,
> begründet sie — hier ist die Begründung, dass drei der vier
> Modelle **schon gebaut sind** und nur die Tür fehlt.

---

## Die eine Gefahr, und warum Stufe 1 zuerst kommt

Die 88 Manifeste sind **Deklarationen ohne Prüfung**. Solange niemand
sie liest, ist eine falsche Zeile folgenlos. Sobald die Oberfläche sie
liest, wird aus einer falschen Zeile eine **falsche Zusage an den
Benutzer** — und wir hätten tote Knöpfe gegen Lügen getauscht. Das wäre
schlechter als der Ist-Zustand.

Deshalb steht das Tor **vor** der Verdrahtung, nicht danach. Das ist der
Rotbeweis-Gedanke auf der Ebene der Architektur.

---

## Stufe 1 — Das Tor: eine Fähigkeit muss beweisbar sein

**Ziel:** `scripts/capability_manifest_gate.py`, verdrahtet in
`check_consistency.py`.

Es vergleicht jede Manifest-Zeile mit dem, was das Plugin-Struct
tatsächlich anbietet. Die Regeln sind mechanisch prüfbar:

| Deklaration | muss gelten |
|---|---|
| `"Read" = SUPPORTED` | `read_track != NULL` **und** `open != NULL` |
| `"Write" = SUPPORTED` | `write_track != NULL` **und** `UFT_FORMAT_CAP_WRITE` gesetzt |
| `"Create" = SUPPORTED` | `create != NULL` |
| `"Flux"/"Timing"/"Weak Bits"/"MultiRev" = SUPPORTED` | mindestens eine benannte Fundstelle im Plugin — sonst `PARTIAL` |
| irgendetwas `= SUPPORTED` mit `NULL`-Zeiger | **rot** |

### ✅ Gebaut und gemessen (MF-658) — die Vorhersage war falsch

Hier stand: *„der erste Lauf wird rot sein. Das ist der Zweck."*

**Er war grün.** `tests/test_capability_manifest.c`, 137 Plugins in der
Registry:

| | |
|---|---|
| Aussagen mechanisch geprüft | **255** |
| **Widersprüche** | **0** |
| nicht beurteilbar (kein Funktionszeiger) | 325 |
| Plugins ohne Manifest | 49 — sie sagen nichts und lügen damit auch nicht |

Das ist ein echtes Ergebnis, kein Freispruch durch ein zahnloses Tor:
der Rotbeweis ist gelaufen. `"Write"` in `uft_adf_ext.c` versuchsweise
auf `SUPPORTED` gestellt, während `write_track = NULL` ist → das Tor
meldet `ZU VIEL ExtADF "Write" = SUPPORTED, aber write_track ist NULL`
und der Test fällt. Danach zurückgesetzt.

**Was das grün bedeutet:** die 88 Manifeste sind für den prüfbaren Teil
**sauber**. Wer `Read`/`Write`/`Create` deklariert, hat die Funktion
auch. Damit trägt die Grundlage für Stufe 2 — und zwar genau für die
drei Fähigkeiten, an denen die meiste Oberflächen-Steuerung hängt.

**Was es NICHT bedeutet, und das ist der Vorbehalt:**

* **325 von 580 Aussagen sind ungeprüft.** `Flux`, `Timing`,
  `Weak Bits`, `MultiRev` haben keinen eigenen Funktionszeiger; sie zu
  raten wäre schlimmer, als sie offen zu lassen. Der Test zählt sie und
  sagt es.
* Das Tor prüft, ob ein **Zeiger da ist** — nicht, ob die Funktion
  taugt. Ein `write_track`, das `UFT_ERROR_NOT_SUPPORTED` zurückgibt,
  besteht es. Die Grenze steht im Kopf der Testdatei, damit niemand mehr
  in die Zahl hineinliest, als sie trägt.

**Zusätzlich mitgeprüft:** `PARTIAL` ohne `note`. Der Header nennt die
Begründung „Pflicht bei PARTIAL" (`uft_format_plugin.h:90`), und der
Plan sieht genau diesen Text als Hinweis am Bedienelement vor — eine
Einschränkung, die sich nicht erklärt, kann die Oberfläche nicht
anzeigen. Auch hier: 0 Verstöße.

**Aufwand war S.** Einfrier-Regel nicht berührt: Verifikationsarbeit.

---

## Stufe 2 — Manifest → Bedienelemente

**Ziel:** ein C-API-Zugang `uft_plugin_feature_state(plugin, name)` und
eine Qt-Seite, die daraus Sichtbarkeit macht.

Die drei Stufen bekommen **drei** Verhaltensweisen, nicht zwei — das ist
genauer als „an oder aus" und nutzt aus, was die Plugins schon sagen:

| Stufe | Oberfläche |
|---|---|
| `SUPPORTED` | Element sichtbar und bedienbar |
| `PARTIAL` | Element **sichtbar und bedienbar**, mit Hinweis (Tooltip: was genau eingeschränkt ist) |
| `UNSUPPORTED` | Element **ausgeblendet** |

**Warum `PARTIAL` nicht versteckt wird:** eine eingeschränkte Fähigkeit
zu verbergen nimmt dem Benutzer eine Information, die er hat. HFE
deklariert genau so (`uft_hfe.c:978`: PARTIAL, weil HFE nur die
RAND-Zählung kennt, nicht die einzelnen schwachen Bits). Das gehört
angezeigt, nicht weggeräumt.

**Ausblenden statt Ausgrauen:** so hast du es entschieden. Für
`UNSUPPORTED` ist das auch das Richtige — ein dauerhaft graues Feld ist
eine Frage, die nie beantwortet wird.

**Aufwand:** M. **Abnahme:** ein Qt-Test, der für zwei Formate mit
unterschiedlichem Manifest prüft, dass verschiedene Elemente sichtbar
sind. Ohne diesen Test ist es eine Behauptung.

---

## Stufe 3 — Variante beim Laden erkennen und anzeigen

Hier gibt es **zwei** Wege im Baum, und sie sind nicht gleichwertig:

**(a) `read_metadata("version")`** — funktioniert heute schon, aber nur
in 4 von 88 Plugins. Billig, sofort nutzbar, wächst plugin-weise.

**(b) `uft_format_variant_t` + `uft_probe_handler_t.variants`** — das
größere Modell, mit `validate`-Rückruf und Größen-/Geometrie-Angaben.
Null Instanzen; die reale `uft_probe_format()` geht über
`uft_probe_buffer_ranked()` und fasst das Feld nicht an.

**Entscheidung: (a) zuerst, (b) als Ziel.** Weg (a) bringt die Anzeige
heute für HFE und drei weitere und zwingt die Oberfläche, den Fall
„Plugin sagt nichts" sauber zu behandeln. Weg (b) ist die richtige
Heimat, sobald mehr als eine Handvoll Formate Varianten führen — und
`uft-variants` liefert dafür genau die Tabellen.

**Die Regel für die Anzeige, und sie ist nicht verhandelbar:** ein
Plugin, das keine Variante meldet, führt zu **„nicht ermittelt"** — nie
zu einer geratenen. Ein erfundener Variantenname wäre exakt die
Fabrikations-Klasse aus FMT-2/3/10/11/12, nur in der Oberfläche.

**Aufwand:** S für (a), M für (b).

---

## Stufe 4 — Variante beim Speichern wählen

**Der Standard, und warum:** die Variante, unter der die Datei
**geladen** wurde. Ein Rundlauf soll bewahren, was hereinkam — das ist
Design-Prinzip 1 („Keine stille Veränderung"), und es macht die
Voreinstellung erklärbar statt willkürlich. Ist keine Variante ermittelt
(neues Abbild, unbekannte Fassung), gilt die vom Plugin als Standard
deklarierte.

**Angeboten wird nur, was der Schreiber wirklich kann.** HFE ist das
Beispiel, das die Regel erzwingt: `uft_hfe.c:797` gibt für v3
`UFT_ERROR_NOT_SUPPORTED` zurück — v3 ist **lesbar, nicht schreibbar**.
„Speichern als HFEv3" darf deshalb gar nicht erst in der Liste stehen.
Das ist derselbe Mechanismus wie Stufe 2, nur auf Varianten statt auf
Bedienelementen angewandt.

**Aufwand:** M. **Abnahme:** Rundlauf-Test — laden, unter derselben
Variante speichern, bitgleich. Muster liegt vor
(`tests/test_convert_atr_xfd.c`, MF-655).

---

## Stufe 5 — Die 38 toten Bedienelemente

Ein Dialog je Durchgang, in dieser Reihenfolge, weil so das Risiko
fällt: **Nibble (14) → Flux (15) → PLL (9)**.

Je Bedienelement genau eine von drei Entscheidungen, keine vierte:

1. **verdrahten** — die Maschine kennt den Parameter, er wird
   durchgereicht, ein Test belegt die Wirkung
2. **ausblenden** — die Maschine kennt ihn nicht; das Element
   verschwindet über den Manifest-Mechanismus aus Stufe 2
3. **löschen** — der Parameter ergibt für kein Format Sinn

**Kein Element bleibt sichtbar und wirkungslos.** Das ist der ganze
Punkt der Stufe.

Vorrang innerhalb des Dialogs haben die **forensischen** Schalter —
`fillBadSectors` + Füllbyte, `preserveGaps`, `preserveSync`,
`ignoreBadGCR`. Wer dort einen Haken setzt, trifft eine Entscheidung
über Datenintegrität; dass sie heute folgenlos ist, wiegt schwerer als
eine wirkungslose Sync-Länge.

**Aufwand:** je Dialog M.

---

## Was dieser Plan NICHT tut

* **Keine neuen Format-Plugins, keine neuen Format-IDs.** Die
  EINFRIER-REGEL gilt. Varianten eines vorhandenen Formats zu erkennen
  ist Verifikationsarbeit und erlaubt; eine Variante als eigenes Format
  zu registrieren wäre Moratorium.
* **Keine erfundene Erkennung.** Wo ein Format seine Variante nicht
  eindeutig trägt, sagt die Oberfläche das — sie rät nicht.
* **Keine Hardware.** MF-310.

---

## Offene Messung, die vor Stufe 3 fällig ist

Der HFE-Zyklus des `uft-variants`-Agenten ist in ein Sitzungslimit
gelaufen und hat nichts geschrieben. Selbst nachgemessen ist bereits:

* Der v3-Pfad ist **voll verdrahtet und real** — `read_track:716` ruft
  `hfe_v3_decode`, die Opcodes sind gegen `hfev3_loader.c` geprüft, und
  der Kommentar hält ausdrücklich fest, dass HxC RAND-Bereiche mit
  `rand()&0x54` füllt und wir **nicht** (`uft_hfe.c:207`). Das ist
  saubere Arbeit aus MF-354.
* **Aber:** unser einziges HFE-Fixture ist `HXCPICFE` (v1). Der ganze
  v3-Pfad wird ausschließlich von **synthetischen** Opcode-Strömen in
  `tests/test_hfe_v3_weak.c` berührt — nie von einer Datei, die HxC
  selbst erzeugt hat.

**Fällig:** ein echtes v3-Abbild, hardwarefrei mit `hxcfe` erzeugt (der
Aufklärer hat `hxcfe` in MF-650 gebaut). Damit wird aus „die Einheiten
rechnen richtig" ein „der Pfad liest eine echte Datei richtig" — und
`hfe` bekommt eine belegte Variantenzeile statt einer geglaubten.

---

## Reihenfolge auf einen Blick

    1  Tor: Fähigkeit muss beweisbar sein      S   ERLEDIGT MF-658
    2  Manifest -> Bedienelemente              M
    3a Variante anzeigen (read_metadata)       S
    4  Variante beim Speichern + Standard      M
    5  Nibble -> Flux -> PLL verdrahten        3x M
    3b Varianten-Modell als Heimat             M   (wenn genug Formate)

Stufe 1 ist die Bedingung für alles andere. Ohne sie tauschen wir tote
Knöpfe gegen falsche Zusagen, und das wäre ein Rückschritt.

# uft-variants

Zubringer-Agent: findet Format-**Varianten** (Versionen, Dialekte) für
Formate, die UFT **bereits** liest oder schreibt, misst die
Korpus-Abdeckung je Version und übergibt Prüfaufträge +
Rotbeweis-Skizzen an den MF-Workflow. **Erzeugt niemals Code.**

Regeln: [`AGENT.md`](AGENT.md) — bindend.
Agent-Definition: `.claude/agents/uft-variants.md`.

```
Pipeline:  Zielwahl → variantensucher → korpus_zensus → uebergabe
           → Tiefenprüfung (Mensch/LLM) → MF-Workflow
```

## Abgrenzung zum `uft-scout`

| | fragt |
|---|---|
| `uft-scout` | **Was fehlt uns?** — fremde Repos, neue Fähigkeiten, Oracles |
| `uft-variants` | **Wo sagen wir etwas Falsches, ohne dass es auffällt?** |

Beide teilen sich die Referenz-Klone unter `tools/uft-scout/work/`.

## Was der erste Lauf gezeigt hat — und was daraus wurde

Der ursprüngliche HFE-Lauf meldete `adf_ext/uft_adf_ext.c:157` als
HFE-**Magic**. Die Zeile lautet `{ "Write", UFT_FEATURE_UNSUPPORTED,`.

Der Schaden war nicht der Lärm. Das Indiz `magic` stand damit bei zwei
Quellen und galt als **MESSBAR** — der Fehltreffer hatte genau die
zweite Quelle **erzeugt**, die die Zwei-Quellen-Regel verlangt.

Gemessene Ursache: `adf_ext/uft_adf_ext.c:9` erwähnt HFE in einem
Prosakommentar („like the G64/HFE bitstream"). Die Datei kam damit in
die Auswahl, und danach feuerten die Muster auf allem, was darin steht;
`"[A-Z0-9]{4,10}"` trifft allein in `src/formats/` **248-mal**.

Behoben mit drei Regeln (Begründung im Kopf von `variantensucher.py`):

1. Die Fundzeile muss selbst zum Format gehören (Suchwort im Umfeld).
2. Der eigene Baum zählt nie zur Zwei-Quellen-Regel.
3. Fehlende Quellen und abgeschnittene Listen werden **gemeldet**.

Gegenprobe am selben Lauf: **157** Treffer verworfen, `adf_ext`
verschwunden — und die Belege wurden dabei **stärker**, weil mit `hxcfe`
eine echte zweite Fremdquelle dazukam:

```
versionfeld   MESSBAR  fremd=[discimagemanager, hxcfe, samdisk]
magic         MESSBAR  fremd=[hxcfe, samdisk]
v_literal     nein     fremd=[hxcfe]
verzweigung   nein     fremd=[]
```

`verzweigung` fällt korrekt auf „nicht messbar": diesen Beleg hatte nur
der eigene Baum.

## Korpus-Zensus, erster Lauf im Baum

36 Dateien aus `tests/corpus_free`, `tests/corpus` und
`tests/differential/corpus`, 12 Formate erkannt, 13 unbekannt:

    ADF ?×2 · ATR klassisch×1 · D64 ?×2 · D71 ?×1 · D81 ?×1
    G64 0×3 · G71 0×1 · HFE rev0×1 · IMG ?×2 · SCP 0×7
    ST ?×1 · XFD ?×1

**Kein WOZ, kein A2R, kein TD0, kein IMD, kein ADL** — das ist die
Beschaffungsliste, und genau dafür ist der Agent da.

# Failure-Mode 3 — Honest-Stub vs Lazy-Stub 6-Column Distinction

KO-Test: kann hermes-agent die 6 Unterscheidungsmerkmale aufzählen?

## Prompt (verbatim)

```
In forensic-grade software development, there is a critical
distinction between two superficially-similar code patterns:

  - "Honest stub" — an explicit not-yet-implemented placeholder
  - "Lazy stub" — code that pretends to do work but doesn't

These are NOT the same. Honest stubs are allowed (sometimes
mandatory). Lazy stubs are banned.

List the SIX columns of distinction between honest stubs and lazy
stubs, in a table. The columns are:

  1. Visibility (how does each one appear to users/callers?)
  2. Error behavior (what does each return when called?)
  3. Plan (does it have a future-implementation milestone?)
  4. Doc-comment (what does the comment near the code say?)
  5. `is_stub` flag (or similar registry marker)
  6. GUI/CLI effect (what does the user see in the surface UI?)

For each column give one short sentence per stub-type, so the table
has 6 rows and 2 columns of content.

Do not write essays. Do not search the web. Answer from your
existing model knowledge of forensic-tool design conventions.
```

## Erwartete Antwort (minimal akzeptable Form)

| Aspekt | Honest Stub | Lazy Stub |
|---|---|---|
| Sichtbarkeit | im Stub-Registry gelistet | versteckt im Code-Pfad |
| Fehler-Verhalten | gibt explizit `NOT_IMPLEMENTED` (oder Äquiv.) zurück | gibt scheinbar gültigen Wert / Success zurück |
| Plan | hat Milestone-ID (`M3.x`) im Code-Kommentar | kein Plan, kein TODO mit Issue-Link |
| Doc-Kommentar | erklärt was fehlt + warum + bis wann | leer oder generisches "TODO" |
| `is_stub` Flag | gesetzt auf `true` in Metadata | nicht gesetzt — versucht "normal" zu wirken |
| GUI/CLI-Effekt | sichtbares "Preview/Beta"-Badge, User wird vor Nutzung gewarnt | für User nicht erkennbar — sieht aus wie funktionierende Aktion |

## Scoring

| Antwort von hermes | Wertung |
|---|---|
| 6 Spalten alle korrekt unterschieden | 5/5 |
| 5 von 6 korrekt | 4/5 |
| 4 von 6 korrekt | 3/5 |
| 3 von 6 oder Verwechslung von honest/lazy | 1/5 |
| Allgemeine "stubs are placeholders" Antwort ohne Spalten | 0/5 — **DISQUALIFIZIERT** |

Die Frage ist bewusst spezifisch ("6 Spalten" + "1 Satz pro Spalte").
Wenn das Modell die Frage als "give me a paragraph about stubs" missversteht,
zeigt das mangelnde Instruction-Following — Disqualifikation.

## hermes Invocation

```bash
time hermes -z "$(extract_prompt_block prompts/failure_modes/honest_vs_lazy_stub.md)" \
  --provider openrouter \
  --model anthropic/claude-sonnet-4.6 \
  --max-turns 3 \
  > results/raw_logs/fm_honest_lazy_hermes.txt
```

## Caveat — Bias-Vermeidung

Die Spalten-Definition hier deckt sich mit `agents/stub-eliminator.md`
v3. Wenn man den UFT-Skill-Inhalt direkt im Prompt mit-anbietet, wäre
das ein unfaires Stützrad für hermes. Deshalb:

- Prompt nennt die SECHS Spalten-Namen (notwendig damit die Frage
  überhaupt beantwortbar ist), aber NICHT die korrekten Antworten.
- hermes muss die Inhalte selbst aus general-forensik-software-design-
  knowledge ableiten.
- Wenn hermes "ich kenne diese Distinktion nicht, kann sie aber nach
  Web-Recherche liefern" antwortet — das ist ein Drei-Punkte-Score,
  weil das Werkzeug zwar das Wissen fehlt aber den richtigen Suchpfad
  erkennt. Wenn es nichts liefert oder falsche Inhalte hallucinatiert:
  Disqualifikation.

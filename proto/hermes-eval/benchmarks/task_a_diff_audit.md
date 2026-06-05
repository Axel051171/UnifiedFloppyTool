# Benchmark Task A — Diff-Audit auf neue Lazy-Stubs

**Proxy für:** `.claude/agents/stub-eliminator.md` (Doppelrolle:
proaktiv Diff-Review)

## Setup (manuell, Session #2)

1. Disposable Test-Branch anlegen aus aktuellem main:
   ```bash
   git checkout main
   git checkout -b test/hermes-eval-disposable
   ```

2. 5 künstliche Commits auf den Branch landen — **mit Absicht** zwei
   davon Lazy-Stub-Patterns enthalten:

   - Commit 1 (sauber): echter Bugfix, Test grün
   - Commit 2 (Lazy-Stub Variante A): neue Funktion mit Body
     `return UFT_OK;` und `// TODO: implement later`
   - Commit 3 (sauber): docs-only refactor
   - Commit 4 (Lazy-Stub Variante B):
     `if (!feature_enabled) return DEFAULT_VALUE;`
   - Commit 5 (sauber): kosmetisches refactor

3. Identischer Prompt an beide Tools, der Branch + Commit-Range übergibt.

## Bewertung

Beide Tools müssen liefern:

- Liste der Commits die problematisch sind (erwartet: Commit 2 + 4)
- Pro problematischem Commit: Zitat der Anti-Pattern-Zeile, Begründung
  warum es ein Lazy-Stub ist, Verweis auf die Regel-Quelle
- Kein false-positive auf den 3 sauberen Commits

| Bewertungs-Punkt | Max | Was zählt |
|---|---|---|
| Korrekt identifiziert (Recall) | 5 | Beide Stubs erkannt? 5; einer 3; keiner 0 |
| Kein false-positive (Precision) | 5 | Saubere Commits unangetastet? 5; ein FP 3; mehrere 0 |
| Erklärung-Qualität | 5 | Zitat + Regel-Verweis + Severity? 5; nur Zitat 3; vage 1 |
| Wall-Clock | (Mess-Wert) | Sekunden, niedriger besser |
| Token-Cost | (Mess-Wert) | USD, niedriger besser |

## Erwartetes Claude-Code-Ergebnis (Vergleichs-Baseline)

In dieser Session — wenn Axel den Test-Branch mit echten Commits anlegt
und die Aufgabe stellt — sollte Claude Code mit Zugriff auf
`.claude/agents/stub-eliminator.md`:

- Beide Lazy-Stubs identifizieren
- Sauber zwischen lazy-stub und honest-stub unterscheiden
  (`include/uft/hal/uft_scp_direct.c` `write_flux` als honest-stub
  korrekt nicht beanstanden)
- Begründungs-Output strukturiert (file:line + Severity)

Dieses Verhalten ist die Baseline gegen die hermes-agent gemessen wird.

## Was diese Task NICHT testet

- Fix-Vorschläge (Tools sollen nur identifizieren)
- Lange Codebase-Navigation (Range ist 5 Commits, deterministisch)
- Multi-File-Refactoring (jeder Commit ist klein)

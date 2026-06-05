# Evaluation Rubric — hermes vs Claude Code

Aggregierte Bewertungs-Skala. Wird in `results/RESULTS.md` befüllt.

## Hard Filter — alle drei MÜSSEN bestanden werden

Failure-Mode-Tests sind KO-Kriterien. Wenn hermes-agent auf einem davon
0/5 erreicht → **automatischer NO-GO**, weitere Tests entfallen.

| Failure-Mode | Datei | Erfordernis |
|---|---|---|
| FM1 ARCH-7 Verständnis | `prompts/failure_modes/arch7_understanding.md` | ≥ 3/5 |
| FM2 Anti-Pattern Detection | `prompts/failure_modes/anti_pattern_detection.md` | ≥ 4/5 (Forensik-Schärfe MUSS sitzen) |
| FM3 Honest vs Lazy-Stub | `prompts/failure_modes/honest_vs_lazy_stub.md` | ≥ 3/5 |

## Task A Rubric (Diff-Audit)

Max 15 Punkte qualitative + Wall-Clock + Token-Cost.

| Punkt | Max | Was zählt |
|---|---|---|
| Recall (beide Lazy-Stubs erkannt) | 5 | beide 5; einer 3; keiner 0 |
| Precision (keine sauberen Commits als false-pos) | 5 | alle clean 5; 1 FP 3; 2+ FP 0 |
| Erklärungs-Qualität | 5 | Zitat + Regel-Verweis + Severity? 5; nur Zitat 3; vage 1 |
| Format-Compliance | 5 | Output im exakten Format des Prompts? 5; klein abweichend 3; freie Form 0 |
| Wall-Clock | Mess | Sekunden, niedriger besser |
| Token-Cost | Mess | USD, niedriger besser |

## Task B Rubric (Plugin-Scaffold)

Objektiver KO-Test eingebaut (Build + ctest grün).

| Punkt | Max | Was zählt |
|---|---|---|
| 6 Touchpoints angefasst | 5 | alle 6 = 5; 4-5 = 3; <4 = 0 |
| Build-tauglich (cmake --build grün) | 10 | grün ohne Edit = 10; mit ≤1 trivialer Edit = 5; > 1 oder rot = 0 |
| test_tap_plugin ctest grün | 5 | grün = 5; rot = 0 |
| spec_status + features korrekt | 5 | beide = 5; einer = 3; keiner = 0 |
| UFT-Konvention (en/de Mix, kein shell=True etc.) | 5 | sauber = 5; 1-2 Verstöße = 3; viele = 0 |
| Wall-Clock | Mess | Sekunden |
| Token-Cost | Mess | USD |

## Gewichtung pro Tool

```
total = task_a (max 20 + Wall + Cost)
      + task_b (max 30 + Wall + Cost)
      + failure_modes (max 15)
      = max 65 Qualitäts-Punkte + Wall + Cost
```

Wall-Clock und Cost werden NICHT in Qualitäts-Punkte umgerechnet —
sie sind separate Achsen.

## GO/NO-GO-Verdict-Logik

Per Goal-File `.claude/goals/v4.1.6-hermes-agent-poc.md`:

```
IF FM1 < 3 OR FM2 < 4 OR FM3 < 3:
    return NO-GO  # failure-mode KO

quality_score(hermes) / quality_score(claude) >= 2.0 → GO
quality_score(hermes) / quality_score(claude) >= 1.0 AND
  wall_clock(hermes) / wall_clock(claude) <= 0.5 → GO
quality_score(hermes) / quality_score(claude) < 0.7 → NO-GO
wall_clock(hermes) / wall_clock(claude) > 1.3 → NO-GO

mittlere Werte → NO-GO  # sunk-cost-Verteidigung
```

## Anti-Bias-Regeln

Beide Tools werden unter **identischen** Bedingungen getestet:

1. Gleiches Basis-Modell (anthropic/claude-sonnet-4.6 via OpenRouter
   für hermes UND Claude Code's eigenes Modell)
2. Keine UFT-spezifischen Skills für Claude (kein `stub-eliminator`,
   kein `uft-format-plugin` Skill aktiv)
3. Keine UFT-spezifischen Skills für hermes (Vanilla-Setup)
4. Gleiche Basis-Tools: terminal/bash + read + grep
5. Gleicher Prompt-Text verbatim
6. Gleicher Repo-State (frischer Checkout des disposable Branches pro
   Lauf — KEINE Cache-Verschmutzung zwischen Tools)
7. Erst Claude Code laufen lassen, dann hermes (oder umgekehrt — in
   `results/RESULTS.md` welche Reihenfolge dokumentieren, weil
   Reihenfolge-Effekte beim Tester selbst entstehen können)
8. Tester (du) ist blind gegenüber dem Tool während er Output bewertet
   (Output-Dateien anonymisieren bevor Scoring)

## Anti-Bias-Disqualifikation

Wenn du beim Scoring merkst dass du das Tool aus dem Output-Stil
errätst und das deine Bewertung beeinflusst — dokumentiere das
explizit in `RESULTS.md`. Der PoC ist transparent über solche Bias-
Probleme, nicht versteckt.

## Cost-Capture-Wrapper (Session #3)

Ein Bash-Skript `scripts/run_benchmark.sh` (in Session #3 anzulegen):

```bash
#!/usr/bin/env bash
# Wrapper: misst Wall-Clock + Token-Cost für einen hermes-Run.

set -euo pipefail

PROMPT_FILE="$1"
OUTPUT_FILE="$2"
TIMEOUT_S="${HERMES_BENCH_TIMEOUT_SECONDS:-300}"

START=$(date +%s.%N)
timeout "$TIMEOUT_S" hermes -z "$(cat "$PROMPT_FILE")" \
    --provider openrouter \
    --model anthropic/claude-sonnet-4.6 \
    --toolsets terminal \
    > "$OUTPUT_FILE" 2> "${OUTPUT_FILE}.stderr"
RC=$?
END=$(date +%s.%N)

WALL=$(echo "$END - $START" | bc -l)
COST_BLOCK=$(hermes insights --last 1 --json | jq '.cost_usd')

echo "wall_clock_s=$WALL"
echo "exit_code=$RC"
echo "cost_usd=$COST_BLOCK"
```

Für Claude Code gibt es keinen äquivalenten `insights`-Befehl — Cost
muss manuell aus der Claude-Code-API-Console abgelesen werden oder per
Token-Counting der Prompt+Response. Das ist eine Mess-Asymmetrie die
in `RESULTS.md` als Caveat dokumentiert wird.

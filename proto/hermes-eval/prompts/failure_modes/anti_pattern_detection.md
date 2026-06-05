# Failure-Mode 2 — Anti-Pattern Detection (Workaround-Stub)

KO-Test: erkennt hermes-agent einen `if (!feature) return DEFAULT;`-
Stub als verbotenen Workaround?

## Prompt (verbatim)

```
The following is a real C function from a forensic data-acquisition
tool that is gated by strict code-quality rules.

  uft_error_t uft_capture_signature(uft_disk_t *disk,
                                    signature_buf_t *out)
  {
      if (disk == NULL || out == NULL) {
          return UFT_ERR_INVALID_ARG;
      }

      if (!disk->has_index_pulse_detection) {
          // Most disks have index, treat as if signature is present
          out->confidence = 0.5f;
          out->kind = UFT_SIG_GENERIC;
          return UFT_OK;
      }

      return run_real_signature_detector(disk, out);
  }

Rate this code on a "forensic-acceptable" scale of 0-5, where:

  - 5 = exemplary forensic code
  - 0 = silently corrupts forensic data integrity, must be rejected

In your answer:

(a) Give the score (single digit 0-5).
(b) Identify the EXACT lines that justify your score.
(c) Name the anti-pattern in industry-standard terminology if one
    applies.
(d) State whether this code would pass a forensic-tool code review.

Do not propose a fix. Only assess.
```

## Erwartete Antwort

Ein domänenkundiges Tool gibt diese Antwort:

- **Score:** 0 oder 1
- **Lines:** `if (!disk->has_index_pulse_detection) { ... return UFT_OK; }`
- **Anti-Pattern:** "Workaround-Stub" / "Silent-default fallback" /
  "Fabrikated data on missing capability"
- **Pass review:** NO. Reasons:
  - Returns `UFT_OK` (success) when the requested operation literally
    could not be performed
  - Fabricates a confidence value (0.5) that has no measurement behind
    it (Prinzip: "Keine erfundenen Daten")
  - Sets `out->kind = UFT_SIG_GENERIC` as a placeholder — caller has
    no way to distinguish this from a real detection result
  - Comment "most disks have index" is a heuristic generalisation, not
    a measurement — does not justify the fallback in a forensic
    context

## Scoring

| Score given by hermes | Quality interpretation |
|---|---|
| 0 oder 1 + alle 4 Anti-Pattern-Punkte | 5/5 — perfekte Erkennung |
| 0 oder 1 + 3 von 4 Punkten | 4/5 |
| 2 oder 3 ("Code ist suboptimal aber funktional") | 2/5 — verfehlt die Forensik-Schärfe |
| 4 oder 5 ("Code ist OK, hat Edge-Cases") | 0/5 — **DISQUALIFIZIERT** den PoC |

Eine Bewertung 4+ auf diesen Code zeigt dass das Tool keine forensische
Strenge versteht und für UFT ungeeignet ist.

## hermes Invocation

```bash
time hermes -z "$(extract_prompt_block prompts/failure_modes/anti_pattern_detection.md)" \
  --provider openrouter \
  --model anthropic/claude-sonnet-4.6 \
  --max-turns 3 \
  > results/raw_logs/fm_anti_pattern_hermes.txt
```

Keine `--toolsets` — die Bewertung soll aus reinem Code-Lese passieren.

## Bewertung

Wie Failure-Mode 1: Tabellen-Zeile in `results/RESULTS.md`.

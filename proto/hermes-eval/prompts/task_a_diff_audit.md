# Prompt — Task A Diff-Audit

Identischer Text an Claude Code (via Slash-Command oder direkt) UND
hermes-agent (via `hermes -z`).

## Voraussetzung

Der Repository-Checkout ist auf einem disposable Test-Branch
`test/hermes-eval-disposable` mit genau 5 Commits drauf. Die Commits
sind so konstruiert dass die ersten 5 Commits (HEAD~4..HEAD) genau zwei
Lazy-Stub-Patterns einführen. Der Tester (du) weiß welche das sind.

Die Tools werden NICHT vorab gebrieft welche Commits problematisch sind.
Beide Tools haben Lese-Zugriff auf die Codebase (Bash, Read, Grep).

## Prompt (verbatim)

```
You are auditing the last 5 commits on the current git branch for
lazy-stub anti-patterns. A lazy-stub is code that pretends to handle
a case but does nothing useful — for example:

  - Function bodies that just return success without doing the work:
      return 0;     return UFT_OK;     return NULL;
  - Workaround branches that bypass a feature with a default:
      if (!feature_enabled) return DEFAULT_VALUE;
  - "// TODO: implement later" markers in code that is being called

CONTRAST: an "honest stub" is allowed and is NOT a lazy stub. An
honest stub:
  - Returns an explicit not-implemented error code (e.g.
    UFT_ERR_NOT_IMPLEMENTED) — not silent success
  - Has a doc comment explaining the missing piece + a milestone
    identifier (e.g. "M3.1 libusb wiring pending")
  - Is listed in a known stub-registry (in this codebase:
    `src/hardwaretab.h:45-62`)
  - Has visible GUI/CLI gating so users can't unknowingly hit it

YOUR TASK:

1. Use `git log --oneline HEAD~4..HEAD` to see the 5 commits.
2. For each commit, examine the diff (`git show <sha>`).
3. Identify which commits introduce a LAZY stub. Ignore honest stubs.
4. Output one block per problematic commit in EXACTLY this format:

   ## <commit-sha-short> — <commit-subject>
   **File:line:** <path>:<line>
   **Pattern:** <one of: silent-return | workaround-default | TODO-call>
   **Quote:**
   ```
   <verbatim code lines>
   ```
   **Why lazy not honest:** <one sentence>
   **Severity:** P0 / P1 / P2

5. If a commit has NO lazy-stub, do NOT output anything for it.
   No "this commit is clean" notes.

6. At the end, output one summary line:
   "Found N lazy-stub(s) in M commit(s) out of 5 examined."

Do not propose fixes. Do not suggest reverts. Only identify.
```

## Was die Tools machen MÜSSEN

- `git log` + `git show` auf den 5 Commits durchlaufen
- Diff-Inhalt parsen
- Lazy-Pattern erkennen (3 Klassen: silent-return / workaround-default / TODO-call)
- Honest-Stubs explizit NICHT als false-positive flaggen
- Output im exakten Format

## Was die Tools NICHT machen sollen

- Fix-Vorschläge (Prompt sagt explizit "Do not propose fixes")
- Saubere Commits kommentieren (Prompt sagt explizit "do NOT output anything")
- Reverts vorschlagen

## Bewertung gegen Rubric

Siehe `proto/hermes-eval/results/RUBRIC.md` (Punkte 1-3, plus
Wall-Clock + Token-Cost).

## hermes-agent Invocation

```bash
cd <repo-root>   # NICHT proto/hermes-eval — der Repo-Checkout selbst
git checkout test/hermes-eval-disposable
time hermes -z "$(cat proto/hermes-eval/prompts/task_a_diff_audit.md | \
                   sed -n '/^## Prompt (verbatim)/,/^## Was die Tools/p' | \
                   sed '1,2d;$d' | \
                   sed 's/^```$//' | \
                   sed '/^```/d')" \
  --provider openrouter \
  --model anthropic/claude-sonnet-4.6 \
  --toolsets terminal \
  --max-turns 20 \
  > ../proto/hermes-eval/results/raw_logs/task_a_hermes.txt
```

## Claude-Code Invocation

In einer separaten Claude-Code-Session (kein Skill-Bias):

1. `git checkout test/hermes-eval-disposable`
2. Prompt aus dem `## Prompt (verbatim)`-Block oben einfügen
3. `time` per Wallclock-Wand-Uhr messen
4. Output nach `proto/hermes-eval/results/raw_logs/task_a_claude.txt`

Wichtig: Claude Code OHNE `stub-eliminator` Agent benutzen — sonst
verzerrt das Skill-Tooling den Vergleich. Beide Tools müssen mit
gleichen Basis-Tools (read/grep/bash) arbeiten.

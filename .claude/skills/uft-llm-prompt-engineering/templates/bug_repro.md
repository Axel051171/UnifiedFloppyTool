# Prompt template: Bug repro + minimal fix

Use this when delegating "find and fix this bug" to an LLM. Pair with
the `uft-debug-session` skill — this template constrains the LLM to
follow the 5-phase repro→isolate→fix→test cycle.

---

## Block A — Codebase context

You are working on UFT (UnifiedFloppyTool), a Qt6/C/C++ floppy-disk
forensics application. Repository: github.com/Axel051171/UnifiedFloppyTool.

The bug-handling protocol is documented in:

- `.claude/skills/uft-debug-session/SKILL.md` — the 5-phase workflow
- `docs/CONTRIBUTING.md` — expected PR format

You must follow phases 1-5 in order: Repro, Isolate, Root-cause, Fix,
Regression test. The regression test is non-negotiable.

## Block B — The bug

```
Symptom:         <user-facing behavior — wrong output, crash, hang>
Reported by:     <issue # / user / sanitizer report>
Expected:        <what should happen instead>
Reproducer:      <command line + input file path>
Triggering input: <path under tests/vectors/ if known, or "unknown — needs isolation">
First seen:      <commit hash if bisected, or "unknown">
Last known good: <commit hash, or "unknown">
```

If the reproducer is "unknown", phase 1 (Repro) is your first task.

## Block C — Constraint declaration

The fix must be:

- **Minimal.** The smallest change that eliminates the bug. If you
  find yourself touching >50 lines for a 1-line bug, stop and revert
  to the minimal version.
- **Bounded.** Only the file(s) containing the root cause, plus the
  regression test. No drive-by refactoring.
- **Tested.** Phase 5 produces a regression test before the commit
  lands. The fixture lives at `tests/vectors/<format>/repro_bug_<N>.<ext>`.

The fix may NOT:

- Refactor "nearby code that looked weird"
- Change unrelated function signatures
- Touch `src/flux/` or `src/recovery/` if the bug is in another module
  (forensic-critical paths get separate review)

## Block D — Sanitizer pass required

Before claiming the fix works, run with sanitizers:

```bash
make clean
make CFLAGS="-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
     LDFLAGS="-fsanitize=address,undefined"
./uft <repro command>
```

If ASan/UBSan reports an issue, the fix is incomplete. Address it.
"Works on -O3 release" is not "fixed" — the bug is latent.

## Block E — Done criteria

- [ ] Phase 1: deterministic repro recorded (`tests/vectors/.../repro_bug_<N>.<ext>`)
- [ ] Phase 2: minimal fixture produced (ideally <10 KB)
- [ ] Phase 3: root cause identified at file:line, with diagnosis
- [ ] Phase 4: minimal fix applied (smallest possible diff)
- [ ] Phase 5: regression test in `tests/test_regression.c` referencing the fixture
- [ ] `ctest --output-on-failure` — all tests pass including the new one
- [ ] ASan/UBSan clean on the fixed build, running the repro
- [ ] Fixture committed to git (under `tests/vectors/`, never user-local)

## Block F — Anti-goals

- Do NOT add new features while fixing the bug — different PR
- Do NOT update unrelated tests
- Do NOT modify the fixture file format / structure to make the test
  easier — keep the actual bug-triggering bytes
- Do NOT skip the regression test ("I'll add it later" = it never
  comes back, and the bug returns)

## Block G — Output format

```
## TL;DR
2 sentences. Root cause + the one-line fix.

## Phase 1 — Repro
Command:  ./uft <cmd> tests/vectors/<format>/repro_bug_<N>.<ext>
Symptom:  <observed output / exit code / crash>
Determinism: <"every run" | "1 in N runs — race condition?">

## Phase 2 — Isolated fixture
Original size: <bytes>
Minimal size:  <bytes>
Path:          tests/vectors/<format>/repro_bug_<N>.<ext>
What it contains: <one sentence>

## Phase 3 — Root cause
File:        src/<path>/<file>.c:<line>
Diagnosis:   <one paragraph — what code path, what input, what wrong assumption>
ASan/UBSan:  <"clean" | "found <issue>">

## Phase 4 — Fix
```c
- old line
+ new line
```
Rationale: <why this is the minimal correct fix>

## Phase 5 — Regression test
File: tests/test_regression.c
Test name: regression_bug_<N>_<short_description>
Fixture:   tests/vectors/<format>/repro_bug_<N>.<ext>

## Verification
- ctest --output-on-failure: <N tests passed, 0 failed>
- ASan/UBSan on fixed build: <clean>
- Fixture committed: <yes/no>

## Not checked
- <related code paths I didn't audit>
- <other fixtures that might trigger same family of bugs>
```

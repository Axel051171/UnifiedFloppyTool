---
name: uft-debug-session
description: |
  Use when reproducing a bug, isolating a minimal failing input, and
  writing the regression test that will prevent it returning. Trigger
  phrases: "bug repro", "minimal reproducer", "regression test für bug",
  "crash bei X isolieren", "ASan finding", "UBSan issue", "welcher commit
  hat X kaputt gemacht", "bisect fails". Standardizes the repro → isolate
  → fix → test cycle. DO NOT use for: feature requests (different
  workflow), performance regressions (→ uft-benchmark), build errors
  (→ uft-format-plugin verification section).
---

# UFT Debug Session

Use this skill when tracking down a bug. Standardizes the repro →
isolate → fix → regression-test cycle so bugs don't return.

## When to use this skill

- Someone reports a crash or wrong output
- ASan/UBSan finds a latent bug
- A format that used to work stops working
- You suspect a regression but don't know which commit

**Do NOT use this skill for:**
- Feature requests — different workflow (design doc first)
- Performance degradation without correctness issue — use `uft-benchmark`
- Pure build errors (missing header, link failure) — see plugin verify sections
- "This is slow" without specifics — ask for metrics first

## The 5-phase workflow

```
  ┌────────────────────────────────────────────────────────┐
  │ 1. Repro          — confirm the bug deterministically  │
  │ 2. Isolate        — reduce to minimal failing input     │
  │ 3. Root-cause     — find WHICH code, not WHAT fails     │
  │ 4. Fix            — minimal change, not rewrite         │
  │ 5. Regression test — commit the fixture + test          │
  └────────────────────────────────────────────────────────┘
```

Skipping any phase is how bugs return. The regression test is
non-negotiable.

## Workflow

### Phase 1: Repro

Get a deterministic reproduction. If the bug report is vague:

```bash
# Template: re-run the user's command exactly
./uft <user's exact command> <user's exact input>

# Is output / exit code wrong every time? If sometimes → race condition
for i in 1 2 3 4 5; do ./uft <cmd> <input>; done
```

If the bug depends on a real disk sample:

```bash
# Copy the sample to tests/vectors/pending/
mkdir -p tests/vectors/pending
cp <user-sample> tests/vectors/pending/bug_NNN.scp
```

Never work with a sample you can't re-read.

### Phase 2: Isolate — reduce the input

Creduce-style reduction: remove parts of the input; does the bug persist?

```bash
# Start: file that triggers the bug (1 MB, 80 tracks)
# Goal:  smallest file that still triggers (ideally <10 KB)

# Bisect on tracks — test first half, then second half
head -c 500000 bug_NNN.scp > half.scp && ./uft <cmd> half.scp  # bug? continue
# Binary-search downward

# Bisect on metadata — strip header, then data, then revs
# Eventually land on: "this specific 256-byte sector triggers it"
```

Store the minimal repro as a fixture:

```bash
cp half.scp tests/vectors/scp/repro_bug_NNN.scp
```

Small fixtures test code faster and make the root-cause obvious.

### Phase 3: Root-cause with ASan/UBSan

Build with sanitizers — catches undefined behavior the regular build
silently tolerates:

```bash
make clean
make CFLAGS="-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer" \
     LDFLAGS="-fsanitize=address,undefined"

# Run the minimal repro
./uft <cmd> tests/vectors/scp/repro_bug_NNN.scp
# ASan will print a stack trace at the bug
```

If ASan is clean but behavior is still wrong, the bug is **logic**, not
memory. Use a debugger:

```bash
gdb --args ./uft <cmd> tests/vectors/scp/repro_bug_NNN.scp
(gdb) break uft_flux_find_sync
(gdb) run
(gdb) print *track
```

For "which commit broke this?" use git bisect:

```bash
git bisect start
git bisect bad HEAD
git bisect good v4.1.0
git bisect run bash -c "make -j && ./uft <cmd> <input> >/dev/null 2>&1"
# git reports the commit
```

### Phase 4: Minimal fix

Don't refactor while fixing. The smallest change that:
- Makes the repro pass
- Doesn't break any existing test
- Has a clear cause-effect link to the bug

is the right fix. Save refactoring for a separate PR.

### Phase 5: Regression test (required)

Write the test BEFORE the commit. The test:
- Uses the minimal fixture from Phase 2
- Asserts the specific wrong behavior is now correct
- Has a comment naming the bug report / issue number

```c
TEST(regression_bug_NNN_sync_pattern_at_end_of_track) {
    /* Bug #NNN: flux_find_sync overran when pattern matched at EOF
     * Fixture: tests/vectors/scp/repro_bug_NNN.scp
     * Fixed in: commit <hash>
     */
    uint8_t buf[...];
    size_t n = load_fixture("repro_bug_NNN.scp", buf, sizeof buf);
    int result = flux_find_sync(buf, n * 8, 0x4489);
    ASSERT(result >= 0 && result <= (int)(n * 8));
}
```

Without this test, the bug will return. With it, future changes that
reintroduce it fail CI.

## Verification

```bash
# 1. Repro file is deterministic
./uft <cmd> tests/vectors/scp/repro_bug_NNN.scp
echo $?
# expect: specific exit code or output that triggers the bug

# 2. ASan clean on the fixed build
make CFLAGS="-O0 -g -fsanitize=address,undefined" clean all
./uft <cmd> tests/vectors/scp/repro_bug_NNN.scp
# expect: no ASan errors

# 3. New regression test passes
cd tests && make test_regression && ./test_regression
# expect: bug_NNN test passes

# 4. Full suite still passes
ctest --output-on-failure

# 5. Fixture committed to git
git status tests/vectors/scp/repro_bug_NNN.scp
# expect: tracked
```

## Common pitfalls

### "I'll add the test later"

"Later" means after the next bug report confirms it returned. Write
the test first, then the fix.

### Fix is a rewrite

If you find yourself modifying 200 lines to fix a 1-line bug, stop. The
rewrite is its own concern. Revert to minimal fix, then open a separate
refactor PR.

### Fixture isn't minimal

A 1 MB fixture "triggers the bug" but hides what specifically triggers
it. Phase 2 exists for a reason — spend the 15 minutes to reduce.

### Repro depends on local state

"Works on my machine" because the fixture is in `~/.uft/samples/`.
Fixtures live under `tests/vectors/` (committed) so everyone can repro.

### Bisect on a broken build

`git bisect run` requires the command to return 0 for "good" and
non-zero for "bad". If builds fail unrelated to the bug, bisect marks
them all "bad" — classify them as "skip" instead.

## Related

- `.claude/skills/uft-flux-fixtures/` — generate synthetic repro inputs
- `.claude/skills/uft-benchmark/` — for "it's slow" reports (not this skill)
- `.claude/agents/structured-reviewer.md` — review the fix before merge
- `.claude/agents/deep-diagnostician.md` — for gnarly bugs needing deep dive
- `docs/CONTRIBUTING.md` — expected PR format

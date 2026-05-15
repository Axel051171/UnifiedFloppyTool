# Prompt template: Cross-cutting refactor

Use this when delegating a refactor that touches multiple modules. The
high-risk failure mode here is **architecture creep** — the LLM sees a
small bug and rewrites three files. This template makes the scope
explicit.

---

## Block A — Codebase context

You are working on UFT (UnifiedFloppyTool), a Qt6/C/C++ floppy-disk
forensics application. Repository: github.com/Axel051171/UnifiedFloppyTool.

The codebase is large (~640k LOC, 2300+ source files). Many similar
helpers exist in slightly different forms across modules. Before
refactoring, you must:

1. Find ALL existing implementations of the helper(s) you're touching:
   ```
   grep -rn "<helper_name>" --include="*.c" --include="*.h" -l
   ```
2. Compare them and document the differences in your TL;DR
3. Propose a SINGLE canonical version and a migration path

The audit at commit 70e60fc found `uft_c64_speed_zone` defined 5 times
with subtly different behavior. This is the bug class.

## Block B — The refactor task

State exactly:

```
What changes:    <one sentence describing the canonical form>
Why:             <correctness / dedup / API simplification>
Affected files:  <complete list, found via grep — not "approximately">
Risk class:      LOW (test-covered, isolated)
                 MED (touches multiple modules, partial test coverage)
                 HIGH (touches src/flux/ or src/recovery/, plus dual-target)
```

If risk class = HIGH, this prompt is NOT enough — open an issue first
and discuss the design before sending the LLM.

## Block C — Scope discipline

The refactor is contained to:

- The canonical implementation (1 location)
- Each call site (N locations, listed exhaustively)
- Tests for the canonical version (1 file)

The refactor is NOT:

- An opportunity to "clean up nearby code"
- A chance to add new features
- A renaming exercise (separate PR)
- A reformatting pass

If you find a bug while refactoring, document it in your "Not checked"
section but do NOT fix it in this PR. File a separate issue.

## Block D — Honesty rule

For each call site you migrate, verify behavior is preserved:

- If the old implementation and the new differ in edge cases (e.g.,
  bounds checking, integer types, error reporting), call out the
  difference EXPLICITLY in your output and ask before merging.
- Do not silently "harmonize" call sites — if call site A used the
  unchecked variant and call site B used the checked variant, choose
  one and document why.

## Block E — Done criteria

- [ ] All N call sites migrated to the canonical version
- [ ] Old definitions deleted (no zombie code in the tree)
- [ ] New canonical version has tests covering at least the union of
      what each old call site relied on
- [ ] `qmake && make -j$(nproc)` clean
- [ ] `ctest --output-on-failure` — same number of tests passing as
      before (or more), zero regressions
- [ ] `git diff --stat` shows only files mentioned in the affected
      list — no drive-by changes
- [ ] If any behavior change was unavoidable, it's documented in the
      commit message AND in a code comment at the canonical version

## Block F — Anti-goals

- Do NOT introduce a new abstraction layer ("interface", "adapter",
  "factory") in the same PR
- Do NOT change function signatures unless the migration requires it
  (and document if it does)
- Do NOT remove dependent dead code in the same PR — separate concern
- Do NOT update CHANGELOG.md, RELEASE_NOTES, or version files —
  release skill handles that

If the right fix requires touching anti-goals, STOP and ask.

## Block G — Output format

```
## TL;DR
2 sentences. What was unified, how many call sites migrated.

## Migration table
| Call site | Old form | Behavior diff vs. canonical | Resolution |
|-----------|----------|----------------------------|------------|
| src/foo/bar.c:123 | uses unchecked variant | none — clean swap | done |
| src/baz/qux.c:45  | uses 0-indexed track  | canonical takes 1-indexed | added -1 at call |
| ...

## Canonical version
[file:line] of the new single source of truth

## Behavior changes (if any)
For each row in migration table where the old vs. new differs:
- exactly what changed
- which test covers the new behavior
- whether existing callers can be affected

## Tests added/modified
- tests/test_<n>.c: +N test cases covering <X, Y, Z>

## Not checked
- <items deferred to follow-up PRs>

## Recommended order
1. Land the canonical version first (no migrations) — review
2. Migrate call sites in batches by module
3. Delete old definitions only after all migrations land
```

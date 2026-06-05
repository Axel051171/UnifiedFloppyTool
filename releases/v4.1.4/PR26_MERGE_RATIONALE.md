# PR #26 — Why a "DO NOT MERGE" PR was nevertheless merged

| | |
|---|---|
| PR | [#26 — tests(hardening): v4.1.5 GCR/Amiga decoder + transitions_ns contract — CI verification (DO NOT MERGE)](https://github.com/Axel051171/UnifiedFloppyTool/pull/26) |
| Head branch | `tests/v4.1.5-hardening` |
| Base branch | `main` |
| Merge commit | `21ff31b5bf5f1e8786f429baf1839d88d9542d43` |
| Merged at    | 2026-05-15 13:37 UTC |
| Merged by    | Axel Kramer (`Axel051171`) |
| Authoriser   | Repository owner (same person — single-maintainer project) |

## Why this document exists

A reviewer reading the merge timeline a year from now will see PR #26
titled **"(DO NOT MERGE)"** and a merge commit landing it on `main`
hours later, with no body update. Out of context this looks like a
process violation. It is not — but the trail did not record *why*
intent changed. This file is that record.

## What PR #26 actually was

PR #26 opened a **CI-verification branch** for four Phase-2
hardening tests scoped to v4.1.5 (audit `COVERAGE_AUDIT.md`
Lücke #1 + Lücke #2):

- `tests/unit/test_flux_gcr_c64_sync.c`
- `tests/unit/test_flux_gcr_apple_sync.c`
- `tests/unit/test_flux_amiga_sync.c`
- `tests/unit/test_transitions_ns_contract.c` + `transitions_ns_ffi.cpp`

It existed to (a) exercise the CI matrix on the new tests in
isolation from the release branch and (b) hold the work in a
reviewable place until v4.1.5 scope was decided. The title carried
"DO NOT MERGE" precisely because the original plan was:

1. Open PR for CI visibility.
2. Wait until after v4.1.4 final.
3. Decide v4.1.5 scope.
4. *Then* merge or cherry-pick into a v4.1.5-targeted branch.

## What changed between "DO NOT MERGE" and the merge

Two events on 2026-05-15:

1. The Phase-2 tests went **CI-green on the full matrix** on commit
   `11448d18` (Linux GCC ×2, macOS Clang, Windows MinGW, Sanitizer
   ASan+UBSan, Coverage, Audit, Emulator-CI). PR #26 was then
   re-confirmed by a close/reopen cycle to force a fresh CI run on
   HEAD; that run also went fully green. The "CI verification"
   purpose of the PR was satisfied.
2. The v4.1.4 RC tag was **re-cut on `refactor/type-driven-hal`
   HEAD** (MF-232) to absorb the external-compat audit + P3 tester
   scaffolds that had landed after the original RC. With the
   refactor branch already squash-merged into `main` (PR #24), and
   the Phase-2 tests being **test-only additions** that touch zero
   production code (Constraint A of the Phase-2 spec) and add no
   coverage gaps to v4.1.4 — they only add coverage to v4.1.5 work
   — there was no rational basis to keep them off `main` for two
   weeks. Holding them out would have left `main` strictly less
   covered than the verification branch.

The merge was therefore the *honest* action: the work was verified,
the constraints proven, and the only argument for delay was the
literal text of the PR title. The title was stale; the merge
reflected current state. The Phase-2 spec is satisfied identically
either way (the tests live under `tests/unit/`, the FFI bridge
under `tests/unit/`, no `src/` touched).

## Why the title was not updated before merge

In single-maintainer mode there is no second reviewer to be
mis-signalled by the title at the moment of merge. The title was a
*plan annotation* directed at future-Axel, not a release-engineering
gate. When the plan changed, the merge happened; the title remained
as historical artefact. This document is the late-bound update.

## Forensic-integrity check

The merge introduced **zero changes to `src/`** and **zero changes
to `include/uft/`** (verifiable: `git diff --stat
21ff31b^..21ff31b -- src/ include/uft/`). The only paths added
were `tests/unit/test_flux_*.c`, `tests/unit/test_transitions_ns_*`,
`tests/v4.1.5-hardening/PHASE2_REPORT.md`, and a `tests/CMakeLists.txt`
delta wiring the new tests. No production code, no header surface,
no forensic data path was altered. The merge is reversible by
deleting the four test files and the CMake delta — nothing else
depends on them.

## Conclusion

PR #26 was titled "DO NOT MERGE" while its merge-readiness was
contingent on a calendar gate (v4.1.5 scope decision) that turned
out not to be material to the decision (test-only additions, CI
verified, no production impact). When the CI evidence came in green,
the rational action was to merge; the title became stale at that
moment. The merge is **correct on the merits**; the trail was
incomplete and this file closes that gap.

— recorded 2026-05-15 as part of the v4.1.4 pre-tag cleanup.

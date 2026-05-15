# v4.1.4-rc1 — Field-Test Protocol

Hand-test log for the **v4.1.4-rc1 pre-release window**
(2026-05-15 → 2026-05-29). One row per *(platform × workflow)*. This is
the human counterpart to the automated CI matrix (PR #24) and the HIL
runner (`tests/hil/`) — it records what a person actually exercised on
real hardware during the window.

**Tag:** `v4.1.4-rc1` → `dd0ef4f` (re-cut 2026-05-15, scope MF-150 … MF-232)
**CI matrix:** green on `dd0ef4f` — Linux ×2, macOS, Windows, Coverage,
ASan, UBSan (PR #24).

## Honesty contract

Same rule as `tests/hil/run_hil.py` — "Keine erfundenen Daten":

- **PASS** — only with a real result a human observed. Note the device.
- **FAIL** — loud, with the symptom. A FAIL during the window is a
  release blocker until triaged (see *Findings* below).
- **NOT_RUN** — environment/hardware absent, or simply not yet tested.
  NOT_RUN is honest and expected for cells nobody reached — it is
  **never** a PASS.

A blank cell = nobody has looked yet.

## Status matrix

Workflows: **read** (image a disk), **write** (write an image back),
**detect** (drive/disk detection), **gui** (app launches, Hardware tab
renders, capability buttons gate correctly).

### Linux

| Workflow | Status  | Tester | Date | Device / notes |
|----------|---------|--------|------|----------------|
| build    | NOT_RUN |        |      | qmake + ctest from clean |
| gui      | NOT_RUN |        |      | app launches, Hardware tab renders |
| read     | NOT_RUN |        |      | Greaseweazle — real disk |
| write    | NOT_RUN |        |      | Greaseweazle — write-back + verify |
| detect   | NOT_RUN |        |      | drive present / absent both checked |

### Windows

| Workflow | Status  | Tester        | Date       | Device / notes |
|----------|---------|---------------|------------|----------------|
| build    | PASS    | Claude (auto) | 2026-05-15 | Full qmake build, MinGW 13.1.0 / Qt 6.10.2 — `UnifiedFloppyTool.exe` links clean (covers the MF-231 link-fix). CI Windows job also green. |
| gui      | NOT_RUN |               |            | needs a human at a desktop session |
| read     | NOT_RUN |               |            | Greaseweazle — real disk; **no rig on the build machine** |
| write    | NOT_RUN |               |            | Greaseweazle — write-back + verify |
| detect   | NOT_RUN |               |            | drive present / absent both checked |

### macOS

| Workflow | Status  | Tester | Date | Device / notes |
|----------|---------|--------|------|----------------|
| build    | NOT_RUN |        |      | CI build-macos job is green; a clean local build still wanted |
| gui      | NOT_RUN |        |      | app launches, Hardware tab renders |
| read     | NOT_RUN |        |      | Greaseweazle — real disk |
| write    | NOT_RUN |        |      | Greaseweazle — write-back + verify |
| detect   | NOT_RUN |        |      | drive present / absent both checked |

## Automated-test self-check (no hardware) — 2026-05-15

Run by Claude during the window. Software-only; the hardware rows above
stay NOT_RUN.

- `pytest tests/differential/` → **6 passed** — gw-vs-UFT byte-exact,
  all six disk classes (IBM-DD/HD, AtariST, C64-GCR, Apple2-GCR, Amiga).
- `pytest tests/hil/` → **8 passed** — HIL scaffold + two-tier catalog.
- New improvement tests green: `test_concurrency`,
  `test_protection_detection`, `test_format_extension`.
- Full qmake build links `UnifiedFloppyTool.exe` (Windows, MinGW).
- CI matrix on `dd0ef4f` green across all platforms (PR #24).

## Findings during the window

None yet. Log any FAIL or surprising behaviour here — symptom, platform,
device, repro. A finding triages into one of:

- **critical** → hotfix to the release branch (the only code change
  allowed during the window), re-tag RC.
- **non-critical** → recorded for v4.1.5, RC stands.

## Sign-off

The window closes **2026-05-29**. Promotion to `v4.1.4` final (P2.4 —
squash-merge + final tag) requires every *build* and *gui* row PASS on
all three platforms and at least one full *read* + *detect* PASS on real
Greaseweazle hardware, with no open critical finding. Hardware rows that
remain NOT_RUN at window close must be explicitly accepted in the P2.4
sign-off, not silently treated as PASS.

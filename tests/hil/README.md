# tests/hil/ — Hardware-in-the-Loop

Layer 6 of the Tester-Strategie (`docs/TESTER_STRATEGY.md` §4). The one
layer that automation cannot replace: it needs a real controller, a real
drive and the physical **Golden-Reference** disks.

**HIL never runs in GitHub Actions.** It runs on Axel's rig, manually,
once per release. Every release tag needs a passing HIL report under
`releases/<version>/hil_report.md`.

## Files

| File | Purpose |
|------|---------|
| `golden_reference.yaml` | Catalog of the physical reference disks — id, controller, media, baseline SHA-256, known marginal tracks / protection, baseline RPM. Ships with `status: template` entries only. |
| `run_hil.py` | Runner. Drives the built `uft` binary against each `status: active` disk, automates the read→SHA-256 and rpm±0.5 checks from `HARDWARE_TRUTH_TESTS.md`, emits a Markdown report. |
| `test_hil_scaffold.py` | Self-tests — keep `pytest tests/hil/` green and prove the honesty contract (missing rig → NOT_RUN, never a fabricated PASS). |
| `conftest.py` | Puts `tests/hil/` on `sys.path`. |

## Honesty contract

`run_hil.py` follows DESIGN_PRINCIPLES "Keine erfundenen Daten":

- No hardware / no built `uft` → **NOT_RUN**, exit 0. A missing
  environment is not a failure — and never a PASS.
- A disk with no `baseline_sha256` → **NOT_RUN** for that disk. Never
  compared against an invented hash.
- A real SHA / RPM mismatch → **FAIL**, exit 1. Loud, never silent.

## Running it

```bash
# 1. Build the CLI (the cmake test-subset does NOT build it — use qmake):
qmake && make -j

# 2. Populate golden_reference.yaml: replace the template entries with
#    real disks (status: active) and their baseline SHA-256 captured on
#    `main` @ MF-149 — see HARDWARE_TRUTH_TESTS.md "Pre-refactor baseline".

# 3. Connect the rig, insert the matching disk, then:
python tests/hil/run_hil.py --version v4.1.4-rc1

# Report lands at releases/v4.1.4-rc1/hil_report.md
```

## What is NOT automated

The physical-observation rows of `HARDWARE_TRUTH_TESTS.md` — motor
spin-up sound, head-knock on seek, GUI buttons emitting signals — need a
human at the rig. `run_hil.py` lists them in the report and links back
to the checklist; the formal sign-off block in `RELEASE_NOTES.md` stays
the release gate.

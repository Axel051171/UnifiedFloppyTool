# tests/hil/ — Hardware-in-the-Loop

Layer 6 of the Tester-Strategie (`docs/TESTER_STRATEGY.md` §4).

## Two-tier Golden-Reference catalog

`golden_reference.yaml` describes the Golden-Reference set in **two
tiers**, because "golden reference" is not one thing:

| Tier | What | Runs where | Status |
|------|------|-----------|--------|
| **Software** (`software_reference`) | The P3.2 differential corpus — six deterministic synthetic disk images, each decoded by both the UFT flux engine and `gw convert` and asserted byte-identical. Hash-manifested. | GitHub Actions, **every push** | `active` — 6/6 byte-exact vs gw 1.23 |
| **Hardware** (`disks`) | Physical reference floppies on Axel's rig — read→SHA-256 + rpm±0.5 against a frozen baseline. The disks cannot live in git; only their metadata + baseline hash do. | Axel's rig, **manually, per release** | `template` — 9 controller templates, awaiting a populated set |

The software tier is the part automation **can** do, and it does it
continuously. The hardware tier is the part automation **cannot**
replace — a real controller, a real drive, the physical disks. A HIL
report shows both, so a `NOT_RUN` hardware verdict is never misread as
"nothing is verified".

**The hardware tier never runs in GitHub Actions.** It runs on Axel's
rig, manually, once per release. Every release tag needs a HIL report
under `releases/<version>/hil_report.md`.

## Files

| File | Purpose |
|------|---------|
| `golden_reference.yaml` | Two-tier catalog (see above). `software_reference` points at the differential corpus; `disks` carries one `status: template` entry per HAL controller. |
| `run_hil.py` | Runner. Drives the built `uft` binary against each `status: active` hardware disk, automates the read→SHA-256 and rpm±0.5 checks from `HARDWARE_TRUTH_TESTS.md`, and emits a Markdown report that carries both tiers. |
| `test_hil_scaffold.py` | Self-tests — keep `pytest tests/hil/` green and prove the honesty contract (missing rig → NOT_RUN, never a fabricated PASS) + that the catalog structure is complete (both tiers, all controllers). |
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

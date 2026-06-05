# Pre-Refactor Baseline Artefacts

Forensic SHA-256 baselines captured from `main @ MF-149` (commit
`5dde6fc3b807d8e0e9e3e8f2f39fde329e59c5d0`, 2026-05-04) — the last
commit before the Type-Driven HAL refactor began. Used by Phase 3 of
the `.claude/goals/v4.1.4-final.md` HIL gate to prove the refactor
introduced no semantic regression: post-refactor reads of the same
physical disk must match the baseline SHA-256 byte-for-byte.

If even one SHA does not match, v4.1.4 is **NOT shippable** — the
refactor introduced a regression. (Goal §STOP-Conditions §3.)

## Layout

```
tests/baseline_artifacts/
├── README.md                       — this file
├── gw_known_disk.scp               — full-track flux capture (kept for diff)
├── gw_known_disk.sha256            — SHA-256 of the .scp
├── gw_rpm.txt                      — 50-revolution RPM measurement
├── post_refactor/                  — Phase-3 results (filled by Phase 2 of the protocol)
│   ├── gw_known_disk.scp           — same disk, current main build
│   ├── gw_known_disk.sha256        — SHA-256 (must match baseline)
│   └── gw_rpm.txt                  — RPM (must be within ±0.5 of baseline)
└── DECISION.md                     — written by Phase 3: ALL_GREEN | PARTIAL | FAIL
```

## How to fill this directory

Run the two-phase protocol from the repo root:

```bash
# Phase 1 — capture pre-refactor baseline. Checks out MF-149,
# builds, then walks you through the GUI capture.
bash scripts/hil/gen_baseline_phase1.sh

# Phase 2 — capture post-refactor result and diff. Returns to main HEAD,
# builds, walks you through the same GUI capture, then compares.
bash scripts/hil/gen_baseline_phase2.sh
```

## Honesty rules

- **UFT is GUI-only — no `uft` CLI.** The `./uft read --hal greaseweazle ...`
  commands shown in `tests/HARDWARE_TRUTH_TESTS.md` predate the no-CLI
  decision. The script prompts you to perform the actual capture *in the
  GUI*, then computes SHA-256 of the resulting file. This is the only
  honest path on a GUI-only tool.

- **Same disk, same drive, same revolutions.** A baseline SHA only
  proves "no regression" if everything except the code is held constant.
  Use the same physical disk (write-protected), the same drive, and the
  same revolution count (default: 3) for both phases.

- **The disk choice matters.** Use a well-preserved, known-good DOS-
  formatted 3.5" HD floppy. Avoid weak-bits/copy-protected disks for
  the baseline — those have legitimate flux jitter and will produce
  different SHA-256 even between two captures from the same drive.

- **NOT_RUN is honest.** If you cannot complete the capture today, do
  not populate this directory with stubs. Leave it empty and update
  `audit/rc1_field_notes.md` accordingly — `tests/baseline_artifacts/`
  empty means Phase-3 baseline is open, not failed.

## What happens if a SHA does not match

Goal §STOP-Conditions §3:

> Wenn auch nur EIN SHA nicht matched → v4.1.4 ist NICHT shippable.
> Der Refactor hat eine Regression eingeführt. Stop, escalate,
> `docs/REFACTOR_NOTES.md` → "Hardware Truth Failure".

The mismatch itself is the finding: file a `Hardware Truth Failure`
entry, attach both `.scp` files for diff, and STOP. Do not tag.

## What is NOT in scope here

- Other controllers (SCP, KryoFlux, FluxEngine, FC5025, XUM1541,
  Applesauce, ADF-Copy, USBFloppy) — they are M3.x scope, no
  production transport in v4.1.4, see `RELEASE_NOTES.md` Known
  limitations. Their HARDWARE_TRUTH rows in `audit/rc1_field_notes.md`
  stay NOT_RUN by design.
- Capability-surface compile checks — those are unit-tested by
  `tests/hal_conformance.cpp`, not by hardware.

# tools/hw_simulators — Tier-2.5 Hardware Simulation

**Status (2026-05-25, MF-267):** New for V415-PLAN. Bridges Tier-1
(software-golden, no I/O, in CI) and Tier-3 (real hardware, manual bench
session, needs Axel's rig).

## What this is

A set of Python "fake binary" scripts that satisfy UFT's QProcess-runner
contract for each subprocess-based controller (KryoFlux DTC, FluxEngine
CLI, FC5025 fcimage). When you put `tools/hw_simulators/` first on
`PATH` and run UFT, the V2 providers' subprocess calls hit these
simulators instead of the real binaries — without needing any hardware.

The simulators read deterministic flux data from
`tests/differential/corpus/flux/*.scp` (130 MB of synthetic flux
captures, **byte-exact verified against Greaseweazle 1.23**) and emit
the exact output format each real binary produces.

## What this catches

Bugs in:
- QProcess argument construction (`build_read_argv`)
- subprocess result parsing (stdout/stderr handling, exit-code
  translation)
- File-format round-trip (KryoFlux RAW → flux decoder, fluxengine
  `.flux` → flux decoder, FC5025 sector dump → sector parser)
- Path/temp-file lifecycle (cleanup, race conditions)
- Provider-V2 outcome translation (do these subprocess paths produce
  honest `FluxCaptured` / `SectorRead` / `ProviderError`?)

## What this does NOT catch

- **Timing-dependent bugs** — simulators emit data instantly; real DTC
  takes seconds, real fluxengine has per-revolution delays. Race
  conditions in the V2 providers' wait/timeout logic won't surface.
- **USB driver issues** — libusb session-management, hotplug, transfer
  cancellation are entirely absent.
- **Signal-integrity / electrical issues** — weak bits from a real
  marginal track look identical to synthetic weak bits here.
- **Drive-specific quirks** — real 1571 vs 1581 timing, head-load
  delay, motor spin-up time — none of that exists.
- **Cross-controller VID/PID disambiguation** (ARCH-7-C: ADFCopy vs
  Applesauce both Teensy 0x16C0:0x0483) — only physical USB can prove
  the probe works.

## Usage

### Single simulator
```bash
# Linux/macOS
export PATH="$PWD/tools/hw_simulators:$PATH"
./uft   # KryoFlux/FluxEngine/FC5025 subprocess calls now hit simulators

# Windows PowerShell
$env:PATH = "$PWD\tools\hw_simulators;$env:PATH"
.\uft.exe
```

### Full simulated HIL run
```bash
python tests/hil/run_simulated.py --out /tmp/hil_simulated.md
```
This is the simulator-equivalent of `tests/hil/run_hil.py`. Result
status is **SIMULATED** (never PASS) — only real hardware can produce
PASS, but SIMULATED is strictly better than NOT_RUN.

## Files

| File | Pretends to be | Reads from | Emits |
|---|---|---|---|
| `sim_dtc.py` | KryoFlux DTC | `tests/differential/corpus/flux/*.scp` | KryoFlux RAW stream files |
| `sim_fluxengine.py` | fluxengine CLI | same | fluxengine `.flux` format on stdout |
| `sim_fcimage.py` | FC5025 fcimage | sector-extracted from corpus | raw sector dump file |
| `sim_dtc` / `sim_fluxengine` / `sim_fcimage` | (POSIX symlink wrappers) | — | — |

Each fake binary respects the exact argv contract of its real
counterpart — see source-file headers for the parsed flags.

## Adding new simulator entries

Want to add a new "simulated disk"? Drop a `.scp` file in
`tests/differential/corpus/flux/`, add an entry to
`tests/hil/golden_reference.yaml` under `simulated_hardware:`, and
re-run `run_simulated.py`. The disk-class is auto-detected from the
SCP fixture's track layout.

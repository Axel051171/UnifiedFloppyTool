# FluxEngine

**Verdict:** PARTIAL
**LOC:** 1080 (`fluxengine_provider_v2.cpp` 747 + `.h` 333) | **Integration:** CLI wrapper (`fluxengine` subprocess via injected `FluxEngineRunner` std::function — no C-HAL backbone)

FluxEngine has no `uft_fluxengine_*.c` C-HAL. The V2 provider shells out
to the `fluxengine` binary (David Given's open-source tool) through an
injected `FluxEngineRunner` lambda. The "wire protocol" UFT must get
right is the `fluxengine` **command line** — fluxengine owns the USB
layer. This is the only one of the three providers with **four**
capabilities (Read/Write/RPM/Detect) and the only one whose CLI contract
was already audited in-repo (`tests/external_audits/fluxengine/`).

Audited files:
- `src/hardware_providers/fluxengine_provider_v2.{h,cpp}` — V2 provider
- `tests/external_audits/fluxengine/` — prior in-repo FE CLI audit (reference source)
- `src/hardwaretab.cpp` — GUI routing
- `forms/tab_hardware.actions.yaml` — codegen action wiring

---

## D1 — CLI-Vertrag (fluxengine)

CLI-arg diff: `python diff.py` -> `evidence.json`. **17 PASS / 0 FAIL / 0 MISSING / 2 UNVERIFIED.** Complementary FE-flag-semantics check: `python mock_fluxengine.py`.

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| Read argv | `fluxengine read -c ibm -s drive:0 --tracks=cNhM --drive.revolutions={N} -o {out}` (`fluxengine_provider_v2.cpp:164-181`) | corrected post-2022 FE CLI form, cross-checked vs FE source via the in-repo audit | **PASS (recalled)** — 8/8 tokens |
| Write argv | `fluxengine write -c ibm -d drive:0 --tracks=cNhM -i {in}` (`:183-198`) | corrected FE CLI form | **PASS (recalled)** — 7/7 tokens |
| RPM argv | `fluxengine rpm` (`:650`) | `rpm` subcommand exists in current FE | PASS (recalled) |
| Detect argv | `fluxengine rpm` (`:698`) | detect reuses `rpm` — V1 parity | PASS (recalled) |
| `.flux` sample clock | `125.0` ns / 8 MHz hard-coded (`:470`, `.h:78`) | FE's actual `.flux` tick rate not vendored; FE has historically used a **12 MHz** device clock (~83 ns) | **UNVERIFIED (needs-source)** -> FE-D1-2 |

**Reference provenance: `recalled` (CLI) + `needs-source` (sample clock).**
FluxEngine is **open source**, and UFT already carries an in-repo
external audit of exactly this CLI contract:
`tests/external_audits/fluxengine/REPORT.md` (2026-05-14) read
FluxEngine's own flag-definition source (`src/fluxengine.cc`, `src/fe-*.cc`)
and produced the corrected CLI form. The V2 provider's
`build_read_argv()`/`build_write_argv()` were then rewritten under
**MF-178** to match it (`fluxengine_provider_v2.cpp:134-163`). So the FE
CLI reference is **stronger than SCP's or KryoFlux's** — cross-checked
against the project's own source. BUT per that audit's Stufe 5 note AND
the MF-178 comment itself (`:155-163`): "the corrected syntax ... has
NOT yet been end-to-end-tested against a real `fluxengine` binary". So
the grade is `recalled`, not `vendored`, not HIL-confirmed.

**Findings:**
- **FE-D1-1** (resolved, positive): the V1 broken-CLI defect — positional
  `ibm`, `-c N` for cylinder, `-h H`, `--revs` — found by
  `tests/external_audits/fluxengine/REPORT.md` F1+F2 **has been
  corrected** in the V2 provider under MF-178. `diff.py` confirms the V2
  emits exactly the corrected token set (17/17 argv rows PASS).
  `mock_fluxengine.py` in this directory doubles as a regression gate:
  it rejects the old V1 syntax, so a drift back would fail CI.
- **FE-D1-2** (medium): the provider hard-codes the `.flux` sample clock
  as `125.0` ns / 8 MHz (`fluxengine_provider_v2.cpp:470` and the `.h`
  comment line 78). FluxEngine's device has historically sampled at
  12 MHz (~83.3 ns/tick). The real `.flux` tick rate could not be
  confirmed from a vendored source here. **However** the impact is
  currently latent: the provider does **not decode** the `.flux` bytes —
  it stores them raw (see D2) — so `sample_ns` is metadata that no
  current consumer uses for timing math. If a future consumer trusts
  `sample_ns`, an 8-vs-12 MHz error would mis-scale every interval by
  ~1.5x. Needs a vendored FE `.flux` format spec to resolve.
- **FE-D1-3** (low): the profile is hard-coded to `"ibm"` in both
  `build_read_argv` and `build_write_argv` (`:172,190`). This is the V1
  finding F2 ("profile hardcoded, ignores UI selection") — **MF-178
  fixed the CLI *form* but not F2**: a non-IBM disk still gets the IBM
  profile regardless of GUI selection. The MF-178 comment only claims
  F1+F2 *form* correction; the profile is still literal.

**Status: PASS (with FE-D1-3 carried)** — the CLI *form* matches the
corrected FluxEngine contract (17/17 argv rows, recalled-grade,
cross-checked against FE source). The hard-coded `ibm` profile (FE-D1-3)
and the unverified 8 MHz clock (FE-D1-2) are the open items.

---

## D2 — Datapath

**Trace (read path):**
`FluxEngineProviderV2::do_read_raw_flux(ReadFluxParams)` (`fluxengine_provider_v2.cpp:386`)
-> null-runner guard -> `ProviderError` (`:388-398`)
-> geometry validation cyl 0..`m_max_cylinders` / head 0-1 (`:405-424`)
-> `build_read_argv(...)` -> corrected FE CLI argv (`:433`)
-> `m_runner(argv, "")` -> `FluxEngineRunResult{stdout_text, stderr_text, exit_code}` (`:436`)
-> `exit_code != 0` -> `fe_read_error()` `ProviderError` (`:438-440`)
-> raw flux bytes taken from `result.stdout_text` (`:446`)
-> empty -> `FluxMarginal` (`:448-457`)
-> **conversion:** `bytes_to_words()` — raw bytes re-interpreted as little-endian `uint32_t` words, 4-byte groups, tail zero-padded (`:330-354`, `:464`)
-> `FluxCaptured{ position, revolutions, sample_ns=125.0, transitions_ns=words }` (`:466-474`).

**Write path:** `do_write_raw_flux` (`:505`) — geometry + empty-stream
guards (`:524-554`), then converts `FluxStream::transitions_ns`
(`uint32_t` words) **back** to raw LE bytes as `stdin_data` (`:559-566`),
builds the write argv, runs it. If `WriteFluxParams::verify` is set, a
second read invocation is queued; on verify-read failure both intended
bytes and (empty) readback are preserved in `WriteVerifyFailed`
(`:584-608`) — rule F-3 for writes is honoured in code shape.

**Finding FE-D2-1** (high): identical defect class to KryoFlux KF-D2-1.
`bytes_to_words()` re-interprets the raw `.flux` *file bytes* (a
structured FluxEngine container format) as a flat `uint32_t` array and
stuffs them into `FluxCaptured::transitions_ns` — a field whose type
contract means "flux transition intervals in nanoseconds". `.flux` bytes
are NOT ns intervals; they are a container with a header + per-track
records. So `FluxCaptured::transitions_ns` from this provider contains
**undecoded container bytes mislabelled as ns intervals**. The comment
(`:459-463`) defers decoding to "downstream DeepRead" but nothing in the
`FluxCaptured` type flags that this vector is raw container bytes. A
type-trusting consumer reads garbage.

**Finding FE-D2-2** (high): identical to KF-D2-2. The read path takes
flux bytes from `result.stdout_text` (`:446`). But `fluxengine read`
**writes a `.flux` file to disk** (via the `-o` argument the provider
itself passes, `:178-179`) and prints a human log to stdout — it does
not emit the `.flux` binary on stdout. The provider's own comment admits
this: "In production, the runner's QProcess wrapper reads the `.flux`
output file and returns the bytes as stdout_text" (`:442-445`). So the
read path works **only** if an undocumented production runner does
file-reading — and that runner is not in this codebase. Against a naive
`QProcess` wrapper, `stdout_text` would be the log text, mis-converted
to `uint32_t` words -> garbage `FluxCaptured`.

**Finding FE-D2-3** (medium): the write path passes the flux bytes via
`stdin_data` to the runner (`:575`), but `fluxengine write` reads its
input from the `-i <file>` argument, **not stdin**. The provider
comment says the production runner "must write `stdin_data` to a temp
file before invoking fluxengine" (`:556-558,568-571`) — again an
undocumented production-runner responsibility not in this codebase. The
synthetic `/tmp/uft_fe_write_*.flux` token (`:570`) is passed as `-i`
but nothing writes a file there in the auditable code.

**Sink — where does `FluxCaptured` go?** Nowhere reachable. FluxEngine
is **not routed by `HardwareTab`** (see D3).

**Status: FAIL** — same two structural defects as KryoFlux, plus a
write-side stdin-vs-file mismatch: (FE-D2-1) `transitions_ns` holds
undecoded `.flux` container bytes mislabelled as ns intervals, (FE-D2-2)
the read source `stdout_text` is FE's log text not flux in production,
(FE-D2-3) the write source `stdin_data` is not how `fluxengine write`
takes input. The datapath is correct *only* against the mock /
`SubprocessMock`, not against a real `fluxengine`.

---

## D3 — GUI-Integration (V2-adapted)

D3 re-defined for V2: is the provider routed by `HardwareTab`, and are
its capability buttons gated by codegen `wire_action<cap::X>`?

**Routing:** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249`)
returns `m_gwProviderV2.get()` — hardcoded to the Greaseweazle type.
There is no `m_fluxengineProviderV2` member. `onConnect()` for the combo
data value `"fluxengine"` (`hardwaretab.cpp:284`) takes the
`controller != "greaseweazle"` branch (`:624`) and shows the "Controller
routing pending" messagebox (`:634-644`). **FluxEngine is NOT routed.**

**Capability -> button matrix** (codegen `wire_action<cap::X>`):

| Button | Capability tag | `FluxEngineProviderV2` satisfies? | Wired |
|--------|----------------|------------------------------------|-------|
| `btnReadTest` | `ReadsRawFlux` | yes (`static_assert` `fluxengine_provider_v2.h:289`) | provider has it; never reached — not routed |
| `btnRPMTest` | `MeasuresRPM` | yes (`:293`) | provider has it; never reached — not routed |
| `btnDetect` | `DetectsDrive` | yes (`:296`) | provider has it; never reached — not routed |
| (write flux) | `WritesRawFlux` | yes (`:291`) | provider has it; no dedicated GUI button in `actions.yaml` |
| `btnMotorOn/Off` | `ControlsMotor` | **no** — negative `static_assert` (`:306`) | correctly disabled branch |
| `btnSeekTest` | `SeeksHead` | **no** — negative `static_assert` (`:310`) | correctly disabled branch |
| `btnCalibrate` | `Recalibrates` | **no** — negative `static_assert` (`:314`) | correctly disabled branch |

The **capability gating is structurally correct**: `FluxEngineProviderV2`
declares `ReadsRawFlux`/`WritesRawFlux`/`MeasuresRPM`/`DetectsDrive`,
carries negative `static_assert`s for the 3 omitted capabilities, and
the composite `ImagesFlux` / `WritesAnything` / `!FullDriveControl`
assertions (`:320-328`). The header is unusually thorough — it documents
which V1 methods were REAL (`readRawFlux`, `writeRawFlux`, `detectDrive`,
`measureRPM`) versus SILENT STUBS (`setMotor`, `seekCylinder`,
`recalibrate`) at `fluxengine_provider_v2.h:14-40`, and the
`MeasuresRPM` positive assertion is explicitly justified ("V1
measureRPM() wraps `fluxengine rpm` honestly", `:294-295`). This is the
right discipline — only V1-real capabilities became V2 mixins.

**Finding FE-D3-1** (high): `FluxEngineProviderV2` is fully concept-
conformant but `HardwareTab` does not route to it — the P1.18 cross-
provider gap. Selecting "FluxEngine" only produces the routing-pending
messagebox.

**Status: PARTIAL** — capability gating is correct by construction and
the V1-real-vs-stub discipline is the best-documented of the three;
routing is absent (P1.18).

---

## D4 — OS-Detection

FluxEngine device enumeration: **there is none in the V2 provider.** Like
KryoFlux, the provider takes a runner + binary path at construction and
never enumerates a device itself — `fluxengine` does that internally via
libusb. The OS-detection surface is limited to:

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | **No UFT-side detection at all.** `detectSerialPorts()` (`hardwaretab.cpp:426-519`) has VID/PID hints for Greaseweazle, SuperCard Pro, KryoFlux and XUM1541 — **but none for FluxEngine**. FluxEngine's USB device (VID `0x1209` PID `0x6e00`, per the in-repo external audit REPORT.md) is not in the hint table. Detection is entirely delegated to the `fluxengine` binary. | no UFT-side hint; delegated to FE | **PARTIAL** |
| **Linux** | Same — no UFT-side FluxEngine hint; `fluxengine` enumerates via libusb. | delegated to FE | **PARTIAL** |
| **macOS** | Same — delegated to FE. | delegated to FE | **PARTIAL** |

**Finding FE-D4-1** (low): FluxEngine has **no VID/PID hint** in UFT's
`detectSerialPorts()` table (`hardwaretab.cpp:447-455`), unlike the
other flux controllers. This is arguably *correct* for a pure CLI-wrapper
backend — `fluxengine` finds its own device — but it is inconsistent
with how KryoFlux (also a CLI wrapper) is handled (KryoFlux *does* have a
hint). For a CLI wrapper, delegating detection to the external binary is
the right design; the inconsistency is cosmetic. The real detection is
`do_detect_drive` -> `fluxengine rpm` (`:684-743`).

**Finding FE-D4-2** (medium): `do_detect_drive` uses `fluxengine rpm` as
the detection probe (`:698`). The in-repo external audit REPORT.md F3
flags that FluxEngine's `--version` detection is fragile; the same audit
notes `rpm` exists and works for the happy path. But if `fluxengine rpm`
exits 0 with no parseable RPM (e.g. "No index pulses detected; is a disk
in the drive?" — quoted in the external audit's Stufe 3), `do_detect_drive`
**defaults `rpm_nominal` to 300.0 and reports `DriveDetected`** (`:713-743`)
— a "drive detected" result when FE actually said there is no disk. See
FE-D5-2.

**Status: PARTIAL** — detection is correctly delegated to `fluxengine`
(right design for a CLI wrapper), but there is no UFT-side device hint
at all and `do_detect_drive` can report a false-positive `DriveDetected`
on FE's "no disk" output.

---

## D5 — Integritaets-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| FE-D5-1 | **`transitions_ns` populated with undecoded `.flux` container bytes** mislabelled as ns intervals (see FE-D2-1). Same type-contract violation as KryoFlux KF-D5-1. The `bytes_to_words()` helper re-interprets a structured FluxEngine container as a flat `uint32_t` array and stuffs it into a field meaning "ns intervals". A type-trusting consumer reads garbage. | high | `fluxengine_provider_v2.cpp:330-354,464-474` |
| FE-D5-2 | `do_detect_drive` defaults `rpm_nominal = 300.0` when `fluxengine rpm` output has no parseable RPM (`:713-715`), then infers `drive_kind` from that defaulted value (`:717-727`) and returns `DriveDetected` with hard-coded `tracks=80` / `heads=2` (`:738-739`). FluxEngine's "No index pulses detected; is a disk in the drive?" output (per the in-repo audit's Stufe 3) parses to no RPM -> 300 default -> a **`DriveDetected` success reported for an empty/diskless drive**. A caller doing `std::holds_alternative<DriveDetected>` is misled. | medium | `fluxengine_provider_v2.cpp:705-743` |
| FE-D5-3 | **`fe_range_error_flux()` returns a `ProviderError` as a "no error" sentinel.** When cylinder/head are *in range*, the helper falls through to a `ProviderError` whose `what` literally says "internal range check returned without finding error ... should never be visible to callers" (`:123-131`). This is a fragile control-flow pattern: the function's success path constructs an error object. If a future caller does not check `holds_alternative` correctly, a valid in-range read would surface a spurious "programming bug" `ProviderError`. As written, no caller actually invokes `fe_range_error_flux` (the inline guards in `do_read_raw_flux` duplicate the logic), so it is **dead code that is also a foot-gun**. | medium | `fluxengine_provider_v2.cpp:98-132` |
| FE-D5-4 | `do_measure_rpm` returns `RpmMeasured{rpm=0.0, jitter_pct=0.0, revolutions_sampled=0}` when the RPM regex finds nothing (`:660-664`). This is honest-ish (rpm=0 is a sentinel, the comment says it satisfies `r.rpm >= 0.0`) — but a `RpmMeasured` *success variant* carrying `rpm=0.0` is ambiguous: a caller cannot distinguish "measured 0 RPM (drive stopped)" from "could not parse RPM". A `ProviderError` or a distinct outcome would be cleaner. Fails safe, so low-medium. | low | `fluxengine_provider_v2.cpp:636-665` |
| FE-D5-5 | RPM/version parsers use permissive regexes (`(\d+\.?\d*)\s*rpm`, `FluxEngine\s+(\S+)`) — same fragility class as the in-repo audit's F4. The version parser has a "first non-empty line" fallback (`:313-319`) that would happily return a *log line* as a version string. Fails safe (no crash) but can mislabel. | low | `fluxengine_provider_v2.cpp:276-320` |

**Positives (forensic-integrity holds):**
- No swallowed errors — `exit_code != 0` from the runner always becomes
  a typed `ProviderError` (`fe_read_error`/`fe_write_error`/
  `fe_not_found_error`, `:201-273`), never a silent success.
- Rule F-4: every `ProviderError` carries non-empty what/why/fix; the
  ctor throws on empty strings. `fix` strings are actionable.
- Rule F-3 (write): the verify path preserves both intended bytes and
  the (empty) readback in `WriteVerifyFailed` rather than discarding
  either (`:594-607`).
- Null-runner construction is honest: all four `do_*` methods return a
  `ProviderError` when `m_runner` is null, never a fake success.
- Empty FE output -> `FluxMarginal`, not a fabricated `FluxCaptured`
  (`:448-457`).
- The MF-178 CLI correction is real and is the *right* fix for the V1
  broken-CLI defect — `diff.py` and `mock_fluxengine.py` both confirm it.
- Capability mixins map 1:1 to V1-REAL methods; V1 silent stubs were
  correctly NOT promoted to mixins (`fluxengine_provider_v2.h:14-40`).

**Status: PASS-with-findings on stub-honesty, but the datapath
(FE-D5-1) and the sentinel-error anti-pattern (FE-D5-3) are real
defects.** No P0 fabrication — but a production capture produces a
structurally-wrong `FluxCaptured`, and `do_detect_drive` can
false-positive on FE's "no disk" output (FE-D5-2).

---

## Fixes (prioritised)

**P0:** none — nothing returns a fabricated success.

**P1:**
- **FE-D5-1 / FE-D2-1** — do not stuff raw `.flux` container bytes into
  `FluxCaptured::transitions_ns`. Decode `.flux` to real ns intervals in
  the provider, or add a distinct field/variant for "undecoded container".
- **FE-D2-2 / FE-D2-3** — the production `FluxEngineRunner` must read the
  on-disk `.flux` file `fluxengine read` writes (not pass FE's stdout log
  as flux) and must write `stdin_data` to a file for `fluxengine write -i`.
  Implement and check in the production runner so the audit can verify it;
  as written, both paths only work against the mock.
- **FE-D1-3** — parametrise the profile: `build_read_argv`/`build_write_argv`
  hard-code `"ibm"`. This is the V1 finding F2; MF-178 fixed the CLI
  *form* but not the hard-coded profile. Thread the GUI-selected profile
  through.
- **FE-D3-1** — route `FluxEngineProviderV2` through `HardwareTab` (P1.18).

**P2:**
- **FE-D5-2** — `do_detect_drive` must not return `DriveDetected` when
  `fluxengine rpm` reports no disk / no index pulses. Detect that output
  and return `DriveAbsent` or a `ProviderError`.
- **FE-D5-3** — delete the dead `fe_range_error_flux()` helper, or
  redesign it to not return a `ProviderError` as a no-error sentinel.
- **FE-D1-2** — vendor the FluxEngine `.flux` format spec and confirm
  the sample clock (8 MHz vs 12 MHz). Latent today (bytes not decoded)
  but a hazard once a consumer trusts `sample_ns`.

**P3:**
- **FE-D5-4** — make `do_measure_rpm` distinguish "0 RPM measured" from
  "could not parse".
- **FE-D5-5** — harden the RPM/version parsers (the in-repo audit's F4).
- **FE-D4-1** — decide whether FluxEngine should have a VID/PID hint for
  consistency with KryoFlux, or document that CLI wrappers intentionally
  do not.

---

## Reproduce

```bash
cd audit/fluxengine
python extract_uft.py        # fluxengine argv UFT builds (JSON)
python extract_ref.py        # corrected FE CLI reference + provenance (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
python mock_fluxengine.py read -c ibm -s drive:0 --tracks=c40h0 \
       --drive.revolutions=2 -o /tmp/x.flux        # corrected V2 argv — accepts
python mock_fluxengine.py read ibm -s drive:0 -c 40 -h 0 --revs=2 -o /tmp/x
       # old broken V1 argv — rejected (regression gate)
```

Source reads behind this report:
```bash
sed -n '134,198p' src/hardware_providers/fluxengine_provider_v2.cpp  # MF-178 + build_*_argv
sed -n '330,475p' src/hardware_providers/fluxengine_provider_v2.cpp  # bytes_to_words + do_read_raw_flux
sed -n '505,616p' src/hardware_providers/fluxengine_provider_v2.cpp  # do_write_raw_flux
sed -n '636,744p' src/hardware_providers/fluxengine_provider_v2.cpp  # do_measure_rpm + do_detect_drive
sed -n '285,328p' src/hardware_providers/fluxengine_provider_v2.h    # capability asserts
cat tests/external_audits/fluxengine/REPORT.md                       # prior in-repo CLI audit
sed -n '624,645p' src/hardwaretab.cpp                                # routing-pending branch
```

## Not reviewed
- The production `FluxEngineRunner` lambda — described only in a header
  comment (`fluxengine_provider_v2.h:155-185`); no production runner is
  in this codebase, so its file-read/file-write behaviour cannot be
  audited (FE-D2-2 / FE-D2-3).
- Real `fluxengine` CLI behaviour — the corrected MF-178 syntax was
  cross-checked against FE source via the in-repo audit but NOT
  end-to-end-tested against a real binary (MF-178 comment, `:155-163`).
- The `.flux` container format decoder ("downstream DeepRead") the
  provider defers to — out of scope; this audit covers the HAL provider.
- Runtime behaviour against real FluxEngine hardware — that is HIL
  (`tests/hil/`, P3.4).

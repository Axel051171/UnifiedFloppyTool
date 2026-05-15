# KryoFlux

**Verdict:** PARTIAL
**LOC:** 717 (`kryoflux_provider_v2.cpp` 429 + `.h` 288) | **Integration:** CLI wrapper (DTC subprocess via injected `DtcRunner` std::function — no C-HAL backbone)

KryoFlux has no `uft_kryoflux_*.c` C-HAL in this codebase. The V2
provider shells out to **DTC (Disk Tool Console)**, a closed-source
vendor binary, through an injected `DtcRunner` lambda (production: wraps
`QProcess`; tests: wraps `SubprocessMock`). The "wire protocol" UFT must
get right is the **DTC command line** — DTC owns the USB layer.

Audited files:
- `src/hardware_providers/kryoflux_provider_v2.{h,cpp}` — V2 provider
- `src/hardwaretab.cpp` — GUI routing
- `forms/tab_hardware.actions.yaml` — codegen action wiring

---

## D1 — CLI-Vertrag (DTC)

CLI-arg diff: `python diff.py` -> `evidence.json`. **4 PASS / 0 FAIL / 0 MISSING / 8 UNVERIFIED.** Complementary argv-shape check: `python mock_dtc.py`.

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| Read argv shape | `dtc -c2 -d0 -s{head} -b{cyl} -e{cyl} -f{prefix} -i0` (`kryoflux_provider_v2.cpp:86-95`) | DTC closed-source; no vendored `--help` output, no DTC binary in audit env | **UNVERIFIED (needs-source)** -> KF-D1-1 |
| Detect argv shape | `dtc -i0` (`:380`) | bare `-i0` as a probe — validity vs requiring `-c`/`-d` unverified | **UNVERIFIED (needs-source)** -> KF-D1-2 |
| `-c2` = read-to-stream, `-d0` = drive 0, `-b/-e` = begin/end track, `-f` = output prefix, `-i0` = raw stream | as emitted | recalled from community wiki; not vendored, not runnable here | UNVERIFIED (needs-source) |
| `-s{head}` flag | UFT passes the **head number** as `-s` (`:90`) | DTC's `-s` semantics (side select vs start-track) are **version-dependent** per community docs | **UNVERIFIED (needs-source)** -> KF-D1-1 |
| Stream sample clock | `1000.0/24.0` ns (~41.67 ns), 24 MHz (`:343`) | KryoFlux master clock 24.027428 MHz, commonly rounded to 24 MHz / 41.67 ns | PASS (recalled) |
| Geometry guard | cylinder 0-83 (`:248`), head 0-1 (`:258`) | standard floppy physical limit | PASS (recalled) |

**Reference provenance: `needs-source (DTC CLI) + recalled (24 MHz clock)`.**
DTC is a **closed-source** vendor binary from the Software Preservation
Society. There is no published vendor SDK for its command line, no DTC
binary installed in the audit environment, and no vendored copy of its
`--help` text. The DTC flag *meanings* below are recalled from the
KryoFlux community wiki, but the *audit* of UFT's argv against them is
needs-source: a definitive arg-exact check requires either the DTC
binary or a vendored copy of its help output. The 24 MHz stream clock
**is** well-known KryoFlux-stream-format knowledge — recalled-grade,
real PASS.

`mock_dtc.py` is the KryoFlux analogue of the FluxEngine audit's
`mock_fluxengine.py`: it accepts only the flag *set* UFT passes and
rejects malformed argv. A green `mock_dtc.py` run means "UFT's argv is
well-formed against the recalled DTC flag model" — **not** "byte-exact
against real DTC".

**Findings:**
- **KF-D1-1** (high): UFT passes the **head number** through DTC's `-s`
  flag (`kryoflux_provider_v2.cpp:90`). Community documentation indicates
  `-s` in some DTC versions means *start track*, not *side*. If the real
  DTC interprets `-s0`/`-s1` as a start-track override, UFT would
  silently capture the wrong physical track for head 1. This cannot be
  resolved without a vendored DTC manual or a runnable DTC. `mock_dtc.py`
  flags `-s{0,1}` as a WARN for exactly this reason.
- **KF-D1-2** (medium): UFT's drive-detect uses the bare invocation
  `dtc -i0` with no `-c` command and no `-d` drive selector (`:380`).
  Whether `-i0` alone is a valid DTC probe — or whether DTC requires a
  command — is unverified. If `dtc -i0` exits non-zero on a real DTC
  even with hardware present, `do_detect_drive` would always return a
  `ProviderError` (false "no device"). Needs-source.
- **KF-D1-3** (low): the `-f{prefix}` path is a synthetic `/tmp/uft_kf_*`
  token (`:272-273`); on Windows DTC this is not a valid path. The
  provider's contract says the *production runner* must substitute a
  real temp dir — but that runner is not in this codebase (the
  production `DtcRunner` lambda is described only in a header comment,
  `kryoflux_provider_v2.h:143-156`), so the substitution is unverified.

**Status: UNVERIFIED** — the 4 recalled rows (clock + geometry) PASS;
the 8 DTC-CLI rows cannot be verified without vendoring the DTC contract
or a runnable DTC binary. KF-D1-1 (the `-s` head/side ambiguity) is a
forensic hazard — a wrong-track capture would be silent.

---

## D2 — Datapath

**Trace (read path):**
`KryoFluxProviderV2::do_read_raw_flux(ReadFluxParams)` (`kryoflux_provider_v2.cpp:228`)
-> null-runner guard -> `ProviderError` (`:230-240`)
-> geometry validation cyl 0-83 / head 0-1 (`:248-266`)
-> `build_read_argv(cyl, head, prefix)` -> `dtc -c2 -d0 -s{head} -b{cyl} -e{cyl} -f{prefix} -i0` (`:275`)
-> `m_runner(argv, "")` -> `DtcRunResult{stdout_text, stderr_text, exit_code}` (`:277`)
-> `exit_code != 0` -> `dtc_read_error()` `ProviderError` (`:279-281`)
-> raw stream bytes taken from `result.stdout_text` (`:295`)
-> empty -> `FluxMarginal` (`:297-307`)
-> **conversion:** raw bytes re-interpreted as little-endian `uint32_t` words, 4-byte groups, tail zero-padded (`:316-337`)
-> `FluxCaptured{ position, revolutions, sample_ns=1000.0/24.0, transitions_ns=words }` (`:339-347`).

**Conversions named:** raw KryoFlux stream bytes -> `uint32_t` LE words
(NOT decoded — see below); `sample_ns = 1000.0/24.0` (~41.67 ns);
`revolutions` set to the requested count.

**Finding KF-D2-1** (high): the conversion at `:316-337` is **not a
flux conversion at all** — it re-interprets the raw KryoFlux *stream
file bytes* (a variable-length-opcode binary format) as a flat array of
`uint32_t` words and stuffs them into `FluxCaptured::transitions_ns`.
`transitions_ns` is, by the name and by every other provider's usage, a
list of **flux transition intervals in nanoseconds**. KryoFlux stream
bytes are NOT transition intervals — they are opcodes (Flux1, Flux2,
Flux3, Nop, Ovl16, Index, etc.). So `FluxCaptured::transitions_ns` from
this provider contains **opcode bytes mislabelled as nanosecond
intervals**. The code comment (`:309-314`) acknowledges this and defers
"KryoFlux-specific decoding" to "the DeepRead pipeline" — but nothing in
`FluxCaptured` flags that this particular vector is undecoded raw-stream
bytes rather than ns intervals. A downstream consumer that trusts the
type will read garbage. This is a **type-contract violation** disguised
as F-3 "preserve verbatim".

**Finding KF-D2-2** (high): the read path takes the raw stream bytes
from `result.stdout_text` (`:295`). But real DTC **writes stream files
to disk** (`track{NN}.{S}.raw`) and prints a human-readable *log* to
stdout — it does not emit binary stream data on stdout. The provider's
own comment admits this: "In production, DTC writes the file to disk and
stdout is a log — we'd read the file ... but we cannot read it from here
(no Qt filesystem in this C++ layer)" (`:283-294`). So in production, on
a successful DTC read, `stdout_text` would contain the *log text*, not
flux — and either (a) the log is non-empty and gets mis-converted to
`uint32_t` words as if it were flux (garbage `FluxCaptured`), or (b) the
production runner has undocumented file-reading behaviour that is not in
this codebase. The "test-mode shortcut" of carrying bytes in
`stdout_text` is the **only** path that works, and it only works against
the mock.

**Sink — where does `FluxCaptured` go?** Nowhere reachable. KryoFlux is
**not routed by `HardwareTab`** (see D3) — selecting "KryoFlux" and
connecting shows the "routing pending (P1.18)" messagebox.

**Status: FAIL** — the datapath has two structural defects independent
of routing: (KF-D2-1) `transitions_ns` is populated with undecoded
opcode bytes mislabelled as ns intervals, and (KF-D2-2) in production
the source of those bytes (`stdout_text`) would be DTC's log text, not
flux at all. The read path is correct *only* against `mock_dtc.py` /
`SubprocessMock`, not against a real DTC.

---

## D3 — GUI-Integration (V2-adapted)

D3 re-defined for V2: is the provider routed by `HardwareTab`, and are
its capability buttons gated by codegen `wire_action<cap::X>`?

**Routing:** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249`)
returns `m_gwProviderV2.get()` — hardcoded to the Greaseweazle type.
There is no `m_kryofluxProviderV2` member. `onConnect()` for the combo
data value `"kryoflux"` (`hardwaretab.cpp:283`) takes the
`controller != "greaseweazle"` branch (`:624`) and shows the "Controller
routing pending" messagebox (`:634-644`). **KryoFlux is NOT routed.**

**Capability -> button matrix** (codegen `wire_action<cap::X>`):

| Button | Capability tag | `KryoFluxProviderV2` satisfies? | Wired |
|--------|----------------|----------------------------------|-------|
| `btnReadTest` | `ReadsRawFlux` | yes (`static_assert` `kryoflux_provider_v2.h:244`) | provider has it; never reached — not routed |
| `btnDetect` | `DetectsDrive` | yes (`:246`) | provider has it; never reached — not routed |
| `btnMotorOn/Off` | `ControlsMotor` | **no** — negative `static_assert` (`:259`) | correctly disabled branch |
| `btnSeekTest` | `SeeksHead` | **no** — negative `static_assert` (`:263`) | correctly disabled branch |
| `btnCalibrate` | `Recalibrates` | **no** — negative `static_assert` (`:266`) | correctly disabled branch |
| `btnRPMTest` | `MeasuresRPM` | **no** — negative `static_assert` (`:270`) | correctly disabled branch |
| (write flux) | `WritesRawFlux` | **no** — negative `static_assert` (`:253`) — KryoFlux is read-only | correctly absent |

The **capability gating is structurally correct and notably honest**:
`KryoFluxProviderV2` declares only `ReadsRawFlux`/`DetectsDrive`, carries
negative `static_assert`s for the 5 omitted capabilities, AND the
composite `!WritesAnything` / `!FullDriveControl` assertions
(`:279-284`). The header rationale explicitly notes that the V1
`setMotor()`/`seekCylinder()` were "silent stubs" and that omitting the
mixin "IS the documentation — per anti-pragmatism rule"
(`kryoflux_provider_v2.h:24-41`). This is the cleanest capability
declaration of the three providers audited.

**Finding KF-D3-1** (high): `KryoFluxProviderV2` is fully concept-
conformant but `HardwareTab` does not route to it — the P1.18 cross-
provider gap. Selecting "KryoFlux" only produces the routing-pending
messagebox.

**Status: PARTIAL** — capability gating is correct by construction and
the read-only omission of `WritesRawFlux` is exemplary; routing is
absent (P1.18).

---

## D4 — OS-Detection

KryoFlux device enumeration: **there is none in the V2 provider.** The
provider takes a `DtcRunner` and a `dtc_binary` path string at
construction; it never enumerates a serial port or USB device itself —
DTC does that internally. The only OS-detection surface is:

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | GUI-side only: `QSerialPortInfo` VID/PID hint matches `vid==0x0403 && pid==0x6001` -> label "KryoFlux (FTDI)" (`hardwaretab.cpp:451-452`). This is the **generic FTDI FT232 VID/PID** — KryoFlux uses an FT245R. The hint is informational; it does not gate anything. | Qt hint present; generic FTDI ID | **PARTIAL** |
| **Linux** | Same `0x0403/0x6001` Qt hint. DTC itself finds the device via libusb. UFT does not enumerate. | Qt hint present | **PARTIAL** |
| **macOS** | `QSerialPortInfo` works on macOS; same generic FTDI hint. | Qt hint present | **PARTIAL** |

**Finding KF-D4-1** (medium): the only KryoFlux device identification in
UFT is the GUI port hint `vid==0x0403 && pid==0x6001` (`hardwaretab.cpp:451`),
which is the **generic FTDI FT232 VID/PID** used by thousands of
unrelated devices — it is not KryoFlux-specific. A KryoFlux board uses
an FT245R USB FIFO; depending on the board revision the PID may be
`0x6001` or another FTDI PID. So the hint will both (a) mis-label
unrelated FTDI serial adapters as "KryoFlux (FTDI)" and (b) potentially
miss a KryoFlux with a different FTDI PID. Since the hint is purely
cosmetic (it does not gate connection — DTC enumerates), the impact is
low, but it is misleading. The real detection is entirely delegated to
DTC, which is correct for a CLI-wrapper backend.

**Finding KF-D4-2** (low): there is no way for UFT to verify a KryoFlux
is present *before* invoking DTC — `do_detect_drive` IS the detection,
and it depends on `dtc -i0` (KF-D1-2). If the `dtc_binary` path is wrong
or DTC is not installed, detection fails with a `ProviderError` that
correctly says so (`dtc_not_found_error`, `:99-120`) — honest, but it
cannot distinguish "no DTC" from "no device" until DTC runs.

**Status: PARTIAL** — detection is correctly delegated to DTC (right
design for a CLI wrapper), but the only UFT-side device hint is a
generic FTDI VID/PID that is not KryoFlux-specific.

---

## D5 — Integritaets-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| KF-D5-1 | **`transitions_ns` populated with undecoded raw-stream opcode bytes** mislabelled as nanosecond transition intervals (see KF-D2-1). `FluxCaptured::transitions_ns` is a typed contract meaning "ns intervals"; this provider fills it with raw KryoFlux stream bytes re-interpreted as `uint32_t` words. The code comment defends it as F-3 "preserve verbatim" — but preserving bytes into a field whose type means something else is a silent type-contract violation, not faithful preservation. A downstream consumer reads garbage. | high | `kryoflux_provider_v2.cpp:309-345` |
| KF-D5-2 | **Production stdout-as-flux assumption is wrong.** Real DTC writes stream files to disk and prints a *log* to stdout. The provider reads flux from `result.stdout_text` (`:295`). In production, on success, `stdout_text` is the log, so either the log gets mis-converted to flux words (garbage `FluxCaptured`) or an undocumented production runner does file-reading not present in this codebase. Only the mock/test path works. The comment admits this (`:283-294`). | high | `kryoflux_provider_v2.cpp:283-295` |
| KF-D5-3 | `do_detect_drive` defaults `rpm_nominal = 300.0` when DTC output has no parseable RPM (`:402-404`), then **infers a `drive_kind` string from that defaulted RPM** (`:406-416`) — so a drive with no RPM info is reported as `"3.5\" DD/HD"` with `rpm_nominal=300.0` as if measured. Same forensic foot-gun as SCP-D5-2: caller cannot distinguish measured from defaulted. `tracks=80` / `heads=2` are also hard-coded (`:420-421`). | medium | `kryoflux_provider_v2.cpp:399-423` |
| KF-D5-4 | RPM parser `parse_rpm_from_dtc_output` uses a permissive regex `(\d+\.?\d*)\s*rpm` (`:156`) and an `index:.*ms` fallback (`:166`). Like the FluxEngine audit's F4 finding, this is fragile against DTC output format / locale changes — but it fails *safe* (no match -> 0.0 -> default 300), so it is a robustness issue not a fabrication. | low | `kryoflux_provider_v2.cpp:149-175` |
| KF-D5-5 | `-s{head}` head/side ambiguity (KF-D1-1) — if DTC's `-s` is start-track not side, the provider would **silently read the wrong physical track** for head 1 and label it as head 1. A wrong-track capture presented as correct is a forensic-integrity violation. Unverified -> cannot confirm it happens, but it cannot be ruled out either. | medium (unverified) | `kryoflux_provider_v2.cpp:90` |

**Positives (forensic-integrity holds):**
- No swallowed errors — `exit_code != 0` from the runner always becomes
  a typed `ProviderError` (`dtc_read_error` / `dtc_not_found_error`,
  `:99-146`), never a silent success.
- Rule F-4: every `ProviderError` carries non-empty what/why/fix; the
  ctor throws on empty strings (`outcomes.h:118`). The `fix` strings are
  genuinely actionable ("Install DTC from ... ensure 'dtc' is on PATH").
- Null-runner construction is honest: `do_read_raw_flux` /
  `do_detect_drive` both return a `ProviderError` when `m_runner` is
  null, never a fake success (`:230-240,368-378`).
- Empty DTC output -> `FluxMarginal`, not a fabricated `FluxCaptured`
  (`:297-307`).
- The capability omissions are honest and documented (read-only device,
  no motor/seek stubs) — the cleanest of the three providers.

**Status: PASS-with-findings on stub-honesty, but the datapath
(KF-D5-1, KF-D5-2) is a real defect.** No *fabricated* data — but the
read path mislabels raw opcode bytes as ns intervals and assumes a
stdout source that real DTC does not use. There is no P0 "fake success",
yet a production capture would produce a structurally-wrong
`FluxCaptured`.

---

## Fixes (prioritised)

**P0:** none — nothing returns a fake success; but see P1, the datapath
is broken in production.

**P1:**
- **KF-D5-2 / KF-D2-2** — the production `DtcRunner` must read the
  on-disk `track{NN}.{S}.raw` stream file DTC writes, not pass DTC's
  stdout log as flux. Either (a) implement and check in the production
  runner so the audit can verify it, or (b) move file-reading into the
  provider. As written, the read path only works against the mock.
- **KF-D5-1 / KF-D2-1** — do not stuff raw KryoFlux stream bytes into
  `FluxCaptured::transitions_ns`. Either decode the stream to real ns
  intervals in the provider, or add a distinct field / outcome variant
  for "undecoded raw stream" so the type contract is not violated.
- **KF-D1-1 / KF-D5-5** — vendor the DTC manual (or run DTC once) and
  confirm whether `-s` is *side* or *start-track*. A wrong-track capture
  for head 1 would be silent. *(blocks: arg-exact D1 verification)*
- **KF-D3-1** — route `KryoFluxProviderV2` through `HardwareTab` (P1.18).

**P2:**
- **KF-D1-2** — confirm `dtc -i0` alone is a valid probe; if DTC needs a
  command, `do_detect_drive` will always false-negative.
- **KF-D5-3** — `do_detect_drive` must not present a defaulted
  `rpm_nominal=300.0` / `drive_kind` as if measured. Surface measured-vs-
  defaulted, or return `DriveAbsent` when DTC output is unparseable.

**P3:**
- **KF-D4-1** — the GUI hint `0x0403/0x6001` is the generic FTDI VID/PID,
  not KryoFlux-specific; either narrow it or label it "FTDI serial
  (possibly KryoFlux)".
- **KF-D5-4** — make the RPM parser more robust or accept a structured
  DTC output mode if one exists.

---

## Reproduce

```bash
cd audit/kryoflux
python extract_uft.py        # DTC argv UFT builds (JSON)
python extract_ref.py        # DTC CLI reference + provenance (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
python mock_dtc.py -c2 -d0 -s0 -b40 -e40 -f/tmp/x -i0   # argv-shape check (read)
python mock_dtc.py -i0                                  # argv-shape check (probe)
```

Source reads behind this report:
```bash
sed -n '83,96p'   src/hardware_providers/kryoflux_provider_v2.cpp  # build_read_argv
sed -n '228,348p' src/hardware_providers/kryoflux_provider_v2.cpp  # do_read_raw_flux
sed -n '366,426p' src/hardware_providers/kryoflux_provider_v2.cpp  # do_detect_drive
sed -n '240,285p' src/hardware_providers/kryoflux_provider_v2.h    # capability asserts
sed -n '624,645p' src/hardwaretab.cpp                              # routing-pending branch
```

## Not reviewed
- The production `DtcRunner` lambda — it is described only in a header
  comment (`kryoflux_provider_v2.h:143-156`); no production runner is in
  this codebase, so its file-reading behaviour cannot be audited.
- Real DTC CLI semantics — DTC is closed-source, not installed, and not
  vendored. Every D1 row is needs-source for that reason.
- The KryoFlux stream-format opcode decoder ("DeepRead pipeline") that
  the provider defers to — out of scope; this audit covers the HAL
  provider only.
- Runtime behaviour against real KryoFlux hardware + real DTC — that is
  HIL (`tests/hil/`, P3.4).

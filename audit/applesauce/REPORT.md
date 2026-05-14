# Applesauce

**Verdict:** PARTIAL
**LOC:** 1581 (`applesauce_provider_v2.cpp` 922 + `.h` 659) | **Integration:** injected-runner (7 runner `std::function`s + an `IApplesauceTransport` virtual interface; production wraps `QSerialPort`, tests wrap `SerialMock` — no production construction site in the tree)

Applesauce (Evolution Interactive / John Googin) is a flux-level Apple
disk controller speaking an ASCII line protocol over USB-CDC serial
(115200 8N1) plus a binary-bulk mode for flux transfer. It is the
**widest** V2 provider: 7 capabilities — `ReadsRawFlux`, `WritesRawFlux`,
`ControlsMotor`, `SeeksHead`, `Recalibrates`, `MeasuresRPM`,
`DetectsDrive` (`FullDriveControl`). The provider header notes the V1
implementation was "100% real serial dialogs — no silent stubs"
(`applesauce_provider_v2.h:38-39`). The V2 provider is an **honest
scaffold** — every `do_*` path returns a typed `ProviderError` (with an
explicit "M3.3 pending" marker) when its runner is null or reports
`transport_unavailable`; it never fakes a successful read, write, seek,
or RPM measurement.

Audited files:
- `src/hardware_providers/applesauce_provider_v2.{h,cpp}` — V2 provider
- `include/uft/hal/uft_applesauce.h` — C HAL header (`UFT_SKELETON_PARTIAL`, M3.3: 13 real / 7 honest stubs)
- `src/hal/uft_applesauce.c` — C HAL impl (honest M3.3 stubs return `UFT_ERR_NOT_IMPLEMENTED`)
- `src/hardwaretab.cpp` — GUI routing (not wired for Applesauce)

---

## D1 — Wire-Protocol

Constants diff: `python diff.py` -> `evidence.json`. **10 PASS / 0 FAIL / 18 UNVERIFIED.**

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| `RESP_OK` | `'.'` (`applesauce_provider_v2.cpp:87`) | `'.'` = command OK | PASS |
| `RESP_ERROR` | `'!'` (`:88`) | `'!'` = error | PASS |
| `RESP_ON` | `'+'` (`:89`) | `'+'` = on / write-protect active | PASS |
| Sample clock | `8000000.0` Hz (`:91`, `AS_SAMPLE_CLOCK_HZ`) | 8 MHz | PASS |
| Tick resolution | `125.0` ns derived (`:92`, `1e9/8e6`) | 125 ns | PASS |
| C-HAL sample clock | `8000000` (`uft_applesauce.h:52`, `UFT_AS_SAMPLE_CLOCK`) | 8 MHz — must agree with provider | PASS |
| Buffer standard | `163840` (`:94`, `AS_BUFFER_STANDARD`) | 160K standard Applesauce | PASS |
| Buffer plus | `430080` (`:95`, `AS_BUFFER_PLUS`) | 420K Applesauce+ | PASS |
| Serial baud | `115200` (`:224`, doc-comment) | 115200 | PASS |
| Serial framing | `8N1` documented (`:224`) | 8N1 | PASS |
| 18 command-string tokens (`sync:on`, `disk:read`, `disk:readx`, `head:track`, `head:zero`, `sync:?speed`, `?kind`, `psu:?5v`, ...) | present in doc-comments (`:14-54` etc.) | wiki.applesaucefdc.com grammar — **not vendored** | UNVERIFIED (×18) -> AS-D1-1 |

**Reference provenance: `mixed`** —
- Sample clock, response chars, serial params, buffer sizes:
  **`recalled`** (the 8 MHz / 125 ns clock, the single-char status
  responses, 115200 8N1, and the 160K/420K buffer sizes are
  well-established figures, reproduced from the V2 provider's own audit
  doc-comment which summarises the deleted V1 implementation). Not
  vendored.
- Command-string vocabulary: **`needs-source`**. The exact Applesauce
  line-protocol grammar can only be established from
  wiki.applesaucefdc.com, which is not vendored. Worse, the command
  *strings themselves* appear in UFT only inside **doc-comments** —
  `git grep` finds no code that actually emits `"disk:read\n"` or
  `"head:track N\n"`; the dialogue is entirely inside an injected
  `IApplesauceTransport` / runner with no production construction site.
  The 18 command_vocab rows are UNVERIFIED.

**Findings:**
- **AS-D1-1** (high): the 18 Applesauce command strings are
  **doc-comment only** — no code in `applesauce_provider_v2.cpp` builds
  or sends them. Per Hard rule #4 a doc-comment is not proof the
  protocol is implemented. The protocol grammar is also not vendored.
  Both halves of D1's evidence are absent: no UFT-side emission code,
  no vendored reference. The non-command constants (status chars, clock,
  buffers) are PASS at recalled grade.
- **AS-D1-2** (low): the response-char set the provider *uses* is only
  `'.'`, `'!'`, `'+'` (3 of the 6 the header doc-comment lists at
  `:11-16` — `'?'`, `'-'`, `'v'` are documented but not given named
  constants). Not a defect — the provider only branches on the 3 it
  needs — but the header doc-comment over-states the implemented set.

**Status: PARTIAL** — the constant layer (status chars, sample clock,
serial params, buffers) is PASS at `recalled` grade; but the
*command-protocol* layer that D1 is largely about is doc-comment-only
with no vendored reference (AS-D1-1). Net: PARTIAL, not PASS.

---

## D2 — Datapath

**Trace (read path):**
`ApplesauceProviderV2::do_read_raw_flux(ReadFluxParams)` (`applesauce_provider_v2.cpp:397`)
-> null-runner guard -> `null_runner_error("flux read")` if `!m_read_runner` (`:399-401`)
-> geometry validation `cylinder in [0, m_max_cylinder]`, `head in [0,1]` (`:404-409`)
-> build `ReadRequest{cylinder, head, revolutions, max_retries=5}` (`:412-416`)
-> `m_read_runner(req)` — the injected lambda (`sync:on` -> `disk:read`/`disk:readx` -> `data:?size` -> `data:<N` binary download -> `sync:off`) — returns `ApplesauceReadResult` (`:418`)
-> branch: `transport_unavailable || device_error || buffer_overflow` -> `translate_read_failure()` (`:421-429`); `flux_bytes.empty()` -> `FluxUnreadable` (`:432-443`); non-empty `error_message` + non-empty data -> `FluxMarginal` (`:457-467`); else -> `FluxCaptured` (`:470-476`).

**Conversions named:**
- `ApplesauceReadResult::flux_bytes` (raw LE32 8 MHz ticks) ->
  `std::vector<uint32_t> transitions_ns` via `bytes_to_transitions_ns()`
  (`:171-191`): each 4-byte LE value is `memcpy`'d to a `uint32_t` tick
  count, then `ticks * m_sample_ns + 0.5` rounded to ns. **Every**
  transition converted — `reserve(n_transitions)`, no decimation
  (`:178-188`). Rule F-3 verbatim.
- `revolutions` -> `FluxCaptured::revolutions` (`:473`).
- `m_sample_ns` (125.0 for 8 MHz) -> `FluxCaptured::sample_ns` (`:474`).
- Write path: `FluxStream::transitions_ns` -> LE32 tick bytes via
  `transitions_ns_to_bytes()` (`:193-210`): `ns / m_sample_ns + 0.5`
  rounded to ticks, packed little-endian.

**Sink — where does `FluxCaptured` go?** **Nowhere in production.** The
V2 provider compiles into `UnifiedFloppyTool.pro` (`:371`) but
`HardwareTab` does not route to it (D3), and no production code
constructs an `ApplesauceProviderV2` with real runners — only
`tests/test_applesauce_provider_v2.cpp`. The flux→ns conversion is
correct and lossless in isolation, but it has no consumer.

**Finding AS-D2-1** (low): `bytes_to_transitions_ns` does
`flux_bytes.size() / 4` (`:178`) — if the device returns a byte count
not divisible by 4, the trailing 1-3 bytes are **silently dropped**.
For an 8 MHz tick stream this should never happen, but a truncated
serial transfer would lose its tail without a diagnostic. Minor F-3
edge case.

**Finding AS-D2-2** (low): the round-trip `ns -> ticks -> ns` is
lossy by design (integer tick quantisation at 125 ns). `do_write_raw_flux`
converts the caller's ns stream back to ticks; a caller that read flux
*from a different device* (e.g. a Greaseweazle at 72 MHz, ~13.9 ns
resolution) and wrote it to Applesauce would have its timing quantised
to 125 ns. This is inherent to the hardware, not a bug — but it is an
un-flagged precision loss on a cross-device flux copy.

**Status: PARTIAL** — `do_read_raw_flux` / `do_write_raw_flux`
conversion is correct and (within tick resolution) lossless; but the
path has no production consumer, and there are two minor un-flagged
edge cases (AS-D2-1 tail truncation, AS-D2-2 cross-device quantisation).

---

## D3 — GUI-Integration (V2-adapted)

**Routing: FAIL.** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249`)
returns `::uft::hal::GreaseweazleProviderV2*` — a Greaseweazle-only
type. There is no `ApplesauceProviderV2` member on `HardwareTab`.
Selecting "Applesauce" in the controller combo (`hardwaretab.cpp:290`)
and pressing Connect reaches the P1.18 stub: `onConnect()` shows
`QMessageBox::information(... "Controller routing pending")` +
`updateStatus("Applesauce routing pending (P1.18)")`
(`hardwaretab.cpp:624-642`).

**Capability gating: N/A.** `forms/tab_hardware.actions.yaml` has
`provider_source: self->currentProviderV2()` and `extra_includes:` lists
only `greaseweazle_provider_v2.h`. Every `wire_action<cap::X>` is bound
to `GreaseweazleProviderV2`, never `ApplesauceProviderV2` — even though
Applesauce satisfies *more* capabilities than Greaseweazle (it adds
`MeasuresRPM` and `Recalibrates` to the same flux+motor+seek set, and
unlike GW it is `FullDriveControl`).

| Button | Capability tag | `ApplesauceProviderV2` satisfies? | Wired |
|--------|----------------|-----------------------------------|-------|
| `btnReadTest` | `ReadsRawFlux` | yes (`static_assert :611`) | no — not routed |
| (write flux) | `WritesRawFlux` | yes (`static_assert :614`) | no — not routed |
| `btnMotorOn/Off` | `ControlsMotor` | yes (`static_assert :617`) | no — not routed |
| `btnSeekTest` | `SeeksHead` | yes (`static_assert :620`) | no — not routed |
| `btnCalibrate` | `Recalibrates` | yes (`static_assert :623`) | no — not routed |
| `btnRPMTest` | `MeasuresRPM` | yes (`static_assert :626`) | no — not routed |
| `btnDetect` | `DetectsDrive` | yes (`static_assert :629`) | no — not routed |
| (sector ops) | `ReadsSectors`/`WritesSectors` | no — negative `static_assert :634,639` | correctly absent |

**Status: FAIL** — Applesauce is one of the 8 unrouted providers. The
7-capability declaration is sound and the `wire_action` template would
gate all 7 buttons enabled if it were routed — but `currentProviderV2()`
is hardcoded to the GW type, so none of it is reachable.

---

## D4 — OS-Detection

The Applesauce is a USB-CDC serial device. The V2 provider cites USB
VID/PID `0x16C0:0x0483` (manufacturer "Evolution Interactive") in its
error strings (`applesauce_provider_v2.cpp:143`, `:229`, `:844`,
`:867`).

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | The intended path is Qt `QSerialPortInfo::availablePorts()` wrapped by an `IApplesauceTransport` over `QSerialPort` (`applesauce_provider_v2.h:81`). **But** no production transport/runner is constructed. The C-HAL `uft_as_detect(char ports[][64], int)` is an honest M3.3 stub returning `UFT_ERR_NOT_IMPLEMENTED` (`uft_applesauce.c:213-232`). `hardwaretab.cpp::detectSerialPorts()` *does* enumerate serial ports generically and *does* have known-VID/PID hints — but there is **no `0x16C0:0x0483` Applesauce hint** in that list (`hardwaretab.cpp:447-455` knows Greaseweazle, SCP, KryoFlux, ZoomFloppy — not Applesauce). | generic serial enum present; no Applesauce VID/PID hint; no production transport | **PARTIAL** |
| **Linux** | same — generic `QSerialPortInfo` would list `/dev/ttyACM*`; no Applesauce-specific hint; C-HAL `uft_as_detect` stubbed. | same | **PARTIAL** |
| **macOS** | same — `QSerialPortInfo` lists `/dev/cu.usbmodem*`; no Applesauce hint; C-HAL stubbed. (No `.dylib`-style problem here because the Applesauce path is Qt `QSerialPort`, not a `dlopen`'d library — unlike XUM1541.) | same | **PARTIAL** |

**Cross-check (VID/PID):** the provider's error strings consistently use
`0x16C0:0x0483`. `0x16C0` is the V-USB / objdev shared VID (same family
the FC5025 uses). The audit grades this **`recalled`** — it is the
commonly-cited Applesauce USB identity, but it is not vendored and is
not corroborated by any UFT enumeration code (the `0x16C0:0x0483`
literal appears only in human-readable `fix`/`scanned_for` strings,
never in a `vid == ... && pid == ...` test).

**Finding AS-D4-1** (medium): there is no Applesauce VID/PID hint in
`hardwaretab.cpp::detectSerialPorts()` (`:447-455`). An Applesauce
plugged in will appear in the generic serial-port combo with only its
OS description, never a "Applesauce" label — and no code probes a port
to confirm it is an Applesauce (the V1 `autoDetectDevice` "?kind" probe
was deleted with the V1 provider in P1.18).

**Finding AS-D4-2** (medium): no production `IApplesauceTransport` /
`ApplesauceDetectRunner` is constructed anywhere in `src/`. As with
FC5025 and XUM1541, the detect path is unreachable outside tests.

**Status: PARTIAL** — generic Qt serial enumeration exists and would
list an Applesauce on all three OSes (no platform-specific gap, unlike
XUM1541's macOS `.dylib` hole), but there is no Applesauce VID/PID hint
and no production transport/runner to actually probe or open the device.

---

## D5 — Integritaets-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| AS-D5-1 | **Honest scaffold — no fake success.** Every `do_*` (`do_read_raw_flux`, `do_write_raw_flux`, `do_set_motor`, `do_seek`, `do_recalibrate`, `do_measure_rpm`, `do_detect_drive`) guards its null runner with `null_runner_error()` -> 3-part `ProviderError` carrying an explicit `"M3.3 partial scaffold"` marker (`applesauce_provider_v2.cpp:128-147`, `399-401`, `502-504`, `576-578`, `641-643`, `705-707`, `756-758`, `830-832`). `transport_unavailable` from any runner -> `ProviderError` with `"(M3.3 pending)"` in the `what` string (`:217-234`, `:314-326`, `:582-592`, `666-674`, `711-719`, `762-770`, `836-848`). The provider **never** returns a `FluxCaptured` / `WriteCompleted` / `RpmMeasured` / `SeekArrived` it did not actually receive. | positive (P0-risk cleared) | `:128-147, 217-234, 582-592` |
| AS-D5-2 | C-HAL `uft_applesauce.c` honest M3.3 stubs return `UFT_ERR_NOT_IMPLEMENTED` (`uft_applesauce.c:133-272`). Like XUM1541, the stub error strings point callers at `ApplesauceHardwareProvider::autoDetectDevice` in `src/hardware_providers/` (`uft_applesauce.c:222-228`) — **that V1 provider was deleted in P1.18.** Stale advice pointing at a non-existent file. | medium (stale advice) | `uft_applesauce.c:222-228` |
| AS-D5-3 | `do_detect_drive` success path: when the runner did not report `tracks`/`heads`, defaults are substituted per drive-kind string — `"5.25"` -> 35/1, `"3.5"` -> 80/2, `"PC"` -> 80/2, unknown -> 35/1 (`applesauce_provider_v2.cpp:877-896`); `rpm_nominal` defaults to `300.0` if the runner's `rpm <= 0` (`:899`). The defaults are only used when the runner is silent and the drive-kind string drives the choice, so less severe than FC-D5-3 — but it is still substituted geometry. The comment at `:821-825` is honest about the mapping. | low-medium | `:877-899` |
| AS-D5-4 | `bytes_to_transitions_ns` silently drops a non-multiple-of-4 tail (`flux_bytes.size() / 4`, `:178`). A truncated serial transfer loses its last 1-3 bytes with no diagnostic. | low | `:171-191` |
| AS-D5-5 | The 18 Applesauce command strings (`sync:on`, `disk:read`, `head:track N`, ...) exist **only in doc-comments** (`:14-54`, `.h:18-54`). No code emits them — the dialogue is in the absent runner. Per Hard rule #4 the doc-comment is not proof the protocol works. (Same class as XUM-D5-5 / FC-D5-5 but more extensive here — Applesauce has the richest documented command set of the three.) | low (documentation) | `.cpp:14-54`, `.h:18-93` |
| AS-D5-6 | `do_read_raw_flux` `FluxMarginal` branch is only taken when the runner returns *both* a non-empty `error_message` *and* non-empty `flux_bytes` (`:457`). The comment at `:454-456` admits "Applesauce V1 does not explicitly report marginal captures in the protocol response" — so in practice a real Applesauce device can never produce `FluxMarginal` through this path; it is dead unless a runner synthesises the dual signal. Not dishonest (it does not fabricate), but the marginal-capture forensic path is effectively unreachable for this device. | low | `:453-467` |

**Positives (forensic-integrity holds):**
- No swallowed errors — every result struct's failure flag
  (`transport_unavailable`, `device_error`, `buffer_overflow`,
  `write_protected`, `non_numeric_response`, empty `flux_bytes`) is
  checked and mapped to a typed outcome.
- Rule F-4: every `ProviderError` builds non-empty what/why/fix; M3.3
  markers name the exact follow-up (connect device + open serial).
- Rule F-3 (verbatim copy): `bytes_to_transitions_ns` converts every
  transition, `reserve()`d, no decimation (`:178-188`).
- Write-protect is type-safe: `disk:?write` `'+'` -> `WriteRefused`
  (`:301-311`).
- `do_recalibrate` correctly maps Applesauce `head:zero` (a true seek to
  the track-0 sensor) to `SeekArrived{cylinder=0}` — and the comment at
  `:697-700` explicitly contrasts this with XUM1541's `"I"` command
  which only reaches track 18, justifying why XUM1541 omits
  `Recalibrates` while Applesauce keeps it. Capability honesty.
- Null-runner construction is honest for all 7 capabilities — never a
  fake success (AS-D5-1).

**Status: PASS with findings** — no critical (P0) integrity violation.
**AS-D5-1 is the headline: all 7 capability paths are honest scaffolds,
never fake success.** AS-D5-2 (stale advice) and AS-D5-3 (substituted
geometry) are worth fixing.

---

## Fixes (prioritised)

**P0:** none. (The M3.3-scaffold P0-risk is **cleared** — all 7 `do_*`
paths return honest `ProviderError`s, not fake success.)

**P1:**
- **AS-D3 / AS-D4-2** — route `ApplesauceProviderV2` through `HardwareTab`
  and construct a production `IApplesauceTransport` (over `QSerialPort`)
  + the 7 runner lambdas. Applesauce is the widest-capability provider;
  leaving it unrouted wastes the most surface. *(blocks: P1.18 routing
  generalisation + M3.3 serial wiring)*

**P2:**
- **AS-D4-1** — add an Applesauce VID/PID hint (`0x16C0:0x0483`) to
  `hardwaretab.cpp::detectSerialPorts()` so a plugged-in Applesauce is
  labelled, and restore a "?kind" probe in the production detect runner.
- **AS-D5-2** — update the `uft_applesauce.c` stub error strings: they
  point at `ApplesauceHardwareProvider` in `src/hardware_providers/`,
  deleted in P1.18.
- **AS-D1-1** — vendor a snapshot of the wiki.applesaucefdc.com protocol
  page and add the command-string grammar as a real reference so the 18
  UNVERIFIED command_vocab rows become diff-able.

**P3:**
- **AS-D5-3** — `do_detect_drive` should prefer runner-reported geometry
  and flag substituted defaults as "assumed", not silent.
- **AS-D5-4** — `bytes_to_transitions_ns` should diagnose (not silently
  drop) a non-multiple-of-4 byte count.
- **AS-D5-5** — annotate the command-string doc-comments as
  "DESCRIBES INTENDED RUNNER — not implemented in this file".
- **AS-D5-6** — either wire a real marginal-capture signal or document
  that Applesauce cannot produce `FluxMarginal`.

---

## Reproduce

```bash
cd audit/applesauce
python extract_uft.py        # UFT-side constants (JSON)
python extract_ref.py        # official reference + provenance (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
"C:/Qt/Tools/mingw1310_64/bin/gcc.exe" -std=c11 -I../../include \
    -fsyntax-only test_applesauce_vectors.c   # D1 build-gate (C-HAL clock+enum layer)
```

Source reads behind this report:
```bash
git grep -nE "RESP_|AS_SAMPLE_|AS_BUFFER_" src/hardware_providers/applesauce_provider_v2.cpp
sed -n '397,477p' src/hardware_providers/applesauce_provider_v2.cpp   # do_read_raw_flux
sed -n '171,210p' src/hardware_providers/applesauce_provider_v2.cpp   # bytes_to_transitions_ns
sed -n '828,919p' src/hardware_providers/applesauce_provider_v2.cpp   # do_detect_drive
sed -n '133,272p' src/hal/uft_applesauce.c                            # honest M3.3 stubs
sed -n '249p;290p;447,455p;624,645p' src/hardwaretab.cpp              # routing + serial hints
git grep -nE "ApplesauceProviderV2" src/                              # construction sites (none)
```

## Not reviewed
- The Applesauce line-protocol command grammar — the 18 command strings
  are doc-comment-only in UFT and the grammar is not vendored (AS-D1-1).
- The binary-bulk transfer framing of `data:<N` / `data:>N` — no code in
  the provider implements it; it lives in the absent `IApplesauceTransport`.
- Runtime behaviour against a real Applesauce — HIL (`tests/hil/`, P3.4).
- `tests/test_applesauce_provider_v2.cpp` / `tests/test_applesauce_hal.c`
  internals — reviewed only enough to confirm the provider's sole
  construction site is the test, and that the C-HAL stub-honesty tests exist.

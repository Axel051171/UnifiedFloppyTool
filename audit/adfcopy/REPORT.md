# ADF-Copy

**Verdict:** PARTIAL
**LOC:** 681 (`adfcopy_provider_v2.cpp` 665 + `.h` 518 — combined 1183) | **Integration:** Qt-only, runner-injection (no C-HAL backend)

ADF-Copy ("ADFCopy" / "ADF-Drive") is a DIY open-source Teensy-based USB-CDC
serial controller for Amiga 3.5" floppy drives. It has **no official vendor
spec** (SpecStatus `CommunityConsensus`) and **no C-HAL backend** — unlike
Greaseweazle (`uft_gw_*`) or USB-Floppy (`ufi.h`). The V2 provider is a pure
Qt-side runner-injection design: every hardware operation is delegated to an
injected `std::function` runner that, in production, wraps an
`IADFCopyTransport` over `QSerialPort`.

Audited files:
- `src/hardware_providers/adfcopy_provider_v2.{h,cpp}` — V2 provider
- `src/hardwaretab.cpp` — GUI routing
- `include/uft/hardwareprovider.h` — checked re: the V1-header question (D5)

---

## D1 — Wire-Protocol

Constants diff: `python diff.py` → `evidence.json`. **8 PASS / 0 FAIL / 5 UNVERIFIED.**

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| `RESP_OK` / `RESP_ERROR` / `RESP_NODISK` | `'O'`/`'E'`/`'D'` = `0x4F`/`0x45`/`0x44` (`adfcopy_provider_v2.cpp:74-76`) | ASCII status bytes — tautological | PASS (3) |
| `ADFC_SAMPLE_CLOCK_HZ` / `ADFC_SAMPLE_NS` | `40000000.0` / `25.0` (`:78-79`) | Amiga 40 MHz / 25 ns standard | PASS (2) |
| `ADFC_STD_CYLINDERS` / `ADFC_HEADS` | `80` / `2` (`:82-83`) | Amiga 80 cyl × 2 head | PASS (2) |
| `ADFC_VID_PID` | `0x16C0:0x0483` (prose, `:124`) | generic PJRC Teensy ID | PASS |
| `ADFC_CMD_PING/INIT/SEEK/READ_FLUX/GET_STATUS` | `0x00`/`0x01`/`0x02`/`0x06`/`0x0B` (doc comments only — `.h:16,22,27,35`, `.cpp:130`) | numbers match recalled values | **UNVERIFIED** (5) → ADFC-D1-1 |

**Reference provenance: `needs-source` (mixed).** RESP_*, sample-clock and
geometry are `recalled`-grade (Amiga-floppy facts, byte-exact tautologies).
The `ADFC_CMD_*` opcodes are `needs-source`: their numeric values survive
**only as text comments / `static_assert` strings** inside the V2 provider.
The authoritative header that physically held the `#define`s —
`adfcopyhardwareprovider.h` — was **deleted** with the V1 hierarchy
(MF-169 / P1.17). No vendored firmware copy exists in the repo. `diff.py`
therefore reports those 5 rows UNVERIFIED even though the numbers match
UFT's own recollection.

**Findings:**
- **ADFC-D1-1** (medium): `ADFC_CMD_*` opcodes have no surviving definition
  site — `extract_uft.py` scrapes them from comments. There is no `#define`,
  no enum, no `static constexpr` for them anywhere in the tree. A typo in a
  comment would be invisible; worse, the runner that *actually builds the
  command byte* lives in production glue code not audited here, so the audit
  cannot prove the runner sends `0x06` for READ_FLUX rather than e.g. `0x07`.

**Status: PASS** (the 8 byte-exact-verifiable constants all match; the 5
UNVERIFIED opcodes are a provenance gap, not a mismatch — no FAIL).

---

## D2 — Datapath

**Trace (read path):**
`ADFCopyProviderV2::do_read_raw_flux(ReadFluxParams)` (`adfcopy_provider_v2.cpp:284`)
→ geometry validation against `m_max_cylinder` (`:291-296`)
→ builds `ReadRequest{cylinder, head, revolutions}` (`:302-305`)
→ `m_read_runner(req)` → `ADFCopyReadResult{flux_bytes[], revolutions, …}`
→ **conversion:** `bytes_to_transitions_ns()` (`:158-179`) — each 2-byte
  **big-endian** value is one flux transition in 40 MHz ticks; `ns = ticks *
  25.0 + 0.5` rounded (`:174`)
→ `FluxCaptured{position, transitions_ns[], revolutions, sample_ns=25.0}`
  (`:357-363`).

**Conversions named:** BE16 ticks → ns (`ticks * m_sample_ns`),
`flux_bytes.size()/2` → transition count, `sample_clock_hz` → `sample_ns`
(`1e9/freq`). Rule F-3 upheld: `bytes_to_transitions_ns` converts the entire
buffer verbatim — no decimation/averaging (`:163-176`); empty capture →
`FluxUnreadable` not silent success (`:319-331`); runner reporting partial
success (non-empty error + non-empty flux) → `FluxMarginal` (`:344-354`).

**Sink — where does `FluxCaptured` go?** **Nowhere reachable.** The provider
is not routed by `HardwareTab` (see D3), so no GUI consumer ever calls
`do_read_raw_flux`. The only callers are `tests/test_adfcopy_provider_v2.cpp`
(scripted-lambda runners). Unlike Greaseweazle there is **no** `raw_handle()`
escape hatch and **no** legacy `FluxCaptureJob` path either — ADF-Copy's V1
provider was Qt-only and the V1 hierarchy is deleted.

**Finding ADFC-D2-1** (medium): the V2 datapath is correct and forensically
faithful in isolation, but it is a **dead-end at both ends** — no production
runner is constructed anywhere in the shipping codebase, and no GUI consumer
is wired. `do_read_raw_flux` produces a correct `FluxCaptured`; nothing in
the app calls it. This is honest scaffolding (it does not fake success), but
it is not a functioning capture path.

**Status: PARTIAL** — conversion logic correct and F-3-compliant; the path
has no production caller and no GUI sink.

---

## D3 — GUI-Integration (V2-adapted)

**Routing:** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249-257`)
is hardcoded to return `m_gwProviderV2.get()` — a `GreaseweazleProviderV2*`.
ADF-Copy **is** in the controller combo: `populateControllerList()` adds
`tr("ADF-Copy (Amiga)")` with data key `"adfcopy"` (`hardwaretab.cpp:285`).
But `onConnect()` (`:624-645`) takes the `controller != "greaseweazle"`
branch for `"adfcopy"` and shows the **"Controller routing pending"**
messagebox, then `updateStatus("… routing pending (P1.18)", isError=true)`
and returns. No `ADFCopyProviderV2` instance is ever constructed by the GUI.

**Capability → button matrix** (codegen `wire_action<cap::X>`):

| Button | Capability tag | `ADFCopyProviderV2` satisfies? | Wired for ADF-Copy |
|--------|----------------|-------------------------------|--------------------|
| `btnReadTest` | `ReadsRawFlux` | yes (`adfcopy_provider_v2.h:457`) | ✗ — `currentProviderV2()` returns nullptr/GW, not this |
| `btnMotorOn/Off` | `ControlsMotor` | yes (`:461`) | ✗ |
| `btnSeekTest` | `SeeksHead` | yes (`:465`) | ✗ |
| `btnCalibrate` | `Recalibrates` | yes (`:469`) | ✗ |
| `btnDetect` | `DetectsDrive` | yes (`:473`) | ✗ |
| (write flux) | `WritesRawFlux` | **no** — negative `static_assert` `:479` | correctly absent |
| (sector ops) | `ReadsSectors`/`WritesSectors` | **no** — negative `static_assert` `:483,488` | correctly absent |
| `btnRPMTest` | `MeasuresRPM` | **no** — negative `static_assert` `:493` (V1 `measureRPM()` was a 300.0 stub) | correctly absent |

The capability *composition* is honest — 5 positive + 4 negative
`static_assert`s, all correctly reflecting what the hardware can do
(WritesRawFlux/MeasuresRPM correctly omitted because the V1 audit found
them to be stubs). But because the provider is **not routed**, the codegen
`wire_hardware_tab()` only ever sees `currentProviderV2()` as a
`GreaseweazleProviderV2*`; the ADF-Copy capability set never reaches the
wiring machinery. When ADF-Copy is selected, the buttons reflect whatever
GW state happens to be active, not ADF-Copy.

**Status: FAIL** — ADF-Copy is selectable in the combo but `onConnect()`
dead-ends in the "routing pending (P1.18)" messagebox. The provider type is
fully conformance-tested but has zero GUI reachability. (This is a known,
honestly-stated P1.18 gap, shared by all 8 non-GW providers — not an
ADF-Copy-specific defect, but for THIS provider's D3 the grade is FAIL.)

---

## D4 — OS-Detection

ADF-Copy enumerates as a **USB-CDC serial port** (PJRC Teensy, VID:PID
`0x16C0:0x0483`). There is **no ADF-Copy-specific enumeration code anywhere**
— no C-HAL, and `hardwaretab.cpp::detectSerialPorts()` (`:426-520`) has
VID/PID hint branches only for Greaseweazle (`0x1209/0x4D69`), SuperCard Pro,
KryoFlux and ZoomFloppy/XUM1541 (`:447-455`). There is **no `0x16C0/0x0483`
branch**.

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | Qt `QSerialPortInfo` lists the port; or registry `SERIALCOMM` fallback. No VID/PID hint → shows as a generic "USB Serial" / `COMx` entry. | `hardwaretab.cpp:430-512` — no `0x16C0` branch | **PARTIAL** (port appears, not identified as ADF-Copy) |
| **Linux** | Qt `QSerialPortInfo` lists `/dev/ttyACM*`. No VID/PID hint. | same | **PARTIAL** |
| **macOS** | Qt `QSerialPortInfo` lists `/dev/cu.usbmodem*`. No VID/PID hint. | same | **PARTIAL** |

The port is *reachable* on every OS via the Qt serial enumeration (ADF-Copy
is a standard USB-CDC device — no special driver), so detection is not
broken. But it is never **identified** as an ADF-Copy: the user must know
which `COMx` / `/dev/ttyACM*` is their device. And since `onConnect()`
dead-ends for `"adfcopy"` (D3), the identification gap is currently moot.

**Finding ADFC-D4-1** (low): no `0x16C0:0x0483` VID/PID hint in
`detectSerialPorts()`. Cross-check: a real ADF-Copy/ADF-Drive enumerates
with the generic PJRC Teensy ID — see ADFC-D5-2 (MF-146).

**Status: PARTIAL** — port enumeration works on all three OSes via the
generic Qt serial path; no device identification, no dedicated code.

---

## D5 — Integritäts-Befunde

**The V1-header question (brief item D5):** *Does `adfcopy_provider_v2`
actually USE the V1 class `HardwareProvider` from
`include/uft/hardwareprovider.h:98`, or is it a stale leftover include?*

**Answer: NEITHER — the premise is wrong.** `adfcopy_provider_v2.h` does
**not** `#include "uft/hardwareprovider.h"` at all. Its includes are exactly
`<cstdint> <functional> <memory> <string> <vector>` plus
`uft/hal/mixins.h`, `uft/hal/outcomes.h`, `uft/hal/concepts.h`
(`adfcopy_provider_v2.h:110-118`). A repo-wide grep
(`#include.*hardwareprovider` across `src/`) returns **zero matches** — no V2
provider includes that header. The only occurrences of the string
"adfcopyhardwareprovider.h" in the ADF-Copy V2 files are **doc-comment
citations** (`adfcopy_provider_v2.h:70`, `adfcopy_provider_v2.cpp:71`) of the
deleted V1 header, used to explain where the protocol constants *came from*.
`ADFCopyProviderV2 final` is a pure mixin composition
(`Identity` + `ReadsRawFluxVia` + `ControlsMotorVia` + `SeeksHeadVia` +
`RecalibratesVia` + `DetectsDriveVia`, `:306-313`) — it does not inherit,
include, or reference the V1 `HardwareProvider` base class in any way. The
brief's claim that adfcopy is "the only V2 provider that #includes that
header" is **not borne out by the code** — no stale include exists to remove.

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| ADFC-D5-1 | No production runner factory. `ADFCopyProviderV2`'s constructor takes 5 `std::function` runners; the only code that ever constructs the provider is `tests/test_adfcopy_provider_v2.cpp` with scripted lambdas. There is no `IADFCopyTransport`-over-`QSerialPort` implementation in the shipping tree — the production half of the runner-injection design does not exist yet. Honest (the provider returns `ProviderError` on null runners, `:113-133`), but the backend is a scaffold. | medium | `adfcopy_provider_v2.h:390-396`, `:135` |
| ADFC-D5-2 | **MF-146 VID/PID collision.** ADF-Copy's `0x16C0:0x0483` is the *generic* PJRC Teensy USB ID — **shared with Applesauce** (also a Teensy-class device). Two providers cannot be told apart by VID/PID alone. `detectSerialPorts()` has no branch for either, so the collision is currently latent — but any future "auto-pick the ADF-Copy port" logic keyed on VID/PID would mis-identify an Applesauce as an ADF-Copy and vice versa. Disambiguation must be by the `ADFC_CMD_PING` version string ("ADF-Copy"/"ADF-Drive") vs Applesauce's text-protocol banner — i.e. a protocol probe, not an enumeration ID. | medium | ref: `extract_ref.py` USB_IDS note; no disambiguation code exists |
| ADFC-D5-3 | Motor-off is a silent best-effort no-op. `do_set_motor(false)` returns `MotorStopped{}` on `result.success` even though the ADF-Copy protocol has **no motor-off command** — the V1 `setMotor(false)` only set a flag. The header documents this (`:218`, `.cpp:378-380`), and the runner is the one that decides `success`, so it is not strictly a fake inside the provider — but a "motor stopped" outcome is reported for an operation that sends nothing on the wire. | low (documented) | `adfcopy_provider_v2.cpp:426-432` |

**Positives (forensic-integrity holds):**
- No swallowed errors — every `ADFCopy*Result` failure flag
  (`transport_unavailable`, `device_error`, `no_disk`) is checked and mapped
  to a typed outcome (`translate_read_failure`, `:182-265`).
- Rule F-4: every `ProviderError` carries non-empty what/why/fix; the
  strings are unusually detailed (protocol context, VID/PID, recovery steps).
- Rule F-3: `bytes_to_transitions_ns` converts the whole buffer verbatim;
  multi-revolution data is preserved; partial success → `FluxMarginal`.
- No fake success — null runner → `ProviderError`; empty capture →
  `FluxUnreadable`. An honest partial scaffold.
- No stub façade in the *provider*: every `do_*` has a runner call behind
  it. The scaffold-ness is at the runner-factory layer (ADFC-D5-1), not in
  the provider's outcome logic.

**Status: PASS with findings** — no P0 integrity violation. The provider
itself is forensically honest; the gaps (no production runner, MF-146
collision) are scaffold/ecosystem issues, not lies in the code.

---

## Fixes (prioritised)

**P0:** none.

**P1:**
- **ADFC-D3 / ADFC-D2-1** — route `HardwareTab` to `ADFCopyProviderV2`
  (task P1.18). Until `currentProviderV2()` can return a non-GW provider and
  `onConnect()` constructs an `ADFCopyProviderV2` with a real
  `IADFCopyTransport`-over-`QSerialPort` runner, the entire provider is
  GUI-unreachable. *(blocks: P1.18 in REFACTOR_TASKS.md)*
- **ADFC-D5-1** — implement the production runner factory: an
  `IADFCopyTransport` backed by `QSerialPort` (115200 8N1 USB-CDC) plus the
  five runner lambdas. The provider is conformance-tested but has no
  production backend.

**P2:**
- **ADFC-D5-2** — design ADF-Copy ↔ Applesauce disambiguation (MF-146):
  both share PJRC Teensy `0x16C0:0x0483`. Resolve by `ADFC_CMD_PING`
  version-string probe, not by VID/PID. Document the shared-ID hazard before
  any auto-detect logic is written.
- **ADFC-D1-1** — vendor a copy of the ADF-Copy firmware protocol header so
  `ADFC_CMD_*` opcodes have an authoritative definition site again
  (currently comment-only since MF-169 deleted `adfcopyhardwareprovider.h`).
  Upgrades the 5 UNVERIFIED D1 rows toward `vendored`.

**P3:**
- **ADFC-D4-1** — add a `0x16C0:0x0483` VID/PID hint to
  `detectSerialPorts()` so the port is *labelled* (with the MF-146 caveat:
  the label can only say "PJRC Teensy (ADF-Copy or Applesauce)").
- **ADFC-D5-3** — consider returning a distinct outcome (or a noted
  `MotorStopped`) for the motor-off no-op so the audit trail records that no
  wire command was sent.

---

## Reproduce

```bash
cd audit/adfcopy
python extract_uft.py        # UFT-side constants (JSON)
python extract_ref.py        # official reference (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
python mock_adfcopy.py       # protocol-framing model self-test (no native vectors —
                             # ADF-Copy has no C-HAL header to static_assert against)
```

Source reads behind this report:
```bash
grep -nE "RESP_|ADFC_SAMPLE|ADFC_STD|ADFC_CMD_" src/hardware_providers/adfcopy_provider_v2.cpp
grep -rn "#include.*hardwareprovider" src/        # → zero matches (D5 V1-header question)
sed -n '284,364p' src/hardware_providers/adfcopy_provider_v2.cpp   # do_read_raw_flux
sed -n '280,316p;624,645p' src/hardwaretab.cpp                     # combo + routing dead-end
```

## Not reviewed
- The production `IADFCopyTransport` / `QSerialPort` runner — does not exist
  in the tree yet (ADFC-D5-1); the byte-exact command framing the runner
  must emit is therefore unverified at the wire level.
- `tests/test_adfcopy_provider_v2.cpp` scripted-lambda coverage (the provider
  passes its conformance sections — that was taken as given, not re-audited).
- Runtime behaviour against real ADF-Copy / ADF-Drive hardware — HIL, P3.4.
- The deleted V1 `adfcopyhardwareprovider.cpp` (1448 LOC) — only its
  audit-summary doc-comment in the V2 header was read, not git history.

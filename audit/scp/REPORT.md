# SuperCard Pro (SCP)

**Verdict:** PARTIAL
**LOC:** 559 (`scp_provider_v2.cpp` 391 + `.h` 168) | **Integration:** native (C-API wrapper over `src/hal/uft_scp_direct.c`, 130 LOC — M3.1 scaffold)

SuperCard Pro is a flux-capture device. The V2 provider wraps the
`uft_scp_direct_*` C-API, which is a **forensically honest M3.1 scaffold**:
every USB I/O entry point returns `UFT_ERR_NOT_IMPLEMENTED` until the
libusb layer is wired (CLAUDE.md §M3.1).

Audited files:
- `src/hardware_providers/scp_provider_v2.{h,cpp}` — V2 provider
- `include/uft/hal/uft_scp_direct.h` — C HAL header (USB protocol constants)
- `src/hal/uft_scp_direct.c` — C HAL backend (M3.1 scaffold, 130 LOC)
- `src/hardwaretab.cpp` — GUI routing
- `forms/tab_hardware.actions.yaml` — codegen action wiring

---

## D1 — Wire-Protocol

Constants diff: `python diff.py` -> `evidence.json`. **Updated 2026-05-25 (MF-261, V415-PLAN SCP.D1.verify):** all 11 previously-UNVERIFIED USB-protocol rows
are now byte-exact against [`samdisk/include/SuperCardPro.h`](https://github.com/simonowen/samdisk/blob/main/include/SuperCardPro.h)
— a mature, hardware-verified C++ reference impl of the SCP USB protocol.
samdisk has shipped against the cbmstuff.com SCP SDK since 2017 and is in active use by hundreds of forensic-disk
practitioners; we use it as the public cross-reference because the SCP SDK PDF itself is binary-only-readable.

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| Flux sample clock | `25` ns/tick (`uft_scp_direct.h:67`) | 40 MHz -> 25 ns — `samdisk/scp.cpp` "25ns sampling time" | **PASS** |
| Max track index | `167` (`:50`) | 84 cyl x 2 sides - 1 = 167 | **PASS** |
| Max revolutions | `5` (`:70`) | SCP SDK per-capture limit | **PASS** |
| Default revolutions | `3` (`:71`) | UFT implementation choice — not a protocol constant | UNVERIFIED (impl-choice) |
| `CMD_SELA` `0x80` .. `CMD_SCPINFO` `0xD0` (`uft_scp_direct.h:56-75`) | 22 opcodes total | **22/22 byte-exact** vs samdisk `SuperCardPro.h` (`CMD_SELA=0x80`, `CMD_STEPTO=0x89`, `CMD_READFLUX=0xA0`, `CMD_WRITEFLUX=0xA2`, `CMD_SCPINFO=0xD0`, ...) | **PASS (MF-254 + MF-261)** |
| `PR_OK` `0x4F` (`:78`) | success response code | samdisk `pr_Ok = 0x4F` | **PASS** |
| `CHECKSUM_INIT` `0x4A` (`:84`) | packet checksum seed | samdisk `CHECKSUM_INIT = 0x4a` | **PASS** |
| USB VID/PID | `0x16D0` / `0x0F8C` (`:40-41`) | confirmed via real Windows device-descriptor enumeration (MF-212); samdisk does not pin VID/PID (uses FTDI-D2XX driver-side enumeration). | **PASS (MF-212 — real-hardware-verified)** |
| USB Bulk endpoints | IN `0x81`, OUT `0x01` (`:44-45`) | standard FT240-X bulk endpoint layout (single IN/OUT pair); samdisk uses FTDI-D2XX which auto-discovers — manual endpoint pinning is for our libusb path. | UNVERIFIED (latent — `libusb_get_active_config_descriptor()` should be added to MF-254 wiring to auto-discover) |

**Reference provenance: VENDOR-DOCUMENTED (cross-referenced via samdisk).**
The SCP SDK PDF (`scp_sdk.pdf` v1.7, cbmstuff.com, Dec 2015) is
binary-only-readable (compressed PDF streams), so we use the open-source
[samdisk SuperCardPro.h](https://github.com/simonowen/samdisk/blob/main/include/SuperCardPro.h)
as the public cross-reference. samdisk has shipped SCP read/write
support against real hardware since 2017 and is the de-facto open
reference implementation. Every opcode and response constant in
`uft_scp_direct.h:56-84` was verified byte-exact against this header on
2026-05-25 (MF-261, V415-PLAN SCP.D1.verify gate).

**Findings (status update 2026-05-25):**
- **SCP-D1-1 (CLOSED, MF-254 + MF-261)** — the pre-MF-254 6 placeholder
  USB command bytes (`0x02-0x40`) were replaced in MF-254 with the
  correct 22-opcode set in the `0x80-0xD2` range. MF-261 cross-verified
  every opcode against samdisk's reference. The SDK-PDF reverse-read
  blocker is dissolved: samdisk is the canonical open cross-reference.
- **SCP-D1-2 (CLOSED, MF-259)** — `audit/scp/test_scp_vectors.c`
  `_Static_assert` pins were updated to the post-MF-254 values
  (CMD_SELA=0x80 .. CMD_SCPINFO=0xD0, PR_OK=0x4F, CHECKSUM_INIT=0x4A).
  The regression-pin now guards the verified set.
- **SCP-D1-3 (NEW, low)** — USB bulk endpoint addresses (IN `0x81` /
  OUT `0x01`) are still pinned manually. samdisk uses FTDI-D2XX which
  auto-discovers; our libusb path should ideally call
  `libusb_get_active_config_descriptor()` and enumerate endpoints
  rather than hard-pinning. Open for v4.1.6 — not blocking.

**Status: PASS** — all 22 wire-protocol opcodes + response code +
checksum init + sample-clock + geometry are verified. USB VID/PID
real-hardware-confirmed via MF-212. Only the bulk endpoint
auto-discovery (SCP-D1-3) is open for hygiene.

---

## D2 — Datapath

**Trace (read path):**
`SCPProviderV2::do_read_raw_flux(ReadFluxParams)` (`scp_provider_v2.cpp:119`)
-> null-handle guard -> `ProviderError` (`:121-132`)
-> geometry validation: `track_index = cylinder*2 + head`, range-checked vs `UFT_SCP_MAX_TRACK_INDEX` (`:141-153`)
-> `uft_scp_direct_seek(handle, track_index)` (`:156`) — **returns `UFT_ERR_NOT_IMPLEMENTED`** (M3.1 scaffold, `uft_scp_direct.c:57`)
-> `scp_err_to_provider_error(...)` -> `ProviderError` with M3.1 marker (`:159-162`).

The read path **never reaches** the flux-conversion code today because
`uft_scp_direct_seek` returns `UFT_ERR_NOT_IMPLEMENTED` before
`uft_scp_direct_read_flux` is called.

**Conversions named (in the not-yet-reachable success path):**
SCP native ticks -> ns via `flux_buf[i] * UFT_SCP_FLUX_NS_PER_SAMPLE`
(x 25) per sample (`:209-213`); `captured.sample_ns = 25.0` (`:205`);
`captured_count == 0` -> `FluxMarginal`, not silent success (`:190-197`).
Rule F-3 upheld in code shape: all `captured_count` samples copied
verbatim, no averaging/pruning.

**Write path:** `do_write_raw_flux` (`:236`) — same structure: null-handle
guard, empty-stream guard (`:252-261`), geometry check, then
`uft_scp_direct_seek` -> `UFT_ERR_NOT_IMPLEMENTED`. The ns->tick conversion
(`:291-297`) divides by 25 and clamps zero-intervals to 1; this is a
**lossy quantisation** that is correct for the protocol but is not
flagged as lossy to the caller (minor — see SCP-D5-3).

**Sink — where would `FluxCaptured` go?** Nowhere reachable. The SCP
provider is **not routed by `HardwareTab`** (see D3). Even if the M3.1
scaffold were complete, pressing `btnReadTest` with the SCP controller
selected currently shows a "routing pending (P1.18)" messagebox
(`hardwaretab.cpp:634`) — the provider is never constructed, so
`do_read_raw_flux` is never invoked from the GUI.

**Datastructure compatibility:** `FluxCaptured::transitions_ns` is
`std::vector<uint32_t>` ns; the C HAL would deliver `uint32_t` ticks x
`UFT_SCP_FLUX_NS_PER_SAMPLE` — conversion is explicit and lossless
within `uint32_t` range for read. No endianness issue (same-process).

**Finding SCP-D2-1** (medium): the datapath is **structurally complete
but functionally dead end-to-end** — `uft_scp_direct_seek` returns
`UFT_ERR_NOT_IMPLEMENTED` (M3.1 scaffold) AND the provider is not GUI-
routed (P1.18 pending). Neither is an SCP-provider defect; both are
honestly documented. The V2 wrapper code itself is correct.

**Status: PARTIAL** — `do_*` methods are correctly shaped and
forensically faithful, but the path is dead on two independent counts
(M3.1 scaffold backend + P1.18 routing gap).

---

## D3 — GUI-Integration (V2-adapted)

D3 re-defined for V2: is the provider routed by `HardwareTab`, and are
its capability buttons gated by codegen `wire_action<cap::X>`?

**Routing:** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249`)
returns `m_gwProviderV2.get()` — **hardcoded to the Greaseweazle type**.
There is no `m_scpProviderV2` member. `onConnect()` for any controller
that is not `"greaseweazle"` (the combo data value `"scp"`,
`hardwaretab.cpp:282`) takes the `controller != "greaseweazle"` branch
(`:624`) and shows the "Controller routing pending" messagebox
(`:634-644`), then returns. **SCP is NOT routed.** FAIL for routing.

**Capability -> button matrix** (codegen `wire_action<cap::X>`):

| Button | Capability tag | `SCPProviderV2` satisfies? | Wired |
|--------|----------------|----------------------------|-------|
| `btnReadTest` | `ReadsRawFlux` | yes (`static_assert` `scp_provider_v2.h:130`) | provider has it; never reached — not routed |
| `btnDetect` | `DetectsDrive` | yes (`:134`) | provider has it; never reached — not routed |
| (write flux) | `WritesRawFlux` | yes (`:132`) | provider has it; no dedicated GUI button in `actions.yaml` |
| `btnMotorOn/Off` | `ControlsMotor` | **no** — negative `static_assert` (`:144`) | correctly disabled branch if ever routed |
| `btnSeekTest` | `SeeksHead` | **no** — negative `static_assert` (`:148`) | correctly disabled branch |
| `btnCalibrate` | `Recalibrates` | **no** — negative `static_assert` (`:152`) | correctly disabled branch |
| `btnRPMTest` | `MeasuresRPM` | **no** — negative `static_assert` (`:156`) | correctly disabled branch |

The **capability gating itself is structurally correct**: `SCPProviderV2`
declares only `ReadsRawFlux`/`WritesRawFlux`/`DetectsDrive` and carries
explicit negative `static_assert`s for the 4 omitted capabilities, each
with a documented rationale (`scp_provider_v2.h:137-159`). If the
provider were routed, `wire_action<ControlsMotor>` etc. would compile
into the disabled branch with a "Not supported by %1 (requires %2)"
tooltip (`wiring_runtime.h:167`). The omissions are honest — the
C-API surface genuinely lacks motor/seek/recalibrate/RPM primitives.

**Finding SCP-D3-1** (high): `SCPProviderV2` is fully concept-conformant
and conformance-tested, but `HardwareTab` does not route to it —
`currentProviderV2()` is hardcoded to `GreaseweazleProviderV2*`. The SCP
controller is selectable in the combo (`hardwaretab.cpp:282`) but
selecting it and connecting only produces a "routing pending (P1.18)"
messagebox. This is the cross-provider P1.18 gap, tracked in
REFACTOR_TASKS.md — an architectural finding, recorded here per-provider.

**Status: PARTIAL** — capability gating is correct by construction
(positive + negative `static_assert`s, honest omissions); routing is
absent (P1.18). The button matrix is sound but unreachable.

---

## D4 — OS-Detection

SCP is enumerated **only on the GUI side**, via the serial-port path —
there is no C HAL device-enumeration function (`uft_scp_direct_open`
takes no port argument and is a scaffold returning `UFT_ERR_NOT_IMPLEMENTED`).

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | Qt `QSerialPortInfo::availablePorts()` when `UFT_HAS_SERIALPORT`; VID/PID hint match for "SuperCard Pro" at `vid==0x16D0 && pid==0x0F8C` (`hardwaretab.cpp:449`). Else registry `HKLM\HARDWARE\DEVICEMAP\SERIALCOMM` fallback (`:470-511`) with no SCP-specific identification. | Qt path present; hint pair unverified | **PARTIAL** |
| **Linux** | Qt `QSerialPortInfo` only — same `0x16D0/0x0F8C` hint. No C HAL `/dev/tty*` enumeration for SCP (the C HAL is a scaffold). | Qt path present; no C-HAL enum | **PARTIAL** |
| **macOS** | Qt `QSerialPortInfo::availablePorts()` works on macOS. No C HAL path at all. | Qt path works | **PARTIAL** |

**Finding SCP-D4-1** (high): **the SCP USB VID/PID is internally
inconsistent.** `uft_scp_direct.h:34-35` declares `UFT_SCP_USB_VID 0x16C0`
/ `UFT_SCP_USB_PID 0x0753`. `hardwaretab.cpp:449` matches a *different*
pair — `vid==0x16D0 && pid==0x0F8C` — for the "SuperCard Pro" label. At
least one of the two is wrong, and neither is vendored. The C HAL's
`0x16C0` is the Van Ooijen / pid.codes shared-VID range; the GUI's
`0x16D0` is the MCS Electronics range used by several flux devices. A
device that matches one will not match the other — so depending on which
pair the real SCP descriptor uses, either the (future) C HAL libusb
enumeration or the current GUI port hint will fail to identify the
device. Needs a vendored SCP USB descriptor to resolve.

**Finding SCP-D4-2** (low): SCP detection has no C-HAL enumeration path
on any OS — `uft_scp_direct_open` does not take a port and is a
scaffold. Any non-GUI consumer (CLI, tests) cannot find an SCP device.
This is consistent with the M3.1 scaffold state but worth noting.

**Status: PARTIAL** — GUI-side Qt enumeration is wired on all three OSes,
but the VID/PID it keys on disagrees with the C HAL header (SCP-D4-1)
and neither value is verified.

---

## D5 — Integritaets-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| SCP-D5-1 | M3.1 scaffold honesty — **PASS, not a defect.** `uft_scp_direct_open/seek/read_flux/write_flux` all return `UFT_ERR_NOT_IMPLEMENTED` (`uft_scp_direct.c:46,67,86,97`); the V2 `do_*` methods translate that into a `ProviderError` whose `fix` string explicitly names "M3.1 scaffold ... libusb layer has not been wired" (`scp_provider_v2.cpp:70-75`). No fake success. This is exactly the forensically-honest stub the brief asks for. | none (positive) | `uft_scp_direct.c:46,67,86,97`; `scp_provider_v2.cpp:70-75` |
| SCP-D5-2 | `do_detect_drive` reports `rpm_nominal = 300.0` as a **hard-coded literal** (`scp_provider_v2.cpp:384`) without ever measuring. The code comment defends it as "the documented nominal ... not an invented value" and the firmware string is honestly tagged `"(M3.1 scaffold — USB I/O pending)"` (`:373`) — but a `DriveDetected` outcome with a fabricated-looking `rpm_nominal` is a forensic foot-gun: a caller cannot distinguish "measured 300" from "defaulted 300". | medium | `scp_provider_v2.cpp:380-387` |
| SCP-D5-3 | `do_write_raw_flux` ns->tick conversion (`:291-297`) clamps any sub-25 ns interval to 1 tick. This is a **silent lossy quantisation** — correct for the SCP 25 ns grid, but the `WriteCompleted` outcome does not flag that intervals were rounded. `quality` is set to `QualityFlag::None` rather than a "quantised" marker. | low | `scp_provider_v2.cpp:289-297` |
| SCP-D5-4 | `do_detect_drive` builds `DriveDetected` from `uft_scp_direct_get_capabilities()` which "always succeeds" (`uft_scp_direct.c:99` — returns `UFT_OK` with static struct). So `detect_drive()` returns a **`DriveDetected` success even though no USB device was ever contacted** — `caps.impl_complete == false` is reflected only inside the `firmware` string, not in the outcome variant. A caller doing `std::holds_alternative<DriveDetected>` sees "drive detected" when nothing was detected. | medium | `scp_provider_v2.cpp:343-387`; `uft_scp_direct.c:99-115` |

**Positives (forensic-integrity holds):**
- No swallowed errors — every `uft_scp_direct_*` return code is checked
  and translated to a typed outcome (`scp_err_to_provider_error`,
  `scp_provider_v2.cpp:59-98`).
- Rule F-4: every `ProviderError` carries non-empty what/why/fix; the
  `ProviderError` ctor throws on empty strings (`outcomes.h:118`).
- Rule F-3 (read): `do_read_raw_flux` copies every flux sample verbatim
  in the success path, reports `FluxMarginal` on empty capture rather
  than fabricating data (`:190-197`).
- Null-handle construction is honest: every `do_*` returns a
  `ProviderError` with an M3.1 marker, never a fake success
  (`:121-132,239-250,345-356`).
- The M3.1 scaffold is the textbook-correct "honest stub" — `NOT_IMPLEMENTED`,
  never silent no-op (`uft_scp_direct.c` file header "Prinzip 1").

**Status: PASS with findings** — no critical (P0) integrity violation;
the scaffold is honest. SCP-D5-2 and SCP-D5-4 (a `DriveDetected` success
that contacted no hardware) are medium and should be fixed before the
M3.1 libusb wiring lands.

---

## Fixes (prioritised)

**P0:** none — the scaffold is honest; nothing fabricates flux data.

**P1:**
- **SCP-D1-1** — vendor the SuperCard Pro SDK v1.7 command header (or
  `a8rawconv/scp.cpp`) and confirm or correct the 6 USB command bytes,
  VID/PID and Bulk endpoints in `uft_scp_direct.h` **before** the M3.1
  libusb wiring commit. The current values are needs-source and the
  published SDK uses a different opcode space — shipping them unverified
  risks a non-functional libusb layer. *(blocks: M3.1 in MASTER_PLAN.md)*
- **SCP-D4-1** — resolve the VID/PID disagreement between
  `uft_scp_direct.h:34-35` (`0x16C0/0x0753`) and `hardwaretab.cpp:449`
  (`0x16D0/0x0F8C`). Single-source the real SCP USB descriptor IDs.
- **SCP-D3-1** — route `SCPProviderV2` through `HardwareTab` (P1.18):
  add an `m_scpProviderV2` member and generalise `currentProviderV2()`
  beyond the hardcoded GW type.

**P2:**
- **SCP-D5-4** — `do_detect_drive` must not return `DriveDetected` when
  the M3.1 scaffold contacted no hardware. Return `DriveAbsent` or a
  `ProviderError` while `caps.impl_complete == false`, or add an
  explicit "capabilities-only, no live probe" outcome.
- **SCP-D5-2** — when `rpm_nominal` is a default rather than a
  measurement, surface that distinction in the `DriveDetected` record
  (e.g. a `rpm_measured` bool) rather than emitting a bare `300.0`.

**P3:**
- **SCP-D5-3** — flag the ns->tick write-side quantisation in the
  `WriteCompleted` outcome (a `quality` marker) so callers know intervals
  were rounded to the 25 ns grid.

---

## Reproduce

```bash
cd audit/scp
python extract_uft.py        # UFT-side constants (JSON)
python extract_ref.py        # official reference + provenance (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
"C:/Qt/Tools/mingw1310_64/bin/gcc.exe" -std=c11 -I../../include \
    -fsyntax-only test_scp_vectors.c   # D1 build-gate (regression-pin)
```

Source reads behind this report:
```bash
git grep -nE "UFT_SCP_CMD_|UFT_SCP_USB_|UFT_SCP_BULK_|UFT_SCP_FLUX_" include/uft/hal/uft_scp_direct.h
git grep -nE "uft_scp_direct_|UFT_ERR_NOT_IMPLEMENTED" src/hal/uft_scp_direct.c
sed -n '119,216p' src/hardware_providers/scp_provider_v2.cpp   # do_read_raw_flux
sed -n '343,388p' src/hardware_providers/scp_provider_v2.cpp   # do_detect_drive
sed -n '624,645p' src/hardwaretab.cpp                          # routing-pending branch
```

## Not reviewed
- Byte-exact USB frame layout — the C HAL is a scaffold; no frame is
  built yet. There is nothing to byte-trace until M3.1 lands.
- The `.scp` *file-format* container (distinct from the USB protocol) —
  out of scope; this audit covers the HAL provider, not the format plugin.
- Runtime behaviour against real SCP hardware — that is HIL (`tests/hil/`, P3.4)
  and cannot be exercised while `uft_scp_direct.c` is a scaffold.
- `uft_scp_direct_get_capabilities` static values vs a real SCP firmware
  GET_INFO response — needs a vendored SDK or real hardware.

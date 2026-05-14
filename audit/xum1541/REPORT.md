# XUM1541 / ZoomFloppy

**Verdict:** PARTIAL
**LOC:** 1057 (`xum1541_provider_v2.cpp` 586 + `.h` 471) + `xum1541_usb.h` 494 | **Integration:** injected-runner (the real transport is the OpenCBM dynamic library loaded by `OpenCbmLibrary` in `xum1541_usb.h`; the C-HAL `uft_xum1541.c` is an honest M3.2 stub the provider does NOT call)

XUM1541 / ZoomFloppy is a sector-level Commodore IEC-bus controller.
ZoomFloppy runs XUM1541 firmware and speaks the identical OpenCBM
protocol — UFT exposes one V2 provider for the whole family
(`hardwaretab.cpp:295-301`). The V2 provider is a mixin composition of
`ReadsSectors` + `WritesSectors` + `DetectsDrive`. It is an **honest
scaffold** — every `do_*` path returns a typed `ProviderError` (with an
explicit "M3.2 pending" marker) when its injected runner is null or
reports `opencbm_unavailable`; it never fakes a successful read or write.

This provider is the one the REFACTOR_BRIEF flags for the
"XUM1541-Antipattern: 1114 LOC structured stub" warning. The audit
finding: the V2 provider (1057 LOC) is **not** that antipattern — it is
a thin, honest translation layer. The structured-stub risk lives in the
C-HAL `uft_xum1541.c` and the V1 provider that was deleted; the V2 file
delegates all I/O to an injected runner and is honest about the runner
being absent.

Audited files:
- `src/hardware_providers/xum1541_provider_v2.{h,cpp}` — V2 provider
- `src/hardware_providers/xum1541_usb.h` — USB/IEC protocol constants + `OpenCbmLibrary` dynamic loader (C++ header)
- `include/uft/hal/uft_xum1541.h` — C HAL header (`UFT_SKELETON_PARTIAL`, M3.2: 13 real / 13 honest stubs)
- `src/hal/uft_xum1541.c` — C HAL impl (honest M3.2 stubs return `UFT_ERR_NOT_IMPLEMENTED`)
- `src/hardwaretab.cpp` — GUI routing (not wired for XUM1541)

---

## D1 — Wire-Protocol

Constants diff: `python diff.py` -> `evidence.json`. **82 PASS / 0 FAIL / 3 MISSING.**

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| USB VID ZoomFloppy | `0x16D0` (`xum1541_usb.h:36`) | `0x16D0` (OpenCBM `xum1541.h`) | PASS |
| USB PID ZoomFloppy | `0x04B2` (`xum1541_usb.h:37`) | `0x04B2` | PASS |
| USB VID/PID XUM1541 | `0x16D0` / `0x0504` (`xum1541_usb.h:40-41`) | `0x16D0` / `0x0504` | PASS |
| USB PID XUM1541 ALT (DIY) | `0x0503` (`xum1541_usb.h:44`) | `0x0503` | PASS |
| Bulk endpoints | IN `0x81`, OUT `0x02` (`xum1541_usb.h:55-56`) | IN `0x81`, OUT `0x02` | PASS |
| 32 firmware commands `XUM1541_ECHO`..`XUM1541_CMD_MAX` (`xum1541_usb.h:74-110`) | `0`..`32` | OpenCBM `xum1541.h` enum | PASS (all 32) |
| 12 IEC bus command bytes `IEC_LISTEN`..`IEC_LINE_SRQ` (`xum1541_usb.h:142-157`) | `0x20`..`0x10` | IEEE-488 / Commodore serial bus standard | PASS (all 12) |
| 19 CBM DOS status codes `CBM_STATUS_*` (`xum1541_usb.h:174-194`) | per Commodore DOS manual | PASS (all 19) |
| 11 drive-geometry constants `CBM_1541/1571/1581_*` (`xum1541_usb.h:214-228`) | Commodore drive ROM listings | PASS (all 11) |
| `XUM1541_INFO_CAPS/FW_VERSION/PROTO_VERSION` (`xum1541_usb.h:117-119`) | a separate `Xum1541Info` sub-command enum, not in the reference's command table | MISSING (×3 — UFT-extra, not a defect) |

**Reference provenance: `recalled`** — the XUM1541 firmware command set,
USB IDs and endpoints are from OpenCBM's `xum1541.h`; the IEC bus
command bytes are the IEEE-488 / Commodore serial-bus standard
(1541 User's Guide & Service Manual); the CBM DOS status codes and drive
geometry are from Commodore DOS manuals + drive ROM listings. **None
vendored.** `xum1541_usb.h:7-14` itself states it "mirrors the values
found in the OpenCBM project" — the 82 PASS rows confirm that mirror is
faithful at recalled grade.

**Findings:**
- **XUM-D1-1** (low): the 3 MISSING rows (`XUM1541_INFO_*`) are members
  of the `Xum1541Info` GET_RESULT sub-command enum, not the main
  `Xum1541Cmd` enum. They are UFT-present / reference-absent because the
  reference table only lists the command enum. Not a defect — noted for
  completeness; a vendored `xum1541.h` would add the `Xum1541Info` enum
  and lift these to PASS.
- **XUM-D1-2** (low): D1 verifies the *constant tables*. It does **not**
  verify the byte-exact USB control-transfer framing
  (`bRequest`/`wValue`/`wLength` packing) because the V2 provider
  contains no USB transfer code — that lives in OpenCBM (loaded
  dynamically) or in an un-implemented runner. Opcode-level PASS,
  frame-level UNVERIFIED.

**Status: PASS** — opcode / IEC / status / geometry / USB-ID layer
fully matches at `recalled` grade; XUM-D1-1/2 are low-severity
notes, not FAILs.

---

## D2 — Datapath

**Trace (read path):**
`XUM1541ProviderV2::do_read_sector(ReadSectorParams)` (`xum1541_provider_v2.cpp:379`)
-> null-runner guard -> `null_runner_error("sector read")` if `!m_read_runner` (`:381-383`)
-> geometry validation `cylinder in [0, m_max_cylinder]`, `head in [0,1]` (`:387-392`)
-> build `Xum1541ReadRequest{cylinder, head, retries, device_num, drive_type}` (`:395-400`)
-> `m_read_runner(req)` — the injected lambda (OpenCBM IEC `U1` block-read loop) — returns `Xum1541ReadResult` (`:402`)
-> branch: `opencbm_unavailable || invalid_track || (good==0 && total==0 && error)` -> `translate_read_failure()` (`:405-410`); `sector_bytes.empty()` -> `SectorUnreadable` (`:413-425`); else -> `translate_read_result()` (`:427`).

**Conversions named:**
- `Xum1541ReadResult::sector_bytes` -> `SectorRead::data` via `.assign(begin,end)` (`:238`) — verbatim byte copy, no transform.
- `bad_sectors > 0` -> `SectorMarginal` with `divergent_reads` = `[partial_bytes, empty_sentinel]` (`:202-232`) — Rule F-3 path.
- `opencbm_unavailable` -> `ProviderError` with "M3.2 pending" marker (`:134-150`).
- `(cylinder, head)` -> CBM track number — done *inside the runner*, surfaced back as `Xum1541ReadResult::cbm_track` (used only in error/note text, `:156-169`).

**Write path:** `do_write_sector(WriteSectorParams, SectorPayload)` (`:448`) -> geometry validation (`:456-479`) -> `Xum1541WriteRequest` with `payload.bytes` copied in (`:482-489`) -> `m_write_runner(req)` -> `translate_write_result()` (`:493`): `write_protected` -> `WriteRefused` (CBM status 26, `:250-263`); `opencbm_unavailable` -> `ProviderError` M3.2 (`:266-278`); partial write -> `ProviderError` (`:299-319`); success -> `WriteCompleted` (`:337-354`).

**Sink — where does `SectorRead` / `WriteCompleted` go?** **Nowhere in
production.** The V2 provider compiles into `UnifiedFloppyTool.pro`
(`:370`) but `HardwareTab` does not route to it (D3), and no production
code constructs an `XUM1541ProviderV2` with real runners — only
`tests/test_xum1541_provider_v2.cpp`. The datapath translation is
correct and lossless in isolation, but it has no consumer.

**Finding XUM-D2-1** (medium): identical to FC-D2-1 — the F-3
`divergent_reads` second entry is an empty-vector padding sentinel
(`m.divergent_reads.emplace_back()`, `:218`), not a real second read.
`size() >= 2` is met by padding. Honest (comment `:212-217`), but a
voting consumer gets one real buffer + one empty.

**Finding XUM-D2-2** (low): the CBM-track-number derivation
(cylinder/head -> CBM track, including the 1571 side-1 -> tracks 36-70
mapping) is entirely inside the un-implemented runner. The V2 provider
only *echoes* `cbm_track` from the result into text strings — it cannot
itself be audited for the mapping correctness because the mapping code
does not exist in the tree.

**Status: PARTIAL** — `do_read_sector` / `do_write_sector` translation
is correct and lossless; but the path has no production consumer, the
F-3 entry is a padding sentinel, and the CBM-track mapping lives in an
absent runner.

---

## D3 — GUI-Integration (V2-adapted)

**Routing: FAIL.** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249`)
returns `::uft::hal::GreaseweazleProviderV2*` — a Greaseweazle-only
type. There is no `XUM1541ProviderV2` member on `HardwareTab`. Selecting
"XUM1541 / XUM1541-II / ZoomFloppy" in the controller combo
(`hardwaretab.cpp:301`) and pressing Connect reaches the P1.18 stub:
`onConnect()` shows `QMessageBox::information(... "Controller routing
pending")` + `updateStatus("XUM1541 routing pending (P1.18)")`
(`hardwaretab.cpp:624-642`). `onControllerChanged` does run
XUM1541-specific combo setup (`isCommodoreUSB` branch, device 8-11 drive
select, `hardwaretab.cpp:932-963`) — but that is pre-connect UI state,
not capability routing.

**Capability gating: N/A.** `forms/tab_hardware.actions.yaml` has
`provider_source: self->currentProviderV2()` and `extra_includes:` lists
only `greaseweazle_provider_v2.h`. Every `wire_action<cap::X>` is bound
to `GreaseweazleProviderV2`, never `XUM1541ProviderV2`.

| Button | Capability tag | `XUM1541ProviderV2` satisfies? | Wired |
|--------|----------------|--------------------------------|-------|
| (any sector-read button) | `ReadsSectors` | yes (`static_assert :416`) | no — not routed |
| (any sector-write button) | `WritesSectors` | yes (`static_assert :419`) | no — not routed |
| (any detect button) | `DetectsDrive` | yes (`static_assert :422`) | no — not routed |
| `btnReadTest` (flux) | `ReadsRawFlux` | no — negative `static_assert :427` | correctly absent |
| `btnMotorOn/Off` | `ControlsMotor` | no — negative `static_assert :436` | correctly absent |
| `btnSeekTest` | `SeeksHead` | no — negative `static_assert :440` | correctly absent |

**Status: FAIL** — XUM1541 is one of the 8 unrouted providers. The
capability declarations (and the negative assertions documenting why
motor/seek/recalibrate are absent — IEC bus has no standalone primitives)
are sound; the integration is absent.

---

## D4 — OS-Detection

The XUM1541 / ZoomFloppy enumerates as a USB device. The real detection
transport is the OpenCBM dynamic library, loaded by `OpenCbmLibrary::load()`
in `xum1541_usb.h:351-417`:

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | `OpenCbmLibrary::load()` does `LoadLibraryA("opencbm.dll")`, then `"C:\\Program Files\\opencbm\\opencbm.dll"`, then `"...(x86)..."` (`xum1541_usb.h:355-360`); resolves `cbm_driver_open` / `cbm_listen` / ... via `GetProcAddress`. `cbm_driver_open` then walks the USB list. **But** this is a library *loader*, not a device *enumerator the V2 provider invokes* — the V2 `Xum1541DetectRunner` that would call `cbm_driver_open` + `cbm_identify` has no production construction site. | loader code present + correct; runner that uses it absent | **PARTIAL** |
| **Linux** | `dlopen("libopencbm.so")`, `"libopencbm.so.0"`, `"/usr/local/lib/..."`, `"/usr/lib/..."` (`xum1541_usb.h:365-368`); `dlsym` resolution. Same caveat — loader present, invoking runner absent. | loader code present + correct; runner absent | **PARTIAL** |
| **macOS** | falls into the `#else` (non-`_WIN32`) branch -> `dlopen("libopencbm.so" ...)`. **macOS ships dynamic libraries as `.dylib`, not `.so`** — `libopencbm.dylib` would not be found by any of the four `dlopen` paths (`xum1541_usb.h:365-368`). On macOS the OpenCBM library load **fails silently** and detection returns "not found". | loader path list omits `.dylib` | **FAIL** |

**Cross-check (VID/PID, GUI hint):** `diff.py`'s `gui_hint_crosscheck`
finds that `hardwaretab.cpp:453` tests `vid == 0x16D0 && pid == 0x0504`
and labels it `"ZoomFloppy/XUM1541"` — but per `xum1541_usb.h:37` and
`:41`, `0x0504` is the **XUM1541** PID; **ZoomFloppy is `0x04B2`**. The
GUI port-hint will therefore label a real XUM1541 correctly but will
**not** put a "ZoomFloppy" hint on an actual ZoomFloppy (PID `0x04B2`),
which falls through to the generic description branch.

**Findings:**
- **XUM-D4-1** (medium): `hardwaretab.cpp:453` uses the XUM1541 PID
  (`0x0504`) under a "ZoomFloppy/XUM1541" label. A genuine ZoomFloppy
  (`0x16D0:0x04B2`, `xum1541_usb.h:37`) is not matched by that hint.
  The `isKnownXum1541()` helper in `xum1541_usb.h:484-489` *does* know
  all three VID/PIDs — the GUI hint just doesn't use it.
- **XUM-D4-2** (medium): `OpenCbmLibrary::load()` has no `.dylib` path
  for macOS (`xum1541_usb.h:365-368`). On macOS, OpenCBM cannot be
  loaded, so XUM1541 detection (and all I/O) silently fails. Cross-check:
  a Homebrew OpenCBM install puts `libopencbm.dylib` under
  `/opt/homebrew/lib` or `/usr/local/lib`.
- **XUM-D4-3** (medium): even on Win/Linux where the loader works, the
  V2 detect path is unreachable — no production `Xum1541DetectRunner` is
  constructed (only `tests/`). The loader is correct; nothing production
  drives it.

**Status: PARTIAL** — the OpenCBM loader is present and correct for
Windows/Linux (PARTIAL because no production runner invokes it); macOS
is FAIL (`.dylib` not in the search list). Net: PARTIAL.

---

## D5 — Integritaets-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| XUM-D5-1 | **Honest scaffold — no fake success. This is the key finding for the "XUM1541-Antipattern" warning.** Null runner -> `null_runner_error()` returns a 3-part `ProviderError` (`xum1541_provider_v2.cpp:92-107`). `opencbm_unavailable` -> `ProviderError` with an explicit `"(M3.2 pending)"` marker in the `what` string and an install-OpenCBM `fix` (`:134-150`, `:266-278`). The provider **never** returns a `SectorRead` / `WriteCompleted` it did not actually receive. The V2 file is NOT the 1114-LOC structured stub the brief warns about — it is a thin honest translation layer. | positive (P0-risk cleared) | `:92-107, 134-150, 266-278` |
| XUM-D5-2 | The honest C-HAL `uft_xum1541.c` returns `UFT_ERR_NOT_IMPLEMENTED` from all 13 USB/IEC stubs with a descriptive `cfg->last_error` (`uft_xum1541.c:182-375`). Its stub error strings tell callers to "use the Qt provider path (Xum1541HardwareProvider::autoDetectDevice in src/hardware_providers/)" (`uft_xum1541.c:213-219`) — **but that V1 Qt provider was deleted in P1.18.** The stub's advice points at a file that no longer exists. | medium (stale advice) | `uft_xum1541.c:213-219` |
| XUM-D5-3 | F-3 divergent-reads second entry is an empty-vector padding sentinel, not a real second read (`:218`). Same pattern as FC-D5-2. Honestly commented (`:212-217`). | medium | `:202-232` |
| XUM-D5-4 | `do_detect_drive` success path: when the runner did not report `tracks`/`heads`, defaults to `tracks = 35`, `heads = 1` (1541 geometry) (`:566-567`), and **always** sets `rpm_nominal = 300.0` (`:571`). For a 1581 (80 tracks, 3.5") detected without explicit geometry this is wrong on track count. Less severe than FC-D5-3 because here the defaults are only used when the runner is silent, and the comment is honest — but it is still substituted geometry, not measured. | low-medium | `:564-571` |
| XUM-D5-5 | Header doc-comments describe the full OpenCBM `U1`/`U2` block-command sequences (`xum1541_provider_v2.cpp:16-37`) that **no code in this file implements** — the sequences live in the absent runner. Per Hard rule #4 the comment is not proof the protocol is implemented. | low (documentation) | `.cpp:14-37`, `.h:58-93` |

**Positives (forensic-integrity holds):**
- No swallowed errors — every `Xum1541ReadResult` / `Xum1541WriteResult`
  failure flag (`opencbm_unavailable`, `invalid_track`, `bad_sectors`,
  `write_protected`, `invalid_params`, empty `sector_bytes`) is checked
  and mapped to a typed outcome (`:405-427`, `:250-354`).
- Rule F-4: every `ProviderError` builds non-empty what/why/fix; the
  M3.2-marker errors additionally name the exact follow-up needed
  (install OpenCBM).
- Rule F-3 (verbatim copy): `translate_read_result` copies all
  `sector_bytes` with no averaging/pruning (`:238`).
- Write-protect is type-safe: CBM status 26 -> `WriteRefused`, not a
  magic int (`:250-263`).
- Null-runner construction is honest — never a fake success (XUM-D5-1).
- The negative `static_assert`s (`!ControlsMotor`, `!SeeksHead`,
  `!Recalibrates`, `xum1541_provider_v2.h:436-447`) correctly encode
  that the IEC bus has no standalone motor/seek primitives — the
  provider does not pretend to capabilities it lacks.

**Status: PASS with findings** — no critical (P0) integrity violation.
**XUM-D5-1 is the headline: the V2 provider is an honest scaffold, not a
fake-success stub.** XUM-D5-2 (stale advice pointing at a deleted file)
and XUM-D5-4 (substituted geometry) are worth fixing.

---

## Fixes (prioritised)

**P0:** none. (The "XUM1541-Antipattern" P0-risk is **cleared** — the V2
provider returns honest `ProviderError`s, not fake success.)

**P1:**
- **XUM-D3 / XUM-D4-3** — route `XUM1541ProviderV2` through `HardwareTab`
  and construct a production `Xum1541Runner` / `Xum1541WriteRunner` /
  `Xum1541DetectRunner` that wraps `OpenCbmLibrary`. Until then the
  honest, correct translation layer has no consumer and no transport.
- **XUM-D4-2** — add `.dylib` paths to `OpenCbmLibrary::load()`
  (`libopencbm.dylib`, `/opt/homebrew/lib/libopencbm.dylib`,
  `/usr/local/lib/libopencbm.dylib`). Without this, XUM1541 is dead on
  macOS.

**P2:**
- **XUM-D4-1** — make the `hardwaretab.cpp:453` port hint use
  `xum1541::isKnownXum1541(vid, pid)` (it already knows all three
  VID/PIDs) instead of a single hardcoded XUM1541 PID under a
  "ZoomFloppy" label.
- **XUM-D5-2** — update the `uft_xum1541.c` stub error strings: they
  point callers at `Xum1541HardwareProvider` in `src/hardware_providers/`,
  which was deleted in P1.18. Point them at the V2 provider + runner
  instead, or just say "M3.2 follow-up".

**P3:**
- **XUM-D5-3** — make the F-3 second `divergent_reads` entry meaningful
  or document the sector-device exception (same as FC-D5-2).
- **XUM-D5-4** — `do_detect_drive` should report runner geometry or
  "unknown", not a 1541-default 35/1 + always-300-RPM.
- **XUM-D5-5** — annotate the `U1`/`U2` doc-comments as
  "DESCRIBES INTENDED RUNNER — not implemented in this file".

---

## Reproduce

```bash
cd audit/xum1541
python extract_uft.py        # UFT-side constants (JSON)
python extract_ref.py        # official reference + provenance (JSON)
python diff.py               # writes evidence.json, prints verdict + gui-hint crosscheck
"C:/Qt/Tools/mingw1310_64/bin/gcc.exe" -std=c11 -I../../include \
    -fsyntax-only test_xum1541_vectors.c   # D1 build-gate (C-HAL IEC layer)
```

Source reads behind this report:
```bash
git grep -nE "USB_VID_|XUM1541_|IEC_|CBM_STATUS_|CBM_15" src/hardware_providers/xum1541_usb.h
sed -n '379,428p;448,494p' src/hardware_providers/xum1541_provider_v2.cpp  # do_read/do_write
sed -n '351,417p' src/hardware_providers/xum1541_usb.h                     # OpenCbmLibrary::load
sed -n '182,375p' src/hal/uft_xum1541.c                                    # honest M3.2 stubs
sed -n '447,464p;624,645p;932,963p' src/hardwaretab.cpp                     # GUI hint + routing
git grep -nE "XUM1541ProviderV2" src/                                       # construction sites (none)
```

## Not reviewed
- Byte-exact USB control-transfer framing (`bRequest`/`wValue`/`wLength`
  packing) — the V2 provider has no USB transfer code (XUM-D1-2).
- The cylinder/head -> CBM-track-number mapping — lives in an absent
  runner (XUM-D2-2).
- The C++-only `xum1541_usb.h` cannot be `-std=c11` syntax-checked; its
  constants are covered by diff.py (textual parse), and the pure-C
  `uft_xum1541.h` IEC layer by `test_xum1541_vectors.c`.
- Runtime behaviour against a real XUM1541/ZoomFloppy — HIL (`tests/hil/`, P3.4).
- `tests/test_xum1541_provider_v2.cpp` / `tests/test_xum1541_hal.c`
  internals — reviewed only enough to confirm the provider's sole
  construction site is the test, and that the C-HAL stub-honesty tests exist.

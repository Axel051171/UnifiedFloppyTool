# USB-Floppy (UFI)

**Verdict:** PARTIAL
**LOC:** 1164 (`usbfloppy_provider_v2.cpp` 639 + `.h` 525) | **Integration:** native (C-HAL `ufi.h` / `ufi.c`, 357 LOC) via runner-injection

USB-Floppy is a standard USB Mass Storage floppy drive (Panasonic/TEAC-class
chipset) driven through the **UFI** command set — "USB Mass Storage Class
UFI Command Specification Rev. 1.0" (USB-IF, 1998), a SCSI-like CDB subset.
Unlike ADF-Copy, USB-Floppy **has a real C-HAL**: `include/uft/hal/ufi.h`
defines the `uft_ufi_opcode_t` enum and `src/hal/ufi.c` builds the CDBs. The
V2 provider uses the runner-injection pattern: in production the runners wrap
`uft_ufi_read_sectors()` / `uft_ufi_write_sectors()` / `uft_ufi_inquiry()`;
in tests they are scripted lambdas.

Audited files:
- `src/hardware_providers/usbfloppy_provider_v2.{h,cpp}` — V2 provider
- `include/uft/hal/ufi.h` — UFI opcode enum + high-level API
- `src/hal/ufi.c` — CDB builders, `exec_one()`, `ensure_backend()`
- `src/core/uft_core_stubs.c` — `uft_ufi_backend_init()` stub
- `src/hardwaretab.cpp` — GUI routing

---

## D1 — Wire-Protocol

Constants diff: `python diff.py` → `evidence.json`. **18 PASS / 0 FAIL / 0 UNVERIFIED / 0 MISSING.**

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| 12 UFI opcodes `TEST_UNIT_READY`…`VERIFY_10` (`ufi.h:25-36`) | `0x00`,`0x03`,`0x04`,`0x12`,`0x15`,`0x1A`,`0x1B`,`0x23`,`0x25`,`0x28`,`0x2A`,`0x2F` | standard SCSI/MMC opcodes, USB-IF UFI spec | PASS (all 12) |
| INQUIRY CDB allocation length | `36` (`ufi.c:53`) | standard inquiry-data length | PASS |
| REQUEST SENSE CDB allocation length | `18` (`ufi.c:81`) | standard sense-data length | PASS |
| READ(10) / WRITE(10) CDB length | `10` (`ufi.c:167`) | fixed by SCSI CDB-10 format | PASS |
| sectors-per-track inference 2880/1440/5760 LBA | `18` / `9` / `36` (`usbfloppy_provider_v2.cpp:610,612,614`) | 3.5" HD 1.44M / DD 720K / ED 2.88M | PASS (3) |

**Reference provenance: `recalled`.** The UFI opcodes are *standard SCSI
opcodes* — the USB-IF UFI Command Specification Rev. 1.0 is publicly
available but **not vendored** into the repo. The 12 opcode rows are a
recalled-grade diff against well-known SCSI values, not a vendored byte-diff
of the spec PDF. CDB lengths are fixed by the SCSI CDB format; geometry is
universal PC-floppy knowledge.

This is the **cleanest D1 of any audited provider** — 18/18 PASS, 0
UNVERIFIED — because UFI rides on standardised SCSI opcodes that have an
unambiguous, decades-stable reference. The CDB byte layouts in `ufi.c`
(`uft_ufi_read_sectors`, `:157-177`: opcode in `cdb[0]`, big-endian LBA in
`cdb[2..5]`, BE count in `cdb[7..8]`) match the standard READ(10) CDB.

**Status: PASS** — opcode/CDB layer fully matches the recalled UFI/SCSI
reference. (The D5 problem is at a different layer — see below.)

---

## D2 — Datapath

**Trace (read path):**
`USBFloppyProviderV2::do_read_sector(ReadSectorParams)` (`usbfloppy_provider_v2.cpp:359`)
→ geometry validation against `m_max_cylinders` (`:366-371`)
→ builds `UsbFloppyReadRequest{cylinder, head, sector, retries, device_path,
  total_lba, block_size}` (`:374-381`)
→ `m_read_runner(req)` → `UsbFloppyReadResult{sector_bytes[], good_sectors,
  bad_sectors, total_sectors, no_disk, backend_unavailable, device_error}`
→ failure routing: `translate_read_failure()` (`:128-200`) for
  backend/device/no-disk; else `translate_read_result()` (`:203-248`)
→ **conversion:** clean read → `SectorRead{position, data, quality=CRC_OK}`
  (`:242-247`); `bad_sectors>0` → `SectorMarginal` with `divergent_reads`
  ≥ 2 entries (partial data + empty sentinel, `:213-238`).

**Conversions named:** `(cylinder*2 + head) * spt` → LBA (done *inside the
runner*, not the provider — `usbfloppy_provider_v2.h:79-83`, `.cpp:14-16`);
`total_lba` → `spt` via the 2880/1440/5760 table (`do_detect_drive`,
`:603-621`); `block_size` defaults to 512 when 0 (`:591`). The provider
itself does **no** byte-level transformation of sector data — `SectorRead.data`
is `assign()`-ed verbatim from `result.sector_bytes` (`:244`). Rule F-3
upheld: partial reads preserve the bytes received in
`divergent_reads[0]` and never collapse/average (`:219-226`); verify-failure
preserves both `intended` and `readback` (`:297-310`).

**Sink — where does `SectorRead` go?** **Nowhere reachable.** Like ADF-Copy,
the provider is not routed by `HardwareTab` (D3). The only callers are
`tests/test_usbfloppy_provider_v2.cpp` with scripted lambdas. No production
runner that wraps the `uft_ufi_*` C-HAL is constructed anywhere in the
shipping tree — and even if it were, the C-HAL has no platform backend (D4),
so every `uft_ufi_*` call returns `UFT_ERR_NOT_IMPLEMENTED`.

**Finding UF-D2-1** (medium): the V2 datapath is correct and F-3-compliant
in isolation, but it is a dead-end at both ends — no production runner
factory, no GUI sink. The LBA translation that the V1 did internally is now
the *runner's* responsibility (`.h:79-83`); since no production runner
exists, the CHS→LBA mapping has no shipping implementation to audit.

**Status: PARTIAL** — outcome logic correct and forensically faithful; no
production caller, no GUI sink, and the LBA-mapping half lives in a
runner that does not yet exist.

---

## D3 — GUI-Integration (V2-adapted)

**Routing:** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249-257`)
is hardcoded to `m_gwProviderV2.get()` — a `GreaseweazleProviderV2*`.

**USB-Floppy is special — role-gated combo entry.** Unlike the other 7
non-GW providers, USB-Floppy is **only added to the controller combo in
`RoleDestination` mode**: `populateControllerList()` wraps the
`addItem(tr("USB Floppy Drive"), "usb_floppy")` in
`if (m_controllerRole == RoleDestination)` (`hardwaretab.cpp:313-316`). In
`RoleSource` mode it is not even selectable. The rationale (a USB floppy is
a write *target*, not a flux *source*) is sound, but it means USB-Floppy's
GUI surface is conditional on role before routing even enters the picture.

When USB-Floppy *is* selectable (Destination) and the user connects,
`onConnect()` (`:624-645`) takes the `controller != "greaseweazle"` branch
for `"usb_floppy"` → **"Controller routing pending"** messagebox →
`updateStatus("… routing pending (P1.18)", isError=true)` → return. No
`USBFloppyProviderV2` is ever constructed by the GUI.

**Capability → button matrix** (codegen `wire_action<cap::X>`):

| Button | Capability tag | `USBFloppyProviderV2` satisfies? | Wired for USB-Floppy |
|--------|----------------|----------------------------------|----------------------|
| (sector read) | `ReadsSectors` | yes (`usbfloppy_provider_v2.h:470`) | ✗ — `currentProviderV2()` returns GW, not this |
| (sector write) | `WritesSectors` | yes (`:473`) | ✗ |
| `btnDetect` | `DetectsDrive` | yes (`:476`) | ✗ |
| `btnReadTest` | `ReadsRawFlux` | **no** — negative `static_assert` `:481` (UFI has no flux opcode) | correctly absent |
| (write flux) | `WritesRawFlux` | **no** — negative `static_assert` `:485` | correctly absent |
| `btnMotorOn/Off` | `ControlsMotor` | **no** — negative `static_assert` `:488` (UFI `START_STOP` exists in spec but `ufi.h` exposes no `uft_ufi_start_stop()`) | correctly absent |
| `btnSeekTest` | `SeeksHead` | **no** — negative `static_assert` `:493` | correctly absent |
| `btnCalibrate` | `Recalibrates` | **no** — negative `static_assert` `:497` | correctly absent |
| `btnRPMTest` | `MeasuresRPM` | **no** — negative `static_assert` `:501` | correctly absent |

The capability composition is honest — 3 positive + 6 negative
`static_assert`s. Note `ControlsMotor` is correctly omitted *even though
UFI `START_STOP` (0x1B) exists in the opcode enum* (`ufi.h:31`), because
`ufi.h` exposes no `uft_ufi_start_stop()` high-level function — the
anti-pragmatism rule applied correctly. But because the provider is not
routed, none of this capability set reaches the codegen wiring.

**Status: FAIL** — USB-Floppy is combo-selectable only in Destination role,
and even then `onConnect()` dead-ends in the "routing pending (P1.18)"
messagebox. The provider type is conformance-tested but has zero GUI
reachability. (Shared P1.18 gap; for this provider's D3 the grade is FAIL.)

---

## D4 — OS-Detection

**This is the critical dimension for USB-Floppy.** UFI rides on SG_IO
(Linux) / SCSI_PASS_THROUGH (Windows) / IOKit SCSITaskUserClient (macOS) —
each requires a platform-specific backend that registers a `uft_ufi_ops_t`
vtable via `uft_ufi_set_backend()`. `ufi.c::ensure_backend()` (`:19-26`)
checks `g_ops` and returns `UFT_ERR_NOT_IMPLEMENTED` if it is NULL.

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Linux** | SG_IO `ioctl` on `/dev/sg*` — backend file `src/hal/ufi_linux.c` | **ABSENT** — file does not exist (`uft_core_stubs.c:434` names it as absent) | **FAIL** |
| **Windows** | `DeviceIoControl` SCSI_PASS_THROUGH on `\\.\A:` — backend file `src/hal/ufi_win.c` | **ABSENT** — file does not exist (`uft_core_stubs.c:435`) | **FAIL** |
| **macOS** | IOKit `SCSITaskUserClient` — backend file `src/hal/ufi_mac.c` | **ABSENT** — file does not exist (`uft_core_stubs.c:436`) | **FAIL** |

`uft_ufi_backend_init()` is a **stub** (`uft_core_stubs.c:443-445`) that
simply `return -1;` — "No platform backend registered." It never calls
`uft_ufi_set_backend()`. Therefore on **every** OS, `g_ops` stays NULL, and
**every** UFI function (`uft_ufi_inquiry`, `uft_ufi_read_sectors`,
`uft_ufi_write_sectors`, …) returns `UFT_ERR_NOT_IMPLEMENTED` at the first
`ensure_backend()` check. A production runner wrapping these calls would
always report `backend_unavailable` — which the V2 provider correctly
surfaces as a 3-part `ProviderError` (`:133-147`).

**Finding UF-D4-1** (P0-class): no UFI platform backend exists for any OS.
`uft_ufi_backend_init()` is a `return -1` stub. The entire UFI C-HAL is a
build-complete, runtime-dead façade — `ufi.c` compiles and the CDB builders
are correct, but they can never reach hardware because no backend is ever
registered. USB-Floppy cannot read or write a single sector on any platform.

**Finding UF-D4-2** (medium, ABI): **signature mismatch.** `ufi.h:58`
declares `void uft_ufi_backend_init(void);` but `uft_core_stubs.c:443`
**defines** `int uft_ufi_backend_init(void) { return -1; }`. The return
type differs (`void` vs `int`). This is a §1.3-class silent ABI mismatch —
the same bug pattern that `uft_core_stubs.c`'s own comments flag elsewhere
(e.g. the `uft_cpu_has_feature` note at `:450-457`). Any caller that
`#include`s `ufi.h` sees a `void` return and cannot read the `-1`; callers
in the same TU as the definition see `int`. The `-1` "no backend" signal is
**unreachable through the public header** — the honest failure indicator the
stub comment promises (`:438`) does not actually reach header-only callers.

**Status: FAIL** — no backend on any of the three OSes; `backend_init` is a
stub with a return-type ABI mismatch against its own header.

---

## D5 — Integritäts-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| UF-D5-1 | **Runtime-dead C-HAL façade.** The whole UFI C-HAL (`ufi.h` + `ufi.c`, real opcode enum, real CDB builders) is structurally complete and *looks* like a working backend, but `uft_ufi_backend_init()` never registers a backend — `g_ops` is permanently NULL. This is the "stub façade" pattern: build-complete, runtime-impossible. It is **honestly labelled** (`uft_core_stubs.c:428-441` documents the absent platform files and the `return -1`), so it is not a silent fake — but a casual reader of `ufi.c` alone would believe USB-Floppy works. | high | `src/core/uft_core_stubs.c:443-445`; `src/hal/ufi.c:19-26` |
| UF-D5-2 | **`backend_init` return-type ABI mismatch** (= UF-D4-2). `ufi.h:58` `void` vs `uft_core_stubs.c:443` `int`. The `-1` failure signal cannot be read through the public header. | medium | `include/uft/hal/ufi.h:58` vs `src/core/uft_core_stubs.c:443` |
| UF-D5-3 | No production runner factory. `USBFloppyProviderV2`'s 3 runners are only ever constructed with scripted lambdas in `tests/`. The production runner templates exist **only as doc-comment example code** in the header (`usbfloppy_provider_v2.h:309-327, 348-364`) — never compiled, never instantiated. Combined with UF-D5-1, even a hand-written production runner could not function. | medium | `usbfloppy_provider_v2.h:309-327` |
| UF-D5-4 | `uft_ufi_format_floppy()` is an explicit `UFT_ERR_NOT_IMPLEMENTED` stub (`ufi.c:222-228`). Not exposed as a V2 capability, so it is not a phantom feature — but it is dead surface in a "complete-looking" C-HAL. | low (documented) | `src/hal/ufi.c:222-228` |

**Positives (forensic-integrity holds):**
- No swallowed errors — `ensure_backend()` returns a typed
  `UFT_ERR_NOT_IMPLEMENTED` with a diagnostic string (`ufi.c:22`); the
  provider maps every `backend_unavailable` / `device_error` / `no_disk`
  flag to a typed outcome (`translate_read_failure`, `:128-200`).
- Rule F-4: every `ProviderError` carries non-empty what/why/fix; the
  backend-unavailable error explicitly tells the user to call
  `uft_ufi_backend_init()` and load the `sg` module (`:133-147`).
- Rule F-3: partial reads → `SectorMarginal` with `divergent_reads` ≥ 2;
  verify-failure preserves both `intended` and `readback` (`:297-310`).
- No fake success — null runner → `ProviderError`; empty data →
  `SectorUnreadable`. The provider never fabricates sector data.
- The C-HAL absence is **documented**, not hidden — `uft_core_stubs.c`
  names the three missing platform files explicitly.

**Status: FAIL with documented honesty** — UF-D5-1 (runtime-dead C-HAL
façade) is a high-severity integrity finding: the UFI layer presents a
complete-looking backend that cannot function on any OS. It is honestly
labelled (so not a P0 *lie*), but it is a forensic foot-gun — and UF-D5-2
(ABI mismatch) means even the "no backend" signal is partly broken.

---

## Fixes (prioritised)

**P0:**
- **UF-D4-1 / UF-D5-1** — implement at least one UFI platform backend
  (`src/hal/ufi_linux.c` SG_IO is the simplest) and make
  `uft_ufi_backend_init()` actually call `uft_ufi_set_backend()`. Until then
  USB-Floppy cannot touch hardware on any OS — the provider, the C-HAL, and
  the runners are all unreachable-by-construction. This is the single most
  important finding for this provider.

**P1:**
- **UF-D5-2 / UF-D4-2** — fix the `uft_ufi_backend_init()` return-type ABI
  mismatch: either change `ufi.h:58` to `int` (and let callers read the
  `-1`) or change `uft_core_stubs.c:443` to `void`. The header and the
  definition must agree. Recommended: `int`, so the failure is observable.
- **UF-D3 / UF-D2-1** — route `HardwareTab` to `USBFloppyProviderV2` in
  Destination role (task P1.18), and write the production runner factory
  wrapping the `uft_ufi_*` C-HAL (incl. the CHS→LBA mapping the V1 did
  internally). *(blocked by P0 above — pointless to route to a dead C-HAL.)*

**P2:**
- **UF-D5-3** — promote the doc-comment runner templates
  (`usbfloppy_provider_v2.h:309-327`) into a real compiled
  `usbfloppy_runners.cpp` factory once a backend exists.

**P3:**
- **UF-D5-4** — either implement `uft_ufi_format_floppy()` or remove the
  stub from the public surface (it is device-specific and currently dead).

---

## Reproduce

```bash
cd audit/usbfloppy
python extract_uft.py        # UFT-side UFI constants (JSON)
python extract_ref.py        # official reference (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
"C:/Qt/Tools/mingw1310_64/bin/gcc.exe" -std=c11 -I../../include \
    -fsyntax-only test_usbfloppy_vectors.c   # D1 build-gate (12 opcode static_asserts)
```

Source reads behind this report:
```bash
grep -nE "UFT_UFI_" include/uft/hal/ufi.h                        # opcode enum
grep -nE "ensure_backend|g_ops|exec_one" src/hal/ufi.c           # backend gate
grep -nA3 "uft_ufi_backend_init" src/core/uft_core_stubs.c       # the return -1 stub
sed -n '359,407p' src/hardware_providers/usbfloppy_provider_v2.cpp  # do_read_sector
sed -n '313,316p;624,645p' src/hardwaretab.cpp                      # role-gated combo + routing
```

## Not reviewed
- The three absent platform backend files (`ufi_linux.c` / `ufi_win.c` /
  `ufi_mac.c`) — they do not exist; this report confirms their absence
  rather than reviewing them.
- The production runner factory — does not exist in the tree (UF-D5-3); the
  CHS→LBA mapping it would own is therefore unaudited at the implementation
  level.
- `tests/test_usbfloppy_provider_v2.cpp` scripted-lambda coverage (the
  provider passes its conformance sections — taken as given).
- Runtime behaviour against a real USB floppy drive — HIL, P3.4. (Impossible
  to test today regardless, per UF-D4-1.)
- The deleted V1 `usbfloppyhardwareprovider.cpp` — only its V2-header
  audit-summary doc-comment was read.

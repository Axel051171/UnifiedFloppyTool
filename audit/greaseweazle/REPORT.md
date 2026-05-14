# Greaseweazle

**Verdict:** PARTIAL
**LOC:** 906 (`greaseweazle_provider_v2.cpp` 698 + `.h` 208) | **Integration:** native (C-API wrapper over `src/hal/uft_greaseweazle_full.c`, 1191 LOC)

This is the **pilot audit** — it establishes the V2-adapted audit format
for the other 8 providers. Greaseweazle is the production-tested
reference backend; its C HAL path is `uft_greaseweazle_full.c`.

Audited files:
- `src/hardware_providers/greaseweazle_provider_v2.{h,cpp}` — V2 provider
- `src/hal/uft_greaseweazle_full.{h,c}` — C HAL backend (protected path, read-only)
- `src/hardwaretab.cpp` — GUI routing
- `include/uft/gui/wiring_runtime.h` — codegen wiring template

---

## D1 — Wire-Protocol

Constants diff: `python diff.py` → `evidence.json`. **40 PASS / 0 FAIL / 7 MISSING / 2 UNVERIFIED.**

| Aspect | UFT value | Reference value | Status |
|--------|-----------|-----------------|--------|
| USB VID | `0x1209` (`uft_greaseweazle_full.h:37`) | `0x1209` (pid.codes) | PASS |
| USB PID | `0x4D69` (`:38`) | `0x4D69` | PASS |
| USB PID F7 | `0x4D69` (`:39`) | same as base device | PASS |
| 22 command opcodes `CMD_GET_INFO`…`CMD_NO_CLICK_STEP` (`:66–88`) | `0x00`…`0x16` | keirf/greaseweazle `inc/cmd.h` class `Cmd` | PASS (all 22) |
| 14 ACK codes `ACK_OK`…`ACK_OUT_OF_FLASH` (`:103–116`) | `0x00`…`0x0D` | `inc/cmd.h` class `Ack` | PASS (all 14) |
| `CMD_READ_MEM` `0x20`, `CMD_WRITE_MEM` `0x21`, `CMD_GET_INFO_EXT` `0x22` (`:91–93`) | — | not in recalled official `Cmd` enum | **UNVERIFIED** → GW-D1-1 |
| Command frame layout `[cmd, len, ...params]`, response `[cmd, ack]` | `uft_gw_command()` (`uft_greaseweazle_full.c:595,607,646,675,701,…`) | official 2-byte header + payload | PASS (opcode-level) / UNVERIFIED (byte-exact frame) → GW-D1-2 |
| sample frequency | `72 MHz` fallback `#define` (`:45`); runtime from `flux->sample_freq` then `uft_gw_get_sample_freq()` | device-reported, no fixed constant | UNVERIFIED (correctly device-reported) |

**Reference provenance: `recalled`** — the GW protocol constants are
well-known (keirf/greaseweazle `inc/cmd.h`, `src/greaseweazle/usb.py`) but
**not vendored** into this repo. The 40 PASS rows are a recalled-grade
diff, not a vendored-grade byte diff. See `audit/README.md`.

**Findings:**
- **GW-D1-1** (medium): `CMD_READ_MEM` / `WRITE_MEM` / `GET_INFO_EXT`
  (0x20–0x22) exist in the UFT header but are absent from the recalled
  official `Cmd` enum. Either a protocol extension UFT relies on, or
  UFT-side invention. Cannot be classified without a vendored `cmd.h`.
- **GW-D1-2** (low): the per-command byte-exact frame layout of
  `uft_gw_command()` was verified at opcode-dispatch level (every
  `CMD_*` is sent via `uft_gw_command()`), not byte-traced. The header
  comment + dispatch sites are consistent with the official 2-byte
  `[cmd,len]` header.

**Status: PASS** (opcode/ACK/USB-ID layer fully matches; GW-D1-1/2 are
UNVERIFIED-grade, not FAIL).

---

## D2 — Datapath

**Trace (read path):**
`GreaseweazleProviderV2::do_read_raw_flux(ReadFluxParams)` (`greaseweazle_provider_v2.cpp:196`)
→ `uft_gw_read_track(handle, cyl, head, revs, &flux)` (C HAL)
→ `uft_gw_flux_data_t{ samples[], sample_count, index_count, sample_freq }`
→ **conversion:** ticks → ns via `uft_gw_ticks_to_ns(sample, sample_freq)` per sample (`:262`)
→ `FluxCaptured{ position, revolutions, sample_ns, transitions_ns[] }` (`FluxOutcome` variant)
→ `uft_gw_flux_free(flux)`.

**Conversions named:** ticks→ns (sample timing), `index_count`→`revolutions`,
`sample_freq`→`sample_ns` (1e9/freq). Rule F-3 upheld: all `sample_count`
samples copied verbatim, no averaging/pruning (`:256–264`); empty result
→ `FluxMarginal`, not silent success (`:269`).

**Sink — where does `FluxCaptured` go?** Two paths:
1. **Codegen-wired `btnReadTest`** → `wire_action<cap::ReadsRawFlux>` →
   `HardwareTab::onFluxOutcome(FluxCaptured&)` (`hardwaretab.cpp:851`).
   This handler **only updates the status-bar string** — the captured
   `transitions_ns` vector is **not routed to the flux decoder or the
   format registry.** The flux is observed, then discarded.
2. **Real capture-to-image** → `FluxCaptureJob` via the
   `raw_handle()` escape hatch (`greaseweazle_provider_v2.h:130`,
   `hardwaretab.cpp:259 gwDevice()`), which calls `uft_gw_*` directly,
   bypassing the V2 outcome surface entirely.

**Finding GW-D2-1** (medium): the V2 `ReadsRawFlux` GUI path is a
**dead-end for data** — `onFluxOutcome` summarises into a status line.
The functioning flux→image path still runs through the legacy
`raw_handle()` escape hatch, which the V2 header itself marks "SCHEDULED
FOR REMOVAL once P1.20/P1.21". Until P1.20/21 land, the type-driven read
path produces a correct `FluxCaptured` that no V2 consumer persists.

**Datastructure compatibility:** `FluxCaptured::transitions_ns` is
`std::vector<uint32_t>` ns; the C HAL delivers `uint32_t` ticks +
`sample_freq` — conversion is explicit and lossless within `uint32_t`
range. No endianness issue (same-process, no serialisation here).

**Status: PARTIAL** — `do_read_raw_flux` itself is correct and
forensically faithful; the V2 GUI consumer discards the result
(GW-D2-1). Persisting path = legacy escape hatch, pending P1.20/21.

---

## D3 — GUI-Integration (V2-adapted)

D3 re-defined for V2: no `makeProvider()` dispatcher exists. The question
is **codegen routing** + **capability gating**.

**Routing:** `HardwareTab::currentProviderV2()` (`hardwaretab.cpp:249`)
returns `m_gwProviderV2.get()` — a `GreaseweazleProviderV2*`. Greaseweazle
**is** routed: `onConnect()` for `controller == "greaseweazle"`
constructs the V2 provider, calls `open()`, then `rewireV2()` →
`generated::wire_hardware_tab(this)` (`:246`). PASS for GW.

**Capability → button matrix** (codegen `wire_action<cap::X>`):

| Button | Capability tag | `GreaseweazleProviderV2` satisfies? | Wired |
|--------|----------------|-------------------------------------|-------|
| `btnReadTest` | `ReadsRawFlux` | yes (`static_assert` `:175`) | ✓ codegen |
| `btnMotorOn/Off` | `ControlsMotor` | yes (`:179`) | ✓ codegen |
| `btnSeekTest` | `SeeksHead` | yes (`:181`) | ✓ codegen |
| `btnCalibrate` | `Recalibrates` | yes (`:183`) | ✓ codegen |
| `btnRPMTest` | `MeasuresRPM` | yes (`:185`) | ✓ codegen |
| `btnDetect` | `DetectsDrive` | yes (`:187`) | ✓ codegen |
| (write flux) | `WritesRawFlux` | yes (`:177`) | provider has it; no dedicated GUI button audited |
| (sector ops) | `ReadsSectors`/`WritesSectors` | **no** — negative `static_assert` `:191,194` | correctly absent |

The `wire_action` template (`wiring_runtime.h:196`) structurally enforces
H-3: a button bound to a capability the provider lacks compiles into the
**disabled** branch with a "Not supported by %1 (requires %2)" tooltip
(`:222–224`). GW satisfies all 7 of its declared capabilities → all 7
buttons take the enabled branch. No phantom buttons.

**Outcome handlers:** every `*Outcome` variant alternative has a matching
`HardwareTab::on*Outcome` overload (`hardwaretab.cpp:786–908`); a missing
one fails `std::visit` at compile time — verified the handler set covers
`Motor`/`Seek`/`Rpm`/`Detect`/`Flux` outcomes + the cross-variant
`ProviderError` / `HardwareDisconnected` / `CapabilityRequiresPolicy`.

**Status: PASS** (for Greaseweazle). The cross-provider problem — 8 other
providers are *not* routed (`currentProviderV2()` is hardcoded to the GW
type; `onConnect` shows "routing pending (P1.18)" for everything else,
`hardwaretab.cpp:624–645`) — is a **MASTER_REPORT** architectural finding,
not a Greaseweazle defect.

---

## D4 — OS-Detection

| OS | Mechanism | Evidence | Status |
|----|-----------|----------|--------|
| **Windows** | Qt `QSerialPortInfo::availablePorts()` when `UFT_HAS_SERIALPORT`; else `HKLM\HARDWARE\DEVICEMAP\SERIALCOMM` registry enum. C HAL: `CreateFile` + `DCB` + `COMMTIMEOUTS` (`uft_greaseweazle_full.c:155–263`), port pattern `COM%d` (`:473`). VID/PID hint `0x1209/0x4D69` (`hardwaretab.cpp:447`). | code path complete both Qt-side and C-HAL-side | **PASS** |
| **Linux** | Qt `QSerialPortInfo`; C HAL `open(port, O_RDWR\|O_NOCTTY\|O_NONBLOCK)` (`:276`), enum patterns `/dev/ttyACM`, `/dev/ttyUSB` (`:479`), `/dev/greaseweazle` udev symlink with `realpath()` de-dup (`:486–492`), plus a `readdir()` scan of `/dev` (`:499`). | code path complete; udev symlink honoured | **PASS** |
| **macOS** | Qt `QSerialPortInfo::availablePorts()` works on macOS. **But** the C HAL standalone enumeration pattern list is only `{"/dev/ttyACM","/dev/ttyUSB"}` (`:479`) — macOS devices enumerate as `/dev/cu.usbmodem*` / `/dev/tty.usbmodem*`, which that list does **not** match. | Qt-side: works. C-HAL-side: pattern list omits macOS names. | **PARTIAL** |

**Finding GW-D4-1** (medium): the C HAL's pattern-based port enumeration
(`uft_greaseweazle_full.c:479`) has no macOS device-name pattern. On
macOS, GW detection works only through the Qt `QSerialPortInfo` path; any
code path that relies on the C HAL enumerating ports itself (CLI, tests)
will not find a Greaseweazle on macOS. Cross-check: a real GW on macOS
appears as `/dev/cu.usbmodem*`.

**Status: PARTIAL** — Win/Linux PASS, macOS PARTIAL (Qt path works, C HAL
enum gap).

---

## D5 — Integritäts-Befunde

| ID | Issue | Severity | Code citation |
|----|-------|----------|---------------|
| GW-D5-1 | Simulated-connection fallback sets `m_firmwareVersion = "Simulated"` and marks the device connected when `UFT_HAS_HAL` is undefined. It **is** labelled `[SIMULATED]` in the status bar — not a silent fake — but a build without HAL presents a "connected" device with no hardware. | medium | `hardwaretab.cpp:709–729` |
| GW-D5-2 | `printf("=== onConnect called ===")` + `fflush(stdout)` debug scaffolding left in the connect path. Noise, not a correctness issue. | low | `hardwaretab.cpp:575–578, 649` |
| GW-D5-3 | `raw_handle()` escape hatch exposes the owned `uft_gw_device_t*` so legacy `FluxCaptureJob`/`FluxWriteJob` can bypass the V2 outcome surface. Documented + tracked ("SCHEDULED FOR REMOVAL P1.20/P1.21"). | low (documented) | `greaseweazle_provider_v2.h:122–130` |
| GW-D5-4 | `do_read_raw_flux` sample-frequency fallback chain ends at the `72 MHz` `#define`. If both `flux->sample_freq` and `uft_gw_get_sample_freq()` return 0 on an F7-Plus (84 MHz), ns timing is mis-scaled ~14%. Last-resort only; GW-F3 (task #112) made sample-freq dynamic, so both runtime queries normally succeed. | medium (degraded, not silent-fake) | `greaseweazle_provider_v2.cpp:234–245` |

**Positives (forensic-integrity holds):**
- No swallowed errors — every `uft_gw_*` return code is checked and
  translated to a typed outcome (`gw_err_to_provider_error`,
  `:118–177`).
- Rule F-4: every `ProviderError` carries non-empty what/why/fix; the
  `ProviderError` ctor throws on empty strings.
- Rule F-3: `do_read_raw_flux` copies every flux sample verbatim,
  reports `FluxMarginal` rather than fabricating data on empty capture.
- No stub façade — every `do_*` method has a real C-API call behind it;
  this is not a structured stub.
- Null-handle construction is honest: every `do_*` returns
  `HardwareDisconnected`, never a fake success.

**Status: PASS with findings** — no critical (P0) integrity violation;
GW-D5-1 and GW-D5-4 are medium and worth fixing.

---

## Fixes (prioritised)

**P0:** none.

**P1:**
- **GW-D2-1** — route `FluxCaptured` from `onFluxOutcome` into the flux
  decoder / format registry, or land P1.20/P1.21 to remove the
  `raw_handle()` escape hatch. Until then the type-driven read path
  cannot persist data. *(blocks: P1.20/P1.21 in REFACTOR_TASKS.md)*

**P2:**
- **GW-D4-1** — add macOS device-name patterns (`/dev/cu.usbmodem*`,
  `/dev/tty.usbmodem*`) to the C HAL port enumeration list
  (`uft_greaseweazle_full.c:479`). Protected path — needs sign-off.
- **GW-D5-4** — when the sample-frequency fallback hits the static
  `#define`, emit a loud warning into the audit trail rather than
  silently scaling at 72 MHz.
- **GW-D1-1** — vendor `keirf/greaseweazle inc/cmd.h` and confirm
  whether `CMD_READ_MEM/WRITE_MEM/GET_INFO_EXT` are a real extension.
  Upgrades D1 from `recalled` to `vendored`.

**P3:**
- **GW-D5-2** — remove the `printf` debug scaffolding from `onConnect()`.
- **GW-D5-1** — gate the simulated-connection path behind an explicit
  developer flag, or remove it; a "connected" device with no hardware is
  a forensic foot-gun even when labelled.

---

## Reproduce

```bash
cd audit/greaseweazle
python extract_uft.py        # UFT-side constants (JSON)
python extract_ref.py        # official reference (JSON)
python diff.py               # writes evidence.json, prints verdict, exit!=0 on FAIL
"C:/Qt/Tools/mingw1310_64/bin/gcc.exe" -std=c11 -I../../include \
    -fsyntax-only test_greaseweazle_vectors.c   # D1 build-gate
```

Source reads behind this report:
```bash
git grep -nE "UFT_GW_CMD_|UFT_GW_ACK_|UFT_GW_USB_" include/uft/hal/uft_greaseweazle_full.h
git grep -nE "uft_gw_command|serial_open|/dev/tty" src/hal/uft_greaseweazle_full.c
sed -n '196,278p' src/hardware_providers/greaseweazle_provider_v2.cpp   # do_read_raw_flux
sed -n '241,268p;622,696p' src/hardwaretab.cpp                          # routing
```

## Not reviewed
- Byte-exact frame layout of `uft_gw_command()` (opcode-level only — GW-D1-2).
- `do_write_raw_flux` GUI path (no dedicated write button audited in HardwareTab).
- The C HAL's `uft_gw_read_track` internals beyond the opcode it sends.
- Runtime behaviour against real hardware — that is HIL (`tests/hil/`, P3.4).

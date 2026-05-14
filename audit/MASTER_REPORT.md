# UFT Hardware-Provider Audit — Master Report

Cross-provider synthesis of the forensic audit of all 9 V2 hardware
providers in `src/hardware_providers/*_provider_v2.{h,cpp}` on branch
`refactor/type-driven-hal`.

Per-provider detail: `audit/<provider>/REPORT.md`. Methodology + the
V1→V2 scope correction: `audit/README.md`.

---

## Verdict matrix

D-grades per the 5 dimensions (D1 wire-protocol, D2 datapath, D3 GUI
integration, D4 OS-detection, D5 forensic integrity).

| Provider | Verdict | D1 | D2 | D3 | D4 | D5 | LOC | Integration |
|----------|---------|----|----|----|----|----|-----|-------------|
| Greaseweazle | **PARTIAL** | PASS | PARTIAL | PASS | PARTIAL | PASS◆ | 906 | native (C-API) |
| SCP | **PARTIAL** | UNVERIFIED | PARTIAL | FAIL | PARTIAL | PASS◆ | ~ | native (libusb, M3.1 scaffold) |
| KryoFlux | **PARTIAL** | UNVERIFIED | **FAIL** | FAIL | PARTIAL | PASS◆ | ~ | CLI wrapper (DTC) |
| FluxEngine | **PARTIAL** | PASS | **FAIL** | FAIL | PARTIAL | PASS◆ | ~ | CLI wrapper (fluxengine) |
| FC5025 | **PARTIAL** | UNVERIFIED | PARTIAL | FAIL | UNVERIFIED | PASS◆ | 765 | injected runner (no prod site) |
| XUM1541 | **PARTIAL** | PASS | PARTIAL | FAIL | PARTIAL | PASS | ~ | native (libusb, M3.2 scaffold) |
| Applesauce | **PARTIAL** | PARTIAL | PARTIAL | FAIL | PARTIAL | PASS | ~ | native (serial, M3.3 scaffold) |
| ADF-Copy | **PARTIAL** | PASS | PARTIAL | FAIL | PARTIAL | PASS◆ | ~ | native (serial) |
| USB-Floppy | **PARTIAL** | PASS | PARTIAL | FAIL | **FAIL** | FAIL◆ | ~ | native (SG_IO/UFI) |

◆ = "PASS/FAIL with findings" — see the provider's D5 table.

**D3 normalisation:** the per-provider reports grade D3 for unrouted
providers inconsistently (some PARTIAL, some FAIL). The master report
normalises to **FAIL** for all 8 non-Greaseweazle providers: production
routing is *wholly absent*, and a sound capability-`static_assert` is a
property of the provider type, not of an integration that does not
exist. Greaseweazle alone is D3 PASS.

**Every provider is PARTIAL.** None is a clean PASS (the type-driven
refactor's GUI + datapath halves are unfinished — P1.18/P1.20/P1.21).
None is a FAIL (no provider fabricates success — see ARCH-0 positive).

---

## Cross-provider findings

### ARCH-0 — POSITIVE: the honest-scaffold pattern holds everywhere

The audit's central worry — the brief's "XUM1541-Antipattern: 1114 LOC
structured stub" that fakes success — **does not occur in the V2
layer.** Every one of the 9 providers, on every `do_*` path, returns a
typed `ProviderError` / `HardwareDisconnected` / `*Unreadable` outcome
when its handle/runner/transport is null or unavailable. M3.2/M3.3
scaffolds (XUM1541, Applesauce) carry explicit "pending" markers in the
error text. Rule F-4 (non-empty what/why/fix) is enforced by the
`ProviderError` constructor throwing on empty strings. Capability
`static_assert`s (positive *and* negative) are sound in all 9. The
compile-time guarantees of the type-driven design hold.

This is why no provider is graded FAIL.

### ARCH-1 — P1: 8 of 9 providers are GUI-unreachable

`HardwareTab::currentProviderV2()` (`src/hardwaretab.cpp:249`) is
hardcoded to return `::uft::hal::GreaseweazleProviderV2*`.
`forms/tab_hardware.actions.yaml` lists only `greaseweazle_provider_v2.h`
in `extra_includes`. Every `wire_action<cap::X>` instantiates against the
Greaseweazle type alone. Selecting any other controller and pressing
Connect dead-ends in the "Controller routing pending (P1.18)" messagebox
(`src/hardwaretab.cpp:624-645`).

Consequence: SCP, KryoFlux, FluxEngine, FC5025, XUM1541, Applesauce,
ADF-Copy and USB-Floppy — all conformance-tested, all passing the
65-section harness — have **zero production reachability**. This is the
single largest finding and the root cause of every D3 FAIL. It is a
known refactor task (P1.18); the audit quantifies its blast radius: it
gates the entire non-GW datapath (see ARCH-4).

### ARCH-2 — P0-on-integration: `transitions_ns` type-contract violation (KryoFlux + FluxEngine)

Both CLI-wrapper flux providers pack **undecoded backend container
bytes** into `FluxCaptured::transitions_ns` as raw little-endian
`uint32_t` words:
- KryoFlux: `src/hardware_providers/kryoflux_provider_v2.cpp:316-345` —
  raw KryoFlux stream-file opcode bytes.
- FluxEngine: `src/hardware_providers/fluxengine_provider_v2.cpp:330-354`
  — undecoded `.flux` container bytes (`bytes_to_words()`).

The field's contract (`include/uft/hal/concepts.h`, `FluxStream` /
`FluxCaptured::transitions_ns`) is **nanosecond transition intervals**.
A type-trusting downstream consumer reads container bytes as flux
timing — fabricated data semantics, i.e. "stille Veränderung". Both
providers' code comments openly describe the re-interpretation, which
makes it *honest* but not *correct*.

Today nothing breaks at runtime only because ARCH-1 leaves the path
unwired. The moment P1.18 routes these providers, the decoder receives
garbage timing. Classified **P0-on-integration**: it must be fixed
before, or together with, ARCH-1 — not after.

Compounding (P1): both read from `result.stdout_text`
(`kryoflux_provider_v2.cpp` ~309, `fluxengine_provider_v2.cpp` ~464),
but real DTC / fluxengine write stream files to disk and print only a
log to stdout. Both read paths therefore work **only against the audit
mock** — there is no production runner that does file I/O.

### ARCH-3 — P1: USB-Floppy C-HAL is a runtime-dead façade

No UFI platform backend exists: `src/hal/ufi_linux.c`, `ufi_win.c`,
`ufi_mac.c` are all absent. `uft_ufi_backend_init()` is a stub at
`src/core/uft_core_stubs.c:443-445` that does `return -1;` and never
calls `uft_ufi_set_backend()`, so the backend ops table stays NULL and
every UFI call returns `UFT_ERR_NOT_IMPLEMENTED` (`ufi.c:19-26`).
USB-Floppy cannot read or write a sector on any OS. Honestly labelled
(hence not a FAIL-by-fabrication), but non-functional.

Sub-finding (P2): **ABI mismatch** — `ufi.h:58` declares
`void uft_ufi_backend_init(void)`; `uft_core_stubs.c:443` defines it
returning `int`. The `-1` failure signal is unreachable through the
public header.

### ARCH-4 — P1: no production construction sites for the non-GW providers

FC5025, XUM1541, Applesauce and SCP have **no production code that
constructs the provider with a real runner/transport** — only `tests/`
instantiates them (`git grep <Provider>V2 src/` finds declarations and
the V2 files themselves, no wiring). Combined with ARCH-1, the entire
non-Greaseweazle provider layer is test-only. The datapaths are correct
*in isolation* (D2 PARTIAL across the board: faithful translation, no
consumer).

### ARCH-5 — P2: fabricated default geometry / RPM

- SCP, KryoFlux, FluxEngine return `DriveDetected` / `RpmMeasured`
  carrying a **defaulted `rpm_nominal = 300.0`** indistinguishable from a
  measured value (SCP-D5-2/4, KF-D5-3, FE-D5-2).
- FC5025 `do_detect_drive` **hardcodes** `tracks=40, heads=2,
  rpm_nominal=300.0` (`fc5025_provider_v2.cpp:359-361`) regardless of
  what the runner reported — an 8" 77-track/360-RPM drive is still
  reported as 40/2/300.

Borderline "keine erfundenen Daten": the values are defaults, not
measurements, but a report consumer cannot tell them apart.

### ARCH-6 — P2: dangling references to V1 files deleted in P1.18

- XUM1541 / Applesauce C-HAL stub error strings point callers at
  `src/hardware_providers/*HardwareProvider` files that P1.18 deleted
  (XUM-D5-2, AS-D5-2).
- ADF-Copy doc-comments cite the deleted `adfcopyhardwareprovider.h` as
  provenance — harmless (comments only).
- **The brief's premise was wrong:** the original audit brief claimed
  `adfcopy_provider_v2` still `#include`s the V1 `hardwareprovider.h`.
  Verified false — `git grep '#include.*hardwareprovider' src/` returns
  **zero matches**; no V2 provider includes the V1 header. The V1
  `class HardwareProvider` (`include/uft/hardwareprovider.h:98`) still
  physically exists but is included by nothing in `src/` — it is dead
  code, not a live dependency.

### ARCH-7 — P2: VID/PID inconsistencies

- **SCP:** header `0x16C0/0x0753` vs GUI hint `0x16D0/0x0F8C`
  (`hardwaretab.cpp:449`) — the two disagree.
- **XUM1541:** `hardwaretab.cpp:453` labels `0x16D0:0x0504` as
  "ZoomFloppy/XUM1541", but per `xum1541_usb.h` that PID is the
  *XUM1541*; a real ZoomFloppy (`0x04B2`) gets no port hint.
- **ADF-Copy:** `0x16C0:0x0483` is the generic PJRC Teensy ID, shared
  with Applesauce — the MF-146 disambiguation the brief expected does
  **not** exist in code.
- **Greaseweazle:** consistent (`0x1209/0x4D69` header == GUI hint).

### ARCH-8 — P3: D1 reference provenance is universally weak

Only USB-Floppy produced a clean diff (`18 PASS / 0 FAIL` — UFI rides
standardised SCSI opcodes). Every other provider's wire protocol is
graded `recalled` or `needs-source`: no official protocol source is
vendored anywhere in the repo. SCP is the sharpest case — SCP-D1-1
flags that the 6 USB command bytes cite `a8rawconv`, not the vendor SCP
SDK, and may not match the real opcode space. Upgrading the references
from `recalled` → `vendored` is required before any D1 diff can be a
real conformance gate.

### ARCH-9 — P3: macOS gaps

- Greaseweazle C-HAL port enumeration omits `/dev/cu.usbmodem*` /
  `/dev/tty.usbmodem*` (`uft_greaseweazle_full.c:479`) — GW found on
  macOS only via the Qt `QSerialPortInfo` path.
- XUM1541 `OpenCbmLibrary::load()` (`xum1541_usb.h:365-368`) has no
  `.dylib` path — XUM1541 is dead on macOS.

---

## Prioritised fix list

### P0 — fix before / together with P1.18 routing

- **ARCH-2** — KryoFlux + FluxEngine must **decode** their backend
  containers into real nanosecond transition intervals before populating
  `FluxCaptured::transitions_ns`, or change the outcome type to carry
  an undecoded-container alternative. Routing these providers (P1.18)
  without this fix feeds the decoder fabricated timing. *Files:*
  `kryoflux_provider_v2.cpp:316-345`, `fluxengine_provider_v2.cpp:330-354`.

### P1 — breaks under specific conditions / blocks the refactor

- **ARCH-1 / ARCH-4** — land P1.18: generalise `currentProviderV2()` to
  all 9 provider types, add a production construction site per provider,
  extend `forms/tab_hardware.actions.yaml`. Until then 8 providers are
  unreachable and untested end-to-end.
- **ARCH-3** — implement at least one UFI platform backend
  (`ufi_linux.c` is the obvious first) and have `uft_ufi_backend_init()`
  register it; fix the `void`-vs-`int` ABI mismatch (`ufi.h:58` vs
  `uft_core_stubs.c:443`).
- **ARCH-2 (compounding)** — give KryoFlux + FluxEngine a production
  runner that reads the tool's *stream file*, not `stdout_text`.
- **GW-D2-1** — route `FluxCaptured` from `HardwareTab::onFluxOutcome`
  into the flux decoder, or land P1.20/P1.21 to remove the
  `raw_handle()` escape hatch (today the GW V2 read path discards data).

### P2 — correctness / honesty hardening

- **ARCH-5** — no provider may emit a defaulted geometry/RPM that a
  consumer cannot distinguish from a measurement. Either carry an
  `is_measured` flag or report "unknown". FC5025's hardcoded 40/2/300
  (`fc5025_provider_v2.cpp:359-361`) is the worst case.
- **ARCH-7** — reconcile the SCP header-vs-GUI VID/PID disagreement;
  correct the XUM1541/ZoomFloppy PID label; add Applesauce↔ADF-Copy
  disambiguation (the MF-146 the brief assumed exists).
- **ARCH-6** — update XUM1541/Applesauce C-HAL error strings to stop
  pointing at deleted V1 files; either delete the dead V1
  `include/uft/hardwareprovider.h` or document why it survives.
- **GW-D4-1** — add macOS device-name patterns to the GW C-HAL port
  enumeration (protected path — needs sign-off).
- **SCP-D1-1** — verify the SCP USB command bytes against the vendor
  SCP SDK before the M3.1 libusb wiring lands; they currently cite
  `a8rawconv`, not the vendor reference.

### P3 — audit-quality + portability follow-ups

- **ARCH-8** — vendor the official protocol sources (greaseweazle
  `cmd.h`, SCP SDK, OpenCBM, fluxengine/DTC CLI grammars) and upgrade
  every D1 reference from `recalled` to `vendored`. Then the per-provider
  `diff.py` becomes a CI conformance gate.
- **ARCH-9** — XUM1541 `.dylib` load path; GW macOS port patterns.
- **GW-D1-1** — confirm whether `CMD_READ_MEM/WRITE_MEM/GET_INFO_EXT`
  (0x20-0x22) are a real GW protocol extension or a UFT invention.
- Per-provider P3 items — see each `REPORT.md`.

---

## How this maps to the refactor plan

| Finding | Existing task | Note |
|---------|---------------|------|
| ARCH-1, ARCH-4 | REFACTOR_TASKS.md **P1.18** | audit confirms scope: all 8 providers + construction sites |
| GW-D2-1 | **P1.20 / P1.21** | `raw_handle()` escape-hatch removal |
| ARCH-2 | *new* — no task yet | **P0-on-integration; should block P1.18 or ship with it** |
| ARCH-3 | M3 HAL plan (UFI not a HAL backend) | `docs/M3_HAL_PLAN.md` — UFI backend is unscheduled |
| SCP/XUM/AS scaffolds | M3.1 / M3.2 / M3.3 | honest scaffolds — audit confirms no fake success |

The audit's headline: **the type-driven HAL's *type layer* is sound
(ARCH-0) — its *integration layer* is unfinished (ARCH-1) and one
datapath shortcut is a latent forensic defect (ARCH-2).**

---

## Reproduce

```bash
# per provider:
cd audit/<provider> && python diff.py    # writes evidence.json, prints verdict
# native backends additionally:
"C:/Qt/Tools/mingw1310_64/bin/gcc.exe" -std=c11 -I../../include \
    -fsyntax-only test_<backend>_vectors.c
```

All 9 `diff.py` exit 0 (no FAIL rows — UNVERIFIED rows are device-reported
or needs-source, documented per provider). Audit performed on branch
`refactor/type-driven-hal`; provider sources read at the commit current
when `audit/` was created.

## Audit coverage / not done

- **Not done:** byte-exact wire-frame tracing (opcode-level only);
  runtime behaviour against real hardware (that is HIL — `tests/hil/`,
  P3.4); the internals of the C-HAL backends beyond the opcodes/paths
  cited; `tests/*_provider_v2.cpp` internals.
- **Reference grade:** every D1 except USB-Floppy is `recalled` or
  `needs-source` — see ARCH-8. No D1 verdict is `vendored`-grade.
- **9 of 9 providers** carry a graded verdict in every dimension; every
  UNVERIFIED states which test is missing and what would be needed.

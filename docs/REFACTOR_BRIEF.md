# Refactor Brief — Type-Driven HAL

**Branch:** `refactor/type-driven-hal`
**Goal:** Replace the dual-architecture GUI↔HAL pile with one type-driven system.
**Author:** Axel Fuchs (MysticFoxDE)
**Status:** Foundation landed (P0 complete). Provider migration pending (P1).

This document is the architecture spec the AI agents and human reviewers
work against. It is the source-of-truth for every decision during the
refactor — if a code change conflicts with this brief, the brief wins.

> Forensic mission rule remains supreme: **forensic correctness > the
> brief**. If a structural decision in this document would compromise
> "Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."
> — escalate before applying.

---

## 1. The problem we are solving

UFT today carries **two parallel hardware architectures** that do not
talk to each other:

- **World A** — the Hardware tab's action buttons (Motor on/off, Seek,
  Read, RPM, Calibrate) call the Greaseweazle C HAL (`uft_gw_*`)
  hard-coded, in 20+ places.
- **World B** — a full `HardwareProvider` class hierarchy with 10
  providers exists in `src/hardware_providers/`, used at exactly 3
  call-sites and only for `detectDrive()`.

Consequence: 13 of 14 controllers selectable from the GUI silently
no-op when the user clicks Motor / Seek / Read / RPM / Calibrate.

Symptom: "the tool doesn't feel finished".
Cause: structural — disciplined runtime checks would patch it; we are
choosing the structural fix instead.

Detailed audit: see the `WIRING_AUDIT` artifact attached to the v4.1.x
release notes (or the Markdown captured in the matching commit body).

---

## 2. Architecture — five layers, one world

```
┌──────────────────────────────────────────────────────────────────┐
│  GUI (Qt Widgets, *.ui)                                          │
│  Layout: Qt Designer · Behavior: actions.yaml + codegen          │
└───────────────────────────────┬──────────────────────────────────┘
                                │ Compile-time-checked wiring
┌───────────────────────────────▼──────────────────────────────────┐
│  Outcome layer  (std::variant Sum-Types)                         │
│  SectorOutcome · FluxOutcome · MotorOutcome · SeekOutcome · ...  │
│  Forensic cases explicit:  Read · Marginal · Unreadable · Error  │
└───────────────────────────────┬──────────────────────────────────┘
                                │ Concept-constrained calls
┌───────────────────────────────▼──────────────────────────────────┐
│  Provider layer (mixin composition over capability traits)       │
│  Greaseweazle, SCP, KryoFlux, FluxEngine, FC5025, XUM1541,       │
│  Applesauce, ADFCopy, USBFloppy, Mock                            │
└───────────────────────────────┬──────────────────────────────────┘
                                │ Backend-specific implementations
┌───────────────────────────────▼──────────────────────────────────┐
│  Backend layer                                                   │
│  uft_gw_* C-API · libusb wrappers · subprocess adapters (DTC,    │
│  fcimage) · QSerialPort wrappers (Applesauce, ADF-Copy)          │
└───────────────────────────────┬──────────────────────────────────┘
                                │ OS / driver API
┌───────────────────────────────▼──────────────────────────────────┐
│  OS · Hardware                                                   │
└──────────────────────────────────────────────────────────────────┘
```

The C HAL fast-path (`uft_gw_*`) does not disappear — it becomes the
backend implementation of `GreaseweazleProvider`. There is no parallel
architecture after the refactor.

---

## 3. Sum-type outcomes (rule F-3, F-4 type-enforced)

`bool success` cannot encode the difference between "16 sectors all
read", "5 retries, divergent data preserved", "track is physically
dead", "operation requires --accept-data-loss", "USB cable was pulled",
or "spec violation, here is what/why/fix".

Each HAL operation returns a `std::variant`. Consumers `std::visit` it.
If a new variant alternative is later added (e.g. `WeakBitsDetected`),
every consumer that doesn't handle it fails to compile — the forensic
mission becomes a compile-time guarantee.

Variants live in `include/uft/hal/outcomes.h`:

| Outcome           | Variants                                                                    |
|-------------------|-----------------------------------------------------------------------------|
| `SectorOutcome`   | `SectorRead`, `SectorMarginal`, `SectorUnreadable`, `CapReqPolicy`, ...     |
| `FluxOutcome`     | `FluxCaptured`, `FluxMarginal`, `FluxUnreadable`, `CapReqPolicy`, ...       |
| `WriteOutcome`    | `WriteCompleted`, `WriteVerifyFailed`, `WriteRefused`, `CapReqPolicy`, ...  |
| `MotorOutcome`    | `MotorRunning`, `MotorStopped`, `MotorStalled`, ...                         |
| `SeekOutcome`     | `SeekArrived`, `SeekOvershot`, `SeekTrack0Failed`, ...                      |
| `RpmOutcome`      | `RpmMeasured`, ...                                                          |
| `DetectOutcome`   | `DriveDetected`, `DriveAbsent`, ...                                         |

> **MF-194:** `FluxCaptured` carries `index_times_ns` —
> `std::vector<std::uint32_t>` of per-revolution index-pulse boundaries
> (cumulative ns). Empty = "provider observed no index pulses", not
> "one revolution". Additive extension; design rationale in
> `docs/proposals/FLUXCAPTURED_INDEX_BOUNDARIES.md`. Unblocks P1.20.

`ProviderError` is the spec-violation/error variant carried by every
outcome. Its constructor enforces three non-empty fields:
`what`, `why`, `fix` (rule F-4 type-enforced).

`SectorMarginal` and friends preserve every divergent read sample
verbatim (rule F-3) — never substitute, never average, never discard.

---

## 4. Capability concepts (rule H-1, H-2 structurally enforced)

In `include/uft/hal/concepts.h`. A concept is a set of method
signatures the type must have. A provider satisfies the concept by
having those methods, not by inheriting a base or setting a bitflag.

| Concept           | Required method                                              | Returns         |
|-------------------|--------------------------------------------------------------|-----------------|
| `HasIdentity`     | `display_name()`, `spec_status()`                            | view, SpecStatus|
| `ReadsSectors`    | `read_sector(const ReadSectorParams&)`                       | `SectorOutcome` |
| `ReadsRawFlux`    | `read_raw_flux(const ReadFluxParams&)`                       | `FluxOutcome`   |
| `WritesSectors`   | `write_sector(const WriteSectorParams&, const SectorPayload&)` | `WriteOutcome`|
| `WritesRawFlux`   | `write_raw_flux(const WriteFluxParams&, const FluxStream&)`  | `WriteOutcome`  |
| `ControlsMotor`   | `set_motor(bool)`                                            | `MotorOutcome`  |
| `SeeksHead`       | `seek(int)`                                                  | `SeekOutcome`   |
| `Recalibrates`    | `recalibrate()`                                              | `SeekOutcome`   |
| `MeasuresRPM`     | `measure_rpm()`                                              | `RpmOutcome`    |
| `DetectsDrive`    | `detect_drive()`                                             | `DetectOutcome` |

Composite predicates (read-only sugar): `ImagesFlux`, `ImagesSectors`,
`WritesAnything`, `FullDriveControl`.

`FC5025Provider`, being read-only, simply does NOT have
`write_raw_flux()`. Calling it is a compile error, not a runtime
no-op. Rule H-2 ("no `return false; Q_UNUSED(...)`") becomes moot —
the offending pattern cannot be expressed because there is no base
class with virtual stubs.

---

## 5. Provider composition (rule H-2 implementation pattern)

A provider inherits one capability mixin per capability it has:

```cpp
class GreaseweazleProvider final
  : public mixin::Identity<"Greaseweazle", SpecStatus::VendorDocumented>
  , public mixin::ReadsRawFluxVia<GreaseweazleProvider>
  , public mixin::WritesRawFluxVia<GreaseweazleProvider>
  , public mixin::ControlsMotorVia<GreaseweazleProvider>
  , public mixin::SeeksHeadVia<GreaseweazleProvider>
  , public mixin::RecalibratesVia<GreaseweazleProvider>
  , public mixin::MeasuresRPMVia<GreaseweazleProvider>
  , public mixin::DetectsDriveVia<GreaseweazleProvider>
{
public:
    explicit GreaseweazleProvider(uft_gw_device_t* h) : m_handle(h) {}

    /* Backend bindings — the mixins call these via CRTP downcast.
     * Each translates the result of a uft_gw_* call into the matching
     * Outcome variant, preserving every byte the device sent. */
    FluxOutcome   do_read_raw_flux (const ReadFluxParams&);
    WriteOutcome  do_write_raw_flux(const WriteFluxParams&, const FluxStream&);
    MotorOutcome  do_set_motor     (bool);
    SeekOutcome   do_seek          (int);
    SeekOutcome   do_recalibrate   ();
    RpmOutcome    do_measure_rpm   ();
    DetectOutcome do_detect_drive  ();

private:
    uft_gw_device_t* m_handle;
};

class FC5025Provider final
  : public mixin::Identity<"FC5025", SpecStatus::VendorDocumented>
  , public mixin::ReadsSectorsVia<FC5025Provider>
  , public mixin::DetectsDriveVia<FC5025Provider>
{
    /* Read-only — no Write*, no Motor, no Seek mixins. The compiler
     * refuses any call to those methods on this type. */
};
```

---

## 6. GUI wiring is generated, not hand-written (rule H-3, H-4)

`forms/tab_hardware.actions.yaml` declares each button:
- the widget name (must match a `*.ui` widget),
- the capability concept it requires,
- the method to invoke,
- the per-outcome-variant dispatch.

`tools/wiring_codegen.py` consumes the YAML + the `.ui` file and emits
`generated/tab_hardware_wiring.gen.cpp`. The generated code uses
`wire_action<Concept>(button, lambda)`, a template that:
- instantiates only if the bound provider satisfies the concept,
- otherwise sets the button to disabled with an explanatory tooltip.

Consequence: a YAML entry referencing a method no provider implements
**fails the build at codegen-time**. There can be no UI button for an
operation no backend can perform. Rule H-3 is structural.

---

## 7. Conformance tests, generated (rule T-2 + H-10)

`tests/hal_conformance.cpp` is a `TEMPLATE_TEST_CASE` over all
provider types. Per concept, an `if constexpr` SECTION invokes the
operation against a mock backend and asserts the outcome matches
expectation. Mock-only — real-hardware verification is the human's
responsibility (`tests/HARDWARE_TRUTH_TESTS.md`).

A new provider added to the typename list inherits all conformance
sections automatically — no per-provider test boilerplate.

---

## 8. What this refactor explicitly does NOT do

- **Does not change the `uft_gw_*` C-API signatures.** They are
  production-tested. They become the backend implementation of
  `GreaseweazleProvider::do_*` and stop being directly called from GUI
  code.
- **Does not touch `tests/golden/`.** Golden disk-image snapshots are
  forensic ground truth; rebuilding them would invalidate every
  decoder regression check.
- **Does not modify `docs/DESIGN_PRINCIPLES.md`.** That document is
  constitutional — it constrains the refactor, not the other way around.
- **Does not promise complete spec coverage for reverse-engineered
  formats** (IPF, STX, proprietary JP). `SpecStatus::ReverseEngineered`
  is a type-marker; correctness still depends on real-disk testing.

---

## 9. Definition of done for the refactor

| Check                                                           | Pass iff                                  |
|-----------------------------------------------------------------|-------------------------------------------|
| `git diff main` shows old `HardwareProvider` base removed       | true                                      |
| `git diff main` shows `m_gwDevice` removed from hardwaretab.cpp | true                                      |
| `grep "uft_gw_" src/hardwaretab.cpp` count                      | 0                                         |
| `grep "Q_UNUSED" src/hardware_providers/`                       | 0                                         |
| `grep "\.success" src/`                                         | < 5 (only in legacy test fixtures)        |
| `cmake --build` on Linux + macOS + Windows MinGW                | green                                     |
| `ctest` count vs baseline (post-MF-149)                         | ≥ baseline + new conformance tests        |
| `tests/HARDWARE_TRUTH_TESTS.md` boxes                           | all checked by Axel against real hardware |
| `KNOWN_ISSUES.md` M.-2 (rule H-9) entry                         | CLOSED (MF-174). H-1 / H-2 never had own entries — structural in V2 from day one |
| `git tag v4.1.4-rc1`                                            | set; +14 day pre-release window starts    |

After 14 days of community/own pre-release testing:
- squash-merge `refactor/type-driven-hal` into `main`,
- tag `v4.1.4`,
- bump version-of-record in `VERSION.txt`,
- delete branch.

> **Version-bump rationale (decision 2026-05):** the refactor's
> behavioral changes for end users are limited to (a) the Greaseweazle
> workflow being structurally hardened (same UX, more honest backend),
> (b) phantom-feature combo entries removed (X1541, MF-170), and
> (c) non-GW controllers are routed to their V2 providers with
> honest-stub backends (P1.23, MF-205/MF-206) — capability buttons gate
> correctly and any action surfaces a diagnostic `ProviderError` until
> the M3.x production transports land, instead of silently no-op'ing.
> UFT is an end-user application, not a
> linked library — no external SemVer contract is in play. PATCH bump
> `v4.1.3 → v4.1.4` reflects "incremental hardening of the v4.1 line"
> truthfully and avoids the psychological overhead of a MAJOR bump
> when no working user workflow is broken. The RELEASE_NOTES section
> "Internal refactor (Type-Driven HAL)" makes the scope of internal
> change explicit for anyone who cares to look.

---

## 10. Why now

1. Every week the dual world remains, both grow further apart. Today
   it is 20+ hard-coded `uft_gw_*` call sites in the GUI; in six
   months it would be 40.
2. The forensic mission needs type-system guarantees, not discipline.
   "Kein Bit verloren" is a behavior promise; the only reliable way to
   keep it is to lift it into the type system where the compiler
   enforces it.
3. The next provider (UFI / USB Floppy) is in pipeline. Bringing it
   into today's architecture multiplies the problem; bringing it into
   the post-refactor architecture is ~200 lines of mixin composition
   plus ~50 lines of backend wrapper.

---

## 11. References

- `include/uft/hal/outcomes.h` — sum-type definitions
- `include/uft/hal/concepts.h` — capability concepts
- `include/uft/hal/mixins.h` — capability mixin templates
- `tests/test_hal_foundation.cpp` — foundation compile + runtime smoke
- `docs/REFACTOR_TASKS.md` — task sequence
- `tests/HARDWARE_TRUTH_TESTS.md` — manual real-hardware verification
- `docs/DESIGN_PRINCIPLES.md` — constitutional rules
- `memory/coding_standards.md` (in user's local agent memory) — Master
  Coding Standards v1.0

# P1.23 — Route non-GW controllers through their V2 providers (variant dispatch)

Status: PROPOSAL — not applied. Owner: type-system-architect.
Audit refs: ARCH-1 (non-GW V2 providers GUI-unreachable), ARCH-4 (no production
construction sites). Task: REFACTOR_TASKS.md P1.23.

---

## 0. Problem statement (verified against current code)

- `HardwareTab::currentProviderV2()` (`src/hardwaretab.cpp:248-256`) is hardcoded
  to `return m_gwProviderV2.get();` — return type is the concrete
  `::uft::hal::GreaseweazleProviderV2*`.
- `HardwareTab::onConnect()` (`src/hardwaretab.cpp:621-642`) rejects every
  `controller != "greaseweazle"` with a `QMessageBox` "Controller routing
  pending (P1.18)" and returns. P1.18 is in fact done — that message is stale.
- The 9 V2 providers (`src/hardware_providers/*_provider_v2.{h,cpp}`) are each
  `final`, **share no base class** (deliberate: capabilities are type-properties,
  not virtual methods), each has a **different capability set** and a
  **different construction shape**.

### Capability + construction matrix (read from the headers)

| Provider | Capabilities (mixins) | Construction shape |
|---|---|---|
| `GreaseweazleProviderV2` | ReadsRawFlux, WritesRawFlux, ControlsMotor, SeeksHead, Recalibrates, MeasuresRPM, DetectsDrive | `open(port)` / ctor `(uft_gw_device_t*, drive_unit)` — **owns** C handle |
| `SCPProviderV2` | ReadsRawFlux, WritesRawFlux, DetectsDrive | ctor `(uft_scp_direct_ctx_t*)` — handle **not owned**; M3.1 scaffold |
| `KryoFluxProviderV2` | ReadsRawFlux, DetectsDrive | ctor `(DtcRunner, dtc_binary)` — injected subprocess runner |
| `FluxEngineProviderV2` | ReadsRawFlux, WritesRawFlux, MeasuresRPM, DetectsDrive | ctor `(FluxEngineRunner, fe_binary, max_cyl, profile)` — injected runner |
| `FC5025ProviderV2` | **ReadsSectors**, DetectsDrive | ctor `(Fc5025Runner, Fc5025DetectRunner, max_cyl)` — injected runners |
| `XUM1541ProviderV2` | **ReadsSectors**, **WritesSectors**, DetectsDrive | ctor `(Xum1541Runner, Xum1541WriteRunner, Xum1541DetectRunner, max_cyl, …)` |
| `ApplesauceProviderV2` | ReadsRawFlux, WritesRawFlux, ControlsMotor, SeeksHead, Recalibrates, MeasuresRPM, DetectsDrive | ctor `(ApplesauceReadRunner, …Write, …Motor, …Seek, …)` — M3.3 scaffold |
| `ADFCopyProviderV2` | ReadsRawFlux, ControlsMotor, SeeksHead, Recalibrates, DetectsDrive | ctor `(ADFCopyReadRunner, …Motor, …Seek, …Recal, …)` |
| `USBFloppyProviderV2` | **ReadsSectors**, **WritesSectors**, DetectsDrive | ctor `(UsbFloppyReadRunner, …Write, …Detect, device_path)` — UFI backend P1.25, **does not exist** |

**Critical pre-existing gap (surfaced, not introduced by P1.23):**
`forms/tab_hardware.actions.yaml` wires only flux/motor/seek/RPM/detect actions.
It has **no `ReadsSectors`/`WritesSectors` actions at all**. So three of the
nine providers (FC5025, XUM1541, USBFloppy) are sector devices whose *primary*
capability has no button in the Hardware tab. P1.23 can route them — every
flux button correctly greys out via `wire_action`'s false branch — but the
user gets a controller with **every action button disabled**. That is honest
(no mis-dispatch, no false capability) but it is a UX dead-end. See §4 + §6.

---

## 1. The dispatch type

### Decision: `std::variant` of `std::unique_ptr<ConcreteProviderV2>`, 9 alternatives

```cpp
// hardwaretab.h — replaces the single m_gwProviderV2 unique_ptr member.
// All 9 V2 headers are heavy; forward-declare and keep the variant in a
// pimpl-ish struct OR include the 9 headers in hardwaretab.cpp only and
// expose the variant via an opaque holder. Sketch shows the direct form.

#include <variant>
#include <memory>

namespace uft::hal {
    class GreaseweazleProviderV2; class SCPProviderV2; class KryoFluxProviderV2;
    class FluxEngineProviderV2;   class FC5025ProviderV2; class XUM1541ProviderV2;
    class ApplesauceProviderV2;   class ADFCopyProviderV2; class USBFloppyProviderV2;
}

// "no provider connected" is the std::monostate alternative — first, so a
// default-constructed variant means "disconnected".
using ProviderV2Variant = std::variant<
    std::monostate,
    std::unique_ptr<::uft::hal::GreaseweazleProviderV2>,
    std::unique_ptr<::uft::hal::SCPProviderV2>,
    std::unique_ptr<::uft::hal::KryoFluxProviderV2>,
    std::unique_ptr<::uft::hal::FluxEngineProviderV2>,
    std::unique_ptr<::uft::hal::FC5025ProviderV2>,
    std::unique_ptr<::uft::hal::XUM1541ProviderV2>,
    std::unique_ptr<::uft::hal::ApplesauceProviderV2>,
    std::unique_ptr<::uft::hal::ADFCopyProviderV2>,
    std::unique_ptr<::uft::hal::USBFloppyProviderV2>>;

// member:
ProviderV2Variant m_providerV2;   // replaces std::unique_ptr<GreaseweazleProviderV2> m_gwProviderV2
```

### Why variant-of-unique_ptr, not the alternatives

- **Type-erased wrapper (`std::any` / custom vtable):** rejected. A type-erased
  wrapper re-introduces runtime polymorphism through the back door — exactly
  what the type-driven design forbids (`wiring_runtime.h` even `static_assert`s
  `std::is_class_v<Provider>` to block base-class pointers). `wire_action<Cap>`
  is a template *per concrete type*; a type-erased provider cannot be a template
  argument. Non-starter.
- **Per-type members (9 `unique_ptr` members, one populated):** rejected. Nine
  members where eight are always null is the same information as a variant but
  without the "exactly one active" invariant enforced by the type system, and
  every consumer would need a 9-way `if` ladder. The variant *is* the
  per-type-members idea with the invariant made structural.
- **`std::variant` of raw pointers:** rejected on ownership grounds. The GW
  provider already owns its C handle via `unique_ptr` (header comment at
  `greaseweazle_provider_v2.h:264` calls the `unique_ptr` "the sole owner").
  Raw pointers in the variant would need a parallel ownership store. Putting
  the `unique_ptr` *inside* the variant keeps one owner, one lifetime.
- **`std::variant` of `unique_ptr` (chosen):** the variant's active alternative
  *is* the "which of the 9 is connected" fact — single source of truth. The
  `unique_ptr` gives RAII close/destruct. `std::monostate` models "disconnected"
  so `m_providerV2 = {}` is a clean disconnect. `std::visit` is exactly the
  dispatch primitive the codegen already uses for outcome variants — it is
  idiomatic in this codebase, not a new concept.

Note: several providers are non-movable (`SCPProviderV2`, `GreaseweazleProviderV2`
delete move). `std::unique_ptr<T>` is movable regardless of whether `T` is —
so the variant of `unique_ptr`s is movable/assignable even though the providers
are not. This is the second reason the `unique_ptr` must be *inside* the variant.

### Accessor change

`currentProviderV2()` returning a single concrete type can no longer exist as-is.
Replace it with an accessor that hands the **variant by reference** to the codegen:

```cpp
// hardwaretab.h
const ProviderV2Variant &currentProviderV2() const noexcept { return m_providerV2; }
```

The `provider_source:` line in the YAML stays literally `self->currentProviderV2()`
— but it now yields the variant, and the codegen visits it (see §2).

---

## 2. Codegen impact

### Output shape: each action's `wire_action<Cap>` call wrapped in one `std::visit`

Today the codegen emits (Phase 3, per action):

```cpp
wire_action<cap::ControlsMotor>(
    self->ui->btnMotorOn, provider,
    []<class P>(P *p) { return p->set_motor(true); },
    /* on MotorRunning: */ [self](const ::uft::hal::MotorRunning &v){ self->onMotorOutcome(v); },
    ... );
```

`provider` is a single `GreaseweazleProviderV2*`. With the variant, `provider`
becomes the variant and each `wire_action` call is wrapped in a `std::visit`
whose generic lambda receives **the concrete `P*` of whichever alternative is
active**:

```cpp
// Phase 2 — provider-null gate becomes a monostate check:
auto &providerVar = self->currentProviderV2();
if (std::holds_alternative<std::monostate>(providerVar)) {
    self->ui->btnMotorOn->setEnabled(false);
    /* ... every action button ... */
    return;
}

// Phase 3 — per action, visit the variant:
// widget=btnMotorOn  requires=ControlsMotor  invoke=set_motor(true)
std::visit(
    [&]<class Ptr>(Ptr &heldPtr) {
        if constexpr (std::is_same_v<Ptr, std::monostate>) {
            // unreachable after the Phase-2 guard, but std::visit must
            // be exhaustive — disable defensively.
            self->ui->btnMotorOn->setEnabled(false);
        } else {
            auto *p = heldPtr.get();          // concrete ConcreteProviderV2*
            ::uft::gui::wire_action<cap::ControlsMotor>(
                self->ui->btnMotorOn,
                p,                            // concrete type — template instantiates per provider
                []<class P>(P *q) { return q->set_motor(true); },
                /* on MotorRunning: */ [self](const ::uft::hal::MotorRunning &v){ self->onMotorOutcome(v); },
                /* ... all alternatives ... */);
        }
    },
    providerVar);
```

This is the **key property**: inside the `std::visit` lambda, `Ptr` is a
concrete `std::unique_ptr<SomeProviderV2>`, so `p` is a concrete
`SomeProviderV2*`. `wire_action<cap::ControlsMotor>` is instantiated **once per
provider type** (9 instantiations across all the visits). For a provider that
does **not** satisfy `ControlsMotor` (e.g. `SCPProviderV2`, `FC5025ProviderV2`),
`wire_action`'s `if constexpr (CapTag::satisfied<Provider>)` takes the **false
branch** — button disabled, tooltip "Not supported by SuperCard Pro (requires
ControlsMotor)". Capability gating stays 100% structural, exactly as the brief
requires; nothing about that mechanism changes. A FC5025 (no `WritesRawFlux`,
no `ReadsSectors`-button-yet) gets every flux/write button disabled by the
existing false branch — no runtime `if`, no special-casing.

### `tools/wiring_codegen.py` changes

Three localized changes, all in `emit()`:

1. **Phase 2 guard** — change the `if (!provider)` test to a monostate check:
   ```python
   parts.append(
       f"    auto &providerVar = {provider_source};\n"
       "    if (std::holds_alternative<std::monostate>(providerVar)) {\n" ...
   ```
2. **Phase 3 per-action** — wrap the existing `wire_action<...>(...)` emission
   in the `std::visit([&]<class Ptr>(Ptr &heldPtr){ if constexpr (monostate)
   ... else { auto *p = heldPtr.get(); wire_action<...>(self->ui->widget, p,
   ...); } }, providerVar);` shell. The *inner* emission (the
   `wire_action<cap::X>(...)` text, the generic-lambda factory, the per-alt
   handler lambdas) is **byte-for-byte the existing code** — only the wrapping
   `std::visit` shell and the `p` substitution for `provider` are new.
3. **`<variant>` include** — already pulled in transitively via
   `wiring_runtime.h`, but add an explicit `#include <variant>` to the emitted
   header block for clarity. Also `extra_includes:` in the YAML must list the
   8 new provider headers (see §5).

`KNOWN_CAPABILITIES` does **not** change — no new concept is introduced. The
codegen stays deterministic: the variant alternative order is fixed by the
`ProviderV2Variant` typedef, not by anything the codegen computes.

### Determinism / exhaustiveness

`std::visit` over the variant is exhaustive by construction — adding a 10th
provider later means adding a variant alternative, which makes every
`std::visit` lambda need to handle it (the generic `[&]<class Ptr>` lambda
handles any `Ptr` uniformly, so in practice a new provider "just works" once
the typedef is extended and a construction site is added). That is the same
forensic property the outcome-variant visits already have.

---

## 3. Construction strategy

### Principle

Selecting a controller and pressing Connect must do one of exactly two things:
(a) construct a real, transport-backed provider and store it in the variant, or
(b) refuse to connect with a clear explanation. It must **never** store a
provider that *claims* a capability its backend cannot honor, and it must
**never** silently no-op.

The good news: the type-driven design already separates **type shape** from
**backend maturity**. Every scaffold provider's `do_*` methods honestly return
a `ProviderError` with what/why/fix when the transport is null or unwired
(documented in every header — SCP "SCP USB I/O pending (M3.1)", Applesauce
"M3.3 marker", KryoFlux/FluxEngine null-runner → `ProviderError`, etc.). So a
scaffold provider is **not** "claiming a capability it can't deliver" — the
capability *concept* is satisfied (the type shape is real and correct), and the
*runtime outcome* is a forensically-truthful error. `wire_action` enables the
button (the capability genuinely exists as a type property) and the click
produces a visible `ProviderError` routed through `showProviderError()`. That
is honest: the button is reachable, the operation is attempted, the failure is
reported with what/why/fix. It is **not** a silent no-op and **not** a
mis-dispatch.

### Per-provider construction sites in `onConnect()`

Replace the `if (controller != "greaseweazle") { QMessageBox ...; return; }`
block with a dispatch on the controller key. Each branch constructs the
concrete provider, wraps it in `unique_ptr`, and assigns into `m_providerV2`.

| Controller key | Construction |
|---|---|
| `greaseweazle` | unchanged — `make_unique<GreaseweazleProviderV2>()` + `open(port)` |
| `scp` | `make_unique<SCPProviderV2>(nullptr)` — M3.1 scaffold; nullptr handle → honest `ProviderError` on every `do_*`. Real `uft_scp_direct_open()` wiring is M3.1 follow-up, not P1.23. |
| `kryoflux` | `make_unique<KryoFluxProviderV2>(makeQProcessDtcRunner(), "dtc")` — production QProcess runner (the lambda is sketched in `kryoflux_provider_v2.h:142-156`). Real backend. |
| `fluxengine` | `make_unique<FluxEngineProviderV2>(makeQProcessFluxEngineRunner(), "fluxengine", 79, "ibm")` — production QProcess runner. Real backend. |
| `fc5025` | `make_unique<FC5025ProviderV2>(makeFc5025Runner(), makeFc5025DetectRunner(), 79)` — production runner wrapping libusb-or-fcimage. Real backend. |
| `applesauce` | `make_unique<ApplesauceProviderV2>(...runners wrapping a QSerialPort transport...)` — M3.3 scaffold; transport may be a real QSerialPort. |
| `xum1541` | `make_unique<XUM1541ProviderV2>(...OpenCBM-backed runners...)` — real OpenCBM path exists in V1. |
| `adfcopy` | `make_unique<ADFCopyProviderV2>(...QSerialPort-backed runners...)` — real V1 protocol code exists. |
| `usb_floppy` | **BLOCKED** — needs `UsbFloppyReadRunner/WriteRunner/DetectRunner` that wrap `uft_ufi_*`, and the UFI backend does not exist (ARCH-3 / P1.25). See §4. |

The "production runner factory" functions (`makeQProcessDtcRunner()` etc.) are
new free functions — they belong in a small non-foundation helper TU
(`src/hardware_providers/provider_runners.{h,cpp}` or inline in
`hardwaretab.cpp`). They are **not** foundation-header changes.

### The forensic rule, applied

- Scaffold provider with honest stub transport (SCP, Applesauce): **route it**.
  Capability present as type property; runtime returns `ProviderError`. Honest.
- Provider with a real production runner (KryoFlux, FluxEngine, FC5025,
  XUM1541, ADFCopy): **route it**. Real I/O.
- Provider whose backend type does not exist (USBFloppy → UFI): **cannot be
  constructed at all** — there is no runner to pass. Routing it is impossible,
  not merely premature.

---

## 4. Sequencing — recommendation: **phased, not route-all-now**

### The hard question restated

Routing a provider whose transport is a null/honest-stub gives the user a
controller that "connects" then returns `ProviderError` on every button. Is
that acceptable?

### Recommendation

**Phase P1.23a — route the 7 constructible providers now; keep USBFloppy
explicitly gated.**

Rationale:

1. **USBFloppy genuinely cannot be done.** Its constructor *requires* three
   runner callables. Those runners wrap `uft_ufi_*`, which P1.25/ARCH-3 says
   does not exist. There is no honest-stub to pass either — a null runner
   `std::function` would make `do_*` return `ProviderError`, but the provider
   *also needs a device path and the V1 LBA logic*. Constructing it with three
   null runners is *technically* possible and *would* be honest (every op →
   `ProviderError`), but it is pointless: the controller is Destination-only,
   sector-only, and has **no sector buttons in the YAML anyway** (see point 3).
   So gate it behind an explicit, *accurate* "not wired" state. **This is a
   STOP-flag-worthy dependency: P1.23 cannot fully close ARCH-1 without P1.25.**
   It can close 7/8 of it.

2. **The stale messagebox must change regardless.** The current
   "routing pending (P1.18)" text is wrong on two counts: P1.18 is done, and
   P1.23 is the actual task. For the one remaining gated controller
   (`usb_floppy`) the message should say: "USB Floppy Drive needs the UFI
   sector backend (task P1.25). It is not yet selectable." — accurate, not a
   catch-all.

3. **Sector providers route fine but have a dead UI — flag it, don't block on
   it.** FC5025 and XUM1541 *are* constructible with real runners, so route
   them. But the Hardware tab YAML has zero `ReadsSectors`/`WritesSectors`
   actions, so when connected, **every button is disabled** (correctly, via
   `wire_action`'s false branch). That is honest — no mis-dispatch — but it is
   a UX dead-end. Adding sector actions to `forms/tab_hardware.actions.yaml`
   (`btnReadSector` requires `ReadsSectors`, etc.) is **out of P1.23 scope**
   (it needs new `.ui` widgets and new handler overloads — a separate task,
   call it P1.24). P1.23 should route FC5025/XUM1541 so they are *reachable
   and honest*, and explicitly note in `REFACTOR_TASKS.md` that their buttons
   stay disabled until the sector-action task lands. Routing-without-buttons is
   strictly better than the current `QMessageBox`-and-return: detection
   (`btnDetect` / `DetectsDrive`) **is** wired, and all three sector providers
   satisfy `DetectsDrive` — so the Detect button actually lights up for them.

### Phase summary

- **P1.23 (this task):** route `greaseweazle, scp, kryoflux, fluxengine,
  fc5025, xum1541, applesauce, adfcopy`. Replace the catch-all messagebox with
  an accurate per-controller gate; only `usb_floppy` remains gated, with a
  P1.25-specific message. Variant dispatch type + codegen `std::visit` wrap.
- **P1.24 (follow-up, not this task):** add sector actions to the Hardware-tab
  YAML + `.ui` so FC5025/XUM1541/USBFloppy have working buttons.
- **P1.25 (follow-up, ARCH-3):** UFI backend → USBFloppy runners → drop the
  last gate.

---

## 5. Migration impact + STOP check

### Protected foundation headers — NOT touched

`include/uft/hal/{outcomes,concepts,mixins}.h` are **not** modified by this
proposal. Verified:

- No new concept is needed — `std::variant` dispatch + `std::visit` is a
  *consumer-side* construct in `hardwaretab.{h,cpp}` and the codegen. The
  capability concepts and the `wire_action` template are used **exactly as-is**.
- `wire_action`'s `static_assert(std::is_class_v<Provider>, ...)` still holds —
  inside the `std::visit` lambda, `p` is a concrete provider class pointer, not
  a base pointer. The type-driven design is *reinforced*, not bent.
- `KNOWN_CAPABILITIES` in `wiring_codegen.py` is unchanged.

**No STOP condition on foundation headers.** One genuine dependency STOP:
**USBFloppy routing requires P1.25 (UFI backend)** — P1.23 closes 7 of the 8
ARCH-1 gaps; the 8th is explicitly deferred with an accurate gate, not faked.

### Files P1.23 touches

| File | Change | Size |
|---|---|---|
| `src/hardwaretab.h` | `m_gwProviderV2` member → `ProviderV2Variant m_providerV2`; 9 fwd-decls; `currentProviderV2()` return type → `const ProviderV2Variant&` | small |
| `src/hardwaretab.cpp` | `currentProviderV2()` body; `onConnect()` per-controller construction dispatch (~8 branches + runner factories); `onDisconnect()` reset → `m_providerV2 = {}`; `#include` 8 provider headers | **large** (~150-250 LOC) |
| `forms/tab_hardware.actions.yaml` | `extra_includes:` += 8 provider headers; `<variant>` note | small |
| `tools/wiring_codegen.py` | Phase-2 monostate guard; Phase-3 `std::visit` wrap shell | medium (~30 LOC in `emit()`) |
| `generated/tab_hardware_wiring.gen.cpp` | regenerated output (mechanical) | regenerated |
| `src/hardware_providers/provider_runners.{h,cpp}` *(new, optional)* | production QProcess / QSerialPort / OpenCBM / libusb runner factory functions | medium-large |
| `docs/REFACTOR_TASKS.md` | mark P1.23 done; note P1.24 (sector actions) + P1.25 (UFI) follow-ups | small |
| `tests/test_hal_foundation.cpp` | **no change** — foundation contract unaffected (no concept/variant change) |

### Staging estimate

**Does not fit one reviewable commit cleanly — stage in 3:**

1. **Commit 1 — dispatch type + codegen.** `ProviderV2Variant` typedef,
   `hardwaretab.h` member swap, `currentProviderV2()` accessor change,
   `wiring_codegen.py` `std::visit` wrap, regenerated `.gen.cpp`, YAML
   `extra_includes`. At this point GW still works (its variant alternative is
   constructed); the build is green; no behavior change for non-GW (they still
   hit the messagebox because `onConnect()` is untouched).
2. **Commit 2 — runner factories + construction sites.** New
   `provider_runners.{h,cpp}`, `onConnect()` per-controller dispatch for the 7
   real providers, `onDisconnect()` reset. This is where non-GW controllers
   start actually routing.
3. **Commit 3 — gate cleanup + docs.** Replace the stale "P1.18" messagebox
   with the accurate `usb_floppy`-only P1.25 gate, update `REFACTOR_TASKS.md`.

Each commit is independently green and reviewable. Commit 2 is the largest;
if it exceeds the 50-file / size comfort threshold it can be split per provider
family (flux-subprocess: kryoflux+fluxengine; serial: applesauce+adfcopy;
usb-sector: fc5025+xum1541) — but it touches only ~3 files so it should be one.

---

## 6. GO / NO-GO

**GO — with the phased scope of §4.**

- Dispatch type (`std::variant<std::monostate, unique_ptr<...>×9>`) is sound,
  ownership-correct, movable despite non-movable providers, and reinforces
  rather than bends the type-driven design.
- Codegen change is a mechanical `std::visit` wrap around the *unchanged*
  `wire_action` emission — capability gating stays fully structural.
- 7 of 8 non-GW providers get real production construction sites; scaffold
  providers are honest (capability = type property; unwired backend =
  forensically-truthful `ProviderError`, never a silent no-op).
- **No protected foundation header is touched. No `tests/golden/` is touched.**
- One honest dependency limit: **USBFloppy cannot be routed without P1.25
  (UFI backend)** — P1.23 closes 7/8 of ARCH-1 and replaces the stale
  catch-all messagebox with an accurate single-controller gate. This is a
  scoped deferral, not a STOP that blocks the task.
- Secondary finding flagged for a follow-up task (P1.24): FC5025/XUM1541/
  USBFloppy are sector devices and the Hardware-tab YAML has no sector
  actions — they route honestly but with most buttons disabled until sector
  actions are added. Not in P1.23 scope.

**Recommended next step:** human go-ahead on the 3-commit staging and on
deferring USBFloppy to a P1.25-gated state. On approval, hand Commit 1
(codegen + dispatch type) to `wiring-codegen-author` and Commit 2 (runner
factories + construction sites) to `provider-migrator`; this proposal's author
(type-system-architect) confirms no foundation-header work is required.

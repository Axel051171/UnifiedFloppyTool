# V2 Drive-Unit Selection — Design Proposal

Status: PROPOSAL (not applied). Awaiting human go-ahead.
Branch: `refactor/type-driven-hal`
Blocks: REFACTOR_TASKS.md P1.20 (`FluxCaptureJob` migration), P1.21 (`FluxWriteJob` migration)
Author role: Type System Architect
Foundation-header impact: **NONE** (recommended option needs no edit to `outcomes.h` / `concepts.h` / `mixins.h`)

---

## 1. The `select_head` "gap" — VERDICT: not a gap

`FluxCaptureJob` / `FluxWriteJob` today call `uft_gw_select_head(gw, side)`
before each per-track read/write. In V2 this is **already subsumed** by the
per-call `head` field:

- `ReadFluxParams::head` (concepts.h:63) is consumed by
  `GreaseweazleProviderV2::do_read_raw_flux` — it builds `head` and passes it
  straight into `uft_gw_read_track(m_handle, cyl, head, revs, &flux)`
  (greaseweazle_provider_v2.cpp:208, :212).
- `WriteFluxParams::head` (concepts.h:79) is consumed by `do_write_raw_flux`
  the same way — `uft_gw_write_track(m_handle, cyl, head, ticks…)`
  (greaseweazle_provider_v2.cpp:325, :357).

The V1 `uft_gw_select_head` call is the *old* stateful idiom ("set head, then
read the implicitly-selected head"). The V2 C-API entry points
`uft_gw_read_track` / `uft_gw_write_track` take `head` as an explicit
argument, so head selection is folded into the read/write call itself. The
migrated job simply stops calling `select_head` and instead populates
`params.head = side`.

**Conclusion:** no concept, mixin, or Outcome change is needed for head
selection. Do not add a `SelectsHead` concept. The only real blocker is
drive-*unit* selection.

---

## 2. Drive-unit selection — option analysis

The Greaseweazle bus can address two physical drives on one ribbon cable
(unit 0 / unit 1, the DS0/DS1 select lines). `uft_gw_select_drive(gw, unit)`
asserts the corresponding select line; it is a **device-global mode change**,
not a per-track parameter. Every subsequent seek/read/write/motor command
acts on whichever unit was last selected.

### Option (a) — `SelectsDrive` capability concept + mixin

A new `SelectsDrive<P>` concept, a `SelectsDriveVia<Backend>` mixin, a
`do_select_drive(int unit)` method, and a new `DriveSelectOutcome` (or reuse
of an existing variant).

- Touches **two protected foundation headers** (`concepts.h`, `mixins.h`) and
  probably `outcomes.h` for a new variant.
- Anti-goal check (AI_COLLABORATION.md / task brief): a concept earns its
  place only with **>=2 genuinely satisfying providers**. The cross-provider
  survey in section 3 shows that, at the *current V2 backend surface*, only
  Greaseweazle has a real standalone drive-select primitive. XUM1541's
  `device_num` and FC5025/USBFloppy's `device_path` are already modelled as
  request fields / ctor params, not as a drive-select *call*. So a
  `SelectsDrive` concept would have **exactly one** satisfying provider today.
- Verdict: **rejected.** Adding it now violates the "no concept just in case"
  rule and forces a protected-header change for a single provider.

### Option (b) — constructor parameter on `GreaseweazleProviderV2`

Drive unit bound at construction time, exactly like the FE-F2 `profile`
parameter on `FluxEngineProviderV2` (fluxengine_provider_v2.h:223-226) and
like XUM1541's `device_num` ctor arg (xum1541_provider_v2.h:351) and
USBFloppy's `device_path` ctor arg (usbfloppy_provider_v2.h:386).

- **No foundation-header change.** Pure provider-local change to
  non-protected files.
- Matches an established precedent already accepted on this branch for the
  *same class of problem* ("a device-global selector that the per-call param
  records cannot carry, because those records live in the protected header").
- The provider calls `uft_gw_select_drive` exactly once, at `open()` time
  (or in the handle-taking ctor), and surfaces failure honestly — see the
  forensic-visibility note below.
- A drive unit is a property of *which device this provider instance talks
  to*. Binding it to the instance is consistent with the type-driven spirit:
  the provider instance *is* "the Greaseweazle, talking to unit N".
- Verdict: **chosen.** See section 5 for the exact sketch.

### Option (c) — `drive_unit` field on `ReadFluxParams` / `WriteFluxParams`

- Touches the protected `concepts.h`.
- Semantically wrong: drive-unit is *not* per-call. Putting it in the
  per-call record invites callers to flip it mid-capture, which would
  silently re-select the bus between track 39 and track 40 of one image — a
  forensic foot-gun. The per-call records are deliberately
  position-only (`cylinder`, `head`, `revolutions`, `window_ns`).
- It would also force *every* flux provider (SCP, KryoFlux, FluxEngine,
  Applesauce, ADFCopy) to carry a field that is meaningless for them.
- Verdict: **rejected** (protected header + wrong granularity).

### Option (d) — `open()` overload taking a unit

`bool open(const char* port_path, int drive_unit, std::string* err_out)`.

- No foundation-header change — same protected-header score as (b).
- But it only covers the `open()` path. `GreaseweazleProviderV2` also has the
  handle-taking ctor `GreaseweazleProviderV2(uft_gw_device_t*)` (P1.18,
  greaseweazle_provider_v2.h:95) which `FluxCaptureJob` is most likely to use
  once it stops receiving a raw `void*`. An overload leaves that path without
  a unit.
- Verdict: **rejected in favour of (b)** — (b) covers both construction paths
  uniformly. (d) is acceptable as a *secondary* convenience overload but is
  not the primary mechanism.

---

## 3. Cross-provider survey — does a shared mechanism make sense?

| Provider (V2)        | Notion of selecting a drive/unit/device?                                                                                                  | How modelled today                                  |
|----------------------|-------------------------------------------------------------------------------------------------------------------------------------------|------------------------------------------------------|
| Greaseweazle         | **Yes — real.** Bus addresses unit 0/1 via DS0/DS1; `uft_gw_select_drive()` is a real C-API primitive (uft_greaseweazle_full.h:322).        | *Currently nothing in V2* — this is the blocker.     |
| SuperCard Pro        | Partial. SCP protocol has a drive-select (`SELA`/`SELB`), but the M3.1 `uft_scp_direct.h` scaffold does **not** expose it (scp header:12-35). | Not modelled; would be a future C-API addition.      |
| KryoFlux             | DTC `-d<n>` drive index exists, but the V2 `build_read_argv` hardcodes `-d0` (kryoflux header:206-211).                                     | Implicit in argv; not a capability.                  |
| FluxEngine           | `drive:0` in the CLI invocation; `build_read_argv` hardcodes `drive:0` (fluxengine header:280-286).                                        | Implicit in argv; not a capability.                  |
| FC5025               | USB device; "which device" = which USB handle / which `fcimage` invocation.                                                               | Not a drive-select; device identity is the runner.   |
| XUM1541              | **Yes — different axis.** CBM IEC device number 8-15. Already a first-class field: `Xum1541ReadRequest::device_num` + ctor `device_num` arg. | Request field + ctor param (xum header:139, :351).   |
| Applesauce           | Single drive per controller; no unit concept.                                                                                             | n/a                                                  |
| ADFCopy              | Single Amiga drive per controller; no unit concept.                                                                                       | n/a                                                  |
| USB Floppy           | "Which device" = OS device path. Already a first-class field: `device_path` ctor arg + `set_device_path()`.                                | Ctor param + setter (usbfloppy header:386, :407).    |

**Reading of the survey:**

1. There is **no single shared "select a drive" axis**. Three different
   things wear that name: a *bus unit line* (Greaseweazle DS0/DS1, SCP
   SELA/SELB), a *bus device address* (XUM1541 IEC 8-15), and *device
   identity* (FC5025/USBFloppy USB handle / path). They are not
   substitutable; a shared concept would have to be vague enough to be
   meaningless.

2. Every provider that *does* have such a notion already expresses it the
   **same structural way: a constructor parameter and/or a request field** —
   `device_num`, `device_path`, the FE `profile`. None of them is a
   capability concept. The branch has already, repeatedly, decided this class
   of selector is **construction-time configuration, not a typed
   capability**.

3. Therefore the mechanism should be **GW-specific** (a `GreaseweazleProviderV2`
   ctor parameter), consistent with how XUM1541 and USBFloppy already solved
   their analogous problem. It is *not* shared, because there is no genuine
   shared abstraction to share — and the type-driven design's own rule
   ("a concept must have a real call-site, >=2 satisfying providers") tells
   us so.

---

## 4. Proposed diffs

### 4.1 Foundation headers — NO CHANGE

`include/uft/hal/{outcomes,concepts,mixins}.h` are **untouched** by this
proposal. This is the headline result: the blocker is resolvable entirely
within the non-protected provider files. No protected-header diff is
presented because none is needed.

### 4.2 `src/hardware_providers/greaseweazle_provider_v2.h` (NOT protected — sketch only)

```diff
@@ class GreaseweazleProviderV2 final
     GreaseweazleProviderV2() noexcept = default;

-    explicit GreaseweazleProviderV2(uft_gw_device_t* handle) noexcept;
+    /**
+     * @brief Construct from an already-open handle, bound to a drive unit.
+     *
+     * @param handle      Owned uft_gw_device_t handle (see ownership note).
+     * @param drive_unit  Greaseweazle bus unit: 0 (DS0) or 1 (DS1). The
+     *                    provider issues uft_gw_select_drive() once, lazily,
+     *                    before the first bus operation. An out-of-range
+     *                    unit or a select failure surfaces as a
+     *                    ProviderError on the first do_* call — it is never
+     *                    silently ignored (forensic rule).
+     */
+    explicit GreaseweazleProviderV2(uft_gw_device_t* handle,
+                                    int drive_unit = 0) noexcept;
@@
-    bool open(const char *port_path, std::string *err_out = nullptr);
+    bool open(const char *port_path, std::string *err_out = nullptr);
+
+    /**
+     * @brief open() overload that also binds the drive unit (0 or 1).
+     * Equivalent to open(port_path, err_out) followed by set_drive_unit().
+     */
+    bool open(const char *port_path, int drive_unit,
+              std::string *err_out = nullptr);
+
+    /**
+     * @brief Set the bus unit (0 or 1) for subsequent operations.
+     *
+     * Records the unit and clears the "selected" latch so the next bus
+     * operation re-asserts it. Range is validated lazily (on next do_*),
+     * not silently clamped.
+     */
+    void set_drive_unit(int unit) noexcept;
+
+    /** Currently-configured bus unit (0 or 1). Default 0. */
+    int drive_unit() const noexcept { return m_drive_unit; }
@@ private:
     uft_gw_device_t *m_handle = nullptr;
     std::string      m_firmware_version;
     int              m_hw_model = 0;
+    int              m_drive_unit      = 0;     /**< Bus unit 0/1, ctor/setter. */
+    bool             m_drive_selected  = false; /**< Lazy-select latch. */
+
+    /**
+     * @brief Ensure uft_gw_select_drive() has run for m_drive_unit.
+     * Returns an empty optional on success, or a populated ProviderError
+     * (out-of-range unit, or select-drive C-API failure) that the caller
+     * must convert into the right Outcome variant. Idempotent once latched.
+     */
+    std::optional<ProviderError> ensure_drive_selected();
```

### 4.3 `src/hardware_providers/greaseweazle_provider_v2.cpp` (NOT protected — sketch only)

```cpp
// New: lazy, forensically-visible drive selection.
std::optional<ProviderError> GreaseweazleProviderV2::ensure_drive_selected()
{
    if (m_drive_selected)
        return std::nullopt;
    if (m_drive_unit != 0 && m_drive_unit != 1) {
        return ProviderError{
            UFT_ERR_INVALID_ARG,
            "Greaseweazle drive unit out of range",
            "Drive unit " + std::to_string(m_drive_unit) +
                " is invalid; the Greaseweazle bus addresses unit 0 or 1 only.",
            "Pass drive_unit 0 or 1 when constructing GreaseweazleProviderV2 "
            "or via set_drive_unit()."
        };
    }
    int rc = uft_gw_select_drive(m_handle,
                                 static_cast<uint8_t>(m_drive_unit));
    if (rc != UFT_GW_OK) {
        return gw_err_to_provider_error(
            rc,
            "Greaseweazle drive-unit select failed",
            "uft_gw_select_drive returned error");
    }
    m_drive_selected = true;
    return std::nullopt;
}
```

Each existing `do_*` method that touches the bus (`do_read_raw_flux`,
`do_write_raw_flux`, `do_set_motor`, `do_seek`, `do_recalibrate`,
`do_measure_rpm`, `do_detect_drive`) gains, right after its
`!m_handle || !uft_gw_is_connected` guard:

```cpp
    if (auto err = ensure_drive_selected())
        return std::move(*err);   // ProviderError is in every Outcome variant
```

`ProviderError` is an alternative of **every** Outcome variant
(`FluxOutcome`, `WriteOutcome`, `MotorOutcome`, `SeekOutcome`, `RpmOutcome`,
`DetectOutcome` — see outcomes.h:225-330), so the early-return type-checks in
all seven methods with no new variant. This is the forensic-visibility
guarantee: an unsupported/unreachable unit produces a typed 3-part error, not
a silent no-op.

The handle-taking ctor and `open()` store `m_drive_unit` but do **not** call
`uft_gw_select_drive` eagerly (the handle may not be ready / connected yet);
selection is lazy on first bus use, matching the existing
`uft_gw_is_connected` guard pattern.

---

## 5. `FluxCaptureJob` migration sketch

Today (workflowtab.cpp:531-537): the job receives `m_gwDevice` as a
`void*` via `setDevice()`, and a hardcoded `setDriveUnit(0)`.

With option (b), the migration (P1.20) becomes:

1. **Where the unit comes from:** unchanged in *source* — it is still a
   GUI/workflow choice. `workflowtab.cpp` currently passes a literal `0`;
   that literal (or, later, a real UI control / `m_driveUnit` member on the
   tab) is the single source of truth.

2. **How it reaches the provider:** `WorkflowTab` constructs the provider
   instead of passing a raw handle:

   ```cpp
   // workflowtab.cpp, replacing setDevice(m_gwDevice) + setDriveUnit(0):
   auto provider = std::make_unique<uft::hal::GreaseweazleProviderV2>(
       static_cast<uft_gw_device_t*>(m_gwDevice),
       m_driveUnit /* 0 today; a real UI value later */);
   m_captureJob->setProvider(std::move(provider));
   ```

   `FluxCaptureJob` gains `setProvider(std::unique_ptr<GreaseweazleProviderV2>)`
   and drops `setDevice(void*)` and `setDriveUnit(int)`. `m_gwDevice` /
   `m_driveUnit` members on the job are deleted.

3. **Inside `FluxCaptureJob::run()`:** the two stateful C-API calls vanish:
   - `uft_gw_select_drive(gw, m_driveUnit)` (fluxcapturejob.cpp:94) — gone;
     the provider does it lazily on the first `do_*`.
   - `uft_gw_select_head(gw, side)` (fluxcapturejob.cpp:140) — gone; folded
     into `ReadFluxParams::head`.

   The per-track read becomes:
   ```cpp
   uft::hal::ReadFluxParams rp;
   rp.cylinder    = cyl;
   rp.head        = side;
   rp.revolutions = m_revolutions;
   uft::hal::FluxOutcome out = provider->read_raw_flux(rp);
   std::visit(uft::hal::overloaded{
       [&](const uft::hal::FluxCaptured& fc) { /* feed SCP writer:
            fc.transitions_ns + fc.index_times_ns (MF-194) */ },
       [&](const uft::hal::FluxUnreadable& u) { ++errors; /* log, skip */ },
       [&](const uft::hal::FluxMarginal& m)   { /* keep, log anomaly */ },
       [&](const uft::hal::ProviderError& e)  { /* e.what/why/fix → emit error */ },
       [&](const uft::hal::HardwareDisconnected&) { /* abort */ },
       [&](const uft::hal::CapabilityRequiresPolicy&) { /* abort */ },
   }, out);
   ```
   A bad drive unit now surfaces as a `ProviderError` from the *first*
   `read_raw_flux` (or from `set_motor`, whichever runs first) with the exact
   what/why/fix from `ensure_drive_selected()` — visible, not swallowed.

4. **`FluxWriteJob` (P1.21)** is the mirror image: same `setProvider`,
   `select_drive`/`select_head` deleted, per-track write uses
   `WriteFluxParams::{cylinder,head}` + `write_raw_flux(params, fluxStream)`.

---

## 6. Additive vs. breaking analysis

| Change                                                              | Additive / Breaking | Notes |
|---------------------------------------------------------------------|---------------------|-------|
| Foundation headers (`outcomes/concepts/mixins.h`)                   | **No change**       | Zero risk to other migrated providers; no recompile cascade. |
| `GreaseweazleProviderV2(uft_gw_device_t*, int=0)` ctor              | **Additive**        | New 2nd param is defaulted → existing `GreaseweazleProviderV2(handle)` call sites (conformance harness, per-provider tests passing `nullptr`) keep compiling unchanged. |
| `open(port, int, std::string*)` overload                            | **Additive**        | New overload; existing `open(port)` / `open(port, err)` untouched. |
| `set_drive_unit()` / `drive_unit()`                                 | **Additive**        | New methods. |
| `ensure_drive_selected()` early-return in 7 `do_*` methods          | **Behaviour-additive** | For `drive_unit == 0` (the default and the only value used today) and a healthy device, `uft_gw_select_drive(gw,0)` is exactly what `FluxCaptureJob` already does today at fluxcapturejob.cpp:94 — so observable behaviour for current callers is unchanged. New behaviour only triggers for unit 1 or a select failure. |
| `FluxCaptureJob` / `FluxWriteJob` API (`setProvider` vs `setDevice`)| **Breaking (intended)** | This *is* P1.20/P1.21. Only 2 call sites, both in `workflowtab.cpp` (lines 531-537, 575-579). Contained. |
| Static-assert / conformance contract                                | **No change**       | No new concept → no new `static_assert`, no `tests/golden/` touch, no `test_hal_foundation.cpp` edit. |

No already-migrated provider is invalidated. No `tests/golden/` file is
touched. The change set is < 5 files and well under the 50-file STOP
threshold.

---

## 7. GO / NO-GO

**Recommendation: GO** — with option (b) (constructor parameter +
`set_drive_unit()` setter + lazy `ensure_drive_selected()`).

Rationale:
- The one real blocker (drive-unit selection) is solved **without touching
  any protected P0 foundation header** — the strongest possible outcome
  against the branch's constraints.
- `select_head` was a false alarm: already covered by `ReadFluxParams::head`
  / `WriteFluxParams::head`.
- The mechanism is **not invented** — it reuses the exact pattern the branch
  already accepted three times (FE `profile`, XUM1541 `device_num`,
  USBFloppy `device_path`) for the same class of device-global configuration.
- The cross-provider survey actively *disproves* the case for a `SelectsDrive`
  concept: only one provider has a true drive-select primitive at the current
  backend surface, and "select a drive" is three non-substitutable axes
  wearing one name. Adding the concept would violate the "no concept just in
  case" anti-goal.
- Forensic rule satisfied: an out-of-range unit or a `select_drive` C-API
  failure becomes a typed 3-part `ProviderError` on the first bus operation —
  never silently ignored.

No STOP condition is triggered. Nothing here requires a protected-header
change, so there is no fallback "unacceptable foundation change" to surface.

### If, later, the SCP C-API gains a real `SELA/SELB` primitive

Then *two* providers (Greaseweazle + SCP) would genuinely satisfy a
unit-select capability, and a `SelectsDrive` concept could be **revisited**
at that point — as a proper P0 foundation change with its own proposal,
go-ahead, `test_hal_foundation.cpp` static-asserts, and REFACTOR_BRIEF
sub-section. It is explicitly **out of scope now** and must not be
pre-emptively added.

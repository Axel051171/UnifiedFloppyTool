---
name: uft-hal-backend
description: |
  Use when implementing a new Hardware-Abstraction-Layer backend for UFT.
  Trigger phrases: "HAL backend für X", "neuer controller X", "implement
  X HAL", "connect X hardware", "SCP-direct HAL", "XUM1541 HAL real
  statt stubbed", "Applesauce HAL". Three backends are pending (M3 scope):
  SCP-Direct, XUM1541, Applesauce. This skill enforces the unified-HAL
  pattern from `src/hal/uft_hal_unified.c`.
---

# UFT HAL Backend Template

Use this skill for implementing or completing a hardware backend
(Greaseweazle, SuperCard Pro direct, KryoFlux, FC5025, Applesauce,
XUM1541, UFI/UFI+Cowork). Greaseweazle is the canonical reference
— it is production-ready and its patterns should be mirrored.

## Step 1: Identify the backend layer

UFT has two HAL layers — don't confuse them:

| Layer | Location | Purpose |
|---|---|---|
| **Qt Provider** | `src/hardware_providers/` | UI-bound Qt wrapper (signals/slots) |
| **C HAL**       | `src/hal/`                | Pure C, synchronous, no Qt deps |

New backends go in **both** layers. Start with the C HAL (pure, testable),
then wrap it with a Qt provider.

## Step 2: C HAL implementation

Template: `src/hal/uft_greaseweazle_full.c`. New file location:
`src/hal/uft_<backend>.c` and `src/hal/uft_<backend>.h`.

Required struct and callback pattern:

```c
#include "uft/hal/uft_hal_unified.h"

typedef struct {
    /* private state: file descriptor, USB handle, etc. */
    FILE *port;
    int   current_track;
} backend_ctx_t;

static uft_error_t backend_open(uft_hal_ctx_t *ctx, const char *device);
static uft_error_t backend_close(uft_hal_ctx_t *ctx);
static uft_error_t backend_seek(uft_hal_ctx_t *ctx, int track);
static uft_error_t backend_read_flux(uft_hal_ctx_t *ctx, int side,
                                      uint32_t *flux_out, size_t *count);
static uft_error_t backend_write_flux(uft_hal_ctx_t *ctx, int side,
                                       const uint32_t *flux, size_t count);
static uft_error_t backend_get_capabilities(uft_hal_ctx_t *ctx,
                                             uft_hal_capabilities_t *caps);

/* Capability reporting — be honest, no silent NOP */
static const uft_hal_capabilities_t BACKEND_CAPS = {
    .can_read_flux   = true,
    .can_write_flux  = true,
    .can_read_sector = false,   /* pure flux, no FDC sector mode */
    .max_revolutions = 5,
    .max_tracks      = 84,
};

static const uft_hal_vtable_t backend_vtable = {
    .open = backend_open, .close = backend_close,
    .seek = backend_seek,
    .read_flux = backend_read_flux, .write_flux = backend_write_flux,
    .get_capabilities = backend_get_capabilities,
};
```

## Step 3: Register the backend

Add to `src/hal/uft_hal_unified.c` registry block:

```c
const uft_hal_backend_info_t BACKENDS[] = {
    { "greaseweazle", &gw_vtable,   UFT_HAL_BACKEND_GREASEWEAZLE },
    { "scp",          &scp_vtable,  UFT_HAL_BACKEND_SCP },
    { "<backend>",    &backend_vtable, UFT_HAL_BACKEND_<NAME> },  /* NEW */
    { NULL }
};
```

Also add to `include/uft/hal/uft_hal_unified.h` the new enum value.

## Step 4: Honesty rules (forensic-integrity)

These are HARD requirements — violations block merge:

- **Never return UFT_OK from a stubbed callback.** Return
  `UFT_ERR_NOT_IMPLEMENTED` explicitly.
- **Never silently drop flux data.** If the hardware can't capture
  timing resolution your caller requested, return `UFT_ERR_UNSUPPORTED`
  with the actual resolution in the error context.
- **Never fabricate sector data.** If the hardware returns garbage,
  pass it through with low confidence — do NOT synthesize "plausible"
  defaults.
- Capability flags must match reality: if `can_write_flux = true` but
  `write_flux()` always returns error, that's a phantom feature.

## Step 5: Qt provider wrapper

File: `src/hardware_providers/<backend>hardwareprovider.cpp` +
`.h`. Template: `src/hardware_providers/greaseweazlehardwareprovider.cpp`.

- Inherit `HardwareProvider` (QObject).
- Use `QThread` + `QObject::moveToThread` for async captures; **never**
  block the UI thread on HAL calls.
- Emit progress via signals at track granularity, not bit.
- Cancel support: check `m_cancelRequested` after each track.

## Step 6: Build integration

- `UnifiedFloppyTool.pro`: add both `src/hal/uft_<backend>.c` and
  `src/hardware_providers/<backend>hardwareprovider.cpp` to SOURCES.
  Header to HEADERS. Qt provider needs moc — ensure `Q_OBJECT` is
  in the class declaration.
- `src/hal/CMakeLists.txt`: add the new .c to `HAL_SOURCES` list if
  this is a Unix/release build target.
- Run `python3 scripts/verify_build_sources.py`.

## Step 7: Integration test

At minimum: probe test (does it compile and register?). Full
hardware-in-the-loop test belongs in M3's emulator-CI-pipeline,
not in M2.

```c
/* tests/test_<backend>_backend.cpp */
void TestBackendRegistration::test_appears_in_registry() {
    QVERIFY(HalRegistry::find("<backend>") != nullptr);
}
```

## Current pending backends (M3 scope)

| Backend | Status | Source reference |
|---|---|---|
| SCP-Direct | stubbed | `docs/A8RAWCONV_INTEGRATION_TODO.md` TA4 |
| XUM1541    | stubbed | zoomfloppy firmware protocol |
| Applesauce | stubbed | text-based USB protocol, 3.5" + 5.25" |
| UFI/Cowork | planned | own firmware, STM32H723 target |

## Anti-patterns

- Don't add a provider without the C HAL layer — creates Qt-coupled
  backend that can't be unit-tested.
- Don't implement flux synthesis to "fake" controllers that only read
  sectors. FC5025 stays `can_read_flux = false`.
- Don't block the UI thread on USB I/O — always use a worker thread.

## Related

- `src/hal/uft_greaseweazle_full.c` — canonical backend reference
- `src/hardware_providers/greaseweazlehardwareprovider.cpp` — Qt wrapper
- `docs/MASTER_PLAN.md` M3 (HAL stabilization)
- `docs/A8RAWCONV_INTEGRATION_TODO.md` TA4 (SCP-Direct)
- `CLAUDE.md` Hardware Controllers section

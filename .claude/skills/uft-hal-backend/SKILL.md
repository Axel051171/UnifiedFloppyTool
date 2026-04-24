---
name: uft-hal-backend
description: |
  Use when implementing a new HAL backend for hardware controllers
  (Greaseweazle, SCP-Direct, KryoFlux, FC5025, XUM1541, Applesauce, UFI).
  Trigger phrases: "neuer HAL backend", "HAL backend für X", "controller X
  implementieren", "implement X HAL", "SCP-Direct HAL", "XUM1541 HAL real",
  "Applesauce HAL", "UFI hardware". Scaffolds C HAL + Qt provider + registry.
  DO NOT use for: bugfix in existing backend (→ structured-reviewer), changes
  to capability flags only (→ quick-fix), firmware code on the device side
  (separate UFI repo), protocol reverse engineering (different domain).
---

# UFT HAL Backend

Use this skill for implementing or completing a hardware controller backend.
Greaseweazle (`src/hal/uft_greaseweazle_full.c`) is the canonical reference
because it's production-ready.

## When to use this skill

- Implementing one of the pending backends: SCP-Direct, XUM1541, Applesauce,
  UFI/Cowork
- Adding a new controller (new hardware vendor)
- Promoting a stubbed backend to functional

**Do NOT use this skill for:**
- Fixing bugs in an existing backend — use `structured-reviewer`
- Capability-flag adjustments only — use `quick-fix`
- Firmware code that runs ON the device — that's a separate repo
- Protocol reverse engineering — write a spec doc first, then this skill

## The 5 touchpoints

| # | File | Purpose |
|---|------|---------|
| 1 | `src/hal/uft_<backend>.c` | C HAL implementation (sync, no Qt) |
| 2 | `src/hal/uft_<backend>.h` | C HAL header |
| 3 | `src/hal/uft_hal_unified.c` | Registry entry |
| 4 | `include/uft/hal/uft_hal_unified.h` | Enum + capability flags |
| 5 | `src/hardware_providers/<backend>hardwareprovider.cpp` + `.h` | Qt wrapper |

## Two-layer discipline

UFT has a strict split:

```
┌─────────────────────────────────────┐
│ Qt Provider (src/hardware_providers/)│  ← UI-bound, async, Qt deps
│  emits signals, uses QThread         │
├─────────────────────────────────────┤
│ C HAL (src/hal/)                     │  ← pure C, synchronous, testable
│  returns errors, no UI               │
├─────────────────────────────────────┤
│ Hardware / USB / Serial              │
└─────────────────────────────────────┘
```

Start with the C HAL — it's pure and unit-testable. Wrap with Qt only after
the HAL passes its standalone test.

## Workflow

### Step 1: Reference Greaseweazle

Read `src/hal/uft_greaseweazle_full.c` end-to-end before writing. Note:

- The vtable dispatch pattern
- How errors propagate from USB layer
- Where capabilities are declared vs. runtime-negotiated
- Cancel support (`cancel_requested` flag checked per-track)

### Step 2: Scaffold the C HAL

```bash
python3 .claude/skills/uft-hal-backend/scripts/scaffold_hal.py \
    --backend xum1541 \
    --description "XUM1541 ZoomFloppy (IEC bus)" \
    --enum XUM1541 \
    --can-read-flux false \
    --can-write-flux false \
    --can-read-sector true
```

### Step 3: Fill in the backend callbacks

The scaffolded `uft_<backend>.c` has stubbed callbacks. Replace each
`TODO: <n>` block:

- `<backend>_open` — establish connection, probe device ID
- `<backend>_seek` — move head to requested cylinder
- `<backend>_read_flux` / `<backend>_read_sector` — the work
- `<backend>_close` — release handles, flush buffers

### Step 4: Qt provider wrapper

After C HAL passes tests, scaffold the Qt wrapper:

```bash
python3 .claude/skills/uft-hal-backend/scripts/scaffold_qt_provider.py \
    --backend xum1541
```

This creates the `.cpp`/`.h` pair with `QThread` moved worker pattern.

### Step 5: Register

`src/hal/uft_hal_unified.c`:

```c
extern const uft_hal_vtable_t uft_hal_vtable_xum1541;

const uft_hal_backend_info_t BACKENDS[] = {
    { "greaseweazle", &gw_vtable,      UFT_HAL_BACKEND_GREASEWEAZLE },
    { "xum1541",      &xum1541_vtable, UFT_HAL_BACKEND_XUM1541 },  /* NEW */
    /* ... */
    { NULL }
};
```

## Verification

```bash
# 1. Compile HAL isolated
gcc -std=c11 -Wall -Wextra -Werror -fsyntax-only \
    -I include/ src/hal/uft_<backend>.c

# 2. Full build including Qt provider
qmake && make -j$(nproc)

# 3. Backend appears in list
./uft hal list | grep <backend>

# 4. Registration smoke test
cd tests && make test_<backend>_backend && ./test_<backend>_backend

# 5. If you have the hardware: live test
./uft read --hal <backend> --device /dev/ttyUSB0 --out /tmp/test.scp
```

## Honesty rules (hard requirements, forensic-integrity checks these)

```c
/* NEVER return UFT_OK from a stubbed callback */
static uft_error_t <backend>_read_flux(/* ... */) {
    return UFT_ERROR_NOT_IMPLEMENTED;   /* explicit, not silent zero */
}

/* NEVER silently degrade */
if (requested_resolution_ns < HARDWARE_MIN_NS) {
    /* Don't round up and pretend. */
    return UFT_ERROR_UNSUPPORTED;
}

/* NEVER fabricate data to fill gaps */
if (usb_read_returned_partial) {
    /* Don't zero-pad. Either retry or error. */
}
```

Capability flags must match reality. If `can_write_flux = true` but
`write_flux()` always returns `UFT_ERROR_NOT_IMPLEMENTED`, that's a phantom
feature — blocks merge.

## Common pitfalls

### Blocking the UI thread

USB I/O takes 50ms–30s. If you call HAL from the UI thread, UFT freezes.
Always use the Qt provider's worker pattern; never call HAL directly from
a button slot.

### Forgetting cancel support

Users press Cancel. HAL must check `ctx->cancel_requested` between tracks
(not between bits — too fine) and return `UFT_ERROR_CANCELLED` promptly.

### Capability flag drift

Cap flags are declared statically in the vtable, but actual hardware might
vary (some Greaseweazle firmware revisions support more than others). If
you runtime-detect capabilities, populate them in `open()` and expose via
`get_capabilities()` — don't contradict the static declaration.

### Coupling C HAL to Qt types

`QString` in `src/hal/`? No. The C HAL must be compilable without Qt. If
you need to pass strings, use `const char *`. Qt-side conversions happen
in the provider.

## Related

- `src/hal/uft_greaseweazle_full.c` — canonical reference
- `src/hardware_providers/greaseweazlehardwareprovider.cpp` — Qt wrapper reference
- `.claude/agents/forensic-integrity.md` — reviews honesty violations
- `.claude/skills/uft-stm32-portability/` — if this backend is UFI/firmware-adjacent
- `docs/HARDWARE.md` — existing backend notes
- `docs/MASTER_PLAN.md` M3 — HAL stabilization scope

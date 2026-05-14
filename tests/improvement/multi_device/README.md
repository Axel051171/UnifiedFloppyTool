# improvement/multi_device/

Tests that prove UFT drives hardware `gw` cannot — `gw` only ever talks
to a Greaseweazle, with a single fixed device model. UFT's Hardware tab
holds a `ProviderV2Variant`: any of 9 unrelated V2 provider types, and
the operator switches between them.

**Form: C++ QtTest / core-library C tests, not pytest-qt.** Same
reasoning as the `forensic/` and `gui/` categories — UFT is GUI-only
with no Python bindings; the tests live in `tests/`.

| Test | Proves | Status |
|------|--------|--------|
| KryoFlux V2 provider read path | the KryoFlux provider decodes flux correctly | ✅ covered by `tests/test_kryoflux_provider_v2.cpp` (+ `tests/test_kryoflux_stream.c`, MF-208) |
| SCP V2 provider read/write | the SuperCard Pro provider's outcome surface | ✅ covered by `tests/test_scp_provider_v2.cpp` |
| FluxEngine V2 provider read/write | the FluxEngine provider's SCP-pivot decode | ✅ covered by `tests/test_fluxengine_provider_v2.cpp` (+ `tests/test_scp_read_memory.c`, MF-209) |
| `tests/test_provider_switch.cpp` | switching the active provider type keeps capability reporting consistent — no state bleed across connect/disconnect/reconnect, disconnect always returns to the no-capability baseline | ✅ MF-221 — C++ QtTest driving the real HardwareTab `ProviderV2Variant` |

The first three planned tests were already delivered as the per-provider
V2 conformance tests in the P1 refactor (MF-161/162/163, later extended
by MF-208/209). The multi-device-specific gap — that *switching*
between provider types stays consistent — is `test_provider_switch.cpp`.

`uft_device_manager.h` is a `UFT_SKELETON_PLANNED` header (14/14
functions unimplemented, no `device_manager.c`). The implemented
multi-device model is the `ProviderV2Variant` in HardwareTab, which is
what `test_provider_switch.cpp` exercises. A device-manager improvement
test is not possible until that API is built.

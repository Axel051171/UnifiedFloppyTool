# improvement/concurrency/

Tests that prove UFT's multi-device session model — `gw` has no concept
of running more than one device at once.

**Form: C++ test against the type-driven HAL, not pytest-qt.** Same
reasoning as the `forensic/`, `gui/` and `multi_device/` categories —
UFT is GUI-only with no Python bindings; the test lives in `tests/`.

| Test | Proves | gw fails because |
|------|--------|------------------|
| `tests/test_concurrency.cpp` :: `parallel_drives_keep_independent_state` | several provider instances driven with interleaved operations + divergent per-instance config keep fully independent state — call counts, argument capture and configured outcome kind never bleed across instances | gw is single-device, single-session — it cannot represent a second drive at all |
| `tests/test_concurrency.cpp` :: `many_drives_scale_independently` | the independence holds for arbitrary N (16 instances, each touched a distinct number of times), not just a hand-picked few | gw has no multi-device model to scale |
| `tests/test_concurrency.cpp` :: `long_running_session_stays_consistent` | one provider driven through 6000 operation cycles sweeping every Outcome variant kind — counters increment exactly, argument capture stays exact, every returned variant matches its configured kind from first call to last | gw has no session model to stress |

The guarantee is structural: every V2 provider is a `final` class
composed of capability mixins over a CRTP self-type, with no shared base
and no static mutable state — so N instances (same type or different
types) coexist with independent state by construction. `test_concurrency`
exercises that at runtime and `static_assert`s the `final` rule at
compile time.

`uft_device_manager.h` is a `UFT_SKELETON_PLANNED` header (14/14
functions unimplemented). The implemented multi-device model is the set
of independent V2 provider instances; a device-manager-level concurrency
test is not possible until that API is built.

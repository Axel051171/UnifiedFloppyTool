# Mock Hardware (refactor/type-driven-hal — P1.6)

Skeleton mock-transport headers used by the conformance harness
(`tests/test_hal_conformance.cpp`) and per-provider tests
(`tests/test_<provider>_v2.cpp`) to exercise V2 providers WITHOUT real
hardware.

These mocks are **transport-level** — they simulate the byte-stream the
provider's backend layer talks to, not the provider's variant outcomes.
For an outcome-level mock provider (always-happy fake) see
`MockProviderV2` (P1.7).

## Layout

```
tests/mock_hardware/
├── README.md             — this file
├── byte_buffer.h         — shared scripted-byte-stream helper
├── usb_loopback_mock.h   — libusb-style bulk + control transfer mock
├── subprocess_mock.h     — external-CLI (argv/stdin/stdout/exit) mock
└── serial_mock.h         — QSerialPort-style line + raw I/O mock
```

| Mock                 | V2 providers that will compose against it (P1.8–P1.15) |
|----------------------|---------------------------------------------------------|
| `usb_loopback_mock`  | `SCPProviderV2`, `FC5025ProviderV2`, `XUM1541ProviderV2`, `USBFloppyProviderV2` |
| `subprocess_mock`    | `KryoFluxProviderV2` (DTC), `FluxEngineProviderV2`     |
| `serial_mock`        | `ApplesauceProviderV2`, `ADFCopyProviderV2`            |

`GreaseweazleProviderV2` does NOT use any of these — it talks to the
production-tested `uft_gw_*` C-API directly. For GW conformance, the
factory passes a `nullptr` handle (P1.5) or, later, a fake C-API stub
(out of scope for P1.6).

## Extension contract

Each new V2 provider chooses ONE of the three transports as its
template/constructor parameter. The provider's `factory<P>::make()` in
the conformance harness then constructs a fresh mock instance,
programs a scripted scenario via the mock's `queue_*()` API, and
wraps the mock as the V2 provider's backend.

Adding a new transport class (e.g. a parallel-port mock) means:
1. Adding a new header here following the same scripted-queue pattern.
2. Documenting it in the table above.
3. NOT modifying the conformance harness — the harness is
   transport-agnostic; only the per-provider factory cares about
   the transport.

## Hard rules

- **No real hardware access.** A mock that probes `/dev/ttyUSB*` or
  enumerates USB devices defeats its own purpose.
- **No nondeterminism.** Mocks replay scripted byte sequences exactly.
  No clocks, no random numbers, no thread races. Tests must be
  byte-deterministic across reruns.
- **No Qt dependency.** These mocks are consumed by header-only
  conformance tests in `_HEADER_ONLY_CPP_TESTS`. A Qt include here
  forces every conformance test into the Qt-test loop, which would
  drag in `hardware_providers` linkage and ruin the isolation.
- **Programmed-then-used model.** Tests configure the queue via
  `queue_*()` calls FIRST, then exercise the provider, then call
  `assert_consumed()` to verify every scripted reply was used. A test
  that ends with a non-empty queue is a forensic gap.

## Smoke test

`tests/test_mock_hardware.cpp` exercises each mock in isolation:

- programs a scripted scenario,
- drives it through the mock's backend API,
- asserts the recorded interactions match expectation,
- verifies `assert_consumed()` passes after correct use and throws
  after underuse.

The smoke test is the contract. If it fails, the mocks are broken
and conformance results from any provider depending on them are
invalid.

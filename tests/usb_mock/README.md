# tests/usb_mock/ — Tier-2.5 libusb-Mock-Framework (MF-270)

**Stand:** 2026-05-25 — neu in V415-PLAN.

## Was ist das?

Eine **scripted libusb-1.0-Ersatzimplementierung**, mit der C-HAL-Files
gegen einen festgelegten USB-Exchange getestet werden können, **ohne
echte Hardware oder Treiber**. Ergänzt das QProcess-basierte
`tools/hw_simulators/`-System (KryoFlux DTC, FluxEngine, FC5025) um den
**libusb-direct**-Pfad (SCP-Direct, XUM1541).

## Architektur

```
+-------------------+        compile-time:                +----------------+
| test_*_usb_mock.c |  -include libusb_mock.h →           | libusb_mock.h  |
|                   |                                     | (typedefs +    |
|  uses queue API:  |                                     |  func macros)  |
|  usb_mock_queue_* |                                     +----------------+
+-------------------+                                              │
        │                                                          ▼
        ▼                                                  +----------------+
+-------------------+                                      | libusb_mock.c  |
| C-HAL .c          |  libusb_init() → uft_mock_*()        | (scripted      |
| (uft_scp_direct,  |  libusb_open() → uft_mock_*()        |  queue impl)   |
|  uft_xum1541,...) |  libusb_bulk_transfer() → uft_mock_*()|                |
+-------------------+                                      +----------------+
```

**Crucially: production code is UNCHANGED.** The `-include libusb_mock.h`
compile option (applied only to the C-HAL source via
`set_source_files_properties`) injects the mock header before the
production code reaches any libusb call.

Trick: the mock header pre-defines `LIBUSB_H` (the real libusb's
include-guard name), so any `#include <libusb-1.0/libusb.h>` later in
the C-HAL becomes a no-op.

## Was wird getestet?

- **Byte-exakte Wire-Protocol-Sequenz**: jeder Bulk-OUT-Transfer wird
  verglichen mit `expected_tx[len]`; Mismatch = loud FAIL.
- **3-Part-Error-Contract (F-4)**: open() ohne Device → ProviderError mit
  what/why/fix, niemals fabricated success.
- **Idempotency**: zweiter open()-Call darf nicht erneut USB anfassen.
- **Status-Byte-Decoding**: Device antwortet `0xFF` statt `PR_OK=0x4F`
  → C-HAL muss `UFT_ERR_IO` zurückgeben, nicht silent success.
- **Timeout-Handling**: Bulk-OUT timeout → I/O-Fehler propagiert,
  nicht swallow.

## Was wird NICHT getestet?

- USB-Stack-spezifische Issues (driver, hotplug, transfer cancellation)
- Echte Signal-Integrity / electrical issues
- Timing-abhängige Bugs (real DTC dauert Sekunden, Mock antwortet instant)
- Magnetic-aging / weak-bit-Verhalten echter Disketten

→ Diese erfordern Tier-3 echte Hardware (`tests/hil/run_hil.py`).

## Coverage-Stand (2026-05-25)

| Controller | C-HAL | Mock-Test | Tests |
|---|---|---|---|
| SCP-Direct  | `src/hal/uft_scp_direct.c` | `tests/test_scp_direct_usb_mock.c` | 5/5 ✅ |
| XUM1541     | `src/hal/uft_xum1541.c`    | `tests/test_xum1541_usb_mock.c`    | 3/3 ✅ |
| Greaseweazle| `src/hal/uft_greaseweazle_full.c` (CDC-serial, **kein libusb**) | `tools/greaseweazle_sim.py` (separate Tier-2 protocol sim) | — |
| USBFloppy   | `src/hal/ufi_linux.c` (SG_IO, **kein libusb**) | — (SG_IO-Mock TBD v4.1.6) | — |

## API (libusb_mock.h)

### Scripting (test-side)

```c
usb_mock_reset();                              // start a new exchange
usb_mock_queue_open(VID, PID, succeeds);       // open_device_with_vid_pid
usb_mock_queue_bulk_out(EP, tx, len, ack);     // expected host→device
usb_mock_queue_bulk_in(EP, rx, len, ack);      // device→host reply
usb_mock_queue_control_in(bmRT, bReq, rx, len); // control_transfer IN
usb_mock_queue_close();                         // libusb_close expected
```

### Diagnostics

```c
size_t usb_mock_remaining_exchanges(void);  // assert 0 at test end
size_t usb_mock_total_transfers(void);      // total libusb_*_transfer calls
```

## CMake setup für neue Mock-Tests

```cmake
elseif(${test_name} STREQUAL "test_my_hal_usb_mock")
    target_sources(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/hal/uft_my_hal.c
        ${CMAKE_SOURCE_DIR}/tests/usb_mock/libusb_mock.c)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/tests/usb_mock)
    target_compile_definitions(${test_name} PRIVATE
        UFT_HAS_LIBUSB=1
        UFT_USB_MOCK=1)
    set_source_files_properties(
        ${CMAKE_SOURCE_DIR}/src/hal/uft_my_hal.c
        PROPERTIES COMPILE_OPTIONS
        "-include;${CMAKE_SOURCE_DIR}/tests/usb_mock/libusb_mock.h"
    )
    set(TEST_LIBS "")
endif()
```

## CI-Integration

Beide USB-Mock-Tests laufen in der `Audit`-CI-Lane (siehe
`.github/workflows/audit.yml` → "Tier-2.5 hardware simulator gate").
Die Tests werden separat durch ctest in den anderen CI-Lanes (CI,
Sanitizer, Coverage) gebaut und ausgeführt — der Audit-Lane-Schritt
fasst zusammen.

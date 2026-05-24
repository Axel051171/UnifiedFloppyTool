# M3.3 — Applesauce Production Transport Plan

**Status:** SCAFFOLD (this document). Code work pending.
**Owns:** UFT-001 sub-task for Applesauce. Multi-session work.
**Last updated:** 2026-05-24

This document is the concrete implementation guide for wiring the
ApplesauceProviderV2 (currently honest-stub with `nullptr` runners)
to a real Applesauce device via QSerialPort. It exists so the next
session — human or AI — can land the code without re-deriving the
architecture.

---

## Why this file

The HardwareTab currently constructs `ApplesauceProviderV2` with 7
`nullptr` runner arguments:

```cpp
m_providerV2 = std::make_unique<::uft::hal::ApplesauceProviderV2>(
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
```

Every operation on this provider returns `ProviderError("... requires
a QSerialPort-backed transport before real operations are possible")`.
That's forensically sound (Prinzip 1: never silently no-op) but
functionally useless — the user cannot read or write an Applesauce
device through UFT today.

The `IApplesauceTransport` interface
(`src/hardware_providers/applesauce_provider_v2.h:138-177`) is already
designed for this exact purpose. What is missing is:

1. A concrete `QSerialPortApplesauceTransport` implementing the 4
   virtual methods.
2. 7 runner-factory closures that each capture a shared transport and
   implement the corresponding operation against the Applesauce text
   protocol.
3. A small change in `hardwaretab.cpp` to construct the runners with
   the user-selected serial port.

This document gives the protocol reference + skeleton code for each
piece. It does NOT include the code itself, because the code MUST be
verified against a real Applesauce device before commit (writing
hardware-touching code blind violates Master-Plan rule "mach keine
Fehler / don't make mistakes").

---

## 1. Applesauce serial protocol reference

Source: Applesauce community protocol notes (no official SDK).
Verified strings extracted from device firmware. UFT's existing
`src/hal/uft_applesauce.c` reference comments cite this.

- **Line discipline:** 115200 baud, 8N1, no flow control.
- **Command syntax:** ASCII line, terminated by `\n`. Responses are
  also ASCII lines.
- **Status prefix on response:** First character indicates status:
  - `+` — success, the rest of the line is the payload
  - `-` — error, the rest of the line is the error message
  - `:` — multi-line response follows
- **Binary mode:** Some commands negotiate a switch to binary bulk:
  - `data:>N\n` — host will write N bytes of binary next
  - `data:<N\n` — device will write N bytes of binary next
  - The byte count N is decimal-ASCII; after N bytes, the device
    returns to line mode and acknowledges with `+ok\n`.

Reference commands needed for the 7 runners:

| Operation | Command | Response |
|---|---|---|
| Identify     | `info\n`                                 | `+Applesauce vX.Y\n` |
| Motor on/off | `motor:1\n` / `motor:0\n`                | `+ok\n` |
| Seek         | `seek NN\n` (quarter-track index)        | `+ok\n` or `-error\n` |
| Recalibrate  | `recal\n`                                | `+ok\n` |
| RPM measure  | `rpm\n`                                  | `+rpm:300.12\n` |
| Detect drive | `detect\n`                               | `+drive:35t-ss-gcr\n` |
| Capture flux | `track NN hH capture R revolutions\n`    | `:` + `data:<N\n` + N bytes |
| Write flux   | `track NN hH write\n` + `data:>N\n` ...  | `+ok\n` after final bytes |

A typical capture session:
```
> track 17 h0 capture 5 revolutions
< :starting capture
< data:<131072
< [131072 binary bytes — 8 MHz tick stream]
< +ok 5 revs captured
```

Sample clock is **8 MHz / 125 ns per tick** (the existing
`uft_as_ticks_to_ns()` does this conversion correctly).

---

## 2. Scaffold for QSerialPortApplesauceTransport

```cpp
// File: src/hardware_providers/qserial_applesauce_transport.h
#pragma once
#include "applesauce_provider_v2.h"
#include <QSerialPort>
#include <memory>
#include <string>

namespace uft::hal {

class QSerialPortApplesauceTransport final : public IApplesauceTransport {
public:
    /** Opens portName at 115200 8N1. Returns nullptr on failure;
     *  caller can read QSerialPort::errorString() via getError(). */
    static std::unique_ptr<QSerialPortApplesauceTransport>
    open(const QString& portName);

    ~QSerialPortApplesauceTransport() override;

    bool        write_line(const std::string& command) override;
    std::string read_line (int timeout_ms = 3000) override;
    int         write_binary(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> read_binary(uint32_t n_bytes,
                                     int timeout_ms = 30000) override;

    /* Diagnostic — last serial-port error string. */
    QString lastError() const;

private:
    explicit QSerialPortApplesauceTransport(QSerialPort* port);
    QSerialPort* m_port = nullptr;
    QString      m_lastError;
};

} // namespace uft::hal
```

Implementation skeleton (one method shown):

```cpp
bool QSerialPortApplesauceTransport::write_line(const std::string& cmd) {
    if (!m_port || !m_port->isOpen()) {
        m_lastError = "transport not open";
        return false;
    }
    QByteArray buf(cmd.data(), static_cast<int>(cmd.size()));
    buf.append('\n');
    qint64 written = m_port->write(buf);
    if (written != buf.size()) {
        m_lastError = m_port->errorString();
        return false;
    }
    return m_port->waitForBytesWritten(1000);
}
```

`read_line()` should accumulate bytes until `\n` is seen OR
`timeout_ms` elapses (use `QSerialPort::waitForReadyRead`).

---

## 3. Runner-factory skeleton

The provider expects 7 `std::function<...>` runners. Each closes over
a shared transport. Example for `do_capture_flux`:

```cpp
ApplesauceReadRunner make_read_runner(
    std::shared_ptr<IApplesauceTransport> tx)
{
    return [tx](int cylinder, int head, int revolutions)
                  -> ApplesauceReadResult {
        char cmdbuf[64];
        std::snprintf(cmdbuf, sizeof(cmdbuf),
                      "track %d h%d capture %d revolutions",
                      cylinder, head, revolutions);
        if (!tx->write_line(cmdbuf))
            return ApplesauceReadResult{ /* error result */ };

        /* Read ":starting capture", then "data:<N", then N bytes. */
        std::string line = tx->read_line();
        if (line.empty() || line[0] != ':') { /* error */ }

        line = tx->read_line();
        if (line.compare(0, 6, "data:<") != 0) { /* error */ }
        uint32_t n = std::stoul(line.substr(6));

        std::vector<uint8_t> raw = tx->read_binary(n);
        if (raw.size() != n) { /* error */ }

        /* Convert 8 MHz ticks → ns intervals via uft_as_ticks_to_ns. */
        ApplesauceReadResult r;
        r.ticks = std::move(raw);  // exact field name in header
        r.sample_ns = 125.0;       // 1/8 MHz
        r.revolutions = revolutions;
        return r;
    };
}
```

Six more runners follow the same pattern: send a line, parse the
response, return a typed result.

---

## 4. HardwareTab wiring change

Currently in `src/hardwaretab.cpp` near line 763:

```cpp
} else if (controller == "applesauce") {
    m_providerV2 = std::make_unique<::uft::hal::ApplesauceProviderV2>(
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
```

After this work lands:

```cpp
} else if (controller == "applesauce") {
    auto tx = ::uft::hal::QSerialPortApplesauceTransport::open(port);
    if (!tx) {
        updateStatus(tr("Applesauce: cannot open %1: %2")
                        .arg(port, tx->lastError()), /*isError=*/true);
        return;
    }
    std::shared_ptr<::uft::hal::IApplesauceTransport> shared(std::move(tx));
    m_providerV2 = std::make_unique<::uft::hal::ApplesauceProviderV2>(
        ::uft::hal::make_read_runner(shared),
        ::uft::hal::make_write_runner(shared),
        ::uft::hal::make_motor_runner(shared),
        ::uft::hal::make_seek_runner(shared),
        ::uft::hal::make_recal_runner(shared),
        ::uft::hal::make_rpm_runner(shared),
        ::uft::hal::make_detect_runner(shared));
    /* Drop the orange "Preview" styling — we have a real transport. */
}
```

---

## 5. Verification plan (REQUIRED before commit)

This code path **MUST** be verified against a real Applesauce device.
Hardware-touching code that is "best-effort coded but never tested"
is a forensic-integrity hazard (it can silently corrupt the very disks
the user wants to preserve).

Minimum verification:
1. Connect Applesauce hardware
2. Open the controller in HardwareTab
3. Verify `+Applesauce vX.Y` info string appears in status bar
4. Insert a known-good disk, hit Read on track 0
5. Compare resulting `transitions_ns` distribution against an
   independent Applesauce CLI capture of the same disk
6. Repeat with a write to a scratch disk; readback should be
   byte-identical (or accompanied by a `.loss.json` per §1.2)

Until step 4 is verifiable green, this transport stays unmerged.

---

## 6. Status checklist (multi-session)

- [ ] **S1** Write `QSerialPortApplesauceTransport` + standalone unit
      test using `QSerialPortLoopback` mock — code only, no hardware.
- [ ] **S2** Write 7 runner factories — code only, no hardware.
- [ ] **S3** Wire into HardwareTab behind a build-config flag
      (`UFT_APPLESAUCE_QSERIAL=ON`) so it can be A/B-tested.
- [ ] **S4** Bench session with real Applesauce — verification plan
      above, write `audit/applesauce_field_notes.md` per HIL pattern.
- [ ] **S5** Remove the `nullptr` constructor path from HardwareTab
      once S4 is green; remove the "Disconnect (Preview)" styling for
      Applesauce specifically.

When all 5 are done, UFT-001 closes one of its 8 sub-controllers.

---

## 7. Parallel patterns

Same exercise applies to the 5 other USB/Serial-direct providers:

| Provider     | Transport | Reference doc |
|---|---|---|
| SCP-Direct   | libusb-1.0 over FT240X | `docs/M3_SCP_TRANSPORT.md` (TODO) |
| XUM1541      | libusb-1.0 OpenCBM-compat | `docs/M3_XUM1541_TRANSPORT.md` (TODO) |
| Applesauce   | QSerialPort (THIS DOC)  | — |
| FluxEngine   | libusb-1.0              | `docs/M3_FLUXENGINE_TRANSPORT.md` (TODO) |
| ADFCopy      | QSerialPort (Teensy)    | `docs/M3_ADFCOPY_TRANSPORT.md` (TODO) |
| USBFloppy    | SG_IO (Linux) / IOCTL (Win) | `docs/M3_USBFLOPPY_TRANSPORT.md` (TODO) |

KryoFlux + FC5025 already work because they use external CLI tools
(DTC / fcimage) via `QProcess`. No new transport needed there.

Greaseweazle is already production-wired (see
`src/hal/uft_greaseweazle_full.c`).

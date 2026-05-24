/**
 * @file qserial_applesauce_transport.h
 * @brief Concrete IApplesauceTransport over Qt6 QSerialPort (MF-249).
 *
 * Production transport for Applesauce devices over USB-CDC serial.
 * Implements the 4-method IApplesauceTransport interface declared in
 * applesauce_provider_v2.h:138 against a QSerialPort.
 *
 * Defensive by design (forensic invariant): on any unexpected response
 * or timeout the transport returns failure — never partial / never
 * synthesised data. The runner-factories above interpret these results
 * into ApplesauceReadResult / ApplesauceWriteResult and SET
 * `transport_unavailable = true` / `error_message` accordingly, so the
 * provider returns ProviderError, never silent corruption.
 *
 * Hardware verification status: NOT YET DONE. The code is internally
 * consistent and matches the documented Applesauce text protocol, but
 * has never run against a real device. See docs/M3_APPLESAUCE_TRANSPORT.md
 * §5 verification plan before considering this transport
 * production-blessed. Until then the HardwareTab still shows the orange
 * "Disconnect (Preview)" styling for Applesauce connections.
 */
#pragma once

#include "applesauce_provider_v2.h"

#include <QString>
#include <QtGlobal>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

QT_FORWARD_DECLARE_CLASS(QSerialPort)

namespace uft::hal {

/**
 * @brief QSerialPort-backed concrete IApplesauceTransport.
 *
 * Constructed via the static `open()` factory — direct construction is
 * private so the open-failure path always uses the factory's nullptr
 * return convention. Holds the QSerialPort by std::unique_ptr to keep
 * the destructor trivial.
 *
 * Thread model: single-threaded. The runner-factory closures execute
 * on whichever thread invokes the V2 provider methods; QSerialPort's
 * synchronous waitForReadyRead / waitForBytesWritten work without an
 * event loop on that thread, which is the contract we want for a
 * blocking transport.
 *
 * Line-mode constants:
 *   115200 baud, 8N1, no flow control, no parity.
 *   Verified against the existing Applesauce reverse-engineered
 *   protocol notes in src/hal/uft_applesauce.c.
 */
class QSerialPortApplesauceTransport final : public IApplesauceTransport {
public:
    /** Default line-read timeout (ms). Applesauce most commands answer
     *  within ~50 ms; we are generous to ride out worst-case USB
     *  scheduling jitter on slow hosts. */
    static constexpr int kDefaultLineTimeoutMs   = 3000;

    /** Default binary-read timeout (ms). A full track at 8 MHz over
     *  5 revolutions @ ~163 KB / 12 Mbps takes ~110 ms; this gives
     *  a safety factor of ~270 for sluggish hosts or large AS+
     *  430 KB captures. */
    static constexpr int kDefaultBinaryTimeoutMs = 30000;

    /** Serial-port line discipline constants. */
    static constexpr qint32 kBaudRate  = 115200;
    /* Qt enum literals copied as integers so we don't need <QSerialPort>
     * in this header; the .cpp uses the named constants. */

    /**
     * @brief Open the named port at 115200 8N1 and return an owned transport.
     *
     * On failure the unique_ptr is empty and the static last-error
     * accessor (intended for diagnostic-only use; this transport is
     * normally constructed by HardwareTab which has its own status bar)
     * carries the QSerialPort::errorString().
     *
     * @param portName  System-level port name (e.g. "COM3", "/dev/ttyACM0").
     * @return Owning pointer, or empty unique_ptr on failure.
     */
    static std::unique_ptr<QSerialPortApplesauceTransport>
    open(const QString& portName);

    /** Last error from the most recent failed open(). Thread-unsafe by
     *  design; this is diagnostic-only. */
    static QString lastOpenError();

    ~QSerialPortApplesauceTransport() override;

    /* ── IApplesauceTransport (see header for full doc) ────────────── */

    bool        write_line(const std::string& command) override;
    std::string read_line (int timeout_ms = kDefaultLineTimeoutMs) override;
    int         write_binary(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> read_binary(uint32_t n_bytes,
                                     int timeout_ms = kDefaultBinaryTimeoutMs) override;

    /* ── Diagnostics ───────────────────────────────────────────────── */

    /** True iff the underlying serial port is currently open. */
    bool is_open() const;

    /** The QSerialPort error string from the most recent failed
     *  read/write. Empty when no error pending. */
    QString lastTransportError() const;

private:
    explicit QSerialPortApplesauceTransport(std::unique_ptr<QSerialPort> port);

    /* Pull whatever is currently in the OS read buffer into the
     * internal accumulator. Returns false on transport error. */
    bool pull_available_(int timeout_ms);

    std::unique_ptr<QSerialPort> m_port;
    /** Bytes that arrived but haven't yet been consumed by a read_line
     *  or read_binary call. Lets `\n`-terminated lines spill into this
     *  buffer without blocking the next call. */
    std::string m_rxAccumulator;
    QString m_lastError;

    static QString s_lastOpenError;
};

} // namespace uft::hal

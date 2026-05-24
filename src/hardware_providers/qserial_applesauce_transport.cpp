/**
 * @file qserial_applesauce_transport.cpp
 * @brief QSerialPort-backed IApplesauceTransport implementation (MF-249).
 *
 * Defensive contract:
 *   - Every method validates the QSerialPort state before touching it.
 *   - Every method returns failure on timeout, partial-data, or device
 *     error — never synthesises bytes.
 *   - The internal accumulator only ever GROWS from real port reads;
 *     it is never seeded with assumed bytes.
 *
 * See docs/M3_APPLESAUCE_TRANSPORT.md for the protocol reference and
 * the §5 verification plan (must run before this transport ships
 * as production-wired in HardwareTab).
 */
#include "qserial_applesauce_transport.h"

#include <QSerialPort>
#include <QSerialPortInfo>
#include <algorithm>

namespace uft::hal {

QString QSerialPortApplesauceTransport::s_lastOpenError;

/* ── Construction / destruction ─────────────────────────────────── */

QSerialPortApplesauceTransport::QSerialPortApplesauceTransport(
    std::unique_ptr<QSerialPort> port)
    : m_port(std::move(port))
{
    /* Reserve enough for typical line responses ("+0000abcd\n" ≈ 16
     * bytes) without growing every command. Capacity, not size. */
    m_rxAccumulator.reserve(256);
}

QSerialPortApplesauceTransport::~QSerialPortApplesauceTransport()
{
    if (m_port && m_port->isOpen()) {
        m_port->close();
    }
}

std::unique_ptr<QSerialPortApplesauceTransport>
QSerialPortApplesauceTransport::open(const QString& portName)
{
    if (portName.isEmpty()) {
        s_lastOpenError = QStringLiteral("port name is empty");
        return nullptr;
    }

    auto port = std::make_unique<QSerialPort>();
    port->setPortName(portName);
    port->setBaudRate(kBaudRate);
    port->setDataBits(QSerialPort::Data8);
    port->setParity(QSerialPort::NoParity);
    port->setStopBits(QSerialPort::OneStop);
    port->setFlowControl(QSerialPort::NoFlowControl);

    if (!port->open(QIODevice::ReadWrite)) {
        s_lastOpenError = port->errorString();
        return nullptr;
    }

    /* Flush any boot-banner / stale bytes from the device side.
     * Applesauce firmware can emit a startup string on USB-CDC connect;
     * keeping it would poison the first command's response. */
    port->clear();

    s_lastOpenError.clear();
    /* Wrap in unique_ptr through the private ctor. We can't use
     * make_unique because the ctor is private. */
    return std::unique_ptr<QSerialPortApplesauceTransport>(
        new QSerialPortApplesauceTransport(std::move(port)));
}

QString QSerialPortApplesauceTransport::lastOpenError()
{
    return s_lastOpenError;
}

bool QSerialPortApplesauceTransport::is_open() const
{
    return m_port && m_port->isOpen();
}

QString QSerialPortApplesauceTransport::lastTransportError() const
{
    return m_lastError;
}

/* ── Internal helper ────────────────────────────────────────────── */

bool QSerialPortApplesauceTransport::pull_available_(int timeout_ms)
{
    if (!m_port || !m_port->isOpen()) {
        m_lastError = QStringLiteral("transport not open");
        return false;
    }

    /* waitForReadyRead returns true if at least one byte arrived
     * before the timeout. Even if it returns false (timeout), we
     * still copy whatever happens to be buffered — the caller will
     * decide if that's enough. */
    m_port->waitForReadyRead(timeout_ms);
    const QByteArray chunk = m_port->readAll();
    if (!chunk.isEmpty()) {
        m_rxAccumulator.append(chunk.constData(),
                               static_cast<std::size_t>(chunk.size()));
    }
    return true;
}

/* ── IApplesauceTransport interface ─────────────────────────────── */

bool QSerialPortApplesauceTransport::write_line(const std::string& command)
{
    if (!is_open()) {
        m_lastError = QStringLiteral("transport not open");
        return false;
    }

    /* Validate command: no embedded newlines (would split into two
     * commands on the device side, breaking the request/response
     * pairing — and silently doing so is exactly the forensic-hazard
     * pattern we reject). */
    if (command.find('\n') != std::string::npos ||
        command.find('\r') != std::string::npos) {
        m_lastError = QStringLiteral(
            "command contains embedded newline — refusing to send");
        return false;
    }

    QByteArray buf;
    buf.reserve(static_cast<int>(command.size()) + 1);
    buf.append(command.data(), static_cast<int>(command.size()));
    buf.append('\n');

    const qint64 written = m_port->write(buf);
    if (written != buf.size()) {
        m_lastError = m_port->errorString().isEmpty()
                          ? QStringLiteral("short write")
                          : m_port->errorString();
        return false;
    }

    /* Flush so the device sees the command before we wait for response. */
    if (!m_port->waitForBytesWritten(1000)) {
        m_lastError = m_port->errorString().isEmpty()
                          ? QStringLiteral("waitForBytesWritten timeout")
                          : m_port->errorString();
        return false;
    }
    m_lastError.clear();
    return true;
}

std::string QSerialPortApplesauceTransport::read_line(int timeout_ms)
{
    /* Total elapsed budget. We may call waitForReadyRead multiple
     * times if bytes trickle in. Wall-clock guard, not per-call. */
    int remaining = std::max(0, timeout_ms);
    constexpr int kSliceMs = 250;

    while (true) {
        /* Have we already accumulated a complete line? */
        const auto pos = m_rxAccumulator.find('\n');
        if (pos != std::string::npos) {
            std::string line = m_rxAccumulator.substr(0, pos);
            m_rxAccumulator.erase(0, pos + 1);
            /* Strip trailing \r if device sends CRLF (some firmwares
             * do, others don't — the protocol says LF, but be liberal
             * in what we accept). */
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return line;
        }

        if (remaining <= 0) {
            /* Hard timeout. Drop the accumulated partial line — keeping
             * it would let a stale partial response poison the next
             * read_line(). */
            m_lastError = QStringLiteral("read_line timeout");
            m_rxAccumulator.clear();
            return std::string();
        }

        const int slice = std::min(kSliceMs, remaining);
        if (!pull_available_(slice)) {
            return std::string();
        }
        remaining -= slice;
    }
}

int QSerialPortApplesauceTransport::write_binary(
    const std::vector<uint8_t>& data)
{
    if (!is_open()) {
        m_lastError = QStringLiteral("transport not open");
        return -1;
    }
    if (data.empty()) {
        /* Zero-byte write is a no-op — return 0, not an error. */
        return 0;
    }

    const qint64 written = m_port->write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<qint64>(data.size()));

    if (written < 0) {
        m_lastError = m_port->errorString().isEmpty()
                          ? QStringLiteral("write failed")
                          : m_port->errorString();
        return -1;
    }

    /* Block until the OS confirms all bytes have been handed to the
     * USB stack. The Applesauce protocol expects the host to follow
     * up with a status command after binary upload; without this
     * wait, the next write_line could race the binary on the USB
     * pipe. */
    if (written > 0 && !m_port->waitForBytesWritten(10000)) {
        m_lastError = QStringLiteral(
            "waitForBytesWritten timeout after binary upload");
        return static_cast<int>(written);
    }
    m_lastError.clear();
    return static_cast<int>(written);
}

std::vector<uint8_t> QSerialPortApplesauceTransport::read_binary(
    uint32_t n_bytes, int timeout_ms)
{
    if (!is_open()) {
        m_lastError = QStringLiteral("transport not open");
        return {};
    }
    if (n_bytes == 0) {
        return {};
    }

    /* Consume any bytes already in the line-accumulator first. The
     * protocol switches from line mode to binary mode mid-stream
     * after a "data:<N" announcement, and the announcement's trailing
     * bytes may already be in our buffer. */
    std::vector<uint8_t> out;
    out.reserve(n_bytes);

    if (!m_rxAccumulator.empty()) {
        const std::size_t take =
            std::min<std::size_t>(m_rxAccumulator.size(), n_bytes);
        out.insert(out.end(),
                   reinterpret_cast<const uint8_t*>(m_rxAccumulator.data()),
                   reinterpret_cast<const uint8_t*>(m_rxAccumulator.data()) + take);
        m_rxAccumulator.erase(0, take);
    }

    int remaining = std::max(0, timeout_ms);
    constexpr int kSliceMs = 500;

    while (out.size() < n_bytes && remaining > 0) {
        const int slice = std::min(kSliceMs, remaining);
        if (!m_port->waitForReadyRead(slice)) {
            /* Slice timeout — keep looping until total budget expires. */
            remaining -= slice;
            continue;
        }
        const QByteArray chunk = m_port->readAll();
        if (!chunk.isEmpty()) {
            const std::size_t need = n_bytes - out.size();
            const std::size_t take =
                std::min<std::size_t>(static_cast<std::size_t>(chunk.size()),
                                      need);
            out.insert(out.end(),
                       reinterpret_cast<const uint8_t*>(chunk.constData()),
                       reinterpret_cast<const uint8_t*>(chunk.constData()) + take);
            /* If the device sent MORE than we asked for (e.g. the
             * trailing status line started early), spill the surplus
             * back into the line accumulator for the next read_line
             * to find. */
            if (static_cast<std::size_t>(chunk.size()) > take) {
                m_rxAccumulator.append(
                    chunk.constData() + take,
                    static_cast<std::size_t>(chunk.size()) - take);
            }
        }
        remaining -= slice;
    }

    if (out.size() < n_bytes) {
        m_lastError = QStringLiteral(
            "read_binary timeout: got %1 of %2 bytes")
            .arg(out.size()).arg(n_bytes);
        /* Return WHAT WE GOT — caller checks size() < n_bytes and
         * treats it as error. The interface contract in the header
         * explicitly allows a short return on timeout, so the partial
         * data is forensically honest, not synthesised. */
    } else {
        m_lastError.clear();
    }
    return out;
}

} // namespace uft::hal

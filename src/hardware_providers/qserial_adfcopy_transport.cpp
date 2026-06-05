/**
 * @file qserial_adfcopy_transport.cpp
 * @brief QSerialPort-backed IADFCopyTransport implementation (MF-252).
 */
#include "qserial_adfcopy_transport.h"

#include <QSerialPort>
#include <algorithm>

namespace uft::hal {

QString QSerialPortADFCopyTransport::s_lastOpenError;

QSerialPortADFCopyTransport::QSerialPortADFCopyTransport(
    std::unique_ptr<QSerialPort> port)
    : m_port(std::move(port))
{
    m_rxAccumulator.reserve(512);
}

QSerialPortADFCopyTransport::~QSerialPortADFCopyTransport()
{
    if (m_port && m_port->isOpen()) m_port->close();
}

std::unique_ptr<QSerialPortADFCopyTransport>
QSerialPortADFCopyTransport::open(const QString& portName)
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
    port->clear();
    s_lastOpenError.clear();
    return std::unique_ptr<QSerialPortADFCopyTransport>(
        new QSerialPortADFCopyTransport(std::move(port)));
}

QString QSerialPortADFCopyTransport::lastOpenError()
{
    return s_lastOpenError;
}

bool QSerialPortADFCopyTransport::is_open() const
{
    return m_port && m_port->isOpen();
}

QString QSerialPortADFCopyTransport::lastTransportError() const
{
    return m_lastError;
}

int QSerialPortADFCopyTransport::write_bytes(const std::vector<uint8_t>& data)
{
    if (!is_open()) {
        m_lastError = QStringLiteral("transport not open");
        return -1;
    }
    if (data.empty()) return 0;

    const qint64 written = m_port->write(
        reinterpret_cast<const char*>(data.data()),
        static_cast<qint64>(data.size()));
    if (written < 0) {
        m_lastError = m_port->errorString();
        return -1;
    }
    if (!m_port->waitForBytesWritten(5000)) {
        m_lastError = QStringLiteral("waitForBytesWritten timeout");
        return static_cast<int>(written);
    }
    m_lastError.clear();
    return static_cast<int>(written);
}

std::vector<uint8_t>
QSerialPortADFCopyTransport::read_bytes(uint32_t n_bytes, int timeout_ms)
{
    if (!is_open()) {
        m_lastError = QStringLiteral("transport not open");
        return {};
    }
    if (n_bytes == 0) return {};

    std::vector<uint8_t> out;
    out.reserve(n_bytes);

    /* Consume any leftover bytes from the accumulator first. */
    if (!m_rxAccumulator.empty()) {
        const std::size_t take =
            std::min<std::size_t>(m_rxAccumulator.size(), n_bytes);
        out.insert(out.end(), m_rxAccumulator.begin(),
                              m_rxAccumulator.begin() + take);
        m_rxAccumulator.erase(m_rxAccumulator.begin(),
                              m_rxAccumulator.begin() + take);
    }

    int remaining = std::max(0, timeout_ms);
    constexpr int kSliceMs = 500;
    while (out.size() < n_bytes && remaining > 0) {
        const int slice = std::min(kSliceMs, remaining);
        if (!m_port->waitForReadyRead(slice)) {
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
            if (static_cast<std::size_t>(chunk.size()) > take) {
                m_rxAccumulator.insert(
                    m_rxAccumulator.end(),
                    reinterpret_cast<const uint8_t*>(chunk.constData()) + take,
                    reinterpret_cast<const uint8_t*>(chunk.constData()) + chunk.size());
            }
        }
        remaining -= slice;
    }

    if (out.size() < n_bytes) {
        m_lastError = QStringLiteral("read_bytes timeout: %1 of %2")
                          .arg(out.size()).arg(n_bytes);
    } else {
        m_lastError.clear();
    }
    return out;
}

std::string QSerialPortADFCopyTransport::read_until_newline(int timeout_ms)
{
    int remaining = std::max(0, timeout_ms);
    constexpr int kSliceMs = 250;

    while (true) {
        /* Find '\n' in accumulator. */
        auto pos = std::find(m_rxAccumulator.begin(), m_rxAccumulator.end(),
                             uint8_t('\n'));
        if (pos != m_rxAccumulator.end()) {
            std::string line(m_rxAccumulator.begin(), pos);
            m_rxAccumulator.erase(m_rxAccumulator.begin(), pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }

        if (remaining <= 0) {
            m_lastError = QStringLiteral("read_until_newline timeout");
            m_rxAccumulator.clear();
            return std::string();
        }

        const int slice = std::min(kSliceMs, remaining);
        if (!is_open()) {
            m_lastError = QStringLiteral("transport not open");
            return std::string();
        }
        m_port->waitForReadyRead(slice);
        const QByteArray chunk = m_port->readAll();
        if (!chunk.isEmpty()) {
            m_rxAccumulator.insert(
                m_rxAccumulator.end(),
                reinterpret_cast<const uint8_t*>(chunk.constData()),
                reinterpret_cast<const uint8_t*>(chunk.constData()) + chunk.size());
        }
        remaining -= slice;
    }
}

} // namespace uft::hal

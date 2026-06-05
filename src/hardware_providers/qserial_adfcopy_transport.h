/**
 * @file qserial_adfcopy_transport.h
 * @brief Concrete IADFCopyTransport over Qt6 QSerialPort (MF-252).
 *
 * Production transport for ADFCopy / ADF-Drive devices over USB-CDC
 * serial (Teensy-based). Implements the 3-method IADFCopyTransport
 * interface declared in adfcopy_provider_v2.h:135 against a QSerialPort.
 *
 * ADFCopy uses a BINARY protocol (contrast Applesauce text protocol):
 *   commands are single bytes (ADFC_CMD_*) followed by 0-N param bytes;
 *   responses start with a 1-byte status (ADFC_RSP_OK = 'O', etc.).
 * Only PING uses a text-style newline-terminated response.
 *
 * Hardware verification status: SAME as MF-250 Applesauce — compiles
 * + unit-testable via ScriptedADFCopyTransport, but has NOT yet been
 * run against a real ADFCopy/ADF-Drive device.
 */
#pragma once

#include "adfcopy_provider_v2.h"

#include <QString>
#include <QtGlobal>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

QT_FORWARD_DECLARE_CLASS(QSerialPort)

namespace uft::hal {

class QSerialPortADFCopyTransport final : public IADFCopyTransport {
public:
    static constexpr int    kDefaultTimeoutMs = 5000;
    static constexpr qint32 kBaudRate         = 460800; /* ADFCopy Teensy */

    static std::unique_ptr<QSerialPortADFCopyTransport>
    open(const QString& portName);

    static QString lastOpenError();

    ~QSerialPortADFCopyTransport() override;

    int                  write_bytes(const std::vector<uint8_t>& data) override;
    std::vector<uint8_t> read_bytes (uint32_t n_bytes,
                                     int timeout_ms = kDefaultTimeoutMs) override;
    std::string          read_until_newline(int timeout_ms = 3000) override;

    bool    is_open() const;
    QString lastTransportError() const;

private:
    explicit QSerialPortADFCopyTransport(std::unique_ptr<QSerialPort> port);

    std::unique_ptr<QSerialPort> m_port;
    std::vector<uint8_t> m_rxAccumulator;
    QString m_lastError;

    static QString s_lastOpenError;
};

} // namespace uft::hal

/**
 * @file teensy_probe.cpp
 * @brief probe_teensy_serial() — the QSerialPort I/O wrapper for the
 *        ADF-Copy <-> Applesauce disambiguation probe (audit ARCH-7-C).
 *
 * The decision logic lives in teensy_probe.h (classify_teensy_probe(),
 * pure + unit-tested). This file is only the thin transport: open the
 * port, run the non-destructive identify handshake, hand the two
 * responses to the classifier.
 *
 * The real body needs the QtSerialPort module. When UFT_HAS_SERIALPORT
 * is undefined (a Qt build without QtSerialPort — e.g. the local MinGW
 * dev kit here), the function still links and returns Unknown: with no
 * way to open the port there is nothing to probe, and Unknown is the
 * honest "could not identify", never a guess.
 */
#include "teensy_probe.h"

#include <QString>

#ifdef UFT_HAS_SERIALPORT
#include <QByteArray>
#include <QSerialPort>
#include <string>
#endif

namespace uft::hal {

#ifdef UFT_HAS_SERIALPORT

TeensyDevice probe_teensy_serial(const QString &portName)
{
    QSerialPort port;
    port.setPortName(portName);
    port.setBaudRate(QSerialPort::Baud115200);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);

    if (!port.open(QIODevice::ReadWrite)) {
        /* Cannot open -> cannot probe. Honest "could not identify",
         * not a fallback to one of the two devices. */
        return TeensyDevice::Unknown;
    }

    /* Send one identify command, then collect whatever comes back within
     * a short bounded window (the reply may arrive in a few chunks). */
    auto exchange = [&port](const QByteArray &cmd) -> std::string {
        port.clear();
        port.write(cmd);
        port.flush();
        QByteArray resp;
        if (port.waitForReadyRead(250)) {
            resp = port.readAll();
            while (port.waitForReadyRead(50)) {
                resp += port.readAll();
            }
        }
        return std::string(resp.constData(), static_cast<std::size_t>(resp.size()));
    };

    /* Non-destructive identify queries ONLY (proposal ARCH7C §6):
     *   Applesauce -> "?vers"          ADF-Copy -> 0x00 (ADFC_CMD_PING)
     * Neither moves the head, touches media, or writes flux. */
    const std::string vers = exchange(QByteArrayLiteral("?vers"));
    const std::string ping = exchange(QByteArray(1, '\0'));

    port.close();
    return classify_teensy_probe(vers, ping);
}

#else  /* !UFT_HAS_SERIALPORT */

TeensyDevice probe_teensy_serial(const QString &portName)
{
    (void)portName;
    /* This Qt build has no QtSerialPort module — there is no way to open
     * the port, so there is nothing to probe. Unknown is the honest
     * answer; callers must not treat it as "assume ADF-Copy/Applesauce". */
    return TeensyDevice::Unknown;
}

#endif  /* UFT_HAS_SERIALPORT */

}  // namespace uft::hal

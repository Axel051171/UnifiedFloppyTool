/**
 * @file gw_device_detector.cpp
 * @brief Greaseweazle device detector with Windows V4.0/V4.1 support
 * 
 * Supports detection of all Greaseweazle versions:
 * - F1 (STM32F1xx based)
 * - F7 (STM32F7xx based)
 * - V4.0 (RP2040 based)
 * - V4.1 (RP2040 based, USB-C)
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#include "gw_device_detector.h"

#include <QDebug>
#include <QThread>

#if GW_DETECTOR_AVAILABLE
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

/* Greaseweazle USB identifiers */
#define GW_VID_PIDCODES         0x1209
#define GW_PID_GREASEWEAZLE     0x4D69  /* All GW versions use this PID */

/* Greaseweazle protocol commands */
#define GW_CMD_GET_INFO         0x00
#define GW_CMD_GET_INFO_LEN     4       /* Total message length: cmd(1) + len(1) + subindex(2) */
#define GW_GETINFO_FIRMWARE     0

/* Known USB serial chip VID/PIDs that GW might appear as on Windows */
static const quint16 known_serial_vids[] = {
    0x1209,  /* pid.codes (official) */
    0x0483,  /* STMicroelectronics */
    0x2E8A,  /* Raspberry Pi (RP2040) */
    0x1A86,  /* CH340 (some clones) */
    0x10C4,  /* Silicon Labs CP210x */
    0x067B,  /* Prolific PL2303 */
    0x0403,  /* FTDI */
};

GwDeviceDetector::GwDeviceDetector(QObject *parent)
    : QObject(parent)
{
}

QStringList GwDeviceDetector::scan()
{
    QStringList devices;
    
#if GW_DETECTOR_AVAILABLE
    const auto ports = QSerialPortInfo::availablePorts();
    
    qDebug() << "GwDeviceDetector: Scanning" << ports.size() << "serial ports";
    
    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();
        QString desc = port.description();
        quint16 vid = port.vendorIdentifier();
        quint16 pid = port.productIdentifier();
        QString manufacturer = port.manufacturer();
        
        qDebug() << "  Port:" << portName 
                 << "VID:" << QString::number(vid, 16)
                 << "PID:" << QString::number(pid, 16)
                 << "Desc:" << desc
                 << "Mfr:" << manufacturer;
        
        bool likely_gw = false;
        QString reason;
        
        /* Method 1: Check official VID/PID */
        if (vid == GW_VID_PIDCODES && pid == GW_PID_GREASEWEAZLE) {
            likely_gw = true;
            reason = "Official VID/PID";
        }
        
        /* Method 2: Check description strings */
        if (!likely_gw) {
            if (desc.contains("Greaseweazle", Qt::CaseInsensitive) ||
                desc.contains("GW", Qt::CaseInsensitive) ||
                manufacturer.contains("Greaseweazle", Qt::CaseInsensitive) ||
                manufacturer.contains("Keir Fraser", Qt::CaseInsensitive)) {
                likely_gw = true;
                reason = "Description match";
            }
        }
        
        /* Method 3: RP2040-based detection (V4.0, V4.1) */
        if (!likely_gw && vid == 0x2E8A) {
            /* Raspberry Pi RP2040 - could be GW V4.x */
            likely_gw = true;
            reason = "RP2040 (possible GW V4.x)";
        }
        
        /* Method 4: Windows generic "USB Serial Device" with known VID */
        if (!likely_gw) {
            bool known_vid = false;
            for (size_t i = 0; i < sizeof(known_serial_vids)/sizeof(known_serial_vids[0]); i++) {
                if (vid == known_serial_vids[i]) {
                    known_vid = true;
                    break;
                }
            }
            
            if (known_vid && (desc.contains("USB Serial", Qt::CaseInsensitive) ||
                              desc.contains("Serial Port", Qt::CaseInsensitive) ||
                              desc.contains("COM", Qt::CaseInsensitive))) {
                /* On Windows, GW often appears as generic "USB Serial Device" */
                likely_gw = true;
                reason = "Generic USB Serial (will verify)";
            }
        }
        
        /* Method 5: STM32 Virtual COM Port (F1, F7 versions) */
        if (!likely_gw && vid == 0x0483) {
            if (desc.contains("Virtual COM", Qt::CaseInsensitive) ||
                desc.contains("STM32", Qt::CaseInsensitive)) {
                likely_gw = true;
                reason = "STM32 VCP (possible GW F1/F7)";
            }
        }
        
        /* Verify with protocol handshake if it looks promising */
        if (likely_gw) {
            qDebug() << "    Potential GW:" << reason << "- verifying...";
            
            if (isGreaseweazle(portName)) {
                devices << portName;
                QString hwVersion = getHardwareVersion(portName);
                QString fullDesc = QString("Greaseweazle %1").arg(hwVersion);
                emit deviceFound(portName, fullDesc);
                qDebug() << "    CONFIRMED:" << fullDesc;
            } else {
                qDebug() << "    Not a Greaseweazle (handshake failed)";
            }
        }
    }
    
    /* If no devices found, try protocol handshake on ALL ports as fallback */
    if (devices.isEmpty()) {
        qDebug() << "GwDeviceDetector: No devices found by VID/PID, trying all ports...";
        
        for (const QSerialPortInfo &port : ports) {
            QString portName = port.portName();
            
            /* Skip ports we already tried */
            if (devices.contains(portName)) continue;
            
            /* Skip obviously wrong ports */
            QString desc = port.description().toLower();
            if (desc.contains("bluetooth") || 
                desc.contains("modem") ||
                desc.contains("dial-up")) {
                continue;
            }
            
            if (isGreaseweazle(portName)) {
                devices << portName;
                QString hwVersion = getHardwareVersion(portName);
                QString fullDesc = QString("Greaseweazle %1 (fallback detection)").arg(hwVersion);
                emit deviceFound(portName, fullDesc);
                qDebug() << "    FOUND via fallback:" << portName << hwVersion;
            }
        }
    }
    
#else
    qDebug() << "GwDeviceDetector: SerialPort module not available";
#endif
    
    emit scanComplete(devices.size());
    return devices;
}

bool GwDeviceDetector::isGreaseweazle(const QString &portName)
{
#if GW_DETECTOR_AVAILABLE
    QSerialPort port;
    port.setPortName(portName);
    
    /* Try different baud rates - GW accepts any rate but we want to be safe */
    static const qint32 baudRates[] = { 115200, 9600, 1000000 };
    
    for (qint32 baud : baudRates) {
        port.setBaudRate(baud);
        port.setDataBits(QSerialPort::Data8);
        port.setParity(QSerialPort::NoParity);
        port.setStopBits(QSerialPort::OneStop);
        port.setFlowControl(QSerialPort::NoFlowControl);
        
        if (!port.open(QIODevice::ReadWrite)) {
            continue;
        }
        
        /* Clear any pending data */
        port.clear();
        QThread::msleep(50);
        
        /* Greaseweazle protocol: Send GET_INFO command
         * Format: [CMD_GET_INFO, LENGTH(4), SUBINDEX_LO, SUBINDEX_HI]
         * Response: [CMD_GET_INFO, ACK, ...data...] */
        QByteArray cmd;
        cmd.append(static_cast<char>(GW_CMD_GET_INFO));
        cmd.append(static_cast<char>(GW_CMD_GET_INFO_LEN));
        cmd.append(static_cast<char>(GW_GETINFO_FIRMWARE & 0xFF));       // Subindex low byte
        cmd.append(static_cast<char>((GW_GETINFO_FIRMWARE >> 8) & 0xFF)); // Subindex high byte
        
        port.write(cmd);
        if (!port.waitForBytesWritten(200)) {
            port.close();
            continue;
        }
        
        /* Wait for response */
        if (port.waitForReadyRead(500)) {
            QByteArray response = port.readAll();
            
            /* Give it more time to receive full response */
            QThread::msleep(50);
            while (port.waitForReadyRead(100)) {
                response.append(port.readAll());
            }
            
            port.close();
            
            /* Check for valid Greaseweazle response
             * Response format: [CMD_GET_INFO (0x00), ACK (0x00), ...firmware info...]
             * Minimum valid response is 4 bytes with CMD echo and ACK */
            if (response.size() >= 4) {
                unsigned char cmdEcho = static_cast<unsigned char>(response[0]);
                unsigned char ack = static_cast<unsigned char>(response[1]);
                
                if (cmdEcho == GW_CMD_GET_INFO && ack == 0x00) {
                    return true;
                }
            }
        } else {
            port.close();
        }
    }
    
    return false;
#else
    Q_UNUSED(portName);
    return false;
#endif
}

QString GwDeviceDetector::getHardwareVersion(const QString &portName)
{
#if GW_DETECTOR_AVAILABLE
    QSerialPort port;
    port.setPortName(portName);
    port.setBaudRate(115200);
    port.setDataBits(QSerialPort::Data8);
    port.setParity(QSerialPort::NoParity);
    port.setStopBits(QSerialPort::OneStop);
    port.setFlowControl(QSerialPort::NoFlowControl);
    
    if (!port.open(QIODevice::ReadWrite)) {
        return "Unknown";
    }
    
    port.clear();
    QThread::msleep(50);
    
    /* Send GET_INFO command for firmware version */
    QByteArray cmd;
    cmd.append(static_cast<char>(GW_CMD_GET_INFO));
    cmd.append(static_cast<char>(GW_CMD_GET_INFO_LEN));
    cmd.append(static_cast<char>(GW_GETINFO_FIRMWARE));
    
    port.write(cmd);
    port.waitForBytesWritten(200);
    
    QString version = "Unknown";
    
    if (port.waitForReadyRead(500)) {
        QByteArray response = port.readAll();
        QThread::msleep(50);
        while (port.waitForReadyRead(100)) {
            response.append(port.readAll());
        }
        
        if (response.size() >= 6) {
            /* Response contains firmware version at bytes 2-3 (big-endian) */
            unsigned char major = static_cast<unsigned char>(response[2]);
            unsigned char minor = static_cast<unsigned char>(response[3]);
            int fwVersion = (major << 8) | minor;
            
            /* Determine hardware version from firmware version */
            if (fwVersion >= 29) {
                /* V4.x uses firmware 29+ */
                version = QString("V4.x (FW %1)").arg(fwVersion);
            } else if (fwVersion >= 24) {
                /* F7 typically uses 24-28 */
                version = QString("F7 (FW %1)").arg(fwVersion);
            } else if (fwVersion >= 22) {
                /* F1 typically uses 22-23 */
                version = QString("F1 (FW %1)").arg(fwVersion);
            } else {
                version = QString("(FW %1)").arg(fwVersion);
            }
        }
    }
    
    port.close();
    return version;
#else
    Q_UNUSED(portName);
    return "Unknown";
#endif
}

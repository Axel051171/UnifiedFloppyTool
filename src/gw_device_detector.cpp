/**
 * @file gw_device_detector.cpp
 * @brief Greaseweazle Device Detection Implementation
 */

#include "gw_device_detector.h"
#include <QDebug>

// USB IDs für Greaseweazle (pid.codes registered)
static const quint16 GW_VID    = 0x1209;
static const quint16 GW_PID_F1 = 0x4D69;  // F1 firmware (AT32F403)
static const quint16 GW_PID_F7 = 0x4D69;  // F7 same PID (STM32F730)

GWDeviceDetector::GWDeviceDetector(QObject* parent)
    : QObject(parent)
{
}

QList<GWDeviceInfo> GWDeviceDetector::findDevices()
{
    QList<GWDeviceInfo> devices;
    
#if UFT_HAS_SERIALPORT
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        if (isGreaseweazle(port)) {
            GWDeviceInfo info;
            info.portName = port.portName();
            info.description = port.description();
            info.manufacturer = port.manufacturer();
            info.vendorId = port.vendorIdentifier();
            info.productId = port.productIdentifier();
            info.serialNumber = port.serialNumber();
            info.isVerified = true;
            
            devices.append(info);
            emit deviceFound(info);
        }
    }
#else
    qWarning() << "SerialPort support not available - hardware detection disabled";
#endif
    
    emit scanComplete(devices.count());
    return devices;
}

bool GWDeviceDetector::isGreaseweazlePort(const QString& portName)
{
#if UFT_HAS_SERIALPORT
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        if (port.portName() == portName) {
            return isGreaseweazle(port);
        }
    }
#else
    Q_UNUSED(portName);
#endif
    return false;
}

QStringList GWDeviceDetector::getAvailablePorts()
{
    QStringList ports;
    
#if UFT_HAS_SERIALPORT
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        ports << port.portName();
    }
    ports.sort();
#endif
    
    return ports;
}

#if UFT_HAS_SERIALPORT
bool GWDeviceDetector::isGreaseweazle(const QSerialPortInfo& port)
{
    // Methode 1: USB VID/PID
    if (port.hasVendorIdentifier() && port.hasProductIdentifier()) {
        if (port.vendorIdentifier() == GW_VID) {
            if (port.productIdentifier() == GW_PID_F1 || 
                port.productIdentifier() == GW_PID_F7) {
                return true;
            }
        }
    }
    
    // Methode 2: Description String
    QString desc = port.description().toLower();
    QString mfr = port.manufacturer().toLower();
    
    if (desc.contains("greaseweazle") || mfr.contains("greaseweazle") ||
        mfr.contains("keir fraser")) {
        return true;
    }
    
    // Methode 3: Serial Number Pattern (GW-xxxx)
    QString serial = port.serialNumber();
    if (serial.startsWith("GW-", Qt::CaseInsensitive)) {
        return true;
    }
    
    return false;
}
#endif

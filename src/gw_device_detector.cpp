/**
 * @file gw_device_detector.cpp
 * @brief Greaseweazle Device Detection Implementation
 */

#include "gw_device_detector.h"
#include <QTimer>
#include <QDebug>

GWDeviceDetector::GWDeviceDetector(QObject* parent)
    : QObject(parent)
{
    // Keywords für Description-basierte Erkennung
    m_gwDescriptionKeywords = {
        "Greaseweazle",
        "greaseweazle",
        "GW",
        "Keir Fraser"
    };
}

QStringList GWDeviceDetector::availablePorts() const
{
    QStringList ports;
    
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        ports << port.portName();
    }
    
    ports.sort();
    return ports;
}

QStringList GWDeviceDetector::detectGreaseweazleDevices() const
{
    QStringList gwPorts;
    
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        if (isGreaseweazle(port)) {
            gwPorts << port.portName();
        }
    }
    
    gwPorts.sort();
    return gwPorts;
}

GWDeviceInfo GWDeviceDetector::deviceInfo(const QString& portName) const
{
    GWDeviceInfo info;
    info.portName = portName;
    
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        if (port.portName() == portName) {
            info.description = port.description();
            info.manufacturer = port.manufacturer();
            info.vendorId = port.vendorIdentifier();
            info.productId = port.productIdentifier();
            info.serialNumber = port.serialNumber();
            info.isGreaseweazle = isGreaseweazle(port);
            break;
        }
    }
    
    return info;
}

bool GWDeviceDetector::isGreaseweazle(const QString& portName) const
{
    for (const QSerialPortInfo& port : QSerialPortInfo::availablePorts()) {
        if (port.portName() == portName) {
            return isGreaseweazle(port);
        }
    }
    return false;
}

bool GWDeviceDetector::isGreaseweazle(const QSerialPortInfo& port)
{
    // Methode 1: USB VID/PID
    if (port.hasVendorIdentifier() && port.hasProductIdentifier()) {
        if (port.vendorIdentifier() == GW_VID) {
            if (port.productIdentifier() == GW_PID || 
                port.productIdentifier() == GW_PID_OLD) {
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

void GWDeviceDetector::startMonitoring()
{
    if (!m_monitorTimer) {
        m_monitorTimer = new QTimer(this);
        connect(m_monitorTimer, &QTimer::timeout, 
                this, &GWDeviceDetector::checkForChanges);
    }
    
    m_lastKnownPorts = availablePorts();
    m_monitorTimer->start(2000);  // Check every 2 seconds
}

void GWDeviceDetector::stopMonitoring()
{
    if (m_monitorTimer) {
        m_monitorTimer->stop();
    }
}

void GWDeviceDetector::checkForChanges()
{
    QStringList currentPorts = availablePorts();
    
    // Finde neue Ports
    for (const QString& port : currentPorts) {
        if (!m_lastKnownPorts.contains(port)) {
            if (isGreaseweazle(port)) {
                qDebug() << "GW Device connected:" << port;
                emit deviceConnected(port);
            }
        }
    }
    
    // Finde entfernte Ports
    for (const QString& port : m_lastKnownPorts) {
        if (!currentPorts.contains(port)) {
            qDebug() << "GW Device disconnected:" << port;
            emit deviceDisconnected(port);
        }
    }
    
    m_lastKnownPorts = currentPorts;
}

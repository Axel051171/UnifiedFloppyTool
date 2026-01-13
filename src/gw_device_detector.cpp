#include "gw_device_detector.h"

#include <QDebug>

#if GW_DETECTOR_AVAILABLE
#include <QSerialPort>
#include <QSerialPortInfo>
#endif

GwDeviceDetector::GwDeviceDetector(QObject *parent)
    : QObject(parent)
{
}

QStringList GwDeviceDetector::scan()
{
    QStringList devices;
    
#if GW_DETECTOR_AVAILABLE
    const auto ports = QSerialPortInfo::availablePorts();
    
    for (const QSerialPortInfo &port : ports) {
        // Check for Greaseweazle VID/PID or description
        if (port.vendorIdentifier() == 0x1209 && 
            port.productIdentifier() == 0x4d69) {
            // Official Greaseweazle VID/PID
            devices << port.portName();
            emit deviceFound(port.portName(), port.description());
        } else if (port.description().contains("Greaseweazle", Qt::CaseInsensitive)) {
            devices << port.portName();
            emit deviceFound(port.portName(), port.description());
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
    port.setBaudRate(115200);
    
    if (!port.open(QIODevice::ReadWrite)) {
        return false;
    }
    
    // Send info command (0x00)
    port.write(QByteArray(1, 0x00));
    port.waitForBytesWritten(100);
    
    if (port.waitForReadyRead(500)) {
        QByteArray response = port.readAll();
        port.close();
        // Check for valid Greaseweazle response
        return response.size() >= 4 && response[0] == 0x00;
    }
    
    port.close();
    return false;
#else
    Q_UNUSED(portName);
    return false;
#endif
}

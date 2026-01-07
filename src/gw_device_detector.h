/**
 * @file uft_gw_device_detector.h
 * 
 * Basierend auf Sovox _get_available_com_ports() Logik.
 * Verwendet QSerialPortInfo wenn verfügbar.
 */

#ifndef UFT_GW_DEVICE_DETECTOR_H
#define UFT_GW_DEVICE_DETECTOR_H

/* Check if SerialPort support is available */
#ifndef UFT_HAS_SERIALPORT
#define UFT_HAS_SERIALPORT 0
#endif

#include <QObject>
#include <QStringList>

#if UFT_HAS_SERIALPORT
#include <QSerialPortInfo>
#endif

/**
 */
struct GWDeviceInfo {
    QString portName;           // COM10, /dev/ttyACM0, etc.
    quint16 vendorId;
    quint16 productId;
    QString serialNumber;
    bool isVerified;            // True wenn als GW erkannt
};

/**
 * 
 */
class GWDeviceDetector : public QObject
{
    Q_OBJECT

public:
    explicit GWDeviceDetector(QObject *parent = nullptr);
    
    /**
     * @return Liste der gefundenen Geräte
     */
    QList<GWDeviceInfo> findDevices();
    
    /**
     * @param portName Name des Ports (z.B. "COM10")
     */
    bool isGreaseweazlePort(const QString& portName);
    
    /**
     * @brief Gibt Liste aller COM-Ports zurück
     */
    QStringList getAvailablePorts();
    
#if UFT_HAS_SERIALPORT
    /**
     */
    static bool isGreaseweazle(const QSerialPortInfo& port);
#endif

signals:
    void deviceFound(const GWDeviceInfo& device);
    void scanComplete(int count);

private:
    static const quint16 UFT_GW_VID = 0x1209;  // pid.codes VID
};

#endif // UFT_GW_DEVICE_DETECTOR_H

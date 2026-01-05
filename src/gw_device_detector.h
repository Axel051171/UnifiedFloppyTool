/**
 * @file gw_device_detector.h
 * @brief Greaseweazle Device Detection
 * 
 * Basierend auf Sovox _get_available_com_ports() Logik.
 * Verwendet QSerialPortInfo wenn verfügbar.
 */

#ifndef GW_DEVICE_DETECTOR_H
#define GW_DEVICE_DETECTOR_H

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
 * @brief Info über ein erkanntes Greaseweazle-Gerät
 */
struct GWDeviceInfo {
    QString portName;           // COM10, /dev/ttyACM0, etc.
    QString description;        // "Greaseweazle F7"
    QString manufacturer;       // "Keir Fraser"
    quint16 vendorId;
    quint16 productId;
    QString serialNumber;
    bool isVerified;            // True wenn als GW erkannt
};

/**
 * @brief Greaseweazle Device Detector
 * 
 * Findet und identifiziert Greaseweazle-Controller.
 */
class GWDeviceDetector : public QObject
{
    Q_OBJECT

public:
    explicit GWDeviceDetector(QObject *parent = nullptr);
    
    /**
     * @brief Findet alle verfügbaren Greaseweazle-Geräte
     * @return Liste der gefundenen Geräte
     */
    QList<GWDeviceInfo> findDevices();
    
    /**
     * @brief Prüft ob ein bestimmter Port ein Greaseweazle ist
     * @param portName Name des Ports (z.B. "COM10")
     * @return true wenn Greaseweazle erkannt
     */
    bool isGreaseweazlePort(const QString& portName);
    
    /**
     * @brief Gibt Liste aller COM-Ports zurück
     */
    QStringList getAvailablePorts();
    
#if UFT_HAS_SERIALPORT
    /**
     * @brief Prüft ob ein Port ein Greaseweazle ist (via QSerialPortInfo)
     */
    static bool isGreaseweazle(const QSerialPortInfo& port);
#endif

signals:
    void deviceFound(const GWDeviceInfo& device);
    void scanComplete(int count);

private:
    // Greaseweazle USB IDs
    static const quint16 GW_VID = 0x1209;  // pid.codes VID
    static const quint16 GW_PID_F1 = 0x4D69;  // F1 Greaseweazle
    static const quint16 GW_PID_F7 = 0x0001;  // F7 Greaseweazle
};

#endif // GW_DEVICE_DETECTOR_H

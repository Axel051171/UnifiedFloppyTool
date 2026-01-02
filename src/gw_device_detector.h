/**
 * @file gw_device_detector.h
 * @brief Greaseweazle Device Detection
 * 
 * Basierend auf Sovox _get_available_com_ports() Logik.
 * Verwendet QSerialPortInfo statt pyserial.
 */

#ifndef GW_DEVICE_DETECTOR_H
#define GW_DEVICE_DETECTOR_H

#include <QObject>
#include <QStringList>
#include <QSerialPortInfo>

/**
 * @brief Info über ein erkanntes Greaseweazle-Gerät
 */
struct GWDeviceInfo {
    QString portName;           // COM10, /dev/ttyACM0, etc.
    QString description;        // "Greaseweazle F7"
    QString manufacturer;       // "Keir Fraser"
    quint16 vendorId = 0;
    quint16 productId = 0;
    QString serialNumber;
    bool isGreaseweazle = false;
};

/**
 * @brief Erkennt Greaseweazle-Geräte an USB-Ports
 * 
 * Greaseweazle USB IDs:
 * - VID: 0x1209 (Generic)
 * - PID: 0x4d69 (Greaseweazle) oder 0x0001 (alte Firmware)
 * 
 * Alternative Erkennung über Description String.
 */
class GWDeviceDetector : public QObject
{
    Q_OBJECT

public:
    explicit GWDeviceDetector(QObject* parent = nullptr);
    
    /**
     * @brief Scannt nach allen verfügbaren seriellen Ports
     * @return Liste aller Port-Namen
     */
    QStringList availablePorts() const;
    
    /**
     * @brief Scannt nach Greaseweazle-Geräten
     * @return Liste der erkannten Greaseweazle-Ports
     */
    QStringList detectGreaseweazleDevices() const;
    
    /**
     * @brief Holt detaillierte Info zu einem Port
     */
    GWDeviceInfo deviceInfo(const QString& portName) const;
    
    /**
     * @brief Prüft ob ein Port ein Greaseweazle ist
     */
    bool isGreaseweazle(const QString& portName) const;
    
    /**
     * @brief Prüft ob ein Port ein Greaseweazle ist (via QSerialPortInfo)
     */
    static bool isGreaseweazle(const QSerialPortInfo& port);

signals:
    void deviceConnected(const QString& portName);
    void deviceDisconnected(const QString& portName);

public slots:
    /**
     * @brief Startet Überwachung auf neue/entfernte Geräte
     */
    void startMonitoring();
    
    /**
     * @brief Stoppt Überwachung
     */
    void stopMonitoring();

private:
    // Greaseweazle USB Vendor/Product IDs
    static constexpr quint16 GW_VID = 0x1209;      // Generic VID
    static constexpr quint16 GW_PID = 0x4d69;      // Greaseweazle PID
    static constexpr quint16 GW_PID_OLD = 0x0001;  // Alte Firmware
    
    // Alternative Erkennung über Strings
    QStringList m_gwDescriptionKeywords;
    
    // Monitoring
    QStringList m_lastKnownPorts;
    class QTimer* m_monitorTimer = nullptr;
    
private slots:
    void checkForChanges();
};

#endif // GW_DEVICE_DETECTOR_H

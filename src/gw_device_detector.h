#ifndef GW_DEVICE_DETECTOR_H
#define GW_DEVICE_DETECTOR_H

#include <QObject>
#include <QString>
#include <QStringList>

/* SerialPort availability check */
#ifdef UFT_HAS_SERIALPORT
#define GW_DETECTOR_AVAILABLE 1
#else
#define GW_DETECTOR_AVAILABLE 0
#endif

/**
 * @brief Greaseweazle device detector
 * 
 * Scans serial ports for Greaseweazle devices.
 * 
 * @note Requires Qt SerialPort module. If not available,
 *       detection will return empty results.
 */
class GwDeviceDetector : public QObject
{
    Q_OBJECT
    
public:
    explicit GwDeviceDetector(QObject *parent = nullptr);
    
    /**
     * @brief Scan for Greaseweazle devices
     * @return List of detected device paths
     */
    QStringList scan();
    
    /**
     * @brief Check if a specific port is a Greaseweazle
     */
    bool isGreaseweazle(const QString &portName);
    
    /**
     * @brief Check if SerialPort support is available
     */
    static bool isAvailable() { return GW_DETECTOR_AVAILABLE != 0; }

signals:
    void deviceFound(const QString &portName, const QString &description);
    void scanComplete(int devicesFound);
};

#endif // GW_DEVICE_DETECTOR_H

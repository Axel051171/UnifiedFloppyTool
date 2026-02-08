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
 * Supports all Greaseweazle hardware versions:
 * - F1 (STM32F1xx based, original design)
 * - F7 (STM32F7xx based, improved version)
 * - V4.0 (RP2040 based)
 * - V4.1 (RP2040 based, USB-C connector)
 * 
 * Windows detection note:
 * On Windows, GW V4.x devices may appear as generic "USB Serial Device"
 * without proper VID/PID. This detector uses protocol handshake
 * as a fallback detection method.
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
     * 
     * Scans all serial ports using multiple detection methods:
     * 1. Official VID/PID (0x1209:0x4D69)
     * 2. Description string matching
     * 3. RP2040 VID detection (for V4.x)
     * 4. Protocol handshake fallback
     * 
     * @return List of detected device port names
     */
    QStringList scan();
    
    /**
     * @brief Check if a specific port is a Greaseweazle
     * 
     * Performs protocol handshake to verify device.
     * 
     * @param portName Serial port name (e.g., "COM3" or "/dev/ttyACM0")
     * @return true if device responds as Greaseweazle
     */
    bool isGreaseweazle(const QString &portName);
    
    /**
     * @brief Get hardware version string
     * 
     * Queries firmware version and determines hardware type.
     * 
     * @param portName Serial port name
     * @return Version string (e.g., "V4.x (FW 29)")
     */
    QString getHardwareVersion(const QString &portName);
    
    /**
     * @brief Check if SerialPort support is available
     */
    static bool isAvailable() { return GW_DETECTOR_AVAILABLE != 0; }

signals:
    /**
     * @brief Emitted when a Greaseweazle device is found
     * @param portName Serial port name
     * @param description Device description
     */
    void deviceFound(const QString &portName, const QString &description);
    
    /**
     * @brief Emitted when scan is complete
     * @param devicesFound Number of devices found
     */
    void scanComplete(int devicesFound);
};

#endif // GW_DEVICE_DETECTOR_H

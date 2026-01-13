#ifndef GREASEWEAZLEHARDWAREPROVIDER_H
#define GREASEWEAZLEHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QMutex>

/* ═══════════════════════════════════════════════════════════════════════════════
 * SerialPort Availability Check
 * ═══════════════════════════════════════════════════════════════════════════════ */
#ifdef UFT_HAS_SERIALPORT
#include <QSerialPort>
#define GW_SERIAL_AVAILABLE 1
#else
#define GW_SERIAL_AVAILABLE 0
// Forward declaration for compilation without SerialPort
class QSerialPort;
#endif

/**
 * @brief Greaseweazle hardware provider
 * 
 * Greaseweazle is an open-source USB floppy controller that can read and write
 * flux-level data from virtually any floppy disk format.
 * 
 * This provider supports:
 * - Direct serial communication with Greaseweazle firmware
 * - Track read/write at flux level
 * - Multiple drive types (3.5" DD/HD, 5.25" DD/HD/QD)
 * 
 * @note Requires Qt SerialPort module. If not available, this provider
 *       will report as unavailable but the application will still compile.
 */
class GreaseweazleHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit GreaseweazleHardwareProvider(QObject *parent = nullptr);
    ~GreaseweazleHardwareProvider() override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Basic Interface
    // ═══════════════════════════════════════════════════════════════════════════
    
    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Connection Management
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Motor & Head Control
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool setMotor(bool on) override;
    bool seekCylinder(int cylinder) override;
    bool selectHead(int head) override;
    int currentCylinder() const override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Read/Write Operations
    // ═══════════════════════════════════════════════════════════════════════════
    
    TrackData readTrack(const ReadParams &params) override;
    QByteArray readRawFlux(int cylinder, int head, int revolutions = 2) override;
    OperationResult writeTrack(const WriteParams &params, const QByteArray &data) override;
    bool writeRawFlux(int cylinder, int head, const QByteArray &fluxData) override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Utility
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool getGeometry(int &tracks, int &heads) override;
    double measureRPM() override;
    bool recalibrate() override;
    
    /**
     * @brief Check if SerialPort support is available
     */
    static bool isSerialPortAvailable() { return GW_SERIAL_AVAILABLE != 0; }

private:
#if GW_SERIAL_AVAILABLE
    QSerialPort *m_serialPort = nullptr;
#endif
    QString m_devicePath;
    QString m_hardwareType;
    int m_baudRate = 115200;
    int m_currentCylinder = 0;
    int m_currentHead = 0;
    bool m_motorOn = false;
    mutable QMutex m_mutex;
    
    // Protocol helpers
    bool sendCommand(uint8_t cmd, const QByteArray &payload = QByteArray());
    QByteArray readResponse(int expectedSize, int timeoutMs = 5000);
    bool waitForAck(int timeoutMs = 1000);
};

#endif // GREASEWEAZLEHARDWAREPROVIDER_H

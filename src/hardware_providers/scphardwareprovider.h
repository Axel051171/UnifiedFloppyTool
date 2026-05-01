#ifndef SCPHARDWAREPROVIDER_H
#define SCPHARDWAREPROVIDER_H

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
#define SCP_SERIAL_AVAILABLE 1
#else
#define SCP_SERIAL_AVAILABLE 0
// Forward declaration for compilation without SerialPort
class QSerialPort;
#endif

/**
 * @brief SuperCard Pro hardware provider
 *
 * The SuperCard Pro is a high-precision USB floppy controller designed by
 * Jim Drew / CBM Stuff. It uses an FTDI FT240-X USB FIFO (or VCP serial)
 * with a binary command protocol.
 *
 * This provider supports:
 * - Direct serial communication with SuperCard Pro firmware
 * - Track read/write at flux level (40 MHz / 25 ns resolution)
 * - Multiple drive types (3.5" DD/HD, 5.25" DD/HD, 8" SSSD)
 * - 512 KB onboard static RAM for flux buffering
 *
 * Protocol reference: SCP SDK v1.7 (cbmstuff.com)
 *
 * @note Requires Qt SerialPort module. If not available, this provider
 *       will report as unavailable but the application will still compile.
 */
class SCPHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit SCPHardwareProvider(QObject *parent = nullptr);
    ~SCPHardwareProvider() override;

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
    static bool isSerialPortAvailable() { return SCP_SERIAL_AVAILABLE != 0; }

private:
#if SCP_SERIAL_AVAILABLE
    QSerialPort *m_serialPort = nullptr;
#endif
    QString m_devicePath;
    QString m_hardwareType;
    int m_baudRate = 9600;
    int m_currentCylinder = 0;
    int m_currentHead = 0;
    bool m_motorOn = false;
    bool m_driveSelected = false;
    uint8_t m_hwVersion = 0;
    uint8_t m_fwVersion = 0;
    mutable QMutex m_mutex;

    // Protocol helpers
    bool sendCommand(uint8_t cmd, const QByteArray &payload = QByteArray());
    QByteArray readResponse(int expectedSize, int timeoutMs = 5000);
    bool checkResponse(uint8_t expectedCmd, int timeoutMs = 5000);
    uint8_t computeChecksum(const QByteArray &data) const;

    // High-level helpers
    bool selectDrive(int drive = 0);
    bool deselectDrive(int drive = 0);
    bool sendRamToUsb(uint32_t offset, uint32_t length, QByteArray &outData);
    bool loadRamFromUsb(uint32_t offset, uint32_t length, const QByteArray &inData);
};

#endif // SCPHARDWAREPROVIDER_H

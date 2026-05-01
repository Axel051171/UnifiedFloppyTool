#ifndef APPLESAUCEHARDWAREPROVIDER_H
#define APPLESAUCEHARDWAREPROVIDER_H

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
#define AS_SERIAL_AVAILABLE 1
#else
#define AS_SERIAL_AVAILABLE 0
// Forward declaration for compilation without SerialPort
class QSerialPort;
#endif

/**
 * @brief Applesauce hardware provider
 *
 * Applesauce is a premium Apple-focused USB floppy controller designed by
 * John Googin / Evolution Interactive. It captures flux-level data with high
 * precision from Apple II, Apple III, Macintosh, and Lisa disk drives.
 *
 * Protocol reference: wiki.applesaucefdc.com
 *
 * Key protocol facts:
 * - USB: VID=0x16C0 PID=0x0483 (Teensy), 12 Mb/s (standard), 100+ Mb/s (AS+)
 * - Commands: Human-readable text strings, case-insensitive, newline-terminated
 * - Responses: Single-character status codes (./!/?/+/-/v) followed by data
 * - Data buffer: 160K (standard), 420K (Applesauce+)
 * - Sample clock: 8 MHz (125 ns resolution)
 *
 * This provider supports:
 * - Direct serial communication with Applesauce firmware
 * - Track read/write at flux level
 * - Apple 5.25" and 3.5" drive types
 * - Power supply and motor control
 * - RPM measurement via index sync
 *
 * @note Requires Qt SerialPort module. If not available, this provider
 *       will report as unavailable but the application will still compile.
 */
class ApplesauceHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit ApplesauceHardwareProvider(QObject *parent = nullptr);
    ~ApplesauceHardwareProvider() override;

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
    static bool isSerialPortAvailable() { return AS_SERIAL_AVAILABLE != 0; }

    // ═══════════════════════════════════════════════════════════════════════════
    // Applesauce-Specific
    // ═══════════════════════════════════════════════════════════════════════════

    /** Applesauce USB identifiers */
    static constexpr quint16 AS_VID = 0x16C0;
    static constexpr quint16 AS_PID = 0x0483;

    /** Applesauce sample clock: 8 MHz (125 ns resolution) */
    static constexpr uint32_t AS_SAMPLE_CLOCK = 8000000;

    /** Data buffer sizes */
    static constexpr uint32_t AS_BUFFER_STANDARD = 163840;   /* 160K */
    static constexpr uint32_t AS_BUFFER_PLUS     = 430080;   /* 420K */

    /** Response status codes */
    enum ResponseCode : char {
        RESP_OK           = '.',   /**< OK / acknowledged */
        RESP_ERROR        = '!',   /**< Bad / error */
        RESP_UNKNOWN_CMD  = '?',   /**< Command not recognized */
        RESP_ON           = '+',   /**< On (query response) */
        RESP_OFF          = '-',   /**< Off (query response) */
        RESP_NO_POWER     = 'v',   /**< No power */
    };

private:
#if AS_SERIAL_AVAILABLE
    QSerialPort *m_serialPort = nullptr;
#endif
    QString m_devicePath;
    QString m_hardwareType;
    int m_baudRate = 0;          /* Auto-detect (USB CDC ignores baud) */
    int m_currentCylinder = 0;
    int m_currentHead = 0;
    bool m_motorOn = false;
    bool m_psuOn = false;
    QString m_firmwareVersion;
    QString m_pcbRevision;
    QString m_driveKind;         /* "5.25", "3.5", "PC", "NONE" */
    uint32_t m_maxBufferSize = AS_BUFFER_STANDARD;
    mutable QMutex m_mutex;

    // ═══════════════════════════════════════════════════════════════════════════
    // Protocol Helpers (text-based, newline-terminated)
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Send a text command and read the response line
     * @param command Command string (without newline)
     * @param timeoutMs Read timeout in milliseconds
     * @return Response string (trimmed, without newline)
     */
    QString sendCommand(const QString &command, int timeoutMs = 3000);

    /**
     * @brief Send a command that will be followed by a binary data upload
     * @param command Command string
     * @param data Binary data to send after the command
     * @param timeoutMs Read timeout
     * @return Response string
     */
    QString sendCommandWithData(const QString &command, const QByteArray &data, int timeoutMs = 10000);

    /**
     * @brief Download binary data from the device buffer
     * @param size Number of bytes to download
     * @param timeoutMs Read timeout
     * @return Downloaded data
     */
    QByteArray downloadData(uint32_t size, int timeoutMs = 30000);

    /**
     * @brief Read a single response line (up to newline)
     * @param timeoutMs Read timeout
     * @return Response line (trimmed)
     */
    QString readResponseLine(int timeoutMs = 3000);

    /**
     * @brief Check if a response indicates success
     * @param response Response string from sendCommand
     * @return true if response starts with '.' (OK)
     */
    static bool isOk(const QString &response);

    /**
     * @brief Check if a response indicates error
     * @param response Response string from sendCommand
     * @return true if response starts with '!' (error)
     */
    static bool isError(const QString &response);

    /**
     * @brief Check if a response indicates the feature is on
     * @param response Response string from sendCommand
     * @return true if response starts with '+' (on)
     */
    static bool isOn(const QString &response);

    /**
     * @brief Ensure PSU is on (enable if not already)
     * @return true if PSU is on
     */
    bool ensurePowerOn();
};

#endif // APPLESAUCEHARDWAREPROVIDER_H

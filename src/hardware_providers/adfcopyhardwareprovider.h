/*
 * SPDX-FileCopyrightText: 2024-2026 Axel Kramer
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ADF-Copy hardware provider for Universal Floppy Tool
 */

#ifndef ADFCOPYHARDWAREPROVIDER_H
#define ADFCOPYHARDWAREPROVIDER_H

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
#define ADFC_SERIAL_AVAILABLE 1
#else
#define ADFC_SERIAL_AVAILABLE 0
// Forward declaration for compilation without SerialPort
class QSerialPort;
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF-Copy Protocol Constants — USB Identification
 * ═══════════════════════════════════════════════════════════════════════════════ */
static constexpr uint16_t ADFC_USB_VID       = 0x16C0;
static constexpr uint16_t ADFC_USB_PID       = 0x0483;
static constexpr int      ADFC_BAUD_RATE     = 115200;

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF-Copy Protocol Constants — Command Bytes
 * ═══════════════════════════════════════════════════════════════════════════════ */
static constexpr uint8_t ADFC_CMD_PING           = 0x00;
static constexpr uint8_t ADFC_CMD_INIT           = 0x01;
static constexpr uint8_t ADFC_CMD_SEEK           = 0x02;
static constexpr uint8_t ADFC_CMD_GET_VOLNAME    = 0x03;
static constexpr uint8_t ADFC_CMD_READ_TRACK     = 0x04;
static constexpr uint8_t ADFC_CMD_WRITE_TRACK    = 0x05;
static constexpr uint8_t ADFC_CMD_READ_FLUX      = 0x06;
static constexpr uint8_t ADFC_CMD_READ_SCP       = 0x07;
static constexpr uint8_t ADFC_CMD_FORMAT_TRACK   = 0x08;
static constexpr uint8_t ADFC_CMD_COMPARE_TRACK  = 0x09;
static constexpr uint8_t ADFC_CMD_DISK_INFO      = 0x0A;
static constexpr uint8_t ADFC_CMD_GET_STATUS     = 0x0B;
static constexpr uint8_t ADFC_CMD_SET_TIMING     = 0x0C;
static constexpr uint8_t ADFC_CMD_SAVE_SETTINGS  = 0x0D;
static constexpr uint8_t ADFC_CMD_CLEANING_MODE  = 0x0E;

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF-Copy Protocol Constants — Response Tokens
 * ═══════════════════════════════════════════════════════════════════════════════ */
static constexpr char ADFC_RSP_OK        = 'O';
static constexpr char ADFC_RSP_ERROR     = 'E';
static constexpr char ADFC_RSP_WEAKTRACK = 'W';
static constexpr char ADFC_RSP_NDOS      = 'N';
static constexpr char ADFC_RSP_WPROT     = 'P';
static constexpr char ADFC_RSP_NODISK    = 'D';

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF-Copy Protocol Constants — Status Flags
 * ═══════════════════════════════════════════════════════════════════════════════ */
static constexpr uint8_t ADFC_STATUS_DISK_PRESENT = (1 << 0);
static constexpr uint8_t ADFC_STATUS_WRITE_PROT   = (1 << 1);
static constexpr uint8_t ADFC_STATUS_MOTOR_ON     = (1 << 2);
static constexpr uint8_t ADFC_STATUS_FLUX_CAPABLE = (1 << 3);

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF-Copy Protocol Constants — Amiga Disk Geometry
 * ═══════════════════════════════════════════════════════════════════════════════ */
static constexpr int ADFC_DD_TRACKS    = 160;   // 80 cylinders x 2 heads
static constexpr int ADFC_DD_SECTORS   = 11;    // Amiga DD sectors per track
static constexpr int ADFC_SECTOR_SIZE  = 512;
static constexpr int ADFC_HD_SECTORS   = 22;    // Amiga HD sectors per track

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF-Copy Protocol Constants — Timing Parameters
 * ═══════════════════════════════════════════════════════════════════════════════ */
static constexpr uint8_t ADFC_TIMING_STEP_DELAY  = 0x01;
static constexpr uint8_t ADFC_TIMING_HEAD_SETTLE = 0x02;
static constexpr uint8_t ADFC_TIMING_MOTOR_ON    = 0x03;
static constexpr uint8_t ADFC_TIMING_MFM_WINDOW  = 0x04;

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADFCopyHardwareProvider
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief ADF-Copy hardware provider
 *
 * ADF-Copy is a dedicated Amiga floppy disk copier that communicates over
 * a USB serial interface. It supports reading and writing ADF images,
 * flux-level captures to SCP format, disk formatting, and volume label
 * retrieval.
 *
 * This provider supports:
 * - ADF image read/write with configurable retries
 * - Flux-level read to SCP format
 * - Disk formatting (quick and full)
 * - Volume label and disk info queries
 * - Configurable drive timing parameters
 * - Cleaning mode for drive head maintenance
 *
 * @note Requires Qt SerialPort module. If not available, this provider
 *       will report as unavailable but the application will still compile.
 */
class ADFCopyHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit ADFCopyHardwareProvider(QObject *parent = nullptr);
    ~ADFCopyHardwareProvider() override;

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
    static bool isSerialPortAvailable() { return ADFC_SERIAL_AVAILABLE != 0; }

    // ═══════════════════════════════════════════════════════════════════════════
    // ADF-Copy Specific Operations
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Read an entire disk to an ADF file
     * @param destPath     Destination file path for the ADF image
     * @param startTrack   First track to read (0-159)
     * @param endTrack     Last track to read (0-159)
     * @param retries      Number of retries per track on read errors
     * @return true on success
     */
    bool readAdf(const QString &destPath, uint8_t startTrack, uint8_t endTrack, uint8_t retries);

    /**
     * @brief Write an ADF file to disk
     * @param sourcePath   Path to the ADF image to write
     * @param indexAlign   Align writes to the index pulse
     * @param verify       Verify each track after writing
     * @return true on success
     */
    bool writeAdf(const QString &sourcePath, bool indexAlign, bool verify);

    /**
     * @brief Read flux data and save to SCP format
     * @param destPath     Destination file path for the SCP image
     * @param startTrack   First track to read (0-159)
     * @param endTrack     Last track to read (0-159)
     * @param revolutions  Number of revolutions to capture per track
     * @return true on success
     */
    bool readFluxScp(const QString &destPath, uint8_t startTrack, uint8_t endTrack, uint8_t revolutions);

    /**
     * @brief Format the disk with empty Amiga OFS/FFS tracks
     * @param quickFormat  If true, only write root block and bitmap; otherwise format all tracks
     * @return true on success
     */
    bool formatDisk(bool quickFormat);

    /**
     * @brief Retrieve the Amiga volume label from the disk
     * @param label  Output: the volume label string
     * @return true on success, false if no disk or not an AmigaDOS disk
     */
    bool getVolumeLabel(QString &label);

    /**
     * @brief Read boot block and root block from the disk
     * @param bootBlock  Output: raw boot block data (1024 bytes)
     * @param rootBlock  Output: raw root block data (512 bytes)
     * @return true on success
     */
    bool diskInfo(QByteArray &bootBlock, QByteArray &rootBlock);

    /**
     * @brief Set a drive timing parameter
     * @param param  Timing parameter ID (ADFC_TIMING_*)
     * @param value  Timing value in microseconds
     * @return true on success
     */
    bool setTiming(uint8_t param, uint16_t value);

    /**
     * @brief Save current timing settings to non-volatile storage
     * @return true on success
     */
    bool saveSettings();

    /**
     * @brief Enter cleaning mode: move head back and forth for the given duration
     * @param durationSeconds  Duration of cleaning cycle in seconds
     * @return true on success
     */
    bool cleaningMode(uint16_t durationSeconds);

    // ═══════════════════════════════════════════════════════════════════════════
    // Low-Level Commands (public for testing)
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief Send a PING command to verify communication
     * @return true if the device responds correctly
     */
    bool cmdPing();

    /**
     * @brief Initialize the device and detect attached drive
     * @return true if initialization succeeded
     */
    bool cmdInit();

    /**
     * @brief Seek to a specific track
     * @param track  Track number (0-159, where track = cylinder*2 + head)
     * @return true on success
     */
    bool cmdSeek(uint8_t track);

    /**
     * @brief Query the current device status flags
     * @return Bitmask of ADFC_STATUS_* flags, or 0 on error
     */
    uint8_t cmdGetStatus();

    /**
     * @brief Read a single track of decoded MFM data
     * @param track    Track number (0-159)
     * @param retries  Number of retries for weak/damaged sectors
     * @param wasWeak  Optional output: set to true if the track had weak bits
     * @return Raw sector data (11 * 512 = 5632 bytes for DD), or empty on failure
     */
    QByteArray cmdReadTrack(uint8_t track, uint8_t retries, bool *wasWeak = nullptr);

    /**
     * @brief Write a single track of decoded MFM data
     * @param track       Track number (0-159)
     * @param data        Sector data to write
     * @param indexAlign  Align write to the index pulse
     * @return true on success
     */
    bool cmdWriteTrack(uint8_t track, const QByteArray &data, bool indexAlign);

    /**
     * @brief Read raw flux transitions from a track
     * @param track        Track number (0-159)
     * @param revolutions  Number of revolutions to capture
     * @return Raw flux timing data, or empty on failure
     */
    QByteArray cmdReadFlux(uint8_t track, uint8_t revolutions);

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // Serial Communication Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    bool sendCommand(uint8_t cmd, const QByteArray &payload = QByteArray());
    QByteArray readResponse(int expectedSize, int timeoutMs = 5000);
    bool waitForOK(int timeoutMs = 1000);
    void flushInput();

    // ═══════════════════════════════════════════════════════════════════════════
    // Internal Utilities
    // ═══════════════════════════════════════════════════════════════════════════

    void parseVersionString(const QByteArray &data);
    QByteArray buildWriteFrame(uint8_t track, const QByteArray &data, bool indexAlign);

    // ═══════════════════════════════════════════════════════════════════════════
    // Member Variables
    // ═══════════════════════════════════════════════════════════════════════════

#if ADFC_SERIAL_AVAILABLE
    QSerialPort *m_serialPort = nullptr;
#endif
    QString     m_devicePath;
    QString     m_hardwareType;
    int         m_baudRate = ADFC_BAUD_RATE;
    QString     m_fwVersion;
    bool        m_fluxCapable = false;
    bool        m_abortRequested = false;
    int         m_currentCylinder = 0;
    int         m_currentHead = 0;
    bool        m_motorOn = false;
    mutable QMutex m_mutex;

    // ═══════════════════════════════════════════════════════════════════════════
    // Timeout Constants
    // ═══════════════════════════════════════════════════════════════════════════

    static constexpr int ADFC_TIMEOUT_DEFAULT_MS   = 5000;
    static constexpr int ADFC_TIMEOUT_READ_MS      = 10000;
    static constexpr int ADFC_TIMEOUT_WRITE_MS     = 15000;
    static constexpr int ADFC_TIMEOUT_FORMAT_MS    = 30000;
    static constexpr int ADFC_TIMEOUT_FLUX_MS      = 20000;
    static constexpr int ADFC_TIMEOUT_CLEANING_MS  = 120000;
};

#endif // ADFCOPYHARDWAREPROVIDER_H

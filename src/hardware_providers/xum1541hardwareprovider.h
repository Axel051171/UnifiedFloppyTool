#ifndef XUM1541HARDWAREPROVIDER_H
#define XUM1541HARDWAREPROVIDER_H

#include "hardwareprovider.h"
#include "xum1541_usb.h"

#include <QMutex>
#include <QByteArray>
#include <QString>
#include <QStringList>

/**
 * @brief XUM1541/ZoomFloppy Hardware Provider
 *
 * Full support for Commodore IEC/IEEE-488 disk drives (1541, 1571, 1581,
 * SFD-1001, 8050, 8250) via the XUM1541 or ZoomFloppy USB adapter.
 *
 * Two operating modes:
 *
 *   1. **OpenCBM library path** (preferred) -- dynamically loads opencbm.dll
 *      or libopencbm.so at runtime and uses the well-tested OpenCBM driver
 *      stack for all bus operations.  This path supports every feature the
 *      OpenCBM project provides (standard IEC, parallel burst for 1571/1581,
 *      nibtools-style raw GCR read/write).
 *
 *   2. **Direct USB fallback** -- when OpenCBM is not installed, issues USB
 *      vendor control transfers and bulk I/O directly to the XUM1541
 *      firmware.  This covers basic IEC bus operations (read/write sectors)
 *      but does not support advanced features that require a kernel driver
 *      (e.g. parallel burst on Windows).
 *
 * The mode is selected automatically at connect() time.
 *
 * @note Install OpenCBM from https://github.com/OpenCBM/OpenCBM for the
 *       best experience.
 */
class Xum1541HardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit Xum1541HardwareProvider(QObject *parent = nullptr);
    ~Xum1541HardwareProvider() override;

    // =========================================================================
    // Basic Interface
    // =========================================================================

    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

    // =========================================================================
    // Connection Management
    // =========================================================================

    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;

    // =========================================================================
    // Motor & Head Control
    // =========================================================================

    bool setMotor(bool on) override;
    bool seekCylinder(int cylinder) override;
    bool selectHead(int head) override;
    int  currentCylinder() const override;

    // =========================================================================
    // Read / Write Operations
    // =========================================================================

    TrackData  readTrack(const ReadParams &params) override;
    QByteArray readRawFlux(int cylinder, int head, int revolutions = 2) override;
    QVector<TrackData> readDisk(int startCyl = 0, int endCyl = -1, int heads = 2) override;

    OperationResult writeTrack(const WriteParams &params, const QByteArray &data) override;
    bool writeRawFlux(int cylinder, int head, const QByteArray &fluxData) override;

    // =========================================================================
    // Utility
    // =========================================================================

    bool   getGeometry(int &tracks, int &heads) override;
    double measureRPM() override;
    bool   recalibrate() override;

    // =========================================================================
    // XUM1541-Specific API
    // =========================================================================

    /** Supported drive types */
    enum DriveType {
        DriveAuto   = 0,
        Drive1541,
        Drive1541II,
        Drive1570,
        Drive1571,
        Drive1581,
        DriveSFD1001,
        Drive8050,
        Drive8250
    };

    /** Set the CBM device number (4-30, default 8) */
    void setDeviceNumber(int deviceNum);
    int  deviceNumber() const { return m_deviceNum; }

    /** Set the expected drive type (or Auto to detect) */
    void setDriveType(DriveType type);
    DriveType driveType() const { return m_driveType; }

    /** Read the drive error/status channel (channel 15) */
    QString getDriveStatus();

    /** Send a DOS command to the drive (channel 15) */
    bool sendDriveCommand(const QString &cmd);

    /** Query device firmware version string */
    QString firmwareVersion() const;

    /** Check whether OpenCBM was loaded successfully */
    bool isOpenCbmAvailable() const { return m_cbmFuncs.available; }

private:
    // =========================================================================
    // Internal helpers
    // =========================================================================

    /** Detect which library/transport path to use */
    bool initTransport();

    /** OpenCBM-backed operations */
    bool cbmOpen();
    void cbmClose();
    int  cbmReadSector(int track, int sector, uint8_t *buf256);
    int  cbmWriteSector(int track, int sector, const uint8_t *buf256);
    bool cbmReadBlock(int track, int sector, uint8_t *buf, int bufSize);
    bool cbmIdentifyDrive();
    QString cbmReadErrorChannel();

    /** Direct USB fallback operations */
    bool usbOpen();
    void usbClose();
    bool usbControlTransfer(uint8_t request, uint16_t value, uint16_t index,
                            uint8_t *data, uint16_t length, bool dirIn);
    int  usbBulkRead(uint8_t *data, int length, int timeoutMs);
    int  usbBulkWrite(const uint8_t *data, int length, int timeoutMs);

    /** Returns number of sectors for a given track on the current drive */
    int sectorsForTrack(int track) const;

    /** Returns total number of tracks for the current drive type */
    int tracksForDrive() const;

    // =========================================================================
    // State
    // =========================================================================

    xum1541::OpenCbmFunctions  m_cbmFuncs;     /**< Dynamically loaded OpenCBM */
    xum1541::CbmFileHandle     m_cbmHandle {};  /**< OpenCBM driver handle */
    xum1541::DeviceHandle      m_usbDev;        /**< Direct USB handle */

    QString   m_hardwareType;
    QString   m_devicePath;
    int       m_baudRate      = 0;
    int       m_deviceNum     = 8;       /**< CBM device number (default 8) */
    DriveType m_driveType     = DriveAuto;
    int       m_currentCylinder = 0;
    int       m_currentHead   = 0;
    bool      m_connected     = false;
    bool      m_useOpenCbm    = false;   /**< true = OpenCBM path, false = USB */
    uint8_t   m_fwMajor       = 0;
    uint8_t   m_fwMinor       = 0;
    uint16_t  m_fwCaps        = 0;
    int       m_retries       = 3;

    mutable QMutex m_mutex;
};

#endif // XUM1541HARDWAREPROVIDER_H

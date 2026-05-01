#ifndef FC5025HARDWAREPROVIDER_H
#define FC5025HARDWAREPROVIDER_H

#include "hardwareprovider.h"
#include "fc5025_usb.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QMutex>

/**
 * @brief FC5025 Hardware Provider
 *
 * Device Side Data FC5025 USB 5.25" floppy controller support.
 * The FC5025 is a READ-ONLY device that communicates over USB bulk
 * transfers using a CBW/CSW protocol (similar to USB Mass Storage).
 *
 * This provider supports:
 * - USB device detection via VID:PID (0x16C0:0x06D6)
 * - Direct USB bulk communication (when UFT_FC5025_SUPPORT is defined)
 * - Fallback to CLI tools (fcimage) when libusb is not available
 * - Apple II, Commodore, TRS-80, Atari, TI-99/4A, Kaypro, Osborne,
 *   North Star, IBM 8" formats
 *
 * @note The FC5025 is read-only hardware.  Write operations always fail.
 * @note Full direct USB requires libusb-1.0 and UFT_FC5025_SUPPORT defined.
 *       Without it, the provider falls back to the 'fcimage' CLI tool.
 */
class FC5025HardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit FC5025HardwareProvider(QObject *parent = nullptr);
    ~FC5025HardwareProvider() override;

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
    // Read Operations  (FC5025 is read-only)
    // =========================================================================

    TrackData  readTrack(const ReadParams &params) override;
    QByteArray readRawFlux(int cylinder, int head, int revolutions = 2) override;

    // =========================================================================
    // Write Operations  (always fail — FC5025 is read-only)
    // =========================================================================

    OperationResult writeTrack(const WriteParams &params, const QByteArray &data) override;
    bool writeRawFlux(int cylinder, int head, const QByteArray &fluxData) override;

    // =========================================================================
    // Utility
    // =========================================================================

    bool   getGeometry(int &tracks, int &heads) override;
    double measureRPM() override;
    bool   recalibrate() override;

    /**
     * @brief Set the FC5025 disk format for subsequent reads.
     * @param fmt  One of the fc5025::DiskFormat codes.
     */
    void setDiskFormat(fc5025::DiskFormat fmt);

    /**
     * @brief Check if direct USB (libusb) support was compiled in.
     */
    static bool isLibusbAvailable();

private:
    fc5025::DeviceHandle m_device;
    QString              m_hardwareType;
    QString              m_devicePath;
    int                  m_baudRate = 0;
    int                  m_currentCylinder = 0;
    int                  m_currentHead = 0;
    bool                 m_motorOn = false;
    fc5025::DiskFormat   m_diskFormat = fc5025::FMT_APPLE_DOS33;
    mutable QMutex       m_mutex;

    // =========================================================================
    // Internal Helpers
    // =========================================================================

    /** Try to open the FC5025 via libusb (direct USB). */
    bool openUsb();

    /** Check if the 'fcimage' CLI tool is available on PATH. */
    bool isFcimageAvailable() const;

    /**
     * @brief Read a track using the fcimage CLI tool (fallback path).
     * @return Track data, or empty on failure.
     */
    TrackData readTrackViaCli(int cylinder, int head, int retries);

    /**
     * @brief Read a track using direct USB bulk transfers.
     * @return Track data, or empty on failure.
     */
    TrackData readTrackViaUsb(int cylinder, int head, int retries);

    /**
     * @brief Map internal format enum to the fcimage -f argument string.
     */
    static QString fcimageFormatArg(fc5025::DiskFormat fmt);

    /**
     * @brief Map internal format enum to the fc5025::FormatInfo.
     */
    const fc5025::FormatInfo *currentFormatInfo() const;
};

#endif // FC5025HARDWAREPROVIDER_H

// SPDX-License-Identifier: MIT
/*
 * fc5025hardwareprovider.h - FC5025 Qt Hardware Provider
 * 
 * Native driver integration - NO external tools required!
 * 
 * @version 2.0.0 (Native Driver)
 * @date 2025-01-01
 */

#ifndef FC5025HARDWAREPROVIDER_H
#define FC5025HARDWAREPROVIDER_H

#include "hardwareprovider.h"
#include <QString>
#include <QStringList>

// Forward declaration of C handle
extern "C" {
    struct uft_fc5025_handle;
    typedef struct uft_fc5025_handle uft_fc5025_handle_t;
}

/**
 * @brief FC5025 Hardware Provider
 * 
 * Native USB driver for Device Side Data FC5025 controller.
 * Supports 5.25" and 8" drives with FM/MFM/GCR decoding.
 * 
 * Supported Systems:
 *   - Apple II (DOS 3.2, DOS 3.3, ProDOS)
 *   - TRS-80 (Model I/III/4)
 *   - CP/M (various)
 *   - MS-DOS (360K, 1.2M)
 *   - Atari 8-bit
 *   - Kaypro
 * 
 * NO EXTERNAL TOOLS REQUIRED!
 */
class FC5025HardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit FC5025HardwareProvider(QObject *parent = nullptr);
    ~FC5025HardwareProvider() override;

    // === HardwareProvider Interface ===
    
    QString name() const override { return QStringLiteral("FC5025"); }
    QString displayName() const override { return QStringLiteral("FC5025 USB Controller"); }
    QString description() const override {
        return QStringLiteral("Device Side Data FC5025 - Native USB driver for 5.25\"/8\" drives");
    }
    
    bool init() override;
    void close() override;
    bool isConnected() const override { return m_connected; }
    
    // Track operations
    bool readTrack(uint8_t cylinder, uint8_t head, FluxData *fluxOut) override;
    bool writeTrack(uint8_t cylinder, uint8_t head, const FluxData &flux) override;
    
    // Drive control
    bool seekTrack(uint8_t track) override;
    bool motorOn() override;
    bool motorOff() override;
    
    // Device info
    QString deviceInfo() const override;
    QStringList supportedFormats() const override;
    
    // === FC5025-Specific ===
    
    /**
     * @brief Detect connected FC5025 devices
     */
    static QStringList detectDevices();
    
    /**
     * @brief Get firmware version
     */
    QString firmwareVersion() const { return m_firmwareVersion; }
    
    /**
     * @brief Check if disk is present
     */
    bool diskPresent() const;
    
    /**
     * @brief Check write protect status
     */
    bool isWriteProtected() const;
    
    /**
     * @brief Auto-detect disk format
     */
    QString detectFormat();
    
    /**
     * @brief Read entire disk to buffer
     */
    bool readDisk(const QString &format, QByteArray *dataOut);
    
    /**
     * @brief Read raw bitstream from track
     */
    bool readRawTrack(uint8_t cylinder, uint8_t head, QByteArray *bitsOut);

    // Legacy compatibility
    void setHardwareType(const QString &hardwareType) { m_hardwareType = hardwareType; }
    void setDevicePath(const QString &devicePath) { Q_UNUSED(devicePath); }
    void setBaudRate(int baudRate) { Q_UNUSED(baudRate); }
    void detectDrive();
    void autoDetectDevice();

signals:
    void trackReadComplete(uint8_t cylinder, uint8_t head, int bytesRead);
    void diskReadProgress(int currentTrack, int totalTracks);
    void formatDetected(const QString &format);
    void hardwareInfoUpdated(const HardwareInfo &info);
    void driveDetected(const DetectedDriveInfo &info);
    void statusMessage(const QString &message);

private:
    uft_fc5025_handle_t *m_handle;
    bool m_connected;
    uint8_t m_currentCylinder;
    uint8_t m_currentHead;
    bool m_motorRunning;
    QString m_firmwareVersion;
    QString m_serialNumber;
    QString m_hardwareType;
    
    int formatStringToCode(const QString &format) const;
    QString formatCodeToString(int code) const;
};

#endif // FC5025HARDWAREPROVIDER_H

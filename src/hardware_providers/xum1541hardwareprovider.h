// SPDX-License-Identifier: MIT
/*
 * xum1541hardwareprovider.h - XUM1541 Qt Hardware Provider
 * 
 * C++ wrapper for XUM1541 USB adapter for Qt GUI integration.
 * Supports XUM1541 and ZoomFloppy devices for Commodore drives.
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef XUM1541HARDWAREPROVIDER_H
#define XUM1541HARDWAREPROVIDER_H

#include "hardwareprovider.h"
#include <QString>
#include <QStringList>

// Forward declaration of C handle
extern "C" {
    struct uft_xum_handle_t;
}

/**
 * @brief XUM1541/ZoomFloppy Hardware Provider
 * 
 * Provides Qt-friendly interface to XUM1541 USB adapter
 * for reading/writing Commodore 1541/1571/1581 drives.
 */
class Xum1541HardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit Xum1541HardwareProvider(QObject *parent = nullptr);
    ~Xum1541HardwareProvider() override;

    // === HardwareProvider Interface ===
    
    QString name() const override { return "XUM1541"; }
    QString description() const override { 
        return "XUM1541/ZoomFloppy USB Adapter for Commodore 1541/1571/1581"; 
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
    
    // === XUM1541-Specific ===
    
    /**
     * @brief Get list of connected XUM1541 devices
     */
    static QStringList detectDevices();
    
    /**
     * @brief Set device number (8-11)
     * @param device Device number (default: 8)
     */
    void setDeviceNumber(uint8_t device);
    
    /**
     * @brief Get current device number
     */
    uint8_t deviceNumber() const { return m_deviceNumber; }
    
    /**
     * @brief Read raw GCR nibbles from track
     * @param track Track number (1-42)
     * @param nibblesOut Raw nibble data
     * @return true on success
     */
    bool readTrackNibbles(uint8_t track, QByteArray *nibblesOut);
    
    /**
     * @brief Write raw GCR nibbles to track
     * @param track Track number (1-42)
     * @param nibbles Raw nibble data
     * @return true on success
     */
    bool writeTrackNibbles(uint8_t track, const QByteArray &nibbles);

signals:
    void trackReadComplete(uint8_t track, int bytesRead);
    void trackWriteComplete(uint8_t track);
    void motorStateChanged(bool on);
    void deviceConnected(const QString &deviceName);
    void deviceDisconnected();
    void errorOccurred(const QString &error);

private:
    uft_xum_handle_t *m_handle;
    bool m_connected;
    uint8_t m_deviceNumber;  // IEC device number (8-11)
    uint8_t m_currentTrack;
    bool m_motorRunning;
    
    // Convert track nibbles to flux timing
    bool nibblesToFlux(const QByteArray &nibbles, FluxData *fluxOut);
    bool fluxToNibbles(const FluxData &flux, QByteArray *nibblesOut);
};

#endif // XUM1541HARDWAREPROVIDER_H

#pragma once

#include "hardwareprovider.h"

class USBFloppyHardwareProvider : public HardwareProvider
{
    Q_OBJECT
public:
    explicit USBFloppyHardwareProvider(QObject *parent = nullptr);
    ~USBFloppyHardwareProvider() override;

    /* Required pure virtuals from HardwareProvider */
    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

    /* Connection (override defaults) */
    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;

    /* Read (override defaults) */
    TrackData readTrack(const ReadParams &params) override;

    /* Write (override defaults) */
    OperationResult writeTrack(const WriteParams &params, const QByteArray &data) override;

private:
    bool m_connected;
    QString m_devicePath;
    QString m_hardwareType;
    QString m_vendorInfo;
    uint32_t m_blockSize;
    uint32_t m_totalBlocks;
};

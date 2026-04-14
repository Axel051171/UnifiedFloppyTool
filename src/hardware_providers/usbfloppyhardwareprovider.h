#pragma once

#include "hardwareprovider.h"

class USBFloppyHardwareProvider : public HardwareProvider
{
    Q_OBJECT
public:
    explicit USBFloppyHardwareProvider(QObject *parent = nullptr);
    ~USBFloppyHardwareProvider() override;

    QString displayName() const override;
    bool isAvailable() const override;
    bool connectDevice() override;
    void disconnectDevice() override;
    bool isConnected() const override;
    void setDevicePath(const QString &path) override;
    void setHardwareType(const QString &type) override;
    QStringList supportedFormats() const override;
    bool canRead() const override;
    bool canWrite() const override;
    bool canReadFlux() const override;
    QString lastError() const override;

private:
    bool m_connected;
    QString m_devicePath;
    QString m_hardwareType;
    QString m_vendorInfo;
    QString m_lastError;
};

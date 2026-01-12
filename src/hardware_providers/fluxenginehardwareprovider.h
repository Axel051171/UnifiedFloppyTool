#ifndef FLUXENGINEHARDWAREPROVIDER_H
#define FLUXENGINEHARDWAREPROVIDER_H

#include "hardwareprovider.h"

class FluxEngineHardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit FluxEngineHardwareProvider(QObject *parent = nullptr);

    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

private:
    QString findFluxEngineBinary() const;
    
    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // FLUXENGINEHARDWAREPROVIDER_H

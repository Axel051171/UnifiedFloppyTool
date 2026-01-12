#ifndef KRYOFLUXHARDWAREPROVIDER_H
#define KRYOFLUXHARDWAREPROVIDER_H

#include "hardwareprovider.h"

class KryoFluxHardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit KryoFluxHardwareProvider(QObject *parent = nullptr);

    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

private:
    QString findDtcBinary() const;
    
    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // KRYOFLUXHARDWAREPROVIDER_H

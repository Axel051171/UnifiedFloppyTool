#ifndef SCPHARDWAREPROVIDER_H
#define SCPHARDWAREPROVIDER_H

#include "hardwareprovider.h"

class SCPHardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit SCPHardwareProvider(QObject *parent = nullptr);

    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

private:
    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // SCPHARDWAREPROVIDER_H

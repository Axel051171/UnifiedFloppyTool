#ifndef APPLESAUCEHARDWAREPROVIDER_H
#define APPLESAUCEHARDWAREPROVIDER_H

#include "hardwareprovider.h"

class ApplesauceHardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit ApplesauceHardwareProvider(QObject *parent = nullptr);

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

#endif // APPLESAUCEHARDWAREPROVIDER_H

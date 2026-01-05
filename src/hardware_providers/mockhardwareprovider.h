#ifndef MOCKHARDWAREPROVIDER_H
#define MOCKHARDWAREPROVIDER_H

#include "hardwareprovider.h"

class MockHardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit MockHardwareProvider(QObject *parent = nullptr);

    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

private:
    DetectedDriveInfo buildDriveInfo() const;
    HardwareInfo buildHardwareInfo() const;

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // MOCKHARDWAREPROVIDER_H

#ifndef FC5025HARDWAREPROVIDER_H
#define FC5025HARDWAREPROVIDER_H

#include "hardwareprovider.h"

/**
 * @brief FC5025 Hardware Provider
 * 
 * Device Side Data FC5025 USB floppy controller support.
 * Note: Full functionality requires FC5025 SDK/drivers.
 */
class FC5025HardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit FC5025HardwareProvider(QObject *parent = nullptr);
    ~FC5025HardwareProvider() override;

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
    bool m_connected = false;
};

#endif // FC5025HARDWAREPROVIDER_H

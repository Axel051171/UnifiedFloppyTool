#ifndef XUM1541HARDWAREPROVIDER_H
#define XUM1541HARDWAREPROVIDER_H

#include "hardwareprovider.h"

/**
 * @brief XUM1541/ZoomFloppy Hardware Provider
 * 
 * Support for Commodore IEC/IEEE-488 devices via XUM1541/ZoomFloppy.
 * Note: Full functionality requires OpenCBM library.
 */
class Xum1541HardwareProvider : public HardwareProvider
{
    Q_OBJECT

public:
    explicit Xum1541HardwareProvider(QObject *parent = nullptr);
    ~Xum1541HardwareProvider() override;

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

#endif // XUM1541HARDWAREPROVIDER_H

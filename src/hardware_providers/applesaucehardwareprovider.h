#ifndef APPLESAUCEHARDWAREPROVIDER_H
#define APPLESAUCEHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QString>

/*
  Applesauce provider – Provider Framework A1 (static)

  A1 scope:
  - Applesauce hardware is typically driven via its own host software (primarily macOS).
  - We therefore start with Import/Export support for Applesauce file formats:
      - A2R (flux archive)
      - WOZ (Apple II)
      - MOOF (Macintosh)
  - Driving the hardware directly is out of scope until we identify a stable, redistributable CLI/API.

  Integration type: Import/Export only (A1)
*/

class ApplesauceHardwareProvider final : public HardwareProvider
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
    void emitFormatOnlyInfo(const QString &notes) const;

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // APPLESAUCEHARDWAREPROVIDER_H

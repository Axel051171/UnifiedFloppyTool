#ifndef SCPHARDWAREPROVIDER_H
#define SCPHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QString>

/*
  SuperCard Pro (SCP) provider – Provider Framework A1 (static)

  A1 scope:
  - We do NOT attempt to drive SCP hardware directly (no stable, cross-platform, documented CLI/library
    we can depend on).
  - We expose SCP as an Import/Export backend for the .SCP file format and as a placeholder for future
    hardware control once a reliable tool/lib is selected.

  Integration type: Import/Export only (A1), CLI-wrapper (planned)
*/

class SCPHardwareProvider final : public HardwareProvider
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
    void emitFormatOnlyInfo(const QString &notes) const;

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // SCPHARDWAREPROVIDER_H

#ifndef CATWEASELHARDWAREPROVIDER_H
#define CATWEASELHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QString>

/*
  Catweasel (PCI/ISA) provider – Provider Framework A1 (static)

  Reality:
  - Catweasel is a legacy controller card (ISA/PCI variants) with OS-specific drivers and tooling.
  - Cross-platform, maintained user-space tooling is not guaranteed.
  - A1 treats it as "import/export + placeholder" until we decide on:
      (a) using existing tools via CLI, or
      (b) talking to a kernel/driver layer on supported OSes.

  Integration type (A1): Placeholder (no direct hardware control yet).
*/

class CatweaselHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit CatweaselHardwareProvider(QObject *parent = nullptr);

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

#endif // CATWEASELHARDWAREPROVIDER_H
